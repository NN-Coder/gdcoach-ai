#include "CoachLayer.hpp"
#include "TelemetryManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/loader/Mod.hpp>
#include <matjson.hpp>
#include <sstream>

using namespace geode::prelude;

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr float POPUP_WIDTH  = 380.f;
static constexpr float POPUP_HEIGHT = 280.f;

// ─────────────────────────────────────────────────────────────────────────────
// Factory & show helper
// ─────────────────────────────────────────────────────────────────────────────

CoachLayer* CoachLayer::create() {
    auto* ret = new CoachLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void CoachLayer::show() {
    if (auto* layer = CoachLayer::create()) {
        layer->FLAlertLayer::show(); // explicitly call base to avoid recursion
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// init
// ─────────────────────────────────────────────────────────────────────────────

bool CoachLayer::init() {
    if (!Popup::init(POPUP_WIDTH, POPUP_HEIGHT)) return false;

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* title = CCLabelBMFont::create("AI Coach", "goldFont.fnt");
    title->setScale(0.85f);
    m_mainLayer->addChildAtPosition(title, Anchor::Top, { 0.f, -24.f });

    // ── Divider line ─────────────────────────────────────────────────────────
    auto* divider = CCLayerColor::create({ 255, 255, 255, 40 }, POPUP_WIDTH - 40.f, 1.f);
    divider->setPosition({ 20.f, POPUP_HEIGHT - 44.f });
    m_mainLayer->addChild(divider);

    // ── Spinner ───────────────────────────────────────────────────────────────
    auto* spinner = CCSprite::createWithSpriteFrameName("loadingCircle.png");
    spinner->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
    spinner->setPosition({ POPUP_WIDTH / 2.f, POPUP_HEIGHT / 2.f - 10.f });
    spinner->setScale(0.6f);
    m_mainLayer->addChild(spinner, 5, 100 /* tag */);
    m_spinnerNode = spinner;
    startSpinner();

    // ── Status label ─────────────────────────────────────────────────────────
    auto* status = CCLabelBMFont::create("Analyzing your session...", "chatFont.fnt");
    status->setColor({ 200, 200, 200 });
    status->setScale(0.55f);
    status->setPosition({ POPUP_WIDTH / 2.f, POPUP_HEIGHT / 2.f - 60.f });
    m_mainLayer->addChild(status, 5);
    m_statusLabel = status;

    // ── Response label (hidden until data arrives) ─────────────────────────
    auto* responseLabel = CCLabelBMFont::create("", "chatFont.fnt");
    responseLabel->setScale(0.5f);
    responseLabel->setColor({ 230, 230, 230 });
    responseLabel->setAlignment(kCCTextAlignmentLeft);
    responseLabel->setVisible(false);
    responseLabel->setPosition({ POPUP_WIDTH / 2.f, POPUP_HEIGHT / 2.f - 10.f });
    m_mainLayer->addChild(responseLabel, 5);
    m_responseLabel = responseLabel;

    // ── Kick off the request ──────────────────────────────────────────────────
    fetchCoachingAdvice();

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spinner helpers
// ─────────────────────────────────────────────────────────────────────────────

void CoachLayer::startSpinner() {
    if (!m_spinnerNode) return;
    m_spinnerNode->setVisible(true);
    auto* spin = CCRepeatForever::create(CCRotateBy::create(1.0f, 360.f));
    m_spinnerNode->runAction(spin);
}

void CoachLayer::stopSpinner() {
    if (!m_spinnerNode) return;
    m_spinnerNode->stopAllActions();
    m_spinnerNode->setVisible(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Network
// ─────────────────────────────────────────────────────────────────────────────

void CoachLayer::fetchCoachingAdvice() {
    auto& tm = TelemetryManager::get();

    // ── Guard: no session data yet ────────────────────────────────────────────
    if (!tm.hasData()) {
        displayError("No session data yet.\nDie at least once in a level first!");
        return;
    }

    // ── Serialise telemetry to JSON (matjson 3.x API) ─────────────────────────
    std::vector<matjson::Value> deathsArr;
    for (const auto& d : tm.deaths) {
        matjson::Value death;
        death.set("percentage",     matjson::Value(d.percentage));
        death.set("gamemode",       matjson::Value(gamemodeToString(d.gamemode)));
        death.set("attempt_number", matjson::Value(d.attemptNumber));
        deathsArr.push_back(std::move(death));
    }

    matjson::Value levelObj;
    levelObj.set("name",         matjson::Value(tm.levelInfo.name));
    levelObj.set("creator",      matjson::Value(tm.levelInfo.creator));
    levelObj.set("difficulty",   matjson::Value(tm.levelInfo.difficulty));
    levelObj.set("level_id",     matjson::Value(tm.levelInfo.levelID));
    levelObj.set("is_platformer",matjson::Value(tm.levelInfo.isPlatformer));

    matjson::Value payload;
    payload.set("level",            levelObj);
    payload.set("deaths",           matjson::Value(deathsArr));
    payload.set("attempt_count",    matjson::Value(tm.attemptCount));
    payload.set("current_gamemode", matjson::Value(gamemodeToString(tm.currentGamemode)));

    std::string jsonBody  = payload.dump();
    std::string serverUrl = Mod::get()->getSettingValue<std::string>("server-url");
    std::string endpoint  = serverUrl + "/analyze";

    log::info("[GDCoach] POSTing telemetry to {}", endpoint);
    log::debug("[GDCoach] Payload: {}", jsonBody);

    // ── Fire-and-forget async request ─────────────────────────────────────────
    // Retain `this` so it stays alive until the network callback fires.
    // TaskHolder is declared locally; it gets captured into the lambda below
    // so its lifetime is tied to the arc runtime task, not to `this`.
    Ref<CoachLayer> self = this;

    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Accept", "text/plain");
    req.bodyString(jsonBody);

    // geode::async::spawn fires the callback on the main thread when done.
    geode::async::spawn(
        req.post(endpoint),
        [self](web::WebResponse res) {
            if (res.cancelled()) {
                self->displayError("Request was cancelled.");
                return;
            }
            if (res.ok()) {
                std::string text = res.string().unwrapOrDefault();
                self->displayResponse(text);
            } else {
                std::string err = "Server returned error " +
                                  std::to_string(res.code()) + ".";
                self->displayError(err);
            }
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Response rendering
// ─────────────────────────────────────────────────────────────────────────────

void CoachLayer::displayResponse(const std::string& text) {
    stopSpinner();

    if (m_statusLabel) {
        m_statusLabel->setVisible(false);
    }

    if (m_responseLabel) {
        m_responseLabel->setString(text.c_str());
        m_responseLabel->setVisible(true);

        // Fade in for a polished feel
        m_responseLabel->setOpacity(0);
        m_responseLabel->runAction(CCFadeIn::create(0.3f));
    }

    log::info("[GDCoach] Coaching advice displayed.");
}

void CoachLayer::displayError(const std::string& message) {
    stopSpinner();

    if (m_statusLabel) {
        m_statusLabel->setString(("! " + message).c_str());
        m_statusLabel->setColor({ 255, 120, 120 });
        m_statusLabel->setScale(0.5f);
    }

    log::warn("[GDCoach] Error displayed: {}", message);
}
