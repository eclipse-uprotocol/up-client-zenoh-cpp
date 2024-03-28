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
 
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>
#include <up-cpp/uri/serializer/MicroUriSerializer.h>
#include "up-client-zenoh-cpp/session/zenohSessionManager.h"

namespace uprotocol::uri {

namespace {
void hexlify(auto& topic, const uint8_t byte) {
    topic << std::hex << std::setw(2) << std::setfill('0');
    topic << static_cast<int>(byte);
}

void hexlify(auto& topic, const auto &&start, const auto &&end) {
    for (auto i = start; i < end; ++i) {
        hexlify(topic, *i);
    }
}

} // anonymous namespace

std::string toZenohKeyString(const uprotocol::v1::UUri &u_uri) {
    constexpr std::string_view remote_prefix{"upr"};
    constexpr std::string_view local_prefix{"upl"};
    constexpr std::string_view separator{"/"};
    constexpr std::string_view wildcard_suffix{"**"};

    using Serializer = uprotocol::uri::MicroUriSerializer;
    constexpr auto authority_offset = Serializer::AuthorityStartPosition;

    std::ostringstream topic;

    if (u_uri.has_authority() && !u_uri.has_entity() && !u_uri.has_resource()) {
        if (auto authority = Serializer::serialize(u_uri.authority()).second;
                !authority.empty()) {
            topic << remote_prefix << separator;
            hexlify(topic, authority.begin(), authority.end());
            topic << wildcard_suffix;
        } else {
            spdlog::error("Serialized micro URI Authority is empty");
        }
    } else if (auto uri = Serializer::serialize(u_uri); uri.empty()) {
        spdlog::error("Serialized micro URI is empty");
    } else {
        if (uri.size() == Serializer::LocalMicroUriLength) {
            topic << local_prefix << separator;
        } else /* Remote */ {
            topic << remote_prefix << separator;
            hexlify(topic, uri.begin() + authority_offset, uri.end());
            topic << separator;
        }
        hexlify(topic, uri.begin(), uri.begin() + authority_offset);
    }

    return topic.str();
}

} // namespace uprotocol::tools
