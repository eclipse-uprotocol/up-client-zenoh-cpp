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

#include <up-client-zenoh-cpp/client/upZenohClient.h>
#include <up-core-api/ustatus.pb.h>

using namespace uprotocol::v1;
using namespace uprotocol::client;

std::shared_ptr<UpZenohClient> UpZenohClient::instance(void) noexcept {
    static std::weak_ptr<UpZenohClient> w_handle;

    if (auto handle = w_handle.lock()) {
        return handle;
    } else {
        static std::mutex construction_mtx;
        std::lock_guard lock(construction_mtx);

        if (handle = w_handle.lock()) {
            return handle;
        }

        handle = std::make_shared<UpZenohClient>(ConstructToken());
        if (handle->rpcSuccess_.code() == UCode::OK && handle->uSuccess_.code() == UCode::OK) {
            w_handle = handle;
            return handle;
        } else {
            spdlog::error("failed to get instance");
            return nullptr;
        }
    }
}
