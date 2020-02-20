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
class ProxyEventTransactor;

template <class T>
class ProxyEventTransactor<apd::proxy::Event<T>&> : public reactor::Reactor {
 private:
  using Event = apd::proxy::Event<T>;

  // state
  Event* event{nullptr};
  apd::Logger& logger;
  const reactor::Duration max_network_delay;
  const reactor::Duration max_synchronization_error;

  // actions
  reactor::PhysicalAction<void> trigger{"trigger", this};
  reactor::LogicalAction<T> send{"send", this};

  // reactions
  reactor::Reaction r_update_binding{"r_update_binding", 1, this,
                                     [this]() { on_update_binding(); }};
  reactor::Reaction r_trigger{"r_trigger", 2, this, [this]() { on_trigger(); }};
  reactor::Reaction r_send{"r_send", 3, this, [this]() { on_send(); }};

  // reaction bodies
  void on_update_binding() {
    this->event = *update_binding.get();
    if (this->event != nullptr) {
      event->Subscribe(ara::com::EventCacheUpdatePolicy::kNewestN, 100);
      event->SetReceiveHandler([this]() { trigger.schedule(); });
    }
  }

  void on_trigger() {
    event->Update();
    const auto& samples = event->GetCachedSamples();

    for (auto sample : samples) {
      auto timestamp = TimeContext::retrieve_timestamp();
      assert(timestamp.HasValue());
      auto t =
          timestamp.Value() + max_network_delay + max_synchronization_error;
      auto lt = get_logical_time();

      if (t > lt) {
        send.schedule(*sample, t - lt);
      } else {
        logger.LogError() << "Timing violation! Received a message with "
                             "timestamp in the past!";
      }
    }
    event->Cleanup();
  }

  void on_send() { notify.set(send.get()); }

 public:
  // potrs
  reactor::Output<T> notify{"notify", this};
  reactor::Input<Event*> update_binding{"update_binding", this};

  ProxyEventTransactor(const std::string& name,
                       reactor::Environment* env,
                       reactor::Duration max_network_delay,
                       reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, env)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  ProxyEventTransactor(const std::string& name,
                       reactor::Reactor* container,
                       reactor::Duration max_network_delay,
                       reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, container)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  void assemble() override {
    r_update_binding.declare_trigger(&update_binding);
    r_trigger.declare_trigger(&trigger);
    r_trigger.declare_scheduable_action(&send);
    r_send.declare_trigger(&send);
    r_send.declare_antidependency(&notify);
  }
};

}  // namespace dear
