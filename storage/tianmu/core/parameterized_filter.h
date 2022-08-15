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
#ifndef TIANMU_CORE_PARAMETERIZED_FILTER_H_
#define TIANMU_CORE_PARAMETERIZED_FILTER_H_
#pragma once

#include "core/condition.h"
#include "core/cq_term.h"
#include "core/joiner.h"
#include "core/multi_index.h"

namespace Tianmu {
namespace core {
class TempTable;
class RoughMultiIndex;

/*
A class defining multidimensional filter (by means of MultiIndex) on a set of
tables. It can store descriptors defining some restrictions on particular
dimensions. It can be parametrized thus the multiindex is not materialized. It
can also store tree of conditions
*/

class ParameterizedFilter final {
 public:
  ParameterizedFilter(uint32_t power, CondType filter_type = CondType::WHERE_COND);
  ParameterizedFilter(const ParameterizedFilter &);
  virtual ~ParameterizedFilter();
  ParameterizedFilter &operator=(const ParameterizedFilter &pf);
  ParameterizedFilter &operator=(ParameterizedFilter &&pf);
  // ParameterizedFilter & operator =(const ParameterizedFilter & pf);
  void AddConditions(const Condition *conds);
  uint NoParameterizedDescs() { return parametrized_desc.Size(); }
  void ProcessParameters();
  void PrepareRoughMultiIndex();
  void RoughUpdateParamFilter();
  void UpdateMultiIndex(bool count_only, int64_t limit);
  bool RoughUpdateMultiIndex();
  void RoughUpdateJoins();
  bool PropagateRoughToMind();
  void SyntacticalDescriptorListPreprocessing(bool for_rough_query = false);
  void DescriptorListOrdering();
  void DescriptorJoinOrdering();
  void RoughMakeProjections();
  void RoughMakeProjections(int dim, bool update_reduced = true);
  void UpdateJoinCondition(Condition &cond, JoinTips &tips);
  void DisplayJoinResults(DimensionVector &all_involved_dims, JoinAlgType cur_join_type, bool is_outer,
                          int conditions_used);
  void ApplyDescriptor(int desc_number, int64_t limit = -1);
  static bool TryToMerge(Descriptor &d1, Descriptor &d2);
  void PrepareJoiningStep(Condition &join_desc, Condition &desc, int desc_no, MultiIndex &mind);
  void RoughSimplifyCondition(Condition &desc);
  /*! \brief true if the desc vector contains at least 2 1-dimensional
   * descriptors defined for different dimensions e.g. true if contains T.a=1,
   * U.b=7, false if T.a=1, T.b=7
   *
   */
  bool DimsWith1dimFilters();
  // for_or: optimize for bigger result (not the smaller one, as in case of AND)
  double EvaluateConditionNonJoinWeight(Descriptor &d, bool for_or = false);
  double EvaluateConditionJoinWeight(Descriptor &d);
  Condition &GetConditions() { return descriptors; }
  void TaskProcessPacks(MIUpdatingIterator *taskIterator, Transaction *ci, common::RSValue *rf, DimensionVector *dims,
                        int desc_number, int64_t limit, int one_dim);

  MultiIndex *mind;
  bool mind_shallow_memory;
  RoughMultiIndex *rough_mind;
  TempTable *table;

 private:
  Condition descriptors;
  Condition parametrized_desc;
  CondType filter_type;

  void AssignInternal(const ParameterizedFilter &pf);
};
}  // namespace core
}  // namespace Tianmu

#endif  // TIANMU_CORE_PARAMETERIZED_FILTER_H_
