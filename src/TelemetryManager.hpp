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

/// A single player input (click or jump) recorded.
struct ClickRecord {
    float x;
    float y;
    float time;
    bool  isDown;
};

/// A single message in the chat history.
struct ChatMessage {
    std::string role; // "user" or "model"
    std::string text;
};

/// Conversation memory for a specific level.
struct ConversationMemory {
    int levelID;
    std::vector<ChatMessage> history;
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

    /// Record a player click input with position and time.
    void recordClick(float x, float y, float time, bool isDown);

    /// Store metadata for the level that just loaded.
    void setLevelInfo(const LevelInfo& info);

    /// Update the active gamemode (called every frame from PlayerObject::update hook).
    void setCurrentGamemode(Gamemode gm);

    /// Save the current conversation history to disk.
    void saveMemory();

    /// Load conversation history from disk for the current level.
    void loadMemory();

    /// Add a message to the conversation history.
    void addChatMessage(const std::string& role, const std::string& text);

    /// Clear the chat history for a specific level (deletes its memory file).
    void clearMemoryForLevel(int levelID);

    /// Clear ALL saved chat histories for every level (deletes all memory_*.json files).
    void clearAllMemory();

    /// Clear the current in-session telemetry (deaths and clicks) without resetting level info or memory.
    void clearCurrentSession();

    /// Return the list of level IDs that have saved memory files on disk.
    std::vector<int> getSavedLevelIDs();

    // ── Read-only state ───────────────────────────────────────────────────────
    LevelInfo               levelInfo;
    std::vector<DeathRecord> deaths;
    std::vector<ClickRecord> clicks;
    ConversationMemory      memory;
    Gamemode                currentGamemode = Gamemode::Cube;
    int                     attemptCount    = 0;

    /// @returns true if there is at least one death recorded this session.
    bool hasData() const;

private:
    TelemetryManager() = default;
    TelemetryManager(const TelemetryManager&) = delete;
    TelemetryManager& operator=(const TelemetryManager&) = delete;
};
