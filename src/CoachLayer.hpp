#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// CoachLayer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Custom popup that orchestrates the full AI coaching flow:
 *
 *   1. Displays a loading indicator immediately on open.
 *   2. Serialises the current TelemetryManager state into JSON.
 *   3. POSTs the payload to the configured inference server asynchronously.
 *   4. Replaces the loading indicator with wrapped coaching text on success,
 *      or a user-friendly error message on failure.
 *
 * The popup is 380 × 280 points — wide enough for comfortable text wrapping
 * and tall enough to display all three coaching paragraphs without scroll.
 *
 * @note In Geode 5.x, geode::Popup is NOT a class template — it is simply
 *       `class Popup : public FLAlertLayer`. State is stored as plain members.
 */
class CoachLayer : public geode::Popup {
public:
    /// Factory — creates, initialises, and shows the popup in one call.
    static CoachLayer* create();

    /// Convenience wrapper: creates and immediately calls FLAlertLayer::show().
    static void show();

protected:
    bool init() override;

private:
    // ── Plain member state ───────────────────────────────────────────────────
    // The web task lifetime is managed in the .cpp via TaskHolder stored there.
    CCLabelBMFont*  m_statusLabel   = nullptr;
    CCNode*         m_spinnerNode   = nullptr;
    CCScale9Sprite* m_textBg        = nullptr;
    CCLabelBMFont*  m_responseLabel = nullptr;

    // ── Helpers ───────────────────────────────────────────────────────────────

    /// Kick off the async web request to the inference server.
    void fetchCoachingAdvice();

    /// Called on the main thread when a successful response arrives.
    void displayResponse(const std::string& text);

    /// Called on the main thread when the request fails.
    void displayError(const std::string& message);

    /// Animates the loading spinner (CCRepeatForever rotate action).
    void startSpinner();

    /// Stops and hides the spinner.
    void stopSpinner();
};
