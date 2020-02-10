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
class SkeletonEventTransactor : public reactor::Reactor {
 private:
  using Event = apd::skeleton::EventDispatcher<T>;

  Event* event;

  reactor::Reaction r_notify{"r_notify", 1, this, [this]() { on_notify(); }};

  reactor::Duration deadline;

  apd::Logger& logger;

 public:
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
                                 ara::log::LogLevel::kDebug)) {
    assert(env != nullptr);
    assert(event != nullptr);
  }

  void assemble() override {
    r_notify.declare_trigger(&notify);
    r_notify.set_deadline(
        deadline, [this]() { logger.LogError() << "Missed the deadline!"; });
  }

  void on_notify() {
    logger.LogDebug() << "notify";
    auto x = notify.get();
    TimeContext::provide_timestamp(this->get_logical_time() + deadline);
    event->Send(*x);
    TimeContext::invalidate_timestamp();
  }
};  // namespace dear

template <class T>
std::unique_ptr<SkeletonEventTransactor<T>> create_skeleton_event_transactor(
    const std::string& name,
    reactor::Environment* env,
    apd::skeleton::EventDispatcher<T>* event,
    reactor::Duration deadline) {
  return std::make_unique<SkeletonEventTransactor<T>>(name, env, event,
                                                      deadline);
}

}  // namespace dear
