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
#ifndef TIANMU_HANDLER_HA_RCENGINE_H_
#define TIANMU_HANDLER_HA_RCENGINE_H_
#pragma once

// mysql <--> tianmu interface functions
namespace Tianmu {
namespace dbhandler {

void TIANMU_UpdateAndStoreColumnComment(TABLE *table, int field_id, Field *source_field, int source_field_id,
                                     CHARSET_INFO *cs);

bool TIANMU_SetStatementAllowed(THD *thd, LEX *lex);

int TIANMU_HandleSelect(THD *thd, LEX *lex, Query_result *&result_output, ulong setup_tables_done_option, int &res,
                     int &optimize_after_tianmu, int &tianmu_free_join, int with_insert = false);
bool tianmu_load(THD *thd, sql_exchange *ex, TABLE_LIST *table_list, void *arg);

}  // namespace dbhandler
}  //  namespace Tianmu

#endif  // TIANMU_HANDLER_HA_RCENGINE_H_
