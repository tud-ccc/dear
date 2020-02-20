/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#pragma once

namespace dear {
template <class... Args>
struct get_request_type;

template <>
struct get_request_type<> {
  using type = void;
};

template <class Head>
struct get_request_type<Head> {
  using type =
      typename std::remove_cv<typename std::remove_reference<Head>::type>::type;
};

template <class Head, class... Tail>
struct get_request_type<Head, Tail...> {
  using type = std::tuple<
      typename std::remove_cv<typename std::remove_reference<Head>::type>::type,
      typename std::remove_cv<
          typename std::remove_reference<Tail>::type>::type...>;
};
}  // namespace dear
