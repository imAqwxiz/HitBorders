#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <algorithm>
#include <cmath>
#include <deque>
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

// ---- look (tweak here) ----
ccColor4F const SOLID_EDGE   = {0.00f, 0.12f, 1.00f, 1.f};  // thick blue outline
ccColor4F const HAZARD_EDGE  = {1.00f, 0.10f, 0.10f, 1.f};  // bright red outline
ccColor4F const RED_FILL     = {0.62f, 0.00f, 0.00f, 0.90f}; // unreachable = full red
ccColor4F const SPECIAL_FILL = {0.00f, 0.65f, 0.10f, 0.85f};
ccColor4F const SPECIAL_EDGE = {0.20f, 1.00f, 0.20f, 1.f};
ccColor4F const TRAIL_COLOR  = {1.00f, 0.92f, 0.20f, 1.f};

constexpr float SOLID_BAND   = 3.0f; // outline thickness around solids
constexpr float HAZARD_BAND  = 1.0f;
constexpr float SPECIAL_BAND = 1.0f;
constexpr float TRAIL_RANGE  = 700.f; // how far behind the player the trail is kept

ccColor4F withAlpha(ccColor4F c, float mul) {
    c.a *= mul;
    return c;
}

// ---- player center trail ----
struct Mark {
    CCPoint pos;
    bool down;
};

struct Trail {
    std::deque<CCPoint> pts;
    std::vector<Mark> marks;
    void clear() {
        pts.clear();
        marks.clear();
    }
};

Trail g_trails[2]; // [0] = player 1, [1] = player 2

struct Draw {
    hb::Pts pts;
    int cat;
    float mul;
};

}

// Click / release markers on the trail.
class $modify(HBBaseLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
        if (!hb::enabled() || button != 1) return;

        auto p = isPlayer1 ? m_player1 : m_player2;
        if (!p) return;
        auto r = p->getObjectRect();
        g_trails[isPlayer1 ? 0 : 1].marks.push_back({CCPoint(r.getMidX(), r.getMidY()), down});
    }
};

class $modify(HBPlayLayer, PlayLayer) {
    struct Fields {
        CCDrawNode* node = nullptr;
        std::vector<Entry> cache;
    };

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        g_trails[0].clear();
        g_trails[1].clear();
        auto node = CCDrawNode::create();
        m_objectLayer->addChild(node, 9999);
        m_fields->node = node;
        this->hbBuildCache();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g_trails[0].clear();
        g_trails[1].clear();
        this->hbBuildCache();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        this->hbUpdateTrail();
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

    void hbUpdateTrail() {
        if (!hb::enabled()) {
            g_trails[0].clear();
            g_trails[1].clear();
            return;
        }

        PlayerObject* ps[2] = {
            m_player1,
            (m_player2 && m_gameState.m_isDualMode) ? m_player2 : nullptr
        };

        for (int i = 0; i < 2; i++) {
            auto& t = g_trails[i];
            if (!ps[i]) {
                t.clear();
                continue;
            }

            auto r = ps[i]->getObjectRect();
            CCPoint c(r.getMidX(), r.getMidY());

            // respawn / checkpoint / teleport: don't draw a line across the jump
            if (!t.pts.empty() && ccpDistance(t.pts.back(), c) > 200.f) t.clear();
            if (t.pts.empty() || ccpDistance(t.pts.back(), c) > 0.01f) t.pts.push_back(c);

            while (!t.pts.empty() && (std::abs(t.pts.front().x - c.x) > TRAIL_RANGE || t.pts.size() > 4000)) {
                t.pts.pop_front();
            }
            t.marks.erase(
                std::remove_if(t.marks.begin(), t.marks.end(), [&](Mark const& m) {
                    return std::abs(m.pos.x - c.x) > TRAIL_RANGE;
                }),
                t.marks.end()
            );
        }
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

        // 1) collect the expanded regions (object hitbox + player hitbox)
        std::vector<Draw> draws;
        auto& cache = m_fields->cache;
        auto it = std::lower_bound(cache.begin(), cache.end(), left, [](Entry const& e, float v) {
            return e.x < v;
        });

        for (; it != cache.end() && it->x <= right; ++it) {
            auto rect = it->obj->getObjectRect();
            if (rect.getMaxY() < bottom || rect.getMinY() > top) continue;

            auto shape = hb::shapeOf(it->obj);
            for (size_t i = 0; i < halves.size(); i++) {
                draws.push_back({
                    hb::expand(shape, halves[i].w, halves[i].h),
                    it->cat,
                    i == 0 ? 1.f : 0.6f // P2 a bit fainter
                });
            }
        }

        auto poly = [&](Draw& d, ccColor4F fill, float width, ccColor4F edge) {
            node->drawPolygon(d.pts.data(), static_cast<unsigned int>(d.pts.size()), fill, width, edge);
        };

        // 2) draw in layers so touching shapes merge into one region
        // specials (orbs / pads / portals): green
        for (auto& d : draws) {
            if (d.cat != 2) continue;
            poly(d, withAlpha(SPECIAL_FILL, d.mul), SPECIAL_BAND, withAlpha(SPECIAL_EDGE, d.mul));
        }
        // outline silhouettes: thick blue for solids, bright red for hazards
        for (auto& d : draws) {
            if (d.cat != 1) continue;
            poly(d, withAlpha(SOLID_EDGE, d.mul), SOLID_BAND, withAlpha(SOLID_EDGE, d.mul));
        }
        for (auto& d : draws) {
            if (d.cat != 0) continue;
            poly(d, withAlpha(HAZARD_EDGE, d.mul), HAZARD_BAND, withAlpha(HAZARD_EDGE, d.mul));
        }
        // full red on top: everywhere the player's center can't be.
        // Overlaps hide the inner outlines, so only the outer edge of the merged region stays visible.
        for (auto& d : draws) {
            if (d.cat == 2) continue;
            poly(d, withAlpha(RED_FILL, d.mul), 0.f, withAlpha(RED_FILL, d.mul));
        }

        // 3) trail of the player's center, with click markers
        int trailCount = (players.size() > 1) ? 2 : 1;
        for (int i = 0; i < trailCount; i++) {
            auto& t = g_trails[i];
            float mul = i == 0 ? 1.f : 0.6f;
            auto col = withAlpha(TRAIL_COLOR, mul);

            for (size_t k = 1; k < t.pts.size(); k++) {
                node->drawSegment(t.pts[k - 1], t.pts[k], 0.8f, col);
            }

            for (auto const& m : t.marks) {
                if (m.down) {
                    // click: open ring
                    hb::Pts ring;
                    for (int s = 0; s < 14; s++) {
                        float ang = static_cast<float>(s) * 0.4487989f; // 2*pi/14
                        ring.push_back(CCPoint(m.pos.x + std::cos(ang) * 3.5f, m.pos.y + std::sin(ang) * 3.5f));
                    }
                    node->drawPolygon(
                        ring.data(), static_cast<unsigned int>(ring.size()),
                        ccColor4F{0.f, 0.f, 0.f, 0.f}, 0.7f, col
                    );
                } else {
                    // release: solid dot
                    node->drawDot(m.pos, 3.5f, col);
                }
            }
        }

        // player centers (this point must stay out of the red)
        for (auto p : players) {
            auto pr = p->getObjectRect();
            node->drawDot(CCPoint(pr.getMidX(), pr.getMidY()), 3.f, TRAIL_COLOR);
        }
    }
};
