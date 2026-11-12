#pragma once
#include <cmath>

namespace automap {
struct Point { float x, y; };
// Screen-space linear transform around the player's unchanged map position.
struct Transform {
    float xx = 1, xy = 0, yx = 0, yy = 1;
    Point Apply(Point p, Point pivot) const {
        const float x = p.x - pivot.x, y = p.y - pivot.y;
        return {pivot.x + xx*x + xy*y, pivot.y + yx*x + yy*y};
    }
};
// Ground-plane camera bases, in screen coordinates (screen Y points down).
// Using the actual view matrices also follows vertical middle-button dragging.
inline bool FromViews(const float* original, const float* rotated, Transform& out) {
    const float a = original[0], b = original[8];
    const float c = -original[1], d = -original[9];
    const float det = a*d - b*c;
    if (!std::isfinite(det) || std::fabs(det) < 1e-6f) return false;
    const float e = rotated[0], f = rotated[8];
    const float g = -rotated[1], h = -rotated[9];
    Transform result{(e*d-f*c)/det, (f*a-e*b)/det,
                     (g*d-h*c)/det, (h*a-g*b)/det};
    if (!std::isfinite(result.xx) || !std::isfinite(result.xy) ||
        !std::isfinite(result.yx) || !std::isfinite(result.yy)) return false;
    out = result;
    return true;
}
struct Vertex { float x, y, z, u, v; };
static_assert(sizeof(Vertex) == 20);
// The game's UI vertex builder uses x = width/2-screenX, y = height/2+screenY.
inline void TransformQuad(Vertex* vertices, const Transform& t, Point pivot,
                          float width, float height) {
    for (int i = 0; i < 4; ++i) {
        Point p{width*0.5f - vertices[i].x, vertices[i].y - height*0.5f};
        p = t.Apply(p, pivot);
        vertices[i].x = width*0.5f - p.x;
        vertices[i].y = p.y + height*0.5f;
    }
}

}
