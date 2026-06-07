#pragma once
#include"common.h"
void Game(GameState& game, Assets& assets);
void EndBossCleanup(GameState& game);	//Input.cppから呼ぶために公開
void HandlePlayerDeath(GameState& game);	//HP0→ゲームオーバー
void HandleBossDefeat(GameState& game);		//ボス撃破→ゲームクリア

//game.cppで使う関数
int AcquirePopupSlot(const Assets& assets);
inline void StartNormalReload(GameState& game);
inline bool Intersects(int, int, int, int, int, int, int, int);
inline void GetEnemyRect(const GameState&, const Assets&, int, int&, int&, int&, int&);
inline void GetRockRect(const GameState&, const Assets&, int, int&, int&, int&, int&);
int FindFreeBulletSlot(GameState&);
int FindFreeRockSlot(GameState&);
int FindFreePickupSlot(GameState&);
int FindFreeEnemySlot(GameState&);
int FindFreeBossSlot(const GameState&);
void SpawnRock(GameState&, const Assets&);
void SpawnEnemy(GameState&, const Assets&);
void SpawnPickup(GameState&, WeaponType, int, int);
void SpawnExplosion(GameState&, const Assets&, int, int);
void SpawnBossSpread(GameState&);
void SpawnBossAimed(GameState&, const Assets&);
void UpdateBossBullets(GameState&, const Assets&);
void ProcessFireRequest(GameState&, const Assets&);
void StartBoss(GameState&, const Assets&);
inline void DecideTeleportDir(GameState&);
inline void ComputeTeleportTarget(GameState&, const Assets&);