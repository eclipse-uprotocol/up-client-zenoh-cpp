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

#include <gtest/gtest.h>
#include <up-cpp/datamodel/builder/Uuid.h>
#include <up-cpp/datamodel/validator/UUri.h>

#include <zenoh.hxx>

#include "up-transport-zenoh-cpp/ZenohUTransport.h"

namespace uprotocol::transport {

using namespace uprotocol::v1;
using namespace uprotocol::transport;

constexpr const char* AUTHORITY_NAME = "test";

class TestZenohUTransport : public testing::Test {
protected:
	// Run once per TEST_F.
	// Used to set up clean environments per test.
	void SetUp() override {}
	void TearDown() override {}

	// Run once per execution of the test application.
	// Used for setup of all tests. Has access to this instance.
	TestZenohUTransport() = default;
	~TestZenohUTransport() = default;

	// Run once per execution of the test application.
	// Used only for global setup outside of tests.
	static void SetUpTestSuite() {}
	static void TearDownTestSuite() {}
};

uprotocol::v1::UUri create_uuri(const std::string& authority, uint32_t ue_id,
                                uint32_t ue_version_major,
                                uint32_t resource_id) {
	uprotocol::v1::UUri uuri;
	uuri.set_authority_name(authority);
	uuri.set_ue_id(ue_id);
	uuri.set_ue_version_major(ue_version_major);
	uuri.set_resource_id(resource_id);

	return uuri;
}

// TODO(sashacmc): config generation
TEST_F(TestZenohUTransport, ConstructDestroy) {
	uprotocol::v1::UUri def_src_uuri;
	def_src_uuri.set_authority_name(AUTHORITY_NAME);
	def_src_uuri.set_ue_id(0x18000);
	def_src_uuri.set_ue_version_major(1);
	def_src_uuri.set_resource_id(0);

	zenoh::init_logger();
	try {
		auto ut = ZenohUTransport(def_src_uuri,
		                          "/home/sashacmc/src/up-client-zenoh-cpp/test/"
		                          "extra/DEFAULT_CONFIG.json5");
	} catch (zenoh::ErrorMessage& e) {
		throw std::runtime_error(std::string(e.as_string_view()));
	}
}

TEST_F(TestZenohUTransport, toZenohKeyString) {
	EXPECT_EQ(
	    ZenohUTransport::toZenohKeyString(
	        "", create_uuri("192.168.1.100", 0x10AB, 3, 0x80CD), std::nullopt),
	    "up/192.168.1.100/10AB/3/80CD/{}/{}/{}/{}");

	EXPECT_EQ(ZenohUTransport::toZenohKeyString(
	              "", create_uuri("192.168.1.100", 0x10AB, 3, 0x80CD),
	              create_uuri("192.168.1.101", 0x20EF, 4, 0)),
	          "up/192.168.1.100/10AB/3/80CD/192.168.1.101/20EF/4/0");

	EXPECT_EQ(ZenohUTransport::toZenohKeyString(
	              "", create_uuri("*", 0xFFFF, 0xFF, 0xFFFF),
	              create_uuri("192.168.1.101", 0x20EF, 4, 0)),
	          "up/*/*/*/*/192.168.1.101/20EF/4/0");

	EXPECT_EQ(ZenohUTransport::toZenohKeyString(
	              "", create_uuri("my-host1", 0x10AB, 3, 0),
	              create_uuri("my-host2", 0x20EF, 4, 0xB)),
	          "up/my-host1/10AB/3/0/my-host2/20EF/4/B");

	EXPECT_EQ(ZenohUTransport::toZenohKeyString(
	              "", create_uuri("*", 0xFFFF, 0xFF, 0xFFFF),
	              create_uuri("my-host2", 0x20EF, 4, 0xB)),
	          "up/*/*/*/*/my-host2/20EF/4/B");

	EXPECT_EQ(ZenohUTransport::toZenohKeyString(
	              "", create_uuri("*", 0xFFFF, 0xFF, 0xFFFF),
	              create_uuri("[::1]", 0xFFFF, 0xFF, 0xFFFF)),
	          "up/*/*/*/*/[::1]/*/*/*");
}

}  // namespace uprotocol::transport
