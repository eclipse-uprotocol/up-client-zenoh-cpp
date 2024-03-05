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

#ifndef _ZENOH_UTRANSPORT_
#define _ZENOH_UTRANSPORT_

#include <cstddef>
#include <unordered_map>
#include <atomic>
#include <zenoh.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-cpp/transport/UTransport.h>

using namespace uprotocol::v1;
using namespace std;
using namespace uprotocol::utransport;

class ListenerContainer {
    public:
        std::vector<z_owned_subscriber_t> subVector_;
        std::vector<z_owned_queryable_t> queryVector_;
        std::vector<const UListener*> listenerVector_;
};

class ZenohUTransport : public UTransport {

    public:

        ZenohUTransport(const ZenohUTransport&) = delete;
        ZenohUTransport& operator=(const ZenohUTransport&) = delete;

        /**
        * The API provides an instance of the zenoh session
        * @return instance of ZenohUTransport
        */
	static ZenohUTransport& instance(void) noexcept;

        /**
        * init the zenohUTransport 
        * @return Returns OK on SUCCESS and ERROR on failure
        */
        UStatus init() noexcept;

        /**
        * Terminates the zenoh utransport  - the API should be called by any class that called init
        * @return Returns OK on SUCCESS and ERROR on failure
        */
        UStatus term() noexcept; 

        /**
        * Transmit UPayload to the topic using the attributes defined in UTransportAttributes.
        * @param topic Resolved UUri topic to send the payload to.
        * @param payload Actual payload.
        * @param attributes Additional transport attributes.
        * @return Returns OKSTATUS if the payload has been successfully sent (ACK'ed), otherwise it
        * returns FAILSTATUS with the appropriate failure.
        */
        UStatus send(const UUri &uri, 
                     const UPayload &payload,
                     const UAttributes &attributes) noexcept;

        /**
        * Register listener to be called when UPayload is received for the specific topic.
        * @param topic Resolved UUri for where the message arrived via the underlying transport technology.
        * @param listener The method to execute to process the date for the topic.
        * @return Returns OKSTATUS if the listener is unregistered correctly, otherwise it returns FAILSTATUS
        * with the appropriate failure.
        */ 
        UStatus registerListener(const UUri &uri,
                                 const UListener &listener) noexcept;

        /**
        * Unregister a listener for a given topic. Messages arriving on this topic will no longer be processed
        * by this listener.
        * @param topic Resolved UUri for where the listener was registered to receive messages from.
        * @param listener The method to execute to process the date for the topic.
        * @return Returns OKSTATUS if the listener is unregistered correctly, otherwise it returns FAILSTATUS
        * with the appropriate failure.
        */
        UStatus unregisterListener(const UUri &uri, 
                                   const UListener &listener) noexcept;

        UStatus receive(const UUri &uri, 
                        const UPayload &payload, 
                        const UAttributes &attributes) noexcept;

    private:

	ZenohUTransport() {}

        static void OnSubscriberClose(void *arg);

        static void OnQueryClose(void *arg);

        static void QueryHandler(const z_query_t *query, 
                                 void *context);

        static void SubHandler(const z_sample_t* sample,
                               void* arg);

        UCode sendPublish(const UUri &uri, 
                          const UPayload &payload,
                          const UAttributes &attributes) noexcept;

        UCode sendQueryable(const UUri &uri, 
                            const UPayload &payload,
                            const UAttributes &attributes) noexcept;

        UCode mapEncoding(const UPayloadFormat &payloadFormat, 
                          z_encoding_t &encoding) noexcept;

        /* zenoh session handle*/
        z_owned_session_t session_;
        /* indicate that termination is pending so no new transactions are allowed*/
        atomic_bool termPending_ = false;
        /* how many send transactions are currently in progress*/
        atomic_uint32_t pendingSendRefCnt_ = 0;
        /* how many times uTransport was initialized*/
        atomic_uint32_t refCount_ = 0;
        
        std::mutex mutex_;

        using uuriKey = size_t;     
        using uuidStr = std::string;

        unordered_map<uuriKey, z_owned_publisher_t> pubHandleMap_;
        unordered_map<uuriKey, std::shared_ptr<ListenerContainer>> listenerMap_;  
        unordered_map<uuriKey, bool> authorized_;
        unordered_map<uuidStr, z_owned_query_t> queryMap_;

        std::mutex pubInitMutex_;
        std::mutex subInitMutex_;

        static constexpr auto termMaxRetries_ = size_t(10);
        static constexpr auto termRetryTimeout_ = std::chrono::milliseconds(100);

        using cbArgumentType = std::tuple<std::shared_ptr<UUri>, ZenohUTransport*, const UListener&>;
};

#endif /*_ZENOH_UTRANSPORT_*/
