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

#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-client-zenoh-cpp/session/zenohSessionManager.h>
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-cpp/utils/ThreadPool.h>
#include <up-core-api/ustatus.pb.h>
#include <up-core-api/uattributes.pb.h>
#include <spdlog/spdlog.h>
#include <zenoh.h>

using namespace uprotocol::utransport;
using namespace uprotocol::uuid;
using namespace uprotocol::uri;
using namespace uprotocol::utils;

ZenohRpcClient& ZenohRpcClient::instance(void) noexcept {
    
    static ZenohRpcClient rpcClient;

    return rpcClient;
}

UStatus ZenohRpcClient::init() noexcept {

    UStatus status;

    if (0 == refCount_) {

        std::lock_guard<std::mutex> lock(mutex_);

        if (0 == refCount_) {
            /* by default initialized to empty strings */
            ZenohSessionManagerConfig config;

            if (UCode::OK != ZenohSessionManager::instance().init(config)) {
                spdlog::error("zenohSessionManager::instance().init() failed");
                status.set_code(UCode::UNAVAILABLE);
                return status;
            }

            if (ZenohSessionManager::instance().getSession().has_value()) {
                session_ = ZenohSessionManager::instance().getSession().value();
            } else {
                status.set_code(UCode::UNAVAILABLE);
                return status;
            }

            threadPool_ = make_shared<ThreadPool>(queueSize_, maxNumOfCuncurrentRequests_);
            if (nullptr == threadPool_) {
                spdlog::error("failed to create thread pool");
                status.set_code(UCode::UNAVAILABLE);
                return status;
            }
        }
        refCount_.fetch_add(1);

    } else {
        refCount_.fetch_add(1);
    }

    status.set_code(UCode::OK);

    return status;
}

UStatus ZenohRpcClient::term() noexcept {

    UStatus status;
    
    std::lock_guard<std::mutex> lock(mutex_);

    refCount_.fetch_sub(1);

    if (0 == refCount_) {

        if (UCode::OK != ZenohSessionManager::instance().term()) {
            spdlog::error("zenohSessionManager::instance().term() failed");
            status.set_code(UCode::UNAVAILABLE);
            return status;
        }
    }

    status.set_code(UCode::OK);

    return status;
}
std::future<UPayload> ZenohRpcClient::invokeMethod(const UUri &uri,
                                                   const UPayload &payload, 
                                                   const UAttributes &attributes) noexcept {

    std::future<UPayload> future;

    if (0 == refCount_) {
        spdlog::error("ZenohRpcClient is not initialized");
        return std::move(future);
    }

    if (false == isRPCMethod(uri.resource())) {
        spdlog::error("URI is not of RPC type");
        return std::move(future);
    }

    if (UMessageType::UMESSAGE_TYPE_REQUEST != attributes.type()) {
        spdlog::error("Wrong message type = {}", static_cast<int>(attributes.type()));
        return std::move(future);
    }

    auto uriHash = std::hash<std::string>{}(LongUriSerializer::serialize(uri));

    z_owned_reply_channel_t *channel = new z_owned_reply_channel_t;
    if (nullptr == channel) {
        spdlog::error("failed to allocate channel");
        return std::move(future);
    }

    *channel = zc_reply_fifo_new(16);

    // Serialize UAttributes
    size_t attrSize = attributes.ByteSizeLong();
    std::vector<uint8_t> serializedAttributes(attrSize);
    if (!attributes.SerializeToArray(serializedAttributes.data(), attrSize)) {
        spdlog::error("SerializeToArray failure");
        return std::move(future);
    }

    z_get_options_t opts = z_get_options_default();
    opts.timeout_ms = requestTimeoutMs_;

    z_owned_bytes_map_t map = z_bytes_map_new();
    z_bytes_t bytes = {.len = serializedAttributes.size(), .start = serializedAttributes.data()};
    z_bytes_map_insert_by_alias(&map, z_bytes_new("attributes"), bytes);

    opts.attachment = z_bytes_map_as_attachment(&map);

    if (0 != z_get(z_loan(session_), z_keyexpr(std::to_string(uriHash).c_str()), 
                   "", z_move(channel->send), &opts)) {
        spdlog::error("z_get failure");
        z_drop(&map);
        delete channel;
        return std::move(future);
    }

    future = threadPool_->submit([=] { return handleReply(channel); });

    z_drop(&map);
    return future;
}

UPayload ZenohRpcClient::handleReply(z_owned_reply_channel_t *channel) {

    z_owned_reply_t reply = z_reply_null();
    
    UPayload retPayload(nullptr, 0, UPayloadType::VALUE);

    while (z_call(channel->recv, &reply), z_check(reply)) {
        if (!z_reply_is_ok(&reply)) {
            spdlog::error("error received");
            break;
        }

        z_sample_t sample = z_reply_ok(&reply);

        if (sample.payload.len == 0 || sample.payload.start == nullptr) {
            spdlog::error("Payload is empty");
            break;
        }

        if (!z_check(sample.attachment)) {
            spdlog::error("No attachment found in the reply");
            break;
        }       

        z_bytes_t serializedAttributes = z_attachment_get(sample.attachment, z_bytes_new("attributes"));
        
        if ((0 == serializedAttributes.len) || (nullptr == serializedAttributes.start)) {
            spdlog::error("Serialized attributes not found in the attachment");
            break;
        }

        uprotocol::v1::UAttributes attributes;
        if (!attributes.ParseFromArray(serializedAttributes.start, serializedAttributes.len)) {
            spdlog::error("ParseFromArray failure");
            break;
        }

        retPayload = UPayload(sample.payload.start, sample.payload.len, UPayloadType::VALUE);

        z_drop(z_move(reply));
    }

    z_drop((z_owned_reply_channel_t*)channel);

    delete channel;

    return retPayload;
}