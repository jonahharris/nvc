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

#include "printer.hpp"

#include <cstdarg>
#include <cstdlib>

int FilePrinter::print(const char *fmt, ...)
{
   std::va_list ap;
   va_start(ap, fmt);
   int nchars = vfprintf(file_, fmt, ap);
   va_end(ap);
   return nchars;
}

StdoutPrinter::StdoutPrinter()
   : FilePrinter(stdout)
{
}

BufferPrinter::BufferPrinter()
   : buffer_((char *)xmalloc(DEFAULT_BUFFER)),
     wptr_(buffer_),
     len_(DEFAULT_BUFFER)
{
   buffer_[0] = '\0';
}

BufferPrinter::~BufferPrinter()
{
   buffer_[0] = '\0';
   free(buffer_);
}

int BufferPrinter::print(const char *fmt, ...)
{
   std::va_list ap;
   va_start(ap, fmt);
   int nchars = vsnprintf(wptr_, buffer_ + len_ - wptr_, fmt, ap);
   va_end(ap);

   if (wptr_ + nchars + 1 > buffer_ + len_) {
      const size_t offset = wptr_ - buffer_;
      const size_t nlen = MAX(offset + nchars + 1, len_ * 2);

      buffer_ = (char *)xrealloc(buffer_, nlen);
      len_ = nlen;
      wptr_ = buffer_ + offset;

      va_start(ap, fmt);
      vsnprintf(wptr_, buffer_ + len_ - wptr_, fmt, ap);
      va_end(ap);
   }

   wptr_ += nchars;
   return nchars;
}
