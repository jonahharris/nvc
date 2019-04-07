#include "bytecode.hpp"
#include "vcode.h"
#include "phase.h"

#include <gtest/gtest.h>
#include <map>

using namespace testing;

class BytecodeTest : public ::testing::Test {
protected:
   using OpCode = Bytecode::OpCode;

   virtual void SetUp() {}
   virtual void TearDown() {}

   static void SetUpTestCase();
   static void TearDownTestCase();

   struct CheckBytecode {
      CheckBytecode(uint16_t v) : value(v) {}

      static const uint16_t DONT_CARE = 0xffff;
      static const uint16_t REG_MASK  = 0x0100;

      uint16_t value;
   };

   static const CheckBytecode _;
   static const CheckBytecode _1, _2;

   void check_bytecodes(const Bytecode *b,
                        const std::vector<CheckBytecode>&& expect);

private:
   static lib_t work_;
};

const BytecodeTest::CheckBytecode BytecodeTest::_(CheckBytecode::DONT_CARE);
const BytecodeTest::CheckBytecode BytecodeTest::_1(CheckBytecode::REG_MASK | 1);
const BytecodeTest::CheckBytecode BytecodeTest::_2(CheckBytecode::REG_MASK | 2);

lib_t BytecodeTest::work_(nullptr);

void BytecodeTest::SetUpTestCase()
{
   work_ = lib_tmp("gtest");
   lib_set_work(work_);

   input_from_file(TESTDIR "/bytecode/functions.vhd");

   tree_t pack = parse();
   ASSERT_NE(nullptr, pack);
   EXPECT_EQ(T_PACKAGE, tree_kind(pack));
   EXPECT_TRUE(sem_check(pack));

   tree_t body = parse();
   ASSERT_NE(nullptr, body);
   EXPECT_EQ(T_PACK_BODY, tree_kind(body));
   EXPECT_TRUE(sem_check(body));

   simplify(body, (eval_flags_t)0);
   lower_unit(body);

   EXPECT_EQ(nullptr, parse());
   EXPECT_EQ(0, parse_errors());
   EXPECT_EQ(0, sem_errors());
}

void BytecodeTest::TearDownTestCase()
{
   lib_set_work(nullptr);
   lib_free(work_);
   work_ = nullptr;
}

void BytecodeTest::check_bytecodes(const Bytecode *b,
                                   const std::vector<CheckBytecode>&& expect)
{
   const uint8_t *p = b->bytes();
   std::map<int, int> match;

   for (const CheckBytecode& c : expect) {
      if (p >= b->bytes() + b->length()) {
         FAIL() << "expected more than " << b->length() << " bytecodes";
         return;
      }
      else if ((c.value & 0xff00) == 0) {
         // Directly compare the bytecode
         EXPECT_EQ(c.value, *p) << "bytecode mismatch at offset "
                                << (p - b->bytes())
                                << std::endl << std::endl << *b;
         ++p;
      }
      else if (c.value == CheckBytecode::DONT_CARE)
         ++p;
      else if ((c.value & CheckBytecode::REG_MASK) == CheckBytecode::REG_MASK) {
         const int num = c.value & 0xff;
         if (match.find(num) == match.end())
            match[num] = *p;
         else
            EXPECT_EQ(match[num], *p) << "placeholder _" << num
                                      << " mismatch at offset"
                                      << (p - b->bytes());
         ++p;
      }
      else
         FAIL() << "unexpected bytecode check" << c.value;
   }

   EXPECT_EQ(b->bytes() + b->length(), p) << "did not match all bytecodes"
                                          << std::endl << std::endl << *b;
}

TEST_F(BytecodeTest, compile_add1) {
   vcode_unit_t context = emit_context(ident_new("gtest"));
   vcode_type_t i32_type = vtype_int(INT32_MIN, INT32_MAX);
   vcode_unit_t unit = emit_function(ident_new("add1"), context, i32_type);

   vcode_reg_t p0 = emit_param(i32_type, i32_type, ident_new("x"));
   emit_return(emit_add(p0, emit_const(i32_type, 1)));

   vcode_opt();

   Bytecode *b = Bytecode::compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::MOV, _1, 0,
         Bytecode::ADDW, _1, 0x01, 0x00, 0x00, 0x00,
         Bytecode::MOV, 0, _1,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
   vcode_unit_unref(context);
}

TEST_F(BytecodeTest, compile_fact) {
   vcode_unit_t unit = vcode_find_unit(ident_new("GTEST.FUNCTIONS.FACT(I)I"));
   ASSERT_NE(nullptr, unit);

   Bytecode *b = Bytecode::compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   b->dump();
}

extern "C" int run_gtests(int argc, char **argv)
{
   InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
