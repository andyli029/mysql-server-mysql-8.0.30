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
#ifndef TIANMU_VC_SUBSELECT_COLUMN_H_
#define TIANMU_VC_SUBSELECT_COLUMN_H_
#pragma once

#include "core/mi_updating_iterator.h"
#include "core/pack_guardian.h"
#include "core/rough_value.h"
#include "core/temp_table.h"
#include "vc/multi_value_column.h"

namespace Tianmu {
namespace core {
class MysqlExpression;
class ValueSet;
}  // namespace core

namespace vcolumn {
/*! \brief A column defined by a set of expressions like in IN operator.
 * SubSelectColumn is associated with an core::MultiIndex object and cannot
 * exist without it. Values contained in SubSelectColumn object may not exist
 * physically, they can be computed on demand.
 */
class SubSelectColumn : public MultiValColumn {
 public:
  class IteratorImpl : public IteratorInterface {
   protected:
    IteratorImpl(core::TempTable::RecordIterator const &it_, core::ColumnType const &expected_type_)
        : it(it_), expected_type(expected_type_) {}
    virtual ~IteratorImpl() {}
    void DoNext() override { ++it; }
    std::unique_ptr<LazyValueInterface> DoGetValue() override {
      return std::unique_ptr<LazyValueInterface>(new LazyValueImpl(*it, expected_type));
    }
    bool DoNotEq(IteratorInterface const *it_) const override {
      return it != static_cast<IteratorImpl const *>(it_)->it;
    }
    std::unique_ptr<IteratorInterface> DoClone() const override {
      return std::unique_ptr<IteratorInterface>(new IteratorImpl(*this));
    }
    friend class SubSelectColumn;
    core::TempTable::RecordIterator it;
    core::ColumnType const &expected_type;
  };
  class LazyValueImpl : public LazyValueInterface {
    std::unique_ptr<LazyValueInterface> DoClone() const override {
      return std::unique_ptr<LazyValueInterface>(new LazyValueImpl(record, expected_type));
    }
    types::BString DoGetString() const override { return record[0].ToBString(); }
    types::RCNum DoGetRCNum() const override {
      if (record[0].GetValueType() == types::ValueTypeEnum::NUMERIC_TYPE ||
          record[0].GetValueType() == types::ValueTypeEnum::DATE_TIME_TYPE)
        return static_cast<types::RCNum &>(record[0]);
      TIANMU_ERROR("Bad cast in RCValueObject::RCNum&()");
      return static_cast<types::RCNum &>(record[0]);
    }
    types::RCValueObject DoGetValue() const override {
      types::RCValueObject val = record[0];
      if (expected_type.IsString())
        val = val.ToBString();
      else if (expected_type.IsNumeric() && core::ATI::IsStringType(val.Type())) {
        types::RCNum rc;
        types::RCNum::Parse(*static_cast<types::BString *>(val.Get()), rc, expected_type.GetTypeName());
        val = rc;
      }
      return val;
    }
    int64_t DoGetInt64() const override { return (static_cast<types::RCNum &>(record[0])).GetValueInt64(); }
    bool DoIsNull() const override { return record[0].IsNull(); }
    LazyValueImpl(core::TempTable::Record const &r_, core::ColumnType const &expected_type_)
        : record(r_), expected_type(expected_type_) {}
    virtual ~LazyValueImpl() {}
    core::TempTable::Record record;
    core::ColumnType const &expected_type;
    friend class IteratorImpl;
    friend class SubSelectColumn;
  };
  /*! \brief Creates a column representing subquery. Type of this column should
   * be inferred from type of wrapped core::TempTable.
   *
   * \param table - core::TempTable representing subquery.
   * \param mind - multindex of query containing subquery.
   */
  SubSelectColumn(core::TempTable *subq, core::MultiIndex *mind, core::TempTable *outer_query_temp_table,
                  int temp_table_alias);

  // created copies share subq and table
  SubSelectColumn(const SubSelectColumn &c);

  virtual ~SubSelectColumn();

  bool IsMaterialized() const { return subq->IsMaterialized(); }
  /*! \brief Returns true if subquery is correlated, false otherwise
   * \return bool
   */
  bool IsCorrelated() const;
  bool IsConst() const override { return var_map.size() == 0; }
  bool IsSubSelect() const override { return true; }
  bool IsThreadSafe() override { return IsConst() && MakeParallelReady(); }
  void PrepareAndFillCache();
  bool IsSetEncoded(common::CT at,
                    int scale) override;  // checks whether the set is constant and fixed size
                                          // equal to the given one
  int64_t GetNotNullValueInt64(const core::MIIterator &mit) override { return GetValueInt64Impl(mit); }
  void GetNotNullValueString(types::BString &s, const core::MIIterator &mit) override {
    return GetValueStringImpl(s, mit);
  }
  core::RoughValue RoughGetValue(const core::MIIterator &mit, core::SubSelectOptimizationType sot);
  bool CheckExists(core::MIIterator const &mit) override;
  common::Tribool RoughIsEmpty(const core::MIIterator &mit, core::SubSelectOptimizationType sot);

 protected:
  uint col_idx;  // index of column in table containing result. Doesn't have to
                 // be 0 in case of hidden columns.
  std::shared_ptr<core::TempTableForSubquery> subq;
  int parent_tt_alias;
  types::RCValueObject min;
  types::RCValueObject max;
  bool min_max_uptodate;
  void CalculateMinMax();

  std::unique_ptr<IteratorInterface> BeginImpl(core::MIIterator const &) override;
  std::unique_ptr<IteratorInterface> EndImpl(core::MIIterator const &) override;
  common::Tribool ContainsImpl(core::MIIterator const &, types::RCDataType const &) override;
  common::Tribool Contains64Impl(const core::MIIterator &mit, int64_t val) override;
  bool CopyCondImpl([[maybe_unused]] const core::MIIterator &mit, [[maybe_unused]] types::CondArray &condition,
                    [[maybe_unused]] DTCollation coll) override {
    return false;
  }
  bool CopyCondImpl([[maybe_unused]] const core::MIIterator &mit,
                    [[maybe_unused]] std::shared_ptr<utils::Hash64> &condition,
                    [[maybe_unused]] DTCollation coll) override {
    return false;
  }
  common::Tribool ContainsStringImpl(const core::MIIterator &mit, types::BString &val) override;
  bool IsNullImpl(core::MIIterator const &mit) override;

  types::RCValueObject GetValueImpl(core::MIIterator const &mit, bool lookup_to_num) override;
  int64_t NumOfValuesImpl(core::MIIterator const &mit) override;
  int64_t AtLeastNoDistinctValuesImpl(core::MIIterator const &mit, int64_t const at_least) override;
  bool ContainsNullImpl(core::MIIterator const &mit) override;
  void SetExpectedTypeImpl(core::ColumnType const &) override;
  types::RCValueObject GetSetMinImpl(core::MIIterator const &mit) override;
  types::RCValueObject GetSetMaxImpl(core::MIIterator const &mit) override;
  bool IsEmptyImpl(core::MIIterator const &) override;
  int64_t GetValueInt64Impl(core::MIIterator const &) override;
  double GetValueDoubleImpl(core::MIIterator const &) override;
  void GetValueStringImpl(types::BString &s, core::MIIterator const &mit) override;

  bool IsNullsPossibleImpl([[maybe_unused]] bool val_nulls_possible) override { return true; }
  int64_t GetMinInt64Impl([[maybe_unused]] const core::MIIterator &mit) override {
    // DEBUG_ASSERT( !"To be implemented." );
    return common::MINUS_INF_64;
  }
  int64_t GetMaxInt64Impl([[maybe_unused]] const core::MIIterator &mit) override {
    // DEBUG_ASSERT( !"To be implemented." );
    return common::PLUS_INF_64;
  }
  int64_t RoughMinImpl() override {
    // DEBUG_ASSERT( !"To be implemented." );
    return common::MINUS_INF_64;
  }
  int64_t RoughMaxImpl() override {
    // DEBUG_ASSERT( !"To be implemented." );
    return common::PLUS_INF_64;
  }
  types::BString GetMaxStringImpl(const core::MIIterator &mit) override;
  types::BString GetMinStringImpl(const core::MIIterator &mit) override;

  bool IsDistinctImpl() override {
    // DEBUG_ASSERT( !"To be implemented." );
    return (false);
  }

  size_t MaxStringSizeImpl() override;  // maximal byte string length in column
  core::PackOntologicalStatus GetPackOntologicalStatusImpl(const core::MIIterator &mit) override;
  void EvaluatePackImpl(core::MIUpdatingIterator &mit, core::Descriptor &desc) override;
  virtual common::ErrorCode EvaluateOnIndexImpl([[maybe_unused]] core::MIUpdatingIterator &mit,
                                                [[maybe_unused]] core::Descriptor &desc,
                                                [[maybe_unused]] int64_t limit) override {
    TIANMU_ERROR("To be implemented.");
    return common::ErrorCode::FAILED;
  }

  bool FeedArguments(const core::MIIterator &mit, bool for_rough);

  const core::MysqlExpression::tianmu_fields_cache_t &GetTIANMUItems() const override { return tianmuitems; }

  core::ColumnType expected_type_;
  std::map<core::VarID, core::ValueOrNull> param_cache_for_exact;
  std::map<core::VarID, core::ValueOrNull> param_cache_for_rough;

 private:
  void SetBufs(core::MysqlExpression::var_buf_t *bufs);
  void RequestEval(const core::MIIterator &mit, const int tta) override;
  void PrepareSubqResult(const core::MIIterator &mit, bool exists_only);
  void RoughPrepareSubqCopy(const core::MIIterator &mit, core::SubSelectOptimizationType sot);
  bool MakeParallelReady();

  core::MysqlExpression::TypOfVars var_types;
  mutable core::MysqlExpression::var_buf_t var_buf_for_exact;
  mutable core::MysqlExpression::var_buf_t var_buf_for_rough;

  std::shared_ptr<core::ValueSet> cache;
  int no_cached_values;
  bool first_eval_for_rough;
  bool out_of_date_rough;  // important only for old mysql expr
};

}  // namespace vcolumn
}  // namespace Tianmu

#endif  // TIANMU_VC_SUBSELECT_COLUMN_H_
