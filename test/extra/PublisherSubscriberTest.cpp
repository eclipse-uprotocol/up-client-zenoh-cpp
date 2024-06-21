// SPDX-FileCopyrightText: 2024 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <up-cpp/datamodel/builder/Uuid.h>
#include <up-cpp/datamodel/validator/UUri.h>

#include <zenoh.hxx>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

namespace {

using namespace uprotocol::v1;
using namespace uprotocol::transport;

constexpr const char* AUTHORITY_NAME = "test";

class TestFixture : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	TestFixture() = default;
	~TestFixture() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

using MsgDiff = google::protobuf::util::MessageDifferencer;

uprotocol::v1::UUID* make_uuid() {
	uint64_t timestamp =
	    std::chrono::duration_cast<std::chrono::milliseconds>(
	        std::chrono::system_clock::now().time_since_epoch())
	        .count();
	auto id = new uprotocol::v1::UUID();
	id->set_msb((timestamp << 16) | (8ULL << 12) |
	            (0x123ULL));  // version 8 ; counter = 0x123
	id->set_lsb((2ULL << 62) | (0xFFFFFFFFFFFFULL));  // variant 10
	return id;
}

// TODO(sashacmc): config generation
TEST_F(TestFixture, PubSub) {
	UUri uuri;

	uuri.set_authority_name(AUTHORITY_NAME);
	uuri.set_ue_id(0x00010001);
	uuri.set_ue_version_major(1);
	uuri.set_resource_id(0);

	zenoh::init_logger();
	try {
		std::cerr << "Test MESSAGE" << std::endl;
		auto ut = ZenohUTransport(uuri,
		                          "/home/sashacmc/src/up-client-zenoh-cpp/test/"
		                          "extra/DEFAULT_CONFIG.json5");

		uprotocol::v1::UUri sink_filter;
		sink_filter.set_authority_name(AUTHORITY_NAME);
		sink_filter.set_ue_id(0x00010001);
		sink_filter.set_ue_version_major(1);
		sink_filter.set_resource_id(0x8000);

		uprotocol::v1::UUri source_filter;
		source_filter.set_authority_name(AUTHORITY_NAME);
		source_filter.set_ue_id(0x00010001);
		source_filter.set_ue_version_major(1);
		source_filter.set_resource_id(0x8000);

		uprotocol::v1::UMessage capture_msg;
		size_t capture_count = 0;
		auto action = [&](const uprotocol::v1::UMessage& msg) {
			capture_msg = msg;
			capture_count++;
		};
		auto lhandle = ut.registerListener(sink_filter, action, source_filter);
		EXPECT_TRUE(lhandle.has_value());
		auto handle = std::move(lhandle).value();
		EXPECT_TRUE(handle);

		const size_t max_count = 1;  // 1000 * 100;
		for (auto i = 0; i < max_count; i++) {
			auto src = new uprotocol::v1::UUri();
			src->set_authority_name(AUTHORITY_NAME);
			src->set_ue_id(0x00010001);
			src->set_ue_version_major(1);
			src->set_resource_id(0x8000);

			auto attr = new uprotocol::v1::UAttributes();
			attr->set_type(uprotocol::v1::UMESSAGE_TYPE_PUBLISH);
			attr->set_allocated_source(src);
			attr->set_allocated_id(make_uuid());
			attr->set_payload_format(uprotocol::v1::UPAYLOAD_FORMAT_PROTOBUF);
			attr->set_ttl(1000);

			uprotocol::v1::UMessage msg;
			msg.set_allocated_attributes(attr);
			msg.set_payload("payload");
			auto result = ut.send(msg);
			EXPECT_EQ(i + 1, capture_count);
			EXPECT_TRUE(MsgDiff::Equals(msg, capture_msg));
		}
		handle.reset();
	} catch (zenoh::ErrorMessage& e) {
		throw std::runtime_error(std::string(e.as_string_view()));
	}
}
}  // namespace
