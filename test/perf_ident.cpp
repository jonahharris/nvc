#include "perf_util.hpp"
#include "ident.h"

#include <cstdlib>

namespace {
   void rand_chars(char *buf, int min, int max)
   {
      const int len = min + random() % (max - min);
      for (int j = 0; j < len; j++)
         buf[j] = '0' + random() % ('Z' - '0');
      buf[len] = '\0';
   }
}

class PerfIdentNew : public PerfTest {
public:
   PerfIdentNew() : PerfTest("IdentNew") {}

   static const int NUM_IDENTS = 100000;
   static const int MAX_LEN = 40;
   static const int MIN_LEN = 1;

   void set_up() override
   {
      ident_wipe();
   }

   void run() override
   {
      char buf[MAX_LEN + 1];

      for (int i = 0; i < NUM_IDENTS; i++) {
         rand_chars(buf, MIN_LEN, MAX_LEN);
         (void)ident_new(buf);
      }
   }
};

class PerfIdentStr : public PerfTest {
public:
   PerfIdentStr() : PerfTest("IdentStr") {}

   static const int NUM_IDENTS = 5000;
   static const int MAX_LEN = 80;
   static const int MIN_LEN = 1;

   void set_up() override
   {
      ident_wipe();

      for (int i = 0; i < NUM_IDENTS; i++) {
         char buf[MAX_LEN + 1];
         rand_chars(buf, MIN_LEN, MAX_LEN);
         idents_[i] = ident_new(buf);
      }
   }

   void run() override
   {
      for (int i = 0; i < NUM_IDENTS; i++) {
         (void)istr(idents_[i]);
      }
   }

private:
   ident_t idents_[NUM_IDENTS];
};

namespace {
   PerfIdentNew IdentNew;
   PerfIdentStr IdentStr;
}
