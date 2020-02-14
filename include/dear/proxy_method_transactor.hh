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
  using ResultFutureValue = reactor::ImmutableValuePtr<ResultFuture>;
  using RequestType = std::tuple<typename std::remove_cv<
      typename std::remove_reference<Args>::type>::type...>;
  using ResponseValue = reactor::ImmutableValuePtr<R>;

  struct ResponseData {
    ResultFutureValue future;
    reactor::TimePoint timestamp;

    ResponseData(ResultFutureValue&& future, reactor::TimePoint timestamp)
        : future(std::forward<ResultFutureValue>(future))
        , timestamp(timestamp) {}
  };

 protected:
  // reactor state
  Method* method{nullptr};
  const reactor::Duration request_deadline;
  const reactor::Duration max_network_delay;
  const reactor::Duration max_synchronization_error;
  apd::Logger& logger;

  // actions
  reactor::LogicalAction<ResultFuture> send_response{"send_response", this};

 private:
  reactor::PhysicalAction<ResponseData> receive_response{"receive_response",
                                                         this};

  // reactions
  reactor::Reaction r_update_binding{"r_update_binding", 1, this,
                                     [this]() { on_update_binding(); }};
  reactor::Reaction r_request{"r_request", 2, this, [this]() { on_request(); }};
  reactor::Reaction r_receive_response{"r_receive_response", 3, this,
                                       [this]() { on_receive_response(); }};
  reactor::Reaction r_send_response{"r_send_response", 4, this,
                                    [this]() { on_send_response(); }};

  // reaction bodies
  void on_update_binding() { this->method = *update_binding.get(); }

  void on_request() {
    // only send requests if this transactor is bound to a service method
    if (method == nullptr) {
      logger.LogWarn() << "Dropping a request as the transactor was not yet "
                          "bound to a service method";
      return;
    }

    dear::TimeContext::provide_timestamp(this->get_logical_time() +
                                         request_deadline);
    auto future = std::apply(*(this->method), *request.get());
    dear::TimeContext::invalidate_timestamp();

    // make sure we keep the future alive until the callback is issued
    auto future_ptr =
        reactor::make_immutable_value<decltype(future)>(std::move(future));
    // define an asynchronous callback for the response that then triggers a
    // physical action
    future_ptr->then([this, future_ptr]() mutable {
      auto timestamp = TimeContext::retrieve_timestamp();
      assert(timestamp.HasValue());
      auto response_data = reactor::make_immutable_value<ResponseData>(
          std::move(future_ptr), timestamp.Value());
      receive_response.schedule(std::move(response_data));
    });
  }

  void on_receive_response() {
    auto response_data = receive_response.get();
    auto t = response_data->timestamp + max_network_delay +
             max_synchronization_error;
    auto lt = get_logical_time();

    if (t > lt) {
      send_response.schedule(std::move(response_data->future), t - lt);
    } else {
      logger.LogError() << "Timing violation! Received a message with "
                           "timestamp in the past!";
    }
  }

 protected:
  virtual void on_send_response() = 0;

 public:
  // reactor ports
  reactor::Input<RequestType> request{"request", this};
  reactor::Output<R> response{"response", this};
  reactor::Input<Method*> update_binding{"update_binding", this};

  BaseProxyMethodTransactor(const std::string& name,
                            reactor::Environment* env,
                            reactor::Duration request_deadline,
                            reactor::Duration max_network_delay,
                            reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, env)
      , request_deadline(request_deadline)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}
  BaseProxyMethodTransactor(const std::string& name,
                            reactor::Reactor* container,
                            reactor::Duration request_deadline,
                            reactor::Duration max_network_delay,
                            reactor::Duration max_synchronization_error)
      : reactor::Reactor(name, container)
      , request_deadline(request_deadline)
      , max_network_delay(max_network_delay)
      , max_synchronization_error(max_synchronization_error)
      , logger(apd::CreateLogger(name.c_str(),
                                 name.c_str(),
                                 ara::log::LogLevel::kDebug)) {}

  void assemble() override {
    r_update_binding.declare_trigger(&update_binding);
    r_request.declare_trigger(&request);

    r_receive_response.declare_trigger(&receive_response);
    r_receive_response.declare_scheduable_action(&send_response);

    r_send_response.declare_trigger(&send_response);
    r_send_response.declare_antidependency(&response);
  }
};

template <class Method>
class ProxyMethodTransactor;

template <class R, class... Args>
class ProxyMethodTransactor<apd::proxy::Method<R(Args...)>>
    : public BaseProxyMethodTransactor<R, Args...> {
 private:
  using Base = BaseProxyMethodTransactor<R, Args...>;

 public:
  ProxyMethodTransactor(const std::string& name,
                        reactor::Environment* env,
                        reactor::Duration request_deadline,
                        reactor::Duration max_network_delay,
                        reactor::Duration max_synchronization_error)
      : Base(name,
             env,
             request_deadline,
             max_network_delay,
             max_synchronization_error) {}
  ProxyMethodTransactor(const std::string& name,
                        reactor::Reactor* container,
                        reactor::Duration request_deadline,
                        reactor::Duration max_network_delay,
                        reactor::Duration max_synchronization_error)
      : Base(name,
             container,
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
class ProxyMethodTransactor<apd::proxy::Method<void(Args...)>>
    : public BaseProxyMethodTransactor<void, Args...> {
 private:
  using Base = BaseProxyMethodTransactor<void, Args...>;

 public:
  ProxyMethodTransactor(const std::string& name,
                        reactor::Environment* env,
                        reactor::Duration request_deadline,
                        reactor::Duration max_network_delay,
                        reactor::Duration max_synchronization_error)
      : Base(name,
             env,
             request_deadline,
             max_network_delay,
             max_synchronization_error) {}
  ProxyMethodTransactor(const std::string& name,
                        reactor::Reactor* container,
                        reactor::Duration request_deadline,
                        reactor::Duration max_network_delay,
                        reactor::Duration max_synchronization_error)
      : Base(name,
             container,
             request_deadline,
             max_network_delay,
             max_synchronization_error) {}

  void on_send_response() override { this->response.set(); }
};

}  // namespace dear
