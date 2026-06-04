#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// DeleteDataLayer
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Popup that lets the user selectively delete stored GDCoach data:
 *
 *   • All chat histories (all memory_*.json files on disk)
 *   • Current level chat history only
 *   • Current session telemetry (deaths and clicks in-memory)
 *
 * Presents toggle buttons for each category and a "Delete Selected" action
 * button that performs the deletions and closes the popup.
 */
class DeleteDataLayer : public geode::Popup {
public:
    static DeleteDataLayer* create();
    static void show();

protected:
    bool init() override;

private:
    // ── Toggle state ─────────────────────────────────────────────────────────
    bool m_deleteAllHistories   = false;
    bool m_deleteCurrentHistory = false;
    bool m_deleteSessionData    = false;

    // ── Toggle buttons ───────────────────────────────────────────────────────
    CCMenuItemToggler* m_toggleAllHistories   = nullptr;
    CCMenuItemToggler* m_toggleCurrentHistory = nullptr;
    CCMenuItemToggler* m_toggleSessionData    = nullptr;

    // ── Callbacks ────────────────────────────────────────────────────────────
    void onToggleAllHistories(CCObject*);
    void onToggleCurrentHistory(CCObject*);
    void onToggleSessionData(CCObject*);
    void onDeleteSelected(CCObject*);
    void onCancel(CCObject*);

    // ── Helpers ───────────────────────────────────────────────────────────────
    CCMenuItemToggler* makeToggle(const char* label, SEL_MenuHandler callback, float y);
    void updateConfirmButton();

    CCMenuItemSpriteExtra* m_confirmBtn = nullptr;
};
