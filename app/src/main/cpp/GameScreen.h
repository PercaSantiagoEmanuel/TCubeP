#pragma once
#include "Global.h"
#include "Renderer.h"
#include <vector>
#include <string>
#include <random>
#include <raylib.h>

struct Player {
    Vec2f pos;
    float radius = 18.0f;
    bool active = true;
};

struct TrailMark {
    Vec2f pos;
    float life;
};

struct Spark {
    Vec2f pos;
    float speed;
    float size;
};

struct Obstacle {
    Vec2f pos;
    Vec2f vel;
    EnemyType type;
    Color4f color;
    float radius = 18.0f;
    float timeAlive = 0.0f;
    float rotation = 0.0f;
    float speed = 0.0f;
};

struct Collectible {
    Vec2f pos;
    ItemType type;
    float radius = 14.0f;
    float speed = 0.0f;
    float timeAlive = 0.0f;
    float rotation = 0.0f;
};

struct Particle {
    Vec2f pos;
    Vec2f vel;
    Color4f color;
    float size;
    float life;
    float maxLife;
    float friction;
    int pType = 0;
};

struct BossShip {
    Vec2f pos;
    bool active = false;
    float timer = 0.0f;
    float attackCooldown = 0.0f;
};

class GameScreen {
public:
    GameScreen(int screenWidth, int screenHeight);
    ~GameScreen();

    void update(float dt, float mouseX, float mouseY, bool isClicked, bool isDown);
    void draw(Renderer& renderer); // Actualizat: eliminat const pentru a permite apelul beginFrame
    void startGame(GameMode mode);
    void forcePause();

private:
    void updateSpawns(float dt);
    void updateEntities(float dt);
    void checkCollisions();
    void triggerDeath();
    void spawnParticles(Vec2f pos, int count, Color4f baseColor, float speedMultiplier = 1.0f, ExplosionType expType = ExplosionType::Normal);
    void spawnFloatingText(Vec2f pos, const std::string& text, Color4f color = { 1,1,1,1 });
    float randFloat(float min, float max);
    bool checkButton(float x, float y, float w, float h, float mx, float my) const;
    void drawPlayerShape(const Renderer& r, float x, float y, float radius, Color4f color, SkinShape shape, float rotation) const;

    void playNextRandomMenuTrack();
    void playNextRandomGameplayTrack();
    void updateAudioVolumes();

    GameState currentState;
    GameMode currentMode;
    int width, height;
    float globalTime;
    int currentFPS;

    Player player;
    BossShip boss;
    std::vector<Obstacle> obstacles;
    std::vector<Collectible> collectibles;
    std::vector<Particle> particles;
    std::vector<Spark> backgroundSparks;
    std::vector<TrailMark> playerTrail;

    struct FloatingText { Vec2f pos; std::string text; Color4f color; float life, maxLife; };
    std::vector<FloatingText> floatingTexts;

    float spawnTimer;
    float spawnInterval;
    float fallSpeed;
    float shakeIntensity;
    float deathTimer;
    float screenFlashTimer;
    float bossWarningTimer;
    float scoreAccumulator;
    int nextBossScore;

    bool hasShield;
    float slowMoTimer;
    float timeScale;

    SaveData gameData;
    int score;
    float progress;
    int multiplier;
    int sessionCoins;

    int tutorialPage;
    int shopCategory;
    int shopSelection;

    std::vector<std::string> menuPlaylist;
    std::vector<std::string> gamePlaylist;

    Music menuMusic;
    Music gameMusic;
    Music bossMusic;

    Sound sfxClick;
    Sound sfxCoin;
    Sound sfxShield;
    Sound sfxTimeWarp;
    Sound sfxDeath;
    Sound sfxBoss;

    std::mt19937 rng;
};