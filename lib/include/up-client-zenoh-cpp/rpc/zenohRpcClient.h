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

#ifndef _ZENOH_RPC_CLIENT_H_
#define _ZENOH_RPC_CLIENT_H_

#include <condition_variable>
#include <up-cpp/rpc/RpcClient.h>
#include <up-cpp/utils/ThreadPool.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-core-api/ustatus.pb.h>
#include <up-core-api/uri.pb.h>
#include <zenoh.h>

namespace uprotocol::rpc {

    struct ZenohRpcClientConfig
    {
        size_t maxQueueSize;
        size_t maxConcurrentRequests;
    };

    class ZenohRpcClient : public RpcClient 
    {
        public:
            ZenohRpcClient(const ZenohRpcClient&) = delete;
            ZenohRpcClient& operator=(const ZenohRpcClient&) = delete;

            /**
            * Support for RPC method invocation.
            * @param topic topic of the method to be invoked (i.e. the name of the API we are calling).
            * @param payload The request message to be sent to the server.
            * @param options options 
            * @return Returns the CompletableFuture with the result or exception.
            */
            std::future<RpcResponse> invokeMethod(const uprotocol::v1::UUri &topic, 
                                                  const uprotocol::utransport::UPayload &payload, 
                                                  const uprotocol::v1::CallOptions &options) noexcept;

            /**
            * API for clients to invoke a method (send an RPC request) and receive the response (the returned 
            * {@link CompletionStage} {@link UPayload}. <br>
            * Client will set method to be the URI of the method they want to invoke, 
            * payload to the request message, and attributes with the various metadata for the 
            * method invocation.
            * @param methodUri The method URI to be invoked, ex (long form): /example.hello_world/1/rpc.SayHello.
            * @param requestPayload The request message to be sent to the server.
            * @param options RPC method invocation call options, see {@link CallOptions}
            * @param callback that will be called once the future is complete
            * @return UStatus
            */
            uprotocol::v1::UStatus invokeMethod(const uprotocol::v1::UUri &topic,
                                                const uprotocol::utransport::UPayload &payload,
                                                const uprotocol::v1::CallOptions &options,
                                                const uprotocol::utransport::UListener &callback) noexcept;

            /**
             * get the number of max concurrent request 
             * @return number of concurrent requests
            */
            size_t getMaxConcurrentRequests() {
                return maxNumOfCuncurrentRequests_;
            }
            
            /**
             * get queue size
             * @return queue size
            */
            size_t getQueueSize() {
                return queueSize_;
            }

        protected:
            /* Initialization success/failure status */
            uprotocol::v1::UStatus rpcSuccess_;

            ZenohRpcClient() noexcept;
            ~ZenohRpcClient() noexcept;

        private:

            static uprotocol::utransport::UPayload handleReply(z_owned_reply_channel_t *channel);

            std::future<uprotocol::rpc::RpcResponse> invokeMethodInternal(const uprotocol::v1::UUri &topic,
                                                                            const uprotocol::utransport::UPayload &payload,
                                                                            const uprotocol::v1::CallOptions &options,
                                                                            const uprotocol::utransport::UListener *callback = nullptr) noexcept;

            static uprotocol::rpc::RpcResponse handleReply(z_owned_reply_channel_t *channel, 
                                                            const uprotocol::utransport::UListener *callback = nullptr) noexcept;
            
            static constexpr auto requestTimeoutMs_ = 5000;
            static constexpr auto queueSizeDefault_ = size_t(20);
            static constexpr auto maxNumOfCuncurrentRequestsDefault_ = size_t(2);

            size_t queueSize_ = queueSizeDefault_;
            size_t maxNumOfCuncurrentRequests_ = maxNumOfCuncurrentRequestsDefault_;

            /* zenoh session handle*/
            z_owned_session_t session_;
            
            std::shared_ptr<uprotocol::utils::ThreadPool> threadPool_;

    };
}

#endif /*_ZENOH_RPC_CLIENT_H_*/
