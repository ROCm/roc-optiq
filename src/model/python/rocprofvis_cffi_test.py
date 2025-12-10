# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT
import cffi_lib
import time
import argparse
from cffi import FFI

@cffi_lib.ffi.callback("void(rocprofvis_db_filename_t, rocprofvis_db_progress_percent_t, rocprofvis_db_status_t, rocprofvis_db_status_message_t)")
def db_progress (db_name,  progress,  status,  msg):
    print("[{}] {}% - {}".format(cffi_lib.ffi.string(db_name).decode('utf-8').ljust(20), progress,cffi_lib.ffi.string(msg).decode('utf-8').ljust(80)), end='\r', flush=True)

parser = argparse.ArgumentParser(description='Test rocprofvis data model CFFI interface')
parser.add_argument('input_rpd', type=str, help="input rpd database")
args = parser.parse_args()

ffi = FFI()
trace = cffi_lib.lib.rocprofvis_dm_create_trace()
if trace != None:
    print(args.input_rpd.encode('ascii'))
    db = cffi_lib.lib.rocprofvis_db_open_database(args.input_rpd.encode('ascii'), cffi_lib.lib.kAutodetect)
    if db != None:
        if cffi_lib.lib.rocprofvis_dm_bind_trace_to_database(trace, db) == cffi_lib.lib.kRocProfVisDmResultSuccess:
            object2wait = cffi_lib.lib.rocprofvis_db_future_alloc(db_progress)
            if object2wait:
                if cffi_lib.lib.rocprofvis_db_read_metadata_async(db, object2wait) == cffi_lib.lib.kRocProfVisDmResultSuccess:
                    if cffi_lib.lib.rocprofvis_db_future_wait(object2wait, 10) == cffi_lib.lib.kRocProfVisDmResultSuccess:
                        start_time = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(trace, cffi_lib.lib.kRPVDMStartTimeUInt64, 0)
                        end_time = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(trace, cffi_lib.lib.kRPVDMEndTimeUInt64, 0)
                        print("Trace start time = {}, end time = {}".format(start_time, end_time))
                        num_tracks = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(trace, cffi_lib.lib.kRPVDMNumberOfTracksUInt64, 0)
                        for i in range(num_tracks):
                            track = cffi_lib.lib.rocprofvis_dm_get_property_as_handle(trace, cffi_lib.lib.kRPVDMTrackHandleIndexed, i)
                            if track!=None:
                                id = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(track, cffi_lib.lib.kRPVDMTrackIdUInt64, 0)
                                node = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(track, cffi_lib.lib.kRPVDMTrackNodeIdUInt64, 0)
                                category  = cffi_lib.ffi.string(cffi_lib.lib.rocprofvis_dm_get_property_as_charptr(track, cffi_lib.lib.kRPVDMTrackCategoryEnumCharPtr, 0)).decode("utf-8")
                                process = cffi_lib.ffi.string(cffi_lib.lib.rocprofvis_dm_get_property_as_charptr(track, cffi_lib.lib.kRPVDMTrackMainProcessNameCharPtr, 0)).decode("utf-8")
                                subprocess = cffi_lib.ffi.string(cffi_lib.lib.rocprofvis_dm_get_property_as_charptr(track, cffi_lib.lib.kRPVDMTrackSubProcessNameCharPtr, 0)).decode("utf-8")
                                print("Track id={} node={} category={} process={} subprocess={}".format(id, node, category, process, subprocess))
                        tracks = []
                        for i in range(num_tracks):
                            tracks.append(i)
                        tracks_selection = ffi.new("uint32_t[]", tracks)
                        if  cffi_lib.lib.rocprofvis_db_read_trace_slice_async(db, start_time, end_time, num_tracks, tracks_selection, object2wait) == cffi_lib.lib.kRocProfVisDmResultSuccess:
                            if cffi_lib.lib.rocprofvis_db_future_wait(object2wait, 10) == cffi_lib.lib.kRocProfVisDmResultSuccess:
                                for i in range(num_tracks):
                                    track = cffi_lib.lib.rocprofvis_dm_get_property_as_handle(trace, cffi_lib.lib.kRPVDMTrackHandleIndexed, tracks_selection[i])
                                    if track!=None:
                                        id = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(track, cffi_lib.lib.kRPVDMTrackIdUInt64, 0)
                                        slice = cffi_lib.lib.rocprofvis_dm_get_property_as_handle(track, cffi_lib.lib.kRPVDMSliceHandleTimed, start_time)
                                        if slice!=None:
                                            num_records = cffi_lib.lib.rocprofvis_dm_get_property_as_uint64(slice, cffi_lib.lib.kRPVDMNumberOfRecordsUInt64, 0); 
                                            print("Track id={} has {} records".format(id, num_records))
                cffi_lib.lib.rocprofvis_db_future_free(object2wait)
    cffi_lib.lib.rocprofvis_dm_delete_trace(trace)

