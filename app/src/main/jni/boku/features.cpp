#include "features.h"

namespace {

Features sakelar;
TeleportRequest teleport;

} // namespace

Features& features() { return sakelar; }

TeleportRequest& teleportRequest() { return teleport; }
