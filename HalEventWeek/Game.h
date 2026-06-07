#pragma once
#include"common.h"
void Game(GameState& game, Assets& assets);
void EndBossCleanup(GameState& game);	//Input.cppから呼ぶために公開
void HandlePlayerDeath(GameState& game);	//HP0→ゲームオーバー
void HandleBossDefeat(GameState& game);		//ボス撃破→ゲームクリア