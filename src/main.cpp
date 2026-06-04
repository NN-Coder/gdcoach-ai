#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>

#include "TelemetryManager.hpp"
#include "CoachLayer.hpp"
#include "DeleteDataLayer.hpp"

using namespace geode::prelude;

// ═════════════════════════════════════════════════════════════════════════════
// Custom Setting: "delete-data-button"
// Renders a "Delete Data" button in the mod settings panel that opens
// DeleteDataLayer without storing any persistent value.
// ═════════════════════════════════════════════════════════════════════════════

class DeleteDataSetting : public SettingV3 {
public:
    static Result<std::shared_ptr<SettingV3>> parse(
        std::string const& key,
        std::string const& modID,
        matjson::Value const& json)
    {
        auto res = std::make_shared<DeleteDataSetting>();
        auto root = checkJson(json, "DeleteDataSetting");
        res->init(key, modID, root);
        root.checkUnknownKeys();
        return root.ok(std::static_pointer_cast<SettingV3>(res));
    }

    bool load(matjson::Value const& json) override { return true; }
    bool save(matjson::Value& json) const override { return true; }
    bool isDefaultValue() const override { return true; }
    void reset() override {}

    SettingNodeV3* createNode(float width) override;
};

class DeleteDataSettingNode : public SettingNodeV3 {
protected:
    bool init(std::shared_ptr<DeleteDataSetting> setting, float width) {
        if (!SettingNodeV3::init(setting, width)) return false;

        // Row height
        this->setContentSize({ width, 40.f });

        // Button centered in the row
        auto* sprite = ButtonSprite::create(
            "Delete Stored Data", "bigFont.fnt", "GJ_button_06.png", 0.6f
        );
        sprite->setColor({ 255, 80, 80 });

        auto* btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(DeleteDataSettingNode::onDeleteData)
        );

        auto* menu = CCMenu::create();
        menu->setPosition({ width / 2.f, 20.f });
        menu->addChild(btn);
        this->addChild(menu);

        return true;
    }

    void onDeleteData(CCObject*) {
        DeleteDataLayer::show();
    }

    // SettingNodeV3 contract — no-ops because this setting stores no value.
    void onCommit() override {}
    bool hasUncommittedChanges() const override { return false; }
    bool hasNonDefaultValue() const override { return false; }
    void onResetToDefault() override {}

public:
    static DeleteDataSettingNode* create(
        std::shared_ptr<DeleteDataSetting> setting, float width)
    {
        auto* node = new DeleteDataSettingNode();
        if (node && node->init(setting, width)) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }
};

SettingNodeV3* DeleteDataSetting::createNode(float width) {
    return DeleteDataSettingNode::create(
        std::static_pointer_cast<DeleteDataSetting>(shared_from_this()), width
    );
}

// Register the custom setting type as soon as the mod loads.
$on_mod(Loaded) {
    (void)Mod::get()->registerCustomSettingType(
        "delete-data-button", &DeleteDataSetting::parse
    );
}

// ═════════════════════════════════════════════════════════════════════════════
// HOOK 1 — PauseLayer::customSetup
// Injects the "AI Coach" button into the pause menu.
// Guarded by the "demon-list-mode" setting for competitive integrity.
// ═════════════════════════════════════════════════════════════════════════════

class $modify(GDCoachPauseLayer, PauseLayer) {

    void customSetup() {
        // Always call the original first so the normal pause UI is built.
        PauseLayer::customSetup();

        // ── Anti-cheat guard ───────────────────────────────────────────────────
        if (Mod::get()->getSettingValue<bool>("demon-list-mode")) {
            log::info("[GDCoach] Demon List Mode is active — skipping UI injection.");
            return;
        }

        // ── Roadmap constraint: fire only on the initialisation frame ──────────
        // customSetup is only called once per pause, so this requirement is
        // naturally satisfied — no extra rate-limiting needed here.

        // ── Create the AI Coach button ─────────────────────────────────────────
        // ButtonSprite renders GD-native styled text on a gold button background.
        auto* btnSprite = ButtonSprite::create(
            "AI Coach",     // label text
            "goldFont.fnt", // font
            "GJ_button_05.png", // background sprite (gold/yellow button)
            0.7f            // scale relative to label
        );

        auto* btn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(GDCoachPauseLayer::onCoachButton)
        );

        // ── Position: bottom-right area of pause menu, clear of GD's buttons ──
        // Use node-ids if available; fall back to a fixed offset otherwise.
        CCMenu* targetMenu = nullptr;

        if (auto* existingMenu = typeinfo_cast<CCMenu*>(
                this->getChildByID("right-button-menu"))) {
            targetMenu = existingMenu;
        }

        if (!targetMenu) {
            // Fallback: create our own menu positioned manually.
            auto* menu = CCMenu::create();
            menu->setPosition({ this->getContentSize().width - 60.f, 60.f });
            this->addChild(menu, 10);
            targetMenu = menu;
        }

        // Scale the button slightly for readability
        btn->setScale(0.85f);
        targetMenu->addChild(btn);

        // Update the menu's layout if it uses one (safe no-op if not).
        if (targetMenu->getLayout()) {
            targetMenu->updateLayout();
        }

        log::info("[GDCoach] AI Coach button injected into PauseLayer.");
    }

    // ── Button callback ───────────────────────────────────────────────────────
    void onCoachButton(CCObject*) {
        log::info("[GDCoach] Coach button pressed.");
        CoachLayer::show();
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// HOOK 2 — PlayLayer
// (a) init        — reset telemetry and capture level metadata
// (b) destroyPlayer — record each death with percentage + active gamemode
// ═════════════════════════════════════════════════════════════════════════════

class $modify(GDCoachPlayLayer, PlayLayer) {

    // ── 2a: Level load ────────────────────────────────────────────────────────
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto& tm = TelemetryManager::get();
        tm.reset();

        if (level) {
            LevelInfo info;
            info.name         = level->m_levelName;
            info.creator      = level->m_creatorName;
            info.difficulty   = static_cast<int>(level->m_difficulty);
            info.levelID      = level->m_levelID;
            info.isPlatformer = level->isPlatformer();
            tm.setLevelInfo(info);
        }

        return true;
    }

    // ── 2b: Player death ──────────────────────────────────────────────────────
    /**
     * destroyPlayer is the canonical hook point for deaths in GD 2.2.
     * m_progressFloat holds the completion ratio [0.0, 1.0] at the moment
     * of destruction, giving an accurate death percentage.
     */
    void destroyPlayer(PlayerObject* player, GameObject* obstacle) {
        // Record BEFORE calling original so the progress value is still valid.
        if (player == m_player1) { // Only track the primary player (not P2 in co-op)
            float percentage = PlayLayer::get()->getCurrentPercent();
            TelemetryManager::get().recordDeath(
                percentage,
                TelemetryManager::get().currentGamemode
            );
        }

        PlayLayer::destroyPlayer(player, obstacle);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// HOOK 3 — PlayerObject::update
// Reads the active vehicle flags from the player each frame and updates the
// TelemetryManager. Transitions are only logged on change (see implementation).
// ═════════════════════════════════════════════════════════════════════════════

class $modify(GDCoachPlayerObject, PlayerObject) {

    void update(float dt) {
        PlayerObject::update(dt);

        // Only track if we are in a live play session.
        if (!PlayLayer::get()) return;

        // Only track the primary player to avoid double-counting in co-op.
        auto* pl = PlayLayer::get();
        if (pl && this != pl->m_player1) return;

        // Read the exclusive vehicle flags set by GD internals.
        // These are mutually exclusive; the first matching one wins.
        Gamemode gm = Gamemode::Cube; // default

        if (m_isShip)   gm = Gamemode::Ship;
        else if (m_isBird)   gm = Gamemode::UFO;   // GD calls UFO "bird"
        else if (m_isBall)   gm = Gamemode::Ball;
        else if (m_isDart)   gm = Gamemode::Wave;  // GD calls Wave "dart"
        else if (m_isRobot)  gm = Gamemode::Robot;
        else if (m_isSpider) gm = Gamemode::Spider;
        else if (m_isSwing)  gm = Gamemode::Swing;

        TelemetryManager::get().setCurrentGamemode(gm);
    }

    void pushButton(PlayerButton p0) {
        PlayerObject::pushButton(p0);
        if (auto pl = PlayLayer::get(); pl && this == pl->m_player1) {
            float x = this->getPositionX();
            float y = this->getPositionY();
            float time = 0.0f; // Time omitted to fix compilation
            TelemetryManager::get().recordClick(x, y, time, true);
        }
    }

    void releaseButton(PlayerButton p0) {
        PlayerObject::releaseButton(p0);
        if (auto pl = PlayLayer::get(); pl && this == pl->m_player1) {
            float x = this->getPositionX();
            float y = this->getPositionY();
            float time = 0.0f; // Time omitted to fix compilation
            TelemetryManager::get().recordClick(x, y, time, false);
        }
    }
};
