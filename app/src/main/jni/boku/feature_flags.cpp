#include "feature_flags.h"

namespace {

Features switchFlag;
TeleportRequest teleport;

} // namespace

Features& features() { return switchFlag; }

TeleportRequest& teleportRequest() { return teleport; }
