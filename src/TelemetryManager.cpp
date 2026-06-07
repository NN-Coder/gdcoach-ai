#include "TelemetryManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
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
    attemptSummaries.clear();
    currentGamemode = Gamemode::Cube;
    attemptCount    = 0;
    levelInfo       = LevelInfo{};
    memory.history.clear(); // Reset chat history for new session
    log::debug("[GDCoach] Telemetry session reset.");
}

void TelemetryManager::recordDeath(float percentage, Gamemode gamemode) {
    // 5% Filter: Ignore deaths at <= 5% for Medium Demon and below
    if (percentage <= 5.0f) {
        if (!levelInfo.isDemon || levelInfo.demonDifficulty == 3 || levelInfo.demonDifficulty == 4) {
            log::debug("[GDCoach] Ignored death at {:.1f}% due to 5% filter.", percentage);
            return;
        }
    }

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
    
    // Ensure level directory exists
    if (levelInfo.levelID != 0) {
        auto dirPath = Mod::get()->getSaveDir() / fmt::format("level_{}", levelInfo.levelID);
        std::error_code ec;
        std::filesystem::create_directories(dirPath, ec);
    }
    
    // Note: We don't call loadMemory() here anymore because chat resets every session.
    // Chat logs are stored in the permanent history file but not loaded into active memory.
    saveLevelMetadata();
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
    
    auto savePath = Mod::get()->getSaveDir() / fmt::format("level_{}/memory.json", levelInfo.levelID);
    
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
    
    auto savePath = Mod::get()->getSaveDir() / fmt::format("level_{}/memory.json", levelInfo.levelID);
    
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

void TelemetryManager::startNewAttempt() {
    if (deaths.empty()) return;
    
    // Compile previous attempt into a summary
    auto lastDeath = deaths.back();
    std::string summary = fmt::format("Attempt #{}: Died at {:.1f}% as {}", 
        lastDeath.attemptNumber, lastDeath.percentage, gamemodeToString(lastDeath.gamemode));
    attemptSummaries.push_back(summary);
    
    // Clear raw telemetry for next attempt
    clicks.clear();
    log::debug("[GDCoach] Summarized attempt: {}", summary);
}

void TelemetryManager::endSession() {
    if (levelInfo.levelID == 0 || attemptSummaries.empty()) return;
    
    // Capture level ID and save dir locally for thread-safety/callbacks
    int levelID = levelInfo.levelID;
    auto historyPath = Mod::get()->getSaveDir() / fmt::format("level_{}/history.json", levelID);
    
    // Keep a local copy of attempt summaries and clear the main vector immediately
    std::vector<std::string> sessionAttempts = attemptSummaries;
    attemptSummaries.clear();
    
    // Copy the current session's chat history to include in the summary context
    std::vector<ChatMessage> sessionChat = memory.history;

    // Build JSON payload
    matjson::Value levelObj;
    levelObj.set("name",          matjson::Value(levelInfo.name));
    levelObj.set("creator",       matjson::Value(levelInfo.creator));
    levelObj.set("difficulty",    matjson::Value(levelInfo.difficulty));
    levelObj.set("level_id",      matjson::Value(levelInfo.levelID));
    levelObj.set("is_platformer", matjson::Value(levelInfo.isPlatformer));

    std::vector<matjson::Value> attemptsArr;
    for (const auto& s : sessionAttempts) {
        attemptsArr.push_back(matjson::Value(s));
    }

    std::vector<matjson::Value> historyArr;
    for (const auto& msg : sessionChat) {
        matjson::Value h;
        h.set("role", matjson::Value(msg.role));
        h.set("text", matjson::Value(msg.text));
        historyArr.push_back(std::move(h));
    }

    matjson::Value payload;
    payload.set("level",        levelObj);
    payload.set("attempts",     matjson::Value(attemptsArr));
    payload.set("chat_history", matjson::Value(historyArr));

    std::string jsonBody = payload.dump();
    std::string endpoint = "https://gdcoach-ai-worker.edcube.workers.dev/summarize";

    log::info("[GDCoach] Requesting session summary for level {} asynchronously...", levelID);

    // Prepare local fallback text in case the request fails
    std::string fallbackSummary = "Session Summary (Local Fallback):\n";
    for (const auto& s : sessionAttempts) {
        fallbackSummary += s + "\n";
    }

    // Fire async web request
    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Accept", "text/plain");
    req.bodyString(jsonBody);

    geode::async::spawn(
        req.post(endpoint),
        [historyPath, fallbackSummary, levelID, sessionAttempts, sessionChat](web::WebResponse res) {
            std::string summaryText;
            bool ok = false;
            
            if (res.ok()) {
                summaryText = res.string().unwrapOrDefault();
                if (!summaryText.empty()) {
                    ok = true;
                }
            }

            if (!ok) {
                log::warn("[GDCoach] Failed to generate AI session summary, using fallback. Error code: {}", res.code());
                summaryText = fallbackSummary;
            } else {
                log::info("[GDCoach] Successfully received AI session summary for level {}.", levelID);
            }

            // Load existing history and append
            std::vector<matjson::Value> historyEntries;
            std::ifstream inFile(historyPath);
            if (inFile.is_open()) {
                std::stringstream buffer;
                buffer << inFile.rdbuf();
                inFile.close();
                auto parseRes = matjson::parse(buffer.str());
                if (parseRes && parseRes.unwrap().isArray()) {
                    historyEntries = parseRes.unwrap().asArray().unwrap();
                }
            }

            matjson::Value newEntry;
            newEntry.set("timestamp", std::time(nullptr));
            newEntry.set("summary", summaryText);

            std::vector<matjson::Value> cbAttemptsArr;
            for (const auto& s : sessionAttempts) {
                cbAttemptsArr.push_back(matjson::Value(s));
            }
            newEntry.set("attempts", matjson::Value(cbAttemptsArr));

            std::vector<matjson::Value> cbHistoryArr;
            for (const auto& msg : sessionChat) {
                matjson::Value h;
                h.set("role", matjson::Value(msg.role));
                h.set("text", matjson::Value(msg.text));
                cbHistoryArr.push_back(std::move(h));
            }
            newEntry.set("chat", matjson::Value(cbHistoryArr));

            historyEntries.push_back(newEntry);

            // Save updated history
            std::ofstream outFile(historyPath);
            if (outFile.is_open()) {
                outFile << matjson::Value(historyEntries).dump();
                outFile.close();
                log::info("[GDCoach] Saved session summary to level {} history.", levelID);
            } else {
                log::error("[GDCoach] Failed to write session summary to file: {}", historyPath.string());
            }
        }
    );
}

void TelemetryManager::saveLevelMetadata() {
    if (levelInfo.levelID == 0) return;
    
    auto metaPath = Mod::get()->getSaveDir() / fmt::format("level_{}/metadata.json", levelInfo.levelID);
    
    // Only save if it doesn't exist
    if (std::filesystem::exists(metaPath)) return;
    
    matjson::Value meta;
    meta.set("name", levelInfo.name);
    meta.set("creator", levelInfo.creator);
    meta.set("difficulty", levelInfo.difficulty);
    meta.set("demonDifficulty", levelInfo.demonDifficulty);
    meta.set("isDemon", levelInfo.isDemon);
    meta.set("levelID", levelInfo.levelID);
    
    std::ofstream outFile(metaPath);
    if (outFile.is_open()) {
        outFile << meta.dump();
        outFile.close();
        log::info("[GDCoach] Saved static level metadata for level {}.", levelInfo.levelID);
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
    auto path = Mod::get()->getSaveDir() / fmt::format("level_{}", levelID);
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    // Also clear in-memory history if it matches the current level
    if (memory.levelID == levelID) {
        memory.history.clear();
    }
    log::info("[GDCoach] Cleared all data for level {}.", levelID);
}

void TelemetryManager::clearAllMemory() {
    auto saveDir = Mod::get()->getSaveDir();
    std::error_code ec;
    for (auto const& entry : std::filesystem::directory_iterator(saveDir, ec)) {
        auto name = entry.path().filename().string();
        if (name.rfind("level_", 0) == 0 && entry.is_directory()) {
            std::filesystem::remove_all(entry.path(), ec);
        }
        // Also remove old format if it exists
        if (name.rfind("memory_", 0) == 0 && name.size() > 12) {
            std::filesystem::remove(entry.path(), ec);
        }
    }
    memory.history.clear();
    log::info("[GDCoach] All level data cleared.");
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
        if (name.rfind("level_", 0) == 0 && entry.is_directory()) {
            auto idStr = name.substr(6);
            try {
                ids.push_back(std::stoi(idStr));
            } catch (...) {}
        } else if (name.rfind("memory_", 0) == 0 && name.size() > 12) {
            // Also include legacy format
            auto idStr = name.substr(7, name.size() - 7 - 5);
            try {
                ids.push_back(std::stoi(idStr));
            } catch (...) {}
        }
    }
    return ids;
}
