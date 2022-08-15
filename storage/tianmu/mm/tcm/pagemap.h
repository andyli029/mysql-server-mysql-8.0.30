// Copyright (c) 2005, Google Inc.
// All rights reserved.
//
// Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Sanjay Ghemawat <opensource@google.com>
//
//
//
// A data structure used by the caching malloc.  It maps from page# to
// a pointer that contains info about that page.  We use two
// representations: one for 32-bit addresses, and another for 64 bit
// addresses.  Both representations provide the same interface.  The
// first representation is implemented as a flat array, the seconds as
// a three-level radix tree that strips away approximately 1/3rd of
// the bits every time.
//
// The BITS parameter should be the number of bits required to hold
// a page number.  E.g., with 32 bit pointers and 4K pages (i.e.,
// page offset fits in lower 12 bits), BITS == 20.
#ifndef TIANMU_MM_PAGEMAP_H_
#define TIANMU_MM_PAGEMAP_H_
#pragma once

#include <cstdint>
#include <list>

#include "mm/tcm/tccommon.h"

namespace Tianmu {
namespace mm {
// Single-level array
template <int BITS>
class TCMalloc_PageMap1 {
 private:
  static const int LENGTH = 1 << BITS;

  void **array_;

 public:
  using Number = uintptr_t;

  explicit TCMalloc_PageMap1(void *(*allocator)(size_t)) {
    static_assert(BITS <= sizeof(Number) * 8);
    array_ = reinterpret_cast<void **>((*allocator)(sizeof(void *) << BITS));
    memset(array_, 0, sizeof(void *) << BITS);
  }

  // Ensure that the map contains initialized entries "x .. x+n-1".
  // Returns true if successful, false if we could not allocate memory.
  bool Ensure(Number x, size_t n) {
    // Nothing to do since flat array was allocated at start.  All
    // that's left is to check for overflow (that is, we don't want to
    // ensure a number y where array_[y] would be an out-of-bounds
    // access).
    return n <= LENGTH - x;  // an overflow-free way to do "x + n <= LENGTH"
  }

  void PreallocateMoreMemory() {}

  // Return the current value for KEY.  Returns NULL if not yet set,
  // or if k is out of range.
  void *get(Number k) const {
    ASSERT(k < sizeof(Number) * 8);
    if ((k >> BITS) > 0) {
      return NULL;
    }
    return array_[k];
  }

  // REQUIRES "k" is in range "[0,2^BITS-1]".
  // REQUIRES "k" has been ensured before.
  //
  // Sets the value 'v' for key 'k'.
  void set(Number k, void *v) { array_[k] = v; }

  // Return the first non-NULL pointer found in this map for
  // a page number >= k.  Returns NULL if no such number is found.
  void *Next(Number k) const {
    static_assert(BITS <= sizeof(Number) * 8);
    while (k < (1 << BITS)) {
      if (array_[k] != NULL) return array_[k];
      k++;
    }
    return NULL;
  }
};

// Three-level radix tree
template <int BITS>
class TCMalloc_PageMap3 {
 private:
  // How many bits should we consume at each interior level
  static const int INTERIOR_BITS = (BITS + 2) / 3;  // Round-up
  static const unsigned int INTERIOR_LENGTH = 1 << INTERIOR_BITS;

  // How many bits should we consume at leaf level
  static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
  static const int LEAF_LENGTH = 1 << LEAF_BITS;

  std::list<void *> system_alloc_list;

  // Interior node
  struct Node {
    Node *ptrs[INTERIOR_LENGTH];
  };

  // Leaf node
  struct Leaf {
    void *values[LEAF_LENGTH];
  };

  Node *root_;                  // Root of radix tree
  void *(*allocator_)(size_t);  // Memory allocator

  Node *NewNode() {
    Node *result = reinterpret_cast<Node *>((*allocator_)(sizeof(Node)));
    if (result != NULL) {
      memset(result, 0, sizeof(*result));
    }
    system_alloc_list.push_back(result);
    return result;
  }

 public:
  using Number = uintptr_t;

  explicit TCMalloc_PageMap3(void *(*allocator)(size_t)) {
    static_assert(BITS <= sizeof(Number) * 8);
    allocator_ = allocator;
    root_ = NewNode();
  }

  ~TCMalloc_PageMap3() {
    for (auto i : system_alloc_list) free(i);
  }

  void *get(Number k) const {
    const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
    const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
    const Number i3 = k & (LEAF_LENGTH - 1);
    if ((k >> BITS) > 0 || root_->ptrs[i1] == NULL || root_->ptrs[i1]->ptrs[i2] == NULL) {
      return NULL;
    }
    return reinterpret_cast<Leaf *>(root_->ptrs[i1]->ptrs[i2])->values[i3];
  }

  void set(Number k, void *v) {
    ASSERT(k >> BITS == 0);
    const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
    const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
    const Number i3 = k & (LEAF_LENGTH - 1);
    reinterpret_cast<Leaf *>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
  }

  bool Ensure(Number start, size_t n) {
    for (Number key = start; key <= start + n - 1;) {
      const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
      const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

      // Check for overflow
      if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH) return false;

      // Make 2nd level node if necessary
      if (root_->ptrs[i1] == NULL) {
        Node *n = NewNode();
        if (n == NULL) return false;
        root_->ptrs[i1] = n;
      }

      // Make leaf node if necessary
      if (root_->ptrs[i1]->ptrs[i2] == NULL) {
        Leaf *leaf = reinterpret_cast<Leaf *>((*allocator_)(sizeof(Leaf)));
        if (leaf == NULL) return false;
        system_alloc_list.push_back(leaf);
        memset(leaf, 0, sizeof(*leaf));
        root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node *>(leaf);
      }

      // Advance key past whatever is covered by this leaf node
      key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
    }
    return true;
  }

  void PreallocateMoreMemory() {}

  void *Next(Number k) const {
    while (k < (Number(1) << BITS)) {
      const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
      const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
      if (root_->ptrs[i1] == NULL) {
        // Advance to next top-level entry
        k = (i1 + 1) << (LEAF_BITS + INTERIOR_BITS);
      } else {
        Leaf *leaf = reinterpret_cast<Leaf *>(root_->ptrs[i1]->ptrs[i2]);
        if (leaf != NULL) {
          for (Number i3 = (k & (LEAF_LENGTH - 1)); i3 < LEAF_LENGTH; i3++) {
            if (leaf->values[i3] != NULL) {
              return leaf->values[i3];
            }
          }
        }
        // Advance to next interior entry
        k = ((k >> LEAF_BITS) + 1) << LEAF_BITS;
      }
    }
    return NULL;
  }
};
}  // namespace mm
}  // namespace Tianmu

#endif  // TIANMU_MM_PAGEMAP_H_
