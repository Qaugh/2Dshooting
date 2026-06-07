#include "common.h"
#include "audio_player.h"
//	     初期化処理
void Init(GameState& game, Assets& assets)
{
	InitConioEx(SCREEN_WIDTH, SCREEN_HEIGHT, FONT_SIZE, FONT_SIZE, true);

	//	乱数
	srand((unsigned int)time(nullptr));

	AudioSystem_Init(); 
	
	//	画像の読み込み
	{
		assets.title               = LoadBmp("pic\\title.bmp"                  , true);
		assets.background[0]       = LoadBmp("pic\\background_00.bmp"          , true);
		assets.background[1]       = LoadBmp("pic\\background_01.bmp"          , true);
		assets.player              = LoadBmp("pic\\player.bmp"                 , true);
		assets.enemy01             = LoadBmp("pic\\enemy_00.bmp"               , true);
		assets.enemy02             = LoadBmp("pic\\enemy_01.bmp"               , true);
		assets.boss                = LoadBmp("pic\\boss.bmp"                   , true);
		assets.bullet              = LoadBmp("pic\\bullet_normal.bmp"          , true);
		assets.chargeBeam          = LoadBmp("pic\\bullet_beam.bmp"            , true);
		assets.spreadBullet        = LoadBmp("pic\\bullet_spread.bmp"          , true);
		assets.enemyBullet         = LoadBmp("pic\\enemy_bullet.bmp"           , true);
		assets.ammoIconNormal      = LoadBmp("pic\\ammo_full_normal.bmp"       , true);
		assets.ammoIconBeam        = LoadBmp("pic\\ammo_full_beam.bmp"         , true);
		assets.ammoIconSpread      = LoadBmp("pic\\ammo_full_spread.bmp"       , true);
		assets.ammoIconNormalEmpty = LoadBmp("pic\\ammo_empty_normal.bmp"      , true);
		assets.ammoIconBeamEmpty   = LoadBmp("pic\\ammo_empty_beam.bmp"        , true);
		assets.ammoIconSpreadEmpty = LoadBmp("pic\\ammo_empty_spread.bmp"      , true);
		assets.uiWeaponIconNormal  = LoadBmp("pic\\icon_normal.bmp"            , true);
		assets.uiWeaponIconBeam    = LoadBmp("pic\\icon_beam.bmp"              , true);
		assets.uiWeaponIconSpread  = LoadBmp("pic\\icon_spread.bmp"            , true);
		assets.uiWeaponSelectFrame = LoadBmp("pic\\icon_frame.bmp"             , true);
		assets.heartEmpty          = LoadBmp("pic\\heart_empty.bmp"            , true);
		assets.heartFull           = LoadBmp("pic\\heart_full.bmp"             , true);
		assets.rock                = LoadBmp("pic\\rock.bmp"                   , true);
		assets.pickupNormal        = LoadBmp("pic\\pickup_normal.bmp"          , true);
		assets.pickupBeam          = LoadBmp("pic\\pickup_beam.bmp"            , true);
		assets.pickupSpread        = LoadBmp("pic\\pickup_spread.bmp"          , true);
		assets.infoimg             = LoadBmp("pic\\info.bmp"                   , true);
		assets.stage_select        = MakeTextBmp(TEXT("STAGE SELECT")               , 42);
		assets.W_stage             = MakeTextBmp(TEXT("W : STAGE 1")                , 36);
		assets.S_stage             = MakeTextBmp(TEXT("S : STAGE 2")                , 36);
		assets.F_start             = MakeTextBmp(TEXT("F : START")                  , 36);
		assets.tab_info            = MakeTextBmp(TEXT("Tab : ShowInformation")      , 36);
		assets.arrow               = MakeTextBmp(TEXT(">")                          , 36);
		assets.dash_ready          = MakeTextBmp(TEXT("Dash : READY")               , 28);
		assets.dash_active         = MakeTextBmp(TEXT("Dash : ACTIVE")              , 28);
		assets.dash_n_active       = MakeTextBmp(TEXT("Dash : COOLDOWN")            , 28);
		assets.labelNormal         = MakeTextBmp(TEXT("NORMAL")                     , 16);
		assets.labelCharge         = MakeTextBmp(TEXT("CHARGE")                     , 16);
		assets.labelSpread         = MakeTextBmp(TEXT("SPREAD")                     , 16);
		assets.gameOver            = MakeTextBmp(TEXT("GAME OVER")                  , 48);
		assets.retry               = MakeTextBmp(TEXT("PRESS ESC TO SELECT STAGE")  , 24);
		assets.returnStageSelect   = MakeTextBmp(TEXT("ESC : STAGE SELECT")         , 48);
		assets.returnGame          = MakeTextBmp(TEXT("SPACE : CONTINUE")           , 36);
		assets.gameClear           = MakeTextBmp(TEXT("GAME CLEAR!")                , 36);

		//	爆発画像読み込みの初期化
		{
			const char* expFiles[] =
			{
				("pic\\explosion_00.bmp"),
				("pic\\explosion_01.bmp"),
				("pic\\explosion_02.bmp"),
				("pic\\explosion_03.bmp"),
			};
			int loaded = 0;
			for (int i = 0; i < (int)(sizeof(expFiles) / sizeof(expFiles[0])); i++)
			{
				Bmp* b = LoadBmp(expFiles[i], true);
				if (!b) break;
				assets.explosionFrames[loaded++] = b;
			}
			assets.explosionCount = loaded;
		}
		//	爆発配列の初期化
		{
			for (int i = 0; i < GameState::EXPLOSION_MAX; i++)
			{
				game.explosions[i] = { 0,0,0,0,false };
			}
		}


		//	tp画像読み込みの初期化
		{
			const char* tpFiles[TP_FRAMES_MAX] =
			{
				("pic\\warp_circle_00.bmp"),
				("pic\\warp_circle_01.bmp"),
				("pic\\warp_circle_02.bmp"),
				("pic\\warp_circle_03.bmp"),
			};
			int loaded = 0;
			for (int i = 0; i < TP_FRAMES_MAX; ++i)
			{
				Bmp* b = LoadBmp(tpFiles[i], true);
				if (b)
				{
					assets.tpCircle[loaded++] = b;
				}
				else
				{
					break; // 連番が切れたら終了
				}
			}
			assets.tpCircleCount = loaded;
		}
	}

	//	スコアHUD/ポップアップ画像
	{
		assets.scoreHud  = nullptr;
		assets.hpHud     = nullptr;
		assets.timeHud   = nullptr;
		assets.weaponHud = nullptr;
		assets.tpCdHud   = nullptr;
		for (int i = 0; i < GameState::POPUP_MAX; i++)	assets.popups[i] = nullptr;
	}

	//	合計スコア・ポップアップ
	{
		game.score      = 0;
		game.popupCount = 0;
		for (int i = 0; i < GameState::POPUP_MAX; i++)
		{
			game.popups[i] = { 0,0,0,0,-1 };
		}
	}

	//	右上HUD Bmp
	{
		game.lastScoreHud    = -1;		//	違う値にして最初に必ず生成されるようにしている
		game.lastHPHud       = -1;
		game.lastTpCDShown   = -1;
	}

	//経過時間表示
	{
		game.playTimeFrames = 0;
		game.lastTimeHud    = -1;
	}

	//	タイトル画面
	{
		game.scene   = Scene::Title;
		game.stageNo = 0;
		game.cursorY = 260;		//	ステージセレクト画面のカーソル位置
	}

	//	自機
	{
		const int ph         = assets.player->height;
		game.playerX         = 0;
		game.playerY         = PLAY_Y_MIN + (PLAY_Y_MAX - PLAY_Y_MIN - ph) / 2;
		game.lastPlayerX     = game.playerX;
		game.lastPlayerY     = game.playerY;
		game.playerHP        = PLAYER_MAX_HP;
		game.playerMaxHP     = PLAYER_MAX_HP;
		game.invincibleTimer = 0;
		game.isInvincible    = false;
	}
	
	//	新弾システム
	{
		game.bulletActive = false;	//	弾の変数
		for (int i = 0; i < PLAYER_BULLET_MAX; i++)
		{
			game.playerBullets[i].x      = 0;
			game.playerBullets[i].y      = 0;
			game.playerBullets[i].vx     = 0;
			game.playerBullets[i].vy     = 0;
			game.playerBullets[i].active = false;
			game.playerBullets[i].type   = WeaponType::Normal;
		}
		game.currentWeapon     = WeaponType::Normal;
		game.lastWeaponHud     = -1;
		game.ammoNormal        = MAX_AMMO_NORMAL;
		game.ammoBeam          = MAX_AMMO_BEAM;
		game.ammoSpread        = MAX_AMMO_SPREAD;
		game.normalReloading   = false;
		game.normalReloadTimer = 0;
		game.fireCooldown      = 0;
	}

	//	背景
	{
		game.bgX      = 0;	//	背景の位置
		game.bgWidth  = assets.background[0]->width;	//	変数「bgWidth」に背景画像の幅を代入
		game.bgHeight = assets.background[0]->height;	//	変数「bgHeight」に背景画像の高さを代入
	}

	//	敵機の変数
	{
		for (int i = 0; i < ENEMY_MAX; i++)
		{
			//	種類：半分を上下移動型に
			game.enemies[i].type = (i % 2 == 0) ? EnemyType::Normal : EnemyType::Vertical;

			//	画像サイズ(種類に応じて)
			Bmp* ebmp = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
			int eh    = ebmp ? ebmp->height : 0;

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
		for (int i                 = 0; i < PICKUP_MAX; i++)
		{
			game.pickups[i].x      = 0;
			game.pickups[i].y      = 0;
			game.pickups[i].vx     = -PICKUP_DRIFT_SPEED;
			game.pickups[i].vy     = PICKUP_FALL_SPEED;
			game.pickups[i].active = false;
			game.pickups[i].type   = WeaponType::Normal;
		}
		//	初回スポーンタイマー(ランダム)
		game.rockSpawnTimer = ROCK_SPAWN_MIN + rand() % (ROCK_SPAWN_MAX - ROCK_SPAWN_MIN + 1);
	}

	//	ボス
	{
		game.boss.x       = /*SCREEN_WIDTH - assets.boss->width*/0;  // 右端に配置
		game.boss.y       = PLAY_Y_MIN;                         // UI の下から
		game.boss.w       = (assets.boss ? assets.boss->width  : BOSS_WIDTH );
		game.boss.h       = (assets.boss ? assets.boss->height : BOSS_HEIGHT);
		game.boss.maxHP   = BOSS_MAX_HP;   // 好きな初期HP
		game.boss.hp      = /*game.boss.maxHP*/0;
		game.bossHpShown  = /*game.boss.hp*/0;
		game.boss.alive   = false;  // 最初は非表示
		game.bossActive   = false;
		game.bossIntro    = false;
		game.bossTargetX  = 0;
		game.bossMoveDirY = +1;
		game.bossOscPhase = 0.0f;
		for (int i = 0; i < BOSS_BULLET_MAX; ++i)
		{
			game.bossBullets[i].x      = game.bossBullets[i].y = 0;
			game.bossBullets[i].fx     = game.bossBullets[i].fy = 0.0f;
			game.bossBullets[i].vx     = game.bossBullets[i].vy = 0.0f;
			game.bossBullets[i].active = false;
			game.bossBullets[i].type   = BossBulletType::Spread;
		}

		// タイマー初期値（何らかの値で開始：負なら即発射でもOK）
		game.bossTimerSpread = BOSS_FIRE_INTERVAL_SP;
		game.bossTimerAimed  = BOSS_FIRE_INTERVAL_AIM;
	}
	
	// 入力方向（-1, 0, 1）、TP系
	{
		game.inputX        = 0;
		game.inputY        = 0;
		game.fireRequest   = false;		//	射撃要求
		game.reloadRequest = false;		//	リロード要求
		game.helpRequest   = false;
		game.showHelp      = false;

		game.tpActive        = false;
		game.tpPhase         = 0;
		game.tpTimer         = 0;
		game.tpCDTimer       = 0;
		game.tpDirX          = 1;  // 右向き
		game.tpDirY          = 0;
		game.tpStartX        = game.playerX;
		game.tpStartY        = game.playerY;
		game.tpEndX          = game.playerX;
		game.tpEndY          = game.playerY;
		game.teleportRequest = false;

	}
	
	//	UI、サウンド系
	{
		game.HUD_X          = SCREEN_WIDTH - 350;
		game.centerX        = SCREEN_WIDTH / 2 - assets.stage_select->width / 2;
		game.stageKillCount = 0;
		game.stageCleared   = false;
		game.bgm            = BgmKind::None;
	}
	SetPalette(assets.player);	
}

//	文字Bmpの生成を楽にするやつ、CreateBmpStringからMakeTextBmpに変換
static Bmp* MakeTextBmp(const TCHAR* text, int size, int bold = 0, int ggo = GGO_BITMAP)
{
	const TCHAR* kFont = TEXT("MS ゴシック");	//	使いたいフォントに
	return CreateBmpString(kFont, size, bold, ggo, text);
}
