//
//  Copyright (C) 2019  Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "bytecode.hpp"

#include <vector>
#include <assert.h>
#include <string.h>
#include <ostream>

namespace {
   class Compiler {
   public:
      Compiler(Bytecode *b);
      Compiler(const Compiler&) = delete;
      ~Compiler();

      void compile(vcode_unit_t unit);

   private:
      void compile_const(int op);
      void compile_addi(int op);
      void compile_return(int op);

      void emit_u8(uint8_t byte);
      void emit_i32(int32_t value);
      void emit_reg(int reg);

      int map_vcode_reg(vcode_reg_t reg) const;

      Bytecode *const      bytecode_;
      std::vector<uint8_t> bytes_;
   };

   class Dumper {
   public:
      Dumper(Printer& printer, const Bytecode *b);

      void dump();

   private:
      void diassemble_one();
      void opcode(const char *name);
      void reg();
      void immed32();

      const uint8_t  *bptr_;
      const Bytecode *bytecode_;
      Printer&        printer_;

      int col_ = 0, pos_ = 0;
   };
}

Compiler::Compiler(Bytecode *b)
   : bytecode_(b)
{

}

Compiler::~Compiler()
{
   bytecode_->set_bytes(bytes_.data(), bytes_.size());
}

int Compiler::map_vcode_reg(vcode_reg_t reg) const
{
   assert(reg < bytecode_->machine().num_regs());
   return reg;
}

void Compiler::compile(vcode_unit_t unit)
{
   vcode_select_unit(unit);

   const int nblocks = vcode_count_blocks();
   for (int i = 0; i < nblocks; i++) {
      vcode_select_block(i);

      const int nops = vcode_count_ops();
      for (int j = 0; j < nops; j++) {
         switch (vcode_get_op(j)) {
         case VCODE_OP_CONST:
            compile_const(j);
            break;
         case VCODE_OP_ADDI:
            compile_addi(j);
            break;
         case VCODE_OP_RETURN:
            compile_return(j);
            break;
         default:
            fatal("cannot compile vcode op %s to bytecode",
                  vcode_op_string(vcode_get_op(j)));
         }
      }
   }
}

void Compiler::compile_const(int op)
{
   emit_u8(Bytecode::MOVC);
   emit_reg(map_vcode_reg(vcode_get_result(op)));
   emit_i32(vcode_get_value(op));
}

void Compiler::compile_addi(int op)
{
   vcode_reg_t dst = vcode_get_result(op);
   vcode_reg_t src = vcode_get_arg(op, 0);

   emit_u8(Bytecode::MOVR);
   emit_reg(map_vcode_reg(dst));
   emit_reg(map_vcode_reg(src));

   emit_u8(Bytecode::ADDI);
   emit_reg(map_vcode_reg(dst));
   emit_i32(vcode_get_value(op));
}

void Compiler::compile_return(int op)
{
   vcode_reg_t value = vcode_get_arg(op, 0);

   if (value != bytecode_->machine().result_reg()) {
      emit_u8(Bytecode::MOVR);
      emit_reg(bytecode_->machine().result_reg());
      emit_reg(map_vcode_reg(value));
   }

   emit_u8(Bytecode::RET);
}

void Compiler::emit_reg(int reg)
{
   assert(bytecode_->machine().num_regs() <= 256);
   assert(reg < 256);
   emit_u8(reg);
}

void Compiler::emit_u8(uint8_t byte)
{
   bytes_.push_back(byte);
}

void Compiler::emit_i32(int32_t value)
{
   bytes_.push_back(value & 0xff);
   bytes_.push_back((value >> 8) & 0xff);
   bytes_.push_back((value >> 16) & 0xff);
   bytes_.push_back((value >> 24) & 0xff);
}

Dumper::Dumper(Printer& printer, const Bytecode *b)
   : bptr_(b->bytes()),
     bytecode_(b),
     printer_(printer)
{
}

void Dumper::opcode(const char *name)
{
   col_ += printer_.print("%s", name);
   bptr_++;
}

void Dumper::reg()
{
   col_ += printer_.print("%s%s", pos_ == 0 ? " " : ", ",
                          bytecode_->machine().fmt_reg(*bptr_));
   bptr_++;
   pos_++;
}

void Dumper::immed32()
{
   assert(bptr_ + 4 <= bytecode_->bytes() + bytecode_->length());

   col_ += printer_.print("%s%d", pos_ == 0 ? " " : ", ",
                          bytecode_->machine().read_i32(bptr_));
   bptr_ += 4;
   pos_++;
}

void Dumper::diassemble_one()
{
   switch ((Bytecode::OpCode)*bptr_) {
   case Bytecode::NOP:
      opcode("NOP");
      break;
   case Bytecode::MOVC:
      opcode("MOVC");
      reg();
      immed32();
      break;
   case Bytecode::RET:
      opcode("RET");
      break;
   case Bytecode::ADD:
      opcode("ADD");
      break;
   case Bytecode::MOVR:
      opcode("MOVR");
      reg();
      reg();
      break;
   case Bytecode::ADDI:
      opcode("ADDI");
      reg();
      immed32();
      break;
   default:
      fatal("invalid bytecode %02x", *bptr_);
   }
}

void Dumper::dump()
{
   while (bptr_ < bytecode_->bytes() + bytecode_->length()) {
      const uint8_t *startp = bptr_;
      col_ = 0;
      pos_ = 0;

      col_ += printer_.print("%4d ", (int)(bptr_ - bytecode_->bytes()));

      diassemble_one();

      while (col_ < 30)
         col_ += printer_.print(" ");

      for (const uint8_t *p2 = startp; p2 < bptr_; p2++)
         printer_.print(" %02x", *p2);

      printer_.print("\n");

      assert(bptr_ > startp);
   }

   assert(bptr_ == bytecode_->bytes() + bytecode_->length());
}

Bytecode::Bytecode(const Machine& m)
   : machine_(m)
{

}

Bytecode::~Bytecode()
{
   delete bytes_;
}

Bytecode *Bytecode::compile(const Machine& m, vcode_unit_t unit)
{
   Bytecode *b = new Bytecode(m);

   Compiler(b).compile(unit);

   return b;
}

void Bytecode::dump(Printer&& printer) const
{
   Dumper(printer, this).dump();
}

void Bytecode::dump(Printer& printer) const
{
   Dumper(printer, this).dump();
}

void Bytecode::set_bytes(const uint8_t *bytes, size_t len)
{
   assert(bytes_ == nullptr);

   bytes_ = new uint8_t[len];
   len_ = len;

   memcpy(bytes_, bytes, len);
}

Machine::Machine(const char *name, int num_regs, int result_reg)
   : name_(name),
     num_regs_(num_regs),
     result_reg_(0)
{
}

const char *Machine::fmt_reg(int reg) const
{
   assert(reg < num_regs_);

   static char buf[32]; // XXX
   checked_sprintf(buf, sizeof(buf), "R%d", reg);
   return buf;
}

int32_t Machine::read_i32(const uint8_t *p) const
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

InterpMachine::InterpMachine()
   : Machine("interp", 256, 0)
{
}

const InterpMachine& InterpMachine::get()
{
   static InterpMachine m;
   return m;
}

std::ostream& operator<<(std::ostream& os, const Bytecode& b)
{
   BufferPrinter printer;
   b.dump(printer);
   return os << printer.buffer();
}
