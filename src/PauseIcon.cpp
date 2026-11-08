#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>
#include "Settings.hpp"

using namespace geode::prelude;

namespace {

// Draggable icon in the pause menu.
// Tap = toggle Hitborders. Drag = move. Fades when left alone, focused when touched/moved.
class HBIcon : public CCLayer {
    CCSprite* m_bg = nullptr;
    CCLabelBMFont* m_label = nullptr;
    bool m_touching = false;
    bool m_moved = false;
    CCPoint m_start;
    CCPoint m_grab;
    float m_idle = 0.f;
    float m_opacity = 255.f;

public:
    static HBIcon* create() {
        auto ret = new HBIcon();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;

        m_bg = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
        m_label = CCLabelBMFont::create("HB", "bigFont.fnt");
        m_label->setScale(0.55f);
        this->addChild(m_bg);
        this->addChild(m_label, 1);

        auto win = CCDirector::sharedDirector()->getWinSize();
        float nx = Mod::get()->getSavedValue<float>("icon-x", 0.93f);
        float ny = Mod::get()->getSavedValue<float>("icon-y", 0.86f);
        this->setPosition(CCPoint(nx * win.width, ny * win.height));

        this->refreshLook();
        this->setTouchEnabled(true);
        this->scheduleUpdate();
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -600, true);
    }

    void refreshLook() {
        bool on = hb::enabled();
        ccColor3B c = on ? ccColor3B{255, 255, 255} : ccColor3B{110, 110, 110};
        m_bg->setColor(c);
        m_label->setColor(c);
    }

    void update(float dt) override {
        m_idle = m_touching ? 0.f : m_idle + dt;

        float target = (m_touching || m_idle < 1.5f) ? 255.f : 70.f;
        m_opacity += (target - m_opacity) * std::min(1.f, dt * 8.f);

        float focus = (m_opacity - 70.f) / 185.f; // 0 = unfocused, 1 = focused
        float scale = 0.85f + 0.15f * focus + (m_touching ? 0.1f : 0.f);

        m_bg->setOpacity(static_cast<GLubyte>(m_opacity));
        m_label->setOpacity(static_cast<GLubyte>(m_opacity));
        m_bg->setScale(scale);
        m_label->setScale(0.55f * scale);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!this->getParent()) return false;
        CCPoint p = this->getParent()->convertToNodeSpace(touch->getLocation());
        if (ccpDistance(p, this->getPosition()) > 24.f) return false;

        m_touching = true;
        m_moved = false;
        m_idle = 0.f;
        m_start = p;
        m_grab = this->getPosition() - p;
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        CCPoint p = this->getParent()->convertToNodeSpace(touch->getLocation());
        m_idle = 0.f;

        if (!m_moved && ccpDistance(p, m_start) > 8.f) m_moved = true;
        if (!m_moved) return;

        auto win = CCDirector::sharedDirector()->getWinSize();
        CCPoint pos = p + m_grab;
        pos.x = std::clamp(pos.x, 20.f, win.width - 20.f);
        pos.y = std::clamp(pos.y, 20.f, win.height - 20.f);
        this->setPosition(pos);
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (m_moved) {
            this->savePosition();
        } else {
            hb::setEnabled(!hb::enabled());
            this->refreshLook();
        }
        m_touching = false;
        m_idle = 0.f;
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {
        if (m_moved) this->savePosition();
        m_touching = false;
        m_idle = 0.f;
    }

    void savePosition() {
        auto win = CCDirector::sharedDirector()->getWinSize();
        Mod::get()->setSavedValue<float>("icon-x", this->getPositionX() / win.width);
        Mod::get()->setSavedValue<float>("icon-y", this->getPositionY() / win.height);
    }
};

}

class $modify(HBPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        if (auto icon = HBIcon::create()) {
            this->addChild(icon, 1000);
        }
    }
};
