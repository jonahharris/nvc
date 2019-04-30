#include "perf_util.hpp"
#include "util.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>

#define ITERATIONS 5

int main(int argc, char **argv)
{
   using clock = std::chrono::high_resolution_clock;
   using std::chrono::duration_cast;
   using std::chrono::microseconds;

   srandom(1234);

   term_init();

#ifndef NDEBUG
   color_printf("\n$red$$bold$PERFORMANCE TESTS SHOULD NOT BE RUN "
                "ON DEBUG BUILDS$$\n");
#endif

   color_printf("\n$white$$bold$%-20s%-12s%-10s%-10s$$\n",
                "Test", "Time", "Units", "Error");

   for (PerfTest *test : PerfTest::all_tests()) {
      printf("%-20s", test->name().c_str());
      fflush(stdout);

      test->set_up();
      test->run();  // Warm-up

      long micros = 0;
      for (int i = 0; i < ITERATIONS; i++) {
         test->set_up();
         auto tstart = clock::now();
         test->run();
         auto tend = clock::now();
         micros += duration_cast<microseconds>(tend - tstart).count();
      }

      const double avg = (double)micros / ITERATIONS;

      if (avg > 1000.0)
         printf("%-12.1f%-10s", avg / 1000.0, "ms");
      else
         printf("%-12.1f%-10s", avg, "us");

      printf("\n");
   }

   printf("\n");
   return 0;
}
