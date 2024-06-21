# SPDX-FileCopyrightText: 2024 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0

enable_testing()

find_package(GTest REQUIRED)
include(GoogleTest)

# Invoked as add_coverage_test("SomeName" sources...)
function(add_coverage_test Name)
    add_executable(${Name} ${ARGN})
    target_compile_options(${Name} PRIVATE -g -Og)
    target_link_libraries(${Name}
        PUBLIC
        up-core-api::up-core-api
        up-cpp::up-cpp
        up-cpp::up-transport-zenoh-cpp
        zenohc::lib
        zenohcxx::zenohc::lib
        spdlog::spdlog
        protobuf::protobuf
        PRIVATE
        GTest::gtest_main
        GTest::gmock
        pthread
    )
    target_include_directories(${Name}
		PRIVATE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		${zenohc_INCLUDE_DIR}
		${zenohcxx_INCLUDE_DIR}
		${up-cpp_INCLUDE_DIR}
		${up-core-api_INCLUDE_DIR}
		${protobuf_INCLUDE_DIR}
		${spdlog_INCLUDE_DIR}
	)
    gtest_discover_tests(${Name} XML_OUTPUT_DIR results)
endfunction()

# NOTE: This is temporarily just a call to add_coverage_test. When coverage
#       reporting is added, this might be changed.
function(add_extra_test Name)
    add_coverage_test(${Name} ${ARGN})
endfunction()

########################### COVERAGE ##########################################
# Transport
add_coverage_test("ZenohUTransportTest" coverage/ZenohUTransportTest.cpp)

########################## EXTRAS #############################################
add_extra_test("PublisherSubscriberTest" extra/PublisherSubscriberTest.cpp)
add_extra_test("NotificationTest" extra/NotificationTest.cpp)
add_extra_test("RpcClientServerTest" extra/RpcClientServerTest.cpp)
