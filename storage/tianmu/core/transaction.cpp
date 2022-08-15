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

#include "core/transaction.h"

#include "core/dpn.h"
#include "core/rc_table.h"
#include "core/tools.h"
#include "system/file_system.h"
#include "util/fs.h"

namespace Tianmu {
//current transaction, thread local var.
thread_local core::Transaction *current_txn_;

namespace core {
common::SequenceGenerator Transaction::sg;

std::shared_ptr<RCTable> Transaction::GetTableByPathIfExists(const std::string &table_path) {
  auto iter = m_modified_tables.find(table_path);
  if (iter != m_modified_tables.end()) return iter->second;

  iter = m_readonly_tables.find(table_path);
  if (iter != m_readonly_tables.end()) return iter->second;

  return nullptr;
}

std::shared_ptr<RCTable> Transaction::GetTableByPath(const std::string &table_path) {
  auto t = GetTableByPathIfExists(table_path);
  ASSERT(t, "table " + table_path + " is not opened by transaction!");
  return t;
}

void Transaction::AddTableRD(std::shared_ptr<TableShare> &share) {
  auto table_path = share->Path();
  if (m_readonly_tables.find(table_path) == m_readonly_tables.end())
    m_readonly_tables[table_path] = share->GetSnapshot();
}

void Transaction::RemoveTable(std::shared_ptr<TableShare> &share) {
  auto table_path = share->Path();
  auto it = m_readonly_tables.find(table_path);
  if (it != m_readonly_tables.end()) {
    m_readonly_tables.erase(it);
  }
  auto it2 = m_modified_tables.find(table_path);
  if (it2 != m_modified_tables.end()) {
    m_modified_tables.erase(it2);
  }
}

void Transaction::AddTableWR(std::shared_ptr<TableShare> &share) {
  auto table_path = share->Path();
  ASSERT(m_modified_tables.find(table_path) == m_modified_tables.end(), "Table view already added to transaction!");
  m_modified_tables[table_path] = share->GetTableForWrite();
}

void Transaction::AddTableWRIfNeeded(std::shared_ptr<TableShare> &share) {
  auto table_path = share->Path();
  if (m_modified_tables.find(table_path) == m_modified_tables.end())
    m_modified_tables[table_path] = share->GetTableForWrite();
}

void Transaction::Commit([[maybe_unused]] THD *thd) {
  for (auto const &iter : m_modified_tables) iter.second->CommitVersion();

  m_modified_tables.clear();
  kv_trans.Commit();
}

void Transaction::Rollback([[maybe_unused]] THD *thd, bool force_error_message) {
  TIANMU_LOG(LogCtl_Level::INFO, "rollback transaction %s.", tid.ToString().c_str());

  std::shared_lock<std::shared_mutex> rlock(drop_rename_mutex_);
  if (m_modified_tables.size()) {
    if (force_error_message) {
      const char *message =
          "Tianmu storage engine has encountered an unexpected error. "
          "The current transaction has been rolled back.";
      TIANMU_LOG(LogCtl_Level::ERROR, "%s", message);
      my_message(ER_XA_RBROLLBACK, message, MYF(0));
    }
  }
  for (auto const &iter : m_modified_tables) {
    iter.second->Rollback(tid);
  }
  m_modified_tables.clear();
  kv_trans.Rollback();
}

ulong Transaction::GetThreadID() const { return pthread_self(); }

void Transaction::SuspendDisplay() { display_lock++; }
void Transaction::ResumeDisplay() { display_lock--; }

void Transaction::ResetDisplay() {
  display_lock = 0;
  ConfigureRCControl();
}
}  // namespace core
}  // namespace Tianmu
