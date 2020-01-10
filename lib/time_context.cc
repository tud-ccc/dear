/*
 * Copyright (C) 2020 TU Dresden
 * All rights reserved.
 *
 * Authors:
 *   Christian Menard
 */

#include "dear/time_context.hh"

namespace dear {

thread_local reactor::TimePoint TimeContext::timestamp;
thread_local bool TimeContext::valid = false;

} // namespace dear
