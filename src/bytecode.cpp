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
#include <map>
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
      void compile_store(int op);
      void compile_cmp(int op);
      void compile_load(int op);
      void compile_jump(int op);
      void compile_cond(int op);
      void compile_mul(int op);

      void emit_u8(uint8_t byte);
      void emit_i32(int32_t value);
      void emit_i16(int16_t value);
      void emit_reg(int reg);
      void emit_patch(vcode_block_t block);

      struct Mapping {
         enum Kind { REGISTER, STACK };

         Kind kind;
         int  slot;
      };

      struct Patch {
         vcode_block_t block;
         unsigned      offset;
      };

      const Mapping& map_vcode_reg(vcode_reg_t reg) const;
      const Mapping& map_vcode_var(vcode_reg_t reg) const;

      Bytecode *const                bytecode_;
      std::vector<uint8_t>           bytes_;
      std::map<vcode_var_t, Mapping> var_map_;
      std::vector<Mapping>           reg_map_;
      std::map<vcode_block_t, int>   block_map_;
      std::vector<Patch>             patches_;
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
      void immed16();
      void immed8();
      void indirect();
      void jump_target();
      void condition();

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

const Compiler::Mapping& Compiler::map_vcode_reg(vcode_reg_t reg) const
{
   assert(reg < reg_map_.size());
   return reg_map_[reg];
}

const Compiler::Mapping& Compiler::map_vcode_var(vcode_var_t var) const
{
   auto it = var_map_.find(var);
   assert(it != var_map_.end());
   return it->second;
}

void Compiler::compile(vcode_unit_t unit)
{
   vcode_select_unit(unit);

   int stack_offset = 0;
   const int nvars = vcode_count_vars();
   for (int i = 0; i < nvars; i++) {
      vcode_var_t var = vcode_var_handle(i);
      Mapping m = { Mapping::STACK, stack_offset };
      var_map_[var] = m;
      stack_offset += 4;
   }

   const int nregs = vcode_count_regs();
   for (int i = 0; i < nregs; i++) {
      Mapping m = { Mapping::REGISTER, i };
      reg_map_.push_back(m);
   }

   const int nblocks = vcode_count_blocks();
   for (int i = 0; i < nblocks; i++) {
      vcode_select_block(i);

      block_map_[i] = bytes_.size();

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
         case VCODE_OP_STORE:
            compile_store(j);
            break;
         case VCODE_OP_CMP:
            compile_cmp(j);
            break;
         case VCODE_OP_JUMP:
            compile_jump(j);
            break;
         case VCODE_OP_LOAD:
            compile_load(j);
            break;
         case VCODE_OP_MUL:
            compile_mul(j);
            break;
         case VCODE_OP_COND:
            compile_cond(j);
            break;
         case VCODE_OP_BOUNDS:
         case VCODE_OP_COMMENT:
         case VCODE_OP_DEBUG_INFO:
            break;
         default:
            vcode_dump_with_mark(j);
            fatal("cannot compile vcode op %s to bytecode",
                  vcode_op_string(vcode_get_op(j)));
         }
      }
   }

   for (const Patch& p : patches_) {
      const int delta = block_map_[p.block] - p.offset;
      bytes_[p.offset] = delta & 0xff;
      bytes_[p.offset + 1] = (delta >> 8) & 0xff;
   }
}

void Compiler::compile_const(int op)
{
   const Mapping& result = map_vcode_reg(vcode_get_result(op));
   assert(result.kind == Mapping::REGISTER);

   int64_t value = vcode_get_value(op);

   if (value >= INT8_MIN && value <= INT8_MAX) {
      emit_u8(Bytecode::MOVB);
      emit_reg(result.slot);
      emit_u8(value);
   }
   else {
      emit_u8(Bytecode::MOVW);
      emit_reg(result.slot);
      emit_i32(vcode_get_value(op));
   }
}

void Compiler::compile_addi(int op)
{
   const Mapping& dst = map_vcode_reg(vcode_get_result(op));
   const Mapping& src = map_vcode_reg(vcode_get_arg(op, 0));

   assert(dst.kind == Mapping::REGISTER);
   assert(src.kind == Mapping::REGISTER);

   emit_u8(Bytecode::MOV);
   emit_reg(dst.slot);
   emit_reg(src.slot);

   int64_t value = vcode_get_value(op);

   if (value >= INT8_MIN && value <= INT8_MAX) {
      emit_u8(Bytecode::ADDB);
      emit_reg(dst.slot);
      emit_u8(value);
   }
   else {
      emit_u8(Bytecode::ADDW);
      emit_reg(dst.slot);
      emit_i32(value);
   }
}

void Compiler::compile_return(int op)
{
   const Mapping& value = map_vcode_reg(vcode_get_arg(op, 0));
   assert(value.kind == Mapping::REGISTER);

   if (value.slot != bytecode_->machine().result_reg()) {
      emit_u8(Bytecode::MOV);
      emit_reg(bytecode_->machine().result_reg());
      emit_reg(value.slot);
   }

   emit_u8(Bytecode::RET);
}

void Compiler::compile_store(int op)
{
   const Mapping& dst = map_vcode_var(vcode_get_address(op));
   assert(dst.kind == Mapping::STACK);

   const Mapping& src = map_vcode_reg(vcode_get_arg(op, 0));
   assert(src.kind == Mapping::REGISTER);

   emit_u8(Bytecode::STR);
   emit_reg(bytecode_->machine().sp_reg());
   emit_i16(dst.slot);
   emit_reg(src.slot);
}

void Compiler::compile_load(int op)
{
   const Mapping& src = map_vcode_var(vcode_get_address(op));
   assert(src.kind == Mapping::STACK);

   const Mapping& dst = map_vcode_reg(vcode_get_result(op));
   assert(dst.kind == Mapping::REGISTER);

   emit_u8(Bytecode::LDR);
   emit_reg(dst.slot);
   emit_reg(bytecode_->machine().sp_reg());
   emit_i16(src.slot);
}

void Compiler::compile_cmp(int op)
{
   const Mapping& dst = map_vcode_reg(vcode_get_result(op));
   const Mapping& lhs = map_vcode_reg(vcode_get_arg(op, 0));
   const Mapping& rhs = map_vcode_reg(vcode_get_arg(op, 1));

   assert(dst.kind == Mapping::REGISTER);
   assert(lhs.kind == Mapping::REGISTER);
   assert(rhs.kind == Mapping::REGISTER);

   emit_u8(Bytecode::CMP);
   emit_reg(lhs.slot);
   emit_reg(rhs.slot);

   emit_u8(Bytecode::CSET);
   emit_reg(dst.slot);
   emit_u8(Bytecode::EQ /* XXX */);
}

void Compiler::compile_cond(int op)
{
   const Mapping& src = map_vcode_reg(vcode_get_arg(op, 0));
   assert(src.kind == Mapping::REGISTER);

   emit_u8(Bytecode::CBNZ);
   emit_reg(src.slot);
   emit_patch(vcode_get_target(op, 0));

   emit_u8(Bytecode::JMP);
   emit_patch(vcode_get_target(op, 1));
}

void Compiler::compile_jump(int op)
{
   emit_u8(Bytecode::JMP);
   emit_patch(vcode_get_target(op, 0));
}

void Compiler::compile_mul(int op)
{
   const Mapping& dst = map_vcode_reg(vcode_get_result(op));
   const Mapping& lhs = map_vcode_reg(vcode_get_arg(op, 0));
   const Mapping& rhs = map_vcode_reg(vcode_get_arg(op, 1));

   assert(dst.kind == Mapping::REGISTER);
   assert(lhs.kind == Mapping::REGISTER);
   assert(rhs.kind == Mapping::REGISTER);

   emit_u8(Bytecode::MOV);
   emit_reg(dst.slot);
   emit_reg(lhs.slot);

   emit_u8(Bytecode::MUL);
   emit_reg(dst.slot);
   emit_reg(rhs.slot);
}

void Compiler::emit_reg(int reg)
{
   assert(bytecode_->machine().num_regs() <= 256);
   assert(reg < 256);
   emit_u8(reg);
}

void Compiler::emit_patch(vcode_block_t block)
{
   const unsigned pos = bytes_.size();

   auto it = block_map_.find(block);
   if (it != block_map_.end()) {
      emit_i16(it->second - pos);
   }
   else {
      patches_.push_back(Patch { block, pos });
      emit_i16(-1);
   }
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

void Compiler::emit_i16(int16_t value)
{
   bytes_.push_back(value & 0xff);
   bytes_.push_back((value >> 8) & 0xff);
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

void Dumper::condition()
{
   const char *names[] = { "Z", "NZ", "GT", "LT", "GE", "LE" };
   assert(*bptr_ < ARRAY_LEN(names));

   const char *name = "?";
   switch ((Bytecode::Condition)*bptr_) {
   case Bytecode::Z:  name = "Z";  break;
   case Bytecode::NZ: name = "NZ"; break;
   case Bytecode::GT: name = "GT"; break;
   case Bytecode::LT: name = "LT"; break;
   case Bytecode::GE: name = "GE"; break;
   case Bytecode::LE: name = "LE"; break;
   }

   col_ += printer_.print("%s%s", pos_ == 0 ? " " : ", ", name);
   bptr_++;
   pos_++;
}

void Dumper::indirect()
{
   col_ += printer_.print("%s[%s%+d]", pos_ == 0 ? " " : ", ",
                          bytecode_->machine().fmt_reg(*bptr_),
                          bytecode_->machine().read_i16(bptr_ + 1));
   bptr_ += 3;
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

void Dumper::immed16()
{
   assert(bptr_ + 2 <= bytecode_->bytes() + bytecode_->length());

   col_ += printer_.print("%s%d", pos_ == 0 ? " " : ", ",
                          bytecode_->machine().read_i16(bptr_));
   bptr_ += 2;
   pos_++;
}

void Dumper::immed8()
{
   col_ += printer_.print("%s%d", pos_ == 0 ? " " : ", ", *bptr_);
   bptr_++;
   pos_++;
}

void Dumper::jump_target()
{
   assert(bptr_ + 2 <= bytecode_->bytes() + bytecode_->length());

   const int delta = bytecode_->machine().read_i16(bptr_);

   col_ += printer_.print("%s%d", pos_ == 0 ? " " : ", ",
                          (int)(bptr_ - bytecode_->bytes() + delta));
   bptr_ += 2;
   pos_++;
}

void Dumper::diassemble_one()
{
   switch ((Bytecode::OpCode)*bptr_) {
   case Bytecode::NOP:
      opcode("NOP");
      break;
   case Bytecode::MOVW:
      opcode("MOVW");
      reg();
      immed32();
      break;
   case Bytecode::MOVB:
      opcode("MOVB");
      reg();
      immed8();
      break;
   case Bytecode::RET:
      opcode("RET");
      break;
   case Bytecode::ADD:
      opcode("ADD");
      break;
   case Bytecode::MOV:
      opcode("MOV");
      reg();
      reg();
      break;
   case Bytecode::ADDW:
      opcode("ADDW");
      reg();
      immed32();
      break;
   case Bytecode::ADDB:
      opcode("ADDB");
      reg();
      immed8();
      break;
   case Bytecode::STR:
      opcode("STR");
      indirect();
      reg();
      break;
   case Bytecode::LDR:
      opcode("LDR");
      reg();
      indirect();
      break;
   case Bytecode::MUL:
      opcode("MUL");
      reg();
      reg();
      break;
   case Bytecode::CSET:
      opcode("CSET");
      reg();
      condition();
      break;
   case Bytecode::CMP:
      opcode("CMP");
      reg();
      reg();
      break;
   case Bytecode::JMP:
      opcode("JMP");
      jump_target();
      break;
   case Bytecode::CBZ:
      opcode("CBZ");
      reg();
      jump_target();
      break;
   case Bytecode::CBNZ:
      opcode("CBNZ");
      reg();
      jump_target();
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

Machine::Machine(const char *name, int num_regs, int result_reg, int sp_reg)
   : name_(name),
     num_regs_(num_regs),
     result_reg_(result_reg),
     sp_reg_(sp_reg)
{
}

const char *Machine::fmt_reg(int reg) const
{
   assert(reg < num_regs_);

   if (reg == sp_reg_)
      return "SP";
   else {
      static char buf[32]; // XXX
      checked_sprintf(buf, sizeof(buf), "R%d", reg);
      return buf;
   }
}

int32_t Machine::read_i32(const uint8_t *p) const
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

int16_t Machine::read_i16(const uint8_t *p) const
{
   return p[0] | (p[1] << 8);
}

InterpMachine::InterpMachine()
   : Machine("interp", 256, 0, 255)
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
