# Aqwxiz Hitborders

Initial Geode prototype for Geometry Dash 2.2081 / Geode 5.11.2.

## Current prototype
- Player 1 and player 2: draws `PlayerObject::getOrientedBox()` corners.
- Solid collision objects: draws `GameObject::getObjectRect()` outlines.
- Hazard collision objects: draws `GameObject::getObjectRect()` outlines.
- Renderer is attached to `GJBaseGameLayer::m_objectLayer` and refreshed after `PlayLayer::postUpdate`.

## Important accuracy note
This is a first rendering pass, not yet a complete representation of every collision primitive.
`getObjectRect()` provides an object collision rectangle, but special collision behavior such as slopes may use additional geometry or logic. Those require a dedicated shape pass and in-game validation.

## Build
Use a Geode SDK environment matching Geode 5.11.2 and GD 2.2081, then configure/build with the Geode SDK's normal CMake workflow.

## Next milestones
1. Verify compilation against the exact SDK and Android64/Windows toolchains.
2. Validate coordinate alignment and object filtering in live gameplay.
3. Add exact slope geometry and special-case collision shapes.
4. Add settings for player 1/player 2, hazards, solids, colors, and line thickness.
5. Profile performance on large levels.
