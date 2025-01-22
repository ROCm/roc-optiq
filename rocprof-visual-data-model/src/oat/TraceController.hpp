
#ifndef TraceController_hpp
#define TraceController_hpp

#include "TraceService.hpp"

#include "oatpp/web/server/api/ApiController.hpp"
#include "oatpp/web/mime/ContentMappers.hpp"
#include "oatpp/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController) //<- Begin Codegen

/**
 * Trace REST controller.
 */
class TraceController : public oatpp::web::server::api::ApiController {
public:
    TraceController(OATPP_COMPONENT(std::shared_ptr<oatpp::web::mime::ContentMappers>, apiContentMappers))
        : oatpp::web::server::api::ApiController(apiContentMappers)
    {
    }
private:
    typedef TraceController __ControllerType;
    TraceService m_TraceService; // Create Trace service.
public:

    static std::shared_ptr<TraceController> createShared(
        OATPP_COMPONENT(std::shared_ptr<oatpp::web::mime::ContentMappers>, apiContentMappers) // Inject ContentMappers
    ) {
        return std::make_shared<TraceController>(apiContentMappers);
    }

    ENDPOINT_INFO(openTrace) {
        info->summary = "Create new Trace";

        info->addResponse<Object<TraceOpenDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
        info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

        info->pathParams["path"].description = "Trace file location";
    }
 
    ENDPOINT("POST", "Traces/{path}", openTrace, PATH(String, path))
    {
        return createDtoResponse(Status::CODE_200, m_TraceService.openTrace(path));
    }

    ENDPOINT_INFO(getTaceInfo) {
        info->summary = "Read trace metadata";

        info->addResponse<Object<TraceInfoDto>>(Status::CODE_200, "application/json");
        info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
        info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

        info->pathParams["traceHandler"].description = "Trace Handler";
    }
    ENDPOINT("GET", "Traces/{traceHandler}/Info", getTaceInfo, PATH(UInt64, traceHandler))
    {
        return createDtoResponse(Status::CODE_200, m_TraceService.readTraceInfo(traceHandler));
    }

  ENDPOINT_INFO(getProgress) {
    info->summary = "Get progress of previous database read request";

    info->addResponse<oatpp::Object<ProgressDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");
  }
  ENDPOINT("GET", "Traces/{traceHandler}/Progress", getProgress, PATH(UInt64, traceHandler))
  {
    return createDtoResponse(Status::CODE_200, m_TraceService.getProgress(traceHandler));
  }
  
  
  ENDPOINT_INFO(updateTrace) {
    info->summary = "Update Trace by reading data chunk";

    info->addConsumes<Object<ReadTraceDto>>("application/json");

    info->addResponse<Object<ReadTraceDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

    info->pathParams["traceHandler"].description = "Trace identifier";

  }
  ENDPOINT("PUT", "Traces/{traceHandler}/Chunk", updateTrace,
           PATH(UInt64, traceHandler),
           BODY_DTO(Object<ReadTraceDto>, readTraceDto))
  {
    readTraceDto->traceHandler = traceHandler;
    return createDtoResponse(Status::CODE_200, m_TraceService.readTraceChunk(readTraceDto));
  }
  
  
  ENDPOINT_INFO(getTracks) {
    info->summary = "Get all tracks for specified trace handler";

    info->addResponse<oatpp::Object<TracksPageDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

    info->pathParams["traceHandler"].description = "Trace identifier";
  }
  ENDPOINT("GET", "Traces/{traceHandler}/Tracks", getTracks, PATH(UInt64, traceHandler))
  {
    return createDtoResponse(Status::CODE_200, m_TraceService.getAllTracks(traceHandler));
  }

  ENDPOINT_INFO(getEvents) {
    info->summary = "Get latest events data for specified track handler";

    info->addResponse<Object<EventTrackArrayDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

    info->pathParams["trackHandler"].description = "Track identifier";
  }
  ENDPOINT("GET", "Traces/{trackHandler}/Events", getEvents, PATH(UInt64, trackHandler))

  {
    return createDtoResponse(Status::CODE_200, m_TraceService.getEventTrackLatestDataArray(trackHandler));
  }
  
  
  ENDPOINT_INFO(getMetrics) {
    info->summary = "Get latest metrics data for specified track handler";

    info->addResponse<Object<MetricTrackArrayDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

    info->pathParams["trackHandler"].description = "Track identifier";
  }
  ENDPOINT("GET", "Traces/{trackHandler}/Metrics", getMetrics, PATH(UInt64, trackHandler))
  {
    return createDtoResponse(Status::CODE_200, m_TraceService.getMetricTrackLatestDataArray(trackHandler));
  }
  
  
  ENDPOINT_INFO(deleteChunks) {
    info->summary = "Delete Trace chanks at specified timestamp";

    info->addResponse<Object<StatusDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

    info->pathParams["traceHandler"].description = "Trace identifier";
    info->pathParams["timestamp"].description = "Time identifier";
  }
  ENDPOINT("DELETE", "Traces/{traceHandler}/Chunks/{timestamp}", deleteChunks,
           PATH(UInt64, traceHandler),
           PATH(UInt64, timestamp))
  {
    return createDtoResponse(Status::CODE_200, m_TraceService.deleteTraceChunksAtTime(traceHandler, timestamp));
  }


  ENDPOINT_INFO(deleteTrace) {
    info->summary = "Delete Trace by TraceId";

    info->addResponse<Object<StatusDto>>(Status::CODE_200, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_500, "application/json");
    info->addResponse<Object<StatusDto>>(Status::CODE_404, "application/json");

    info->pathParams["traceHandler"].description = "Trace identifier";
  }
  ENDPOINT("DELETE", "Traces/{traceHandler}", deleteTrace, PATH(UInt64, traceHandler))
  {
    return createDtoResponse(Status::CODE_200, m_TraceService.deleteTrace(traceHandler));
  }

};

#include OATPP_CODEGEN_END(ApiController) //<- End Codegen

#endif /* TraceController_hpp */
