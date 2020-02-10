/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#pragma once

#include <reactor-cpp/time.hh>
#include <type_traits>

#include "dear/apd_dependencies.hh"

namespace dear {

namespace internal {

template <class... T>
struct _message_size;

template <class Head, class... Tail>
struct _message_size<Head, Tail...> {
  static size_t size(const std::shared_ptr<::vsomeip::message>& message,
                     size_t offset) {
    // XXX I have no clue why this is required... Somewhere const& gets added
    // to the type and we need to remove it ....
    using base_type = typename std::remove_cv<
        typename std::remove_reference<Head>::type>::type;
    const ::vsomeip::payload& payload = *message->get_payload();
    apd::Deserializer<base_type> deserializer(payload.get_data() + offset,
                                              payload.get_length() - offset);

    offset += deserializer.getSize();
    return _message_size<Tail...>::size(message, offset);
  }
};

template <>
struct _message_size<> {
  static size_t size(const std::shared_ptr<::vsomeip::message>& message,
                     size_t offset) {
    return offset;
  }
};

}  // namespace internal

template <class... T>
uint64_t get_message_size(const std::shared_ptr<::vsomeip::message>& message) {
  return internal::_message_size<T...>::size(message, 0);
}

template <class... Args>
apd::Result<reactor::TimePoint, bool> get_timestamp_from_message(
    const std::shared_ptr<::vsomeip::message>& message) {
  using Result = apd::Result<reactor::TimePoint, bool>;

  size_t payload_size = message->get_payload()->get_length();
  size_t message_size = get_message_size<Args...>(message);

  // check if there is a timestamp attached
  if (payload_size == sizeof(reactor::Duration::rep) + message_size) {
    apd::Unmarshaller<Args..., reactor::Duration::rep> unmarshaller(*message);
    reactor::Duration::rep time_ns = unmarshaller.template unmarshal<sizeof...(Args)>();
    reactor::TimePoint timestamp{reactor::Duration{time_ns}};
    return Result::FromValue(timestamp);
  }

  return Result::FromError(false);
}

}  // namespace dear
