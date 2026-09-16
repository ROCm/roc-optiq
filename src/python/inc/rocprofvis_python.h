// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

/*
 * Include this after the C++ standard library headers. Stock CPython on
 * Windows does not ship python3xx_d.lib; including Python.h with _DEBUG
 * set injects a pragma for that import library.
 */
#if defined(_MSC_VER) && defined(_DEBUG)
#    undef _DEBUG
#    include <Python.h>
#    define _DEBUG
#else
#    include <Python.h>
#endif
