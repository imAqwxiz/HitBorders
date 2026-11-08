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

// Exact hitbox of an object as a convex polygon:
//  - round objects (saws, orbs): 16-gon circle
//  - slopes (triangle = true): right triangle, honoring flip + rotation
//  - everything else: rectangle, honoring rotation (incl. non-90 angles)
inline Pts shapeOf(GameObject* obj, bool triangle) {
    Pts out;
    auto r = obj->getObjectRect();
    float cx = r.getMidX(), cy = r.getMidY();

    if (obj->m_objectRadius > 0.f) {
        float rad = std::min(r.size.width, r.size.height) * 0.5f;
        for (int i = 0; i < 16; i++) {
            float a = static_cast<float>(i) * 0.3926991f; // 2*pi/16
            out.push_back(cocos2d::CCPoint(cx + std::cos(a) * rad, cy + std::sin(a) * rad));
        }
        return out;
    }

    float rot = obj->getRotation();
    int quarter = static_cast<int>(std::lround(rot / 90.f));
    bool ortho = std::fabs(rot - static_cast<float>(quarter) * 90.f) < 0.5f;

    // getObjectRect() of a 90/270 rotated object has width/height swapped: undo that
    // to get the un-rotated size, then rotate the real shape ourselves.
    float uw = r.size.width, uh = r.size.height;
    if (ortho && (quarter & 1)) std::swap(uw, uh);
    float hw = uw * 0.5f, hh = uh * 0.5f;

    std::vector<std::pair<float, float>> local;
    if (triangle) {
        // default slope: right angle at bottom-right (ramp going up to the right)
        local = {{-hw, -hh}, {hw, -hh}, {hw, hh}};
        bool fx = obj->isFlipX(), fy = obj->isFlipY();
        for (auto& p : local) {
            if (fx) p.first = -p.first;
            if (fy) p.second = -p.second;
        }
    } else {
        local = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
    }

    // cocos rotation is clockwise degrees
    float a = -rot * 0.017453293f;
    float ca = std::cos(a), sa = std::sin(a);
    for (auto const& p : local) {
        out.push_back(cocos2d::CCPoint(
            cx + p.first * ca - p.second * sa,
            cy + p.first * sa + p.second * ca
        ));
    }
    return out;
}

// Minkowski sum of a convex shape with a (2hw x 2hh) rectangle:
// the area where the CENTER of a player with that hitbox would touch the shape.
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
