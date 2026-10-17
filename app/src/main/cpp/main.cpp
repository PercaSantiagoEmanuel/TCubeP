#include <raylib.h>
#include "Renderer.h"
#include "GameScreen.h"

int main() {
    // Forțăm Fullscreen pentru Android
    SetConfigFlags(FLAG_FULLSCREEN_MODE);

    Renderer renderer;
    renderer.init(0, 0, "Neon Dodge");

    // Cadru de trezire pentru Android
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (sw <= 0) sw = 1080;
    if (sh <= 0) sh = 1920;

    // ========================================================
    // AICI E SECRETUL: Scădem rezoluția virtuală la 540
    // Asta va face ca TOT jocul (HUD, meniuri, nave) să devină
    // proporțional mai mare și mult mai vizibil pe telefon!
    // ========================================================
    const int virtualWidth = 540;
    const int virtualHeight = (int)((float)sh / sw * virtualWidth);

    RenderTexture2D target = LoadRenderTexture(virtualWidth, virtualHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    InitAudioDevice();

    GameScreen game(virtualWidth, virtualHeight);

    while (!WindowShouldClose()) {
        sw = GetScreenWidth();
        sh = GetScreenHeight();
        if (sw <= 0) sw = 1080;
        if (sh <= 0) sh = 1920;

        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.016f;

        if (!IsWindowFocused() || IsWindowMinimized()) {
            game.forcePause();
        }

        Vector2 mousePos = GetMousePosition();
        if (GetTouchPointCount() > 0) {
            mousePos = GetTouchPosition(0);
        }

        // Convertim locația degetului pe noul "zoom" de 540p
        float scale = (float)sw / virtualWidth;
        float virtualMouseX = mousePos.x / scale;
        float virtualMouseY = mousePos.y / scale;

        bool isClicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || (GetTouchPointCount() > 0 && IsGestureDetected(GESTURE_TAP));
        static bool wasTouched = false;
        bool isTouching = GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        if (isTouching && !wasTouched) isClicked = true;
        wasTouched = isTouching;

        game.update(dt, virtualMouseX, virtualMouseY, isClicked, isTouching);

        // Randăm intern jocul la rezoluția mărită
        BeginTextureMode(target);
        ClearBackground(BLACK);
        game.draw(renderer);
        EndMode2D();
        EndTextureMode();

        // Întindem rezultatul peste tot ecranul
        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, -(float)target.texture.height };
        Rectangle destRec = { 0.0f, 0.0f, (float)sw, (float)sh };
        DrawTexturePro(target.texture, sourceRec, destRec, { 0.0f, 0.0f }, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseAudioDevice();
    renderer.close();
    return 0;
}