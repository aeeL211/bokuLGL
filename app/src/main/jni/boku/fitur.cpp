#include "fitur.h"

namespace {

Fitur sakelar;
PermintaanTeleport teleport;

} // namespace

Fitur& fitur() { return sakelar; }

PermintaanTeleport& permintaanTeleport() { return teleport; }
