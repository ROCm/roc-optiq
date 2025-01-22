
#ifndef CRUD_TraceSERVICE_HPP
#define CRUD_TraceSERVICE_HPP


#include "oatpp/web/protocol/http/Http.hpp"
#include "oatpp/macro/component.hpp"
#include "oatpp/macro/codegen.hpp"
#include "oatpp/Types.hpp"
#include "Dto.hpp"
#include "../data/TraceInterface.h"



class TraceService {
private:
  typedef oatpp::web::protocol::http::Status Status;
  std::vector<unsigned long long> m_traces;
  std::chrono::steady_clock::time_point time;
public:

  oatpp::Object<TraceOpenDto> openTrace(const oatpp::String& path);
  oatpp::Object<ProgressDto> getProgress(const oatpp::UInt64& traceHandler);
  oatpp::Object<TraceInfoDto> readTraceInfo(const oatpp::UInt64& traceHandler);
  oatpp::Object<StatusDto> readTraceChunk(const oatpp::Object<ReadTraceDto>& dto);
  oatpp::Object<PageDto<oatpp::Object<TrackInfoDto>>> getAllTracks(const oatpp::UInt64& traceHandler);
  oatpp::Object<PageDto<oatpp::Object<EventTrackDataDto>>> getEventTrackLatestData(const oatpp::UInt64& trackHandler);
  oatpp::Object<PageDto<oatpp::Object<MetricTrackDataDto>>> getMetricTrackLatestData(const oatpp::UInt64& trackHandler);
  oatpp::Object<MetricTrackArrayDto> getMetricTrackLatestDataArray(const oatpp::UInt64& trackHandler);
  oatpp::Object<EventTrackArrayDto> getEventTrackLatestDataArray(const oatpp::UInt64& trackHandler);
  oatpp::Object<StatusDto> deleteTraceChunksAtTime(const oatpp::UInt64& traceHandler, const oatpp::UInt64& time);
  oatpp::Object<StatusDto> deleteTrace(const oatpp::UInt64& traceHandler);  

};

#endif //CRUD_TraceSERVICE_HPP
