#include "GameScreen.h"
#include <cmath>
#include <algorithm>
#include <ctime>

// --- STRUCTURI PENTRU BOSS ---
struct BossZone {
    Vec2f pos;
    float radius;
    float timer;
    float maxTimer;
    bool isSafe;
};
static std::vector<BossZone> bossZones;

static float currentMusicAmplitude = 0.0f;
static float smoothedAmplitude = 0.0f;

static void AudioProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    float sum = 0.0f;
    for (unsigned int i = 0; i < frames * 2; i++) {
        sum += fabsf(samples[i]);
    }
    currentMusicAmplitude = sum / (frames * 2.0f);
}

static bool IsTrackReady(const Music& music) {
    return music.stream.buffer != nullptr;
}

GameScreen::GameScreen(int screenWidth, int screenHeight)
        : width(screenWidth), height(screenHeight), rng(static_cast<unsigned int>(time(nullptr))), globalTime(0.0f), currentFPS(60) {

    obstacles.reserve(500);
    collectibles.reserve(50);
    particles.reserve(2000);
    backgroundSparks.reserve(100);
    floatingTexts.reserve(20);
    playerTrail.reserve(100);

    gameData = LoadGameData();

    currentState = GameState::MENU;
    currentMode = GameMode::CLASSIC;

    tutorialPage = 0;
    shopCategory = 0;
    shopSelection = 0;
    nextBossScore = 20000;

    for (int i = 0; i < 50; ++i) {
        backgroundSparks.push_back({
                                           { randFloat(0.0f, (float)width), randFloat(0.0f, (float)height) },
                                           randFloat(50.0f, 150.0f),
                                           randFloat(1.0f, 3.0f)
                                   });
    }

    sfxClick = LoadSound("audio/click.wav");
    sfxCoin = LoadSound("audio/coin.wav");
    sfxShield = LoadSound("audio/shield.wav");
    sfxTimeWarp = LoadSound("audio/timewarp.wav");
    sfxDeath = LoadSound("audio/death.wav");
    sfxBoss = LoadSound("audio/boss.wav");

    menuPlaylist.clear();
    menuPlaylist.push_back("audio/menu1.mp3");
    menuPlaylist.push_back("audio/menu2.mp3");
    menuPlaylist.push_back("audio/menu3.mp3");

    gamePlaylist.clear();
    gamePlaylist.push_back("audio/game1.mp3");
    gamePlaylist.push_back("audio/game2.mp3");
    gamePlaylist.push_back("audio/game3.mp3");

    menuMusic = { 0 };
    gameMusic = { 0 };
    bossMusic = LoadMusicStream("audio/boss_theme.mp3");
    if (IsTrackReady(bossMusic)) bossMusic.looping = false;

    updateAudioVolumes();
    playNextRandomMenuTrack();
}

GameScreen::~GameScreen() {
    if (IsTrackReady(menuMusic)) UnloadMusicStream(menuMusic);
    if (IsTrackReady(gameMusic)) {
        DetachAudioStreamProcessor(gameMusic.stream, AudioProcessCallback);
        UnloadMusicStream(gameMusic);
    }
    if (IsTrackReady(bossMusic)) {
        DetachAudioStreamProcessor(bossMusic.stream, AudioProcessCallback);
        UnloadMusicStream(bossMusic);
    }
    if (sfxClick.frameCount > 0) UnloadSound(sfxClick);
    if (sfxCoin.frameCount > 0) UnloadSound(sfxCoin);
    if (sfxShield.frameCount > 0) UnloadSound(sfxShield);
    if (sfxTimeWarp.frameCount > 0) UnloadSound(sfxTimeWarp);
    if (sfxDeath.frameCount > 0) UnloadSound(sfxDeath);
    if (sfxBoss.frameCount > 0) UnloadSound(sfxBoss);
}

void GameScreen::updateAudioVolumes() {
    if (IsTrackReady(menuMusic)) SetMusicVolume(menuMusic, gameData.menuMusicVolume);
    if (IsTrackReady(gameMusic)) SetMusicVolume(gameMusic, gameData.gameMusicVolume);
    if (IsTrackReady(bossMusic)) SetMusicVolume(bossMusic, gameData.gameMusicVolume);

    if (sfxClick.frameCount > 0) SetSoundVolume(sfxClick, gameData.sfxVolume);
    if (sfxCoin.frameCount > 0) SetSoundVolume(sfxCoin, gameData.sfxVolume);
    if (sfxShield.frameCount > 0) SetSoundVolume(sfxShield, gameData.sfxVolume);
    if (sfxTimeWarp.frameCount > 0) SetSoundVolume(sfxTimeWarp, gameData.sfxVolume);
    if (sfxDeath.frameCount > 0) SetSoundVolume(sfxDeath, gameData.sfxVolume);
    if (sfxBoss.frameCount > 0) SetSoundVolume(sfxBoss, gameData.sfxVolume);
}

void GameScreen::playNextRandomMenuTrack() {
    if (menuPlaylist.empty()) return;
    if (IsTrackReady(menuMusic)) {
        StopMusicStream(menuMusic);
        UnloadMusicStream(menuMusic);
    }
    int randomIndex = rng() % menuPlaylist.size();
    menuMusic = LoadMusicStream(menuPlaylist[randomIndex].c_str());
    if (IsTrackReady(menuMusic)) {
        menuMusic.looping = false;
        SetMusicVolume(menuMusic, gameData.menuMusicVolume);
        PlayMusicStream(menuMusic);
    }
}

void GameScreen::playNextRandomGameplayTrack() {
    if (gamePlaylist.empty()) return;
    if (IsTrackReady(gameMusic)) {
        DetachAudioStreamProcessor(gameMusic.stream, AudioProcessCallback);
        StopMusicStream(gameMusic);
        UnloadMusicStream(gameMusic);
    }
    int randomIndex = rng() % gamePlaylist.size();
    gameMusic = LoadMusicStream(gamePlaylist[randomIndex].c_str());
    if (IsTrackReady(gameMusic)) {
        gameMusic.looping = false;
        SetMusicVolume(gameMusic, gameData.gameMusicVolume);
        AttachAudioStreamProcessor(gameMusic.stream, AudioProcessCallback);
        PlayMusicStream(gameMusic);
    }
}

void GameScreen::startGame(GameMode mode) {
    currentMode = mode;
    currentState = GameState::PLAYING;
    player.active = true;
    player.pos = { width / 2.0f, height * 0.8f };

    obstacles.clear();
    collectibles.clear();
    particles.clear();
    floatingTexts.clear();
    playerTrail.clear();
    bossZones.clear();

    score = 0;
    sessionCoins = 0;
    progress = 0.0f;
    multiplier = 1;
    shakeIntensity = 0.0f;
    deathTimer = 0.0f;
    screenFlashTimer = 0.0f;
    bossWarningTimer = 0.0f;
    scoreAccumulator = 0.0f;
    nextBossScore = 20000;

    hasShield = false;
    slowMoTimer = 0.0f;
    timeScale = 1.0f;

    if (boss.active && IsTrackReady(bossMusic)) {
        DetachAudioStreamProcessor(bossMusic.stream, AudioProcessCallback);
        StopMusicStream(bossMusic);
    }
    boss.active = false;

    spawnTimer = 0.0f;
    spawnInterval = (mode == GameMode::HARDCORE) ? 0.6f : 1.2f;
    fallSpeed = (mode == GameMode::HARDCORE) ? 900.0f : 500.0f;

    if (IsTrackReady(menuMusic)) PauseMusicStream(menuMusic);
    playNextRandomGameplayTrack();
}

void GameScreen::forcePause() {
    if (currentState == GameState::PLAYING) {
        currentState = GameState::PAUSED;
    }
}

float GameScreen::randFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

bool GameScreen::checkButton(float x, float y, float w, float h, float mx, float my) const {
    if (mx >= x && mx <= x + w && my >= y && my <= y + h) {
        if (sfxClick.frameCount > 0) PlaySound(sfxClick);
        return true;
    }
    return false;
}

void GameScreen::update(float dt, float mouseX, float mouseY, bool isClicked, bool isDown) {
    globalTime += dt;
    if (dt > 0.0f) currentFPS = static_cast<int>(1.0f / dt);
    smoothedAmplitude += (currentMusicAmplitude - smoothedAmplitude) * 15.0f * dt;

    float cx = width / 2.0f;
    float safeTop = 15.0f;

    // --- REZOLVARE BUG TELEPORTARE ---
    static bool blockPlayerMovement = false;
    if (!isDown && !isClicked) {
        blockPlayerMovement = false;
    }

    if (currentState == GameState::PLAYING) {
        if (IsTrackReady(menuMusic) && IsMusicStreamPlaying(menuMusic)) PauseMusicStream(menuMusic);
        if (boss.active) {
            if (IsTrackReady(bossMusic)) {
                UpdateMusicStream(bossMusic);
                if (!IsMusicStreamPlaying(bossMusic)) ResumeMusicStream(bossMusic);
            }
        } else {
            if (IsTrackReady(gameMusic)) {
                UpdateMusicStream(gameMusic);
                if (!IsMusicStreamPlaying(gameMusic)) {
                    float played = GetMusicTimePlayed(gameMusic);
                    float length = GetMusicTimeLength(gameMusic);
                    if (length > 0.0f && played >= length - 0.1f) playNextRandomGameplayTrack();
                    else ResumeMusicStream(gameMusic);
                }
            }
        }

        if (player.active) playerTrail.push_back({player.pos, 0.25f});

        if (bossWarningTimer > 0.0f) {
            bossWarningTimer -= dt;
            if (bossWarningTimer <= 0.0f) {
                boss.active = true;
                boss.pos = { cx, -100.0f };
                boss.timer = 100.0f; // Boss scurtat la 100 secunde (1:40)
                bossZones.clear();

                if (IsTrackReady(gameMusic)) {
                    DetachAudioStreamProcessor(gameMusic.stream, AudioProcessCallback);
                    PauseMusicStream(gameMusic);
                }
                if (IsTrackReady(bossMusic)) {
                    SeekMusicStream(bossMusic, 0.0f);
                    PlayMusicStream(bossMusic);
                    AttachAudioStreamProcessor(bossMusic.stream, AudioProcessCallback);
                }
            }
        }
    } else {
        if (IsTrackReady(gameMusic) && IsMusicStreamPlaying(gameMusic)) PauseMusicStream(gameMusic);
        if (IsTrackReady(bossMusic) && IsMusicStreamPlaying(bossMusic)) PauseMusicStream(bossMusic);
        if (IsTrackReady(menuMusic)) {
            UpdateMusicStream(menuMusic);
            if (!IsMusicStreamPlaying(menuMusic)) {
                float played = GetMusicTimePlayed(menuMusic);
                float length = GetMusicTimeLength(menuMusic);
                if (length > 0.0f && played >= length - 0.1f) playNextRandomMenuTrack();
                else ResumeMusicStream(menuMusic);
            }
        }
    }

    if (shakeIntensity > 0.0f) {
        shakeIntensity -= 30.0f * dt;
        if (shakeIntensity < 0.0f) shakeIntensity = 0.0f;
    }
    if (screenFlashTimer > 0.0f) screenFlashTimer -= dt * 2.0f;

    float bgSpeedMod = 1.0f + (smoothedAmplitude * 3.0f);
    for (auto& spark : backgroundSparks) {
        spark.pos.y -= spark.speed * dt * timeScale * bgSpeedMod * ((currentMode == GameMode::HARDCORE) ? 2.0f : 1.0f);
        if (spark.pos.y < 0.0f) {
            spark.pos.y = (float)height;
            spark.pos.x = randFloat(0.0f, (float)width);
        }
    }

    if (currentState == GameState::MENU) {
        if (isClicked) {
            if (checkButton(cx - 130, height / 2.0f - 40, 260, 65, mouseX, mouseY)) startGame(GameMode::CLASSIC);
            else if (checkButton(cx - 130, height / 2.0f + 45, 260, 65, mouseX, mouseY)) {
                currentState = GameState::SHOP; shopCategory = 0; shopSelection = gameData.equippedShape;
            }
            else if (checkButton(cx - 130, height / 2.0f + 130, 260, 65, mouseX, mouseY)) currentState = GameState::SETTINGS;
            else if (checkButton(width - 70, safeTop, 50, 50, mouseX, mouseY)) {
                currentState = GameState::TUTORIAL; tutorialPage = 0;
            }
        }
        return;
    }

    if (currentState == GameState::SETTINGS) {
        if (isDown && mouseX >= cx - 140 && mouseX <= cx + 140 && mouseY >= height / 2.0f + 20 && mouseY <= height / 2.0f + 70) {
            gameData.menuMusicVolume = std::clamp((mouseX - (cx - 140)) / 280.0f, 0.0f, 1.0f);
            updateAudioVolumes();
        }
        if (isDown && mouseX >= cx - 140 && mouseX <= cx + 140 && mouseY >= height / 2.0f + 90 && mouseY <= height / 2.0f + 140) {
            gameData.sfxVolume = std::clamp((mouseX - (cx - 140)) / 280.0f, 0.0f, 1.0f);
            updateAudioVolumes();
        }

        if (isClicked) {
            if (checkButton(cx - 160, height / 2.0f - 120, 320, 50, mouseX, mouseY)) {
                gameData.particlesEnabled = !gameData.particlesEnabled; SaveGameData(gameData);
            }
            else if (checkButton(cx - 160, height / 2.0f - 50, 320, 50, mouseX, mouseY)) {
                gameData.showFPS = !gameData.showFPS; SaveGameData(gameData);
            }
            else if (checkButton(cx - 160, height / 2.0f + 160, 320, 50, mouseX, mouseY)) {
                gameData.highscore = 0; gameData.totalCoins = 0; gameData.equippedShape = 0; gameData.equippedColor = 0;
                gameData.equippedExplosion = 0; gameData.unlockedShapes = 1; gameData.unlockedColors = 1; gameData.unlockedExplosions = 1;
                SaveGameData(gameData);
            }
            else if (checkButton(cx - 130, height - 120, 260, 65, mouseX, mouseY)) {
                SaveGameData(gameData); currentState = GameState::MENU;
            }
        }
        return;
    }

    if (currentState == GameState::TUTORIAL) {
        if (isClicked) {
            if (checkButton(cx - 140, height - 160, 130, 50, mouseX, mouseY) && tutorialPage > 0) tutorialPage--;
            else if (checkButton(cx + 10, height - 160, 130, 50, mouseX, mouseY) && tutorialPage < 2) tutorialPage++;
            else if (checkButton(cx - 130, height - 90, 260, 60, mouseX, mouseY)) currentState = GameState::MENU;
        }
        return;
    }

    if (currentState == GameState::SHOP) {
        if (isClicked) {
            if (checkButton(cx - 190, safeTop + 70, 120, 45, mouseX, mouseY)) { shopCategory = 0; shopSelection = gameData.equippedShape; }
            else if (checkButton(cx - 60, safeTop + 70, 120, 45, mouseX, mouseY)) { shopCategory = 1; shopSelection = gameData.equippedColor; }
            else if (checkButton(cx + 70, safeTop + 70, 120, 45, mouseX, mouseY)) { shopCategory = 2; shopSelection = gameData.equippedExplosion; }

            else if (checkButton(20, height / 2 - 35, 60, 60, mouseX, mouseY)) shopSelection = (shopSelection + 2) % 3;
            else if (checkButton(width - 80, height / 2 - 35, 60, 60, mouseX, mouseY)) shopSelection = (shopSelection + 1) % 3;

            else if (checkButton(cx - 120, height / 2 + 130, 240, 60, mouseX, mouseY)) {
                int bitMask = 1 << shopSelection;
                int* unlocked = (shopCategory == 0) ? &gameData.unlockedShapes : ((shopCategory == 1) ? &gameData.unlockedColors : &gameData.unlockedExplosions);
                int* equipped = (shopCategory == 0) ? &gameData.equippedShape : ((shopCategory == 1) ? &gameData.equippedColor : &gameData.equippedExplosion);

                int price = (shopCategory == 0) ? ((shopSelection == 1) ? 500 : ((shopSelection == 2) ? 1000 : 0)) :
                            (shopCategory == 1) ? ((shopSelection == 1) ? 300 : ((shopSelection == 2) ? 600 : 0)) :
                            ((shopSelection == 1) ? 800 : ((shopSelection == 2) ? 1500 : 0));

                if (*unlocked & bitMask) { *equipped = shopSelection; SaveGameData(gameData); }
                else if (gameData.totalCoins >= price) {
                    gameData.totalCoins -= price; *unlocked |= bitMask; *equipped = shopSelection; SaveGameData(gameData);
                }
            }
            else if (checkButton(cx - 120, height - 100, 240, 60, mouseX, mouseY)) {
                SaveGameData(gameData); currentState = GameState::MENU;
            }
        }
        return;
    }

    if (currentState == GameState::GAMEOVER) {
        deathTimer -= dt;
        if (deathTimer <= 0.0f && isClicked && checkButton(cx - 150, height / 2.0f + 120, 300, 60, mouseX, mouseY)) {
            currentState = GameState::MENU;
        }
        updateEntities(dt);
        return;
    }

    if (currentState == GameState::PAUSED) {
        if (isDown && mouseX >= cx - 130 && mouseX <= cx + 130 && mouseY >= height / 2.0f + 120 && mouseY <= height / 2.0f + 170) {
            gameData.gameMusicVolume = std::clamp((mouseX - (cx - 130)) / 260.0f, 0.0f, 1.0f);
            updateAudioVolumes();
        }
        if (isClicked) {
            if (checkButton(cx - 130, height / 2.0f - 50, 260, 60, mouseX, mouseY)) {
                SaveGameData(gameData);
                currentState = GameState::PLAYING;
                blockPlayerMovement = true; // Previne saltul pe ecran
            }
            else if (checkButton(cx - 130, height / 2.0f + 30, 260, 60, mouseX, mouseY)) {
                SaveGameData(gameData);
                currentState = GameState::MENU;
            }
        }
        return;
    }

    if (currentState == GameState::PLAYING) {
        if (isClicked && checkButton(width - 110.0f, safeTop + 10.0f, 95.0f, 50.0f, mouseX, mouseY)) {
            currentState = GameState::PAUSED;
            blockPlayerMovement = true; // Previne saltul pe ecran
            return;
        }

        if (player.active && !blockPlayerMovement) {
            if (isClicked) spawnParticles(player.pos, 10, SHOP_COLORS[gameData.equippedColor], 0.5f, static_cast<ExplosionType>(gameData.equippedExplosion));
            if (isClicked || isDown) player.pos = { mouseX, mouseY };
        }

        if (slowMoTimer > 0.0f) { slowMoTimer -= dt; timeScale = 0.4f; }
        else timeScale += (1.0f - timeScale) * 5.0f * dt;

        progress += 100.0f * dt;
        if (progress >= 500.0f) {
            progress = 0.0f; multiplier++;
            spawnFloatingText(player.pos, "MULTIPLIER x" + std::to_string(multiplier), SHOP_COLORS[gameData.equippedColor]);
            fallSpeed += 25.0f; spawnInterval = std::max(0.4f, spawnInterval - 0.05f);
        }

        if (score >= nextBossScore && !boss.active && bossWarningTimer <= 0.0f) {
            bossWarningTimer = 3.0f; nextBossScore += 20000;
            if (sfxBoss.frameCount > 0) PlaySound(sfxBoss);
        }

        scoreAccumulator += multiplier * 15.0f * dt;
        if (scoreAccumulator >= 1.0f) {
            int add = static_cast<int>(scoreAccumulator);
            score += add; scoreAccumulator -= add;
        }

        updateSpawns(dt);
        updateEntities(dt);
        if (currentMode != GameMode::ZEN) checkCollisions();
    }
}

void GameScreen::updateSpawns(float dt) {
    spawnTimer += dt * timeScale;
    float dynamicInterval = spawnInterval;
    if (smoothedAmplitude > 0.15f) dynamicInterval *= 0.6f;

    // NU mai generăm inamici normali din tavan dacă suntem la Boss Faza 3
    if (boss.active && boss.timer <= 60.0f) return;

    if (spawnTimer >= dynamicInterval) {
        spawnTimer = 0.0f;
        if (randFloat(0.0f, 1.0f) < 0.12f) {
            Collectible item;
            item.pos = { randFloat(20.0f, width - 20.0f), -30.0f };
            item.speed = fallSpeed * 0.7f;
            item.rotation = 0.0f;
            float r = randFloat(0.0f, 1.0f);
            if (r < 0.6f) item.type = ItemType::COIN;
            else if (r < 0.8f) item.type = ItemType::SHIELD;
            else item.type = ItemType::TIMEWARP;
            collectibles.push_back(item);
            return;
        }

        Obstacle obs;
        obs.pos = { randFloat(20.0f, width - 20.0f), -30.0f };
        obs.speed = randFloat(fallSpeed * 0.8f, fallSpeed * 1.2f);
        obs.timeAlive = 0.0f;
        obs.rotation = randFloat(0.0f, 360.0f);
        float r = randFloat(0.0f, 1.0f);
        if (r < 0.6f) { obs.type = EnemyType::NORMAL; obs.color = COLOR_ENEMY_N; }
        else if (r < 0.85f) { obs.type = EnemyType::SINEWAVE; obs.color = COLOR_ENEMY_S; }
        else { obs.type = EnemyType::HOMING; obs.color = COLOR_ENEMY_H; }
        obstacles.push_back(obs);
    }
}

void GameScreen::updateEntities(float dt) {
    float gameDt = dt * timeScale;
    for (int i = static_cast<int>(playerTrail.size()) - 1; i >= 0; --i) {
        playerTrail[i].life -= dt;
        if (playerTrail[i].life <= 0.0f) {
            playerTrail[i] = playerTrail.back();
            playerTrail.pop_back();
        }
    }

    if (boss.active) {
        boss.timer -= gameDt; // Timpul scade de la 100
        boss.pos.y += (120.0f - boss.pos.y) * 2.0f * gameDt;
        boss.pos.x = (width / 2.0f) + std::sinf(globalTime * 2.5f) * (width * 0.3f);
        boss.attackCooldown -= gameDt;

        if (boss.timer > 80.0f) {
            // FAZA 1 (1:40 - 1:20): Conuri de triunghiuri
            if (boss.attackCooldown <= 0.0f) {
                boss.attackCooldown = 0.18f; // Trage rapid

                Obstacle obs;
                obs.type = EnemyType::HOMING; // O facem triunghi portocaliu
                obs.color = COLOR_ENEMY_H;
                obs.speed = fallSpeed * 0.9f;
                obs.timeAlive = 0.0f;
                obs.rotation = randFloat(0.0f, 360.0f);

                // Mâna Stângă
                obs.pos = { boss.pos.x - 80.0f, boss.pos.y };
                obs.vel = { randFloat(-300.0f, 0.0f), randFloat(200.0f, 400.0f) };
                obstacles.push_back(obs);

                // Mâna Dreaptă
                obs.pos = { boss.pos.x + 80.0f, boss.pos.y };
                obs.vel = { randFloat(0.0f, 300.0f), randFloat(200.0f, 400.0f) };
                obstacles.push_back(obs);
            }
        }
        else if (boss.timer > 60.0f) {
            // FAZA 2 (1:20 - 1:00): Zone Roșii Random (Danger Zones)
            if (boss.attackCooldown <= 0.0f) {
                boss.attackCooldown = 0.7f;
                BossZone z;
                z.pos = { randFloat(50.0f, width - 50.0f), randFloat(150.0f, height - 50.0f) };
                z.radius = randFloat(90.0f, 130.0f);
                z.timer = 1.5f;
                z.maxTimer = 1.5f;
                z.isSafe = false;
                bossZones.push_back(z);
            }
        }
        else if (boss.timer > 0.0f) {
            // FAZA 3 (1:00 - 0:00): Doar Safe Zones
            if (bossZones.empty() && boss.attackCooldown <= 0.0f) {
                boss.attackCooldown = 1.5f; // Timpul intre zone
                BossZone z;
                z.pos = { randFloat(80.0f, width - 80.0f), randFloat(200.0f, height - 80.0f) };
                z.radius = 110.0f;
                z.timer = 2.5f;
                z.maxTimer = 2.5f;
                z.isSafe = true;
                bossZones.push_back(z);
            }
        }
        else {
            // FINAL BOSS
            boss.active = false;
            bossZones.clear();
            score += 10000;
            spawnFloatingText(boss.pos, "BOSS DEFEATED +10000", COLOR_COIN);
            spawnParticles(boss.pos, 150, COLOR_COIN, 2.0f, ExplosionType::Geometry);
            if (IsTrackReady(bossMusic)) { DetachAudioStreamProcessor(bossMusic.stream, AudioProcessCallback); StopMusicStream(bossMusic); }
            playNextRandomGameplayTrack();
        }
    }

    // UPDATE PENTRU ZONELE BOSS-ULUI
    for (int i = static_cast<int>(bossZones.size()) - 1; i >= 0; --i) {
        bossZones[i].timer -= gameDt;
        if (bossZones[i].timer <= 0.0f) {
            float dx = player.pos.x - bossZones[i].pos.x;
            float dy = player.pos.y - bossZones[i].pos.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            bool isInside = dist < bossZones[i].radius;

            if (bossZones[i].isSafe) {
                // Ai murit dacă nu erai în cercul verde la expirarea timpului
                if (!isInside && player.active) {
                    if (hasShield) { hasShield = false; PlaySound(sfxShield); screenFlashTimer = 1.0f; }
                    else triggerDeath();
                }
            } else {
                // Ai murit dacă erai fix în zona roșie la expirarea timpului
                if (isInside && player.active) {
                    if (hasShield) { hasShield = false; PlaySound(sfxShield); screenFlashTimer = 1.0f; }
                    else triggerDeath();
                }
            }

            spawnParticles(bossZones[i].pos, 40, bossZones[i].isSafe ? COLOR_SHIELD : COLOR_ENEMY_N, 1.5f, ExplosionType::Normal);
            bossZones[i] = bossZones.back();
            bossZones.pop_back();
        }
    }

    for (int i = static_cast<int>(obstacles.size()) - 1; i >= 0; --i) {
        Obstacle& o = obstacles[i];
        o.timeAlive += gameDt; o.rotation += 90.0f * gameDt;

        o.pos.x += o.vel.x * gameDt; // Mișcare laterală (pt Faza 1 boss)
        o.pos.y += o.speed * gameDt;

        if (o.type == EnemyType::SINEWAVE) o.pos.x += std::sinf(o.timeAlive * 5.0f) * 150.0f * gameDt;
        else if (o.type == EnemyType::HOMING && o.vel.x == 0.0f) {
            // Inamicii Homing normali care n-au fost trași de boss curbează spre jucător
            float dirX = player.pos.x - o.pos.x;
            float sign = (dirX > 0.0f) ? 1.0f : ((dirX < 0.0f) ? -1.0f : 0.0f);
            o.pos.x += sign * 120.0f * gameDt;
        }

        if (o.pos.y > height + 50.0f || o.pos.x < -100.0f || o.pos.x > width + 100.0f) {
            obstacles[i] = obstacles.back(); obstacles.pop_back();
        }
    }

    for (int i = static_cast<int>(collectibles.size()) - 1; i >= 0; --i) {
        Collectible& c = collectibles[i];
        c.rotation += 180.0f * gameDt; c.pos.y += c.speed * gameDt;
        if (c.pos.y > height + 50.0f) { collectibles[i] = collectibles.back(); collectibles.pop_back(); }
    }

    for (int i = static_cast<int>(particles.size()) - 1; i >= 0; --i) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0.0f) { particles[i] = particles.back(); particles.pop_back(); continue; }
        p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt;
        p.vel.x *= p.friction; p.vel.y *= p.friction;
    }

    for (int i = static_cast<int>(floatingTexts.size()) - 1; i >= 0; --i) {
        FloatingText& ft = floatingTexts[i];
        ft.life -= dt; ft.pos.y -= 50.0f * dt;
        if (ft.life <= 0.0f) { floatingTexts[i] = floatingTexts.back(); floatingTexts.pop_back(); }
    }
}

void GameScreen::checkCollisions() {
    if (!player.active) return;
    for (int i = static_cast<int>(collectibles.size()) - 1; i >= 0; --i) {
        float dx = player.pos.x - collectibles[i].pos.x; float dy = player.pos.y - collectibles[i].pos.y;
        float distSq = dx * dx + dy * dy;
        float radSum = player.radius + collectibles[i].radius;
        if (distSq < (radSum * radSum)) {
            if (collectibles[i].type == ItemType::COIN) {
                if (sfxCoin.frameCount > 0) PlaySound(sfxCoin);
                sessionCoins++; score += 500;
                spawnFloatingText(player.pos, "+500", COLOR_COIN);
                spawnParticles(collectibles[i].pos, 15, COLOR_COIN, 0.5f);
            }
            else if (collectibles[i].type == ItemType::SHIELD) {
                if (sfxShield.frameCount > 0) PlaySound(sfxShield);
                hasShield = true; screenFlashTimer = 1.0f;
                spawnFloatingText(player.pos, "SHIELD ACTIVE!", COLOR_SHIELD);
            }
            else if (collectibles[i].type == ItemType::TIMEWARP) {
                if (sfxTimeWarp.frameCount > 0) PlaySound(sfxTimeWarp);
                slowMoTimer = 5.0f; screenFlashTimer = 1.0f;
                spawnFloatingText(player.pos, "SLOW MOTION!", COLOR_TIMEWARP);
            }
            collectibles[i] = collectibles.back(); collectibles.pop_back();
        }
    }

    for (int i = static_cast<int>(obstacles.size()) - 1; i >= 0; --i) {
        float dx = player.pos.x - obstacles[i].pos.x; float dy = player.pos.y - obstacles[i].pos.y;
        float distSq = dx * dx + dy * dy;
        float leniencyDist = (player.radius + obstacles[i].radius) - 5.0f;

        if (leniencyDist > 0.0f && distSq < (leniencyDist * leniencyDist)) {
            if (hasShield) {
                if (sfxShield.frameCount > 0) PlaySound(sfxShield);
                hasShield = false; shakeIntensity = 15.0f;
                spawnParticles(obstacles[i].pos, 40, obstacles[i].color, 1.5f, static_cast<ExplosionType>(gameData.equippedExplosion));
                spawnFloatingText(player.pos, "DEFLECTED!", COLOR_SHIELD);
                obstacles[i] = obstacles.back(); obstacles.pop_back();
            } else {
                triggerDeath();
            }
            return;
        }
    }
}

void GameScreen::triggerDeath() {
    if (sfxDeath.frameCount > 0) PlaySound(sfxDeath);
    player.active = false; shakeIntensity = 30.0f;
    currentState = GameState::GAMEOVER; deathTimer = 2.0f; bossWarningTimer = 0.0f;

    gameData.highscore = std::max(score, gameData.highscore);
    gameData.totalCoins += sessionCoins;
    SaveGameData(gameData);

    Color4f pColor = SHOP_COLORS[gameData.equippedColor];
    ExplosionType expType = static_cast<ExplosionType>(gameData.equippedExplosion);

    spawnParticles(player.pos, 80, pColor, 1.0f, expType);
    spawnParticles(player.pos, 40, { 1.0f, 1.0f, 0.8f, 1.0f }, 1.2f, expType);
}

void GameScreen::spawnParticles(Vec2f pos, int count, Color4f baseColor, float speedMult, ExplosionType expType) {
    if (!gameData.particlesEnabled) return;
    for (int i = 0; i < count; ++i) {
        float angle = randFloat(0.0f, 6.283f);
        float speed = randFloat(100.0f, 600.0f) * speedMult;
        Particle p; p.pos = pos; p.vel = { std::cosf(angle) * speed, std::sinf(angle) * speed };
        p.color = baseColor; p.size = randFloat(2.0f, 6.0f);
        p.maxLife = p.life = randFloat(0.5f, 1.5f); p.friction = 0.92f;
        if (expType == ExplosionType::Normal) p.pType = 0;
        else if (expType == ExplosionType::Rings) { p.pType = 1; p.size = randFloat(10.0f, 25.0f); }
        else if (expType == ExplosionType::Geometry) { p.pType = (randFloat(0.0f, 1.0f) > 0.5f) ? 1 : 2; p.size = randFloat(6.0f, 15.0f); }
        particles.push_back(p);
    }
}

void GameScreen::spawnFloatingText(Vec2f pos, const std::string& text, Color4f color) {
    floatingTexts.push_back({ pos, text, color, 1.5f, 1.5f });
}

void GameScreen::drawPlayerShape(const Renderer& r, float x, float y, float radius, Color4f color, SkinShape shape, float rotation) const {
    Color4f glow2 = color; glow2.a *= 0.15f; Color4f glow1 = color; glow1.a *= 0.40f; Color4f core = {1.0f, 1.0f, 1.0f, color.a};
    if (shape == SkinShape::Square) {
        r.drawRotatedRect(x, y, radius * 3.5f, radius * 3.5f, rotation * 100.0f, glow2);
        r.drawRotatedRect(x, y, radius * 2.5f, radius * 2.5f, rotation * 100.0f, glow1);
        r.drawRotatedRect(x, y, radius * 1.5f, radius * 1.5f, rotation * 100.0f, core);
    } else if (shape == SkinShape::Triangle) {
        r.drawPoly(x, y, 3, radius * 2.5f, rotation * 100.0f, glow2);
        r.drawPoly(x, y, 3, radius * 1.8f, rotation * 100.0f, glow1);
        r.drawPoly(x, y, 3, radius * 1.2f, rotation * 100.0f, core);
    } else {
        r.drawCircle(x, y, radius * 2.5f, glow2);
        r.drawCircle(x, y, radius * 1.8f, glow1);
        r.drawCircle(x, y, radius * 0.8f, core);
    }
}

void GameScreen::draw(Renderer& renderer) {
    float cx = width / 2.0f;
    float safeTop = 15.0f;

    float currentShakeX = (shakeIntensity > 0.0f) ? std::sinf(shakeIntensity * 145.0f) * shakeIntensity : 0.0f;
    float currentShakeY = (shakeIntensity > 0.0f) ? std::cosf(shakeIntensity * 123.0f) * shakeIntensity : 0.0f;

    float cameraZoom = 1.0f;
    if (currentState == GameState::PLAYING && boss.active) cameraZoom = 1.0f + (smoothedAmplitude * 0.15f);

    renderer.beginFrame(currentShakeX, currentShakeY, cameraZoom);

    // --- EFECT FUNDAL PERICOL FAZA 3 BOSS ---
    if (boss.active && boss.timer <= 60.0f) {
        float pulse = 0.15f + 0.1f * std::sin(globalTime * 5.0f);
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{1.0f, 0.0f, 0.0f, pulse});
    }

    if (boss.active) {
        float pulseAlpha = 0.15f + (smoothedAmplitude * 0.4f);
        if (pulseAlpha > 0.6f) pulseAlpha = 0.6f;
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{1.0f, 0.0f, 0.0f, pulseAlpha});
    }

    if (currentState == GameState::PLAYING) {
        float pulseWidth = smoothedAmplitude * 250.0f;
        Color4f barColor;
        barColor.r = 0.5f + 0.5f * std::sinf(score * 0.01f);
        barColor.g = 0.5f + 0.5f * std::sinf(score * 0.01f + 2.09f);
        barColor.b = 0.5f + 0.5f * std::sinf(score * 0.01f + 4.18f);

        if (boss.active || bossWarningTimer > 0.0f) barColor = {1.0f, 0.2f, 0.2f, 1.0f};

        barColor.a = 0.4f + (smoothedAmplitude * 4.0f);
        if (barColor.a > 0.9f) barColor.a = 0.9f;
        Color4f coreColor = barColor; coreColor.a = 1.0f;

        renderer.drawRect(0, 0, 10.0f + pulseWidth, (float)height, barColor);
        renderer.drawRect((float)width - 10.0f - pulseWidth, 0, 10.0f + pulseWidth, (float)height, barColor);
        renderer.drawRect(0, 0, 4.0f + pulseWidth * 0.4f, (float)height, coreColor);
        renderer.drawRect((float)width - 4.0f - pulseWidth * 0.4f, 0, 4.0f + pulseWidth * 0.4f, (float)height, coreColor);
    }

    for (const auto& s : backgroundSparks) {
        Color4f c = slowMoTimer > 0.0f ? COLOR_TIMEWARP : SHOP_COLORS[gameData.equippedColor]; c.a = 0.2f;
        renderer.drawRect(s.pos.x, s.pos.y, s.size, s.size * 5.0f, c);
    }

    if (gameData.particlesEnabled) {
        for (const auto& p : particles) {
            Color4f c = p.color; c.a = p.life / p.maxLife;
            if (p.pType == 0) renderer.drawCircle(p.pos.x, p.pos.y, p.size * c.a, c);
            else if (p.pType == 1) { float s = p.size * (2.0f - c.a); renderer.drawHollowRect(p.pos.x - s/2, p.pos.y - s/2, s, s, 2.0f, c); }
            else if (p.pType == 2) renderer.drawPoly(p.pos.x, p.pos.y, 3, p.size * c.a, p.life * 100.0f, c);
        }
    }

    for (const auto& mark : playerTrail) {
        float alpha = mark.life / 0.25f; Color4f pColor = SHOP_COLORS[gameData.equippedColor]; pColor.a = alpha * 0.4f;
        drawPlayerShape(renderer, mark.pos.x, mark.pos.y, player.radius * (0.5f + (alpha * 0.5f)), pColor, static_cast<SkinShape>(gameData.equippedShape), globalTime);
    }

    if (player.active) {
        drawPlayerShape(renderer, player.pos.x, player.pos.y, player.radius, SHOP_COLORS[gameData.equippedColor], static_cast<SkinShape>(gameData.equippedShape), globalTime);
        if (hasShield) renderer.drawRotatedRect(player.pos.x, player.pos.y, player.radius * 3.2f, player.radius * 3.2f, player.pos.x * 2.0f, Color4f{ COLOR_SHIELD.r, COLOR_SHIELD.g, COLOR_SHIELD.b, 0.4f });
    }

    // --- DESENARE BOSS ---
    if (boss.active) {
        renderer.drawPoly(boss.pos.x, boss.pos.y, 6, 45.0f, globalTime * 50.0f, COLOR_WARNING);
        renderer.drawGlowCircle(boss.pos.x, boss.pos.y, 20.0f, Color4f{1,1,1,1});

        // Desenăm mâinile pentru faza 1
        if (boss.timer > 80.0f) {
            renderer.drawCircle(boss.pos.x - 80.0f, boss.pos.y, 15.0f, COLOR_ENEMY_H);
            renderer.drawCircle(boss.pos.x + 80.0f, boss.pos.y, 15.0f, COLOR_ENEMY_H);
        }
    }

    // --- DESENARE ZONE BOSS ---
    for (const auto& z : bossZones) {
        float prog = 1.0f - (z.timer / z.maxTimer);
        if (z.isSafe) {
            renderer.drawCircle(z.pos.x, z.pos.y, z.radius, Color4f{0.1f, 0.9f, 0.3f, 0.3f});
            renderer.drawCircle(z.pos.x, z.pos.y, z.radius * prog, Color4f{0.1f, 0.9f, 0.3f, 0.6f});
            renderer.drawString("SAFE", z.pos.x, z.pos.y, 28, Color4f{1,1,1,1}, true);
        } else {
            float blink = 0.2f + 0.4f * std::abs(std::sin(z.timer * 15.0f));
            renderer.drawCircle(z.pos.x, z.pos.y, z.radius, Color4f{1.0f, 0.0f, 0.0f, blink});
            renderer.drawCircle(z.pos.x, z.pos.y, z.radius * prog, Color4f{1.0f, 0.0f, 0.0f, 0.6f});
            renderer.drawString("!", z.pos.x, z.pos.y, 40, Color4f{1,0,0,1}, true);
        }
    }

    // --- DESENARE PUTERI DIFFERENȚIATE ---
    for (const auto& c : collectibles) {
        float s = c.radius * 2.2f;

        if (c.type == ItemType::COIN) {
            renderer.drawRotatedRect(c.pos.x, c.pos.y, s, s, c.rotation, COLOR_COIN);
            renderer.drawCircle(c.pos.x, c.pos.y, c.radius * 0.5f, Color4f{ 1,1,1,0.8f });
        }
        else if (c.type == ItemType::SHIELD) {
            // Desenăm un scut veritabil
            renderer.drawPoly(c.pos.x, c.pos.y + c.radius*0.3f, 3, c.radius * 1.5f, 180.0f, COLOR_SHIELD);
            renderer.drawCircle(c.pos.x, c.pos.y - c.radius*0.2f, c.radius*1.15f, COLOR_SHIELD);
            renderer.drawCircle(c.pos.x, c.pos.y, c.radius*0.4f, Color4f{1,1,1,0.9f});
        }
        else if (c.type == ItemType::TIMEWARP) {
            // Desenăm un ceas
            renderer.drawCircle(c.pos.x, c.pos.y, c.radius * 1.3f, COLOR_TIMEWARP);
            renderer.drawCircle(c.pos.x, c.pos.y, c.radius * 1.0f, COLOR_BG);

            float a1 = globalTime * 4.0f;
            float a2 = globalTime * 0.5f;
            renderer.drawRotatedRect(c.pos.x + std::cos(a1)*c.radius*0.4f, c.pos.y + std::sin(a1)*c.radius*0.4f, 3.0f, c.radius, a1*57.29f, COLOR_TIMEWARP);
            renderer.drawRotatedRect(c.pos.x + std::cos(a2)*c.radius*0.2f, c.pos.y + std::sin(a2)*c.radius*0.2f, 4.0f, c.radius*0.6f, a2*57.29f, COLOR_TIMEWARP);
        }
    }

    // --- DESENARE INAMICI DIFERENȚIAȚI ---
    for (const auto& obs : obstacles) {
        float s = obs.radius * 2.0f;

        if (obs.type == EnemyType::NORMAL) {
            // Pătrat Roșu
            renderer.drawRotatedRect(obs.pos.x, obs.pos.y, s, s, obs.rotation, obs.color);
            renderer.drawRotatedRect(obs.pos.x, obs.pos.y, s*0.5f, s*0.5f, obs.rotation, Color4f{1,1,1,0.8f});
        }
        else if (obs.type == EnemyType::SINEWAVE) {
            // Cerc Mov
            renderer.drawCircle(obs.pos.x, obs.pos.y, obs.radius, obs.color);
            renderer.drawCircle(obs.pos.x, obs.pos.y, obs.radius*0.5f, Color4f{1,1,1,0.8f});
        }
        else if (obs.type == EnemyType::HOMING) {
            // Triunghi Portocaliu
            renderer.drawPoly(obs.pos.x, obs.pos.y, 3, obs.radius * 1.4f, obs.rotation, obs.color);
            renderer.drawPoly(obs.pos.x, obs.pos.y, 3, obs.radius * 0.6f, obs.rotation, Color4f{1,1,1,0.8f});
        }
    }

    for (const auto& ft : floatingTexts) {
        Color4f c = ft.color; c.a = ft.life / ft.maxLife;
        renderer.drawString(ft.text, ft.pos.x, ft.pos.y, 25, c, true);
    }

    if (currentState == GameState::PLAYING) {
        if (bossWarningTimer > 0.0f) {
            float flash = (std::sinf(bossWarningTimer * 20.0f) + 1.0f) * 0.5f;
            renderer.drawRect(0, 0, (float)width, (float)height, Color4f{1.0f, 0.0f, 0.0f, flash * 0.3f});
            renderer.drawString("WARNING!", cx, height / 2.0f - 40.0f, 50, Color4f{1.0f, 0.2f, 0.2f, flash}, true);
            renderer.drawString("BOSS APPROACHING", cx, height / 2.0f + 20.0f, 30, Color4f{1.0f, 0.2f, 0.2f, flash}, true);
        }

        renderer.drawGlassCard(10.0f, safeTop, (float)width - 20.0f, 70.0f, 0.2f);
        renderer.drawString("SCORE: " + std::to_string(score), 30.0f, safeTop + 10.0f, 25, Color4f{ 1,1,1,1 });
        renderer.drawString("COINS: " + std::to_string(sessionCoins), 30.0f, safeTop + 45.0f, 18, COLOR_COIN);

        float barWidth = 180.0f; float fillWidth = (progress / 500.0f) * barWidth;
        renderer.drawRect(cx - barWidth / 2.0f + 20, safeTop + 25.0f, barWidth, 12.0f, Color4f{ 0.2f, 0.2f, 0.2f, 0.5f });
        renderer.drawRect(cx - barWidth / 2.0f + 20, safeTop + 25.0f, fillWidth, 12.0f, SHOP_COLORS[gameData.equippedColor]);
        renderer.drawString("x" + std::to_string(multiplier), cx + barWidth / 2.0f + 40.0f, safeTop + 15.0f, 25, SHOP_COLORS[gameData.equippedColor]);

        renderer.drawGlassCard((float)width - 110.0f, safeTop + 10.0f, 95.0f, 50.0f, 0.3f);
        renderer.drawString("PAUSE", (float)width - 62.5f, safeTop + 25.0f, 18, Color4f{ 1,1,1,1 }, true);

        if (boss.active) {
            int m = std::max(0, (int)boss.timer) / 60; int s = std::max(0, (int)boss.timer) % 60;
            std::string timeStr = "BOSS: " + std::string(m < 10 ? "0" : "") + std::to_string(m) + ":" + std::string(s < 10 ? "0" : "") + std::to_string(s);
            renderer.drawString(timeStr, cx, safeTop + 100.0f, 35, Color4f{1.0f, 0.2f, 0.2f, 0.6f + (std::sinf(globalTime * 15.0f) * 0.4f)}, true);
        }

        if (slowMoTimer > 0.0f) renderer.drawString("TIME WARP", cx, safeTop + 140.0f, 25, COLOR_TIMEWARP, true);
        if (gameData.showFPS) renderer.drawString("FPS: " + std::to_string(currentFPS), 15.0f, height - 30.0f, 20, Color4f{1,1,1,0.5f});
    }

    if (screenFlashTimer > 0.0f) renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 1.0f, 1.0f, 1.0f, screenFlashTimer * 0.3f });

    if (currentState == GameState::MENU) {
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 0.0f, 0.0f, 0.0f, 0.7f });
        renderer.drawString("NEON DODGE", cx, height / 2.0f - 180.0f + std::sinf(globalTime * 3.0f) * 15.0f, 60, SHOP_COLORS[gameData.equippedColor], true);
        renderer.drawString("HIGH SCORE: " + std::to_string(gameData.highscore), cx, height / 2.0f - 100.0f, 22, Color4f{ 1,1,1,1 }, true);
        renderer.drawString("TOTAL COINS: " + std::to_string(gameData.totalCoins), cx, height / 2.0f - 70.0f, 22, COLOR_COIN, true);

        renderer.drawGlassCard(cx - 130, height / 2.0f - 40, 260, 65, 0.3f);
        renderer.drawString("PLAY", cx, height / 2.0f - 22, 26, Color4f{ 1,1,1,1 }, true);

        renderer.drawGlassCard(cx - 130, height / 2.0f + 45, 260, 65, 0.3f);
        renderer.drawString("SHOP", cx, height / 2.0f + 63, 26, COLOR_COIN, true);

        renderer.drawGlassCard(cx - 130, height / 2.0f + 130, 260, 65, 0.3f);
        renderer.drawString("SETTINGS", cx, height / 2.0f + 148, 26, Color4f{ 1,1,1,1 }, true);

        renderer.drawGlassCard(width - 70, safeTop, 50, 50, 0.3f);
        renderer.drawString("?", width - 45, safeTop + 12, 28, Color4f{ 1,1,1,1 }, true);
    }
    else if (currentState == GameState::TUTORIAL) {
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 0.0f, 0.0f, 0.0f, 0.95f });
        renderer.drawString("HOW TO PLAY", cx, safeTop + 30.0f, 40, COLOR_SHIELD, true);
        renderer.drawGlassCard(30.0f, safeTop + 100.0f, width - 60.0f, height - safeTop - 300.0f, 0.2f);

        if (tutorialPage == 0) {
            renderer.drawString("CONTROLS", cx, safeTop + 140.0f, 30, SHOP_COLORS[gameData.equippedColor], true);
            renderer.drawString("HOLD and drag the ship to", cx, safeTop + 220.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("easily dodge blocks and enemies.", cx, safeTop + 250.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("TAP anywhere in empty space to", cx, safeTop + 320.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("instantly teleport the ship in a pinch!", cx, safeTop + 350.0f, 18, Color4f{ 1,1,1,1 }, true);
        } else if (tutorialPage == 1) {
            renderer.drawString("ENEMIES", cx, safeTop + 140.0f, 30, COLOR_ENEMY_N, true);
            renderer.drawString("RED (SQUARE): Falls fast.", cx, safeTop + 220.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("PURPLE (CIRCLE): Moves in waves.", cx, safeTop + 280.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("ORANGE (TRIANGLE): Chases you!", cx, safeTop + 340.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("Each will destroy you on contact.", cx, safeTop + 400.0f, 18, Color4f{ 1,1,1,0.5f }, true);
        } else if (tutorialPage == 2) {
            renderer.drawString("POWER-UPS", cx, safeTop + 140.0f, 30, COLOR_COIN, true);
            renderer.drawString("COINS (Yellow): Used in the shop.", cx, safeTop + 220.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("SHIELD (Cyan): Gives you an extra life", cx, safeTop + 280.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("and destroys the block that hit you.", cx, safeTop + 310.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("TIME WARP (Clock): Completely slows", cx, safeTop + 370.0f, 18, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("down the game for a short moment!", cx, safeTop + 400.0f, 18, Color4f{ 1,1,1,1 }, true);
        }

        renderer.drawString("PAGE " + std::to_string(tutorialPage + 1) + "/3", cx, height - 190.0f, 20, Color4f{ 1,1,1,0.5f }, true);

        if (tutorialPage > 0) {
            renderer.drawGlassCard(cx - 140, height - 160, 130, 50, 0.3f);
            renderer.drawString("PREV", cx - 75, height - 146, 22, Color4f{ 1,1,1,1 }, true);
        }
        if (tutorialPage < 2) {
            renderer.drawGlassCard(cx + 10, height - 160, 130, 50, 0.3f);
            renderer.drawString("NEXT", cx + 75, height - 146, 22, Color4f{ 1,1,1,1 }, true);
        }

        renderer.drawGlassCard(cx - 130, height - 90, 260, 60, 0.3f);
        renderer.drawString("CLOSE", cx, height - 73, 24, Color4f{ 1,1,1,1 }, true);
    }
    else if (currentState == GameState::SETTINGS) {
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 0.0f, 0.0f, 0.0f, 0.95f });
        renderer.drawGlassCard(cx - 170, height / 2.0f - 200, 340, 460, 0.2f);
        renderer.drawString("SETTINGS", cx, height / 2.0f - 170, 40, SHOP_COLORS[gameData.equippedColor], true);

        renderer.drawGlassCard(cx - 160, height / 2.0f - 120, 320, 50, 0.2f);
        std::string partText = gameData.particlesEnabled ? "ON" : "OFF";
        renderer.drawString("PARTICLES: " + partText, cx, height / 2.0f - 105, 20, Color4f{1,1,1,1}, true);

        renderer.drawGlassCard(cx - 160, height / 2.0f - 50, 320, 50, 0.2f);
        std::string fpsText = gameData.showFPS ? "ON" : "OFF";
        renderer.drawString("SHOW FPS: " + fpsText, cx, height / 2.0f - 35, 20, Color4f{1,1,1,1}, true);

        renderer.drawString("MENU MUSIC", cx, height / 2.0f + 25, 18, Color4f{1,1,1,1}, true);
        renderer.drawRect(cx - 130, height / 2.0f + 45, 260, 10, Color4f{0.3f, 0.3f, 0.3f, 1.0f});
        renderer.drawRect(cx - 130, height / 2.0f + 45, 260.0f * gameData.menuMusicVolume, 10, SHOP_COLORS[gameData.equippedColor]);
        renderer.drawCircle(cx - 130 + 260.0f * gameData.menuMusicVolume, height / 2.0f + 50, 12, Color4f{1,1,1,1});

        renderer.drawString("SFX VOLUME", cx, height / 2.0f + 85, 18, Color4f{1,1,1,1}, true);
        renderer.drawRect(cx - 130, height / 2.0f + 105, 260, 10, Color4f{0.3f, 0.3f, 0.3f, 1.0f});
        renderer.drawRect(cx - 130, height / 2.0f + 105, 260.0f * gameData.sfxVolume, 10, SHOP_COLORS[gameData.equippedColor]);
        renderer.drawCircle(cx - 130 + 260.0f * gameData.sfxVolume, height / 2.0f + 110, 12, Color4f{1,1,1,1});

        renderer.drawGlassCard(cx - 160, height / 2.0f + 160, 320, 50, 0.2f);
        renderer.drawString("RESET SAVE DATA", cx, height / 2.0f + 175, 20, COLOR_ENEMY_N, true);

        renderer.drawGlassCard(cx - 130, height - 120, 260, 65, 0.3f);
        renderer.drawString("BACK TO MENU", cx, height - 102, 24, Color4f{ 1,1,1,1 }, true);
    }
    else if (currentState == GameState::SHOP) {
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 0.0f, 0.0f, 0.0f, 0.95f });
        renderer.drawString("- NEON SHOP -", cx, safeTop + 15, 35, COLOR_COIN, true);
        renderer.drawString("COINS: " + std::to_string(gameData.totalCoins), cx, safeTop + 45, 20, COLOR_COIN, true);

        renderer.drawGlassCard(cx - 190, safeTop + 70, 120, 45, (shopCategory == 0) ? 0.6f : 0.2f);
        renderer.drawString("SHAPES", cx - 130, safeTop + 83, 18, (shopCategory == 0) ? COLOR_COIN : Color4f{1,1,1,1}, true);

        renderer.drawGlassCard(cx - 60, safeTop + 70, 120, 45, (shopCategory == 1) ? 0.6f : 0.2f);
        renderer.drawString("COLORS", cx, safeTop + 83, 18, (shopCategory == 1) ? COLOR_COIN : Color4f{1,1,1,1}, true);

        renderer.drawGlassCard(cx + 70, safeTop + 70, 120, 45, (shopCategory == 2) ? 0.6f : 0.2f);
        renderer.drawString("EFFECTS", cx + 130, safeTop + 83, 18, (shopCategory == 2) ? COLOR_COIN : Color4f{1,1,1,1}, true);

        renderer.drawGlassCard(20, height / 2 - 35, 60, 60, 0.3f);
        renderer.drawString("<", 50, height / 2 - 18, 35, Color4f{1,1,1,1}, true);
        renderer.drawGlassCard(width - 80, height / 2 - 35, 60, 60, 0.3f);
        renderer.drawString(">", width - 50, height / 2 - 18, 35, Color4f{1,1,1,1}, true);

        std::string itemName = "";
        int price = 0; int bitMask = 1 << shopSelection; bool isUnlocked = false; bool isEquipped = false;

        if (shopCategory == 0) {
            isUnlocked = gameData.unlockedShapes & bitMask; isEquipped = gameData.equippedShape == shopSelection;
            itemName = (shopSelection==0) ? "CIRCLE" : ((shopSelection==1) ? "SQUARE" : "TRIANGLE");
            price = (shopSelection==1) ? 500 : ((shopSelection==2) ? 1000 : 0);
            drawPlayerShape(renderer, cx, height / 2, 45.0f, SHOP_COLORS[gameData.equippedColor], static_cast<SkinShape>(shopSelection), globalTime);
        } else if (shopCategory == 1) {
            isUnlocked = gameData.unlockedColors & bitMask; isEquipped = gameData.equippedColor == shopSelection;
            itemName = (shopSelection==0) ? "CYAN" : ((shopSelection==1) ? "PINK" : "GOLD");
            price = (shopSelection==1) ? 300 : ((shopSelection==2) ? 600 : 0);
            drawPlayerShape(renderer, cx, height / 2, 45.0f, SHOP_COLORS[shopSelection], static_cast<SkinShape>(gameData.equippedShape), globalTime);
        } else if (shopCategory == 2) {
            isUnlocked = gameData.unlockedExplosions & bitMask; isEquipped = gameData.equippedExplosion == shopSelection;
            itemName = (shopSelection==0) ? "BASIC" : ((shopSelection==1) ? "RINGS" : "SHARDS");
            price = (shopSelection==1) ? 800 : ((shopSelection==2) ? 1500 : 0);
            float previewTime = fmodf(globalTime, 1.5f); float s = previewTime * 80.0f;
            Color4f c = SHOP_COLORS[gameData.equippedColor]; c.a = 1.0f - (previewTime / 1.5f);
            if (shopSelection == 0) renderer.drawCircle(cx, height / 2, s/2.0f, c);
            else if (shopSelection == 1) renderer.drawHollowRect(cx - s/2.0f, height / 2 - s/2.0f, s, s, 5.0f, c);
            else renderer.drawPoly(cx, height / 2, 3, s/1.5f, globalTime * 100.0f, c);
        }

        renderer.drawString(itemName, cx, height / 2 + 80, 28, Color4f{1,1,1,1}, true);

        renderer.drawGlassCard(cx - 120, height / 2 + 130, 240, 60, 0.3f);
        if (isEquipped) renderer.drawString("EQUIPPED", cx, height / 2 + 148, 22, COLOR_SHIELD, true);
        else if (isUnlocked) renderer.drawString("EQUIP", cx, height / 2 + 148, 22, Color4f{1,1,1,1}, true);
        else renderer.drawString("BUY: " + std::to_string(price), cx, height / 2 + 148, 22, (gameData.totalCoins >= price) ? COLOR_COIN : COLOR_ENEMY_N, true);

        renderer.drawGlassCard(cx - 120, height - 100, 240, 60, 0.3f);
        renderer.drawString("BACK TO MENU", cx, height - 82, 20, Color4f{ 1,1,1,1 }, true);
    }
    else if (currentState == GameState::PAUSED) {
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 0.0f, 0.0f, 0.0f, 0.7f });
        renderer.drawGlassCard(cx - 160, height / 2.0f - 130, 320, 300, 0.2f);
        renderer.drawString("PAUSED", cx, height / 2.0f - 100, 40, SHOP_COLORS[gameData.equippedColor], true);

        renderer.drawGlassCard(cx - 130, height / 2.0f - 50, 260, 60, 0.3f);
        renderer.drawString("RESUME", cx, height / 2.0f - 34, 26, Color4f{ 1,1,1,1 }, true);

        renderer.drawGlassCard(cx - 130, height / 2.0f + 30, 260, 60, 0.3f);
        renderer.drawString("MAIN MENU", cx, height / 2.0f + 46, 24, Color4f{ 1,1,1,1 }, true);

        renderer.drawString("GAME MUSIC", cx, height / 2.0f + 115, 18, Color4f{1,1,1,1}, true);
        renderer.drawRect(cx - 130, height / 2.0f + 135, 260, 10, Color4f{0.3f, 0.3f, 0.3f, 1.0f});
        renderer.drawRect(cx - 130, height / 2.0f + 135, 260.0f * gameData.gameMusicVolume, 10, SHOP_COLORS[gameData.equippedColor]);
        renderer.drawCircle(cx - 130 + 260.0f * gameData.gameMusicVolume, height / 2.0f + 140, 12, Color4f{1,1,1,1});
    }
    else if (currentState == GameState::GAMEOVER) {
        renderer.drawRect(0, 0, (float)width, (float)height, Color4f{ 0.0f, 0.0f, 0.0f, 0.85f });
        if (deathTimer <= 0.0f) {
            renderer.drawString("GAME OVER", cx, height / 2.0f - 80, 55, COLOR_ENEMY_N, true);
            renderer.drawString("FINAL SCORE: " + std::to_string(score), cx, height / 2.0f + 10, 35, Color4f{ 1,1,1,1 }, true);
            renderer.drawString("COINS: " + std::to_string(sessionCoins), cx, height / 2.0f + 60, 25, COLOR_COIN, true);

            renderer.drawGlassCard(cx - 150, height / 2.0f + 120, 300, 60, 0.3f);
            renderer.drawString("TAP TO RESTART", cx, height / 2.0f + 137, 24, Color4f{ 1,1,1,1 }, true);
        }
    }
}