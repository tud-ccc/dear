/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#pragma once

#include <reactor-cpp/reactor-cpp.hh>

#include "dear/apd_dependencies.hh"
#include "dear/time_context.hh"

namespace dear {

template <class T>
class SkeletonEventTransactor;

template <class T>
class SkeletonEventTransactor<apd::skeleton::EventDispatcher<T>>
    : public reactor::Reactor {
 private:
  using Event = apd::skeleton::EventDispatcher<T>;

  // state
  Event* event;
  const reactor::Duration deadline;
  apd::Logger& logger;

  // reactions
  reactor::Reaction r_notify{"r_notify", 1, this, [this]() { on_notify(); }};

  // reaction bodies
  void on_notify() {
    auto x = notify.get();
    TimeContext::provide_timestamp(this->get_logical_time() + deadline);
    event->Send(*x);
    TimeContext::invalidate_timestamp();
  }

 public:
  // ports
  reactor::Input<T> notify{"notify", this};

  SkeletonEventTransactor(const std::string& name,
                          reactor::Environment* env,
                          Event* event,
                          reactor::Duration deadline)
      : reactor::Reactor(name, env)
      , event(event)
      , deadline(deadline)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  SkeletonEventTransactor(const std::string& name,
                          reactor::Reactor* container,
                          Event* event,
                          reactor::Duration deadline)
      : reactor::Reactor(name, container)
      , event(event)
      , deadline(deadline)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  void assemble() override {
    r_notify.declare_trigger(&notify);
    r_notify.set_deadline(
        deadline, [this]() { logger.LogError() << "Missed the deadline!"; });
  }
};

}  // namespace dear
