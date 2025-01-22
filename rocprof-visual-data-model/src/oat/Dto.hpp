#ifndef Dto_hpp
#define Dto_hpp

#include "oatpp/macro/codegen.hpp"
#include "oatpp/Types.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)


ENUM(DtoReadOptions, v_int32,
     VALUE(READ_ALL_TRACKS, 0, "READ_ALL_TRACKS"),
     VALUE(READ_TRACK_BY_TRACK, 1, "READ_TRACK_BY_TRACK")
)

ENUM(DtoTrackType, v_int32,
     VALUE(CPU_TRACK, 1, "CPU_TRACK"),
     VALUE(GPU_TRACK, 2, "GPU_TRACK"),
     VALUE(METRIC_TRACK, 3, "METRIC_TRACK")
)

class TraceOpenDto : public oatpp::DTO {
  DTO_INIT(TraceOpenDto, DTO)
  DTO_FIELD(UInt64, traceHandler, "hndl");
  DTO_FIELD(Float32, timeSpent, "took");
};

class TraceInfoDto : public oatpp::DTO {  
  DTO_INIT(TraceInfoDto, DTO)

  DTO_FIELD(UInt64, minTime, "tmin");
  DTO_FIELD(UInt64, maxTime, "tmax");
  DTO_FIELD(UInt16, numTracks, "trnum");  
};

class ProgressDto : public oatpp::DTO { 
  DTO_INIT(ProgressDto, DTO)
  
  DTO_FIELD(Float32, timeSpent, "took");
  DTO_FIELD(Float64, progress, "progress");
  DTO_FIELD(String, message, "msg");
  DTO_FIELD(Boolean, completed, "completed");
};

class ReadTraceDto : public oatpp::DTO {  
  DTO_INIT(ReadTraceDto, DTO)

  DTO_FIELD(UInt64, traceHandler, "hndl");
  DTO_FIELD(UInt64, startTime, "tstart");
  DTO_FIELD(UInt64, endTime, "tend");
  DTO_FIELD(Vector<UInt16>, trackIds, "tids");
  DTO_FIELD(Enum<DtoReadOptions>::AsNumber, readOptions, "ropts");
};

class TrackInfoDto : public oatpp::DTO {  
  DTO_INIT(TrackInfoDto, DTO)

  DTO_FIELD(UInt64, trackHandler, "hndl");
  DTO_FIELD(String, groupName, "ngroup");
  DTO_FIELD(String, trackName, "ntrack");
  DTO_FIELD(Enum<DtoTrackType>::AsNumber, trackType, "type");
};

class EventTrackDataDto : public oatpp::DTO {
  DTO_INIT(EventTrackDataDto, DTO)

  DTO_FIELD(UInt64, id, "id");
  DTO_FIELD(UInt64, timestamp, "ts");
  DTO_FIELD(Int64, duration, "dur");
  DTO_FIELD(String, eventType, "type");
  DTO_FIELD(String, eventDescription, "descr");
};

class EventTrackArrayDto : public oatpp::DTO {
  DTO_INIT(EventTrackArrayDto, DTO)

  DTO_FIELD(Vector<UInt64>, ids, "ids");
  DTO_FIELD(Vector<UInt64>, timestamps, "tss");
  DTO_FIELD(Vector<Int64>, durations, "durs");
  DTO_FIELD(Vector<String>, eventTypes, "types");
  DTO_FIELD(Vector<String>, eventDescriptions, "descrs");
  DTO_FIELD(Float32, timeSpent, "took");
};

class MetricTrackDataDto : public oatpp::DTO {
  DTO_INIT(MetricTrackDataDto, DTO)

  DTO_FIELD(UInt64, timestamp, "ts");
  DTO_FIELD(Float32, value, "val");
};

class MetricTrackArrayDto : public oatpp::DTO {
  DTO_INIT(MetricTrackArrayDto, DTO)

  DTO_FIELD(Vector<UInt64>, timestamps,"tss");
  DTO_FIELD(Vector<Float32>, values, "vals");
  DTO_FIELD(Float32, timeSpent, "took");
};


template<class T>
class PageDto : public oatpp::DTO {

  DTO_INIT(PageDto, DTO)
  DTO_FIELD(UInt32, count);
  DTO_FIELD(Vector<T>, items);
  DTO_FIELD(Float32, timeSpent, "took");

};

class TracksPageDto : public PageDto<oatpp::Object<TrackInfoDto>> {

  DTO_INIT(TracksPageDto, PageDto<oatpp::Object<TrackInfoDto>>);

};

class MetricsDataDto : public PageDto<oatpp::Object<MetricTrackDataDto>> {

  DTO_INIT(MetricsDataDto, PageDto<oatpp::Object<MetricTrackDataDto>>);

};

class EventsDataDto : public PageDto<oatpp::Object<EventTrackDataDto>> {

  DTO_INIT(EventsDataDto, PageDto<oatpp::Object<EventTrackDataDto>>);

};




class StatusDto : public oatpp::DTO {

  DTO_INIT(StatusDto, DTO)

  DTO_FIELD_INFO(status) {
    info->description = "Short status text";
  }
  DTO_FIELD(String, status);

  DTO_FIELD_INFO(code) {
    info->description = "Status code";
  }
  DTO_FIELD(Int32, code);

  DTO_FIELD_INFO(message) {
    info->description = "Verbose message";
  }
  DTO_FIELD(String, message);

  DTO_FIELD_INFO(timeSpent) {
      info->description = "Time spent running";
  }
  DTO_FIELD(Float32, timeSpent, "took");

};


#include OATPP_CODEGEN_END(DTO)

#endif /* Dto_hpp */
