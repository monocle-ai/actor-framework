/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <atomic>
#include <iostream>

#include "cppa/scheduler.hpp"
#include "cppa/exception.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/thread_mapped_actor.hpp"

#include "cppa/detail/actor_count.hpp"
#include "cppa/detail/empty_tuple.hpp"
#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/network_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/decorated_names_map.hpp"
#include "cppa/detail/thread_pool_scheduler.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

using std::cout;
using std::endl;

namespace {

using namespace cppa;
using namespace cppa::detail;

//volatile uniform_type_info_map* s_uniform_type_info_map = 0;

std::atomic<uniform_type_info_map*> s_uniform_type_info_map;
std::atomic<decorated_names_map*> s_decorated_names_map;
std::atomic<network_manager*> s_network_manager;
std::atomic<actor_registry*> s_actor_registry;
std::atomic<group_manager*> s_group_manager;
std::atomic<empty_tuple*> s_empty_tuple;
std::atomic<scheduler*> s_scheduler;

template<typename T>
void stop_and_kill(std::atomic<T*>& ptr) {
    for (;;) {
        auto p = ptr.load();
        if (p == nullptr) {
            return;
        }
        else if (ptr.compare_exchange_weak(p, nullptr)) {
            p->stop();
            delete p;
            ptr = nullptr;
            return;
        }
    }
}

template<typename T>
T* lazy_get(std::atomic<T*>& ptr) {
    T* result = ptr.load(std::memory_order_seq_cst);
    if (result == nullptr) {
        auto tmp = new T;
        if (ptr.compare_exchange_strong(result, tmp, std::memory_order_seq_cst)) {
            return tmp;
        }
        else {
            delete tmp;
            return lazy_get(ptr);
        }
    }
    return result;
}

} // namespace <anonymous>

namespace cppa {

void shutdown() {
    if (self.unchecked() != nullptr) {
        try { self.unchecked()->quit(exit_reason::normal); }
        catch (actor_exited&) { }
    }
    auto rptr = s_actor_registry.load();
    if (rptr) {
        rptr->await_running_count_equal(0);
    }
    stop_and_kill(s_network_manager);
    stop_and_kill(s_scheduler);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    // it's safe now to delete all other singletons now
    delete s_actor_registry.load();
    s_actor_registry = nullptr;
    delete s_group_manager.load();
    s_group_manager = nullptr;
    auto et = s_empty_tuple.load();
    if (et && !et->deref()) delete et;
    s_empty_tuple = nullptr;
    delete s_uniform_type_info_map.load();
    s_uniform_type_info_map = nullptr;
    delete s_decorated_names_map.load();
    s_decorated_names_map = nullptr;
}

} // namespace cppa

namespace cppa { namespace detail {

actor_registry* singleton_manager::get_actor_registry() {
    return lazy_get(s_actor_registry);
}

uniform_type_info_map* singleton_manager::get_uniform_type_info_map() {
    return lazy_get(s_uniform_type_info_map);
}

group_manager* singleton_manager::get_group_manager() {
    return lazy_get(s_group_manager);
}

scheduler* singleton_manager::get_scheduler() {
    return s_scheduler.load();
}

decorated_names_map* singleton_manager::get_decorated_names_map() {
    return lazy_get(s_decorated_names_map);
}

bool singleton_manager::set_scheduler(scheduler* ptr) {
    scheduler* expected = nullptr;
    if (s_scheduler.compare_exchange_weak(expected, ptr)) {
        ptr->start();
        auto nm = network_manager::create_singleton();
        network_manager* nm_expected = nullptr;
        if (s_network_manager.compare_exchange_weak(nm_expected, nm)) {
            nm->start();
        }
        else {
            delete nm;
        }
        return true;
    }
    else {
        delete ptr;
        return false;
    }
}

network_manager* singleton_manager::get_network_manager() {
    network_manager* result = s_network_manager.load();
    if (result == nullptr) {
        scheduler* s = new thread_pool_scheduler;
        // set_scheduler sets s_network_manager
        set_scheduler(s);
        return get_network_manager();
    }
    return result;
}

empty_tuple* singleton_manager::get_empty_tuple() {
    empty_tuple* result = s_empty_tuple.load();
    if (result == nullptr) {
        auto tmp = new empty_tuple;
        if (s_empty_tuple.compare_exchange_weak(result, tmp) == false) {
            delete tmp;
        }
        else {
            result = tmp;
            result->ref();
        }
    }
    return result;
}

} } // namespace cppa::detail
