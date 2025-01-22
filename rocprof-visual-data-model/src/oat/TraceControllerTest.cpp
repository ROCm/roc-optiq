#include "TraceControllerTest.hpp"

#include "oatpp/web/client/HttpRequestExecutor.hpp"
#include "oatpp-test/web/ClientServerTestRunner.hpp"

#include "TraceController.hpp"

#include "TestClient.hpp"
#include "TestComponent.hpp"

#include <cstdio>
#include <iostream>
#include <vector>
#include <thread>

void TraceControllerTest::onRun() {


  /* Register test components */
  TestComponent component;

  /* Create client-server test runner */
  oatpp::test::web::ClientServerTestRunner runner;

  /* Add TraceController endpoints to the router of the test server */
  runner.addController(std::make_shared<TraceController>());

  /* Run test */
  runner.run([this, &runner] {

    /* Get client connection provider for Api Client */
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ClientConnectionProvider>, clientConnectionProvider);

    /* Get object mapper component */
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::mime::ContentMappers>, contentMappers);

    /* Create http request executor for Api Client */
    auto requestExecutor = oatpp::web::client::HttpRequestExecutor::createShared(clientConnectionProvider);

    /* Create Test API client */
    auto client = TestClient::createShared(requestExecutor,
                                           contentMappers->getMapper("application/json"));

    auto traceOpenResponse = client->openTrace(m_traceName);
    OATPP_ASSERT(traceOpenResponse->getStatusCode() == 200);
    auto traceOpenDto = traceOpenResponse->readBodyToDto<oatpp::Object<TraceOpenDto>>(
        contentMappers->selectMapperForContent(traceOpenResponse->getHeader("Content-Type"))
    );    
    std::cout << "Opening trace time = " << traceOpenDto->timeSpent << std::endl;

    double totalTime = 0;
    while (1){
        auto traceProgress = client->getProgress(traceOpenDto->traceHandler);
        OATPP_ASSERT(traceProgress->getStatusCode() == 200);
        auto traceProgressDto = traceProgress->readBodyToDto<oatpp::Object<ProgressDto>>(
            contentMappers->selectMapperForContent(traceProgress->getHeader("Content-Type"))
        );
        totalTime += traceProgressDto->timeSpent;
        std::cout << "\rLoaded : " << traceProgressDto->progress << "% - " << *traceProgressDto->message << ",time = " << totalTime << std::string(40, ' ') << std::flush;
        if (*traceProgressDto->completed.get()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    } 

    auto traceInfoResponse = client->getTaceInfo(traceOpenDto->traceHandler);
    OATPP_ASSERT(traceInfoResponse->getStatusCode() == 200);
    auto traceInfoDto = traceInfoResponse->readBodyToDto<oatpp::Object<TraceInfoDto>>(
        contentMappers->selectMapperForContent(traceOpenResponse->getHeader("Content-Type"))
    );
    std::cout << std::endl << "Trace has " << traceInfoDto->numTracks << " tracks , startTime = " << traceInfoDto->minTime << ", endTime = " << traceInfoDto->maxTime << std::endl;


    auto dto = ReadTraceDto::createShared();
    dto->startTime = traceInfoDto->minTime;
    dto->endTime = traceInfoDto->maxTime;
    dto->readOptions = DtoReadOptions::READ_ALL_TRACKS;
    dto->trackIds = {};
    for (unsigned short i = 0; i < traceInfoDto->numTracks; i++) dto->trackIds->push_back(i);
    auto traceUpdateResponse = client->updateTrace(traceOpenDto->traceHandler, dto);
    OATPP_ASSERT(traceUpdateResponse->getStatusCode() == 200);
    auto statusDto = traceUpdateResponse->readBodyToDto<oatpp::Object<StatusDto>>(
        contentMappers->selectMapperForContent(traceUpdateResponse->getHeader("Content-Type"))
    );

    totalTime = 0;
    while (1) {
        auto traceProgress = client->getProgress(traceOpenDto->traceHandler);
        OATPP_ASSERT(traceProgress->getStatusCode() == 200);
        auto traceProgressDto = traceProgress->readBodyToDto<oatpp::Object<ProgressDto>>(
            contentMappers->selectMapperForContent(traceProgress->getHeader("Content-Type"))
        );
        totalTime += traceProgressDto->timeSpent;
        std::cout << "\rReading trace ,time = " << totalTime << std::string(20, ' ') << std::flush;
        if (*traceProgressDto->completed.get()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << std::endl;

    auto getTracksResponse = client->getTracks(traceOpenDto->traceHandler);
    OATPP_ASSERT(getTracksResponse->getStatusCode() == 200);
    auto getTracksDto = getTracksResponse->readBodyToDto<oatpp::Object<TracksPageDto>> (
        contentMappers->selectMapperForContent(getTracksResponse->getHeader("Content-Type"))
    );

    std::cout << "Tracks:" << std::endl;
    
    for (int i = 0; i < getTracksDto->count; i++)
    {
        std::cout << *getTracksDto->items[i]->groupName << ":" << *getTracksDto->items[i]->trackName << ", type = " << *(uint16_t*)getTracksDto->items[i]->trackType.get();
        if (*getTracksDto->items[i]->trackType.get() == DtoTrackType::CPU_TRACK || *getTracksDto->items[i]->trackType.get() == DtoTrackType::GPU_TRACK)
        {
            auto t0 = std::chrono::steady_clock::now();
            auto getEventsResponse = client->getEvents(getTracksDto->items[i]->trackHandler);
            OATPP_ASSERT(getEventsResponse->getStatusCode() == 200);
            EventTrackArrayDto::Wrapper getEventsDto;
            getEventsDto = getEventsResponse->readBodyToDto<oatpp::Object<EventTrackArrayDto>>(
                contentMappers->selectMapperForContent(getEventsResponse->getHeader("Content-Type"))
            );
            std::cout << ", has " << getEventsDto->timestamps->size() << " events";
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double> diff = t1 - t0;
            std::cout << " Time spent on server = " << getEventsDto->timeSpent << " Overall = " << diff.count() << std::endl;
        }
        else
        if (*getTracksDto->items[i]->trackType.get() == DtoTrackType::METRIC_TRACK)
        {
            auto t0 = std::chrono::steady_clock::now();
            auto getMetricsResponse = client->getMetrics(getTracksDto->items[i]->trackHandler);
            OATPP_ASSERT(getMetricsResponse->getStatusCode() == 200);
            auto getMetricsDto = getMetricsResponse->readBodyToDto<oatpp::Object<MetricTrackArrayDto>>(
                contentMappers->selectMapperForContent(getMetricsResponse->getHeader("Content-Type"))
            );
            std::cout << ", has " << getMetricsDto->timestamps->size() << " points";
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double> diff = t1 - t0;
            std::cout << " Time spent on server = " << getMetricsDto->timeSpent << " Overall = " << diff.count() << std::endl;
        }

    }
    

    auto getStatusResponse = client->deleteChunks(traceOpenDto->traceHandler, traceInfoDto->minTime);
    OATPP_ASSERT(getStatusResponse->getStatusCode() == 200);
    auto getStatusDto = getStatusResponse->readBodyToDto<oatpp::Object<StatusDto>>(
        contentMappers->selectMapperForContent(getStatusResponse->getHeader("Content-Type"))
    );
    std::cout << *getStatusDto->message << std::endl;

    getStatusResponse = client->deleteTrace(traceOpenDto->traceHandler);
    OATPP_ASSERT(getStatusResponse->getStatusCode() == 200);
    getStatusDto = getStatusResponse->readBodyToDto<oatpp::Object<StatusDto>>(
        contentMappers->selectMapperForContent(getStatusResponse->getHeader("Content-Type"))
    );
    std::cout << *getStatusDto->message << std::endl;


  }, std::chrono::minutes(20) /* test timeout */);

  /* wait all server threads finished */
  std::this_thread::sleep_for(std::chrono::seconds(1));


}