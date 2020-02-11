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

template <class R, class... Args>
class BaseProxyMethodTransactor : public reactor::Reactor {
 public:
  using Method = apd::proxy::Method<R(Args...)>;
  using ResultFuture = apd::Future<R>;
  using ResultFuturePtr = std::shared_ptr<ResultFuture>;
  using RequestType = std::tuple<Args...>;

 protected:
  Method* method;

  reactor::Duration request_deadline;
  reactor::Duration max_network_delay;
  reactor::Duration max_synchronization_error;
  apd::Logger& logger;

  reactor::LogicalAction<ResultFuture> send_response{"send_response", this};

 private:
  reactor::PhysicalAction<ResultFuture> receive_response{"receive_response",
                                                         this};

  reactor::Reaction r_request{"r_request", 1, this, [this]() { on_request(); }};
  reactor::Reaction r_receive_response{"r_receive_response", 1, this,
                                       [this]() { on_receive_response(); }};
  reactor::Reaction r_send_response{"r_send_response", 1, this,
                                    [this]() { on_send_response(); }};

 public:
  // reactor ports
  reactor::Input<RequestType> request{"request", this};
  reactor::Output<R> response{"response", this};

  BaseProxyMethodTransactor(const std::string& name,
                            reactor::Environment* env,
                            Method* method,
                            reactor::Duration request_deadline,
                            reactor::Duration max_network_delay,
                            reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, env)
      , method(method)
      , request_deadline(request_deadline)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {
    assert(method != nullptr);
    assert(env != nullptr);
  }

  void assemble() override {
    r_request.declare_trigger(&request);
    r_receive_response.declare_trigger(&receive_response);
    r_receive_response.declare_scheduable_action(&send_response);
    r_send_response.declare_trigger(&send_response);
    r_send_response.declare_antidependency(&response);
  }

  void on_request() {
    dear::TimeContext::provide_timestamp(this->get_logical_time() +
                                         request_deadline);
    auto future = std::apply(*(this->method), *request.get());
    dear::TimeContext::invalidate_timestamp();

    // make sure we keep the future alive until the callback is issued
    auto future_ptr =
        reactor::make_mutable_value<decltype(future)>(std::move(future));
    future_ptr->then([this, &future_ptr]() {
      receive_response.schedule(std::move(future_ptr));
    });
  }

  void on_receive_response() {
    auto timestamp = TimeContext::retrieve_timestamp();
    assert(timestamp.HasValue());
    auto t = timestamp.Value() + max_network_delay + max_synchronization_error;
    auto lt = get_logical_time();

    if (t > lt) {
      send_response.schedule(receive_response.get(), t - lt);
    } else {
      logger.LogError() << "Timing violation! Received a message with "
                           "timestamp in the past!";
    }
  }

  virtual void on_send_response() = 0;
};

template <class R, class... Args>
class ProxyMethodTransactor : public BaseProxyMethodTransactor<R, Args...> {
 private:
  using Base = BaseProxyMethodTransactor<R, Args...>;

 public:
  ProxyMethodTransactor(const std::string& name,
                        reactor::Environment* env,
                        apd::proxy::Method<R(Args...)>* method,
                        reactor::Duration request_deadline,
                        reactor::Duration max_network_delay,
                        reactor::Duration max_synchronization_error)
      : Base(name,
             env,
             method,
             request_deadline,
             max_network_delay,
             max_synchronization_error) {}

  void on_send_response() override {
    auto future_ptr = this->send_response.get();
    auto result = future_ptr->GetResult();
    assert(result.HasValue());
    this->response.set(result.Value());
  }
};

template <class... Args>
class ProxyMethodTransactor<void, Args...>
    : public BaseProxyMethodTransactor<void, Args...> {
 private:
  using Base = BaseProxyMethodTransactor<void, Args...>;

 public:
  ProxyMethodTransactor(const std::string& name,
                        reactor::Environment* env,
                        apd::proxy::Method<void(Args...)>* method,
                        reactor::Duration request_deadline,
                        reactor::Duration max_network_delay,
                        reactor::Duration max_synchronization_error)
      : Base(name,
             env,
             method,
             request_deadline,
             max_network_delay,
             max_synchronization_error) {}

  void on_send_response() override { this->response.set(); }
};

template <class R, class... Args>
std::unique_ptr<ProxyMethodTransactor<R, Args...>>
create_proxy_method_transactor(const std::string& name,
                               reactor::Environment* env,
                               apd::proxy::Method<R(Args...)>* method,
                               reactor::Duration request_deadline,
                               reactor::Duration max_network_delay,
                               reactor::Duration max_synchronization_error) {
  return std::make_unique<ProxyMethodTransactor<R, Args...>>(
      name, env, method, request_deadline, max_network_delay,
      max_synchronization_error);
}

}  // namespace dear
