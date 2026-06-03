#include "TelemetryManager.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

TelemetryManager& TelemetryManager::get() {
    static TelemetryManager instance;
    return instance;
}

// ─────────────────────────────────────────────────────────────────────────────
// Mutation
// ─────────────────────────────────────────────────────────────────────────────

void TelemetryManager::reset() {
    deaths.clear();
    currentGamemode = Gamemode::Cube;
    attemptCount    = 0;
    levelInfo       = LevelInfo{};
    log::debug("[GDCoach] Telemetry session reset.");
}

void TelemetryManager::recordDeath(float percentage, Gamemode gamemode) {
    attemptCount++;
    DeathRecord record{
        .percentage    = percentage,
        .gamemode      = gamemode,
        .attemptNumber = attemptCount,
    };
    deaths.push_back(record);

    log::debug(
        "[GDCoach] Death recorded — attempt {}: {:.1f}% as {}",
        attemptCount,
        percentage,
        gamemodeToString(gamemode)
    );
}

void TelemetryManager::setLevelInfo(const LevelInfo& info) {
    levelInfo = info;
    log::info(
        "[GDCoach] Level loaded — '{}' by {} (ID: {}, Difficulty: {})",
        info.name,
        info.creator,
        info.levelID,
        info.difficulty
    );
}

void TelemetryManager::setCurrentGamemode(Gamemode gm) {
    // Only log transitions to avoid flooding the debug output every frame.
    if (gm != currentGamemode) {
        log::debug(
            "[GDCoach] Gamemode transition: {} → {}",
            gamemodeToString(currentGamemode),
            gamemodeToString(gm)
        );
        currentGamemode = gm;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Query
// ─────────────────────────────────────────────────────────────────────────────

bool TelemetryManager::hasData() const {
    return !deaths.empty();
}
