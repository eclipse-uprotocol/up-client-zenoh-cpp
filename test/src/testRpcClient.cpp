/*
 * Copyright (c) 2024 General Motors GTO LLC
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2024 General Motors GTO LLC
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csignal>
#include <spdlog/spdlog.h>
#include <up-client-zenoh-cpp/client/upZenohClient.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <gtest/gtest.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::uri;
using namespace uprotocol::rpc;
using namespace uprotocol::client;

UUri rpcUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.handler");
UUri rpcNoServerUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.noServer");

class RpcServer : public UListener {

     public:

        UStatus onReceive(UMessage &message) const override {

            UStatus status;

            status.set_code(UCode::OK);
            
            auto builder = UAttributesBuilder::response(message.attributes().source(), message.attributes().sink(), UPriority::UPRIORITY_CS0, message.attributes().id());

            UAttributes responseAttributes = builder.build();

            UPayload outPayload = message.payload();

            UMessage respMessage(outPayload, responseAttributes);

            if (nullptr != message.payload().data()) {

                std::string cmd(message.payload().data(), message.payload().data() + message.payload().size());

                if ("No Response" != cmd) {
                    return UpZenohClient::instance()->send(respMessage);
                }
            } else {
                return UpZenohClient::instance()->send(respMessage);
            }
                   
            return status;
        }
};

class ResponseListener : public UListener {

     public:

        UStatus onReceive(UMessage &message) const override {

            (void) message;

            UStatus status;

            status.set_code(UCode::OK);
           
            return status;
        }
};

class TestRPcClient : public ::testing::Test {

    public:
        // SetUpTestSuite() is called before all tests in the test suite
        static void SetUpTestSuite() {

        }

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {

        }

        static RpcServer rpcListener;
        static ResponseListener responseListener;

};

RpcServer TestRPcClient::rpcListener;
ResponseListener TestRPcClient::responseListener;

TEST_F(TestRPcClient, InvokeMethodWithoutServer) {
    
    auto instance = UpZenohClient::instance();
    
    EXPECT_NE(instance, nullptr);

    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);

    std::future<RpcResponse> future = instance->invokeMethod(rpcNoServerUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_NE(response.status.code(), UCode::OK);
}

TEST_F(TestRPcClient, InvokeMethodWithLowPriority) {
    
    auto instance = UpZenohClient::instance();

    EXPECT_NE(instance, nullptr);

    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS3);

    std::future<RpcResponse> future = instance->invokeMethod(rpcNoServerUri, payload, options);

    EXPECT_EQ(future.valid(), false);
}

TEST_F(TestRPcClient, invokeMethodNoResponse) {
    
    auto instance = UpZenohClient::instance();

    EXPECT_NE(instance, nullptr);

    std::string message = "No Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = instance->invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_NE(response.status.code(), UCode::OK);
}

TEST_F(TestRPcClient, maxSimultaneousRequests) {
    
    auto instance = UpZenohClient::instance();

    EXPECT_NE(instance, nullptr);

    auto status = instance->registerListener(rpcUri, TestRPcClient::rpcListener);

    EXPECT_EQ(status.code(), UCode::OK);

    std::string message = "No Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(5000);

    size_t numRequestsUntilQueueIsFull = instance->getMaxConcurrentRequests() + instance->getQueueSize();

    for (size_t i = 0; i < (numRequestsUntilQueueIsFull + 1) ; ++i) {
        std::future<RpcResponse> future = instance->invokeMethod(rpcUri, payload, options);

        if (i < numRequestsUntilQueueIsFull) {
            EXPECT_EQ(future.valid(), true);
        } else {
            EXPECT_EQ(future.valid(), false);
        }
    }

    /* wait for al futures to return */
    sleep(10);

    std::future<RpcResponse> future = instance->invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);

    status = instance->unregisterListener(rpcUri, TestRPcClient::rpcListener);

    EXPECT_EQ(status.code(), UCode::OK);
}

TEST_F(TestRPcClient, invokeMethodWithNullResponse) {
    
    auto instance = UpZenohClient::instance();

    EXPECT_NE(instance, nullptr);

    auto status = instance->registerListener(rpcUri, TestRPcClient::rpcListener);

    EXPECT_EQ(status.code(), UCode::OK);

    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = instance->invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_EQ(response.status.code(), UCode::OK);

    EXPECT_EQ(response.message.payload().size(), 0);

    status = instance->unregisterListener(rpcUri, TestRPcClient::rpcListener);

    EXPECT_EQ(status.code(), UCode::OK);
}

TEST_F(TestRPcClient, invokeMethodWithResponse) {
    
    std::string message = "Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    auto instance = UpZenohClient::instance();

    EXPECT_NE(instance, nullptr);

    auto status = instance->registerListener(rpcUri, TestRPcClient::rpcListener);

    EXPECT_EQ(status.code(), UCode::OK);

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);    

    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = instance->invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_EQ(response.status.code(), UCode::OK);

    EXPECT_NE(response.message.payload().data(), nullptr);
    EXPECT_NE(response.message.payload().size(), 0);

    status = instance->unregisterListener(rpcUri, TestRPcClient::rpcListener);

    EXPECT_EQ(status.code(), UCode::OK);
}

TEST_F(TestRPcClient, invokeMethodWithCbResponse) {
    
    auto instance = UpZenohClient::instance();
    
    EXPECT_NE(instance, nullptr);

    std::string message = "Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);    

    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    auto status = instance->invokeMethod(rpcUri, payload, options, responseListener);

    EXPECT_EQ(status.code(), UCode::OK);  
}

TEST_F(TestRPcClient, invokeMethodWithCbResponseFailure) {
    
    auto instance = UpZenohClient::instance();
  
    EXPECT_NE(instance, nullptr);

    std::string message = "Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);    

    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS0);
    options.set_ttl(1000);

    auto status = instance->invokeMethod(rpcUri, payload, options, responseListener);

    EXPECT_NE(status.code(), UCode::OK);  
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}