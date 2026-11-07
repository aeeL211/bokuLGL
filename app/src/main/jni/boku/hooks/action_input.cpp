#include "hooks.h"

#include "il2cpp_api.h"
#include "log.h"

namespace {

Field fieldInput = {};
Field fieldManagerInput = {};
Field fieldAction = {};
Field fieldHold = {};

} // namespace

// Tombol interaksi sedang ditahan selama objek input-nya menyimpan aksi.
bool isActionButtonPressed(void* manager) {
    void* input = readPtr(manager, fieldInput);
    void* managerInput = readPtr(input, fieldManagerInput);
    void* action = readPtr(managerInput, fieldAction);
    return action && readBool(action, fieldHold);
}

// Dipakai bersama oleh setiap fitur yang mengulang aksinya selama tombol ditahan.
void findActionFields() {
    void* managerClass = classOf("Player__Manager");
    bool ok = findField(managerClass, "input", fieldInput) &&
              findField(fieldClass(fieldInput), "input", fieldManagerInput) &&
              findField(fieldClass(fieldManagerInput), "action", fieldAction) &&
              findField(fieldClass(fieldAction), "keep", fieldHold);
    if (ok)
        LOGI("Field input aksi OK");
    else
        LOGE("Field input aksi tidak ditemukan; ulangan tombol tahan tetap mati");
}
