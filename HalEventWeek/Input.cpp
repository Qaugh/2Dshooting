#include "common.h"
#include "audio_player.h"
//	     入力処理

static void EndBossCleanup(GameState& game, const Assets& assets)
{
	//	ボス本体
	game.boss.alive = false;
	game.bossActive = false;
	game.bossIntro = false;

	//	ボス弾
	for (int i = 0; i < BOSS_BULLET_MAX; i++)
	{
		game.bossBullets[i].active = false;
		game.bossBullets[i].fx = game.bossBullets[i].fy = 0.0f;
		game.bossBullets[i].vx = game.bossBullets[i].vy = 0.0f;
		game.bossBullets[i].x  = game.bossBullets[i].y = 0;
	}

	//	移動・射撃タイマー
	game.bossMoveDirY    = +1;
	game.bossOscPhase    = 0.0f;
	game.bossTimerSpread = BOSS_FIRE_INTERVAL_SP;
	game.bossTimerAimed  = BOSS_FIRE_INTERVAL_AIM;

	//	テレポート演出の後始末
	game.teleportRequest = false;
	if (game.tpActive)
	{
		game.tpActive = false;
		game.tpPhase  = 0;
		game.tpTimer  = 0;
	}

	//	BGM
	StopBGM();
	//	StageSelectに戻った瞬間にBGMを確実に再開できる
	game.bgm = BgmKind::None;
}

void Input(GameState& game, const Assets& assets)
{
	switch (game.scene)
	{
		//	==========タイトル画面での処理	==========
	case Scene::Title:
	{
		GetKeyAll();

		//!	Enterキーでステージセレクトへ
		if (ChkKeyEdge(PK_ENTER))	game.scene       = Scene::StageSelect;

		{
			//PlayBGM(L"sound\\bgm\\bgm.mp3");

			// まだ通常BGMでなければ開始（ループ想定）
			if (game.bgm != BgmKind::Normal)
			{
				PlayBGM(BGM_PATH_NORMAL);
				game.bgm = BgmKind::Normal;
			}

		}

		//!	ESCでゲーム終了
		if (ChkKeyEdge(PK_ESC))		game.requestQuit = true;

		break;
	}

	//	==========ステージセレクト中の処理==========
	case Scene::StageSelect:
	{
		//!	上下でステージ変更
		GetKeyAll();

		// タイトル以外から戻って来た場合など、BGMが止まっていれば通常BGMを再開
		if (game.bgm != BgmKind::Normal)
		{
			PlayBGM(BGM_PATH_NORMAL);
			game.bgm = BgmKind::Normal;
			
		}


		if (ChkKeyEdge(PK_W))	game.stageNo = 0, game.cursorY = 260;
		if (ChkKeyEdge(PK_S))	game.stageNo = 1, game.cursorY = 300;

		//!	ヘルプ画面表示
		if (ChkKeyEdge(PK_TAB))	game.showHelp = !game.showHelp;
		
		if (game.showHelp) break;

		//!	ゲーム開始時に諸々リセット
		if (ChkKeyEdge(PK_F))
		{
			//	シーン、背景、射撃、リロード
			{
				game.scene = Scene::Play;
				game.bgX = 0;
				game.fireRequest = false;
				game.reloadRequest = false;
			}

			//	HP
			{
				game.playerHP        = PLAYER_MAX_HP;
				game.invincibleTimer = 0;
				game.isInvincible    = false;
				game.lastHPHud       = -1;
			}

			//	スコア
			{
				game.score        = 0;
				game.lastScoreHud = -1;
			}

			//	経過時間
			{
				game.playTimeFrames = 0;
				game.lastTimeHud    = -1;
				game.stageKillCount = 0;
				game.stageCleared   = false;
			}

			//	自機の位置
			{
				game.playerX = 50;
				game.playerY = (PLAY_Y_MIN + PLAY_Y_MAX) / 2 - assets.player->height / 2;
			}

			//	残弾数
			{
				game.ammoNormal   = MAX_AMMO_NORMAL;
				game.ammoBeam     = MAX_AMMO_BEAM;
				game.ammoSpread   = MAX_AMMO_SPREAD;
				game.fireCooldown = 0;
			}
			
			//	新弾システム
			{
				for (int i = 0; i < PLAYER_BULLET_MAX; i++)
				{
					game.playerBullets[i].active = false;
				}
				game.currentWeapon = WeaponType::Normal;
				game.lastWeaponHud = -1;
			}

			//	敵機の変数
			{
				for (int i = 0; i < ENEMY_MAX; i++)
				{
					//	種類：半分を上下移動型に
					game.enemies[i].type = (i % 2 == 0) ? EnemyType::Normal : EnemyType::Vertical;

					//	画像サイズ(種類に応じて)
					Bmp* ebmp = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
					int eh = ebmp ? ebmp->height : 0;

					// 初期状態は出さない
					game.enemies[i].x          = 0;
					game.enemies[i].y          = 0;
					game.enemies[i].alive      = false;     // ★ 変更：最初はfalse
					game.enemies[i].type       = (i % 2 == 0) ? EnemyType::Normal : EnemyType::Vertical;
					game.enemies[i].shootTimer = 0;
					game.enemies[i].vy         = 0;
				}
				// 敵スポーンタイマー初期化
				game.enemySpawnTimer = ENEMY_SPAWN_MIN + rand() % (ENEMY_SPAWN_MAX - ENEMY_SPAWN_MIN + 1); // ★ 追加
			}
		
			//	敵弾
			{
				for (int i = 0; i < ENEMY_BULLET_MAX; i++)
				{
					game.enemyBullets[i].x      = 0;
					game.enemyBullets[i].y      = 0;
					game.enemyBullets[i].active = false;
				}
			}
	
			//	岩・ピックアップアイテム初期化
			{
				for (int i = 0; i < ROCK_MAX; i++)
				{
					game.rocks[i] = { 0, 0, 0, false };
				}
				for (int i = 0; i < PICKUP_MAX; i++)
				{
					game.pickups[i].x      = 0;
					game.pickups[i].y      = 0;
					game.pickups[i].vy     = PICKUP_FALL_SPEED;
					game.pickups[i].active = false;
					game.pickups[i].type   = WeaponType::Normal;
				}
				//	初回スポーンタイマー(ランダム)
				game.rockSpawnTimer = ROCK_SPAWN_MIN + rand() % (ROCK_SPAWN_MAX - ROCK_SPAWN_MIN + 1);
			}
		}

		//!	ESCでゲーム終了
		if (ChkKeyEdge(PK_ESC))		game.scene = Scene::Title;
		break;
	}

	//	==========ゲームプレイ中の処理	==========
	case Scene::Play:
	{
		GetKeyAll();

		//!	ESCでポーズ画面
		if (ChkKeyEdge(PK_ESC))
		{
			game.scene = Scene::Pause;
			break;
		}

		//!	通常移動入力
		{
			game.inputX = 0;
			game.inputY = 0;
			if (ChkKeyPress(PK_UP)    || ChkKeyPress(PK_W))	game.inputY -= 1;
			if (ChkKeyPress(PK_DOWN)  || ChkKeyPress(PK_S))	game.inputY += 1;
			if (ChkKeyPress(PK_LEFT)  || ChkKeyPress(PK_A))	game.inputX -= 1;
			if (ChkKeyPress(PK_RIGHT) || ChkKeyPress(PK_D))	game.inputX += 1;
		}

		//!	TP要求
		{
			if (ChkKeyEdge(PK_SHIFT))
			{
				if (!game.tpActive && game.tpCDTimer <= 0)
				{
					game.teleportRequest = true;
				}
			}
		}

		//!	武器切り替え
		{
			if (ChkKeyEdge(PK_1))	game.currentWeapon = WeaponType::Normal;
			if (ChkKeyEdge(PK_2))	game.currentWeapon = WeaponType::ChargeBeam;
			if (ChkKeyEdge(PK_3))	game.currentWeapon = WeaponType::Spread;
		}

		//!	弾発射処理
		{
			if (ChkKeyEdge(PK_SP))	game.fireRequest = true;
		}

		//!	リロード
		{
			if (ChkKeyEdge(PK_R))	game.reloadRequest = true;
		}
		break;
	}

	//	==========ボス戦の処理==========
	case Scene::Boss:
	{
		GetKeyAll();

		//!	ESCでポーズ画面
		if (ChkKeyEdge(PK_ESC))
		{
			game.scene = Scene::Pause;
			break;
		}

		//!	通常移動入力
		{
			game.inputX = 0;
			game.inputY = 0;
			if (ChkKeyPress(PK_UP)    || ChkKeyPress(PK_W))	game.inputY -= 1;
			if (ChkKeyPress(PK_DOWN)  || ChkKeyPress(PK_S))	game.inputY += 1;
			if (ChkKeyPress(PK_LEFT)  || ChkKeyPress(PK_A))	game.inputX -= 1;
			if (ChkKeyPress(PK_RIGHT) || ChkKeyPress(PK_D))	game.inputX += 1;
		}

		//!	TP要求
		{
			if (ChkKeyEdge(PK_SHIFT))
			{
				if (!game.tpActive && game.tpCDTimer <= 0)
				{
					game.teleportRequest = true;
				}
			}
		}

		//!	武器切り替え
		{
			if (ChkKeyEdge(PK_1))	game.currentWeapon = WeaponType::Normal;
			if (ChkKeyEdge(PK_2))	game.currentWeapon = WeaponType::ChargeBeam;
			if (ChkKeyEdge(PK_3))	game.currentWeapon = WeaponType::Spread;
		}

		//!	弾発射処理
		{
			if (ChkKeyEdge(PK_SP))	game.fireRequest = true;
		}

		//!	リロード
		{
			if (ChkKeyEdge(PK_R))	game.reloadRequest = true;
		}
		break;
	}

	//	==========ポーズ画面の処理		==========
	case Scene::Pause:
	{
		GetKeyAll();

		//	Z か Space で再開
		if (ChkKeyEdge(PK_Z) || ChkKeyEdge(PK_SP))	game.scene = Scene::Play;
		//	Z か ESC でステージセレクト画面
		if (ChkKeyEdge(PK_X) || ChkKeyEdge(PK_ESC))	game.scene = Scene::StageSelect;

		break;
	}

	//	==========ゲームオーバー画面の処理==========
	case Scene::GameOver:
	{
		GetKeyAll();

		if (ChkKeyEdge(PK_ESC) == 1)	game.scene = Scene::StageSelect;

		{
			StopBGM();
		}
		break;
	}

	//	==========ゲームクリア画面の処理==========
	case Scene::GameClear:
	{
		GetKeyAll();
		if (ChkKeyEdge(PK_ESC))	game.scene = Scene::StageSelect;

			StopBGM();
		break;
	}

	}
}