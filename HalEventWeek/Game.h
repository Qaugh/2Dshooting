#pragma once
#include"common.h"
void Game(GameState& game, Assets& assets);
void EndBossCleanup(GameState& game);	//Input.cppから呼ぶために公開
void HandlePlayerDeath(GameState& game);	//HP0→ゲームオーバー
void HandleBossDefeat(GameState& game);		//ボス撃破→ゲームクリア

//game.cppで使う関数
static int AcquirePopupSlot(const Assets& assets);
static inline void StartNormalReload(GameState& game);
static inline bool Intersects(int, int, int, int, int, int, int, int);
static inline void GetEnemyRect(const GameState&, const Assets&, int, int&, int&, int&, int&);
static inline void GetRockRect(const GameState&, const Assets&, int, int&, int&, int&, int&);
static int FindFreeBulletSlot(GameState&);
static int FindFreeRockSlot(GameState&);
static int FindFreePickupSlot(GameState&);
static int FindFreeEnemySlot(GameState&);
static int FindFreeBossSlot(const GameState&);
static void SpawnRock(GameState&, const Assets&);
static void SpawnEnemy(GameState&, const Assets&);
static void SpawnPickup(GameState&, WeaponType, int, int);
static void SpawnExplosion(GameState&, const Assets&, int, int);
static void SpawnBossSpread(GameState&);
static void SpawnBossAimed(GameState&, const Assets&);
static void UpdateBossBullets(GameState&, const Assets&);
static void ProcessFireRequest(GameState&, const Assets&);
static void StartBoss(GameState&, const Assets&);
static inline void DecideTeleportDir(GameState&);
static inline void ComputeTeleportTarget(GameState&, const Assets&);