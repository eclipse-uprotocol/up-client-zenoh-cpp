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
 
#include "up-client-zenoh-cpp/session/zenohSessionManager.h"
#include <spdlog/spdlog.h>

ZenohSessionManager& ZenohSessionManager::instance(void) noexcept {

	static ZenohSessionManager sessionManager;

	return sessionManager;
}

UCode ZenohSessionManager::init(ZenohSessionManagerConfig &sessionConfig) noexcept {

    if (0 == refCount_) {

        std::lock_guard<std::mutex> lock(mutex_);

        if (0 == refCount_) {

            z_owned_config_t config = z_config_default();
        
            if (0 < sessionConfig.connectKey.length()) {

                if (0 > zc_config_insert_json(z_loan(config), Z_CONFIG_CONNECT_KEY, sessionConfig.connectKey.c_str())) {
                    spdlog::error("zc_config_insert_json (Z_CONFIG_CONNECT_KEY) failed");
                    return UCode::INTERNAL;
                }
            }

            if (0 < sessionConfig.listenKey.length()) {

                if (0 > zc_config_insert_json(z_loan(config), Z_CONFIG_LISTEN_KEY, sessionConfig.listenKey.c_str())) {
                    spdlog::error("zc_config_insert_json (Z_CONFIG_LISTEN_KEY) failed");
                    return UCode::INTERNAL;
                }
            }

            session_ = z_open(z_move(config));

            if (false == z_check(session_)) {

                spdlog::error("z_open failed");
                return UCode::INTERNAL;
            }
        }

        refCount_.fetch_add(1);
    } else {
        refCount_.fetch_add(1);
    }
    
    spdlog::info("ZenohSessionManager::init done , ref count {}", refCount_.load());
    
    return UCode::OK;
}

UCode ZenohSessionManager::term() noexcept {
    
    std::lock_guard<std::mutex> lock(mutex_);

    if (0 < refCount_) {

        refCount_.fetch_sub(1);

        if (0 == refCount_) {
            z_close(z_move(session_));
            spdlog::info("ZenohSessionManager::term() done");
        }
    }

    return UCode::OK;
}

std::optional<z_owned_session_t> ZenohSessionManager::getSession() noexcept {

    std::lock_guard<std::mutex> lock(mutex_);

    if (0 < refCount_) {
        return session_;
    }

    return std::nullopt;
}