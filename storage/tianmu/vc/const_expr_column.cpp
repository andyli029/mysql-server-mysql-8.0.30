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

#include "const_expr_column.h"

#include "core/compiled_query.h"
#include "core/mysql_expression.h"
#include "core/rc_attr.h"

namespace Tianmu {
namespace vcolumn {
void ConstExpressionColumn::RequestEval([[maybe_unused]] const core::MIIterator &mit, [[maybe_unused]] const int tta) {
  first_eval = true;
  // TODO: check if parameters were changed before reeval
  if (expr_->GetItem()->type() == core::Item_tianmufield::get_tianmuitem_type()) {
    // a special case when a naked column is a parameter
    last_val = std::make_shared<core::ValueOrNull>(((core::Item_tianmufield *)(expr_->GetItem()))->GetCurrentValue());
    last_val->MakeStringOwner();
  } else
    last_val = expr_->Evaluate();
}

double ConstExpressionColumn::GetValueDoubleImpl([[maybe_unused]] const core::MIIterator &mit) {
  DEBUG_ASSERT(core::ATI::IsNumericType(TypeName()));
  double val = 0;
  if (last_val->IsNull()) return NULL_VALUE_D;
  if (core::ATI::IsIntegerType(TypeName()))
    val = (double)last_val->Get64();
  else if (core::ATI::IsFixedNumericType(TypeName()))
    val = ((double)last_val->Get64()) / types::PowOfTen(ct.GetScale());
  else if (core::ATI::IsRealType(TypeName())) {
    union {
      double d;
      int64_t i;
    } u;
    u.i = last_val->Get64();
    val = u.d;
  } else if (core::ATI::IsDateTimeType(TypeName())) {
    types::RCDateTime vd(last_val->Get64(),
                         TypeName());  // 274886765314048  ->  2000-01-01
    int64_t vd_conv = 0;
    vd.ToInt64(vd_conv);  // 2000-01-01  ->  20000101
    val = (double)vd_conv;
  } else if (core::ATI::IsStringType(TypeName())) {
    auto vs = last_val->ToString();
    if (vs) val = std::stod(*vs);
  } else
    DEBUG_ASSERT(0 && "conversion to double not implemented");
  return val;
}

types::RCValueObject ConstExpressionColumn::GetValueImpl([[maybe_unused]] const core::MIIterator &mit,
                                                         bool lookup_to_num) {
  if (last_val->IsNull()) return types::RCValueObject();

  if (core::ATI::IsStringType((TypeName()))) {
    types::BString s;
    last_val->GetBString(s);
    return s;
  }
  if (core::ATI::IsIntegerType(TypeName())) return types::RCNum(last_val->Get64(), -1, false, TypeName());
  if (core::ATI::IsDateTimeType(TypeName())) return types::RCDateTime(last_val->Get64(), TypeName());
  if (core::ATI::IsRealType(TypeName())) return types::RCNum(last_val->Get64(), 0, true, TypeName());
  if (lookup_to_num || TypeName() == common::CT::NUM)
    return types::RCNum((int64_t)last_val->Get64(), Type().GetScale());
  DEBUG_ASSERT(!"Illegal execution path");
  return types::RCValueObject();
}

int64_t ConstExpressionColumn::GetSumImpl(const core::MIIterator &mit, bool &nonnegative) {
  DEBUG_ASSERT(!core::ATI::IsStringType(TypeName()));
  nonnegative = true;
  if (last_val->IsNull())
    return common::NULL_VALUE_64;  // note that this is a bit ambiguous: the
                                   // same is for sum of nulls and for "not
                                   // implemented"
  if (core::ATI::IsRealType(TypeName())) {
    double res = last_val->GetDouble() * mit.GetPackSizeLeft();
    return *(int64_t *)&res;
  }
  return (last_val->Get64() * mit.GetPackSizeLeft());
}

types::BString ConstExpressionColumn::GetMinStringImpl([[maybe_unused]] const core::MIIterator &mit) {
  types::BString s;
  last_val->GetBString(s);
  return s;
}

types::BString ConstExpressionColumn::GetMaxStringImpl([[maybe_unused]] const core::MIIterator &mit) {
  types::BString s;
  last_val->GetBString(s);
  return s;
}

int64_t ConstExpressionColumn::GetApproxDistValsImpl([[maybe_unused]] bool incl_nulls,
                                                     [[maybe_unused]] core::RoughMultiIndex *rough_mind) {
  return 1;
}

size_t ConstExpressionColumn::MaxStringSizeImpl()  // maximal byte string length
                                                   // in column
{
  return ct.GetPrecision();
}

core::PackOntologicalStatus ConstExpressionColumn::GetPackOntologicalStatusImpl(
    [[maybe_unused]] const core::MIIterator &mit) {
  if (last_val->IsNull()) return core::PackOntologicalStatus::NULLS_ONLY;
  return core::PackOntologicalStatus::UNIFORM;
}

void ConstExpressionColumn::EvaluatePackImpl([[maybe_unused]] core::MIUpdatingIterator &mit,
                                             [[maybe_unused]] core::Descriptor &desc) {
  DEBUG_ASSERT(0);  // comparison of a const with a const should be simplified earlier
}

common::RSValue ConstExpressionColumn::RoughCheckImpl([[maybe_unused]] const core::MIIterator &mit,
                                                      [[maybe_unused]] core::Descriptor &d) {
  return common::RSValue::RS_SOME;  // not implemented
}

types::BString ConstExpressionColumn::DecodeValue_S([[maybe_unused]] int64_t code) {
  types::BString s;
  last_val->GetBString(s);
  return s;
}

}  // namespace vcolumn
}  // namespace Tianmu
