/* Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1335 USA
*/
#ifndef TIANMU_CORE_COMPILATION_TOOLS_H_
#define TIANMU_CORE_COMPILATION_TOOLS_H_
#pragma once

#include "core/query.h"

namespace Tianmu {
namespace core {
#define ASSERT_MYSQL_STRING(x)                           \
  DEBUG_ASSERT(!x.str[x.length] &&                       \
               "Verification that LEX_STRING ends with " \
               "nil")

#define EMPTY_TABLE_CONST_INDICATOR "%%TMP_TABLE%%"

class ReturnMeToMySQLWithError {};

const char *TablePath(TABLE_LIST *tab);
Item *UnRef(Item *item);
int OperationUnmysterify(Item *item, common::ColOperation &oper, bool &distinct, const int group_by_clause);
void PrintItemTree(Item *item, int indent = 0);
void PrintItemTree(char const *info, Item *item);
}  // namespace core
}  // namespace Tianmu

#endif  // TIANMU_CORE_COMPILATION_TOOLS_H_
