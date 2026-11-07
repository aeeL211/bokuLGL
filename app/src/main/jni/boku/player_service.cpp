#include "player_service.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "clock.h"

namespace {

constexpr double kDelayAction = 0.25;
constexpr double kDelayList = 0.4;

// Salinan tetap hidup dua detik sesudah pembacaan terakhir. Menu membaca sekali
// per detik, jadi menutup menu menghentikan pembaruan tanpa menghapus data yang
// masih ditampilkan.
constexpr double kPeriodList = 2.0;
constexpr double kPeriodPosition = 2.0;

std::atomic<bool> serviceReady{false};

std::mutex listKey;
player::Summary playerList[player::kMaxPlayer] = {};
int countPlayer = 0;
double nextList = 0.0;
std::atomic<double> listRequested{0.0};

std::mutex actionKey;
player::Action queue[player::kLimitAction] = {};
int countQueue = 0;
double nextAction = 0.0;

struct PositionSummary {
    bool has;
    bool hasTile;
    float tileX;
    float tileY;
    float worldX;
    float worldY;
};

std::mutex keyPosition;
PositionSummary positionSummary = {};
std::atomic<double> positionRequested{0.0};

bool enqueue(player::Action::Kind jenis, const char* key, const char* name) {
    bool needKey = jenis == player::Action::Kind::Kick ||
                      jenis == player::Action::Kind::KickPermanent ||
                      jenis == player::Action::Kind::Copy;
    if (!serviceReady.load() || (needKey && (!key || !*key)) ||
        (key && std::strlen(key) >= player::kCharacterKey))
        return false;

    std::lock_guard<std::mutex> hold(actionKey);
    bool diterima = countQueue < player::kLimitAction;
    if (diterima) {
        player::Action& action = queue[countQueue++];
        action.jenis = jenis;
        std::snprintf(action.key, sizeof(action.key), "%s", key ? key : "");
        std::snprintf(action.name, sizeof(action.name), "%s", name && *name ? name : key);
    }
    return diterima;
}

} // namespace

namespace player {

void setReady(bool ready) { serviceReady.store(ready); }

bool serveList() {
    double now = nowSeconds();
    if (now < nextList || now > listRequested.load())
        return false;
    nextList = now + kDelayList;
    return true;
}

void storeList(const Summary* data, int count) {
    if (!data || count < 0 || count > kMaxPlayer)
        return;
    std::lock_guard<std::mutex> hold(listKey);
    std::memcpy(playerList, data, sizeof(Summary) * static_cast<std::size_t>(count));
    countPlayer = count;
}

void copyList(char* keluaran, std::size_t size) {
    if (!keluaran || size == 0)
        return;
    keluaran[0] = '\0';
    listRequested.store(nowSeconds() + kPeriodList);

    Summary salinan[kMaxPlayer] = {};
    int count = 0;
    {
        std::lock_guard<std::mutex> hold(listKey);
        count = countPlayer;
        std::memcpy(salinan, playerList, sizeof(Summary) * static_cast<std::size_t>(count));
    }

    std::size_t position = 0;
    for (int i = 0; i < count; i++) {
        int ditulis =
            std::snprintf(keluaran + position, size - position, "%s%s\t%s\t%s", i ? "\n" : "",
                          salinan[i].name, salinan[i].key, salinan[i].role);
        if (ditulis < 0 || static_cast<std::size_t>(ditulis) >= size - position)
            break;
        position += static_cast<std::size_t>(ditulis);
    }
}

bool enqueueKick(const char* key, const char* name, bool permanent) {
    return enqueue(permanent ? Action::Kind::KickPermanent : Action::Kind::Kick, key, name);
}

bool enqueueKickHost(const char* name) {
    return enqueue(Action::Kind::KickHost, nullptr, name);
}

bool enqueueKickAll() { return enqueue(Action::Kind::KickAll, nullptr, "semua tamu"); }

bool enqueueCopy(const char* key, const char* name) {
    return enqueue(Action::Kind::Copy, key, name);
}

bool takeAction(Action& exit) {
    double now = nowSeconds();
    if (now < nextAction)
        return false;

    bool has = false;
    {
        std::lock_guard<std::mutex> hold(actionKey);
        if (countQueue > 0) {
            exit = queue[0];
            for (int i = 1; i < countQueue; i++)
                queue[i - 1] = queue[i];
            countQueue--;
            has = true;
        }
    }
    if (!has)
        return false;

    nextAction = now + kDelayAction;
    return true;
}

bool servePosition() { return nowSeconds() < positionRequested.load(); }

void storePosition(bool hasTile, float tileX, float tileY, float worldX, float worldY) {
    std::lock_guard<std::mutex> hold(keyPosition);
    positionSummary.has = true;
    positionSummary.hasTile = hasTile;
    positionSummary.tileX = hasTile ? tileX : 0.0f;
    positionSummary.tileY = hasTile ? tileY : 0.0f;
    positionSummary.worldX = worldX;
    positionSummary.worldY = worldY;
}

void copyPosition(char* keluaran, std::size_t size) {
    if (!keluaran || size == 0)
        return;
    keluaran[0] = '\0';
    positionRequested.store(nowSeconds() + kPeriodPosition);

    PositionSummary salinan = {};
    {
        std::lock_guard<std::mutex> hold(keyPosition);
        salinan = positionSummary;
    }

    if (!salinan.has)
        return;
    if (salinan.hasTile) {
        std::snprintf(keluaran, size, "%.2f\t%.2f\t%.1f\t%.1f", salinan.tileX, salinan.tileY,
                      salinan.worldX, salinan.worldY);
    } else {
        std::snprintf(keluaran, size, "\t\t%.1f\t%.1f", salinan.worldX, salinan.worldY);
    }
}

} // namespace pemain
