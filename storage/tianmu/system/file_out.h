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
#ifndef TIANMU_SYSTEM_FILE_OUT_H_
#define TIANMU_SYSTEM_FILE_OUT_H_
#pragma once

#include <fstream>
#include <string>

#include "common/exception.h"
#include "system/channel_out.h"

namespace Tianmu {
namespace system {

class FileOut : public ChannelOut {
 public:
  FileOut(std::string const &filepath) : out_stream_(filepath.c_str(), std::ios_base::out | std::ios_base::app) {
    if (!out_stream_.is_open()) throw common::FileException(std::string("Unable to open ") + std::string(filepath));
  }

  ~FileOut() {}

  ChannelOut &operator<<(short value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(int value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(long value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(float value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(double value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(long double value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(unsigned short value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(unsigned int value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(unsigned long value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(int long long value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(int long long unsigned value) override {
    out_stream_ << value;
    return *this;
  };

  ChannelOut &operator<<(char c) override {
    out_stream_ << c;
    return *this;
  };

  ChannelOut &operator<<(const char *buffer) override {
    out_stream_ << buffer;
    return *this;
  };

  ChannelOut &operator<<(const wchar_t *buffer) override {
    out_stream_ << buffer;
    return *this;
  };

  ChannelOut &operator<<(const std::string &str) override {
    out_stream_ << str.c_str();
    return *this;
  };

  void setf(std::ios_base::fmtflags _Mask) override { out_stream_.setf(_Mask); };
  void precision(std::streamsize prec) override { out_stream_.precision(prec); };

  ChannelOut &flush() override {
    out_stream_.flush();
    return *this;
  };

  ChannelOut &fixed() override {
    out_stream_.setf(std::ios_base::fixed, std::ios_base::floatfield);
    return *this;
  };

  void close() override {
    if (out_stream_.is_open()) out_stream_.close();
  };

  ChannelOut &operator<<(ChannelOut &(*_Pfn)(ChannelOut &)) override { return _Pfn(*this); };

 private:
  std::ofstream out_stream_;
};

}  // namespace system
}  // namespace Tianmu

#endif  // TIANMU_SYSTEM_FILE_OUT_H_
