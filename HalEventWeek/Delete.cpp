#include"common.h"
#include "audio_player.h"

void Delete(GameState& game, Assets& assets) {
	//	終了時に生成したBmpを開放しておく
	for (int i = 0; i < GameState::POPUP_MAX; i++)
	{
		if (assets.popups[i]) { DeleteBmp(&assets.popups[i]); }
	}

	for (int i = 0; i < assets.tpCircleCount; i++)
	{
		if (assets.tpCircle[i]) { DeleteBmp(&assets.tpCircle[i]); }
	}
	assets.tpCircleCount = 0;

	for (int i = 0; i < assets.explosionCount; i++)
	{
		if (assets.explosionFrames[i]) { DeleteBmp(&assets.explosionFrames[i]); }
	}
	assets.explosionCount = 0;

	if (assets.stage_select) { DeleteBmp(&assets.stage_select); }
	if (assets.W_stage) { DeleteBmp(&assets.W_stage); }
	if (assets.S_stage) { DeleteBmp(&assets.S_stage); }
	if (assets.F_start) { DeleteBmp(&assets.F_start); }
	if (assets.arrow) { DeleteBmp(&assets.arrow); }
	if (assets.title) { DeleteBmp(&assets.title); }
	if (assets.tab_info) { DeleteBmp(&assets.tab_info); }
	if (assets.player) { DeleteBmp(&assets.player); }
	if (assets.enemy01) { DeleteBmp(&assets.enemy01); }
	if (assets.enemy02) { DeleteBmp(&assets.enemy02); }
	if (assets.boss) { DeleteBmp(&assets.boss); }
	if (assets.bullet) { DeleteBmp(&assets.bullet); }
	if (assets.chargeBeam) { DeleteBmp(&assets.chargeBeam); }
	if (assets.spreadBullet) { DeleteBmp(&assets.spreadBullet); }
	if (assets.enemyBullet) { DeleteBmp(&assets.enemyBullet); }
	if (assets.background[0]) { DeleteBmp(&assets.background[0]); }
	if (assets.background[1]) { DeleteBmp(&assets.background[1]); }
	if (assets.rock) { DeleteBmp(&assets.rock); }
	if (assets.pickupNormal) { DeleteBmp(&assets.pickupNormal); }
	if (assets.pickupBeam) { DeleteBmp(&assets.pickupBeam); }
	if (assets.pickupSpread) { DeleteBmp(&assets.pickupSpread); }

	if (assets.ammoIconNormal) { DeleteBmp(&assets.ammoIconNormal); }
	if (assets.ammoIconBeam) { DeleteBmp(&assets.ammoIconBeam); }
	if (assets.ammoIconSpread) { DeleteBmp(&assets.ammoIconSpread); }
	if (assets.ammoIconNormalEmpty) { DeleteBmp(&assets.ammoIconNormalEmpty); }
	if (assets.ammoIconBeamEmpty) { DeleteBmp(&assets.ammoIconBeamEmpty); }
	if (assets.ammoIconSpreadEmpty) { DeleteBmp(&assets.ammoIconSpreadEmpty); }


	if (assets.heartEmpty) { DeleteBmp(&assets.heartEmpty); }
	if (assets.heartFull) { DeleteBmp(&assets.heartFull); }

	if (assets.uiWeaponIconNormal) { DeleteBmp(&assets.uiWeaponIconNormal); }
	if (assets.uiWeaponIconBeam) { DeleteBmp(&assets.uiWeaponIconBeam); }
	if (assets.uiWeaponIconSpread) { DeleteBmp(&assets.uiWeaponIconSpread); }
	if (assets.uiWeaponSelectFrame) { DeleteBmp(&assets.uiWeaponSelectFrame); }

	//	TP状態
	if (assets.dash_ready) { DeleteBmp(&assets.dash_ready); }
	if (assets.dash_active) { DeleteBmp(&assets.dash_active); }
	if (assets.dash_n_active) { DeleteBmp(&assets.dash_n_active); }

	// 武器ラベル
	if (assets.labelNormal) { DeleteBmp(&assets.labelNormal); }
	if (assets.labelCharge) { DeleteBmp(&assets.labelCharge); }
	if (assets.labelSpread) { DeleteBmp(&assets.labelSpread); }

	// ゲーム状態テキスト
	if (assets.gameOver) { DeleteBmp(&assets.gameOver); }
	if (assets.retry) { DeleteBmp(&assets.retry); }
	if (assets.returnStageSelect) { DeleteBmp(&assets.returnStageSelect); }
	if (assets.returnGame) { DeleteBmp(&assets.returnGame); }
	if (assets.gameClear) { DeleteBmp(&assets.gameClear); }

	// ヘルプ／インフォ
	if (assets.infoimg) { DeleteBmp(&assets.infoimg); }
	// 動的HUDテキスト
	if (assets.scoreHud) { DeleteBmp(&assets.scoreHud); }
	if (assets.hpHud) { DeleteBmp(&assets.hpHud); }
	if (assets.timeHud) { DeleteBmp(&assets.timeHud); }
	if (assets.weaponHud) { DeleteBmp(&assets.weaponHud); }
	if (assets.tpCdHud) { DeleteBmp(&assets.tpCdHud); }

	AudioSystem_Shutdown();  //サウンドファイル
}