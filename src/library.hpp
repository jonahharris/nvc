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
#include "ident.h"

class Library {
public:
   static Library *open(ident_t);
   static Library *open(const char *name);

   ident_t name() const { return name_; }

 private:
   Library(ident_t name);
   Library(const Library&) = delete;

   const ident_t name_;
};
