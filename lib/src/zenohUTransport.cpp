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

#include <up-client-zenoh-cpp/transport/zenohUTransport.h>
#include <up-client-zenoh-cpp/session/zenohSessionManager.h>
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <up-client-zenoh-cpp/uri/zenohUri.h>
#include <up-cpp/uri/builder/BuildUResource.h>
#include <up-cpp/uri/builder/BuildUUri.h>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <spdlog/spdlog.h>
#include <zenoh.h>
#include <up-core-api/uattributes.pb.h>

// Only for testing
#include <up-cpp/uri/serializer/LongUriSerializer.h>

using namespace std;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::v1;
using namespace uprotocol::utransport;

ZenohUTransport::ZenohUTransport() noexcept {
    /* by default initialized to empty strings */
    ZenohSessionManagerConfig sessionConfig;

    if (UCode::OK != ZenohSessionManager::instance().init(sessionConfig)) {
        spdlog::error("zenohSessionManager::instance().init() failed");
        uSuccess_.set_code(UCode::INTERNAL);
    }

    if (ZenohSessionManager::instance().getSession().has_value()) {
        session_ = ZenohSessionManager::instance().getSession().value();
    } else {
        uSuccess_.set_code(UCode::INTERNAL);
    }
    
    uSuccess_.set_code(UCode::OK);
}

ZenohUTransport::~ZenohUTransport() noexcept {

    for (auto pub : pubHandleMap_) {
        if (0 != z_undeclare_publisher(z_move(pub.second))) {
            //TODO - print the URI that failed
            spdlog::error("z_undeclare_publisher failed");
        }
    }

    pubHandleMap_.clear();

    for (auto listenerInfo : listenerMap_) {
        for (auto sub : listenerInfo.second->subVector_) {
            if (0 != z_undeclare_subscriber(z_move(sub))) {
                spdlog::error("z_undeclare_publisher failed");
            } else {
                spdlog::debug("z_undeclare_subscriber done");
            }
         }

         listenerInfo.second->listenerVector_.clear();
         listenerInfo.second->subVector_.clear();
    }

    listenerMap_.clear();

    if (UCode::OK != ZenohSessionManager::instance().term()) {
        spdlog::error("zenohSessionManager::instance().term() failed");
    }

    spdlog::info("ZenohUTransport destructor done");
}

UStatus ZenohUTransport::send(const UMessage &message) noexcept {
    
    UStatus status;

    if (UMessageType::UMESSAGE_TYPE_PUBLISH == message.attributes().type()) {
        status.set_code(sendPublish(message));
    } else if (UMessageType::UMESSAGE_TYPE_RESPONSE == message.attributes().type()) {
         if (false == isRPCMethod(message.attributes().sink())) {
             spdlog::error("message defined as response but the URI is not RPC ");
             auto sink = message.attributes().sink();
             spdlog::info("Sink (micro UUri): {}", toZenohKeyString(sink));
             spdlog::info("Sink (long UUri): {}", LongUriSerializer::serialize(sink));
             return status;
         }
         status.set_code(sendQueryable(message));
    } else {
        spdlog::error("message type is not supported");
        status.set_code(UCode::INTERNAL);
    }

    return status;
}

UCode ZenohUTransport::sendPublish(const UMessage &message) noexcept {

    UCode status = UCode::UNAVAILABLE;

    do {
      
        if ((0 == message.payload().size()) || (nullptr == message.payload().data())) {
            spdlog::error("payload not valid");
            break;
        }

        /* get key and check if the publisher for the URI is already exists */
        auto key = uprotocol::uri::toZenohKeyString(message.attributes().source());
        if (key.empty()) {
            spdlog::error("failed to convert UUri to zenoh key");
            break;
        }
        auto handleInfo = pubHandleMap_.find(key);

        z_owned_publisher_t pub;

        /* check if the publisher exists */
        if (handleInfo == pubHandleMap_.end()) {
            std::lock_guard<std::mutex> lock(pubInitMutex_);

            if (handleInfo != pubHandleMap_.end()) {
                pub = handleInfo->second;
            } else {

                pub = z_declare_publisher(z_loan(session_), z_keyexpr(key.c_str()), nullptr);
                if (false == z_check(pub)) {
                    spdlog::error("Unable to declare Publisher for key expression!");
                    break;
                }
                pubHandleMap_[key] = pub;
            }
        } else {
            pub = handleInfo->second;
        }

        z_publisher_put_options_t options = z_publisher_put_options_default();

        z_owned_bytes_map_t map = z_bytes_map_new();
        options.attachment = z_bytes_map_as_attachment(&map);

        // Serializing UAttributes
        size_t attrSize = message.attributes().ByteSizeLong();
        std::vector<uint8_t> serializedAttributes(attrSize);
        if (!message.attributes().SerializeToArray(serializedAttributes.data(), attrSize)) {
            spdlog::error("SerializeToArray failure");
            return UCode::INTERNAL;
        }

        z_bytes_t attrBytes = {.len = serializedAttributes.size(), .start = serializedAttributes.data()};
        z_bytes_map_insert_by_alias(&map, z_bytes_new("attributes"), attrBytes);
    
        // Publish the message
        if (0 != z_publisher_put(z_loan(pub), message.payload().data(), message.payload().size(), &options)) {
            spdlog::error("z_publisher_put failed");
            z_drop(z_move(map));
            break;
        }

        z_drop(z_move(map));
        status = UCode::OK;
        
    } while (0);
   
    return status;
}

UCode ZenohUTransport::sendQueryable(const UMessage &message) noexcept {

    auto uuidStr = UuidSerializer::serializeToString(message.attributes().id());
    if (queryMap_.find(uuidStr) == queryMap_.end()) {
        spdlog::error("failed to find UUID = {}", uuidStr);
        return UCode::UNAVAILABLE;
    }

    auto query = queryMap_[uuidStr];

    z_query_reply_options_t options = z_query_reply_options_default();

    if (UCode::OK != mapEncoding(message.payload().format(), options.encoding)) {
        spdlog::error("mapEncoding failure");
        return UCode::INTERNAL;
    }

    // Serialize the UAttributes
    size_t attrSize = message.attributes().ByteSizeLong();
    std::vector<uint8_t> serializedAttributes(attrSize);
    if (!message.attributes().SerializeToArray(serializedAttributes.data(), attrSize)) {
        spdlog::error("SerializeToArray failure");
        return UCode::INTERNAL;
    }

    z_owned_bytes_map_t map = z_bytes_map_new();
    z_bytes_t attrBytes = {.len = serializedAttributes.size(), .start = serializedAttributes.data()};
    z_bytes_map_insert_by_alias(&map, z_bytes_new("attributes"), attrBytes);
    
    options.attachment = z_bytes_map_as_attachment(&map);

    z_query_t lquery = z_loan(query);

    if (0 != z_query_reply(&lquery, z_query_keyexpr(&lquery), message.payload().data(), message.payload().size(), &options)) {
        spdlog::error("z_query_reply failed");
        z_drop(z_move(map));
        return UCode::INTERNAL;
    }

    auto keyStr = z_keyexpr_to_string(z_query_keyexpr(&lquery));

    z_drop(z_move(keyStr));
    z_drop(z_move(query));

    spdlog::debug("replied on query with uid = {}", uuidStr);
    /* once replied remove the uuid from the map, as it cannot be reused */
    queryMap_.erase(uuidStr);

    z_drop(z_move(map));

    return UCode::OK;
}

UStatus ZenohUTransport::registerListener(const UUri &uri,
                                          const UListener &listener) noexcept {
   
    UStatus status;

    cbArgumentType* arg;
    std::shared_ptr<ListenerContainer> listenerContainer;

    do {

        status.set_code(UCode::OK);
        arg = nullptr;
        listenerContainer = nullptr;

        std::lock_guard<std::mutex> lock(subInitMutex_);

        auto key = uprotocol::uri::toZenohKeyString(uri);
        if (key.empty()) {
            spdlog::error("failed to convert UUri to zenoh key");
            break;
        }

        // check if URI exists 
        if (listenerMap_.find(key) != listenerMap_.end()) {

            listenerContainer = listenerMap_[key];

            for (const UListener *existingListenerPtr : listenerContainer->listenerVector_) {
                if (existingListenerPtr == &listener) {
                    spdlog::error("listener already set for URI");
                    status.set_code(UCode::INVALID_ARGUMENT);
                    break;
                }
            }
            
            if (UCode::OK != status.code()) {
               break;
            }
        }

        if (nullptr == listenerContainer) {
            listenerContainer = make_shared<ListenerContainer>();
            if (nullptr == listenerContainer) {
                spdlog::error("listenerContainer allocation failure");
                status.set_code(UCode::INTERNAL);
                break;
            }
        }

        std::shared_ptr<UUri> copyUri = make_shared<UUri>(uri);
        
        arg = new cbArgumentType(copyUri, this, listener);
        if (nullptr == arg) {
            spdlog::error("failed to allocate arguments for callback");
            status.set_code(UCode::INTERNAL);
            break;
        }

        /* listener for a regular pub-sub*/
        if (false == isRPCMethod(uri.resource())) {

            z_owned_closure_sample_t callback = z_closure(SubHandler, OnSubscriberClose, arg);

            auto sub = z_declare_subscriber(z_loan(session_), z_keyexpr(key.c_str()), z_move(callback), nullptr);
            if (!z_check(sub)) {
                spdlog::error("z_declare_subscriber failed");
                status.set_code(UCode::INTERNAL);
                break;
            }
            
            listenerContainer->subVector_.push_back(sub);
            listenerContainer->listenerVector_.push_back(&listener);

        // } else if (typeid(listener) == typeid(RequestListener)) {
        } else {

            z_owned_closure_query_t callback = z_closure(QueryHandler, OnQueryClose, arg);
        
            auto qable = z_declare_queryable(z_loan(session_), z_keyexpr(key.c_str()), z_move(callback), nullptr);
            if (!z_check(qable)) {
                spdlog::error("failed to create queryable");
                status.set_code(UCode::INTERNAL);
                break;
            }

            listenerContainer->queryVector_.push_back(qable);
            listenerContainer->listenerVector_.push_back(&listener);
        }

        listenerMap_[key] = listenerContainer;

        status.set_code(UCode::OK);

    } while(0);

    if (UCode::OK != status.code()) {
        
        if (nullptr != listenerContainer) {
            listenerContainer.reset();
        }

        if (nullptr != arg) {
            delete arg;
        }
    }

    return status;
}

UStatus ZenohUTransport::unregisterListener(const UUri &uri, 
                                            const UListener &listener) noexcept {

    UStatus status;

    std::shared_ptr<ListenerContainer> listenerContainer;

    auto key = uprotocol::uri::toZenohKeyString(uri);

    if (key.empty() || listenerMap_.find(key) == listenerMap_.end()) {
        status.set_code(UCode::INVALID_ARGUMENT);
        return status;
    }

    listenerContainer = listenerMap_[key];

    int32_t index = 0;

    /* need to check with who the listener is associated */
    for (const UListener *existingListenerPtr : listenerContainer->listenerVector_) {

        if (&listener == existingListenerPtr) {

            listenerContainer->listenerVector_.erase(listenerContainer->listenerVector_.begin() + index);

            if (false == listenerContainer->subVector_.empty()){
                z_undeclare_subscriber(z_move(listenerContainer->subVector_[index]));
                listenerContainer->subVector_.erase(listenerContainer->subVector_.begin() + index);
            } else {
                z_undeclare_queryable(z_move(listenerContainer->queryVector_[index]));
                listenerContainer->queryVector_.erase(listenerContainer->queryVector_.begin() + index);
            }
            break;
        }

        ++index;
    }

    status.set_code(UCode::OK);
    return status;
}

void ZenohUTransport::SubHandler(const z_sample_t* sample, void* arg) {

    if ((nullptr == sample) || (nullptr == arg)) {
       spdlog::error("Invalid arguments for SubHandler");
       return;
    }

    if (!z_check(sample->attachment)) {
       spdlog::error("No attachments found");
       return;
    }

    z_bytes_t serializedAttributes = z_attachment_get(sample->attachment, z_bytes_new("attributes"));
    if (serializedAttributes.len == 0 || serializedAttributes.start == nullptr) {
       spdlog::error("Serialized attributes not found in the attachment");
       return;
    }

    uprotocol::v1::UAttributes attributes;
    if (!attributes.ParseFromArray(serializedAttributes.start, serializedAttributes.len)) {
       spdlog::error("ParseFromArray failure");
       return;
    }

    UPayload payload{sample->payload.start, sample->payload.len, UPayloadType::REFERENCE};
   
    cbArgumentType *tuplePtr = static_cast<cbArgumentType*>(arg);

    auto listener = &get<2>(*tuplePtr);

    UMessage message(payload, attributes);

    // Pass the parsed headers and payload to the listener's onReceive method
    if (UCode::OK != listener->onReceive(message).code()) {
       spdlog::error("listener->onReceive failed");
       /* TODO handle error */
    }
}

void ZenohUTransport::QueryHandler(const z_query_t *query, void *arg) {
    cbArgumentType *tuplePtr = static_cast<cbArgumentType*>(arg);

    z_attachment_t attachment = z_query_attachment(query);
    if (false == z_check(attachment)) {
       spdlog::error("z_query_attachment does not exist");
       return;
    }

    z_bytes_t serializedAttributes = z_attachment_get(attachment, z_bytes_new("attributes"));

    if (serializedAttributes.len == 0 || serializedAttributes.start == nullptr) {
       spdlog::error("Serialized attributes not found in the attachment");
       return;
    }

    uprotocol::v1::UAttributes attributes;
    if (!attributes.ParseFromArray(serializedAttributes.start, serializedAttributes.len)) {
       spdlog::error("ParseFromArray failure");
       return;
    }

    z_value_t payloadValue = z_query_value(query);

    UPayload payload(payloadValue.payload.start, payloadValue.payload.len, UPayloadType::REFERENCE);

    auto uri = get<0>(*tuplePtr);
    auto instance = get<1>(*tuplePtr);
    auto listener = &get<2>(*tuplePtr);

    auto uuidStr = UuidSerializer::serializeToString(attributes.id());

    instance->queryMap_[uuidStr] = z_query_clone(query);

    if (UMessageType::UMESSAGE_TYPE_REQUEST != attributes.type()) {
       spdlog::error("Wrong message type = {}", static_cast<int>(attributes.type()));
       return;
    }

    UMessage message(payload, attributes);

    if (UCode::OK != listener->onReceive(message).code()) {
       /*TODO error handling*/
       spdlog::error("onReceive failure");
       return;
    }
}

UCode ZenohUTransport::mapEncoding(const UPayloadFormat &payloadFormat, 
                                   z_encoding_t &encoding) noexcept {
    switch (payloadFormat) {
        case UPayloadFormat::PROTOBUF:
        case UPayloadFormat::PROTOBUF_WRAPPED_IN_ANY:
            encoding = z_encoding(Z_ENCODING_PREFIX_APP_OCTET_STREAM, nullptr);
            break;
        case UPayloadFormat::JSON:
            encoding = z_encoding(Z_ENCODING_PREFIX_APP_JSON, nullptr);
            break;
        case UPayloadFormat::SOMEIP:
        case UPayloadFormat::SOMEIP_TLV:
            encoding = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, nullptr);
            break;
        case UPayloadFormat::RAW:
            encoding = z_encoding(Z_ENCODING_PREFIX_APP_OCTET_STREAM, nullptr);
            break;
        case UPayloadFormat::TEXT:
            encoding = z_encoding(Z_ENCODING_PREFIX_TEXT_PLAIN, nullptr);
            break;
        default:
            spdlog::error("wrong format provided");
            return UCode::UNAVAILABLE;
    }

    return UCode::OK;
}

void ZenohUTransport::OnSubscriberClose(void *arg) {

    if (nullptr == arg) {
        spdlog::error("arg is nullptr");
    } else {

        cbArgumentType *info = 
            reinterpret_cast<cbArgumentType*>(arg);

        delete info;
    }
}

void ZenohUTransport::OnQueryClose(void *arg) {

    if (nullptr == arg) {
        spdlog::error("arg is nullptr");
    } else {

        cbArgumentType *info = 
            reinterpret_cast<cbArgumentType*>(arg);

        delete info;
    }
}
