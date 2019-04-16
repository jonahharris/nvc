#include "bytecode.hpp"
#include "interp.hpp"
#include "vcode.h"
#include "phase.h"

#include <gtest/gtest.h>
#include <map>

#define __ asm_.

using namespace testing;

class InterpTest : public ::testing::Test {
protected:
   InterpTest()
      : asm_(InterpMachine::get())
   {}

   virtual void SetUp() {}
   virtual void TearDown() {}

   Bytecode::Assembler asm_;
   Interpreter         interp_;
};

TEST_F(InterpTest, add1) {
   __ add(Bytecode::R(0), 1);
   __ ret();

   Bytecode *b = __ finish();

   interp_.set_reg(0, 5);
   EXPECT_EQ(6, interp_.run(b));

   interp_.set_reg(0, 42);
   EXPECT_EQ(43, interp_.run(b));
}

TEST_F(InterpTest, fact) {
   Bytecode::Register r0 = Bytecode::R(0);
   Bytecode::Register r1 = Bytecode::R(1);
   Bytecode::Register r3 = Bytecode::R(3);
   Bytecode::Register r8 = Bytecode::R(8);
   Bytecode::Register r9 = Bytecode::R(9);
   Bytecode::Register r10 = Bytecode::R(10);
   Bytecode::Register r11 = Bytecode::R(11);
   Bytecode::Register r13 = Bytecode::R(13);

   __ mov(r1, 1);
   __ str(__ sp(), 0, r1);
   __ cmp(r1, r0);
   __ cset(r3, Bytecode::GT);
   __ cbnz(r3, 29);
   __ jmp(21);
   __ str(__ sp(), 4, r1);
   __ jmp(38);
   __ ldr(r13, __ sp(), 0);
   __ mov(r0, r13);
   __ ret();
   __ ldr(r8, __ sp(), 0);
   __ ldr(r9, __ sp(), 4);
   __ mov(r10, r8);
   __ mul(r10, r9);
   __ str(__ sp(), 0, r10);
   __ mov(r11, r9);
   __ add(r11, 1);
   __ str(__ sp(), 4, r11);
   __ cmp(r9, r0);
   __ cset(r3, Bytecode::EQ);
   __ cbnz(r3, 29);
   __ jmp(38);

   Bytecode *b = __ finish();

   b->dump();

   interp_.set_reg(0, 1);
   EXPECT_EQ(1, interp_.run(b));

   interp_.set_reg(0, 5);
   EXPECT_EQ(120, interp_.run(b));

   interp_.set_reg(0, 10);
   EXPECT_EQ(3628800, interp_.run(b));
}
