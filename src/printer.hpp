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

#pragma once

#include "util.h"

#include <cstdio>

struct Printer {
   virtual ~Printer() {}

   __attribute__((format(printf, 2, 3)))
   virtual int print(const char *fmt, ...) = 0;
};

class FilePrinter : public Printer {
public:
   FilePrinter(FILE *f) : file_(f) {}

   int print(const char *fmt, ...);

private:
   FILE *file_;
};

class StdoutPrinter : public FilePrinter {
public:
   StdoutPrinter();
};

class BufferPrinter : public Printer {
public:
   BufferPrinter();
   ~BufferPrinter();
   BufferPrinter(const BufferPrinter&) = delete;

   int print(const char *fmt, ...);
   const char *buffer() const { return buffer_; }

private:
   static const int DEFAULT_BUFFER = 256;

   char *buffer_;
   char *wptr_;
   size_t len_;
};
