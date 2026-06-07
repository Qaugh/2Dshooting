#include "common.h"
#include"audio_player.h"
#include "Game.h"
//	ゲームの進行処理

//	文字Bmpの生成を楽にするやつ、CreateBmpStringからMakeTextBmpに変換
static Bmp* MakeTextBmp(const TCHAR* text, int size, int bold = 0, int ggo = GGO_BITMAP)
{
		const TCHAR* kFont = TEXT("MS ゴシック");
		return CreateBmpString(kFont, size, bold, ggo, text);
}
//	画像プールから空きスロットを探す(見つかればその添え字、なかったら-1)
static int AcquirePopupSlot(const Assets& assets)
{
		for (int i = 0; i < GameState::POPUP_MAX; i++)
		{
			if (assets.popups[i] == nullptr)	return i;
		}
		return -1;
}
// （オプション）通常弾のリロード開始
static inline void StartNormalReload(GameState& game)
{
#ifdef NORMAL_RELOAD_FRAMES
		if (!game.normalReloading && game.ammoNormal < MAX_AMMO_NORMAL) {
			game.normalReloading   = true;
			game.normalReloadTimer = NORMAL_RELOAD_FRAMES; // 例: 60
			game.lastWeaponHud     = -1;
			PlaySE(L"sound\\se\\reload.mp3");
		}
#else
		// 即時リロード派ならこちら
		if (game.ammoNormal < MAX_AMMO_NORMAL) {
			game.ammoNormal    = MAX_AMMO_NORMAL;
			game.lastWeaponHud = -1;
		}
#endif
}
//	矩形交差(AABB)
static inline bool Intersects(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
		return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}
//	矩形を取得
static inline void GetEnemyRect(const GameState& game , const Assets& assets, int i  , int& ex, int& ey, int& ew, int& eh)
{
		ex              = game.enemies[i].x;
		ey              = game.enemies[i].y;
		Bmp* ebmp       = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
		if (!ebmp) ebmp = assets.enemy01;	//	フォールバック
		ew              = ebmp ? ebmp->width : 32;
		eh              = ebmp ? ebmp->height : 32;
}
static inline void GetRockRect (const GameState& game , const Assets& assets, int idx, int& rx, int& ry, int& rw, int& rh)
{
		rx = game.rocks[idx].x;
		ry = game.rocks[idx].y;
		rw = assets.rock ? assets.rock->width : 40;
		rh = assets.rock ? assets.rock->height : 40;
}
//	空スロット検索
static int FindFreeBulletSlot(GameState& game)
{
	for (int i = 0; i < PLAYER_BULLET_MAX; i++)	if (!game.playerBullets[i].active)	return i;
	
	return -1;	//	空き無し
}
static int FindFreeRockSlot  (GameState& game)
{
	for (int i = 0; i < ROCK_MAX; ++i)			if (!game.rocks[i].alive)			return i;
	return -1;
}
static int FindFreePickupSlot(GameState& game)
{
	for (int i = 0; i < PICKUP_MAX; ++i)		if (!game.pickups[i].active)		return i;
	return -1;
}	
static int FindFreeEnemySlot (GameState& game) 
{
	for (int i = 0; i < ENEMY_MAX; ++i)			if (!game.enemies[i].alive)			return i;
	return -1;
}
static int FindFreeBossBullet(const GameState& game)
{
	for (int i = 0; i < BOSS_BULLET_MAX; ++i)	if (!game.bossBullets[i].active)	return i;
	return -1;
}
//	物体スポーン
static void SpawnRock  (GameState& game, const Assets& assets)
{
		int slot = FindFreeRockSlot(game);
		if (slot < 0)	return;
		const int rh = assets.rock ? assets.rock->height : 40;//画像未設定時のフォールバック高さ
		const int rw = assets.rock ? assets.rock->height : 40;

		const int maxTry = 20;	//	重ならない位置を探す(最大20回)
		for (int t = 0; t < maxTry; t++)
		{
			int y = PLAY_Y_MIN + rand() & (std::max)(1, (PLAY_Y_MAX - PLAY_Y_MIN - rh));
			int x = PLAY_X_MAX + 50;
			//既存の敵と交差しないか確認
			bool ok = true;
			for (int i = 0; i < ENEMY_MAX; i++)
			{
				if (!game.enemies[i].alive)	continue;
				int ex, ey, ew, eh;
				GetEnemyRect(game, assets, i, ex, ey, ew, eh);
				if (Intersects(x, y, rw, rh, ex, ey, ew, eh))
				{
					ok = false; break;
				}
			}
			if (ok)
			{
				game.rocks[slot] = { x,y,/*hp*/3,/*alive*/true };
				return;
			}
		}
}
static void SpawnEnemy (GameState& game, const Assets& assets)
{
		int slot = FindFreeEnemySlot(game);
		if (slot < 0) return; // 空きがなければ出さない

		// 種類：Normal/Vertical を適当に振り分け
		EnemyType t  = (rand() % 2 == 0) ? EnemyType::Normal : EnemyType::Vertical;
		Bmp* ebmp    = (t == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
		const int ew = ebmp ? ebmp->width : 32;
		const int eh = ebmp ? ebmp->height : 32;

		const int maxTry = 20;	//	重ならない位置を探す(最大20回)
		for (int tr = 0; tr < maxTry; tr++)
		{
			int x = PLAY_X_MAX + 50;
			int y = PLAY_Y_MIN + rand() % (std::max)(1, (PLAY_Y_MAX - PLAY_Y_MIN - eh));

			//	既存の岩と交差しないか確認
			bool ok = true;
			for (int r = 0; r < ROCK_MAX; r++)
			{
				if (!game.rocks[r].alive)	continue;
				int rx, ry, rw, rh;
				GetRockRect(game, assets, r, rx, ry, rw, rh);
				if (Intersects(x, y, ew, eh, rx, ry, rw, rh))
				{
					ok = false; break;
				}
			}
			if (ok)
			{
				game.enemies[slot].x          = x;
				game.enemies[slot].y          = y;
				game.enemies[slot].type       = t;
				game.enemies[slot].alive      = true;
				game.enemies[slot].shootTimer = 60 + rand() % 120;
				game.enemies[slot].vy         = (t == EnemyType::Vertical) ? ((rand() % 2) ? 2 : -2) : 0;
				return;
			}
		}
}
static void SpawnPickup(GameState& game, WeaponType type, int x, int y)
{
		int slot = FindFreePickupSlot(game);
		if (slot < 0) return;
		game.pickups[slot].active = true;
		game.pickups[slot].x      = x;
		game.pickups[slot].y      = y;
		game.pickups[slot].vx     = -PICKUP_DRIFT_SPEED;
		game.pickups[slot].vy     = PICKUP_FALL_SPEED;
		game.pickups[slot].type   = type;
		game.pickups[slot].life   = 300;	//	５秒 : 60fps
}
static void SpawnExplosion(GameState& game, const Assets& assets, int cx, int cy)
{
	if (assets.explosionCount <= 0)	return;
	for (int i = 0; i < GameState::EXPLOSION_MAX; i++)
	{
		if (!game.explosions[i].active)
		{
			game.explosions[i].active = true;
			game.explosions[i].x      = cx;
			game.explosions[i].y      = cy;
			game.explosions[i].frame  = 0;
			game.explosions[i].timer  = 0;
			return;
		}
	}
}
// 7方向拡散（中心角を0°として ±(N-1)/2 * step）
static void SpawnBossSpread(GameState& game)
{
	const int sx   = game.boss.x - 8;
	const int sy   = game.boss.y + game.boss.h / 2;
	const int half = (BOSS_SPREAD_COUNT - 1) / 2;   // 7→3
	const float baseDeg = 180.0f;
	for (int k = -half; k <= half; ++k) {
		int slot  = FindFreeBossBullet(game);
		if (slot < 0) break;
		float deg = baseDeg + k * BOSS_SPREAD_DEG_STEP;
		float rad = deg * 3.14159265f / 180.0f;
		game.bossBullets[slot].fx     = float(sx);
		game.bossBullets[slot].fy     = float(sy);
		game.bossBullets[slot].x      = sx;
		game.bossBullets[slot].y      = sy;
		game.bossBullets[slot].vx     = BOSS_BULLET_SPEED_SP * cosf(rad);
		game.bossBullets[slot].vy     = BOSS_BULLET_SPEED_SP * sinf(rad);
		game.bossBullets[slot].active = true;
		game.bossBullets[slot].type   = BossBulletType::Spread;
	}
}
// 自機狙い（ボス中心→プレイヤー中心方向へ正規化して発射）
static void SpawnBossAimed(GameState& game, const Assets& assets)
{
	int slot = FindFreeBossBullet(game);
	if (slot < 0) return;

	// 砲口（左縁中央）
	const float sx = float(game.boss.x - 8);
	const float sy = float(game.boss.y + game.boss.h / 2);

	// 自機中心
	const int pw   = assets.player ? assets.player->width : 32;
	const int ph   = assets.player ? assets.player->height : 32;
	const float px = float(game.playerX + pw / 2);
	const float py = float(game.playerY + ph / 2);

	// 自機のフレーム間速度（px/frame）
	const float vxP = float(game.playerX - game.lastPlayerX);
	const float vyP = float(game.playerY - game.lastPlayerY);

	// 係数（||P + V t - S|| = s t）
	const float rx = px - sx;
	const float ry = py - sy;
	const float v2 = vxP * vxP + vyP * vyP;
	const float s  = float(BOSS_BULLET_SPEED_AIM);
	const float c  = rx * rx + ry * ry;

	float t = 0.0f;
	// v2 == s^2 の近傍で数値不安定になるので分岐
	const float a = v2 - s * s;
	const float b = 2.0f * (vxP * rx + vyP * ry);

	if (std::fabs(a) < 1e-6f)
	{
		// a ≈ 0 → 線形： b t + c = 0
		if (std::fabs(b) < 1e-6f) {
			t = (s > 1e-6f) ? (std::sqrt(c) / s) : 0.0f; // 苦し紛れ：真っ直ぐ
		}
		else {
			float t1 = -c / b;
			t = (t1 > 0.0f) ? t1 : 0.0f;
		}
	}
	else
	{
		// 二次方程式： a t^2 + b t + c = 0
		float D = b * b - 4.0f * a * c;
		if (D < 0.0f) {
			// 届かない（自機のほうが速い等）→ いまの位置へ撃つ
			t = 0.0f;
		}
		else {
			float sqrtD = std::sqrt(D);
			float t1    = (-b - sqrtD) / (2.0f * a);
			float t2    = (-b + sqrtD) / (2.0f * a);
			// 正で小さいほう
			t = (t1 > 0.0f && t2 > 0.0f) ? (std::min)(t1, t2)
				: (t1 > 0.0f ? t1 : (t2 > 0.0f ? t2 : 0.0f));
		}
	}

	// 狙い点（先読み位置）
	const float ax = px + vxP * t;
	const float ay = py + vyP * t;

	// 正規化して発射
	float dx  = ax - sx;
	float dy  = ay - sy;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len < 1e-6f) { dx = -1.0f; dy = 0.0f; len = 1.0f; }

	game.bossBullets[slot].fx     = sx;
	game.bossBullets[slot].fy     = sy;
	game.bossBullets[slot].x      = int(sx);
	game.bossBullets[slot].y      = int(sy);
	game.bossBullets[slot].vx     = s * (dx / len);
	game.bossBullets[slot].vy     = s * (dy / len);
	game.bossBullets[slot].active = true;
	game.bossBullets[slot].type   = BossBulletType::Aimed;
}
// 移動＆画面外消去
static void UpdateBossBullets(GameState& game, const Assets& assets)
{
	(void)assets;
	for (int i = 0; i < BOSS_BULLET_MAX; ++i)
	{
		if (!game.bossBullets[i].active) continue;
		game.bossBullets[i].fx += game.bossBullets[i].vx;
		game.bossBullets[i].fy += game.bossBullets[i].vy;
		game.bossBullets[i].x   = int(game.bossBullets[i].fx);
		game.bossBullets[i].y   = int(game.bossBullets[i].fy);

		if (game.bossBullets[i].x < -50 || game.bossBullets[i].x > PLAY_X_MAX + 50 ||
			game.bossBullets[i].y < PLAY_Y_MIN - 50 || game.bossBullets[i].y > PLAY_Y_MAX + 50)
		{
			game.bossBullets[i].active = false;
		}
	}
}
//	射撃処理
static void ProcessFireRequest(GameState& game, const Assets& assets)
{
		//	クールダウン中は何もせず終了
		if (game.fireCooldown > 0)	return;
		// 要求消費の直前で武器種類を固定（このフレームの状態で撃つ）
		if (game.currentWeapon == WeaponType::Normal)
		{
#ifdef NORMAL_RELOAD_FRAMES
			if (game.normalReloading) return; // リロード中は撃てない仕様
#endif
			if (game.ammoNormal <= 0) { StartNormalReload(game); return; }

			int slot = FindFreeBulletSlot(game);
			if (slot >= 0) {
				game.playerBullets[slot].active = true;
				game.playerBullets[slot].x      = game.playerX + 30;
				game.playerBullets[slot].y      = game.playerY + 10;
				game.playerBullets[slot].vx     = 12.0f;
				game.playerBullets[slot].vy     = 0.0f;
				game.playerBullets[slot].type   = WeaponType::Normal;
				game.ammoNormal--;

				PlaySE(L"sound\\se\\shot_normal.mp3");

				if (game.ammoNormal <= 0) StartNormalReload(game);
				game.fireCooldown = FIRE_COOLDOWN_FRAMES_NORMAL;
			}
		}
		else if (game.currentWeapon == WeaponType::ChargeBeam)
		{
			if (game.ammoBeam <= 0) return;
			int slot = FindFreeBulletSlot(game);
			if (slot >= 0) {
				game.playerBullets[slot].active = true;
				game.playerBullets[slot].x      = game.playerX + 30;
				game.playerBullets[slot].y      = game.playerY + 10;
				game.playerBullets[slot].vx     = 20.0f;
				game.playerBullets[slot].vy     = 0.0f;
				game.playerBullets[slot].type   = WeaponType::ChargeBeam;
				game.ammoBeam--;

				PlaySE(L"sound\\se\\shot_beam.mp3");
				game.fireCooldown = FIRE_COOLDOWN_FRAMES_BEAM;
			}
		}
		else if (game.currentWeapon == WeaponType::Spread)
		{
			if (game.ammoSpread <= 0) return;
			const float angles[5] = { -30.f,-15.f,0.f,15.f,30.f };
			const float PI        = 3.14159265f, speed = 10.f;
			for (int i = 0; i < 5; ++i) {
				int slot = FindFreeBulletSlot(game);
				if (slot < 0) break;
				float a                         = angles[i] * PI / 180.f;
				game.playerBullets[slot].active = true;
				game.playerBullets[slot].x      = game.playerX + 30;
				game.playerBullets[slot].y      = game.playerY + 10;
				game.playerBullets[slot].vx     = speed * cosf(a);
				game.playerBullets[slot].vy     = speed * sinf(a);
				game.playerBullets[slot].type   = WeaponType::Spread;
			}
			game.ammoSpread--;
			PlaySE(L"sound\\se\\shot_spread.mp3");
			game.fireCooldown = FIRE_COOLDOWN_FRAMES_SPREAD;
		}
}
//	ボス戦開始用
static void Startboss(GameState& game, const Assets& assets)
{
	game.scene      = Scene::Boss;
	if (assets.boss)
	{
		game.boss.w = assets.boss->width;
		game.boss.h = assets.boss->height;
	}
	else {
		game.boss.w = BOSS_WIDTH;
		game.boss.h = BOSS_HEIGHT;
	}
	game.boss.x      = PLAY_X_MAX;	                //	画面外
	game.boss.y      = PLAY_Y_MIN;	                //	UI領域のすぐ下
	game.bossTargetX = PLAY_X_MAX - game.boss.w;	//	止めたい位置(右端150px分内側とか)
	game.boss.maxHP  = BOSS_MAX_HP;
	game.boss.hp     = BOSS_MAX_HP;
	game.bossHpShown = game.boss.hp;
	game.bossIntro   = true;
	game.boss.alive  = true;
	game.bossActive  = true;

	// ▼BGM切替：通常→ボス
	StopBGM();
	PlayBGM(BGM_PATH_BOSS);   // ループ再生想定
	game.bgm = BgmKind::Boss;

	//ボス戦では雑魚・敵弾を消す
	for (int i = 0; i < ENEMY_MAX; i++)		  game.enemies[i].alive		  = false;
	for (int i = 0; i < ENEMY_BULLET_MAX; i++)game.enemyBullets[i].active = false;
}
//	ボス戦終了用
void EndBossCleanup(GameState& game, const Assets& assets)
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
		game.bossBullets[i].type == BossBulletType::Spread;
	}

	//	移動・射撃タイマー
	game.bossMoveDirY = +1;
	game.bossOscPhase = 0.0f;
	game.bossTimerSpread = BOSS_FIRE_INTERVAL_SP;
	game.bossTimerAimed = BOSS_FIRE_INTERVAL_AIM;

	//	テレポート演出の後始末
	game.teleportRequest = false;
	game.tpActive = false;
	game.tpPhase = 0;
	game.tpTimer = 0;
	//	BGM
	StopBGM();
	//	StageSelectに戻った瞬間にBGMを確実に再開できる
	game.bgm = BgmKind::None;
}
// 入力から進行方向（-1,0,1）ベクトルを得る。無入力なら現状の tpDir を維持。
static inline void DecideTeleportDir(GameState& game) {
	int dx = game.inputX;
	int dy = game.inputY;
	if (dx == 0 && dy == 0) {
		// 維持（最後の向き）。何もなければ右
		if (game.tpDirX == 0 && game.tpDirY == 0) { game.tpDirX = 1; game.tpDirY = 0; }
		return;
	}
	// 正規化（斜めは -1/1 のまま）
	if (dx != 0) dx = (dx < 0) ? -1 : 1;
	if (dy != 0) dy = (dy < 0) ? -1 : 1;
	game.tpDirX     = dx;
	game.tpDirY     = dy;
}
// プレイ領域に収まるよう目的地を計算・クランプ
static inline void ComputeTeleportTarget(GameState& game, const Assets& assets) {
	const int pw = assets.player ? assets.player->width : 32;
	const int ph = assets.player ? assets.player->height : 32;
	int x        = game.playerX + game.tpDirX * TP_DISTANCE;
	int y        = game.playerY + game.tpDirY * TP_DISTANCE;

	// クランプ（プレイ領域内）
	if (x < PLAY_X_MIN) x      = PLAY_X_MIN;
	if (y < PLAY_Y_MIN) y      = PLAY_Y_MIN;
	if (x > PLAY_X_MAX - pw) x = PLAY_X_MAX - pw;
	if (y > PLAY_Y_MAX - ph) y = PLAY_Y_MAX - ph;

	game.tpEndX = x;
	game.tpEndY = y;
}

void Game(GameState& game, Assets& assets)
{

	switch (game.scene)
	{
	case Scene::Title:
	{
		break;
	}
	case Scene::StageSelect:
	{
		break;
	}

	case Scene::Play:
	{
		//!	経過時間をカウント(60フレーム = 1秒)
		game.playTimeFrames++;

		//!	背景画像処理
		{
			game.bgX -= 5;	                                            //	背景を左に移動(スクロール)
			if (game.bgX <= -game.bgWidth)	game.bgX += game.bgWidth;	//	背景画像のx座標が背景画像の幅より小さくなったら	背景位置をリセット
		}

		//!	通常移動処理
		{
			const int NORMAL_SPEED = 5;

			
				const int pw = assets.player->width;
				const int ph = assets.player->height;
				// 左
				if		(game.inputX < 0)
				{
					if (game.playerX > PLAY_X_MIN)
						game.playerX += game.inputX * NORMAL_SPEED;
				}
				// 右
				else if (game.inputX > 0)
				{
					if (game.playerX < PLAY_X_MAX - pw)
						game.playerX += game.inputX * NORMAL_SPEED;
				}
				// 上
				if		(game.inputY < 0)
				{
					if (game.playerY > PLAY_Y_MIN)
						game.playerY += game.inputY * NORMAL_SPEED;
				}
				// 下
				else if (game.inputY > 0)
				{
					if (game.playerY < PLAY_Y_MAX - ph)
						game.playerY += game.inputY * NORMAL_SPEED;
				}
			
		}

		//!	TP
		{
			//	開始
			{
				if (!game.tpActive && game.teleportRequest && game.tpCDTimer <= 0)
				{
					game.teleportRequest = false;      // 消費
					DecideTeleportDir(game);           // 方向決定
					game.tpStartX = game.playerX;     // 出発点保存
					game.tpStartY = game.playerY;

					ComputeTeleportTarget(game, assets);// 到着点計算
					game.tpPhase  = 1;                   // Windup
					game.tpTimer  = TP_WINDUP_FRAMES;    // 縮小演出
					game.tpActive = true;

					// 無敵を先に少し伸ばす（演出中に被弾しない）
					game.invincibleTimer = TP_INVINCIBLE_FRAMES;
					game.isInvincible    = true;
					PlaySE(L"sound\\se\\teleport.mp3");
				}
			}
			//	進行
			{
				if (game.tpActive)
					{
					if		(game.tpPhase == 1) { // Windup：縮小
						if (--game.tpTimer <= 0) {
							// 瞬間移動
							game.playerX = game.tpEndX;
							game.playerY = game.tpEndY;
							// 到着側 Recover 開始
							game.tpPhase = 3;
							game.tpTimer = TP_RECOVER_FRAMES;
						}
					}
					else if (game.tpPhase == 3) { // Recover：拡大
						if (--game.tpTimer <= 0) {
							// 完了
							game.tpActive  = false;
							game.tpPhase   = 0;
							game.tpCDTimer = TP_COOLDOWN_FRAMES;
						}
					}
				}
			}
			//	クールダウン減衰
			{
				if (game.tpCDTimer > 0) --game.tpCDTimer;
			}
		}

		//!	無敵時間の更新
		{
			if (game.invincibleTimer > 0)
			{
				game.invincibleTimer--;
				game.isInvincible = (game.invincibleTimer > 0);
			}
		}

		//!	敵スポーン
		{
			//	敵スポーン
			if (--game.enemySpawnTimer <= 0)
			{
				if (FindFreeEnemySlot(game) >= 0)
				{
					SpawnEnemy(game, assets);
				}
				game.enemySpawnTimer = ENEMY_SPAWN_MIN + rand() % (ENEMY_SPAWN_MAX - ENEMY_SPAWN_MIN + 1);
			}
		}

		//!	プレイヤー射撃処理
		{
			if (game.fireRequest)
			{
				ProcessFireRequest(game, assets);
				game.fireRequest = false;
			}
		}

		//!	射撃クールダウンの進行
		{
			if (game.fireCooldown > 0)
			{
				--game.fireCooldown;
			}
		}

		//!	通常弾強制リロード
		{
			if (game.normalReloading) {
				if (--game.normalReloadTimer <= 0) {

					game.normalReloading = false;
					game.ammoNormal      = MAX_AMMO_NORMAL;   // 満タンになる
					game.lastWeaponHud   = -1;             // HUD更新
				}
			}
		}

		//!	リロード処理
		{
			if (game.reloadRequest)
			{	
				StartNormalReload(game);
				game.reloadRequest = false;
			}
		}

		//!	自機x敵機の当たり判定処理
		{
			if (!game.isInvincible)
			{
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player->width;
				const int ph = assets.player->height;
				//	当たり判定を取る
				for (int i = 0; i < ENEMY_MAX; i++)
				{
					if (!game.enemies[i].alive)	continue;
					const int ex = game.enemies[i].x;
					const int ey = game.enemies[i].y;
					const int ew = assets.enemy01->width;
					const int eh = assets.enemy01->height;

					const bool hit =
						(px < ex + ew) &&
						(px + pw > ex) &&
						(py < ey + eh) &&
						(py + ph > ey);

					//	ヒット時の挙動
					if (hit)
					{
						game.playerHP--;						//	HPを1減らす
						game.lastHPHud       = -1;					//	HP HUDを再生成
						game.invincibleTimer = INVINCIBLE_TIME; //	無敵時間を設定
						game.isInvincible    = true;				//	無敵になる
						//	ヒットした敵を倒す
						game.enemies[i].alive = false;
						PlaySE(L"sound\\se\\beep.mp3");
						if (game.stageNo == 0)
						{
							game.stageKillCount++;
						}
						break;
					}
				}
			}
		}

		//!	自機x敵弾の当たり判定
		{
			if (!game.isInvincible)
			{
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player->width;
				const int ph = assets.player->height;

				for (int i = 0; i < ENEMY_BULLET_MAX; i++)
				{
					if (!game.enemyBullets[i].active)	continue;
					const int ebx  = game.enemyBullets[i].x;
					const int eby  = game.enemyBullets[i].y;
					const int ebw  = assets.enemyBullet->width;
					const int ebh  = assets.enemyBullet->height;

					const bool hit =
						(px < ebx + ebw)&&
						(px + pw > ebx) &&
						(py < eby + ebh)&&
						(py + ph > eby);

					if (hit)
					{
						game.playerHP--;						//	HPを1減らす
						game.lastHPHud              = -1;				//	HP HUDを再生成
						game.invincibleTimer        = INVINCIBLE_TIME;	//	無敵時間を設定
						game.isInvincible           = true;			//	無敵になる
						//	当たった敵弾を消す
						game.enemyBullets[i].active = false;
						PlaySE(L"sound\\se\\beep.mp3");
						break;
					}
				}
			}
		}

		//!	HPが0になったらゲームオーバー
		{
			if (game.playerHP <= 0)
			{
				game.scene = Scene::GameOver;
				return;		//この後の処理をスキップ	
			}
		}

		//!	自弾の移動処理
		{
			for (int i = 0; i < PLAYER_BULLET_MAX; i++)
			{
				if (!game.playerBullets[i].active)	continue;

				//	位置更新
				game.playerBullets[i].x += (int)game.playerBullets[i].vx;
				game.playerBullets[i].y += (int)game.playerBullets[i].vy;

				//	画面外判定(チャージビームは画面端まで、それ以外は通常判定)
				if (game.playerBullets[i].type == WeaponType::ChargeBeam)
				{
					//	チャージビームは画面右端に到達したら消える
					if (game.playerBullets[i].x > PLAY_X_MAX)
					{
						game.playerBullets[i].active = false;
					}
				}
				else
				{
					//	通常弾と拡散弾は画面外に出たら消える
					if (game.playerBullets[i].x > PLAY_X_MAX ||
						game.playerBullets[i].x < -50 ||
						game.playerBullets[i].y > PLAY_Y_MAX ||
						game.playerBullets[i].y < PLAY_Y_MIN)
					{
						game.playerBullets[i].active = false;
					}
				}
			}
		}

		//!	敵の移動処理&画面外復活&射撃処理
		{
			for (int i = 0; i < ENEMY_MAX; i++)
			{
				if (!game.enemies[i].alive)	continue;	//	敵が生きていないならスキップ

				//	種類に応じて使う画像とサイズ
				Bmp* ebmp = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
				// ebmpがNULLの場合はassets.enemy01を必ず使う
				if (ebmp == nullptr)
				{
					ebmp = assets.enemy01;
				}
				int ew = ebmp->width;
				int eh = ebmp->height;

				game.enemies[i].x -= 4;	//	敵を左に移動

				//!	上下移動
				if (game.enemies[i].type == EnemyType::Vertical)
				{
					game.enemies[i].y += game.enemies[i].vy;
					//	岩との衝突で反射(Vertical / vyが0ではない場合)
					if (game.enemies[i].type == EnemyType::Vertical && game.enemies[i].vy != 0)
					{
						int ex = game.enemies[i].x;
						int ey = game.enemies[i].y;
						int ew = ebmp->width;
						int eh = ebmp->height;
						for (int r = 0; r < ROCK_MAX; r++)
						{
							if (!game.rocks[r].alive)	continue;
							int rx, ry, rw, rh;
							GetRockRect(game, assets, r, rx, ry, rw, rh);
							if (Intersects(ex, ey, ew, eh, rx, ry, rw, rh))
							{
								//	どちら側から当たったかでYを押し戻す
								if (game.enemies[i].vy > 0) {
									game.enemies[i].y = ry - eh;	//	下方向へ動いていた→岩の上面に合わせて反射
								}
								else {
									game.enemies[i].vy *= -1;		//	反射(符号反転)
									break;
								}
							}
						}
					}

					//	画面端で反射
					if		(game.enemies[i].y < PLAY_Y_MIN)
					{
						game.enemies[i].y = PLAY_Y_MIN;
						game.enemies[i].vy *= -1;
					}
					else if (game.enemies[i].y > PLAY_Y_MAX - eh)
					{
						game.enemies[i].y = PLAY_Y_MAX - eh;
						game.enemies[i].vy *= -1;
					}
				}

				//!	射撃タイマーを減らす
				game.enemies[i].shootTimer--;
				if (game.enemies[i].shootTimer <= 0)
				{
					//	弾を撃つ：空の射撃スロットを探す
					for (int j = 0; j < ENEMY_BULLET_MAX; j++)
					{
						if (!game.enemyBullets[j].active)
						{
							game.enemyBullets[j].active = true;
							game.enemyBullets[j].x      = game.enemies[i].x;
							game.enemyBullets[j].y      = game.enemies[i].y;
							PlaySE(L"sound\\se\\shot_enemy.mp3");
							break;	//	1発撃ったら終了
						}
					}
					//	次の射撃タイマーをリセット(60 ~ 180フレーム後)
					game.enemies[i].shootTimer = 60 + rand() % 120;
				}
				//!	画面外に出たら復活or生産終了
				if (game.enemies[i].x < -assets.enemy01->width)
				{
					game.enemies[i].alive = false;
				}
			}
		}

		//!	敵弾の移動処理
		{
			for (int i = 0; i < ENEMY_BULLET_MAX; i++)
			{
				if (!game.enemyBullets[i].active)	continue;
				game.enemyBullets[i].x -= 5;	//	左に移動

				//	画面外に出たら非アクティブ化
				if (game.enemyBullets[i].x < -assets.enemyBullet->width)
				{
					game.enemyBullets[i].active = false;
				}
			}
		}

		//!	自弾x敵弾の当たり判定処理
		{
			for (int i = 0; i < PLAYER_BULLET_MAX; i++)
			{
				if (!game.playerBullets[i].active)	continue;
				const int pbx = game.playerBullets[i].x;
				const int pby = game.playerBullets[i].y;
				const int pbw = assets.bullet->width;
				const int pbh = assets.bullet->height;
				for (int j = 0; j < ENEMY_BULLET_MAX; j++)
				{
					if (!game.enemyBullets[j].active)	continue;
					const int ebx  = game.enemyBullets[j].x;
					const int eby  = game.enemyBullets[j].y;
					const int ebw  = assets.enemyBullet->width;
					const int ebh  = assets.enemyBullet->height;
					const bool hit =
						(pbx < ebx + ebw) && (pbx + pbw > ebx) &&
						(pby < eby + ebh) && (pby + pbh > eby);
					if (hit)
					{
						//	チャージビームは敵弾を貫通、それ以外は相殺
						if (game.playerBullets[i].type != WeaponType::ChargeBeam)
						{
							game.playerBullets[i].active = false;
						}
						game.enemyBullets[j].active = false;
						PlaySE(L"sound\\se\\hit_bullet.mp3");
						break;
					}
				}
			}
		}

		//!	自弾x敵機当たり判定処理	全ての敵に対して判定
		{
			for (int i = 0; i < ENEMY_MAX; i++)
			{
				if (!game.enemies[i].alive)	continue;
				//	敵のサイズ取得
				Bmp* ebmp    = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
				const int ex = game.enemies[i].x;
				const int ey = game.enemies[i].y;
				const int ew = ebmp->width;
				const int eh = ebmp->height;
				//全ての弾と判定
				for (int j = 0; j < PLAYER_BULLET_MAX; j++)
				{
					if (!game.playerBullets[j].active)	continue;
					const int bx   = game.playerBullets[j].x;
					const int by   = game.playerBullets[j].y;
					const int bw   = assets.bullet->width;	//	10
					const int bh   = assets.bullet->height;	//	10
					const bool hit =
						(bx < ex + ew) && (bx + bw > ex) &&
						(by < ey + eh) && (by + bh > ey);
					if (hit)
					{
						game.enemies[i].alive = false;
						PlaySE(L"sound\\se\\kill_enemy.mp3");
						//	爆発を出す
						Bmp* ebmp = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
						int ex = game.enemies[i].x;
						int ey = game.enemies[i].y;
						int ew = ebmp ? ebmp->width  : 32;
						int eh = ebmp ? ebmp->height : 32;
						SpawnExplosion(game, assets, ex + ew / 2, ey + eh / 2);
						//	stage1のみ：撃破数をカウント
						if (game.stageNo == 00)
						{
							game.stageKillCount++;
						}
						//チャージビームは貫通、それ以外は消える
						if (game.playerBullets[j].type != WeaponType::ChargeBeam)
						{
							game.playerBullets[j].active = false;
						}
						//	スコア加点(武器の種類に応じて)
						int add = 100;
						if		(game.playerBullets[j].type == WeaponType::ChargeBeam)
						{
							add = 200;
						}
						else if (game.playerBullets[j].type == WeaponType::Spread)
						{
							add = 80;
						}
						game.score		 += add;
						game.lastScoreHud = -1;

						//	敵を倒したら25%の確率で通常弾回復
						if (rand() % 4 == 0)	//　4 : 25%
						{
							if (game.ammoNormal < MAX_AMMO_NORMAL)
							{
								game.ammoNormal++;
								game.lastWeaponHud = -1;
							}
						}

						//	スコアポップアップ生成
						int slot = AcquirePopupSlot(assets);
						if (slot >= 0 && game.popupCount < GameState::POPUP_MAX)
						{
							TCHAR txt[16];
							_stprintf(txt, TEXT("+%d"), add);
							assets.popups[slot] = MakeTextBmp(txt, 22);
							ScorePopupInfo& sp  = game.popups[game.popupCount++];
							sp.x                = game.enemies[i].x;
							sp.y                = game.enemies[i].y - 10;
							sp.life             = 30;
							sp.value            = add;
							sp.slot             = slot;
							sp.alive            = true;
						}
						//	チャージビーム以外は1体倒したら終了
						if (game.playerBullets[j].type != WeaponType::ChargeBeam)
							break;
					}
				}

			}
		}

		//!	岩スポーンタイマー
		{
			if (--game.rockSpawnTimer <= 0)
			{
				SpawnRock(game, assets);
				game.rockSpawnTimer = ROCK_SPAWN_MIN + rand() % (ROCK_SPAWN_MAX - ROCK_SPAWN_MIN + 1);
			}
		}

		//!	岩の移動 & 画面外処理
		{
			for (int i = 0; i < ROCK_MAX; i++)
			{
				if (!game.rocks[i].alive) continue;
				game.rocks[i].x -= ROCK_SPEED;
				const int rw	 = assets.rock ? assets.rock->width : 40;
				if (game.rocks[i].x < -rw)
				{
					game.rocks[i].alive = false;
				}
			}
		}

		//!	自機x岩の当たり判定
		{
			if (!game.isInvincible)
			{
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player->width;
				const int ph = assets.player->height;

				for (int i = 0; i < ROCK_MAX; i++)
				{
					if (!game.rocks[i].alive) continue;

					const int rx = game.rocks[i].x;
					const int ry = game.rocks[i].y;
					const int rw = assets.enemy01->width;   // 仮
					const int rh = assets.enemy01->height;

					const bool hit =
						(px < rx + rw) &&
						(px + pw > rx) &&
						(py < ry + rh) &&
						(py + ph > ry);

					if (hit)
					{
						game.playerHP--;
						game.lastHPHud       = -1;
						game.invincibleTimer = INVINCIBLE_TIME;
						game.isInvincible    = true;

						if (game.playerHP <= 0)
						{
							game.scene = Scene::GameOver;
						}
						break;
					}
				}
			}
		}

		//!	自弾x岩の当たり判定(HP減少 / 破壊時にピックアップ生成)
		{
			for (int i = 0; i < ROCK_MAX; i++)
			{
				if (!game.rocks[i].alive)	continue;
				const int rx = game.rocks[i].x;
				const int ry = game.rocks[i].y;
				const int rw = assets.rock ? assets.rock->width : 40;
				const int rh = assets.rock ? assets.rock->height : 40;

				for (int j = 0; j < PLAYER_BULLET_MAX; j++)
				{
					if (!game.playerBullets[j].active)	continue;
					const int bx = game.playerBullets[j].x;
					const int by = game.playerBullets[j].y;
					const int bw =
						(game.playerBullets[j].type == WeaponType::ChargeBeam && assets.chargeBeam) ? assets.chargeBeam->width :
						(game.playerBullets[j].type == WeaponType::Spread && assets.spreadBullet)   ? assets.spreadBullet->width :
						(assets.bullet ? assets.bullet->width : 10);
					const int bh =
						(game.playerBullets[j].type == WeaponType::ChargeBeam && assets.chargeBeam) ? assets.chargeBeam->height :
						(game.playerBullets[j].type == WeaponType::Spread && assets.spreadBullet)   ? assets.spreadBullet->height :
						(assets.bullet ? assets.bullet->height : 10);

					const bool hit =
						(bx < rx + rw) && (bx + bw > rx) &&
						(by < ry + rh) && (by + bh > ry);

					if (hit) {
						//	Beam は貫通、それ以外は消える
						if (game.playerBullets[j].type != WeaponType::ChargeBeam)
						{
							game.playerBullets[j].active = false;
						}

						//	「次の衝突で壊れるか」を先に判定
						bool willBreak = (game.rocks[i].hp - 1) <= 0;

						//	岩 HP 減少
						--game.rocks[i].hp;

						if (willBreak)
						{
							//	破壊・サウンド
							game.rocks[i].alive = false;
							PlaySE(L"sound\\se\\kill_rock.mp3");

							//	ランダム武器種のピックアップを生成
							int r = rand() % 3;
							WeaponType drop = (r == 0) ? WeaponType::Normal : (r == 1) ? WeaponType::ChargeBeam : WeaponType::Spread;
							SpawnPickup(game, drop, rx, ry);
							//	スコア加点
							game.score		 += 50;
							game.lastScoreHud = -1; //	HUD 再生成フラグ
						}
						else
						{
							//	命中・サウンド
							if (game.playerBullets[j].type != WeaponType::ChargeBeam)
							{
								PlaySE(L"sound\\se\\hit_rock.mp3");
							}
						}
						break; //	1弾で1回処理
					}

				}
			}
		}

		//!	ピックアップ(アイテム)落下 & プレイヤー取得判定
		{
			for (int i = 0; i < PICKUP_MAX; i++)
			{
				if (!game.pickups[i].active)	continue;
				game.pickups[i].x += game.pickups[i].vx;
				game.pickups[i].y += game.pickups[i].vy;
				//	画面外で消去
				if (game.pickups[i].y > PLAY_Y_MAX)
				{
					game.pickups[i].active = false;
					continue;
				}
				//	寿命を減らして０以下で消す
				if (--game.pickups[i].life <= 0)
				{
					game.pickups[i].active = false;
					continue;
				}
				//	自機当たり判定
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player ? assets.player->width : 32;
				const int ph = assets.player ? assets.player->height : 32;

				//	ピックアップ画像サイズ
				Bmp* pimg = nullptr;
				switch (game.pickups[i].type)
				{
				case WeaponType::Normal:	 pimg = assets.pickupNormal; break;
				case WeaponType::ChargeBeam: pimg = assets.pickupBeam;   break;
				case WeaponType::Spread:     pimg = assets.pickupSpread; break;
				}

				const int qw = pimg ? pimg->width : 16;
				const int qh = pimg ? pimg->height : 16;

				const bool got =
					(px < game.pickups[i].x + qw) && (px + pw > game.pickups[i].x) &&
					(py < game.pickups[i].y + qh) && (py + ph > game.pickups[i].y);

				if (got) {
					//	対応弾を１回復(上限まで)
					if		(game.pickups[i].type == WeaponType::Normal) {
						if (game.ammoNormal < MAX_AMMO_NORMAL)	game.ammoNormal++;
					}
					else if (game.pickups[i].type == WeaponType::ChargeBeam) {
						if (game.ammoBeam < MAX_AMMO_BEAM)		game.ammoBeam++;
					}
					else {
						if (game.ammoSpread < MAX_AMMO_SPREAD)	game.ammoSpread++;
					}
					game.pickups[i].active = false;
					game.lastWeaponHud     = -1;
					PlaySE(L"sound\\se\\pickup.mp3");
				}
			}
		}

		//!	ポップアップ更新
		{
			for (int i = 0; i < game.popupCount; i++)
			{
				ScorePopupInfo& p = game.popups[i];
				if (!p.alive)	continue;	//	死んでる個体はスキップ

				p.y -= 1;					//	ふわっと上に
				if (p.life-- <= 0) {
					p.alive = false;		//	時間で非active
				}
			}
		//!	削除＆前詰め
			int write = 0;
			for (int i = 0; i < game.popupCount; i++)
			{
				auto& p = game.popups[i];
				//p.y -= 1;	//	ふわっと上へ
				//	表示カウント
				if (p.alive)
				{
					if (write != i)	game.popups[write] = p;
					write++;
				}
				else  //		表示時間切れ：画像を開放(Assets側)
				{
					if (p.slot >= 0 && (assets.popups[p.slot]))
						DeleteBmp(&assets.popups[p.slot]);	//	内部でNULLになる想定
				}

			}
			game.popupCount = write;
		}

		//!	次のフレーム用に自機の位置を取る
		{
			game.lastPlayerX = game.playerX;
			game.lastPlayerY = game.playerY;
		}
		
		//!	stage1 : 15倒したらクリア
		{
			if (game.stageNo == 0 && game.stageKillCount >= STAGE1_TARGET_KILL)
			{
				game.stageCleared = true;
				Startboss(game, assets);
				return;
			}
		}

		//!	stage2 : 1分(=60秒)耐えたらクリア
		{
			if (game.stageNo == 1 && game.playTimeFrames >= (STAGE2_SURVIVE_SECONDS * 60))
			{
				game.stageCleared = true;
				Startboss(game, assets);
				return;
			}

		}

		//!	フレームを進めて寿命が来たら消す
		{
			for (int i = 0; i < GameState::EXPLOSION_MAX; i++)
			{
				auto& e = game.explosions[i];
				if (!e.active)	continue;
				if (e.timer++ >= EXPLOSION_FRAME_DELAY)
				{
					e.timer = 0;
					if (e.frame++ >= (int)assets.explosionCount)
					{
						e.active = false;
					}
				}
			}
		}
		break;
	}
	
	case Scene::Boss:
	{
		//!	経過時間をカウント(60フレーム = 1秒)
		game.playTimeFrames++;

		//!	背景画像処理
		{
			game.bgX -= 5;	                                            //	背景を左に移動(スクロール)
			if (game.bgX <= -game.bgWidth)	game.bgX += game.bgWidth;	//	背景画像のx座標が背景画像の幅より小さくなったら	背景位置をリセット
		}

		//!	ボス入場アニメーション
		{
			if (game.bossIntro)
			{
				//	左へ移動
				game.boss.x -= BOSS_ENTRANCE_SPEED;

				//	目標に到着→停止して完了
				if (game.boss.x <= game.bossTargetX)
				{
					game.boss.x    = game.bossTargetX;
					game.bossIntro = false;
				}
			}
		}

		//!	通常移動処理
		{
			const int NORMAL_SPEED = 5;
			const int pw           = assets.player->width;
			const int ph           = assets.player->height;
			// 左
			if		(game.inputX < 0)
				{
					if (game.playerX > PLAY_X_MIN)
						game.playerX += game.inputX * NORMAL_SPEED;
				}
			// 右
			else if (game.inputX > 0)
				{
					if (game.playerX < PLAY_X_MAX - pw)
						game.playerX += game.inputX * NORMAL_SPEED;
				}
			// 上
			if		(game.inputY < 0)
				{
					if (game.playerY > PLAY_Y_MIN)
						game.playerY += game.inputY * NORMAL_SPEED;
				}
			// 下
			else if (game.inputY > 0)
				{
					if (game.playerY < PLAY_Y_MAX - ph)
						game.playerY += game.inputY * NORMAL_SPEED;
				}
			
		}

		//!	TP
		{	
			//	開始
			{
				if (!game.tpActive && game.teleportRequest && game.tpCDTimer <= 0)
				{
					game.teleportRequest = false;      // 消費
					DecideTeleportDir(game);           // 方向決定
					game.tpStartX = game.playerX;     // 出発点保存
					game.tpStartY = game.playerY;

					ComputeTeleportTarget(game, assets);// 到着点計算
					game.tpPhase  = 1;                   // Windup
					game.tpTimer  = TP_WINDUP_FRAMES;    // 縮小演出
					game.tpActive = true;

					// 無敵を先に少し伸ばす（演出中に被弾しない）
					game.invincibleTimer = TP_INVINCIBLE_FRAMES;
					game.isInvincible    = true;
					PlaySE(L"sound\\se\\teleport.mp3");
				}
			}
			//	進行
			{
				if (game.tpActive)
				{
					if		(game.tpPhase == 1) { // Windup：縮小
						if (--game.tpTimer <= 0) {
							// 瞬間移動
							game.playerX = game.tpEndX;
							game.playerY = game.tpEndY;
							// 到着側 Recover 開始
							game.tpPhase = 3;
							game.tpTimer = TP_RECOVER_FRAMES;
						}
					}
					else if (game.tpPhase == 3) { // Recover：拡大
						if (--game.tpTimer <= 0) {
							// 完了
							game.tpActive  = false;
							game.tpPhase   = 0;
							game.tpCDTimer = TP_COOLDOWN_FRAMES;
						}
					}
				}
			}
			//	クールダウン減衰
			{
				if (game.tpCDTimer > 0) --game.tpCDTimer;
			}
		}

		//!	無敵時間の更新
		{
			if (game.invincibleTimer > 0)
			{
				game.invincibleTimer--;
				game.isInvincible = (game.invincibleTimer > 0);
			}
		}

		//!	プレイヤー射撃処理
		{
			if (game.fireRequest)
			{
				ProcessFireRequest(game, assets);
				game.fireRequest = false;
			}
		}

		//!	射撃クールダウンの進行
		{
			if (game.fireCooldown > 0)
			{
				--game.fireCooldown;
			}
		}

		//!	通常弾強制リロード
		{
			if (game.normalReloading) {
				if (--game.normalReloadTimer <= 0) {
					game.normalReloading = false;
					game.ammoNormal      = MAX_AMMO_NORMAL;   // 満タンになる
					game.lastWeaponHud   = -1;             // HUD更新
				}
			}
		}

		//!	リロード処理
		{
			if (game.reloadRequest)
			{
				StartNormalReload(game);
				game.reloadRequest = false;
			}
		}

		//!	HPが0になったらゲームオーバー
		{
			if (game.playerHP <= 0)
			{
				EndBossCleanup(game);
				game.scene = Scene::GameOver;
				return;		//この後の処理をスキップ	
			}
		}

		//!	自弾の移動処理
		{
			for (int i = 0; i < PLAYER_BULLET_MAX; i++)
			{
				if (!game.playerBullets[i].active)	continue;

				//	位置更新
				game.playerBullets[i].x += (int)game.playerBullets[i].vx;
				game.playerBullets[i].y += (int)game.playerBullets[i].vy;

				//	画面外判定(チャージビームは画面端まで、それ以外は通常判定)
				if (game.playerBullets[i].type == WeaponType::ChargeBeam)
				{
					//	チャージビームは画面右端に到達したら消える
					if (game.playerBullets[i].x > PLAY_X_MAX)
					{
						game.playerBullets[i].active = false;
					}
				}
				else
				{
					//	通常弾と拡散弾は画面外に出たら消える
					if (game.playerBullets[i].x > PLAY_X_MAX ||
						game.playerBullets[i].x < -50 ||
						game.playerBullets[i].y > PLAY_Y_MAX ||
						game.playerBullets[i].y < PLAY_Y_MIN)
					{
						game.playerBullets[i].active = false;
					}
				}
			}
		}

		//!	岩スポーンタイマー
		{
			if (--game.rockSpawnTimer <= 0)
			{
				SpawnRock(game, assets);
				game.rockSpawnTimer = ROCK_SPAWN_MIN + rand() % (ROCK_SPAWN_MAX - ROCK_SPAWN_MIN + 1);
			}
		}

		//!	岩の移動 & 画面外処理
		{
			for (int i = 0; i < ROCK_MAX; i++)
			{
				if (!game.rocks[i].alive) continue;
				game.rocks[i].x -= ROCK_SPEED;
				const int rw = assets.rock ? assets.rock->width : 40;
				if (game.rocks[i].x < -rw)
				{
					game.rocks[i].alive = false;
				}
			}
		}

		//!	自機x岩の当たり判定
		{
			if (!game.isInvincible)
			{
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player->width;
				const int ph = assets.player->height;

				for (int i = 0; i < ROCK_MAX; i++)
				{
					if (!game.rocks[i].alive) continue;

					const int rx = game.rocks[i].x;
					const int ry = game.rocks[i].y;
					const int rw = assets.enemy01->width;   // 仮
					const int rh = assets.enemy01->height;

					const bool hit =
						(px < rx + rw) &&
						(px + pw > rx) &&
						(py < ry + rh) &&
						(py + ph > ry);

					if (hit)
					{
						game.playerHP--;
						game.lastHPHud       = -1;
						game.invincibleTimer = INVINCIBLE_TIME;
						game.isInvincible    = true;

						if (game.playerHP <= 0)
						{
							EndBossCleanup(game, assets);
							game.scene = Scene::GameOver;
						}
						break;
					}
				}
			}
		}

		//!	自弾x岩の当たり判定(HP減少 / 破壊時にピックアップ生成)
		{
			for (int i = 0; i < ROCK_MAX; i++)
			{
				if (!game.rocks[i].alive)	continue;
				const int rx = game.rocks[i].x;
				const int ry = game.rocks[i].y;
				const int rw = assets.rock ? assets.rock->width : 40;
				const int rh = assets.rock ? assets.rock->height : 40;

				for (int j = 0; j < PLAYER_BULLET_MAX; j++)
				{
					if (!game.playerBullets[j].active)	continue;
					const int bx = game.playerBullets[j].x;
					const int by = game.playerBullets[j].y;
					const int bw =
						(game.playerBullets[j].type == WeaponType::ChargeBeam && assets.chargeBeam) ? assets.chargeBeam->width :
						(game.playerBullets[j].type == WeaponType::Spread && assets.spreadBullet)   ? assets.spreadBullet->width :
						(assets.bullet ? assets.bullet->width : 10);
					const int bh =
						(game.playerBullets[j].type == WeaponType::ChargeBeam && assets.chargeBeam) ? assets.chargeBeam->height :
						(game.playerBullets[j].type == WeaponType::Spread && assets.spreadBullet)   ? assets.spreadBullet->height :
						(assets.bullet ? assets.bullet->height : 10);

					const bool hit =
						(bx < rx + rw) && (bx + bw > rx) &&
						(by < ry + rh) && (by + bh > ry);

					if (hit) {
						//	Beam は貫通、それ以外は消える
						if (game.playerBullets[j].type != WeaponType::ChargeBeam)
						{
							game.playerBullets[j].active = false;
						}

						//	「次の衝突で壊れるか」を先に判定
						bool willBreak = (game.rocks[i].hp - 1) <= 0;

						//	岩 HP 減少
						--game.rocks[i].hp;

						if (willBreak)
						{
							//	破壊・サウンド
							game.rocks[i].alive = false;
							PlaySE(L"sound\\se\\kill_rock.mp3");

							//	ランダム武器種のピックアップを生成
							int r = rand() % 3;
							WeaponType drop = (r == 0) ? WeaponType::Normal : (r == 1) ? WeaponType::ChargeBeam : WeaponType::Spread;
							SpawnPickup(game, drop, rx, ry);
							//	スコア加点
							game.score		 += 50;
							game.lastScoreHud = -1; //	HUD 再生成フラグ
						}
						else
						{
							//	命中・サウンド
							if (game.playerBullets[j].type != WeaponType::ChargeBeam)
							{
								PlaySE(L"sound\\se\\hit_rock.mp3");
							}
						}
						break; //	1弾で1回処理
					}

				}
			}
		}

		//!	自機xボスの当たり判定
		{
			if (!game.bossIntro && game.boss.alive && !game.isInvincible && assets.player)
			{
				//	ボス矩形
				const int bxL = game.boss.x;
				const int bxT = game.boss.y;
				const int bxR = bxL + game.boss.w;
				const int bxB = bxT + game.boss.h;
				//	自機矩形
				const int pw  = assets.player->width;
				const int ph  = assets.player->height;
				const int pxL = game.playerX;
				const int pyT = game.playerY;
				const int pxR = pxL + pw;
				const int pyB = pyT + ph;
				//	AABB
				const bool hit =
					(pxL < bxR) && (pxR > bxL) &&
					(pyT < bxB) && (pyB > bxT);

				if (hit)
				{
					game.playerHP--;
					game.lastHPHud       = -1;
					game.invincibleTimer = INVINCIBLE_TIME;
					game.isInvincible    = true;
					PlaySE(L"sound\\se\\beep.mp3");
					if (game.playerHP <= 0)
					{
						EndBossCleanup(game);
						game.scene = Scene::GameOver;
						return;
					}
				}
				
			}
		}

		//!	自弾xボスの当たり判定
		{
			if (!game.bossIntro && game.boss.alive)
			{
				const int bxL = game.boss.x;
				const int bxT = game.boss.y;
				const int bxR = bxL + game.boss.w;
				const int bxB = bxT + game.boss.h;

				for (int i = 0; i < PLAYER_BULLET_MAX; i++)
				{
					if (!game.playerBullets[i].active)	continue;

					//	弾の矩形(弾の種類によって画像サイズを分けている)
					const int bw =
						(game.playerBullets[i].type == WeaponType::ChargeBeam && assets.chargeBeam) ? assets.chargeBeam->width :
						(game.playerBullets[i].type == WeaponType::Spread && assets.spreadBullet) ? assets.spreadBullet->width :
						(assets.bullet ? assets.bullet->width : 10);
					const int bh =
						(game.playerBullets[i].type == WeaponType::ChargeBeam && assets.chargeBeam) ? assets.chargeBeam->height :
						(game.playerBullets[i].type == WeaponType::Spread && assets.spreadBullet) ? assets.spreadBullet->height :
						(assets.bullet ? assets.bullet->height : 10);

					const int px = game.playerBullets[i].x;
					const int py = game.playerBullets[i].y;

					const bool hit =
						(px < bxR) && (px + bw > bxL) &&
						(py < bxB) && (py + bh > bxT);

					if (hit)
					{
						//	ダメージ
						int damage = 10;
						if		(game.playerBullets[i].type == WeaponType::ChargeBeam)	damage = 20;
						else if (game.playerBullets[i].type == WeaponType::Spread)		damage = 6;

						game.boss.hp -= damage;

						//	チャージビーム以外は弾を消す(ビームは貫通)
						if (game.playerBullets[i].type != WeaponType::ChargeBeam)
							game.playerBullets[i].active = false;

						//	撃破 → クリア
						if (game.boss.hp <= 0)
						{
							game.boss.hp    = 0;
							EndBossCleanup(game);
							//	ボス撃破スコア追加
							game.score += BOSS_CLEAR_BONUS;
							game.lastScoreHud = -1;
							game.scene      = Scene::GameClear;
							PlaySE(L"sound\\se\\game_clear.mp3");
							break;	//	以後の弾は無視
						}
					}
				}
			}
			//	表示用HP(bossHPShown)を実HPへゆっくり寄せる
			{
				game.bossHpShown = (game.boss.hp < 0) ? 0 : game.boss.hp;
			}

		}

		//!	ボス弾x自機当たり判定
		{
			if (!game.isInvincible) {
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player ? assets.player->width : 32;
				const int ph = assets.player ? assets.player->height : 32;

				const int bw = assets.enemyBullet ? assets.enemyBullet->width : 10;
				const int bh = assets.enemyBullet ? assets.enemyBullet->height : 10;

				for (int i = 0; i < BOSS_BULLET_MAX; ++i) {
					if (!game.bossBullets[i].active) continue;
					const int bx   = game.bossBullets[i].x;
					const int by   = game.bossBullets[i].y;
					const bool hit =
						(px < bx + bw) && (px + pw > bx) &&
						(py < by + bh) && (py + ph > by);
					if (hit) {
						game.playerHP--;            // HP-1
						game.lastHPHud             = -1;
						game.invincibleTimer       = INVINCIBLE_TIME;
						game.isInvincible          = true;
						game.bossBullets[i].active = false;
						PlaySE(L"sound\\se\\beep.mp3");
						if (game.playerHP <= 0) {
							EndBossCleanup(game, assets);
							game.scene = Scene::GameOver;
							return;
						}
						break;
					}
				}
			}
		}

		//!ボス射撃
		{
			if (!game.bossIntro && game.boss.alive) {
				if (--game.bossTimerSpread <= 0) {
					SpawnBossSpread(game);
					PlaySE(L"sound\\se\\boss_shot_spread.mp3");
					game.bossTimerSpread = BOSS_FIRE_INTERVAL_SP;
				}
				if (--game.bossTimerAimed <= 0) {
					SpawnBossAimed(game, assets);
					PlaySE(L"sound\\se\\boss_shot_aimed.mp3");
					game.bossTimerAimed = BOSS_FIRE_INTERVAL_AIM;
				}
			}
		}

		//!	ボス移動
		{
			if (!game.bossIntro && game.boss.alive)
			{
				const int top = PLAY_Y_MIN;
				const int bottom = PLAY_Y_MAX - game.boss.h;
				//	進める
				game.boss.y += BOSS_MOVE_SPEED_Y * game.bossMoveDirY;
				//	端で反転 + クランプ
				if		(game.boss.y <= top)
				{
					game.boss.y = top;
					game.bossMoveDirY = +1;
				}
				else if (game.boss.y >= bottom)
				{
					game.boss.y = bottom;
					game.bossMoveDirY = -1;
				}
			}
		}

		//! 弾の更新
		{
			UpdateBossBullets(game, assets);
		}

		//!	ボス弾x自弾
		{
			// ボス弾の当たりサイズ（見た目は enemyBullet を流用）
			const int bossBw = assets.enemyBullet ? assets.enemyBullet->width : 10;
			const int bossBh = assets.enemyBullet ? assets.enemyBullet->height : 10;

			for (int pi = 0; pi < PLAYER_BULLET_MAX; ++pi) {
				if (!game.playerBullets[pi].active) continue;

				// 自弾の当たりサイズ（種類で切替）
				int pbw = 10, pbh = 10;
				if		(game.playerBullets[pi].type == WeaponType::ChargeBeam && assets.chargeBeam) {
					pbw = assets.chargeBeam->width;   pbh = assets.chargeBeam->height;
				}
				else if (game.playerBullets[pi].type == WeaponType::Spread && assets.spreadBullet) {
					pbw = assets.spreadBullet->width; pbh = assets.spreadBullet->height;
				}
				else
				{
					if (assets.bullet) { pbw = assets.bullet->width; pbh = assets.bullet->height; }
				}

				const int pbx = game.playerBullets[pi].x;
				const int pby = game.playerBullets[pi].y;

				for (int bi = 0; bi < BOSS_BULLET_MAX; ++bi) {
					if (!game.bossBullets[bi].active) continue;

					const int bbx = game.bossBullets[bi].x;
					const int bby = game.bossBullets[bi].y;

					const bool hit =
						(pbx < bbx + bossBw) && (pbx + pbw > bbx) &&
						(pby < bby + bossBh) && (pby + pbh > bby);

					if (!hit) continue;

					// 相殺ルール：ビームは貫通、それ以外は相殺で消す。ボス弾は常に消す。
					if (game.playerBullets[pi].type != WeaponType::ChargeBeam) {
						game.playerBullets[pi].active = false;
						PlaySE(L"sound\\se\\hit_bullet.mp3");
					}
						game.bossBullets[bi].active = false;

					if (game.playerBullets[pi].type != WeaponType::ChargeBeam) {
						break; // 1個相殺したら次の自弾へ（ビームはそのまま複数相殺可）
					}
				}
			}
		}

		//!	ピックアップ(アイテム)落下 & プレイヤー取得判定
		{
			for (int i = 0; i < PICKUP_MAX; i++)
			{
				if (!game.pickups[i].active)	continue;
				game.pickups[i].x += game.pickups[i].vx;
				game.pickups[i].y += game.pickups[i].vy;
				//	画面外で消去
				if (game.pickups[i].y > PLAY_Y_MAX)
				{
					game.pickups[i].active = false;
					continue;
				}
				//	寿命を減らして０以下で消す
				if (--game.pickups[i].life <= 0)
				{
					game.pickups[i].active = false;
					continue;
				}
				//	自機当たり判定
				const int px = game.playerX;
				const int py = game.playerY;
				const int pw = assets.player ? assets.player->width : 32;
				const int ph = assets.player ? assets.player->height : 32;

				//	ピックアップ画像サイズ
				Bmp* pimg = nullptr;
				switch (game.pickups[i].type)
				{
				case WeaponType::Normal:	 pimg = assets.pickupNormal; break;
				case WeaponType::ChargeBeam: pimg = assets.pickupBeam;   break;
				case WeaponType::Spread:     pimg = assets.pickupSpread; break;
				}

				const int qw = pimg ? pimg->width : 16;
				const int qh = pimg ? pimg->height : 16;

				const bool got =
					(px < game.pickups[i].x + qw) && (px + pw > game.pickups[i].x) &&
					(py < game.pickups[i].y + qh) && (py + ph > game.pickups[i].y);

				if (got) {
					//	対応弾を１回復(上限まで)
					if (game.pickups[i].type == WeaponType::Normal) {
						if (game.ammoNormal < MAX_AMMO_NORMAL)	game.ammoNormal++;
					}
					else if (game.pickups[i].type == WeaponType::ChargeBeam) {
						if (game.ammoBeam < MAX_AMMO_BEAM)		game.ammoBeam++;
					}
					else {
						if (game.ammoSpread < MAX_AMMO_SPREAD)	game.ammoSpread++;
					}
					game.pickups[i].active = false;
					game.lastWeaponHud = -1;
					PlaySE(L"sound\\se\\pickup.mp3");
				}
			}
		} 

		//!	ポップアップ更新
		{
			for (int i = 0; i < game.popupCount; i++)
			{
				ScorePopupInfo& p = game.popups[i];
				if (!p.alive)	continue;	//	死んでる個体はスキップ

				p.y -= 1;					//	ふわっと上に
				if (p.life-- <= 0) {
					p.alive = false;		//	時間で非active
				}
			}
		//!	削除＆前詰め
			int write = 0;
			for (int i = 0; i < game.popupCount; i++)
			{
				auto& p = game.popups[i];
				//p.y -= 1;	//	ふわっと上へ
				//	表示カウント
				if (p.alive)
				{
					if (write != i)	game.popups[write] = p;
					write++;
				}
				else  //		表示時間切れ：画像を開放(Assets側)
				{
					if (p.slot >= 0 && (assets.popups[p.slot]))
						DeleteBmp(&assets.popups[p.slot]);	//	内部でNULLになる想定
				}

			}
			game.popupCount = write;
		}

		//!	次のフレーム用に自機の位置を取る
		{
			game.lastPlayerX = game.playerX;
			game.lastPlayerY = game.playerY;
		}

		break;
	}

	case Scene::Pause:
	{
		break;
	}
	case Scene::GameOver:
	{
		break;
	}
	case Scene::GameClear:
	{
		break;
	}

	}
}

void EndBossCleanup(GameState& game)
{
	//ボス状態を終了扱いへ
	game.boss.alive = false;
	game.bossActive = false;
	game.bossIntro = false;

	//ボス弾を全消去
	for (int i = 0; i < BOSS_BULLET_MAX; i++)
	{
		game.bossBullets[i].active = false;
		game.bossBullets[i].x = 0;
		game.bossBullets[i].y = 0;
		game.bossBullets[i].fx = 0.0f;
		game.bossBullets[i].fy = 0.0f;
		game.bossBullets[i].vx = 0.0f;
		game.bossBullets[i].vy = 0.0f;
		game.bossBullets[i].type = BossBulletType::Spread;
	}

	//ボスの移動・射撃タイマーを初期値へ
	game.bossMoveDirY = +1;
	game.bossOscPhase = 0.0f;
	game.bossTimerSpread = BOSS_FIRE_INTERVAL_SP;
	game.bossTimerAimed = BOSS_FIRE_INTERVAL_AIM;

	//TP演出が残っていたら止める
	game.teleportRequest = false;
	game.tpActive = false;
	game.tpPhase = 0;
	game.tpTimer = 0;

}

void HandlePlayerDeath(GameState& game)
{

}

void HandleBossDefeat(GameState& game)
{

}
