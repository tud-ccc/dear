/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#pragma once

#include <cassert>
#include <utility>

#include <reactor-cpp/logical_time.hh>

#include "dear/apd_dependencies.hh"

namespace dear {

class TimeContext {
private:
  static thread_local bool valid;
  static thread_local reactor::TimePoint timestamp;

public:
  static void provide_timestamp(const reactor::TimePoint &t) {
    assert(!valid);
    valid = true;
    timestamp = t;
  }

  static apd::Result<reactor::TimePoint, bool> retrieve_timestamp() {
    using Result = apd::Result<reactor::TimePoint, bool>;
    if (valid) {
      return Result::FromValue(timestamp);
    } else {
      return Result::FromError(false);
    }
  }

  static void invalidate_timestamp() {
    assert(valid);
    valid = false;
  }
};

} // namespace dear
