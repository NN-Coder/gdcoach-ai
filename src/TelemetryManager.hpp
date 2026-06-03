#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <vector>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Gamemode enum — mirrors every vehicle in Geometry Dash 2.2
// ─────────────────────────────────────────────────────────────────────────────
enum class Gamemode : uint8_t {
    Cube        = 0,
    Ship        = 1,
    Ball        = 2,
    UFO         = 3,  // m_isBird in the GD bindings
    Wave        = 4,  // m_isDart in the GD bindings
    Robot       = 5,
    Spider      = 6,
    Swing       = 7,
};

/// Convert a Gamemode to a display-friendly string for the server payload.
inline std::string gamemodeToString(Gamemode gm) {
    switch (gm) {
        case Gamemode::Cube:    return "Cube";
        case Gamemode::Ship:    return "Ship";
        case Gamemode::Ball:    return "Ball";
        case Gamemode::UFO:     return "UFO";
        case Gamemode::Wave:    return "Wave";
        case Gamemode::Robot:   return "Robot";
        case Gamemode::Spider:  return "Spider";
        case Gamemode::Swing:   return "Swing";
        default:                return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Data structs
// ─────────────────────────────────────────────────────────────────────────────

/// A single death event recorded during a level session.
struct DeathRecord {
    float    percentage;    ///< 0.0 – 100.0
    Gamemode gamemode;      ///< Vehicle active at time of death
    int      attemptNumber; ///< Which attempt this death occurred on
};

/// Metadata about the level currently being played.
struct LevelInfo {
    std::string name;
    std::string creator;
    int         difficulty; ///< GJ difficulty int (0=N/A, 1=Auto, 2=Easy … 10=Demon)
    int         levelID;
    bool        isPlatformer;
};

// ─────────────────────────────────────────────────────────────────────────────
// TelemetryManager — singleton that accumulates session data
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Thread-locality note: all methods are expected to be called from the
 *        Cocos2d main thread. No synchronisation primitives are therefore
 *        required; Geode's hook system guarantees this.
 */
class TelemetryManager {
public:
    // ── Singleton access ──────────────────────────────────────────────────────
    static TelemetryManager& get();

    // ── Mutation API ─────────────────────────────────────────────────────────

    /// Clears all accumulated data. Call when a new level session begins.
    void reset();

    /// Record a death at the given level-percentage in the given gamemode.
    void recordDeath(float percentage, Gamemode gamemode);

    /// Store metadata for the level that just loaded.
    void setLevelInfo(const LevelInfo& info);

    /// Update the active gamemode (called every frame from PlayerObject::update hook).
    void setCurrentGamemode(Gamemode gm);

    // ── Read-only state ───────────────────────────────────────────────────────
    LevelInfo               levelInfo;
    std::vector<DeathRecord> deaths;
    Gamemode                currentGamemode = Gamemode::Cube;
    int                     attemptCount    = 0;

    /// @returns true if there is at least one death recorded this session.
    bool hasData() const;

private:
    TelemetryManager() = default;
    TelemetryManager(const TelemetryManager&) = delete;
    TelemetryManager& operator=(const TelemetryManager&) = delete;
};
