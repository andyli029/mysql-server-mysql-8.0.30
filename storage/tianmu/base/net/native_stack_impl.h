/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 * Copyright (c) 2022 StoneAtom, Inc. All rights reserved.
 */
#ifndef TIANMU_BASE_NATIVE_STACK_IMPL_H_
#define TIANMU_BASE_NATIVE_STACK_IMPL_H_
#pragma once

#include "base/core/reactor.h"
#include "base/net/stack.h"

namespace Tianmu {
namespace base {
namespace net {

using namespace Tianmu::base;

template <typename Protocol>
class native_server_socket_impl;

template <typename Protocol>
class native_connected_socket_impl;

class native_network_stack;

// native_server_socket_impl
template <typename Protocol>
class native_server_socket_impl : public server_socket_impl {
  typename Protocol::listener _listener;

 public:
  native_server_socket_impl(Protocol &proto, uint16_t port, listen_options opt);
  virtual future<connected_socket, socket_address> accept() override;
  virtual void abort_accept() override;
};

template <typename Protocol>
native_server_socket_impl<Protocol>::native_server_socket_impl(Protocol &proto, uint16_t port, listen_options opt)
    : _listener(proto.listen(port)) {}

template <typename Protocol>
future<connected_socket, socket_address> native_server_socket_impl<Protocol>::accept() {
  return _listener.accept().then([](typename Protocol::connection conn) {
    return make_ready_future<connected_socket, socket_address>(
        connected_socket(std::make_unique<native_connected_socket_impl<Protocol>>(make_lw_shared(std::move(conn)))),
        make_ipv4_address(conn.foreign_ip().ip, conn.foreign_port()));
  });
}

template <typename Protocol>
void native_server_socket_impl<Protocol>::abort_accept() {
  _listener.abort_accept();
}

// native_connected_socket_impl
template <typename Protocol>
class native_connected_socket_impl : public connected_socket_impl {
  lw_shared_ptr<typename Protocol::connection> _conn;
  class native_data_source_impl;
  class native_data_sink_impl;

 public:
  explicit native_connected_socket_impl(lw_shared_ptr<typename Protocol::connection> conn) : _conn(std::move(conn)) {}
  virtual data_source source() override;
  virtual data_sink sink() override;
  virtual void shutdown_input() override;
  virtual void shutdown_output() override;
  virtual void set_nodelay(bool nodelay) override;
  virtual bool get_nodelay() const override;
  void set_keepalive(bool keepalive) override;
  bool get_keepalive() const override;
  void set_keepalive_parameters(const keepalive_params &) override;
  keepalive_params get_keepalive_parameters() const override;
};

template <typename Protocol>
class native_socket_impl final : public socket_impl {
  Protocol &_proto;
  lw_shared_ptr<typename Protocol::connection> _conn;

 public:
  explicit native_socket_impl(Protocol &proto) : _proto(proto), _conn(nullptr) {}

  virtual future<connected_socket> connect(socket_address sa, socket_address local,
                                           transport proto = transport::TCP) override {
    // TODO: implement SCTP
    assert(proto == transport::TCP);

    // FIXME: local is ignored since native stack does not support multiple IPs
    // yet
    assert(sa.as_posix_sockaddr().sa_family == AF_INET);

    _conn = make_lw_shared<typename Protocol::connection>(_proto.connect(sa));
    return _conn->connected().then([conn = _conn]() mutable {
      auto csi = std::make_unique<native_connected_socket_impl<Protocol>>(std::move(conn));
      return make_ready_future<connected_socket>(connected_socket(std::move(csi)));
    });
  }

  virtual void shutdown() override {
    if (_conn) {
      _conn->shutdown_connect();
    }
  }
};

template <typename Protocol>
class native_connected_socket_impl<Protocol>::native_data_source_impl final : public data_source_impl {
  using connection_type = typename Protocol::connection;
  lw_shared_ptr<connection_type> _conn;
  size_t _cur_frag = 0;
  bool _eof = false;
  packet _buf;

 public:
  explicit native_data_source_impl(lw_shared_ptr<connection_type> conn) : _conn(std::move(conn)) {}
  virtual future<temporary_buffer<char>> get() override {
    if (_eof) {
      return make_ready_future<temporary_buffer<char>>(temporary_buffer<char>(0));
    }
    if (_cur_frag != _buf.nr_frags()) {
      auto &f = _buf.fragments()[_cur_frag++];
      return make_ready_future<temporary_buffer<char>>(
          temporary_buffer<char>(f.base, f.size, make_deleter(deleter(), [p = _buf.share()]() mutable {})));
    }
    return _conn->wait_for_data().then([this] {
      _buf = _conn->read();
      _cur_frag = 0;
      _eof = !_buf.len();
      return get();
    });
  }
  future<> close() override {
    _conn->close_write();
    return make_ready_future<>();
  }
};

template <typename Protocol>
class native_connected_socket_impl<Protocol>::native_data_sink_impl final : public data_sink_impl {
  using connection_type = typename Protocol::connection;
  lw_shared_ptr<connection_type> _conn;

 public:
  explicit native_data_sink_impl(lw_shared_ptr<connection_type> conn) : _conn(std::move(conn)) {}
  virtual future<> put(packet p) override { return _conn->send(std::move(p)); }
  virtual future<> close() override {
    _conn->close_write();
    return make_ready_future<>();
  }
};

template <typename Protocol>
data_source native_connected_socket_impl<Protocol>::source() {
  return data_source(std::make_unique<native_data_source_impl>(_conn));
}

template <typename Protocol>
data_sink native_connected_socket_impl<Protocol>::sink() {
  return data_sink(std::make_unique<native_data_sink_impl>(_conn));
}

template <typename Protocol>
void native_connected_socket_impl<Protocol>::shutdown_input() {
  _conn->close_read();
}

template <typename Protocol>
void native_connected_socket_impl<Protocol>::shutdown_output() {
  _conn->close_write();
}

template <typename Protocol>
void native_connected_socket_impl<Protocol>::set_nodelay(bool nodelay) {
  // FIXME: implement
}

template <typename Protocol>
bool native_connected_socket_impl<Protocol>::get_nodelay() const {
  // FIXME: implement
  return true;
}

template <typename Protocol>
void native_connected_socket_impl<Protocol>::set_keepalive(bool keepalive) {
  // FIXME: implement
  std::cerr << "Keepalive is not supported by native stack" << std::endl;
}
template <typename Protocol>
bool native_connected_socket_impl<Protocol>::get_keepalive() const {
  // FIXME: implement
  return false;
}

template <typename Protocol>
void native_connected_socket_impl<Protocol>::set_keepalive_parameters(const keepalive_params &) {
  // FIXME: implement
  std::cerr << "Keepalive parameters are not supported by native stack" << std::endl;
}

template <typename Protocol>
keepalive_params native_connected_socket_impl<Protocol>::get_keepalive_parameters() const {
  // FIXME: implement
  return tcp_keepalive_params{std::chrono::seconds(0), std::chrono::seconds(0), 0};
}

}  // namespace net
}  // namespace base
}  // namespace Tianmu

#endif  // TIANMU_BASE_NATIVE_STACK_IMPL_H_
