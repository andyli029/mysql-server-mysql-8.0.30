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

#include "type_cast_column.h"
#include "core/transaction.h"
#include "types/rc_num.h"

namespace Tianmu {
namespace vcolumn {

TypeCastColumn::TypeCastColumn(VirtualColumn *from, core::ColumnType const &target_type)
    : VirtualColumn(target_type.RemovedLookup(), from->GetMultiIndex()), full_const(false), vc(from) {
  dim = from->GetDim();
}

TypeCastColumn::TypeCastColumn(const TypeCastColumn &c) : VirtualColumn(c) {
  DEBUG_ASSERT(CanCopy());
  vc = CreateVCCopy(c.vc);
}

String2NumCastColumn::String2NumCastColumn(VirtualColumn *from, core::ColumnType const &to) : TypeCastColumn(from, to) {
  core::MIIterator mit(NULL, PACK_INVALID);
  full_const = vc->IsFullConst();
  if (full_const) {
    types::BString rs;
    types::RCNum rcn;
    vc->GetValueString(rs, mit);
    if (rs.IsNull()) {
      rcv = types::RCNum();
      val = common::NULL_VALUE_64;
    } else {
      common::ErrorCode retc = ct.IsFixed() ? types::RCNum::ParseNum(rs, rcn, ct.GetScale())
                                            : types::RCNum::Parse(rs, rcn, to.GetTypeName());
      if (retc != common::ErrorCode::SUCCESS) {
        std::string s = "Truncated incorrect numeric value: \'";
        s += rs.ToString();
        s += "\'";
      }
    }
    rcv = rcn;
    val = rcn.GetValueInt64();
  }
}

int64_t String2NumCastColumn::GetNotNullValueInt64(const core::MIIterator &mit) {
  if (full_const) return val;
  types::BString rs;
  types::RCNum rcn;
  vc->GetValueString(rs, mit);
  if (ct.IsFixed()) {
    if (types::RCNum::ParseNum(rs, rcn, ct.GetScale()) != common::ErrorCode::SUCCESS) {
      std::string s = "Truncated incorrect numeric value: \'";
      s += rs.ToString();
      s += "\'";
      common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
    }
  } else {
    if (types::RCNum::ParseReal(rs, rcn, ct.GetTypeName()) != common::ErrorCode::SUCCESS) {
      std::string s = "Truncated incorrect numeric value: \'";
      s += rs.ToString();
      s += "\'";
      common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
    }
  }
  return rcn.GetValueInt64();
}

int64_t String2NumCastColumn::GetValueInt64Impl(const core::MIIterator &mit) {
  if (full_const)
    return val;
  else {
    types::BString rs;
    types::RCNum rcn;
    vc->GetValueString(rs, mit);
    if (rs.IsNull()) {
      return common::NULL_VALUE_64;
    } else {
      common::ErrorCode rc;
      if (ct.IsInt()) {  // distinction for convert function
        rc = types::RCNum::Parse(rs, rcn, ct.GetTypeName());
        if (rc != common::ErrorCode::SUCCESS) {
          std::string s = "Truncated incorrect numeric value: \'";
          s += rs.ToString();
          s += "\'";
          common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        }
        if (rc == common::ErrorCode::OUT_OF_RANGE && rcn.GetValueInt64() > 0) return -1;
      } else if (ct.IsFixed()) {
        rc = types::RCNum::ParseNum(rs, rcn, ct.GetScale());
        if (rc != common::ErrorCode::SUCCESS) {
          std::string s = "Truncated incorrect numeric value: \'";
          s += rs.ToString();
          s += "\'";
          common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        }
      } else {
        if (types::RCNum::Parse(rs, rcn) != common::ErrorCode::SUCCESS) {
          std::string s = "Truncated incorrect numeric value: \'";
          s += rs.ToString();
          s += "\'";
          common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        }
      }
      return rcn.GetValueInt64();
    }
  }
}

double String2NumCastColumn::GetValueDoubleImpl(const core::MIIterator &mit) {
  if (full_const)
    return *(double *)&val;
  else {
    types::BString rs;
    types::RCNum rcn;
    vc->GetValueString(rs, mit);
    if (rs.IsNull()) {
      return NULL_VALUE_D;
    } else {
      types::RCNum::Parse(rs, rcn, common::CT::FLOAT);
      // if(types::RCNum::Parse(rs, rcn, common::CT::FLOAT) !=
      // common::ErrorCode::SUCCESS) {
      //	std::string s = "Truncated incorrect numeric value: \'";
      //	s += (std::string)rs;
      //  s += "\'";
      //	PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING,
      // ER_TRUNCATED_WRONG_VALUE, s.c_str());
      //}
      return (double)rcn;
    }
  }
}

types::RCValueObject String2NumCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const)
    return rcv;
  else {
    types::BString rs;
    types::RCNum rcn;
    vc->GetValueString(rs, mit);
    if (rs.IsNull()) {
      return types::RCNum();
    } else if (types::RCNum::Parse(rs, rcn) != common::ErrorCode::SUCCESS) {
      std::string s = "Truncated incorrect numeric value: \'";
      s += rs.ToString();
      s += "\'";
      common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
    }
    return rcn;
  }
}

int64_t String2NumCastColumn::GetMinInt64Impl(const core::MIIterator &m) {
  if (full_const)
    return val;
  else if (IsConst()) {
    // const with parameters
    types::BString rs;
    types::RCNum rcn;
    vc->GetValueString(rs, m);
    if (rs.IsNull()) {
      return common::NULL_VALUE_64;
    } else if (types::RCNum::Parse(rs, rcn) != common::ErrorCode::SUCCESS) {
      std::string s = "Truncated incorrect numeric value: \'";
      s += rs.ToString();
      s += "\'";
      common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
    }
    return rcn.GetValueInt64();
  } else
    return common::MINUS_INF_64;
}

int64_t String2NumCastColumn::GetMaxInt64Impl(const core::MIIterator &m) {
  int64_t v = GetMinInt64Impl(m);
  return v != common::MINUS_INF_64 ? v : common::PLUS_INF_64;
}

String2DateTimeCastColumn::String2DateTimeCastColumn(VirtualColumn *from, core::ColumnType const &to)
    : TypeCastColumn(from, to) {
  full_const = vc->IsFullConst();
  if (full_const) {
    types::BString rbs;
    core::MIIterator mit(NULL, PACK_INVALID);
    vc->GetValueString(rbs, mit);
    if (rbs.IsNull()) {
      val = common::NULL_VALUE_64;
      rcv = types::RCDateTime();
    } else {
      types::RCDateTime rcdt;
      common::ErrorCode rc = types::RCDateTime::Parse(rbs, rcdt, TypeName());
      if (common::IsWarning(rc) || common::IsError(rc)) {
        std::string s = "Incorrect datetime value: \'";
        s += rbs.ToString();
        s += "\'";
        // mysql is pushing this warning anyway, removed duplication
        // PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING,
        // ER_TRUNCATED_WRONG_VALUE, s.c_str());
      }
      val = rcdt.GetInt64();
      rcv = rcdt;
    }
  }
}

int64_t String2DateTimeCastColumn::GetNotNullValueInt64(const core::MIIterator &mit) {
  if (full_const) return val;

  types::RCDateTime rcdt;
  types::BString rbs;
  vc->GetValueString(rbs, mit);
  common::ErrorCode rc = types::RCDateTime::Parse(rbs, rcdt, TypeName());
  if (common::IsWarning(rc) || common::IsError(rc)) {
    std::string s = "Incorrect datetime value: \'";
    s += rbs.ToString();
    s += "\'";
    common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
    return common::NULL_VALUE_64;
  }
  return rcdt.GetInt64();
}

int64_t String2DateTimeCastColumn::GetValueInt64Impl(const core::MIIterator &mit) {
  if (full_const)
    return val;
  else {
    types::RCDateTime rcdt;
    types::BString rbs;
    vc->GetValueString(rbs, mit);
    if (rbs.IsNull())
      return common::NULL_VALUE_64;
    else {
      common::ErrorCode rc = types::RCDateTime::Parse(rbs, rcdt, TypeName());
      if (common::IsWarning(rc) || common::IsError(rc)) {
        std::string s = "Incorrect datetime value: \'";
        s += rbs.ToString();
        s += "\'";
        common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        return common::NULL_VALUE_64;
      } /* else if(common::IsWarning(res)) {
                           std::string s = "Incorrect Date/Time value: ";
                           s += (std::string)rbs;
                           PushWarning(ConnInfo()->Thd(),
           Sql_condition::SL_WARNING, ER_WARN_INVALID_TIMESTAMP,
           s.c_str());
                   }*/
      return rcdt.GetInt64();
    }
  }
}

void String2DateTimeCastColumn::GetValueStringImpl(types::BString &rbs, const core::MIIterator &mit) {
  if (full_const)
    rbs = rcv.ToBString();
  else {
    types::RCDateTime rcdt;
    vc->GetValueString(rbs, mit);
    if (rbs.IsNull())
      return;
    else {
      common::ErrorCode rc = types::RCDateTime::Parse(rbs, rcdt, TypeName());
      if (common::IsWarning(rc) || common::IsError(rc)) {
        std::string s = "Incorrect datetime value: \'";
        s += rbs.ToString();
        s += "\'";
        common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        rbs = types::BString();
        return;
      } /* else if(common::IsWarning(res)) {
                           std::string s = "Incorrect Date/Time value: ";
                           s += (std::string)rbs;
                           PushWarning(ConnInfo()->Thd(),
           Sql_condition::SL_WARNING, ER_WARN_INVALID_TIMESTAMP,
           s.c_str());
                   }*/
      rbs = rcdt.ToBString();
    }
  }
}

types::RCValueObject String2DateTimeCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const)
    return rcv;
  else {
    types::RCDateTime rcdt;
    types::BString rbs;
    vc->GetValueString(rbs, mit);
    if (rbs.IsNull())
      return types::RCDateTime();
    else {
      common::ErrorCode rc = types::RCDateTime::Parse(rbs, rcdt, TypeName());
      if (common::IsWarning(rc) || common::IsError(rc)) {
        std::string s = "Incorrect datetime value: \'";
        s += rbs.ToString();
        s += "\'";
        common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        // return types::RCDateTime();
      }
      return rcdt;
    }
  }
}

int64_t String2DateTimeCastColumn::GetMinInt64Impl(const core::MIIterator &m) {
  if (full_const)
    return val;
  else if (IsConst()) {
    // const with parameters
    return ((types::RCDateTime *)GetValueImpl(m).Get())->GetInt64();
  } else
    return common::MINUS_INF_64;
}

int64_t String2DateTimeCastColumn::GetMaxInt64Impl(const core::MIIterator &m) {
  int64_t v = GetMinInt64Impl(m);
  return v != common::MINUS_INF_64 ? v : common::PLUS_INF_64;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Num2DateTimeCastColumn::Num2DateTimeCastColumn(VirtualColumn *from, core::ColumnType const &to)
    : String2DateTimeCastColumn(from, to) {
  core::MIIterator mit(NULL, PACK_INVALID);
  full_const = vc->IsFullConst();
  if (full_const) {
    core::MIIterator mit(NULL, PACK_INVALID);
    val = vc->GetValueInt64(mit);
    // rcv = from->GetValue(mit);
    types::RCDateTime rcdt;
    if (val != common::NULL_VALUE_64) {
      if (TypeName() == common::CT::TIME) {
        MYSQL_TIME ltime;
        short timehour;
        TIME_from_longlong_time_packed(&ltime, val);
        short second = ltime.second;
        short minute = ltime.minute;
        if (ltime.neg == 1)
          timehour = ltime.hour * -1;
        else
          timehour = ltime.hour;
        types::RCDateTime rctime(timehour, minute, second, common::CT::TIME);
        rcdt = rctime;
      } else {
        common::ErrorCode rc = types::RCDateTime::Parse(val, rcdt, TypeName(), ct.GetPrecision());
        if (common::IsWarning(rc) || common::IsError(rc)) {
          std::string s = "Incorrect datetime value: \'";
          s += rcv.ToBString().ToString();
          s += "\'";
          TIANMU_LOG(LogCtl_Level::WARN, "Num2DateTimeCast %s", s.c_str());
        }
        if (TypeName() == common::CT::TIMESTAMP) {
          // needs to convert value to UTC
          MYSQL_TIME myt;
          memset(&myt, 0, sizeof(MYSQL_TIME));
          myt.year = rcdt.Year();
          myt.month = rcdt.Month();
          myt.day = rcdt.Day();
          myt.hour = rcdt.Hour();
          myt.minute = rcdt.Minute();
          myt.second = rcdt.Second();
          myt.time_type = MYSQL_TIMESTAMP_DATETIME;
          if (!common::IsTimeStampZero(myt)) {
            bool myb;
            // convert local time to UTC seconds from beg. of EPOCHE
            my_time_t secs_utc = current_txn_->Thd()->variables.time_zone->TIME_to_gmt_sec(&myt, &myb);
            // UTC seconds converted to UTC TIME
            common::GMTSec2GMTTime(&myt, secs_utc);
          }
          rcdt = types::RCDateTime(myt, common::CT::TIMESTAMP);
        }
      }

      val = rcdt.GetInt64();
    }
    rcv = rcdt;
  }
}

types::RCValueObject Num2DateTimeCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const)
    return rcv;
  else {
    types::RCDateTime rcdt;
    types::RCValueObject r(vc->GetValue(mit));

    if (!r.IsNull()) {
      common::ErrorCode rc =
          types::RCDateTime::Parse(((types::RCNum)r).GetIntPart(), rcdt, TypeName(), ct.GetPrecision());
      if (common::IsWarning(rc) || common::IsError(rc)) {
        std::string s = "Incorrect datetime value: \'";
        s += r.ToBString().ToString();
        s += "\'";

        common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        return types::RCDateTime();
      }
      return rcdt;
    } else
      return r;
  }
}

int64_t Num2DateTimeCastColumn::GetValueInt64Impl(const core::MIIterator &mit) {
  if (full_const)
    return val;
  else {
    types::RCDateTime rcdt;
    int64_t v = vc->GetValueInt64(mit);
    types::RCNum r(v, vc->Type().GetScale(), vc->Type().IsFloat(), vc->Type().GetTypeName());
    if (v != common::NULL_VALUE_64) {
      common::ErrorCode rc = types::RCDateTime::Parse(r.GetIntPart(), rcdt, TypeName(), ct.GetPrecision());
      if (common::IsWarning(rc) || common::IsError(rc)) {
        std::string s = "Incorrect datetime value: \'";
        s += r.ToBString().ToString();
        s += "\'";

        common::PushWarning(ConnInfo()->Thd(), Sql_condition::SL_WARNING, ER_TRUNCATED_WRONG_VALUE, s.c_str());
        return common::NULL_VALUE_64;
      }
      return rcdt.GetInt64();
    } else
      return v;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime2VarcharCastColumn::DateTime2VarcharCastColumn(VirtualColumn *from, core::ColumnType const &to)
    : TypeCastColumn(from, to) {
  core::MIIterator mit(NULL, PACK_INVALID);
  full_const = vc->IsFullConst();
  if (full_const) {
    int64_t i = vc->GetValueInt64(mit);
    if (i == common::NULL_VALUE_64) {
      rcv = types::BString();
    } else {
      types::RCDateTime rcdt(i, vc->TypeName());
      rcv = rcdt.ToBString();
    }
  }
}

types::RCValueObject DateTime2VarcharCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const)
    return rcv;
  else {
    types::BString rcb;
    vc->GetValueString(rcb, mit);
    if (rcb.IsNull()) {
      return types::BString();
    } else {
      return rcb;
    }
  }
}

Num2VarcharCastColumn::Num2VarcharCastColumn(VirtualColumn *from, core::ColumnType const &to)
    : TypeCastColumn(from, to) {
  core::MIIterator mit(NULL, PACK_INVALID);
  full_const = vc->IsFullConst();
  if (full_const) {
    rcv = vc->GetValue(mit);
    if (rcv.IsNull()) {
      rcv = types::BString();
    } else {
      rcv = rcv.ToBString();
    }
  }
}

types::RCValueObject Num2VarcharCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const)
    return rcv;
  else {
    types::RCValueObject r(vc->GetValue(mit));
    if (r.IsNull()) {
      return types::BString();
    } else {
      return r.ToBString();
    }
  }
}

void Num2VarcharCastColumn::GetValueStringImpl(types::BString &s, const core::MIIterator &m) {
  if (full_const)
    s = rcv.ToBString();
  else {
    int64_t v = vc->GetValueInt64(m);
    if (v == common::NULL_VALUE_64)
      s = types::BString();
    else {
      types::RCNum rcd(v, vc->Type().GetScale(), vc->Type().IsFloat(), vc->TypeName());
      s = rcd.ToBString();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DateTime2NumCastColumn::DateTime2NumCastColumn(VirtualColumn *from, core::ColumnType const &to)
    : TypeCastColumn(from, to) {
  core::MIIterator mit(NULL, PACK_INVALID);
  full_const = vc->IsFullConst();
  if (full_const) {
    rcv = vc->GetValue(mit);
    if (rcv.IsNull()) {
      rcv = types::RCNum();
      val = common::NULL_VALUE_64;
    } else {
      ((types::RCDateTime *)rcv.Get())->ToInt64(val);
      if (vc->TypeName() == common::CT::YEAR && vc->Type().GetPrecision() == 2) val %= 100;
      if (to.IsFloat()) {
        double x = (double)val;
        val = *(int64_t *)&x;
      }
      rcv = types::RCNum(val, ct.GetScale(), ct.IsFloat(), TypeName());
    }
  }
}

int64_t DateTime2NumCastColumn::GetNotNullValueInt64(const core::MIIterator &mit) {
  if (full_const) return val;
  int64_t v = vc->GetNotNullValueInt64(mit);
  if (vc->TypeName() == common::CT::YEAR && vc->Type().GetPrecision() == 2) v %= 100;
  types::RCDateTime rdt(v, vc->TypeName());
  int64_t r;
  rdt.ToInt64(r);
  if (Type().IsFloat()) {
    double x = (double)r;
    r = *(int64_t *)&x;
  }
  return r;
}

int64_t DateTime2NumCastColumn::GetValueInt64Impl(const core::MIIterator &mit) {
  if (full_const)
    return val;
  else {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) return v;
    types::RCDateTime rdt(v, vc->TypeName());
    int64_t r;
    rdt.ToInt64(r);
    if (vc->TypeName() == common::CT::YEAR && vc->Type().GetPrecision() == 2) r = r % 100;
    if (Type().IsFloat()) {
      double x = (double)r;
      r = *(int64_t *)&x;
    }
    return r;
  }
}

double DateTime2NumCastColumn::GetValueDoubleImpl(const core::MIIterator &mit) {
  if (full_const) {
    int64_t v;
    types::RCDateTime rdt(val, vc->TypeName());
    rdt.ToInt64(v);
    return (double)v;
  } else {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) return NULL_VALUE_D;
    types::RCDateTime rdt(v, vc->TypeName());
    rdt.ToInt64(v);
    return (double)v;
  }
}

types::RCValueObject DateTime2NumCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const)
    return rcv;
  else {
    types::RCValueObject v(vc->GetValue(mit));
    if (v.IsNull()) {
      return types::RCNum();
    } else {
      int64_t r;
      ((types::RCDateTime *)v.Get())->ToInt64(r);
      if (vc->TypeName() == common::CT::YEAR && vc->Type().GetPrecision() == 2) r %= 100;
      if (Type().IsFloat()) {
        double x = (double)r;
        r = *(int64_t *)&x;
      }
      return types::RCNum(r, ct.GetScale(), ct.IsFloat(), TypeName());
    }
  }
}

TimeZoneConversionCastColumn::TimeZoneConversionCastColumn(VirtualColumn *from)
    : TypeCastColumn(from, core::ColumnType(common::CT::DATETIME)) {
  DEBUG_ASSERT(from->TypeName() == common::CT::TIMESTAMP);
  core::MIIterator mit(NULL, PACK_INVALID);
  full_const = vc->IsFullConst();
  if (full_const) {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) {
      rcv = types::RCDateTime();
      val = common::NULL_VALUE_64;
    } else {
      rcv = types::RCDateTime(v, vc->TypeName());
      types::RCDateTime::AdjustTimezone(rcv);
      val = ((types::RCDateTime *)rcv.Get())->GetInt64();
    }
  }
}

int64_t TimeZoneConversionCastColumn::GetNotNullValueInt64(const core::MIIterator &mit) {
  if (full_const) return val;
  int64_t v = vc->GetNotNullValueInt64(mit);
  types::RCDateTime rdt(v, vc->TypeName());
  types::RCDateTime::AdjustTimezone(rdt);
  return rdt.GetInt64();
}

int64_t TimeZoneConversionCastColumn::GetValueInt64Impl(const core::MIIterator &mit) {
  if (full_const)
    return val;
  else {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) return v;
    types::RCDateTime rdt(v, vc->TypeName());
    types::RCDateTime::AdjustTimezone(rdt);
    return rdt.GetInt64();
  }
}

types::RCValueObject TimeZoneConversionCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool b) {
  if (full_const) {
    if (Type().IsString()) return rcv.ToBString();
    return rcv;
  } else {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) return types::RCDateTime();
    types::RCDateTime rdt(v, vc->TypeName());
    types::RCDateTime::AdjustTimezone(rdt);
    if (Type().IsString()) return rdt.ToBString();
    return rdt;
  }
}

double TimeZoneConversionCastColumn::GetValueDoubleImpl(const core::MIIterator &mit) {
  if (full_const) {
    int64_t v;
    types::RCDateTime rdt(val, vc->TypeName());
    rdt.ToInt64(v);
    return (double)v;
  } else {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) return NULL_VALUE_D;
    types::RCDateTime rdt(v, vc->TypeName());
    types::RCDateTime::AdjustTimezone(rdt);
    rdt.ToInt64(v);
    return (double)v;
  }
}

void TimeZoneConversionCastColumn::GetValueStringImpl(types::BString &s, const core::MIIterator &mit) {
  if (full_const) {
    s = rcv.ToBString();
  } else {
    int64_t v = vc->GetValueInt64(mit);
    if (v == common::NULL_VALUE_64) {
      s = types::BString();
      return;
    }
    types::RCDateTime rdt(v, vc->TypeName());
    types::RCDateTime::AdjustTimezone(rdt);
    s = rdt.ToBString();
  }
}

types::RCValueObject StringCastColumn::GetValueImpl(const core::MIIterator &mit, [[maybe_unused]] bool lookup_to_num) {
  types::BString s;
  vc->GetValueString(s, mit);
  return s;
}

}  // namespace vcolumn
}  // namespace Tianmu
