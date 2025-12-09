# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT
import cffi
import pathlib
import platform

ffi = cffi.FFI()
this_dir = pathlib.Path().absolute()
lib_dir = this_dir
target_lib_name = "cffi_lib"
clib_name = "rocprofvis-datamodel"
link_flags = []
if platform.system() == "Windows":
    lib_dir = this_dir / "Release"
    win_gen_lib_dir = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.22621.0"
    lib_dirs = [lib_dir.as_posix(),win_gen_lib_dir+"\\um\\x64\\",win_gen_lib_dir+"\\ucrt\\x64\\"]
    
elif platform.system() == "Linux":
    lib_dirs = [lib_dir.as_posix()]
    link_flags = ['-W', '-Wno-undef', '-lstdc++']

h_file_name = "../inc/rocprofvis_interface_types.h"
with open(h_file_name) as h_file:
    ffi.cdef(h_file.read())
h_file_name = "../inc/rocprofvis_interface.h"
with open(h_file_name) as h_file:
    ffi.cdef(h_file.read())

ffi.set_source(
target_lib_name,
'''
#include "../inc/rocprofvis_interface_types.h"
#include "../inc/rocprofvis_interface.h"
''',
libraries=[clib_name],
library_dirs=lib_dirs,
extra_link_args = link_flags,
)

ffi.compile()