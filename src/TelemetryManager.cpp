#include "TelemetryManager.hpp"
#include <Geode/Geode.hpp>
#include <matjson.hpp>
#include <fstream>
#include <filesystem>

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
    clicks.clear();
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
    loadMemory();
}

void TelemetryManager::recordClick(float x, float y, float time, bool isDown) {
    clicks.push_back({x, y, time, isDown});
    // Limit to last 50 clicks to save memory and payload size
    if (clicks.size() > 50) {
        clicks.erase(clicks.begin());
    }
}

void TelemetryManager::addChatMessage(const std::string& role, const std::string& text) {
    memory.history.push_back({role, text});
    saveMemory();
}

void TelemetryManager::saveMemory() {
    if (levelInfo.levelID == 0) return;
    
    auto savePath = Mod::get()->getSaveDir() / fmt::format("memory_{}.json", levelInfo.levelID);
    
    std::vector<matjson::Value> arr;
    for (const auto& msg : memory.history) {
        matjson::Value obj;
        obj.set("role", msg.role);
        obj.set("text", msg.text);
        arr.push_back(obj);
    }
    
    matjson::Value val(arr);
    std::string data = val.dump();
    std::ofstream file(savePath);
    if (file.is_open()) {
        file << data;
        file.close();
    }
}

void TelemetryManager::loadMemory() {
    memory.levelID = levelInfo.levelID;
    memory.history.clear();
    
    if (levelInfo.levelID == 0) return;
    
    auto savePath = Mod::get()->getSaveDir() / fmt::format("memory_{}.json", levelInfo.levelID);
    
    std::ifstream file(savePath);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        auto res = matjson::parse(buffer.str());
        if (res) {
            auto val = res.unwrap();
            if (val.isArray()) {
                auto arrRes = val.asArray();
                if (arrRes) {
                    for (const auto& item : arrRes.unwrap()) {
                        if (item.contains("role") && item.contains("text")) {
                            memory.history.push_back({
                                item["role"].asString().unwrapOr("user"),
                                item["text"].asString().unwrapOr("")
                            });
                        }
                    }
                }
            }
        }
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// Data deletion helpers
// ─────────────────────────────────────────────────────────────────────────────

void TelemetryManager::clearMemoryForLevel(int levelID) {
    if (levelID == 0) return;
    auto path = Mod::get()->getSaveDir() / fmt::format("memory_{}.json", levelID);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    // Also clear in-memory history if it matches the current level
    if (memory.levelID == levelID) {
        memory.history.clear();
    }
    log::info("[GDCoach] Cleared chat history for level {}.", levelID);
}

void TelemetryManager::clearAllMemory() {
    auto saveDir = Mod::get()->getSaveDir();
    std::error_code ec;
    for (auto const& entry : std::filesystem::directory_iterator(saveDir, ec)) {
        auto name = entry.path().filename().string();
        if (name.rfind("memory_", 0) == 0 && name.size() > 12) {
            std::filesystem::remove(entry.path(), ec);
        }
    }
    memory.history.clear();
    log::info("[GDCoach] All chat histories cleared.");
}

void TelemetryManager::clearCurrentSession() {
    deaths.clear();
    clicks.clear();
    attemptCount = 0;
    log::info("[GDCoach] Current session telemetry (deaths & clicks) cleared.");
}

std::vector<int> TelemetryManager::getSavedLevelIDs() {
    std::vector<int> ids;
    auto saveDir = Mod::get()->getSaveDir();
    std::error_code ec;
    for (auto const& entry : std::filesystem::directory_iterator(saveDir, ec)) {
        auto name = entry.path().filename().string();
        // Files are named memory_<levelID>.json
        if (name.rfind("memory_", 0) == 0 && name.size() > 12) {
            // Strip prefix "memory_" (7 chars) and suffix ".json" (5 chars)
            auto idStr = name.substr(7, name.size() - 7 - 5);
            try {
                ids.push_back(std::stoi(idStr));
            } catch (...) {}
        }
    }
    return ids;
}
