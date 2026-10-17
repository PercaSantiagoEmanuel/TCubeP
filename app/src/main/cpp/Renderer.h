#pragma once
#include <raylib.h>
#include <string>
#include "Global.h"

class Renderer {
public:
    void init(int width, int height, const char* title);
    void close() const;
    void beginFrame(float shakeX, float shakeY, float zoom);
    void endFrame() const;

    void drawRect(float x, float y, float w, float h, Color4f color) const;
    void drawHollowRect(float x, float y, float w, float h, float thick, Color4f color) const;
    void drawCircle(float x, float y, float radius, Color4f color) const;
    void drawGlowCircle(float x, float y, float radius, Color4f color) const;
    void drawRotatedRect(float x, float y, float w, float h, float rotation, Color4f color) const;
    void drawPoly(float x, float y, int sides, float radius, float rotation, Color4f color) const;
    void drawGlassCard(float x, float y, float w, float h, float roundness) const;
    void drawString(const std::string& text, float x, float y, int fontSize, Color4f color, bool centered = false) const;

    Color ToRayColor(Color4f c) const;

private:
    int screenWidth = 0;
    int screenHeight = 0;
    Camera2D camera = { 0 };
};