
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
    constexpr float kLineRadius = 0.75f;

    void drawRectOutline(CCDrawNode* draw, const CCRect& rect, const ccColor4F& color) {
        const CCPoint bl = rect.origin;
        const CCPoint br = {rect.getMaxX(), rect.getMinY()};
        const CCPoint tr = {rect.getMaxX(), rect.getMaxY()};
        const CCPoint tl = {rect.getMinX(), rect.getMaxY()};

        draw->drawSegment(bl, br, kLineRadius, color);
        draw->drawSegment(br, tr, kLineRadius, color);
        draw->drawSegment(tr, tl, kLineRadius, color);
        draw->drawSegment(tl, bl, kLineRadius, color);
    }

    void drawPlayerOBB(CCDrawNode* draw, PlayerObject* player, const ccColor4F& color) {
        if (!player) return;

        auto* box = player->getOrientedBox();
        if (!box) return;

        const auto& corners = box->m_corners;

        for (size_t i = 0; i < corners.size(); ++i) {
            draw->drawSegment(
                corners[i],
                corners[(i + 1) % corners.size()],
                kLineRadius,
                color
            );
        }
    }
}

class $modify(AqwxizHitbordersPlayLayer, PlayLayer) {
    struct Fields {
        CCDrawNode* drawNode = nullptr;
    };

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // Create the renderer once.
        if (!m_fields->drawNode) {
            auto* node = CCDrawNode::create();
            if (!node || !m_objectLayer) return;

            m_fields->drawNode = node;
            m_objectLayer->addChild(node, 9999);
        }

        auto* draw = m_fields->drawNode;
        draw->clear();

        // Player collision boundaries.
        const ccColor4F playerColor(0.20f, 0.85f, 1.00f, 1.00f);

        drawPlayerOBB(draw, m_player1, playerColor);
        drawPlayerOBB(draw, m_player2, playerColor);

        // Solid objects.
        const ccColor4F solidColor(1.00f, 0.85f, 0.20f, 1.00f);

        for (auto* object : m_solidCollisionObjects) {
            if (!object || !object->isVisible()) continue;

            drawRectOutline(draw, object->getObjectRect(), solidColor);
        }

        // Hazard objects.
        const ccColor4F hazardColor(1.00f, 0.25f, 0.30f, 1.00f);

        for (auto* object : m_hazardCollisionObjects) {
            if (!object || !object->isVisible()) continue;

            drawRectOutline(draw, object->getObjectRect(), hazardColor);
        }
    }

    void onExit() {
        if (m_fields->drawNode) {
            m_fields->drawNode->removeFromParent();
            m_fields->drawNode = nullptr;
        }

        PlayLayer::onExit();
    }
};

