#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <vector>
#include "Settings.hpp"
#include "Geometry.hpp"

using namespace geode::prelude;

namespace {

struct Entry {
    float x;
    GameObject* obj;
    int cat;
};

// 0 = hazard, 1 = solid, 2 = special (orbs/pads/portals), -1 = skip
int classify(GameObject* obj) {
    switch (obj->m_objectType) {
        case GameObjectType::Hazard: return 0;
        case GameObjectType::Solid:
        case GameObjectType::Slope: return 1;
        case GameObjectType::Decoration:
        case GameObjectType::Modifier: return -1;
        default: return 2;
    }
}

ccColor4F const COLORS[3] = {
    {1.00f, 0.16f, 0.16f, 1.f}, // hazard
    {0.16f, 0.47f, 1.00f, 1.f}, // solid
    {0.24f, 1.00f, 0.47f, 1.f}, // special
};
constexpr float FILL_ALPHA = 0.25f;
constexpr float LINE_WIDTH = 0.5f;

ccColor4F withAlpha(ccColor4F c, float mul) {
    c.a *= mul;
    return c;
}

}

class $modify(HBPlayLayer, PlayLayer) {
    struct Fields {
        CCDrawNode* node = nullptr;
        std::vector<Entry> cache;
    };

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto node = CCDrawNode::create();
        m_objectLayer->addChild(node, 9999);
        m_fields->node = node;
        this->hbBuildCache();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        this->hbBuildCache();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        this->hbRedraw();
    }

    // Objects sorted by x so each frame only looks at what's on screen.
    void hbBuildCache() {
        auto& cache = m_fields->cache;
        cache.clear();
        if (!m_objects) return;
        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (!obj) continue;
            int cat = classify(obj);
            if (cat < 0) continue;
            cache.push_back({obj->getPositionX(), obj, cat});
        }
        std::sort(cache.begin(), cache.end(), [](Entry const& a, Entry const& b) {
            return a.x < b.x;
        });
    }

    void hbRedraw() {
        auto node = m_fields->node;
        if (!node) return;
        node->clear();
        if (!hb::enabled()) return;

        // visible area in object-layer space
        auto win = CCDirector::sharedDirector()->getWinSize();
        CCPoint a = m_objectLayer->convertToNodeSpace(CCPoint(0.f, 0.f));
        CCPoint b = m_objectLayer->convertToNodeSpace(CCPoint(win.width, win.height));
        float left = std::min(a.x, b.x) - 250.f;
        float right = std::max(a.x, b.x) + 250.f;
        float bottom = std::min(a.y, b.y) - 100.f;
        float top = std::max(a.y, b.y) + 100.f;

        // players whose hitbox size is used
        std::vector<PlayerObject*> players;
        if (m_player1) players.push_back(m_player1);
        if (m_player2 && m_gameState.m_isDualMode) players.push_back(m_player2);

        struct Half { float w, h; };
        std::vector<Half> halves;
        for (auto p : players) {
            auto pr = p->getObjectRect();
            halves.push_back({pr.size.width * 0.5f, pr.size.height * 0.5f});
        }

        auto& cache = m_fields->cache;
        auto it = std::lower_bound(cache.begin(), cache.end(), left, [](Entry const& e, float v) {
            return e.x < v;
        });

        for (; it != cache.end() && it->x <= right; ++it) {
            auto rect = it->obj->getObjectRect();
            if (rect.getMaxY() < bottom || rect.getMinY() > top) continue;

            auto shape = hb::shapeOf(it->obj);
            auto const& col = COLORS[it->cat];

            for (size_t i = 0; i < halves.size(); i++) {
                float mul = i == 0 ? 1.f : 0.6f; // P2 a bit fainter
                auto border = hb::expand(shape, halves[i].w, halves[i].h);
                node->drawPolygon(
                    border.data(), static_cast<unsigned int>(border.size()),
                    withAlpha(col, FILL_ALPHA * mul), LINE_WIDTH, withAlpha(col, mul)
                );
            }
        }

        // player centers (this is the point that must stay outside the borders)
        for (auto p : players) {
            auto pr = p->getObjectRect();
            node->drawDot(CCPoint(pr.getMidX(), pr.getMidY()), 2.f, ccColor4F{1.f, 0.9f, 0.16f, 1.f});
        }
    }
};
