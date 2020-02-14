/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#pragma once

namespace dear {

#include <reactor-cpp/reactor-cpp.hh>

#include "dear/apd_dependencies.hh"
#include "dear/time_context.hh"

template <class R, class... Args>
class BaseSkeletonMethodTransactor : public reactor::Reactor {
 public:
  using RequestType = std::tuple<typename std::remove_cv<
      typename std::remove_reference<Args>::type>::type...>;
  using RequestValue = reactor::ImmutableValuePtr<RequestType>;

  struct RequestData {
    apd::Promise<R> promise;
    RequestValue args;
    reactor::TimePoint timestamp;

    RequestData(apd::Promise<R>&& promise,
                RequestValue&& args,
                reactor::TimePoint timestamp)
        : promise(std::forward<apd::Promise<R>>(promise))
        , args(std::forward<RequestValue>(args))
        , timestamp(timestamp) {}
  };

 protected:
  // reactor state
  reactor::Duration response_deadline;
  reactor::Duration max_network_delay;
  reactor::Duration max_synchronization_error;
  apd::Logger& logger;
  std::map<reactor::TimePoint, apd::Promise<R>> pending_requests;

  // actions
  reactor::PhysicalAction<RequestData> receive_request{"receive_request", this};
  reactor::LogicalAction<RequestType> send_request{"send_request", this};

  // reactions
  reactor::Reaction r_receive_request{"r_receive_request", 1, this,
                                      [this]() { on_receive_request(); }};
  reactor::Reaction r_send_request{"r_send_request", 2, this,
                                   [this]() { on_send_request(); }};
  reactor::Reaction r_response{"r_response", 3, this,
                               [this]() { on_response(); }};

  // reaction bodies
  void on_receive_request() {
    auto request = receive_request.get();

    auto t = request->timestamp + max_network_delay + max_synchronization_error;
    auto lt = get_logical_time();

    auto result = pending_requests.insert(
        std::make_pair(request->timestamp, std::move(request->promise)));
    assert(result.second);

    if (t > lt) {
      send_request.schedule(std::move(request->args), t - lt);
    } else {
      logger.LogError() << "Timing violation! Received a message with "
                           "timestamp in the past!";
    }
  }

  void on_send_request() { request.set(send_request.get()); }

  void on_response() {
    logger.LogInfo() << "Send response";
    dear::TimeContext::provide_timestamp(this->get_logical_time() +
                                         response_deadline);
    pending_requests.begin()->second.set_value(*response.get());
    dear::TimeContext::invalidate_timestamp();
    pending_requests.erase(pending_requests.begin());
  }

 public:
  // reactor ports
  reactor::Output<RequestType> request{"request", this};
  reactor::Input<R> response{"response", this};

  BaseSkeletonMethodTransactor(const std::string& name,
                               reactor::Environment* env,
                               reactor::Duration response_deadline,
                               reactor::Duration max_network_delay,
                               reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, env)
      , response_deadline(response_deadline)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  BaseSkeletonMethodTransactor(const std::string& name,
                               reactor::Reactor* container,
                               reactor::Duration response_deadline,
                               reactor::Duration max_network_delay,
                               reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, container)
      , response_deadline(response_deadline)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  void assemble() override {
    r_receive_request.declare_trigger(&receive_request);
    r_receive_request.declare_scheduable_action(&send_request);
    r_send_request.declare_trigger(&send_request);
    r_send_request.declare_antidependency(&request);
    r_response.declare_trigger(&response);
  }

  // This is called asynchronously to indicate a new request
  apd::Future<R> process_request(Args&&... args) {
    apd::Promise<R> promise;
    auto future = promise.get_future();
    auto request =
        reactor::make_immutable_value<RequestType>(std::forward<Args>(args)...);
    auto timestamp = TimeContext::retrieve_timestamp();
    assert(timestamp.HasValue());
    auto value = reactor::make_immutable_value<RequestData>(
        std::move(promise), std::move(request), timestamp.Value());
    receive_request.schedule(std::move(value));
    return future;
  }
};

template <class Func>
class SkeletonMethodTransactor;

template <class Service, class R, class... Args>
class SkeletonMethodTransactor<apd::Future<R> (Service::*)(Args...)>
    : public BaseSkeletonMethodTransactor<R, Args...> {
 private:
  using Base = BaseSkeletonMethodTransactor<R, Args...>;

 public:
  SkeletonMethodTransactor(const std::string& name,
                           reactor::Environment* env,
                           reactor::Duration response_deadline,
                           reactor::Duration max_network_delay,
                           reactor::Duration max_synchronization_error)
      : Base(name,
             env,
             response_deadline,
             max_network_delay,
             max_synchronization_error) {}

  SkeletonMethodTransactor(const std::string& name,
                           reactor::Reactor* container,
                           reactor::Duration response_deadline,
                           reactor::Duration max_network_delay,
                           reactor::Duration max_synchronization_error)
      : Base(name,
             container,
             response_deadline,
             max_network_delay,
             max_synchronization_error) {}
};

template <class Service, class... Args>
class SkeletonMethodTransactor<apd::Future<void> (Service::*)(Args...)>
    : public BaseSkeletonMethodTransactor<void, Args...> {
 private:
  using Base = BaseSkeletonMethodTransactor<void, Args...>;

 public:
  SkeletonMethodTransactor(const std::string& name,
                           reactor::Environment* env,
                           reactor::Duration response_deadline,
                           reactor::Duration max_network_delay,
                           reactor::Duration max_synchronization_error)
      : Base(name,
             env,
             response_deadline,
             max_network_delay,
             max_synchronization_error) {}

  SkeletonMethodTransactor(const std::string& name,
                           reactor::Reactor* container,
                           reactor::Duration response_deadline,
                           reactor::Duration max_network_delay,
                           reactor::Duration max_synchronization_error)
      : Base(name,
             container,
             response_deadline,
             max_network_delay,
             max_synchronization_error) {}
};

}  // namespace dear
