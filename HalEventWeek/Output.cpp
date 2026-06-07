#include "common.h"
#include "Output.h"
#include <algorithm>
//	出力処理
namespace HUD
{
	constexpr int kScoreY        = 20;	//	Scoreの規定Y
	constexpr int kTpMarginY     = 6;		//	ScoreとTPの間の縦の間隔
	constexpr int kInvItemGapY   = 6;	//	間隔Y

	//	stageの進捗バーの数値
	constexpr int kStageBarL      = 600;
	constexpr int kStageBarT      = 1;
	constexpr int kStageBarW      = 250;
	constexpr int kStagelabelGapX = 10;
	//	インベントリの間隔
	constexpr int kInvGapFromLabelX = 12;	//	ラベル右端からインベントリまでの余白
	constexpr int kInvSelectPad     = 0;	//	枠画像がアイコンと同じサイズんら０でおｋ
}

//	文字Bmpの生成を楽にするやつ、CreateBmpStringからMakeTextBmpに変換
Bmp* MakeTextBmp(const TCHAR* text, int size, int bold = 0, int ggo = GGO_BITMAP)
{
	const TCHAR* kFont = TEXT("MS ゴシック");	//	使いたいフォントに
	return CreateBmpString(kFont, size, bold, ggo, text);
}
//	HPの画像
void DrawHpHearts(const GameState& gs, const Assets& assets,	int x, int y, int spacing = -1, float scale = 1.0f)
{
	//	画像がない/未ロードなら何もしない
	if (!assets.heartFull || !assets.heartEmpty)	return;
	//	playerHp	/ playerMaxHpはGameStateに
	const int maxHP    = (gs.playerMaxHP > 0) ? gs.playerMaxHP : 0;
	int hp             = gs.playerHP;
	if (hp < 0)	    hp = 0;
	if (hp > maxHP) hp = maxHP;

	//とりあえず等倍描画(scale != 1.0fを使うならここで拡大処理を別途用意)
	const int iconW = assets.heartFull->width;
	const int step  = (spacing > 0) ? spacing : (iconW + 2);

	int px = x;
	for (int i = 0; i < maxHP; i++, px += step)
	{
		//	i < hp　までは満タン、それ以降は空
		DrawBmp(px, y, (i < hp) ? assets.heartFull : assets.heartEmpty,/*tr=*/true);
	}
}

//	枠と塗りつぶしでプログレスバーを描く
inline void DrawProgressBarI(int left, int top, int right, int bottom, float ratio, int frameColor, int fillColor) 
{
	if (right - left < 4 || bottom - top < 4)	return;

	if (ratio < 0.0f) ratio = 0.0f;
	if (ratio > 1.0f) ratio = 1.0f;

	//	①枠(塗りつぶし=false)
	DrawRect(left, top, right, bottom, frameColor, false);

	//	②中身(塗りつぶし=true)
	const int innerL = left   + 1;
	const int innerT = top    + 1;
	const int innerR = innerL + (int)((right - left - 2) * ratio);
	const int innerB = bottom - 1;

	if (innerR > innerL)
	{
		DrawRect(innerL, innerT, innerR, innerB, fillColor, true);
	}
}

//	TP状態の描画
void DrawTeleportState(const GameState& game, const Assets& assets)
{
	int scoreH = 0;
	if (assets.scoreHud)	scoreH = assets.scoreHud->height;
	const int labelX = game.HUD_X;
	const int labelY = HUD::kScoreY + scoreH + HUD::kTpMarginY;
	//	ACTIVE中は何も書かない
	if (game.tpActive)
	{
		if (assets.tpCdHud)
		{
			DeleteBmp(const_cast<Bmp**>(&assets.tpCdHud));
			const_cast<GameState&>(game).lastTpCDShown = -1;
		}
		return;
	}
	//	READY / COOLDOWNのラベル選択
	Bmp* tpLabel = nullptr;
	if (game.tpCDTimer > 0)
	{
		tpLabel = assets.dash_n_active;
	}
	else
	{
		tpLabel = assets.dash_ready;
	}
	if (tpLabel)	DrawBmp(labelX, labelY, tpLabel, true);
	//	COOLDOWNの間だけ、残り秒数を右側に表示
	if (game.tpCDTimer > 0) {}
	else 
	{
		//	READY中は数値を非表示
		if (assets.tpCdHud)
		{
			DeleteBmp(const_cast<Bmp**>(&assets.tpCdHud));
			const_cast<GameState&>(game).lastTpCDShown = -1;
		}
	}
}

//	武器インベントリ描画
void DrawWeaponInventory(const GameState& game, const Assets& assets)
{
	//	stage1撃破ラベルの文字列を既存t同じフォントサイズで生成し、幅だけ取得
	int total = (STAGE1_TARGET_KILL > 0) ? STAGE1_TARGET_KILL : 15;
	TCHAR txt[32];
	_stprintf(txt, TEXT("%02d / %02d"), game.stageKillCount, total);
	int labelW = 0;
	{
		if (Bmp* temp = MakeTextBmp(txt, 20))
		{
			labelW = temp->width;
			DeleteBmp(&temp);
		}
	}
	//	ラベルの右端をアンカーにして、さらに少し余白を開けた位置をインベントリの起点とする
	const int L = HUD::kStageBarL;
	const int T = HUD::kStageBarT;
	const int R = L + HUD::kStageBarW;
	const int labelX = R + HUD::kStagelabelGapX;
	const int extraOffsetX = (game.stageNo == 1) ? 60 : 0;
	const int anchorX = labelX + labelW + HUD::kInvGapFromLabelX + extraOffsetX;
	//const int anchorX = labelX + labelW + HUD::kInvGapFromLabelX;
	const int anchorY = T;
	struct IconItem { WeaponType type; Bmp* bmp; };
	const IconItem items[] = {
		{WeaponType::Normal,	assets.uiWeaponIconNormal },
		{WeaponType::ChargeBeam,assets.uiWeaponIconBeam	  },
		{WeaponType::Spread,	assets.uiWeaponIconSpread },
	};
	int y = anchorY;
	for (const auto& it : items)
	{
		if (!it.bmp) continue;
		DrawBmp(anchorX, y, it.bmp, true);
		if (it.type == game.currentWeapon && assets.uiWeaponSelectFrame)
		{
			DrawBmp(anchorX - HUD::kInvSelectPad, y - HUD::kInvSelectPad, assets.uiWeaponSelectFrame, true);
		}
		y += it.bmp->height + HUD::kInvItemGapY;
	}

}

void Output(const GameState& game, const Assets& assets)
{
	ClearScreen();

	switch (game.scene)
	{
	//!==========タイトル画面==========
	case Scene::Title:
	{
		if (assets.title)	DrawBmp(0, 0, assets.title, false);

		break;
	}

	//!==========ステージセレクト==========
	case Scene::StageSelect:
	{
		//	ステージセレクト系文字
		{
			if (!game.showHelp)
			{
				if (assets.stage_select)	DrawBmp(game.centerX, 200, assets.stage_select, true);
				if (assets.W_stage)			DrawBmp(game.centerX, 260, assets.W_stage, true);
				if (assets.S_stage)			DrawBmp(game.centerX, 300, assets.S_stage, true);
				if (assets.F_start)			DrawBmp(game.centerX, 360, assets.F_start, true);
				if (assets.tab_info)		DrawBmp(game.centerX, 420, assets.tab_info, true);
			}
		}

		//	カーソル(選択行のみ表示)
		{
			if (!game.showHelp)
			{
				if (assets.arrow)
				{
					int arrowY = (game.stageNo == 0) ? 260 : 300;
					DrawBmp(game.centerX - 50, arrowY, assets.arrow, true);
				}
			}
		}

		//	ヘルプ画面表示
		{
			// Output.cpp : Scene::StageSelect の末尾あたり
			if (game.showHelp)
			{
				if (assets.infoimg)
				{
					const int x = (SCREEN_WIDTH - assets.infoimg->width) / 2;
					const int y = (SCREEN_HEIGHT - assets.infoimg->height) / 2;
					DrawBmp(x, y, assets.infoimg, /*transparent=*/true);
					
				}
				else
				{
				}
			}
		}

		break;
	}

	//!==========ゲームプレイ==========
	case Scene::Play:
	{
		//!	背景画像
		{
			Bmp* bg = assets.background[game.stageNo];
			//!	背景画像(２枚)
			if (bg)
			{
				DrawBmp(game.bgX			   , UI_HEIGHT, bg, false);
				DrawBmp(game.bgX + game.bgWidth, UI_HEIGHT, bg, false);
			}
		}

		//!	自機画像
		{
			if (assets.player)
			{
				//	無敵時間中は点滅(5フレーム毎に表示/非表示を切り替え)
				if (!game.isInvincible || (game.invincibleTimer / 5) % 2 == 0)	DrawBmp(game.playerX, game.playerY, assets.player, true);
			}
		}

		//! Teleport 円（Windup=出発点で縮小 / Recover=到着点で拡大）
		{
			if (game.tpActive && assets.tpCircleCount > 0) {
				// 正しいラムダの書き方（キャプチャ+[引数]->戻り値）
				auto pickIdx = [&](int phase, int timer) -> int {
					int last = assets.tpCircleCount - 1;
					if (phase == 1) { // Windup：縮小（大→小）
						int done = TP_WINDUP_FRAMES - timer;
						int idx = (done * assets.tpCircleCount) / (TP_WINDUP_FRAMES + 1);
						return (idx > last) ? last : idx;
					}
					else {           // Recover：拡大（小→大）
						int done = TP_RECOVER_FRAMES - timer;
						int idx = last - (done * assets.tpCircleCount) / (TP_RECOVER_FRAMES + 1);
						return (idx < 0) ? 0 : idx;
					}
					};

				if (game.tpPhase == 1) {
					int idx = pickIdx(1, game.tpTimer);
					Bmp* b = assets.tpCircle[idx];
					if (b && assets.player) {
						int x = game.tpStartX + assets.player->width / 2 - b->width / 2;
						int y = game.tpStartY + assets.player->height / 2 - b->height / 2;
						DrawBmp(x, y, b, true);
					}
				}
				else if (game.tpPhase == 3) {
					int idx = pickIdx(3, game.tpTimer);
					Bmp* b = assets.tpCircle[idx];
					if (b && assets.player) {
						int x = game.tpEndX + assets.player->width / 2 - b->width / 2;
						int y = game.tpEndY + assets.player->height / 2 - b->height / 2;
						DrawBmp(x, y, b, true);
					}
				}
			}
		}
	
		//!	弾画像
		{
			for (int i = 0; i < PLAYER_BULLET_MAX; i++)
			{
				if (!game.playerBullets[i].active)	continue;

				//	弾の種類に応じて画像を変える
				Bmp* bulletImg = nullptr;
				if		(game.playerBullets[i].type == WeaponType::ChargeBeam && assets.chargeBeam)		bulletImg = assets.chargeBeam;
				else if (game.playerBullets[i].type == WeaponType::Spread && assets.spreadBullet)		bulletImg = assets.spreadBullet;
				else if (assets.bullet)																	bulletImg = assets.bullet;
				if (bulletImg)		DrawBmp(game.playerBullets[i].x, game.playerBullets[i].y, bulletImg, true);

			}
		}

		//!	岩画像
		{
			if (assets.rock) 
			{
				for (int i = 0; i < ROCK_MAX; i++)
				{
					int ry = game.rocks[i].y;
					int rh = assets.rock ? assets.rock->height : 40;
					if (ry < PLAY_Y_MIN) continue; // UI帯にかかる岩は描画しない
					if (!game.rocks[i].alive)	continue;
					DrawBmp(game.rocks[i].x, game.rocks[i].y, assets.rock, true);
				}
			}
		}

		//!	ピックアップ(アイテム)の描画
		{
			for (int i = 0; i < PICKUP_MAX; i++)
			{
				if (!game.pickups[i].active)	continue;

				bool visible = true;
				if (game.pickups[i].life <= 60)
				{
					visible = ((game.playTimeFrames / 5) % 2 == 0);
				}
				if (!visible)	continue;
				Bmp* pimg = nullptr;
				switch (game.pickups[i].type)
				{
				case WeaponType::Normal:	pimg = assets.pickupNormal;	break;
				case WeaponType::ChargeBeam:pimg = assets.pickupBeam;	break;
				case WeaponType::Spread:	pimg = assets.pickupSpread;	break;
				}
				if (pimg)
				{
					DrawBmp(game.pickups[i].x, game.pickups[i].y, pimg, true);
				}
			}
		}

		//!	爆発
		{
			for (int i = 0; i < GameState::EXPLOSION_MAX; i++)
			{
				const auto& e = game.explosions[i];
				if (!e.active)	continue;
				int f = e.frame;
				if (f >= 0 && f < assets.explosionCount)
				{
					Bmp* b = assets.explosionFrames[f];
					if (b)
					{
						//	中心合わせ(cx,cy から左上に変換)
						int drawX = e.x - b->width  / 2;
						int drawY = e.y - b->height / 2;
						DrawBmp(drawX, drawY, b, true);
					}
				}
			}
		}

		//!	左上の経過時間表示
		{
			int currentSeconds = game.playTimeFrames / 60;	//	60フレーム
			if (game.lastTimeHud != currentSeconds)
			{
				if (assets.timeHud)
				{
					DeleteBmp(const_cast<Bmp**>(&assets.timeHud));
				}
				int minutes = currentSeconds / 60;
				int seconds = currentSeconds % 60;
				TCHAR timeText[64];
				_stprintf(timeText, TEXT("TIME : %02d:%02d"), minutes, seconds);
				const_cast<Assets&>	  (assets).timeHud   = MakeTextBmp(timeText, 22);
				const_cast<GameState&>(game).lastTimeHud = currentSeconds;
			}
			if (assets.timeHud)	DrawBmp(20, 20, assets.timeHud, true);
		}

		//!	右上のスコア表示
		{
			if (game.lastScoreHud != game.score)
			{
				if (assets.scoreHud) {
					//	const_castでメンバを書き換える
					DeleteBmp(const_cast<Bmp**>(&assets.scoreHud));
				}
				TCHAR sc[64];
				_stprintf(sc, TEXT("SCORE : %06d"), game.score);
				//	const_castを用いて生成結果をAssetsに格納
				const_cast<Assets&>(assets).scoreHud      = MakeTextBmp(sc, 22);
				//	lastScoreHud は GameState のメンバなので const_cast
				const_cast<GameState&>(game).lastScoreHud = game.score;
			}
			if (assets.scoreHud)	DrawBmp(game.HUD_X, HUD::kScoreY, assets.scoreHud, true);
		}

		//!	選択武器表示
		{
			DrawWeaponInventory(game, assets);
		}

		//!	TP状態HUD
		{
			DrawTeleportState(game, assets);
		}
		
		//!	HPHUD
		{
			DrawHpHearts(game, assets,/*x=*/375,/*y=*/40,/*spacing=*/-1,/*scale=*/1.0f);
		}

		//!	武器表示HUD
		{
			int weaponTypeNow = (int)game.currentWeapon;
			if (game.lastWeaponHud != weaponTypeNow)
			{
				if (assets.weaponHud)
				{
					DeleteBmp(const_cast<Bmp**>(&assets.weaponHud));
				}
				TCHAR weaponText[64];
				if		(game.currentWeapon == WeaponType::Normal)		_stprintf(weaponText, TEXT("WEAPON : NORMAL (1)"));
				else if (game.currentWeapon == WeaponType::ChargeBeam)	_stprintf(weaponText, TEXT("WEAPON : BEAM (2)"));
				else if (game.currentWeapon == WeaponType::Spread)		_stprintf(weaponText, TEXT("WEAPON : SPREAD (3)"));

				const_cast<Assets&>	  (assets).weaponHud   = MakeTextBmp(weaponText, 22);
				const_cast<GameState&>(game).lastWeaponHud = weaponTypeNow;
			}
			if (assets.weaponHud)	DrawBmp(game.HUD_X, 104, assets.weaponHud, true);
		}

		//!	残弾数ゲージ表示
		{
			{
				const int gaugeStartX  = 20;   // 左上X
				const int gaugeStartY  = 50;   // 左上Y（通常弾の行）
				const int gaugeSpacing = 28;  // 行間
				const int iconW        = 16;         // 新アイコン幅
				const int iconH        = 25;         // 新アイコン高
				const int step         = 22;         // 並べ間隔（必要なら微調整）

				// --- 通常弾 ---
				if (assets.labelNormal) DrawBmp(gaugeStartX, gaugeStartY, assets.labelNormal, true);
				if (assets.ammoIconNormal)
				{
					for (int i = 0; i < MAX_AMMO_NORMAL; ++i)
					{
						// ※満/空の2種類が無い場合は「残弾が残っている数だけ描く」方式にする
						if (i < game.ammoNormal)
						{
							DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY, assets.ammoIconNormal, true);
						}
						else
						{
							// 何も描かない（空スロットは非表示）or 半透明表示などにしたい場合は工夫
							if (assets.ammoIconNormalEmpty) 
							{
								DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY, assets.ammoIconNormalEmpty, true);
							}
						}
					}
				}

				// --- ビーム ---
				if (assets.labelCharge) DrawBmp(gaugeStartX, gaugeStartY + gaugeSpacing, assets.labelCharge, true);
				if (assets.ammoIconBeam)
				{
					for (int i = 0; i < MAX_AMMO_BEAM; ++i)
					{
						if (i < game.ammoBeam)
						{
							DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing, assets.ammoIconBeam, true);
						}
						else 
						{
							if (assets.ammoIconBeamEmpty)
							{
								DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing, assets.ammoIconBeamEmpty, true);
							}
						}
					}
				}

				// --- 拡散 ---
				if (assets.labelSpread) DrawBmp(gaugeStartX, gaugeStartY + gaugeSpacing * 2, assets.labelSpread, true);
				if (assets.ammoIconSpread)
				{
					for (int i = 0; i < MAX_AMMO_SPREAD; ++i)
					{
						if (i < game.ammoSpread)
						{
							DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing * 2, assets.ammoIconSpread, true);
						}
						else
						{
							if (assets.ammoIconSpreadEmpty)
							{
								DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing * 2, assets.ammoIconSpreadEmpty, true);
							}
						}
					}
				}
			}
		}
		
		//!	リロード
		{
			if (game.normalReloading)
			{
				//	進捗 = (経過) / (所要)
				float prog  = 1.0f - (float)game.normalReloadTimer / (float)NORMAL_RELOAD_FRAMES;
				//	バーの位置とサイズ
				const int L = 600;
				const int T = 70;
				const int R = L + 250;
				const int B = T + 16;
				//	枠 : YELLOW　/　中身 : LIGHTGREEN
				DrawProgressBarI(L, T, R, B, prog, LIGHTBLUE, BLUE);
			
			}
		}

		//!	スコアポップアップ描画
		{
			for (int i = 0; i < game.popupCount; i++)
			{
				const auto& p = game.popups[i];
				if (!p.alive) continue;
				if (p.slot >= 0 && p.slot < GameState::POPUP_MAX)
				{
					Bmp* bmp = assets.popups[p.slot];
					if (bmp)DrawBmp(p.x, p.y, bmp, true);
				}
			}
		}


		//!	敵機画像
		{
			for (int i = 0; i < ENEMY_MAX; i++)	//	敵機の数だけ繰り返す
			{
				//敵機が生きている場合のみ描画
				if (!game.enemies[i].alive)	continue;
					
				//	種類で画像を切り替え
				Bmp* ebmp = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy02 : assets.enemy01;
				if (ebmp == nullptr)	//	フォールバック
				{
					ebmp = (game.enemies[i].type == EnemyType::Vertical) ? assets.enemy01 : assets.enemy02;
				}

				DrawBmp(game.enemies[i].x, game.enemies[i].y, ebmp, true);
			}
		}

		//!	敵弾画像
		{
			for (int i = 0; i < ENEMY_BULLET_MAX; i++)
			{
				//	敵弾が存在する場合のみ描画
				if (game.enemyBullets[i].active)	DrawBmp(game.enemyBullets[i].x - 50, game.enemyBullets[i].y + 15, assets.enemyBullet, true);
			}
		}
		
		//!	stage1の撃破数、stage2の時間経過バー描画
		{
			//!	stage1の撃破数
			if (game.stageNo == 0)
			{
				//!	プログレスバー
				const int total      = (STAGE1_TARGET_KILL > 0) ? STAGE1_TARGET_KILL : 15;
				float prog           = (float)game.stageKillCount / (float)total;
				if (prog > 1.0f)prog = 1.0f;

				const int L = 600;		//左
				const int T = 25;		//上
				const int R = L + 250;	//右
				const int B = T + 16;	//下

				//	枠 : LIGHTBLUE　/　中身 : LIGHTCYAN
				DrawProgressBarI(L, T, R, B, prog, LIGHTBLUE, CYAN);

				//!	テキスト
				if (game.stageNo == 0) {
					TCHAR txt[32];
					int total = (STAGE1_TARGET_KILL > 0) ? STAGE1_TARGET_KILL : 15;
					_stprintf(txt, TEXT("%02d / %02d"), game.stageKillCount, total);
					if (Bmp* b = MakeTextBmp(txt, 20)) {
						DrawBmp(R + 10, T, b, true);
						DeleteBmp(&b);
					}
				}

			}

			//!	stage2のスコアバー？描画
			if (game.stageNo == 1)
				{
#ifdef STAGE2_SURVIVE_SECONDS
				const int totalSec    = (STAGE2_SURVIVE_SECONDS > 0) ? STAGE2_SURVIVE_SECONDS : 60;
#else
				const int totalSec    = 120 * 60; // 既定=2分
#endif
				const int totalFrames = totalSec * 60;

				// 進捗 = 経過フレーム / 目標フレーム（0.0～1.0）
				float prog = static_cast<float>(game.playTimeFrames) / static_cast<float>(totalFrames);
					
				if (prog > 1.0f) prog = 1.0f;
				// 既存の見た目と同じ領域（(550,50)-(910,100)）に描画
				const int L = 600;		//左
				const int T = 25;		//上
				const int R = L + 250;	//右
				const int B = T + 16;	//下
				// 枠：YELLOW / 中身：LIGHTGREEN（パレット色）
				DrawProgressBarI(L, T, R, B, prog, LIGHTBLUE, CYAN);

				//	ラベル
				int nowSec                    = game.playTimeFrames / 60;
				if (nowSec > totalSec) nowSec = totalSec;
				const int mm                  = nowSec / 60;
				const int ss                  = nowSec % 60;
				TCHAR txt[32];
				_stprintf(txt, TEXT("%02d:%02d / %02d:00"), mm, ss, totalSec);
				if (Bmp* b = MakeTextBmp(txt, 20))
				{
					DrawBmp(R+10, T, b, true);
				}
			}
			
		}

		break;
	}

	//!==========ボス戦==========
	case Scene::Boss:
	{
		//!	背景画像
		{
			Bmp* bg = assets.background[game.stageNo];
			//!	背景画像(２枚)
			if (bg)
			{
				DrawBmp(game.bgX, UI_HEIGHT, bg, false);
				DrawBmp(game.bgX + game.bgWidth, UI_HEIGHT, bg, false);
			}
		}

		//!	自機画像
		{
			if (assets.player)
			{
				//	無敵時間中は点滅(5フレーム毎に表示/非表示を切り替え)
				if (!game.isInvincible || (game.invincibleTimer / 5) % 2 == 0)	DrawBmp(game.playerX, game.playerY, assets.player, true);
			}
		}

		//! Teleport 円（Windup=出発点で縮小 / Recover=到着点で拡大）
		{
			if (game.tpActive && assets.tpCircleCount > 0) {
				// 正しいラムダの書き方（キャプチャ+[引数]->戻り値）
				auto pickIdx = [&](int phase, int timer) -> int {
					int last = assets.tpCircleCount - 1;
					if (phase == 1) { // Windup：縮小（大→小）
						int done = TP_WINDUP_FRAMES - timer;
						int idx = (done * assets.tpCircleCount) / (TP_WINDUP_FRAMES + 1);
						return (idx > last) ? last : idx;
					}
					else {           // Recover：拡大（小→大）
						int done = TP_RECOVER_FRAMES - timer;
						int idx = last - (done * assets.tpCircleCount) / (TP_RECOVER_FRAMES + 1);
						return (idx < 0) ? 0 : idx;
					}
					};

				if (game.tpPhase == 1) {
					int idx = pickIdx(1, game.tpTimer);
					Bmp* b = assets.tpCircle[idx];
					if (b && assets.player) {
						int x = game.tpStartX + assets.player->width / 2 - b->width / 2;
						int y = game.tpStartY + assets.player->height / 2 - b->height / 2;
						DrawBmp(x, y, b, true);
					}
				}
				else if (game.tpPhase == 3) {
					int idx = pickIdx(3, game.tpTimer);
					Bmp* b = assets.tpCircle[idx];
					if (b && assets.player) {
						int x = game.tpEndX + assets.player->width / 2 - b->width / 2;
						int y = game.tpEndY + assets.player->height / 2 - b->height / 2;
						DrawBmp(x, y, b, true);
					}
				}
			}
		}

		//!	弾画像
		{
			for (int i = 0; i < PLAYER_BULLET_MAX; i++)
			{
				if (!game.playerBullets[i].active)	continue;

				//	弾の種類に応じて画像を変える
				Bmp* bulletImg = nullptr;
				if (game.playerBullets[i].type == WeaponType::ChargeBeam && assets.chargeBeam)		bulletImg = assets.chargeBeam;
				else if (game.playerBullets[i].type == WeaponType::Spread && assets.spreadBullet)		bulletImg = assets.spreadBullet;
				else if (assets.bullet)																	bulletImg = assets.bullet;
				if (bulletImg)		DrawBmp(game.playerBullets[i].x, game.playerBullets[i].y, bulletImg, true);

			}
		}

		//!	岩画像
		{
			if (assets.rock)
			{
				for (int i = 0; i < ROCK_MAX; i++)
				{
					int ry = game.rocks[i].y;
					int rh = assets.rock ? assets.rock->height : 40;
					if (ry < PLAY_Y_MIN) continue; // UI帯にかかる岩は描画しない
					if (!game.rocks[i].alive)	continue;
					DrawBmp(game.rocks[i].x, game.rocks[i].y, assets.rock, true);
				}
			}
		}

		//!	ピックアップ(アイテム)の描画
		{
			for (int i = 0; i < PICKUP_MAX; i++)
			{
				if (!game.pickups[i].active)	continue;

				bool visible = true;
				if (game.pickups[i].life <= 60)
				{
					visible = ((game.playTimeFrames / 5) % 2 == 0);
				}
				if (!visible)	continue;
				Bmp* pimg = nullptr;
				switch (game.pickups[i].type)
				{
				case WeaponType::Normal:	pimg = assets.pickupNormal;	break;
				case WeaponType::ChargeBeam:pimg = assets.pickupBeam;	break;
				case WeaponType::Spread:	pimg = assets.pickupSpread;	break;
				}
				if (pimg)
				{
					DrawBmp(game.pickups[i].x, game.pickups[i].y, pimg, true);
				}
			}
		}

		//!	左上の経過時間表示
		{
			int currentSeconds = game.playTimeFrames / 60;	//	60フレーム
			if (game.lastTimeHud != currentSeconds)
			{
				if (assets.timeHud)
				{
					DeleteBmp(const_cast<Bmp**>(&assets.timeHud));
				}
				int minutes = currentSeconds / 60;
				int seconds = currentSeconds % 60;
				TCHAR timeText[64];
				_stprintf(timeText, TEXT("TIME : %02d:%02d"), minutes, seconds);
				const_cast<Assets&>	  (assets).timeHud = MakeTextBmp(timeText, 22);
				const_cast<GameState&>(game).lastTimeHud = currentSeconds;
			}
			if (assets.timeHud)	DrawBmp(20, 20, assets.timeHud, true);
		}

		//!	右上のスコア表示
		{
			if (game.lastScoreHud != game.score)
			{
				if (assets.scoreHud) {
					//	const_castでメンバを書き換える
					DeleteBmp(const_cast<Bmp**>(&assets.scoreHud));
				}
				TCHAR sc[64];
				_stprintf(sc, TEXT("SCORE : %06d"), game.score);
				//	const_castを用いて生成結果をAssetsに格納
				const_cast<Assets&>(assets).scoreHud = MakeTextBmp(sc, 22);
				//	lastScoreHud は GameState のメンバなので const_cast
				const_cast<GameState&>(game).lastScoreHud = game.score;
			}
			if (assets.scoreHud)	DrawBmp(game.HUD_X, HUD::kScoreY, assets.scoreHud, true);
		}

		//!	選択武器表示
		{
			DrawWeaponInventory(game, assets);
		}

		//!	TP状態HUD
		{
			DrawTeleportState(game, assets);
		}

		//!	HPHUD
		{
			DrawHpHearts(game, assets,/*x=*/375,/*y=*/40,/*spacing=*/-1,/*scale=*/1.0f);
		}

		//!	武器表示HUD
		{
			int weaponTypeNow = (int)game.currentWeapon;
			if (game.lastWeaponHud != weaponTypeNow)
			{
				if (assets.weaponHud)
				{
					DeleteBmp(const_cast<Bmp**>(&assets.weaponHud));
				}
				TCHAR weaponText[64];
				if (game.currentWeapon == WeaponType::Normal)		_stprintf(weaponText, TEXT("WEAPON : NORMAL (1)"));
				else if (game.currentWeapon == WeaponType::ChargeBeam)	_stprintf(weaponText, TEXT("WEAPON : BEAM (2)"));
				else if (game.currentWeapon == WeaponType::Spread)		_stprintf(weaponText, TEXT("WEAPON : SPREAD (3)"));

				const_cast<Assets&>	  (assets).weaponHud = MakeTextBmp(weaponText, 22);
				const_cast<GameState&>(game).lastWeaponHud = weaponTypeNow;
			}
			if (assets.weaponHud)	DrawBmp(game.HUD_X, 104, assets.weaponHud, true);
		}

		//!	残弾数ゲージ表示
		{
			{
				const int gaugeStartX = 20;   // 左上X
				const int gaugeStartY = 50;   // 左上Y（通常弾の行）
				const int gaugeSpacing = 28;  // 行間
				const int iconW = 16;         // 新アイコン幅
				const int iconH = 25;         // 新アイコン高
				const int step = 22;         // 並べ間隔（必要なら微調整）

				// --- 通常弾 ---
				if (assets.labelNormal) DrawBmp(gaugeStartX, gaugeStartY, assets.labelNormal, true);
				if (assets.ammoIconNormal)
				{
					for (int i = 0; i < MAX_AMMO_NORMAL; ++i)
					{
						// ※満/空の2種類が無い場合は「残弾が残っている数だけ描く」方式にする
						if (i < game.ammoNormal)
						{
							DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY, assets.ammoIconNormal, true);
						}
						else
						{
							// 何も描かない（空スロットは非表示）or 半透明表示などにしたい場合は工夫
							if (assets.ammoIconNormalEmpty)
							{
								DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY, assets.ammoIconNormalEmpty, true);
							}
						}
					}
				}

				// --- ビーム ---
				if (assets.labelCharge) DrawBmp(gaugeStartX, gaugeStartY + gaugeSpacing, assets.labelCharge, true);
				if (assets.ammoIconBeam)
				{
					for (int i = 0; i < MAX_AMMO_BEAM; ++i)
					{
						if (i < game.ammoBeam)
						{
							DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing, assets.ammoIconBeam, true);
						}
						else
						{
							if (assets.ammoIconBeamEmpty)
							{
								DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing, assets.ammoIconBeamEmpty, true);
							}
						}
					}
				}

				// --- 拡散 ---
				if (assets.labelSpread) DrawBmp(gaugeStartX, gaugeStartY + gaugeSpacing * 2, assets.labelSpread, true);
				if (assets.ammoIconSpread)
				{
					for (int i = 0; i < MAX_AMMO_SPREAD; ++i)
					{
						if (i < game.ammoSpread)
						{
							DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing * 2, assets.ammoIconSpread, true);
						}
						else
						{
							if (assets.ammoIconSpreadEmpty)
							{
								DrawBmp(gaugeStartX + 70 + i * step, gaugeStartY + gaugeSpacing * 2, assets.ammoIconSpreadEmpty, true);
							}
						}
					}
				}
			}
		}

		//!	リロード
		{
			if (game.normalReloading)
			{
				//	進捗 = (経過) / (所要)
				float prog = 1.0f - (float)game.normalReloadTimer / (float)NORMAL_RELOAD_FRAMES;
				//	バーの位置とサイズ
				const int L = 600;
				const int T = 70;
				const int R = L + 250;
				const int B = T + 16;
				//	枠 : YELLOW　/　中身 : LIGHTGREEN
				DrawProgressBarI(L, T, R, B, prog, LIGHTBLUE, BLUE);

			}
		}

		//!	スコアポップアップ描画
		{
			for (int i = 0; i < game.popupCount; i++)
			{
				const auto& p = game.popups[i];
				if (!p.alive) continue;
				if (p.slot >= 0 && p.slot < GameState::POPUP_MAX)
				{
					Bmp* bmp = assets.popups[p.slot];
					if (bmp)DrawBmp(p.x, p.y, bmp, true);
				}
			}
		}

		//! ボス本体
		{
			if (game.boss.alive)
			{
				const int bw = assets.boss->width;
				const int bh = assets.boss->height;
				const int bx = game.boss.x;
				const int by = game.boss.y;	
				SetPalette(assets.boss);
				DrawBmp(bx, by, assets.boss, true);
				SetPalette(assets.player);
			}
		}

		//!	ボス弾
		{
			Bmp* bb = assets.enemyBullet; // 流用
			if (bb) {
				for (int i = 0; i < BOSS_BULLET_MAX; ++i) {
					if (!game.bossBullets[i].active) continue;
					DrawBmp(game.bossBullets[i].x, game.bossBullets[i].y, bb, true);
				}
			}
		}

		//! BOSS HPバー（スコアの代わり）
		{
			const int L = 600;		//左
			const int T = 25;		//上
			const int R = L + 250;	//右
			const int B = T + 16;	//下

			const int cur = (game.bossHpShown > 0) ? game.bossHpShown : 0;
			const int max = (game.boss.maxHP > 0)  ? game.boss.maxHP  : 1;

			float ratio = static_cast<float>(cur) / static_cast<float>(max);
			if (ratio < 0.0f) ratio = 0.0f;
			if (ratio > 1.0f) ratio = 1.0f;
			DrawRect(L, T, R, B, LIGHTBLUE, 0);						//	外枠
			DrawProgressBarI(L, T, R, B, ratio, LIGHTBLUE, CYAN);	//	中身

			// "BOSS" ラベル（任意）
			if (Bmp* lab = MakeTextBmp(TEXT("BOSS"), 22)) {
				DrawBmp(L - 70, T - 2, lab, true);
				DeleteBmp(&lab);
			}
		}

		break;
	}
	
	//!==========ポーズ画面==========
	case Scene::Pause:
	{
		Bmp* bg = assets.background[game.stageNo];
		if (bg)
		{
			DrawBmp(game.bgX               , UI_HEIGHT, bg, false);
			DrawBmp(game.bgX + game.bgWidth, UI_HEIGHT, bg, false);
		}
		//	案内
		{
			if (assets.returnGame)			DrawBmp(game.centerX - 150, 360, assets.returnGame, true);
			if (assets.returnStageSelect)	DrawBmp(game.centerX - 96 , 400, assets.returnStageSelect, true);
		}

		break;
	}

	//!==========ゲームオーバー画面==========
	case Scene::GameOver:
	{
		//	背景
		{
			Bmp* goBg = assets.background[game.stageNo];
			if (goBg)
			{
				DrawBmp(game.bgX               , UI_HEIGHT, goBg, false);
				DrawBmp(game.bgX + game.bgWidth, UI_HEIGHT, goBg, false);
			}
		}

		//	GAME OVERテキスト
		{
			if (assets.gameOver)
			{
				DrawBmp(300, 200, assets.gameOver, true);
			}
		}

		//	スコア表示
		{
			TCHAR finalScore[64];
			_stprintf(finalScore, TEXT("FINAL SCORE : %06d"), game.score);
			Bmp* finalScoreBmp = MakeTextBmp(finalScore, 28);
			if (finalScoreBmp)
			{
				DrawBmp(300, 280, finalScoreBmp, true);
				DeleteBmp(&finalScoreBmp);	//	一時的な表示なので即削除
			}
		}

		//	クリアタイム表示
		{
			int totalSeconds = game.playTimeFrames / 60;
			int minutes      = totalSeconds / 60;
			int seconds      = totalSeconds % 60;
			TCHAR surviveTime[64];
			_stprintf(surviveTime, TEXT("SURVIVE TIME : %02d:%02d"), minutes, seconds);
			Bmp* surviveTimeBmp = MakeTextBmp(surviveTime, 24);
			if (surviveTimeBmp)
			{
				DrawBmp(300, 320, surviveTimeBmp, true);
				DeleteBmp(&surviveTimeBmp);
			}
		}

		//	リトライ案内
		{
			if (assets.retry)
			{
				DrawBmp(500, 400, assets.retry, true);
			}
		}
		break;
	}

	//!==========ゲームクリア画面==========
	case Scene::GameClear:
	{
		//	背景
		{
			Bmp* bg = assets.background[game.stageNo];
			if (bg)
			{
				DrawBmp(game.bgX			   , UI_HEIGHT, bg, false);
				DrawBmp(game.bgX + game.bgWidth, UI_HEIGHT, bg, false);
			}
		}
		//	GAME CLEAR!テキスト
		{
			if (assets.gameClear)
			{
				DrawBmp(300, 200, assets.gameClear, true);
			}
		}

		//	スコア表示
		{
			TCHAR clearScore[64];
			_stprintf(clearScore, TEXT("SCORE : %06d"), game.score);
			Bmp* scoreBmp = MakeTextBmp(clearScore, 28);
			if (scoreBmp)
			{
				DrawBmp(300, 280, scoreBmp, true);
				DeleteBmp(&scoreBmp);
			}
		}

		//	クリアタイム表示
		{
			int totalSeconds = game.playTimeFrames / 60;
			int minutes = totalSeconds / 60;
			int seconds = totalSeconds % 60;
			TCHAR clearTime[64];
			_stprintf(clearTime, TEXT("CLEAR TIME : %02d:%02d"), minutes, seconds);
			if (Bmp* timeBmp = MakeTextBmp(clearTime, 24))
			{
				DrawBmp(300, 320, timeBmp, true);
				DeleteBmp(&timeBmp);
			}
		}


		//	案内
		{
			if (assets.returnStageSelect)	DrawBmp(game.centerX - 96, 400, assets.returnStageSelect, true);
		}

		break;
	}
	}
	
	PrintFrameBuffer();
	FlipScreen();
}
