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
#ifndef TIANMU_MM_HUGE_HEAP_POLICY_H_
#define TIANMU_MM_HUGE_HEAP_POLICY_H_
#pragma once

#include <string>

#include "mm/tcm_heap_policy.h"

namespace Tianmu {
namespace mm {

class HugeHeap : public TCMHeap {
 public:
  HugeHeap(std::string hugedir, size_t size);
  virtual ~HugeHeap();

 private:
  int fd_;
  void *heap_frame_;
  char huge_filename_[2048];
};

}  // namespace mm
}  // namespace Tianmu

#endif  // TIANMU_MM_HUGE_HEAP_POLICY_H_
