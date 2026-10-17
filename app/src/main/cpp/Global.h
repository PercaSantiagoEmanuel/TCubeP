#pragma once
#include <cstdint>
#include <string>
#include <algorithm>
#include <raylib.h> // ADĂUGAT: Obligatoriu pentru funcțiile de salvare sigure pe Android

enum class GameState { MENU, SHOP, PLAYING, PAUSED, GAMEOVER, SETTINGS, TUTORIAL };
enum class GameMode { CLASSIC, HARDCORE, ZEN };
enum class EnemyType { NORMAL, SINEWAVE, HOMING };
enum class ItemType { COIN, SHIELD, TIMEWARP };

struct Vec2f { float x = 0.0f; float y = 0.0f; };
struct Color4f { float r = 1.0f; float g = 1.0f; float b = 1.0f; float a = 1.0f; };

constexpr Color4f COLOR_BG = { 0.05f, 0.05f, 0.12f, 1.0f };
constexpr Color4f COLOR_ENEMY_N = { 1.0f, 0.2f, 0.3f, 1.0f };
constexpr Color4f COLOR_ENEMY_S = { 0.8f, 0.2f, 1.0f, 1.0f };
constexpr Color4f COLOR_ENEMY_H = { 1.0f, 0.6f, 0.1f, 1.0f };
constexpr Color4f COLOR_GLASS = { 1.0f, 1.0f, 1.0f, 0.1f };
constexpr Color4f COLOR_GL_BORD = { 1.0f, 1.0f, 1.0f, 0.4f };

constexpr Color4f COLOR_COIN = { 1.0f, 0.8f, 0.1f, 1.0f };
constexpr Color4f COLOR_SHIELD = { 0.1f, 0.9f, 0.8f, 1.0f };
constexpr Color4f COLOR_TIMEWARP = { 0.9f, 0.9f, 0.1f, 1.0f };
constexpr Color4f COLOR_WARNING = { 1.0f, 0.0f, 0.2f, 0.6f };

enum class SkinShape { Circle = 0, Square = 1, Triangle = 2 };
enum class SkinColor { Cyan = 0, Pink = 1, Gold = 2 };
enum class ExplosionType { Normal = 0, Rings = 1, Geometry = 2 };

constexpr Color4f SHOP_COLORS[3] = {
        { 0.2f, 0.8f, 1.0f, 1.0f }, // Cyan
        { 1.0f, 0.2f, 0.8f, 1.0f }, // Neon Pink
        { 1.0f, 0.8f, 0.2f, 1.0f }  // Gold
};

struct SaveData {
    int highscore = 0;
    int totalCoins = 0;
    int equippedShape = 0;
    int equippedColor = 0;
    int equippedExplosion = 0;
    int unlockedShapes = 1;
    int unlockedColors = 1;
    int unlockedExplosions = 1;

    bool particlesEnabled = true;
    bool showFPS = false;

    // NOU: Salvare volume separat
    float menuMusicVolume = 0.3f;
    float gameMusicVolume = 0.4f;
    float sfxVolume = 0.8f;
};

inline SaveData LoadGameData() {
    SaveData data;

    // Asigurăm valori implicite la prima deschidere a jocului
    data.highscore = 0;
    data.totalCoins = 0;
    data.equippedShape = 0;
    data.equippedColor = 0;
    data.equippedExplosion = 0;
    data.unlockedShapes = 1;
    data.unlockedColors = 1;
    data.unlockedExplosions = 1;
    data.particlesEnabled = true;
    data.showFPS = false;
    data.menuMusicVolume = 0.3f;
    data.gameMusicVolume = 0.4f;
    data.sfxVolume = 0.8f;

    int dataSize = 0;
    // Raylib citește automat din memoria internă permisă pe Android
    unsigned char* fileData = LoadFileData("neondodge_save.dat", &dataSize);

    if (fileData != nullptr) {
        // Verificăm dacă fișierul are dimensiunea exactă a structurii tale pentru a nu corupe datele
        if (dataSize == sizeof(SaveData)) {
            data = *(SaveData*)fileData;
        }
        UnloadFileData(fileData);
    }

    return data;
}

inline void SaveGameData(const SaveData& data) {
    // Raylib va scrie automat fișierul folosind calea corectă permisă de Android
    SaveFileData("neondodge_save.dat", (void*)&data, sizeof(SaveData));
}