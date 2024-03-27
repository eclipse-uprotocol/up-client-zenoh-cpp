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

#ifndef _ZENOH_SESSION_MANAGER_H_
#define _ZENOH_SESSION_MANAGER_H_

#include <atomic>
#include <optional>
#include <zenoh.h>
#include <up-core-api/ustatus.pb.h>

using namespace std;
using namespace uprotocol::v1;

struct ZenohSessionManagerConfig
{
    /* used for static connection between zenoh peers */
    std::string connectKey;
    std::string listenKey;

    ZenohSessionManagerConfig() : connectKey(), listenKey() {}
};

/**
 * ZenohSessionManager class is responsible for managing the zenoh session during life cycle of the process  
 * The class is used implicitly by the zenohUtransport and zenohRpcClient classes
 */

class ZenohSessionManager {

    public:

        /**
        * The API provides an instance of the zenoh session
        * @return instance of ZenohSessionManager
        */
        static ZenohSessionManager& instance() noexcept;

        /**
        * Initialize the zenoh session manager - the API should be called by any class that want to 
        * use zenoh 
        * @param sessionConfig provide configuration for the zenoh session
        * @return Returns OK on SUCCESS and ERROR on failure
        */
        UCode init(ZenohSessionManagerConfig &sessionConfig) noexcept;

        /**
        * Terminates the zenoh session manager - the API should be called by any class that called init
        * @return Returns OK on SUCCESS and ERROR on failure
        */
        UCode term() noexcept;

        /**
        * Get instance of the zenoh session
        * @return Returns session if on SUCCESS and nullopt on failure
        */
        std::optional<z_owned_session_t> getSession() noexcept;

    private:

        z_owned_session_t session_;
        atomic_uint32_t refCount_ = 0;
        std::mutex mutex_;
};

#endif /*_ZENOH_SESSION_MANAGER_H_*/