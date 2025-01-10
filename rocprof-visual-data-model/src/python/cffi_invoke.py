import cffi_lib
import time
import argparse

startTime = 0
endTime = 0
startTrack = 0
endTrack = 0

@cffi_lib.ffi.callback("void(int, char*)")
def progressCallback(percent, message):
    print("{}% - {}".format(percent,cffi_lib.ffi.string(message).decode('utf-8').ljust(80)), end='\r', flush=True)

@cffi_lib.ffi.callback("void(int, char*)")
def queryCallback(percent, message):
    print("Query : {}".format(cffi_lib.ffi.string(message).decode('utf-8').ljust(80)))

def generateChunkConfig(trace, db, numChunksX, numChunksY, chunkX, chunkY):
    numOfTracks = cffi_lib.lib.get_number_of_tracks(trace)
    traceMinTime = cffi_lib.lib.get_trace_min_time(trace)
    traceMaxTime = cffi_lib.lib.get_trace_max_time(trace)
    traceTime = traceMaxTime - traceMinTime
    sizeX = traceTime/numChunksX
    sizeY = numOfTracks/numChunksY
    if sizeY < 1: 
        return False
    if sizeX < 1000000: 
        return False
    global startTime
    global endTime 
    global startTrack
    global endTrack
    startTime =  int(traceMinTime+chunkX*sizeX)
    endTime = int(startTime + sizeX)
    startTrack = int(chunkY*sizeY)
    endTrack = int(chunkY*sizeY+sizeY)
    cffi_lib.lib.configure_trace_read_time_period(db, startTime, endTime)
    print("Reading {}ns chunk starting at {}".format(sizeX,startTime), end='')
    print(" for tracks {",end='')
    for i in range(startTrack, endTrack):
        cffi_lib.lib.add_track_to_trace_read_config(db, i)
        print("{},".format(i),end='')
    print("}")
    return True

def testChunk(trace, db, allthrough):
    t0 = time.time()
    if allthrough:
        cffi_lib.lib.read_trace_chunk_all_tracks(db,  queryCallback)
    else:
        cffi_lib.lib.read_trace_chunk_track_by_track(db, queryCallback)

    t1 = time.time()
    print("Reading trace took {} seconds".format(t1-t0))

    totalMemoryFootprint = 0
    totalNumberOfRecords = 0
    for i in range(startTrack,endTrack):
        track = cffi_lib.lib.get_track_at(trace, i)
        if track != 0:
            groupname = cffi_lib.ffi.string(cffi_lib.lib.get_track_group_name(track)).decode("utf-8")
            nodename = cffi_lib.ffi.string(cffi_lib.lib.get_track_name(track)).decode("utf-8")
            chunk = cffi_lib.lib.get_track_last_acquaired_chunk(track)
            if chunk != 0:
                memoryFootprint = cffi_lib.lib.get_chunk_memory_footprint(chunk)
                numberOfRecords = cffi_lib.lib.get_chunk_number_of_records(chunk)
                totalMemoryFootprint += memoryFootprint
                totalNumberOfRecords += numberOfRecords
                print("{}:{} has {} records, {} of memory".format(groupname, nodename, numberOfRecords, memoryFootprint))

    print("Total for this chunk: {} records, {} of memory".format(totalNumberOfRecords, totalMemoryFootprint))
    cffi_lib.lib.delete_trace_chunks_at(trace, startTime)


def intInput(message):
    valid = False
    answer = 0
    while valid!=True:
        try:
            answer = int(input(message))
            valid = True
        except:
            valid = False
    return answer

parser = argparse.ArgumentParser(description='Test visualization tool data model CFFI interface')
parser.add_argument('input_rpd', type=str, help="input rpd db")
args = parser.parse_args()

trace = cffi_lib.lib.create_trace()
if trace != None:
    db = cffi_lib.lib.open_rocpd_database(args.input_rpd.encode('ascii'))
    if db != None:
        if cffi_lib.lib.bind_trace_to_database(trace, db):
            start = time.time()
            if cffi_lib.lib.read_trace_properties(db,progressCallback):
                end = time.time()
                print()
                print("Reading trace properties took {} s".format(end-start))
                run = 1
                numChunksX=1
                numChunksY=1
                while run > 0:
                    numChunksX=intInput("Enter amount of chunks to split trace horizontally:")
                    numChunksY=intInput("Enter amount of chunks to split trace vertically:")
                    for x in range(numChunksX):
                        for y in range(numChunksY):
                            if generateChunkConfig(trace, db, numChunksX, numChunksY, x, y) == True:
                                testChunk(trace, db, True)
                    run = intInput("Enter 1 to continue and 0 to exit:")

            else:
                print("Error : Failed to bind trace to database!")
        else:
            print("Error : Failed to bind trace to database!")
    else:
        print("Error : Failed to open database!")

        cffi_lib.lib.close_rocpd_database(db)
else:
    print("Error : Failed to create trace!")
