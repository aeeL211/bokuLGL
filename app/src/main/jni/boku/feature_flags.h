#pragma once

#include <atomic>
#include <cstdint>

// The menu only writes values here. Every hook reads the switch on each call,
// so turning a feature on or off never installs or removes a hook while the
// game runs. Writes come from the UI thread and reads from the game thread, so
// every switch is atomic.
struct Features {
    std::atomic<bool> iap{true};
    std::atomic<bool> slot{true};
    std::atomic<bool> inputText{true};
    // Skin shift limit in game units: 5 means -5..+5 as before, 25 means
    // -25..+25. The menu sends 5..100.
    std::atomic<int32_t> skinRange{25};
    std::atomic<bool> rudeWord{true};
    std::atomic<bool> saveWorld{true};
    std::atomic<bool> roomSize{true};
    // Build and destroy permission in other rooms is bypassed by forcing
    // Enforce on tile packets and Across on object packets.
    std::atomic<bool> permissionRoomOther{true};
    std::atomic<bool> roomPassword{true};
    std::atomic<bool> privacy{true};
    std::atomic<bool> privateChat{true};
    std::atomic<bool> shootAutomatic{true};
    std::atomic<bool> busterFull{true};
    std::atomic<bool> feces{true};
    std::atomic<bool> food{true};
    // Camera shake and tile debris are separate because shake only disturbs
    // the view, while debris also affects the rendering load on screen.
    std::atomic<bool> shake{true};
    std::atomic<bool> debris{true};
    // Off by default: every guest that joins writes a model and portrait file,
    // so the user is warned about the extra work before turning it on.
    std::atomic<bool> autoDump{false};
    // World override is local to this device: the three selections call the
    // game's own local setter instead of the host-only network path. A zero
    // selection means that part is left alone.
    std::atomic<bool> worldLocal{false};
    std::atomic<int32_t> worldBackground{0};
    std::atomic<int32_t> worldPeriod{0};
    std::atomic<int32_t> worldWeather{0};
    std::atomic<bool> knockback{true};
    std::atomic<bool> noClip{false};
    // Slider values are sent by the menu as integers: speed 5..50 means
    // 0.5x..5.0x and gravity 0..30 means 0.0x..3.0x. A value of 10 is 1.0x.
    std::atomic<int32_t> speedPlayer{10};
    std::atomic<int32_t> gravityPlayer{10};
};

// Menu buttons run on the UI thread, while position is read by the player
// update hook on the game thread. A request is only parked, then used once.
struct TeleportRequest {
    std::atomic<int32_t> x{0};
    std::atomic<int32_t> y{0};
    std::atomic<bool> waiting{false};
};

Features& features();
TeleportRequest& teleportRequest();
