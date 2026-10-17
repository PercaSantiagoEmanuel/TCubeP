#include <raylib.h>
#include <algorithm>
#include "Renderer.h"

Color Renderer::ToRayColor(Color4f c) const {
    return Color{
            static_cast<unsigned char>(std::clamp(c.r * 255.0f, 0.0f, 255.0f)),
            static_cast<unsigned char>(std::clamp(c.g * 255.0f, 0.0f, 255.0f)),
            static_cast<unsigned char>(std::clamp(c.b * 255.0f, 0.0f, 255.0f)),
            static_cast<unsigned char>(std::clamp(c.a * 255.0f, 0.0f, 255.0f))
    };
}

void Renderer::init(int width, int height, const char* title) {
    InitWindow(width, height, title);

    screenWidth = GetScreenWidth();
    screenHeight = GetScreenHeight();
    if (screenWidth <= 0) screenWidth = (width > 0 ? width : 600);
    if (screenHeight <= 0) screenHeight = (height > 0 ? height : 900);

    SetTargetFPS(60);

    camera.zoom = 1.0f;
    camera.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
    camera.target = { screenWidth / 2.0f, screenHeight / 2.0f };
    camera.rotation = 0.0f;
}

void Renderer::close() const {
    CloseWindow();
}

void Renderer::beginFrame(float shakeX, float shakeY, float zoom) {
    camera.offset = { screenWidth / 2.0f + shakeX, screenHeight / 2.0f + shakeY };
    camera.zoom = zoom;
    camera.target = { screenWidth / 2.0f, screenHeight / 2.0f };

    BeginDrawing();
    ClearBackground(ToRayColor(COLOR_BG));
    BeginMode2D(camera);
}

void Renderer::endFrame() const {
    EndMode2D();
    EndDrawing();
}

void Renderer::drawRect(float x, float y, float w, float h, Color4f color) const {
    DrawRectangleRec(Rectangle{ x, y, w, h }, ToRayColor(color));
}

void Renderer::drawHollowRect(float x, float y, float w, float h, float thick, Color4f color) const {
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, thick, ToRayColor(color));
}

void Renderer::drawCircle(float x, float y, float radius, Color4f color) const {
    DrawCircleV(Vector2{ x, y }, radius, ToRayColor(color));
}

void Renderer::drawGlowCircle(float x, float y, float radius, Color4f color) const {
    DrawCircleGradient(static_cast<int>(x), static_cast<int>(y), radius, ToRayColor(color), Fade(ToRayColor(color), 0.0f));
}

void Renderer::drawRotatedRect(float x, float y, float w, float h, float rotation, Color4f color) const {
    Rectangle rec{ x, y, w, h };
    Vector2 origin{ w / 2.0f, h / 2.0f };
    DrawRectanglePro(rec, origin, rotation, ToRayColor(color));
}

void Renderer::drawPoly(float x, float y, int sides, float radius, float rotation, Color4f color) const {
    DrawPoly(Vector2{ x, y }, sides, radius, rotation, ToRayColor(color));
}

void Renderer::drawGlassCard(float x, float y, float w, float h, float roundness) const {
    Rectangle rec{ x, y, w, h };
    DrawRectangleRounded(rec, roundness, 16, ToRayColor(COLOR_GLASS));
    DrawRectangleRoundedLines(rec, roundness, 16, 2.0f, ToRayColor(COLOR_GL_BORD));
}

void Renderer::drawString(const std::string& text, float x, float y, int fontSize, Color4f color, bool centered) const {
    Font defaultFont = GetFontDefault();
    float spacing = 2.0f;
    Vector2 pos = { x, y };

    if (centered) {
        Vector2 textSize = MeasureTextEx(defaultFont, text.c_str(), static_cast<float>(fontSize), spacing);
        pos.x -= textSize.x / 2.0f;
        pos.y -= textSize.y / 2.0f;
    }

    DrawTextEx(defaultFont, text.c_str(), pos, static_cast<float>(fontSize), spacing, ToRayColor(color));
}