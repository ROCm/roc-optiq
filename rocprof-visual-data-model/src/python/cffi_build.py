import cffi
import pathlib

ffi = cffi.FFI()
win_lib_dir = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.22621.0"
this_dir = pathlib.Path().absolute()
lib_dir = this_dir / "../../build/Release"
lib_name = "cffi_lib"
h_file_name = "../data/CffiInterface.h"
with open(h_file_name) as h_file:
    ffi.cdef(h_file.read())
h_file_name = "../database/CffiInterface.h"
with open(h_file_name) as h_file:
    ffi.cdef(h_file.read())

ffi.set_source(
lib_name,
'''
#include "../data/CffiInterface.h"
#include "../database/CffiInterface.h"
''',
libraries=["rocprof-visual-data-model-lib"],
library_dirs=[lib_dir.as_posix(),win_lib_dir+"\\um\\x64\\",win_lib_dir+"\\ucrt\\x64\\"],
)

ffi.compile()


