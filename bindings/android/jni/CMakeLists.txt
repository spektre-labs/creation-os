# SPDX-License-Identifier: AGPL-3.0-or-later
#
# CMake build for the Android NDK shared library.  Pairs with
# app/build.gradle's `externalNativeBuild { cmake { ... } }` block.
cmake_minimum_required(VERSION 3.22.1)
project(spektre_surface LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Adjust these paths if you copy the tree into your app module.
set(KERNEL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(V76_ROOT    ${KERNEL_ROOT}/src/v76)
set(LIC_ROOT    ${KERNEL_ROOT}/src/license_kernel)

add_library(spektre_surface SHARED
    spektre_surface_jni.c
    ${V76_ROOT}/surface.c
    ${LIC_ROOT}/license_attest.c
)

target_include_directories(spektre_surface PRIVATE
    ${V76_ROOT}
    ${LIC_ROOT}
)

target_compile_options(spektre_surface PRIVATE
    -O2 -Wall -Wextra -fPIC
)

find_library(log-lib log)
target_link_libraries(spektre_surface ${log-lib})
