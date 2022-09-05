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
#ifndef TIANMU_MM_HEAP_POLICY_H_
#define TIANMU_MM_HEAP_POLICY_H_
#pragma once
#include <cstddef>
#include "mm/memory_block.h"

namespace Tianmu {
namespace mm {

enum class HEAP_STATUS { HEAP_SUCCESS, HEAP_OUT_OF_MEMORY, HEAP_CORRUPTED, HEAP_ERROR };

class HeapPolicy {
 public:
  HeapPolicy(size_t sz) : size_(sz), heap_status_(HEAP_STATUS::HEAP_ERROR) {}
  virtual ~HeapPolicy() {}

  virtual void *alloc(size_t size) = 0;
  virtual void dealloc(void *mh) = 0;
  virtual void *rc_realloc(void *mh, size_t size) = 0;
  virtual size_t getBlockSize(void *mh) = 0;

  HEAP_STATUS getHeapStatus() { return heap_status_; }

 protected:
  size_t size_;
  HEAP_STATUS heap_status_;
};

}  // namespace mm
}  // namespace Tianmu

#endif  // TIANMU_MM_HEAP_POLICY_H_
