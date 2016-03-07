/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE io_dynamic_remote_actor
#include "caf/test/unit_test.hpp"

#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

namespace {

constexpr char local_host[] = "127.0.0.1";

caf::actor_system_config make_actor_system_config() {
  caf::actor_system_config cfg(caf::test::engine::argc(),
                               caf::test::engine::argv());
  cfg.load<caf::io::middleman>();
  // see `CAF_TEST(custom_message_type)`
  cfg.add_message_type<std::vector<int>>("std::vector<int>");
  return cfg;
}

struct fixture {
  caf::actor_system server_side{make_actor_system_config()};
  caf::actor_system client_side{make_actor_system_config()};
  caf::io::middleman& server_side_mm = server_side.middleman();
  caf::io::middleman& client_side_mm = client_side.middleman();
};

caf::behavior make_pong_behavior() {
  return {
    [](int val) -> int {
      CAF_MESSAGE("pong with " << ++val);
      return val;
    }
  };
}

caf::behavior make_ping_behavior(caf::event_based_actor* self,
                                 caf::actor pong) {
  CAF_MESSAGE("ping with " << 0);
  self->send(pong, 0);
  return {
    [=](int val) -> int {
      if (val == 3) {
        CAF_MESSAGE("ping with exit");
        self->send_exit(self->current_sender(),
                        caf::exit_reason::user_shutdown);
        CAF_MESSAGE("ping quits");
        self->quit();
      }
      CAF_MESSAGE("ping with " << val);
      return val;
    }
  };
}

std::string to_string(const std::vector<int>& vec) {
  std::ostringstream os;
  for (auto i = 0; i + 1 < static_cast<int>(vec.size()); ++i) {
    os << vec[i] << ", ";
  }
  os << vec.back();
  return os.str();
}

caf::behavior make_sort_behavior() {
  return {
    [](std::vector<int>& vec) -> std::vector<int> {
      CAF_MESSAGE("sorter received: " << to_string(vec));
      std::sort(vec.begin(), vec.end());
      CAF_MESSAGE("sorter sent: " << to_string(vec));
      return std::move(vec);
    }
  };
}

caf::behavior make_sort_requester_behavior(caf::event_based_actor* self,
                                           caf::actor sorter) {
  std::vector<int> vec = {5, 4, 3, 2, 1};
  self->send(sorter, std::move(vec));
  return {
    [=](const std::vector<int>& vec) {
      CAF_MESSAGE("sort requester received: " << to_string(vec));
      for (int i = 1; i <= 5; ++i) {
        CAF_CHECK_EQUAL(i, vec[i - 1]);
      }
      self->send_exit(sorter, caf::exit_reason::user_shutdown);
      self->quit();
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(dynamic_remote_actor_tests, fixture)

CAF_TEST(identity_semantics) {
  // server side
  auto server = server_side.spawn(make_pong_behavior);
  auto port1 = server_side_mm.publish(server, 0, local_host);
  auto port2 = server_side_mm.publish(server, 0, local_host);
  CAF_REQUIRE(port1 && *port1);
  CAF_REQUIRE(port2 && *port2);
  CAF_CHECK_NOT_EQUAL(*port1, *port2);
  auto same_server = server_side_mm.remote_actor(local_host, *port2);
  CAF_REQUIRE_EQUAL(same_server, server);
  CAF_CHECK_EQUAL(same_server->node(), server_side.node());
  // client side
  auto server1 = client_side_mm.remote_actor(local_host, *port1);
  auto server2 = client_side_mm.remote_actor(local_host, *port2);
  CAF_CHECK(server1 && server2);
  CAF_CHECK_EQUAL(server1, client_side_mm.remote_actor(local_host, *port1));
  CAF_CHECK_EQUAL(server2, client_side_mm.remote_actor(local_host, *port2));
  // cleanup
  caf::anon_send_exit(server, caf::exit_reason::user_shutdown);
}

CAF_TEST(ping_pong) {
  // server side
  auto port = server_side_mm.publish(server_side.spawn(make_pong_behavior),
                                     0, local_host);
  CAF_REQUIRE(port && *port);
  // client side
  auto pong = client_side_mm.remote_actor(local_host, *port);
  CAF_REQUIRE(pong);
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST(custom_message_type) {
  // server side
  auto port = server_side_mm.publish(server_side.spawn(make_sort_behavior),
                                     0, local_host);
  CAF_REQUIRE(port && *port);
  // client side
  auto sorter = client_side_mm.remote_actor(local_host, *port);
  CAF_REQUIRE(sorter);
  client_side.spawn(make_sort_requester_behavior, sorter);
}

CAF_TEST_FIXTURE_SCOPE_END()
