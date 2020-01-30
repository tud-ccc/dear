/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#pragma once

#include <ara/com/internal/proxy/event.h>
#include <ara/com/internal/proxy/method.h>
#include <ara/com/internal/skeleton/event_dispatcher.h>
#include <ara/com/internal/vsomeip/vsomeip_marshalling.h>
#include <ara/core/promise.h>
#include <ara/log/logging.h>

namespace dear {
namespace apd {

using ara::com::internal::InstanceId;
using ara::com::internal::vsomeip::common::Deserializer;
using ara::com::internal::vsomeip::common::Unmarshaller;
using ara::core::Future;
using ara::core::Promise;
using ara::core::Result;

using ara::log::CreateLogger;
using ara::log::Logger;

namespace skeleton {
using ara::com::internal::skeleton::EventDispatcher;
}

namespace proxy {
using ara::com::internal::proxy::Event;
using ara::com::internal::proxy::Method;
}  // namespace proxy

}  // namespace apd
}  // namespace dear
