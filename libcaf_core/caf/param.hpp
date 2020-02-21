/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <type_traits>

#include "caf/fwd.hpp"

namespace caf {

/// Represents a message handler parameter of type `T` and
/// guarantees copy-on-write semantics.
template <class T>
class param {
public:
  enum flag {
    shared_access,    // x_ lives in a shared message
    exclusive_access, // x_ lives in an unshared message
    private_access,   // x_ is a copy of the original value
  };

  param(const void* ptr, bool is_shared)
    : x_(reinterpret_cast<T*>(const_cast<void*>(ptr))) {
    flag_ = is_shared ? shared_access : exclusive_access;
  }

  param(const param& other) = delete;

  param& operator=(const param& other) = delete;

  param(param&& other) : x_(other.x_), flag_(other.flag_) {
    other.x_ = nullptr;
  }

  ~param() {
    if (flag_ == private_access)
      delete x_;
  }

  const T& get() const {
    return *x_;
  }

  operator const T&() const {
    return *x_;
  }

  const T* operator->() const {
    return x_;
  }

  /// Detaches the value if needed and returns a mutable reference to it.
  T& get_mutable() {
    if (flag_ == shared_access) {
      auto cpy = new T(get());
      x_ = cpy;
      flag_ = private_access;
    }
    return *x_;
  }

  /// Moves the value out of the `param`.
  T&& move() {
    return std::move(get_mutable());
  }

private:
  T* x_;
  flag flag_;
};

/// Converts `T` to `param<T>` unless `T` is arithmetic, an atom constant, or
/// a stream handshake.
template <class T>
struct add_param
  : std::conditional<std::is_arithmetic<T>::value || std::is_empty<T>::value, T,
                     param<T>> {
  // nop
};

template <class T>
struct add_param<stream<T>> {
  using type = stream<T>;
};

/// Convenience alias that wraps `T` into `param<T>` unless `T` is arithmetic,
/// a stream handshake or an atom constant.
template <class T>
using param_t = typename add_param<T>::type;

/// Unpacks `param<T>` to `T`.
template <class T>
struct remove_param {
  using type = T;
};

template <class T>
struct remove_param<param<T>> {
  using type = T;
};

/// Convenience struct for `remove_param<std::decay<T>>`.
template <class T>
struct param_decay {
  using type = typename remove_param<typename std::decay<T>::type>::type;
};

/// @relates param_decay
template <class T>
using param_decay_t = typename param_decay<T>::type;

/// Queries whether `T` is a ::param.
template <class T>
struct is_param {
  static constexpr bool value = false;
};

template <class T>
struct is_param<param<T>> {
  static constexpr bool value = true;
};

/// @relates is_param
template <class T>
constexpr bool is_param_v = is_param<T>::value;

} // namespace caf
