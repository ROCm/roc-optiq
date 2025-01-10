#include "RocpdDatabase.h"
#include <sstream>
#include <cassert>

int RocpdDatabase::callback_get_min_time(void *data, int argc, char **argv, char **azColName){
    assert(argc==1 && "callback_get_min_time takes 1 arcument");
    RocpdDatabase* db = (RocpdDatabase*)data;
    db->minTime = std::min(std::stoll( argv[0] ),db->minTime);      
    return 0;
}

int RocpdDatabase::callback_get_max_time(void *data, int argc, char **argv, char **azColName){
    assert(argc==1 && "callback_get_max_time takes 1 arcument");
    RocpdDatabase* db = (RocpdDatabase*)data;
    db->maxTime = std::max(std::stoll( argv[0] ),db->maxTime);       
    return 0;
}

int RocpdDatabase::callback_add_string_count(void *data, int argc, char **argv, char **azColName){
    assert(argc==1 && "callback_add_string_count takes 1 argument");
    RocpdDatabase* db = (RocpdDatabase*)data;
    db->stringCount += std::stol( argv[0] );       
    return 0;
}

int RocpdDatabase::callback_add_cpu_track(void *data, int argc, char **argv, char **azColName){
    assert(argc==3 && "callback_add_cpu_track takes 3 arguments");
    TrackProperties props;
    RocpdDatabase* db = (RocpdDatabase*)data;
    uint32_t count =  (uint32_t)std::stol( argv[0] );
    props.data.Cpu.processId =  std::stol( argv[1] );
    props.data.Cpu.threadId =  std::stol( argv[2] ); 
    props.type=TrackType::CPU;
        
    db->trackProps.push_back(props);
    return db->m_bindData.funcAddTrack(db->m_bindData.handler, props, count) != INVALID_TRACK_INDEX ? 0 : 1;   
}

int RocpdDatabase::callback_add_gpu_track(void *data, int argc, char **argv, char **azColName){
    assert(argc==3 && "callback_add_gpu_track takes 3 arguments");
    TrackProperties props;
    RocpdDatabase* db = (RocpdDatabase*)data;
    uint32_t count =  (uint32_t)std::stol( argv[0] );
    props.data.Gpu.gpuId =  std::stol( argv[1] );
    props.data.Gpu.queueId =  std::stol( argv[2] );  
    props.type=TrackType::GPU;
    
    db->trackProps.push_back(props);
    return db->m_bindData.funcAddTrack(db->m_bindData.handler, props, count) != INVALID_TRACK_INDEX ? 0 : 1; 
}

int RocpdDatabase::callback_add_metric_track(void *data, int argc, char **argv, char **azColName){
    assert(argc==3 && "callback_add_metric_track takes 3 arguments");
    TrackProperties props;
    RocpdDatabase* db = (RocpdDatabase*)data;
    uint32_t count = (uint32_t)std::stol( argv[0] );
    std::string monitorType = argv[1];
    if ((props.data.Metric.metricId = db->m_bindData.funcAddStringRecord(db->m_bindData.handler, monitorType.c_str())) == INVALID_TRACK_INDEX) return 1;
    props.data.Metric.gpuId = std::stol(argv[2]);
    db->metricNames.push_back({ (uint32_t)db->trackProps.size(), props.data.Metric.gpuId, monitorType });  
    props.type=TrackType::METRICS;
    db->trackProps.push_back(props);
    return db->m_bindData.funcAddTrack(db->m_bindData.handler, props, count) != INVALID_TRACK_INDEX ? 0 : 1; 

}

int RocpdDatabase::callback_add_kernel_launch_record(void *data, int argc, char **argv, char **azColName){
    assert(argc==5 && "callback_add_kernel_launch_record takes 5 arguments");

    RocpdDatabase* db = (RocpdDatabase*)data;
    DataRecord record;
    record.Cpu.timestamp =  std::stoll( argv[0] );
    record.Cpu.duration =  std::stoll( argv[1] ); 
    record.Cpu.id = std::stoll( argv[4] );
    if (db->convertStringId(std::stoll( argv[2]), record.Cpu.apiCallId ) &&
        db->convertStringId(std::stoll( argv[3] ), record.Cpu.argumentsId ))
    {
        if (db->m_bindData.funcAddTrackRecord(db->m_bindData.handler, db->trackId, RecordType::CPU, record ) == INVALID_RECORD_ID) return 1;  
    } 
    
    return 0;
}

int RocpdDatabase::callback_add_kernel_launch_record_all_tracks(void *data, int argc, char **argv, char **azColName){
    assert(argc==7 && "callback_add_kernel_launch_record_all_tracks takes 7 arguments");

    RocpdDatabase* db = (RocpdDatabase*)data;
    DataRecord record;
    record.Cpu.timestamp =  std::stoll( argv[0] );
    record.Cpu.duration =  std::stoll( argv[1] ); 
    record.Cpu.id = std::stoll( argv[4] );
    uint32_t pid = std::stol( argv[5] );
    uint32_t tid = std::stol( argv[6] );
    uint16_t trackId = db->getTrackIdByTrackProperties(TrackProperties{TrackType::CPU, pid, tid});
    if (trackId!=INVALID_TRACK_INDEX  && db->isTrackRequested(trackId))
    {
        if (db->convertStringId(std::stoll( argv[2]), record.Cpu.apiCallId ) &&
            db->convertStringId(std::stoll( argv[3] ), record.Cpu.argumentsId ))
        {
            if (db->m_bindData.funcAddTrackRecord(db->m_bindData.handler, trackId, RecordType::CPU, record ) == INVALID_RECORD_ID) return 1;  
        } 
    }   
    return 0;
}

int RocpdDatabase::callback_add_kernel_execute_record(void *data, int argc, char **argv, char **azColName){
    assert(argc==5 && "callback_add_kernel_execute_record takes 5 arguments");
    RocpdDatabase* db = (RocpdDatabase*)data;
    DataRecord record;
    record.Gpu.timestamp =  std::stoll( argv[0] );
    record.Gpu.duration =  std::stoll( argv[1] ); 
    record.Gpu.id = std::stoll( argv[4] );
    if (db->convertStringId(std::stoll( argv[2]), record.Gpu.typeId ) &&
        db->convertStringId(std::stoll( argv[3] ), record.Gpu.descriptionId ))
    {
        if (db->m_bindData.funcAddTrackRecord(db->m_bindData.handler,db->trackId, RecordType::GPU, record ) == INVALID_RECORD_ID) return 1;
    }            
    return 0;
}

int RocpdDatabase::callback_add_kernel_execute_record_all_tracks(void *data, int argc, char **argv, char **azColName){
    assert(argc==7 && "callback_add_kernel_execute_record_all_tracks takes 7 arguments");
    RocpdDatabase* db = (RocpdDatabase*)data;
    DataRecord record;
    record.Gpu.timestamp =  std::stoll( argv[0] );
    record.Gpu.duration =  std::stoll( argv[1] ); 
    record.Gpu.id = std::stoll( argv[4] );
    uint32_t gpuId = std::stol( argv[5] );
    uint32_t queueId = std::stol( argv[6] );
    uint16_t trackId = db->getTrackIdByTrackProperties(TrackProperties{TrackType::GPU, gpuId, queueId});
    if (trackId!=INVALID_TRACK_INDEX  && db->isTrackRequested(trackId))
    {
        if (db->convertStringId(std::stoll( argv[2]), record.Gpu.typeId ) &&
            db->convertStringId(std::stoll( argv[3] ), record.Gpu.descriptionId ))
        {
            if (db->m_bindData.funcAddTrackRecord(db->m_bindData.handler, trackId, RecordType::GPU, record ) == INVALID_RECORD_ID) return 1;
        } 
    }           

    return 0;
}

int RocpdDatabase::callback_add_metrics_record(void *data, int argc, char **argv, char **azColName){
    assert(argc==2 && "callback_add_kernel_metric_record takes 2 arguments");
    RocpdDatabase* db = (RocpdDatabase*)data;
    DataRecord record;
    record.Metric.timestamp =  std::stoll( argv[0] );
    record.Metric.value =  std::stod( argv[1] );

    return db->m_bindData.funcAddTrackRecord(db->m_bindData.handler,db->trackId, RecordType::METRICS, record ) != INVALID_RECORD_INDEX ? 0 : 1;   
}

int RocpdDatabase::callback_add_metrics_record_all_tracks(void *data, int argc, char **argv, char **azColName){
    assert(argc==4 && "callback_add_kernel_metric_record_all_tracks takes 4 arguments");
    RocpdDatabase* db = (RocpdDatabase*)data;
    DataRecord record;
    record.Metric.timestamp =  std::stoll( argv[0] );
    record.Metric.value =  std::stod( argv[1] );
    uint32_t deviceId = std::stol( argv[2] );
    uint16_t trackId = db->getTrackIdByMetricNameAndGpuId(argv[3],(uint16_t)deviceId);
    if (trackId != INVALID_TRACK_INDEX)
    {
        if (trackId!=INVALID_TRACK_INDEX && db->isTrackRequested(trackId))
        {
            return db->m_bindData.funcAddTrackRecord(db->m_bindData.handler,trackId, RecordType::METRICS, record ) != INVALID_RECORD_INDEX ? 0 : 1; 
        }
    }  
    return 0;
}

int RocpdDatabase::callback_add_strings_record(void *data, int argc, char **argv, char **azColName){
    assert(argc==2 && "callback_add_strings_record takes 2 arguments");

    RocpdDatabase* db = (RocpdDatabase*)data;
    std::string str = argv[0]; 
    std::stringstream ids(argv[1]);
    uint32_t stringId = db->m_bindData.funcAddStringRecord(db->m_bindData.handler, argv[0]);
    if (stringId == INVALID_RECORD_INDEX) return 1;
    
    for (uint64_t id; ids >> id;) {   
        db->stringRefs.insert(StringPair(id,stringId));
        if (ids.peek() == ',') ids.ignore();
    }

    return 0;
}

int RocpdDatabase::callback_add_flow_info(void *data, int argc, char **argv, char **azColName){
    assert(argc==8 && "callback_add_flow_info takes 2 arguments");
    RocpdDatabase* db = (RocpdDatabase*)data;
    uint64_t api_id =  std::stoll( argv[0] );
    uint64_t op_id =  std::stoll( argv[1] );
    uint32_t pid =  std::stol( argv[2] );
    uint32_t tid =  std::stol( argv[3] );
    uint32_t gpuid =  std::stol( argv[4] );
    uint32_t qid =  std::stol( argv[5] );
    uint64_t cputime =  std::stoll( argv[6] );
    uint64_t gputime =  std::stoll( argv[7] );
    uint16_t cpuTrack = db->getTrackIdByTrackProperties(TrackProperties{TrackType::CPU, pid, tid});
    uint16_t gpuTrack = db->getTrackIdByTrackProperties(TrackProperties{TrackType::GPU, gpuid, qid});
    if (cpuTrack!=INVALID_TRACK_INDEX && gpuTrack!=INVALID_TRACK_INDEX)
    {
        FlowRecord record;
        record.recordId = api_id;
        record.flowId.trackId = gpuTrack;
        record.flowId.recordId = op_id;
        record.flowId.timestamp = cputime;
        db->m_bindData.funcAddFlowRecord(db->m_bindData.handler, cpuTrack, record); 
        record.recordId = op_id;
        record.flowId.trackId = cpuTrack;
        record.flowId.recordId = api_id;
        record.flowId.timestamp = gputime;
        db->m_bindData.funcAddFlowRecord(db->m_bindData.handler, gpuTrack, record);
    } 

    return 0;
}


bool RocpdDatabase::readTraceChunkAllTracks(DbReadProgress progressCallback)
{
    static std::stringstream query;
    if (readConfig.endTime == 0 || readConfig.tracks.size() == 0) return false;
    for (int i = 0; i < readConfig.tracks.size();i++) {
        m_bindData.funcAddTrackArray(m_bindData.handler, readConfig.tracks[i] ,readConfig.startTime, readConfig.endTime);
    }
    query.str("");
    query   << "select start, (end-start), apiName_id, args_id, id, pid, tid from rocpd_api where start >= " << readConfig.startTime 
            << " and end < " << readConfig.endTime 
            << " ORDER BY start;";
    showLoadProgress(1, query.str().c_str(), progressCallback);
    if (!executeSQLQuery(query.str().c_str(), &callback_add_kernel_launch_record_all_tracks)) return false;
    query.str("");
    query   << "select start, (end-start), opType_id, description_id, id, gpuId, queueId from rocpd_op where start >= " << readConfig.startTime 
            << " and end < " << readConfig.endTime 
            << " ORDER BY start;";
    showLoadProgress(1, query.str().c_str(), progressCallback);
    if (!executeSQLQuery(query.str().c_str(), &callback_add_kernel_execute_record_all_tracks)) return false;
    query.str("");
    query   << "select start, value, deviceId, monitorType from rocpd_monitor where start >= " << readConfig.startTime 
            << " and start < " << readConfig.endTime 
            << " ORDER BY start;";
    showLoadProgress(1, query.str().c_str(), progressCallback);
    if (!executeSQLQuery(query.str().c_str(), &callback_add_metrics_record_all_tracks)) return false;
    

    return true;
}

bool RocpdDatabase::readTraceChunkTrackByTrack(DbReadProgress progressCallback)
{
    static std::stringstream query;
    if (readConfig.endTime == 0 || readConfig.tracks.size() == 0) return false;
    for (int i = 0; i < readConfig.tracks.size();i++) {
        trackId = readConfig.tracks[i];
        TrackProperties props = trackProps[trackId];
        m_bindData.funcAddTrackArray(m_bindData.handler, trackId, readConfig.startTime, readConfig.endTime);
        query.str("");
        if (props.type == TrackType::CPU)
        {
            query << "select start, (end-start), apiName_id, args_id, id from rocpd_api where pid == " << props.data.Cpu.processId
                << " and tid == " << props.data.Cpu.threadId
                << " and start >= " << readConfig.startTime
                << " and end < " << readConfig.endTime
                << " ORDER BY start;";
            showLoadProgress(1, query.str().c_str(), progressCallback);
            if (!executeSQLQuery(query.str().c_str(), &callback_add_kernel_launch_record)) return false;
        }
        else
            if (props.type == TrackType::GPU)
            {
                query << "select start, (end-start), opType_id, description_id, id from rocpd_op where gpuId == " << props.data.Gpu.gpuId
                    << " and queueId == " << props.data.Gpu.queueId
                    << " and start >= " << readConfig.startTime
                    << " and end < " << readConfig.endTime
                    << " ORDER BY start;";
                showLoadProgress(1, query.str().c_str(), progressCallback);
                if (!executeSQLQuery(query.str().c_str(), &callback_add_kernel_execute_record)) return false;
            }
            else
                if (props.type == TrackType::METRICS)
                {
                    const char* metricName = getMetricNameByTrackId(trackId);
                    if (metricName != nullptr)
                    {
                        query << "select start, value from rocpd_monitor where deviceId == " << props.data.Metric.gpuId
                            << " and monitorType like\"" << metricName << "\""
                            << " and start >= " << readConfig.startTime
                            << " and start < " << readConfig.endTime
                            << " ORDER BY start;";
                        showLoadProgress(1, query.str().c_str(), progressCallback);
                        if (!executeSQLQuery(query.str().c_str(), &callback_add_metrics_record)) return false;
                    }
                }
    }
    return true;
}

bool RocpdDatabase::readTraceProperties(DbReadProgress progressCallback)
{
    resetLoadProgress();
    showLoadProgress(1, "Getting minimum timestamp", progressCallback);
    if (!executeSQLQuery("SELECT MIN(start) FROM rocpd_api;", &callback_get_min_time)) return false;
    showLoadProgress(1, "Get maximum timestamp", progressCallback);
    if (!executeSQLQuery("select MAX(start) from rocpd_op;", &callback_get_max_time)) return false;
    showLoadProgress(1, "Getting number of strings",progressCallback);
    if (!executeSQLQuery("select COUNT(distinct(string)) from rocpd_string;", &callback_add_string_count)) return false;
    showLoadProgress(1,"Getting number of metrics", progressCallback);
    if (!executeSQLQuery("select count(distinct(monitorType)) from rocpd_monitor where deviceId > 0;", &callback_add_string_count)) return false;   
    m_bindData.funcSetTraceParameters(m_bindData.handler,minTime,maxTime,stringCount);
    showLoadProgress(1, "Adding CPU tracks", progressCallback);
    if (!executeSQLQuery("select count(*), pid, tid  from rocpd_api group by pid, tid;", &callback_add_cpu_track)) return false;
    showLoadProgress(1, "Adding GPU tracks", progressCallback);
    if (!executeSQLQuery("select count(*), gpuId, queueId from rocpd_op group by gpuId, queueId;", &callback_add_gpu_track)) return false;
    showLoadProgress(1, "Adding Metric tracks", progressCallback);
    if (!executeSQLQuery("select count(*), monitorType, deviceId from rocpd_monitor where deviceId > 0 group by monitorType, deviceId;", &callback_add_metric_track)) return false;
    showLoadProgress(1, "Loading strings", progressCallback);
    if (!executeSQLQuery("SELECT string, GROUP_CONCAT(id) AS ids FROM rocpd_string GROUP BY string;", &callback_add_strings_record)) return false;
    showLoadProgress(1, "Loading flow linkage data", progressCallback);
    if (!executeSQLQuery("select rocpd_api_ops.api_id, rocpd_api_ops.op_id, pid, tid, gpuId, queueId, rocpd_api.start, rocpd_op.start  from rocpd_api_ops INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id", &callback_add_flow_info)) return false;
 
    showLoadProgress(100-getLoadProgress(), "Trace properties successfully loaded", progressCallback);
    return true;
}


RocpdDatabase::RocpdDatabase(const char* path) : SqliteDatabase(path) { 
    minTime=std::numeric_limits<long long>::max();
    maxTime=std::numeric_limits<long long>::min();
    stringCount = 0;
};

const bool RocpdDatabase::convertStringId(const uint64_t dbStringId, uint32_t & stringId)
{
    StringMap::const_iterator pos = stringRefs.find(dbStringId);
    if (pos == stringRefs.end()) return false;
    stringId = pos->second;
    return true;
}

const char* RocpdDatabase::getMetricNameByTrackId(uint32_t trackId)
{
    for (int i = 0; i < metricNames.size(); i++)
    {
        if (metricNames[i].id == trackId)
        {
            return metricNames[i].name.c_str();
        }
    }
    return nullptr;
}

const uint16_t RocpdDatabase::getTrackIdByMetricNameAndGpuId(char* metricName, uint16_t gpuId)
{
    for (int i = 0; i < metricNames.size(); i++)
    {
        if (metricNames[i].name == metricName && metricNames[i].dId == gpuId)
        {
            return metricNames[i].id;
        }
    }
    return INVALID_TRACK_INDEX;
}

const uint16_t RocpdDatabase::getTrackIdByTrackProperties(TrackProperties & props)
{
    for (int i = 0; i < trackProps.size(); i++)
    {
        if (props.type ==  trackProps[i].type && props.data.value == trackProps[i].data.value)
        {
              return i;
        }
    }
    return INVALID_TRACK_INDEX;
}

const bool RocpdDatabase::isTrackRequested(uint16_t trackId)
{
    for (int i=0; i < readConfig.tracks.size(); i++)
    {
        if (readConfig.tracks[i] == trackId) return true;
    }
    return false;
}
