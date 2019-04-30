#include "perf_util.hpp"

PerfTest::List PerfTest::all_tests_;

PerfTest::PerfTest(const std::string& name)
   : name_(name)
{
   all_tests_.push_back(this);
}
