This experiment implemets Oat++ REST server and client test components for testing Oat++
capabilities, stability, ease of use and performance.

Oat++ framework implements set of macros and classes which makes HTTP bases clent-server
communication transparrent for a developer. It enables user to generate Web Api in 
declarative manner. It has ready components to access SQL or MongoDb databases. Has
a Swagger component for API documentation. Has Asynchronous API to support unlimited 
number of connections.

To develop this test I had to:

1. Implement set of classes describing Data Transfer Objects (DTO). For example:
```
    class TraceInfoDto : public oatpp::DTO {  
        DTO_INIT(TraceInfoDto, DTO)

        DTO_FIELD(UInt64, minTime, "tmin");
        DTO_FIELD(UInt64, maxTime, "tmax");
        DTO_FIELD(UInt16, numTracks, "trnum");  
    };
```
    Using DTO-related macros we can implement structure of an object we transfer 
    between client and server. Class member types may be primitive C types as well
    more complex types like Vector or List.
    Note, DTO_INIT macro is necessary for the class instantiation and first parameter of
    the macro has to be name of the class it belongs to. Compiler wouldn't catch if 
    you have there a name of a different class. It's easy to make this mistake if you 
    copy/paste number of classes.  
2.  Implement Service class which implements all the methods needed to collect and 
    process data on server side. For example one of the Service methods:
```
        oatpp::Object<TraceInfoDto> TraceService::readTraceInfo(const oatpp::UInt64& traceHandler)
        {
            // return 404 if handler is not valid
            OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), traceHandler) != m_traces.end(), 
                Status::CODE_404, "Trace not found");               
            //Create DTO object
            auto traceInfo = TraceInfoDto::createShared();          

            //assign values to DTO class member parameters
            traceInfo->numTracks = get_number_of_tracks(*(TraceHandler*)traceHandler.get());   
            traceInfo->minTime = get_trace_min_time(*(TraceHandler*)traceHandler.get());
            traceInfo->maxTime = get_trace_max_time(*(TraceHandler*)traceHandler.get());
                        
            return traceInfo; //return DTO object
        }
```
3.  Implement controller class, based on oatpp::web::server::api::ApiController,
    which contains server endpoint information and trace services class instantiated.
    Oat++ provides set of macros to add endpoints to the class:
```
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
```
        ENDPOINT_INFO is optional but good for describing an endpoint.
        ENDPOINT macro provides information which HTTP method will be used, path URL, name and 
        path parameters. It also calls Service method and creates DTO response based on DTO 
        object returned by the method.


4. Implement Client class based on oatpp::web::client::ApiClient. Add API calls to the client
    using API_CALL macro:
```
    API_CALL("GET", "Traces/{traceHandler}/Info", getTaceInfo, PATH(UInt64, traceHandler))
```

5. Implement Client test class based on oatpp::test::UnitTest and override onRun() method, containing
    actuall test. The test requests ClientConnectionProvider, ContentMappers, HttpRequestExecutor and 
    creates Client object based on requested components. Then it call client API calls in order test
    is designed. Here is related to previous examples part of the test:
```
    auto traceInfoResponse = client->getTaceInfo(traceOpenDto->traceHandler);           // calling client API
    OATPP_ASSERT(traceInfoResponse->getStatusCode() == 200);                            // checking for status code
                                                                                        // converting HTTP reponse to DTO
    auto traceInfoDto = traceInfoResponse->readBodyToDto<oatpp::Object<TraceInfoDto>>(
        contentMappers->selectMapperForContent(traceOpenResponse->getHeader("Content-Type"))
    );
                                                                                        // use DTO class members
    std::cout << std::endl << "Trace has " << traceInfoDto->numTracks << " tracks , startTime = " 
        << traceInfoDto->minTime << ", endTime = " << traceInfoDto->maxTime << std::endl; 
```

**Here are my test results**:

1.  Capabilities - with help of provide macros, classes and examples it's pretty easy to create
    REST API and make it functional. This test does not use full set of capabilities, like asynchronous
    endpoints, for example. Asynchronous endpoints suppose to increase performance an improve CPU
    utilization, but all the asynchronous coroutines should be non-blocking. So, in order to use
    asyncronous API, all underlying API should be non-blocking. I extended database access api to
    run on separate threads. So, an example of the asynchronous API would be 1. checking for thread
    to complete reading data from dabase, 2. collecting data from memory and creating DTO.
    For now I'm using simple API and query a progress of data db read completion from client, in 
    order to show progress on the console screen. 
2.  Stability - it's very stable, but has some issues. First, it's very susceptible to typos,
    due to using macros and string tags, URLs, etc, which gets parsed runtime and transparrent for 
    compiler. Second, some types do not get properly parsed by Oatpp JSON parser. One example is
    floating point. I experienced an issue when Json compiler couldn't properly parse scientific
    notation, but only if type is Froat64. Float32 seems to handle scientific notation properly.
    That issue created an effect of application crashing sporadically, but it was only crashing
    when value was big or small enough to be encoded in exponential format.
3.  Easy of use - there was enough documentation and examples to create small test application. 
    It's intuitive, thoughtful and well built. So overall impression is good. When comes to more 
    complex tasks, like using asynchronous API, I would expect more comprehesive documentation
    and examples.
4.  Performance - performance degradation is definitely a big problem, due to using JSON for data 
    transfer. Base on test results array of 157557 events takes 0.0895971 to prepare and 0.954377
    to convert to JSON on server side and back to DTO on client side. For 473850 metrics points,
    which has smaller footprint, 0.0824631 and 0.912278 seconds respectively. It would take about 
    3 time longer to encode/decode if we send list of DTOs, which is how it's presented for in 
    Oat++ examples for transfering array of structures. To improve performance, I had to send 
    single DTO with Vector type of every member: 

**The way shown in examples:**
```
        class EventTrackDataDto : public oatpp::DTO {
            DTO_INIT(EventTrackDataDto, DTO)

            DTO_FIELD(UInt64, id, "id");
            DTO_FIELD(UInt64, timestamp, "ts");
            DTO_FIELD(Int64, duration, "dur");
            DTO_FIELD(String, eventType, "type");
            DTO_FIELD(String, eventDescription, "descr");
        };
        template<class T>
            class PageDto : public oatpp::DTO {

            DTO_INIT(PageDto, DTO)
            DTO_FIELD(UInt32, count);
            DTO_FIELD(Vector<T>, items);
            DTO_FIELD(Float32, timeSpent, "took");
        };

        class EventsDataDto : public PageDto<oatpp::Object<EventTrackDataDto>> {

            DTO_INIT(EventsDataDto, PageDto<oatpp::Object<EventTrackDataDto>>);

        };
```

**The way for better performance:**

```
    class EventTrackArrayDto : public oatpp::DTO {
        DTO_INIT(EventTrackArrayDto, DTO)

        DTO_FIELD(Vector<UInt64>, ids, "ids");
        DTO_FIELD(Vector<UInt64>, timestamps, "tss");
        DTO_FIELD(Vector<Int64>, durations, "durs");
        DTO_FIELD(Vector<String>, eventTypes, "types");
        DTO_FIELD(Vector<String>, eventDescriptions, "descrs");
        DTO_FIELD(Float32, timeSpent, "took");
    };
```
