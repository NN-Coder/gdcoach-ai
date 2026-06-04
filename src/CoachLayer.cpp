#include "CoachLayer.hpp"
#include "TelemetryManager.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/TextArea.hpp>
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

/// Hardcoded inference server base URL.
static constexpr const char* SERVER_BASE_URL = "https://gdcoach-ai-worker.edcube.workers.dev";

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

    // ── Chat Scroll Layer ────────────────────────────────────────────────────
    auto scrollSize = CCSize{ POPUP_WIDTH - 40.f, POPUP_HEIGHT - 100.f };
    m_chatScroll = ScrollLayer::create(scrollSize);
    m_chatScroll->setPosition({ 20.f, 50.f });
    m_mainLayer->addChild(m_chatScroll);

    // ── Text Input ───────────────────────────────────────────────────────────
    m_textInput = TextInput::create(scrollSize.width - 60.f, "Ask for advice...");
    m_textInput->setCommonFilter(CommonFilter::Any);
    m_textInput->setPosition({ POPUP_WIDTH / 2.f - 40.f, 25.f });
    m_mainLayer->addChild(m_textInput);

    // ── Submit Button ────────────────────────────────────────────────────────
    auto* submitBtnSprite = ButtonSprite::create("Send", "goldFont.fnt", "GJ_button_01.png", 0.6f);
    auto* submitBtn = CCMenuItemSpriteExtra::create(submitBtnSprite, this, menu_selector(CoachLayer::onSubmit));
    auto* submitMenu = CCMenu::create();
    submitMenu->setPosition({ POPUP_WIDTH - 35.f, 25.f });
    submitMenu->addChild(submitBtn);
    m_mainLayer->addChild(submitMenu);

    // ── Spinner ───────────────────────────────────────────────────────────────
    auto* spinner = CCSprite::createWithSpriteFrameName("loadingCircle.png");
    spinner->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });
    spinner->setPosition({ POPUP_WIDTH / 2.f, POPUP_HEIGHT / 2.f });
    spinner->setScale(0.6f);
    m_mainLayer->addChild(spinner, 5, 100 /* tag */);
    m_spinnerNode = spinner;
    stopSpinner();

    // ── Status label ─────────────────────────────────────────────────────────
    auto* status = CCLabelBMFont::create("", "chatFont.fnt");
    status->setColor({ 255, 120, 120 });
    status->setScale(0.55f);
    status->setPosition({ POPUP_WIDTH / 2.f, POPUP_HEIGHT / 2.f - 60.f });
    status->setVisible(false);
    m_mainLayer->addChild(status, 5);
    m_statusLabel = status;

    // ── Load history ─────────────────────────────────────────────────────────
    auto& tm = TelemetryManager::get();
    for (const auto& msg : tm.memory.history) {
        addChatMessageToUI(msg.role, msg.text);
    }

    if (tm.memory.history.empty()) {
        fetchCoachingAdvice(); // initial fetch
    } else {
        // Delay to let layout settle, then scroll to bottom
        this->scheduleOnce(schedule_selector(CoachLayer::scrollToBottom), 0.f);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat UI
// ─────────────────────────────────────────────────────────────────────────────

void CoachLayer::addChatMessageToUI(const std::string& role, const std::string& text) {
    // Prefix label for role identification
    std::string prefixed = (role == "user") ? ("You: " + text) : ("Coach: " + text);

    // ── Text-width fix ─────────────────────────────────────────────────────────
    static constexpr float LABEL_INSET = 8.f;
    static constexpr float RIGHT_INSET = 24.f;  // Increased breathing room
    static constexpr float LABEL_SCALE = 0.5f;

    float scrollWidth   = POPUP_WIDTH - 40.f;
    float renderWidth   = scrollWidth - LABEL_INSET - RIGHT_INSET;

    // Use Geode's SimpleTextArea which is immune to the Cocos2d BMFont width bug
    auto* label = geode::SimpleTextArea::create(prefixed, "chatFont.fnt", LABEL_SCALE, renderWidth);
    label->setAnchorPoint({ 0.f, 1.f });
    
    // Set color using SimpleTextArea's native method
    cocos2d::ccColor4B color = (role == "user") ? ccc4(150, 255, 150, 255) : ccc4(230, 230, 230, 255);
    label->setColor(color);

    float padding     = 8.f;
    // Use SimpleTextArea's getHeight() to get the proper physical height
    float labelHeight = label->getHeight();
    m_chatHeight += labelHeight + padding;

    // Resize content layer
    m_chatScroll->m_contentLayer->setContentSize({ scrollWidth, m_chatHeight });

    // Re-stack all existing children top-down
    float currentY = m_chatHeight;
    auto children = CCArrayExt<CCNode*>(m_chatScroll->m_contentLayer->getChildren());
    for (auto* child : children) {
        float h;
        if (auto* textArea = static_cast<geode::SimpleTextArea*>(child)) {
            h = textArea->getHeight();
        } else {
            h = child->getContentSize().height * child->getScale();
        }
        child->setPosition({ LABEL_INSET, currentY });
        currentY -= (h + padding);
    }

    // Place new label at the bottom
    label->setPosition({ LABEL_INSET, currentY });
    m_chatScroll->m_contentLayer->addChild(label);

    // ── Conditionally scroll to bottom ──────────────────────────────────────────
    // The user requested: "when the AI gives a response, I don't want to scroll
    // to the top or bottom. I want it to stay at the start of the AI's reply."
    // So we ONLY force a scroll to bottom if the message is from the user.
    if (role == "user") {
        this->scheduleOnce(schedule_selector(CoachLayer::scrollToBottom), 0.f);
    }
}

void CoachLayer::keyDown(cocos2d::enumKeyCodes key, double p1) {
    if (key == cocos2d::KEY_Enter) {
        if (m_textInput && !m_textInput->getString().empty()) {
            this->onSubmit(nullptr);
            return; // consume the event
        }
    }
    geode::Popup::keyDown(key, p1);
}

void CoachLayer::onSubmit(CCObject*) {
    std::string text = m_textInput->getString();
    if (text.empty()) return;
    
    m_textInput->setString("");
    
    auto& tm = TelemetryManager::get();
    tm.addChatMessage("user", text);
    addChatMessageToUI("user", text);
    
    fetchCoachingAdvice(text);
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

void CoachLayer::fetchCoachingAdvice(const std::string& userMessage) {
    auto& tm = TelemetryManager::get();

    // ── Guard: no session data yet ────────────────────────────────────────────
    if (!tm.hasData() && userMessage.empty()) {
        displayError("No session data yet.\nDie at least once in a level first!");
        return;
    }
    
    startSpinner();
    if (m_statusLabel) m_statusLabel->setVisible(false);

    // ── Serialise telemetry to JSON (matjson) ─────────────────────────────────
    std::vector<matjson::Value> deathsArr;
    for (const auto& d : tm.deaths) {
        matjson::Value death;
        death.set("percentage",     matjson::Value(d.percentage));
        death.set("gamemode",       matjson::Value(gamemodeToString(d.gamemode)));
        death.set("attempt_number", matjson::Value(d.attemptNumber));
        deathsArr.push_back(std::move(death));
    }

    std::vector<matjson::Value> clicksArr;
    for (const auto& c : tm.clicks) {
        matjson::Value click;
        click.set("x", matjson::Value(c.x));
        click.set("y", matjson::Value(c.y));
        click.set("time", matjson::Value(c.time));
        click.set("isDown", matjson::Value(c.isDown));
        clicksArr.push_back(std::move(click));
    }

    std::vector<matjson::Value> historyArr;
    for (const auto& msg : tm.memory.history) {
        // Exclude the most recent message if it matches userMessage to avoid duplicate in context
        // Actually, our worker uses history so it's fine, it will see everything.
        matjson::Value h;
        h.set("role", matjson::Value(msg.role));
        h.set("text", matjson::Value(msg.text));
        historyArr.push_back(std::move(h));
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
    payload.set("clicks",           matjson::Value(clicksArr));
    payload.set("history",          matjson::Value(historyArr));
    payload.set("attempt_count",    matjson::Value(tm.attemptCount));
    payload.set("current_gamemode", matjson::Value(gamemodeToString(tm.currentGamemode)));
    
    if (!userMessage.empty()) {
        payload.set("message", matjson::Value(userMessage));
    }

    int maxTokens = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-tokens"));
    payload.set("max_tokens", matjson::Value(maxTokens));

    std::string jsonBody = payload.dump();
    std::string endpoint = std::string(SERVER_BASE_URL) + "/analyze";

    log::info("[GDCoach] POSTing telemetry to {} (max_tokens={})", endpoint, maxTokens);

    // ── Fire-and-forget async request ─────────────────────────────────────────
    Ref<CoachLayer> self = this;

    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.header("Accept", "text/plain");
    req.bodyString(jsonBody);

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

    auto& tm = TelemetryManager::get();
    tm.addChatMessage("model", text);
    addChatMessageToUI("model", text);

    log::info("[GDCoach] Coaching advice displayed.");
}

void CoachLayer::displayError(const std::string& message) {
    stopSpinner();

    if (m_statusLabel) {
        m_statusLabel->setString(("! " + message).c_str());
        m_statusLabel->setVisible(true);
    }

    log::warn("[GDCoach] Error displayed: {}", message);
}

// ─────────────────────────────────────────────────────────────────────────────
// Deferred scroll to bottom
// ─────────────────────────────────────────────────────────────────────────────

void CoachLayer::scrollToBottom(float) {
    if (!m_chatScroll || !m_chatScroll->m_contentLayer) return;
    
    // In Cocos2d-x ScrollLayer, the content layer anchor point is 0,0 (bottom-left).
    // When the content is taller than the view, setting Y to 0 aligns the bottom
    // of the content with the bottom of the scroll view. Setting Y to -overflow
    // aligns the top of the content with the top of the scroll view.
    // Since we place new messages at the bottom (near Y=0), we want Y=0.
    float viewHeight = m_chatScroll->getContentSize().height;
    if (m_chatHeight > viewHeight) {
        m_chatScroll->m_contentLayer->setPositionY(0.f);
    } else {
        // If content is smaller than view, Y=0 puts it at the bottom.
        // Usually we want it to start from top if it's small, which means
        // Y = viewHeight - m_chatHeight.
        m_chatScroll->m_contentLayer->setPositionY(viewHeight - m_chatHeight);
    }
}
