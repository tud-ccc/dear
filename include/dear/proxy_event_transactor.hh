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
class ProxyEventTransactor : public reactor::Reactor {
 private:
  using Event = apd::proxy::Event<T>;

  Event* event;

  reactor::PhysicalAction<void> trigger{"trigger", this};
  reactor::LogicalAction<T> send{"send", this};

  reactor::Reaction r_trigger{"r_trigger", 1, this, [this]() { on_trigger(); }};
  reactor::Reaction r_send{"r_send", 2, this, [this]() { on_send(); }};

  reactor::Duration max_network_delay;
  reactor::Duration max_synchronization_error;
  apd::Logger& logger;

 public:
  reactor::Output<T> notify{"notify", this};

  ProxyEventTransactor(const std::string& name,
                       reactor::Environment* env,
                       Event* event,
                       reactor::Duration max_network_delay,
                       reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, env)
      , event(event)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {
    assert(event != nullptr);
    assert(env != nullptr);
    event->Subscribe(ara::com::EventCacheUpdatePolicy::kNewestN, 100);
    event->SetReceiveHandler([this]() { trigger.schedule(); });
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

  void assemble() override {
    r_trigger.declare_trigger(&trigger);
    r_trigger.declare_scheduable_action(&send);
    r_send.declare_trigger(&send);
    r_send.declare_antidependency(&notify);
  }
};

template <class T>
std::unique_ptr<ProxyEventTransactor<T>> create_proxy_event_transactor(
    const std::string& name,
    reactor::Environment* env,
    apd::proxy::Event<T>* event,
    reactor::Duration max_network_delay,
    reactor::Duration max_synchronization_error) {
  return std::make_unique<ProxyEventTransactor<T>>(
      name, env, event, max_network_delay, max_synchronization_error);
}

}  // namespace dear
