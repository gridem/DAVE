# Copyright 2013-2016 Grigory Demchenko (aka gridem)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(dave)
cmake_minimum_required(VERSION 2.8)

option(STATIC_ALL "Use static libraries" ON)
option(LOG_DEBUG "Use debug output" OFF)

if(LOG_DEBUG)
    add_definitions(-DflagLOG_DEBUG)
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(GCC_LIKE_COMPILER ON)
endif()

if(NOT MSVC AND NOT GCC_LIKE_COMPILER)
    message(FATAL_ERROR "Supports only gcc, clang or msvc compilers")
endif()

if(STATIC_ALL)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
    if(MSVC)
        set(MSVC_RUNTIME static)
    endif()
else()
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF)
    if(MSVC)
        set(MSVC_RUNTIME dynamic)
    endif()
endif()

if(MSVC)
    include_directories($ENV{INCLUDE})
    add_definitions(-DflagMSC)
    include(MSVCRuntime.cmake)
    configure_msvc_runtime()
endif()

if(GCC_LIKE_COMPILER)
    add_definitions(-std=c++14)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        add_definitions(-stdlib=libc++)
    endif()
endif()

find_package(Boost 1.56 REQUIRED)

file(GLOB SRC src/*)

include_directories(${Boost_INCLUDE_DIR})

add_executable(dave ${SRC}) 
