#pragma once

#include "util.h"

#include <string>
#include <vector>

class PerfTest {
public:
   PerfTest(const std::string& name);
   virtual ~PerfTest() {}

   virtual void set_up() {}
   virtual void run() {}

   const std::string &name() const { return name_; }

   typedef std::vector<PerfTest*> List;

   static const List &all_tests() { return all_tests_; }

 private:
   const std::string name_;

   static List all_tests_;
};
