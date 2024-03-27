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
#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-cpp/transport/builder/UAttributesBuilder.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <gtest/gtest.h>

using namespace uprotocol::utransport;
using namespace uprotocol::v1;
using namespace uprotocol::uri;
using namespace uprotocol::rpc;

UUri rpcUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.handler");
UUri rpcNoServerUri = LongUriSerializer::deserialize("/test_rpc.app/1/rpc.noServer");

class RpcServer : public UListener {

     public:

        UStatus onReceive(UMessage &message) const override {

            UStatus status;

            status.set_code(UCode::OK);
            
            UAttributesBuilder builder(message.attributes().source(),
                                       message.attributes().id(), 
                                       UMessageType::UMESSAGE_TYPE_RESPONSE, 
                                       UPriority::UPRIORITY_CS0);

            UAttributes responseAttributes = builder.build();

            UPayload outPayload = message.payload();

            UMessage respMessage(outPayload, responseAttributes);

            if (nullptr != message.payload().data()) {

                std::string cmd(message.payload().data(), message.payload().data() + message.payload().size());

                if ("No Response" != cmd) {
                    return ZenohUTransport::instance().send(respMessage);
                }
            } else {
                return ZenohUTransport::instance().send(respMessage);
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

            if (UCode::OK != ZenohUTransport::instance().init().code()) {
                spdlog::error("ZenohUTransport::instance().init failed");
                return;
            }

            if (UCode::OK != ZenohRpcClient::instance().init().code()) {
                spdlog::error("ZenohRpcClient::instance().init failed");
                return;
            }

            ZenohUTransport::instance().registerListener(rpcUri, TestRPcClient::rpcListener);
        }

        // TearDownTestSuite() is called after all tests in the test suite
        static void TearDownTestSuite() {

            if (UCode::OK != ZenohUTransport::instance().term().code()) {
                spdlog::error("ZenohUTransport::instance().term() failed");
                return;
            }

            if (UCode::OK != ZenohRpcClient::instance().term().code()) {
                spdlog::error("ZenohRpcClient::instance().term() failed");
                return;
            }
        }
    
        static RpcServer rpcListener;
        static ResponseListener responseListener;

};

RpcServer TestRPcClient::rpcListener;
ResponseListener TestRPcClient::responseListener;

TEST_F(TestRPcClient, InvokeMethodWithoutServer) {
    
    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcNoServerUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_NE(response.status.code(), UCode::OK);
}

TEST_F(TestRPcClient, InvokeMethodWithLowPriority) {
    
    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS3);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcNoServerUri, payload, options);

    EXPECT_EQ(future.valid(), false);
}

TEST_F(TestRPcClient, invokeMethodNoResponse) {
    
    std::string message = "No Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_NE(response.status.code(), UCode::OK);
}

TEST_F(TestRPcClient, maxSimultaneousRequests) {
    
    std::string message = "No Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(5000);

    size_t numRequestsUntilQueueIsFull = ZenohRpcClient::instance().getMaxConcurrentRequests() + ZenohRpcClient::instance().getQueueSize();

    for (size_t i = 0; i < (numRequestsUntilQueueIsFull + 1) ; ++i) {
        std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

        if (i < numRequestsUntilQueueIsFull) {
            EXPECT_EQ(future.valid(), true);
        } else {
            EXPECT_EQ(future.valid(), false);
        }
    }

    /* wait for al futures to return */
    sleep(10);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
}

TEST_F(TestRPcClient, invokeMethodWithNullResponse) {
    
    UPayload payload(nullptr, 0, UPayloadType::REFERENCE);
    
    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_EQ(response.status.code(), UCode::OK);

    EXPECT_EQ(response.message.payload().size(), 0);
}

TEST_F(TestRPcClient, invokeMethodWithResponse) {
    
    std::string message = "Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);    

    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    std::future<RpcResponse> future = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options);

    EXPECT_EQ(future.valid(), true);
    
    auto response = future.get();
    
    EXPECT_EQ(response.status.code(), UCode::OK);

    EXPECT_NE(response.message.payload().data(), nullptr);
    EXPECT_NE(response.message.payload().size(), 0);
}

TEST_F(TestRPcClient, invokeMethodWithCbResponse) {
    
    std::string message = "Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);    

    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS4);
    options.set_ttl(1000);

    auto status = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options, responseListener);

    EXPECT_EQ(status.code(), UCode::OK);  
}

TEST_F(TestRPcClient, invokeMethodWithCbResponseFailure) {
    
    std::string message = "Response";
    std::vector<uint8_t> data(message.begin(), message.end());

    UPayload payload(data.data(), data.size(), UPayloadType::VALUE);    

    CallOptions options;

    options.set_priority(UPriority::UPRIORITY_CS0);
    options.set_ttl(1000);

    auto status = ZenohRpcClient::instance().invokeMethod(rpcUri, payload, options, responseListener);

    EXPECT_NE(status.code(), UCode::OK);  
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}