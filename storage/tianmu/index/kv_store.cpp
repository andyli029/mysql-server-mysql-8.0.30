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

#include "index/kv_store.h"

#include "core/engine.h"

namespace Tianmu {
namespace index {

void KVStore::Init() {

  rocksdb::Options options;
  options.create_if_missing = true;

  if (tianmu_sysvar_index_cache_size != 0) {
    bb_table_option_.no_block_cache = false;
    bb_table_option_.cache_index_and_filter_blocks = true;
    bb_table_option_.block_cache = rocksdb::NewLRUCache(tianmu_sysvar_index_cache_size << 20);
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(bb_table_option_));
  }

  // get column family names from manfest file
  rocksdb::TransactionDBOptions txn_db_options;
  std::vector<std::string> cf_names;
  std::vector<rocksdb::ColumnFamilyDescriptor> cf_descr;
  std::vector<rocksdb::ColumnFamilyHandle *> cf_handles;
  rocksdb::DBOptions db_option(options);
  
  //set the DBOptions's params.
  auto rocksdb_datadir = kv_data_dir_ / ".index";
  int max_compact_threads = std::thread::hardware_concurrency() / 4;
  max_compact_threads = (max_compact_threads > 1) ? max_compact_threads : 1;
  db_option.max_background_compactions = max_compact_threads;
  db_option.max_subcompactions = max_compact_threads;
  db_option.env->SetBackgroundThreads(max_compact_threads, rocksdb::Env::Priority::LOW);
  db_option.statistics = rocksdb::CreateDBStatistics();
  rocksdb::Status status = rocksdb::DB::ListColumnFamilies(db_option, rocksdb_datadir, &cf_names);
  if (!status.ok() && ( (status.subcode() == rocksdb::Status::kNone) || (status.subcode() == rocksdb::Status::kPathNotFound)) ) 
  {
      TIANMU_LOG(LogCtl_Level::INFO, "First init rocksdb, create default cloum family");
      cf_names.push_back(DEFAULT_CF_NAME);
  }

  // Disable compactions to prevent compaction start before compaction filter is ready.
  rocksdb::ColumnFamilyOptions rs_cf_option(options);
  rocksdb::ColumnFamilyOptions index_cf_option(options);
  index_cf_option.disable_auto_compactions = true;
  index_cf_option.compaction_filter_factory.reset(new IndexCompactFilterFactory);
  for (auto &cfn : cf_names) {
    IsRowStoreCF(cfn)? cf_descr.emplace_back(cfn, rs_cf_option) 
                     : cf_descr.emplace_back(cfn, index_cf_option);
  }

  // open db, get column family handles
  status = rocksdb::TransactionDB::Open(options, txn_db_options, rocksdb_datadir, cf_descr, &cf_handles, &txn_db_);
  if (!status.ok()) {
    throw common::Exception("Error opening rocks instance. status msg: " + status.ToString());
  }
  //init the cf manager and ddl manager with dict manager.
  cf_manager_.init(cf_handles);
  if (!dict_manager_.init(txn_db_->GetBaseDB(), &cf_manager_)) {
    throw common::Exception("RocksDB: Failed to initialize data dictionary.");
  }
  if (!ddl_manager_.init(&dict_manager_, &cf_manager_)) {
    throw common::Exception("RocksDB: Failed to initialize DDL manager.");
  }

  // Enable compaction, things needed for compaction filter are finished
  // initializing
  status = txn_db_->EnableAutoCompaction(cf_handles);
  if (!status.ok()) {
    throw common::Exception("RocksDB: Failed to enable compaction.");
  }

  drop_kv_thread_ = std::thread([this] { this->AsyncDropData(); });
  inited_ = true;
}

void KVStore::UnInit() {
  inited_ = !inited_;

  DeleteDataSignal();
  drop_kv_thread_.join();

  // flush all memtables
  for (const auto &cf_handle : cf_manager_.get_all_cf()) {
    txn_db_->Flush(rocksdb::FlushOptions(), cf_handle);
  }
  // Stop all rocksdb background work
  rocksdb::CancelAllBackgroundWork(txn_db_->GetBaseDB(), true);

  // clear primary key info
  ddl_manager_.cleanup();
  dict_manager_.cleanup();
  cf_manager_.cleanup();
  
  //earase unref entries in single cache shard.
  if (bb_table_option_.block_cache) bb_table_option_.block_cache->EraseUnRefEntries();

  //release the rocksdb handler, txn_db_.
  if (txn_db_) {
    delete txn_db_;
    txn_db_ = nullptr;
  }
}

void KVStore::AsyncDropData() {
  while (inited_) {
    std::unique_lock<std::mutex> lk(cv_drop_mtx_);
    //wait for 5 seconds.
    cv_drop_.wait_for(lk, std::chrono::seconds(5 * 60));

    //id set of index wich will be deleted.
    std::vector<GlobalId> del_index_ids;
    dict_manager_.get_ongoing_index(del_index_ids, MetaType::DDL_DROP_INDEX_ONGOING);
    for (auto id : del_index_ids) {
      //set compaction options.
      rocksdb::CompactRangeOptions options;
      options.bottommost_level_compaction = rocksdb::BottommostLevelCompaction::kForce;
      options.exclusive_manual_compaction = false;

      uchar buf_begin[INDEX_NUMBER_SIZE] = {0};
      uchar buf_end[INDEX_NUMBER_SIZE] = {0};
      be_store_index(buf_begin, id.index_id);
      be_store_index(buf_end, id.index_id + 1);

      rocksdb::Range range =
          rocksdb::Range({reinterpret_cast<const char *>(buf_begin), INDEX_NUMBER_SIZE}, 
                         {reinterpret_cast<const char *>(buf_end), INDEX_NUMBER_SIZE});
      rocksdb::ColumnFamilyHandle *cfh = cf_manager_.get_cf_by_id(id.cf_id);
      DEBUG_ASSERT(cfh);
      //delete files by range, [start_index, end_index]
      //for more info: http://rocksdb.org/blog/2018/11/21/delete-range.html
      rocksdb::Status status = rocksdb::DeleteFilesInRange(txn_db_->GetBaseDB(), cfh, &range.start, &range.limit);
      if (!status.ok()) {
        TIANMU_LOG(LogCtl_Level::ERROR, "RocksDB: delete file range fail, status: %s ", status.ToString().c_str());
        if (status.IsShutdownInProgress()) {
          break;
        }
      }
      //star to do compaction.
      status = txn_db_->CompactRange(options, cfh, &range.start, &range.limit);
      if (!status.ok()) {
        TIANMU_LOG(LogCtl_Level::ERROR, "RocksDB: Compact range index fail, status: %s ", status.ToString().c_str());
        if (status.IsShutdownInProgress()) {
          break;
        }
      }
    }

    dict_manager_.finish_indexes(del_index_ids, MetaType::DDL_DROP_INDEX_ONGOING);
  }

  TIANMU_LOG(LogCtl_Level::INFO, "KVStore drop Table thread exiting...");
}

common::ErrorCode KVStore::KVWriteTableMeta(std::shared_ptr<RdbTable> tbl) {
  const std::unique_ptr<rocksdb::WriteBatch> wb = dict_manager_.begin();
  rocksdb::WriteBatch *const batch = wb.get();

  dict_manager_.lock();
  std::shared_ptr<void> defer(nullptr, [this](...) { dict_manager_.unlock(); });

  //writes the tbl defs in batch.
  ddl_manager_.put_and_write(tbl, batch);
  if (!dict_manager_.commit(batch)) {
    TIANMU_LOG(LogCtl_Level::ERROR, "Commit table metadata fail!");
    return common::ErrorCode::FAILED;
  }

  return common::ErrorCode::SUCCESS;
}

common::ErrorCode KVStore::KVDelTableMeta(const std::string &tablename) {
  //  Find the table in the hash
  std::shared_ptr<RdbTable> tbl = ddl_manager_.find(tablename);
  if (!tbl) return common::ErrorCode::FAILED;

  const std::unique_ptr<rocksdb::WriteBatch> wb = dict_manager_.begin();
  rocksdb::WriteBatch *const batch = wb.get();

  dict_manager_.lock();
  std::shared_ptr<void> defer(nullptr, [this](...) { dict_manager_.unlock(); });

  // Remove the table entry in data dictionary (this will also remove it from
  // the persistent data dictionary).
  dict_manager_.add_drop_table(tbl->m_rdbkeys, batch);
  ddl_manager_.remove(tbl, batch);
  if (!dict_manager_.commit(batch)) {
    return common::ErrorCode::FAILED;
  }
  
  //notify Delete data signal.
  DeleteDataSignal();

  return common::ErrorCode::SUCCESS;
}

common::ErrorCode KVStore::KVRenameTableMeta(const std::string &s_name, const std::string &d_name) {
  const std::unique_ptr<rocksdb::WriteBatch> wb = dict_manager_.begin();
  rocksdb::WriteBatch *const batch = wb.get();

  dict_manager_.lock();
  std::shared_ptr<void> defer(nullptr, [this](...) { dict_manager_.unlock(); });

  //start to rename.
  if (!ddl_manager_.rename(s_name, d_name, batch)) {
    TIANMU_LOG(LogCtl_Level::ERROR, "rename table %s not exsit", s_name.c_str());
    return common::ErrorCode::FAILED;
  }

  return dict_manager_.commit(batch)? common::ErrorCode::SUCCESS : common::ErrorCode::FAILED;
}

common::ErrorCode KVStore::KVWriteMemTableMeta(std::shared_ptr<core::RCMemTable> tb_mem) {
  const std::unique_ptr<rocksdb::WriteBatch> wb = dict_manager_.begin();
  rocksdb::WriteBatch *const batch = wb.get();
  
  dict_manager_.lock();
  std::shared_ptr<void> defer(nullptr, [this](...) { dict_manager_.unlock(); });
  
  //put the tb_mem into mem cache and stores into dict data.
  ddl_manager_.put_mem(tb_mem, batch);
  if (!dict_manager_.commit(batch)) {
    TIANMU_LOG(LogCtl_Level::ERROR, "Commit memory table metadata fail!");
    return common::ErrorCode::FAILED;
  }

  return common::ErrorCode::SUCCESS;
}

common::ErrorCode KVStore::KVDelMemTableMeta(std::string table_name) {
  std::shared_ptr<core::RCMemTable> tb_mem = ddl_manager_.find_mem(table_name);
  if (!tb_mem) return common::ErrorCode::FAILED;

  const std::unique_ptr<rocksdb::WriteBatch> wb = dict_manager_.begin();
  rocksdb::WriteBatch *const batch = wb.get();

  dict_manager_.lock();
  std::shared_ptr<void> defer(nullptr, [this](...) { dict_manager_.unlock(); });

  std::vector<GlobalId> dropped_index_ids;
  GlobalId gid;
  //gets column family id.
  gid.cf_id = tb_mem->GetCFHandle()->GetID();
  //gets index id.
  gid.index_id = tb_mem->GetMemID();
  dropped_index_ids.push_back(gid);
  dict_manager_.add_drop_index(dropped_index_ids, batch);
  //removes from mem cache and dict data.
  ddl_manager_.remove_mem(tb_mem, batch);
  if (!dict_manager_.commit(batch)) return common::ErrorCode::FAILED;

  //notify the drop data signal.
  DeleteDataSignal();

  return common::ErrorCode::SUCCESS;
}

common::ErrorCode KVStore::KVRenameMemTableMeta(std::string s_name, std::string d_name) {
  const std::unique_ptr<rocksdb::WriteBatch> wb = dict_manager_.begin();
  rocksdb::WriteBatch *const batch = wb.get();

  dict_manager_.lock();
  std::shared_ptr<void> defer(nullptr, [this](...) { dict_manager_.unlock(); });
  //rename the memtable.
  if (!ddl_manager_.rename_mem(s_name, d_name, batch)) {
    TIANMU_LOG(LogCtl_Level::ERROR, "rename table %s failed", s_name.c_str());
    return common::ErrorCode::FAILED;
  }
  if (!dict_manager_.commit(batch)) return common::ErrorCode::FAILED;

  return common::ErrorCode::SUCCESS;
}

bool KVStore::KVDeleteKey(rocksdb::WriteOptions &wopts, rocksdb::ColumnFamilyHandle *cf, rocksdb::Slice &key) {
  rocksdb::Status s = txn_db_->Delete(wopts, cf, key);
  if (!s.ok()) {
    TIANMU_LOG(LogCtl_Level::ERROR, "Rdb delete key fail: %s", s.ToString().c_str());
    return false;
  }

  return true;
}

bool KVStore::KVWriteBatch(rocksdb::WriteOptions &wopts, rocksdb::WriteBatch *batch) {
  const rocksdb::Status s = txn_db_->GetBaseDB()->Write(wopts, batch);
  if (!s.ok()) {
    TIANMU_LOG(LogCtl_Level::ERROR, "Rdb write batch fail: %s", s.ToString().c_str());
    return false;
  }

  return true;
}

bool IndexCompactFilter::Filter([[maybe_unused]] int level, const rocksdb::Slice &key,
                                [[maybe_unused]] const rocksdb::Slice &existing_value,
                                [[maybe_unused]] std::string *new_value, [[maybe_unused]] bool *value_changed) const {
  GlobalId gl_index_id;
  gl_index_id.cf_id = cf_id_;
  gl_index_id.index_id = be_to_uint32(reinterpret_cast<const uchar *>(key.data()));

  if (gl_index_id.index_id != prev_index_.index_id) {
    should_delete_ = ha_kvstore_->IndexDroping(gl_index_id);
    prev_index_ = gl_index_id;
  }

  if (should_delete_) {
    return true;
  }

  return false;
}

}  // namespace index
}  // namespace Tianmu
