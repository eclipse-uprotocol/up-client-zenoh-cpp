# Copyright (c) 2024 General Motors GTO LLC
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
cmake_minimum_required(VERSION 3.20)
project(up-client-zenoh-cpp VERSION 0.1.0  LANGUAGES CXX DESCRIPTION "C++ client library to connect to the zenoh implementation of uProtocol.")

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    # message(STATUS "This is the root CMakeLists.txt file; We can set project wide settings here.")
    set(CMAKE_CXX_STANDARD 17)
	if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) # Check if the file exists
    	include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) # Include the file
		conan_basic_setup() # Set up Conan
	endif()
	# place libraries in a lib directory and executables in a bin directory,
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
else()
    # message(STATUS "This is NOT the root CMakeLists.txt file; We should get project wide settings from project root.")
endif()
    
add_subdirectory(lib)

if(BUILD_TESTING)
	enable_testing()
	#add_subdirectory(test)
endif()