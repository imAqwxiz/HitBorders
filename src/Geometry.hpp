#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace hb {

using Pts = std::vector<cocos2d::CCPoint>;

inline float cross(cocos2d::CCPoint const& o, cocos2d::CCPoint const& a, cocos2d::CCPoint const& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Convex hull (Andrew's monotone chain), counter-clockwise.
inline Pts hull(Pts p) {
    std::sort(p.begin(), p.end(), [](auto const& a, auto const& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    p.erase(std::unique(p.begin(), p.end(), [](auto const& a, auto const& b) {
        return a.x == b.x && a.y == b.y;
    }), p.end());
    if (p.size() < 3) return p;

    Pts h(2 * p.size());
    size_t k = 0;
    for (size_t i = 0; i < p.size(); i++) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], p[i]) <= 0) k--;
        h[k++] = p[i];
    }
    for (size_t i = p.size() - 1, t = k + 1; i > 0; i--) {
        while (k >= t && cross(h[k - 2], h[k - 1], p[i - 1]) <= 0) k--;
        h[k++] = p[i - 1];
    }
    h.resize(k - 1);
    return h;
}

// Hitbox of an object as a convex polygon (rect, rotated rect, or circle).
inline Pts shapeOf(GameObject* obj) {
    Pts out;
    auto r = obj->getObjectRect();
    float cx = r.getMidX(), cy = r.getMidY();
    float hw = r.size.width * 0.5f, hh = r.size.height * 0.5f;

    // round hazards (saws etc.)
    if (obj->m_objectRadius > 0.f) {
        float rad = std::min(hw, hh);
        for (int i = 0; i < 16; i++) {
            float a = static_cast<float>(i) * 0.3926991f; // 2*pi/16
            out.push_back(cocos2d::CCPoint(cx + std::cos(a) * rad, cy + std::sin(a) * rad));
        }
        return out;
    }

    float rot = std::fmod(obj->getRotation(), 90.f);
    if (rot < 0.f) rot += 90.f;

    // axis-aligned (0/90/180/270)
    if (rot < 0.5f || rot > 89.5f) {
        out.push_back(cocos2d::CCPoint(cx - hw, cy - hh));
        out.push_back(cocos2d::CCPoint(cx + hw, cy - hh));
        out.push_back(cocos2d::CCPoint(cx + hw, cy + hh));
        out.push_back(cocos2d::CCPoint(cx - hw, cy + hh));
        return out;
    }

    // arbitrary rotation (cocos rotation is clockwise degrees)
    float a = -obj->getRotation() * 0.017453293f;
    float ca = std::cos(a), sa = std::sin(a);
    float xs[4] = {-hw, hw, hw, -hw};
    float ys[4] = {-hh, -hh, hh, hh};
    for (int i = 0; i < 4; i++) {
        out.push_back(cocos2d::CCPoint(cx + xs[i] * ca - ys[i] * sa, cy + xs[i] * sa + ys[i] * ca));
    }
    return out;
}

// Minkowski sum of a convex shape with a (2hw x 2hh) rectangle:
// the area where the CENTER of a player with that hitbox would collide.
inline Pts expand(Pts const& shape, float hw, float hh) {
    Pts all;
    all.reserve(shape.size() * 4);
    for (auto const& p : shape) {
        all.push_back(cocos2d::CCPoint(p.x - hw, p.y - hh));
        all.push_back(cocos2d::CCPoint(p.x + hw, p.y - hh));
        all.push_back(cocos2d::CCPoint(p.x + hw, p.y + hh));
        all.push_back(cocos2d::CCPoint(p.x - hw, p.y + hh));
    }
    return hull(all);
}

}
