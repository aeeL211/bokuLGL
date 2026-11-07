#pragma once

#include <ctime>

// The monotonic clock drives the queue rhythm, the player list lifetime, and
// position updates. Wall-clock changes from the time zone or a user setting
// therefore never change the timing of a feature.
inline double nowSeconds() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}
