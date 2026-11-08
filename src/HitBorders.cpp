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

// ---------------------------------------------------------------------------
// How the game really works (this is what makes the borders accurate):
//  - The player has an OUTER hitbox (full size) and a much smaller INNER hitbox.
//  - Hazards kill when the OUTER hitbox touches them.
//  - Solid blocks kill only when the INNER hitbox touches them; if just the outer
//    hitbox touches, the player lands / slides (that's the blue band).
//  - Orbs / pads / portals trigger when the OUTER hitbox touches them.
// So for the player's CENTER:
//  - hazard  + outer  -> red  (dead)
//  - solid   + inner  -> red  (dead)
//  - solid   + outer  -> blue (touching / standing, not dead)
//  - special + outer  -> green (triggers)
// The trail therefore must never go inside red; standing on a block runs along
// the outer edge of the blue band.
// ---------------------------------------------------------------------------

// inner hitbox size relative to the outer one (cube: 9 / 30)
constexpr float INNER_RATIO = 0.3f;

ccColor4F const SOLID_BAND   = {0.00f, 0.12f, 1.00f, 1.f};   // blue band around solids
ccColor4F const EDGE_RED     = {1.00f, 0.10f, 0.10f, 1.f};   // bright red outline
ccColor4F const RED_FILL     = {0.62f, 0.00f, 0.00f, 0.92f}; // unreachable = full red
ccColor4F const SPECIAL_FILL = {0.00f, 0.65f, 0.10f, 0.85f};
ccColor4F const SPECIAL_EDGE = {0.20f, 1.00f, 0.20f, 1.f};
ccColor4F const TRAIL_COLOR  = {1.00f, 0.92f, 0.20f, 1.f};

constexpr float EDGE_WIDTH   = 1.0f;
constexpr float TRAIL_RANGE  = 700.f; // how far behind the player the trail is kept

enum Kind { K_HAZARD = 0, K_SOLID_OUTER = 1, K_SOLID_INNER = 2, K_SPECIAL = 3 };

struct Entry {
    float x;
    GameObject* obj;
    int cat;     // 0 hazard, 1 solid, 2 special
    bool slope;
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

// Called after EVERY physics step (not just every frame), so the trail follows the
// real path instead of cutting straight lines between frames.
void sampleTrail(PlayerObject* p, int i) {
    auto& t = g_trails[i];
    if (!p) {
        t.clear();
        return;
    }

    auto r = p->getObjectRect();
    CCPoint c(r.getMidX(), r.getMidY());

    // respawn / checkpoint / teleport: don't draw a line across the jump
    if (!t.pts.empty() && ccpDistance(t.pts.back(), c) > 200.f) t.clear();
    if (t.pts.empty() || ccpDistance(t.pts.back(), c) > 0.01f) t.pts.push_back(c);

    while (!t.pts.empty() && (std::abs(t.pts.front().x - c.x) > TRAIL_RANGE || t.pts.size() > 8000)) {
        t.pts.pop_front();
    }
    t.marks.erase(
        std::remove_if(t.marks.begin(), t.marks.end(), [&](Mark const& m) {
            return std::abs(m.pos.x - c.x) > TRAIL_RANGE;
        }),
        t.marks.end()
    );
}

struct Draw {
    hb::Pts pts;
    int kind;
    float mul;
};

}

class $modify(HBBaseLayer, GJBaseGameLayer) {
    // one physics step
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (!hb::enabled()) {
            g_trails[0].clear();
            g_trails[1].clear();
            return;
        }
        sampleTrail(m_player1, 0);
        sampleTrail((m_player2 && m_gameState.m_isDualMode) ? m_player2 : nullptr, 1);
    }

    // click / release markers on the trail
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
        int frame = 0;
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
        // keep the sorted object list correct for objects moved by triggers
        if (++m_fields->frame % 60 == 0) this->hbRefreshCache();
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
            bool slope = obj->m_objectType == GameObjectType::Slope;
            cache.push_back({obj->getPositionX(), obj, cat, slope});
        }
        std::sort(cache.begin(), cache.end(), [](Entry const& a, Entry const& b) {
            return a.x < b.x;
        });
    }

    void hbRefreshCache() {
        auto& cache = m_fields->cache;
        for (auto& e : cache) e.x = e.obj->getPositionX();
        auto cmp = [](Entry const& a, Entry const& b) { return a.x < b.x; };
        if (!std::is_sorted(cache.begin(), cache.end(), cmp)) {
            std::sort(cache.begin(), cache.end(), cmp);
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

        struct Box { float ow, oh, iw, ih; };
        std::vector<Box> boxes;
        for (auto p : players) {
            auto pr = p->getObjectRect();
            float ow = pr.size.width * 0.5f, oh = pr.size.height * 0.5f;
            boxes.push_back({ow, oh, std::max(ow * INNER_RATIO, 0.5f), std::max(oh * INNER_RATIO, 0.5f)});
        }

        // 1) collect the regions the player's center can't (or shouldn't) be in
        std::vector<Draw> draws;
        auto& cache = m_fields->cache;
        auto it = std::lower_bound(cache.begin(), cache.end(), left, [](Entry const& e, float v) {
            return e.x < v;
        });

        for (; it != cache.end() && it->x <= right; ++it) {
            auto rect = it->obj->getObjectRect();
            if (rect.getMaxY() < bottom || rect.getMinY() > top) continue;

            auto shape = hb::shapeOf(it->obj, it->slope);
            for (size_t i = 0; i < boxes.size(); i++) {
                float mul = i == 0 ? 1.f : 0.6f; // P2 a bit fainter
                auto const& bx = boxes[i];

                if (it->cat == 0) {
                    draws.push_back({hb::expand(shape, bx.ow, bx.oh), K_HAZARD, mul});
                } else if (it->cat == 1) {
                    draws.push_back({hb::expand(shape, bx.ow, bx.oh), K_SOLID_OUTER, mul});
                    draws.push_back({hb::expand(shape, bx.iw, bx.ih), K_SOLID_INNER, mul});
                } else {
                    draws.push_back({hb::expand(shape, bx.ow, bx.oh), K_SPECIAL, mul});
                }
            }
        }

        auto poly = [&](Draw& d, ccColor4F fill, float width, ccColor4F edge) {
            node->drawPolygon(d.pts.data(), static_cast<unsigned int>(d.pts.size()), fill, width, edge);
        };

        // 2) draw in layers so touching shapes merge into one region
        // green: orbs / pads / portals
        for (auto& d : draws) {
            if (d.kind != K_SPECIAL) continue;
            poly(d, withAlpha(SPECIAL_FILL, d.mul), EDGE_WIDTH, withAlpha(SPECIAL_EDGE, d.mul));
        }
        // blue band: where a solid is touched by the outer hitbox (safe, thick outline)
        for (auto& d : draws) {
            if (d.kind != K_SOLID_OUTER) continue;
            poly(d, withAlpha(SOLID_BAND, d.mul), 0.f, withAlpha(SOLID_BAND, d.mul));
        }
        // bright red outlines (silhouettes; the fill below hides the inner part)
        for (auto& d : draws) {
            if (d.kind != K_HAZARD && d.kind != K_SOLID_INNER) continue;
            poly(d, withAlpha(EDGE_RED, d.mul), EDGE_WIDTH, withAlpha(EDGE_RED, d.mul));
        }
        // full red: everywhere the player's center means death
        for (auto& d : draws) {
            if (d.kind != K_HAZARD && d.kind != K_SOLID_INNER) continue;
            poly(d, withAlpha(RED_FILL, d.mul), 0.f, withAlpha(RED_FILL, d.mul));
        }

        // 3) trail of the player's center, with click markers
        int trailCount = (players.size() > 1) ? 2 : 1;
        for (int i = 0; i < trailCount; i++) {
            auto& t = g_trails[i];
            float mul = i == 0 ? 1.f : 0.6f;
            auto col = withAlpha(TRAIL_COLOR, mul);

            size_t step = t.pts.size() > 3000 ? 2 : 1; // keep it light on long trails
            for (size_t k = step; k < t.pts.size(); k += step) {
                node->drawSegment(t.pts[k - step], t.pts[k], 0.8f, col);
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
