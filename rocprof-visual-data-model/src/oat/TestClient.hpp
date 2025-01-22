#ifndef TraceTestClient_hpp
#define TraceTestClient_hpp

#include "oatpp/web/client/ApiClient.hpp"
#include "oatpp/macro/codegen.hpp"

#include "Dto.hpp"

/* Begin Api Client code generation */
#include OATPP_CODEGEN_BEGIN(ApiClient)

/**
 * Test API client.
 * Use this client to call application APIs.
 */
class TestClient : public oatpp::web::client::ApiClient {

  API_CLIENT_INIT(TestClient)

  /*****************************************************************
   * TraceController
   *****************************************************************/

  API_CALL("POST", "Traces/{path}", openTrace, PATH(String, path))
  API_CALL("PUT", "Traces/{traceHandler}/Chunk", updateTrace, PATH(UInt64, traceHandler), BODY_DTO(Object<ReadTraceDto>, ReadTraceDto))
  API_CALL("GET", "Traces/{traceHandler}/Progress", getProgress, PATH(UInt64, traceHandler))
  API_CALL("GET", "Traces/{traceHandler}/Info", getTaceInfo, PATH(UInt64, traceHandler))
  API_CALL("GET", "Traces/{traceHandler}/Tracks", getTracks, PATH(UInt64, traceHandler))
  API_CALL("GET", "Traces/{trackHandler}/Events", getEvents, PATH(UInt64, trackHandler))
  API_CALL("GET", "Traces/{trackHandler}/Metrics", getMetrics, PATH(UInt64, trackHandler))
  API_CALL("DELETE", "Traces/{traceHandler}/Chunks/{timestamp}", deleteChunks, PATH(UInt64, traceHandler), PATH(UInt64, timestamp))
  API_CALL("DELETE", "Traces/{traceHandler}", deleteTrace, PATH(UInt64, traceHandler))


  /*****************************************************************/

  // TODO - add more client API calls here

};

/* End Api Client code generation */
#include OATPP_CODEGEN_END(ApiClient)

#endif // TraceTestClient_hpp