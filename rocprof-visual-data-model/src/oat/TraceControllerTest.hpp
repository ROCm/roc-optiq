#ifndef TraceControllerTest_hpp
#define TraceControllerTest_hpp

#include "oatpp-test/UnitTest.hpp"

class TraceControllerTest : public oatpp::test::UnitTest {
public:
    TraceControllerTest() : oatpp::test::UnitTest("TEST[TraceControllerTest]")
    {}
    TraceControllerTest(const std::string& traceName) : m_traceName(traceName), oatpp::test::UnitTest("TEST[TraceControllerTest]")
    {}

    void onRun() override;

    private:
    std::string m_traceName;
};

#endif // TraceControllerTest_hpp