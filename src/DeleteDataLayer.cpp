#include "DeleteDataLayer.hpp"
#include "TelemetryManager.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr float DEL_WIDTH  = 340.f;
static constexpr float DEL_HEIGHT = 240.f;

// ─────────────────────────────────────────────────────────────────────────────
// Factory & show
// ─────────────────────────────────────────────────────────────────────────────

DeleteDataLayer* DeleteDataLayer::create() {
    auto* ret = new DeleteDataLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void DeleteDataLayer::show() {
    if (auto* layer = DeleteDataLayer::create()) {
        layer->FLAlertLayer::show();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a row with a CCMenuItemToggler + label
// ─────────────────────────────────────────────────────────────────────────────

CCMenuItemToggler* DeleteDataLayer::makeToggle(
    const char* label, SEL_MenuHandler callback, float y)
{
    // OFF sprite (unchecked box)
    auto* offSprite = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    // ON sprite (checked box)
    auto* onSprite  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");

    auto* toggle = CCMenuItemToggler::create(offSprite, onSprite, this, callback);
    toggle->setPosition({ 30.f, y });
    toggle->setScale(0.8f);

    // Side label
    auto* lbl = CCLabelBMFont::create(label, "bigFont.fnt");
    lbl->setScale(0.45f);
    lbl->setAnchorPoint({ 0.f, 0.5f });
    lbl->setPosition({ 50.f, y });
    m_mainLayer->addChild(lbl);

    return toggle;
}

// ─────────────────────────────────────────────────────────────────────────────
// init
// ─────────────────────────────────────────────────────────────────────────────

bool DeleteDataLayer::init() {
    if (!Popup::init(DEL_WIDTH, DEL_HEIGHT)) return false;

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = CCLabelBMFont::create("Delete Data", "goldFont.fnt");
    title->setScale(0.8f);
    m_mainLayer->addChildAtPosition(title, Anchor::Top, { 0.f, -22.f });

    // ── Divider ───────────────────────────────────────────────────────────────
    auto* divider = CCLayerColor::create({ 255, 255, 255, 40 }, DEL_WIDTH - 40.f, 1.f);
    divider->setPosition({ 20.f, DEL_HEIGHT - 40.f });
    m_mainLayer->addChild(divider);

    // ── Description ──────────────────────────────────────────────────────────
    auto* desc = CCLabelBMFont::create("Select the data types you want to remove:", "chatFont.fnt");
    desc->setScale(0.5f);
    desc->setColor({ 200, 200, 200 });
    desc->setAnchorPoint({ 0.f, 0.5f });
    desc->setPosition({ 20.f, DEL_HEIGHT - 55.f });
    m_mainLayer->addChild(desc);

    // ── Toggle menu ───────────────────────────────────────────────────────────
    auto* toggleMenu = CCMenu::create();
    toggleMenu->setPosition({ 0.f, 0.f });
    toggleMenu->setContentSize({ DEL_WIDTH, DEL_HEIGHT });
    m_mainLayer->addChild(toggleMenu);

    float rowY1 = DEL_HEIGHT - 83.f;
    float rowY2 = rowY1 - 32.f;
    float rowY3 = rowY2 - 32.f;

    m_toggleAllHistories = makeToggle(
        "All chat histories (every level)",
        menu_selector(DeleteDataLayer::onToggleAllHistories),
        rowY1
    );
    toggleMenu->addChild(m_toggleAllHistories);

    auto& tm = TelemetryManager::get();
    bool hasCurrentLevel = (tm.levelInfo.levelID != 0);

    // Row 2: current level history (greyed out if no level loaded)
    std::string currentLevelLabel = "Current level chat history";
    if (tm.levelInfo.levelID != 0) {
        currentLevelLabel += " (ID: " + std::to_string(tm.levelInfo.levelID) + ")";
    } else {
        currentLevelLabel += " (no level loaded)";
    }
    m_toggleCurrentHistory = makeToggle(
        currentLevelLabel.c_str(),
        menu_selector(DeleteDataLayer::onToggleCurrentHistory),
        rowY2
    );
    if (!hasCurrentLevel) {
        m_toggleCurrentHistory->setEnabled(false);
        m_toggleCurrentHistory->setOpacity(80);
    }
    toggleMenu->addChild(m_toggleCurrentHistory);

    // Row 3: current session deaths & clicks
    std::string sessionLabel = "Current session data (deaths & clicks)";
    if (tm.deaths.empty() && tm.clicks.empty()) {
        sessionLabel += " (none recorded)";
    } else {
        sessionLabel += " (" + std::to_string(tm.deaths.size()) + " deaths)";
    }
    m_toggleSessionData = makeToggle(
        sessionLabel.c_str(),
        menu_selector(DeleteDataLayer::onToggleSessionData),
        rowY3
    );
    if (tm.deaths.empty() && tm.clicks.empty()) {
        m_toggleSessionData->setEnabled(false);
        m_toggleSessionData->setOpacity(80);
    }
    toggleMenu->addChild(m_toggleSessionData);

    // ── Action buttons ───────────────────────────────────────────────────────
    auto* btnMenu = CCMenu::create();
    btnMenu->setPosition({ DEL_WIDTH / 2.f, 28.f });
    btnMenu->setLayout(RowLayout::create()->setGap(12.f));
    m_mainLayer->addChild(btnMenu);

    // Cancel button
    auto* cancelSprite = ButtonSprite::create("Cancel", "bigFont.fnt", "GJ_button_06.png", 0.6f);
    auto* cancelBtn = CCMenuItemSpriteExtra::create(
        cancelSprite, this, menu_selector(DeleteDataLayer::onCancel));
    cancelBtn->setScale(0.85f);
    btnMenu->addChild(cancelBtn);

    // Delete (confirm) button — starts disabled
    auto* confirmSprite = ButtonSprite::create("Delete", "bigFont.fnt", "GJ_button_06.png", 0.6f);
    confirmSprite->setColor({ 255, 80, 80 });
    m_confirmBtn = CCMenuItemSpriteExtra::create(
        confirmSprite, this, menu_selector(DeleteDataLayer::onDeleteSelected));
    m_confirmBtn->setScale(0.85f);
    m_confirmBtn->setEnabled(false);
    m_confirmBtn->setOpacity(100);
    btnMenu->addChild(m_confirmBtn);

    btnMenu->updateLayout();

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Toggle callbacks
// ─────────────────────────────────────────────────────────────────────────────

void DeleteDataLayer::onToggleAllHistories(CCObject*) {
    m_deleteAllHistories = !m_deleteAllHistories;
    updateConfirmButton();
}

void DeleteDataLayer::onToggleCurrentHistory(CCObject*) {
    m_deleteCurrentHistory = !m_deleteCurrentHistory;
    updateConfirmButton();
}

void DeleteDataLayer::onToggleSessionData(CCObject*) {
    m_deleteSessionData = !m_deleteSessionData;
    updateConfirmButton();
}

void DeleteDataLayer::updateConfirmButton() {
    bool anySelected = m_deleteAllHistories || m_deleteCurrentHistory || m_deleteSessionData;
    if (m_confirmBtn) {
        m_confirmBtn->setEnabled(anySelected);
        m_confirmBtn->setOpacity(anySelected ? 255 : 100);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Action callbacks
// ─────────────────────────────────────────────────────────────────────────────

void DeleteDataLayer::onDeleteSelected(CCObject*) {
    auto& tm = TelemetryManager::get();

    if (m_deleteAllHistories) {
        tm.clearAllMemory();
        log::info("[GDCoach] Deleted all chat histories.");
    } else if (m_deleteCurrentHistory) {
        // Only clear current level if 'all' was not already chosen
        tm.clearMemoryForLevel(tm.levelInfo.levelID);
        log::info("[GDCoach] Deleted current level chat history.");
    }

    if (m_deleteSessionData) {
        tm.clearCurrentSession();
        log::info("[GDCoach] Cleared current session telemetry.");
    }

    // Show a brief confirmation, then close
    FLAlertLayer::create(
        "Data Deleted",
        "Selected data has been successfully removed.",
        "OK"
    )->show();

    onClose(nullptr);
}

void DeleteDataLayer::onCancel(CCObject*) {
    onClose(nullptr);
}
