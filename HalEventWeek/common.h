#pragma once
#include"conioex_New.h"
#include<cstdlib>
#include<ctime>
#include <algorithm>
#include<thread>

#define SCREEN_WIDTH		    (1500)
#define SCREEN_HEIGHT		    (750)
#define FONT_SIZE	            (1)		//	フォントサイズの定義

#define UI_HEIGHT			    (150)	//UI領域
#define PLAY_X_MIN			    (0)
#define PLAY_X_MAX			    (SCREEN_WIDTH)
#define	PLAY_Y_MIN			    (UI_HEIGHT)
#define PLAY_Y_MAX			    (SCREEN_HEIGHT)

#define ENEMY_MAX	            (15)	//	敵機の最大数
#define ENEMY_TYPE2_MAX		    (3)		//	2種類目の敵の最大数
#define ENEMY_BULLET_MAX	    (30)	//	敵の弾の最大数
#define ENEMY_SPAWN_MIN			(45)	//	敵スポーン間隔の最小フレーム、0.75秒
#define ENEMY_SPAWN_MAX			(120)	//	敵スポーン間隔の最大フレーム、2秒

#define BOSS_WIDTH				(150)
#define BOSS_HEIGHT				(600)
#define BOSS_MAX_HP				(1250)
#define BOSS_ENTRANCE_SPEED		(8)		//	ボスが出てくる時の速度
#define BOSS_BULLET_MAX			(120)	//	ボス弾の最大数
#define BOSS_BULLET_SPEED_AIM	(8)		//	狙い撃ちの速さ
#define BOSS_BULLET_SPEED_SP	(9)		//	拡散弾の速さ
#define BOSS_FIRE_INTERVAL_SP	(65)	//	ボスの拡散弾発射間隔
#define BOSS_FIRE_INTERVAL_AIM	(30)	//	ボスの狙い撃ち発射間隔
#define BOSS_SPREAD_COUNT		(7)		//	拡散弾の数
#define BOSS_SPREAD_DEG_STEP	(15)	//	中心から±45度

#define PLAYER_MAX_HP		    (3)		//	プレイヤー最大HP
#define INVINCIBLE_TIME		    (120)	//	無敵時間フレーム数(約2秒)
#define SPREAD_BULLET_MAX	    (1)		//	拡散弾の弾数
#define PLAYER_BULLET_MAX	    (40)	//	プレイヤーの弾の最大数(拡散弾用)
#define MAX_AMMO_NORMAL		    (7)		//	通常弾の最大弾数
#define MAX_AMMO_BEAM		    (3)		//	チャージビームの最大弾数
#define MAX_AMMO_SPREAD		    (3)		//	拡散弾の最大弾数

#define TP_DISTANCE				(240)	//	TP移動距離
#define TP_WINDUP_FRAMES		(6)		//	出発側の縮小演出フレーム
#define TP_RECOVER_FRAMES		(6)		//	到着側の拡大演出フレーム
#define TP_INVINCIBLE_FRAMES	(12)	//	無敵付与(演出合計より長めに)
#define TP_COOLDOWN_FRAMES		(30)	//	クールダウン
#define TP_FRAMES_MAX			(4)		//	円画像の数

#define ROCK_MAX			    (8)		//	岩の同時最大数
#define PICKUP_MAX			    (12)	//	落下アイテムの最大数
#define PICKUP_FALL_SPEED	    (0)		//	ピックアックの落下速度
#define PICKUP_DRIFT_SPEED		(1)		//	ピックアップの移動速度
#define ROCK_SPEED			    (4)		//	岩の移動速度
#define ROCK_SPAWN_MIN		    (90)	//	スポーン感覚の最小(フレーム)
#define ROCK_SPAWN_MAX		    (120)	//	スポーン感覚の最大(フレーム)

#define EXPLOSION_FRAME_DELAY	(2)		// 何フレームごとに次の絵へ進むか（2なら 30fps相当で8枚= ~0.27秒）

#define TRIGGER_OFF			    (0)		//	トリガーオフ
#define TRIGGER_ON			    (1)		//	トリガーオン
#define GAME_FPS				(60)


// --- 射撃のクールダウン（60FPSで数フレーム） ---
#define FIRE_COOLDOWN_FRAMES_NORMAL  (6)   // 通常弾: 6フレーム ≒ 0.1秒
#define FIRE_COOLDOWN_FRAMES_BEAM    (10)  // ビーム
#define FIRE_COOLDOWN_FRAMES_SPREAD  (12)  // 拡散

#define NORMAL_RELOAD_FRAMES	     (60)		//	リロード所要時間1.0秒(60fps)
#define STAGE1_TARGET_KILL	         (15)	//	ステージ１目標撃破数
#define STAGE2_SURVIVE_SECONDS	     (60)	//	ステージ２耐久秒数(1分)

#define BOSS_MOVE_SPEED_Y            (3)    // バウンド移動の基本速度(px/フレーム)
#define BOSS_OSC_AMP                 (60)   // サイン波の振幅(px)
#define BOSS_OSC_STEP                (0.06f) // サイン波の位相ステップ(ラジアン換算前の増分)
#define BOSS_TRACK_STEP_MAX          (4)   // 追尾時の1フレーム最大追従量
#define BOSS_CLEAR_BONUS			 (500)

#define BGM_PATH_NORMAL	(L"sound\\bgm\\bgm.mp3")
#define BGM_PATH_BOSS	(L"sound\\bgm\\boss.mp3")

//!	シーン遷移用の列挙型変数
enum class Scene
{
	Title,			//	タイトル画面
	StageSelect,	//	ステージセレクト
	Play,			//	ゲーム本編
	Pause,			//	ポーズ画面
	Boss,			//	ボス戦
	GameOver,		//	ゲームオーバー画面
	GameClear
};

//!	当たり判定変数箱
struct Rect
{
	int left;
	int top;
	int right;
	int bottom;
};

//!	プレイする場所の当たり判定
const Rect PLAY_AREA =
{
	0,
	UI_HEIGHT,
	SCREEN_WIDTH,
	SCREEN_HEIGHT
};

//!	岩
struct Rock
{
	int  x, y;
	int  hp;
	bool alive;
};

//!	敵の種類
enum class EnemyType
{
	Normal,		//	横移動のみ
	Vertical	//	上下に動く
};

//!	敵機のデータを保持する構造体
struct Enemy
{
	int  x;			//	敵機の位置
	int  y;
	bool alive;		//	敵機が生きているか判定する変数
	int  shootTimer;	//	射撃タイマー(0になったら撃つ)

	EnemyType type;	//	敵の種類
	int vy;			//	上下移動速度
};

//!	敵の弾データ
struct EnemyBullet
{
	int  x;
	int  y;
	bool active;
};

//!	武器の種類
enum class WeaponType 
{
	Normal,		//	通常弾、		 １
	ChargeBeam,	//	チャージビーム、２	画面端まで
	Spread		//	拡散弾、		 ３	5方向？
};

//!	弾回復アイテム
struct Pickup
{
	int			x, y;
	int			vx,vy;
	bool		active;
	WeaponType  type;
	int			life;
};

//!	プレイヤーの弾(拡散弾用)
struct PlayerBullet
{
	int		   x;
	int		   y;
	float	   vx;		//	x方向の速度
	float	   vy;		//	y方向の速度
	bool	   active;
	WeaponType type;	//	弾の種類判別
};

//!	スコアポップアップ用の構造体(+100を一定時間浮かせて表示)
struct ScorePopupInfo
{
	int  x, y;		//	表示位置
	int  life;		//	残り寿命(フレーム)
	int  value;		//	表示するスコア、「+100」とか
	int  slot;		//	assets側のpopups[]の添え字
	bool alive;		//	生存フラグ
};

//!	ボスの情報
struct Boss 
{
	int  x, y;
	int  w, h;
	int  hp;
	int  maxHP;
	bool alive;
};

//!	ボスの弾の種類
enum class BossBulletType
{
	Spread,
	Aimed
};

//!	ボスの弾
struct BossBullet
{
	int   x, y;			//	描画用整数座標
	float fx, fy;		//	実座標(更新はこれ)
	float vx, vy;
	bool  active;
	BossBulletType type;
};

//!	爆発エフェクト
struct Explosion
{
	int x, y;	//	座標
	int frame;
	int timer;
	bool active;
};

//!	BGM種類分け
enum class BgmKind{
	None,
	Normal,
	Boss};

//	前方宣言
struct Assets;

//!	ゲームの何かしらの状態を保持する構造体
struct GameState
{
	Scene scene;				//	画面の状態
	int	  stageNo;				//	選択中のステージ番号
	int   cursorY;				//	ステージセレクト画面のカーソル位置

	// 入力方向（-1, 0, 1）
	int  inputX;
	int  inputY;
	bool fireRequest;
	bool reloadRequest;
	bool dashRequest;	// ダッシュ要求
	bool helpRequest;
	bool showHelp;

	//	TP用
	bool tpActive;			//	TP中(windup/warp/recoverのどれか)
	int  tpPhase;			//	0=none / 1=windup / 2=warp / 3=recover
	int  tpTimer;			//	位相内の残カウント
	int  tpCDTimer;			//	CD
	int  tpDirX, tpDirY;		//	進行方向
	int  tpStartX, tpStartY;	//	出発点
	int  tpEndX, tpEndY;		//	到着店
	bool teleportRequest;

	BgmKind bgm;				//	現在のBgmを管理

	int  playerX;				//	自機の位置
	int  playerY;
	int  lastPlayerX;			//	自機の前フレーム位置
	int  lastPlayerY;
	int  playerHP;				//	プレイ中HP
	int  playerMaxHP;			//	最大HP
	int  invincibleTimer;		//	無敵時間管理用(0なら無敵じゃないよ～)
	bool isInvincible;			//	無敵状態管理用

	//	新弾システム
	bool		 bulletActive;
	PlayerBullet playerBullets[PLAYER_BULLET_MAX];	//	プレイヤーの弾配列
	WeaponType	 currentWeapon;						//	選択中の武器
	int			 lastWeaponHud;						//	直前に描画した武器種類
	
	int  ammoNormal;			//	通常弾	　  残弾数
	int  ammoBeam;				//	チャージビーム残弾数
	int  ammoSpread;			//	拡散弾	　　残弾数
	bool normalReloading;		//	リロード判定
	int  normalReloadTimer;		//	リロード時間
	int  fireCooldown;

	int  bgX;			//	背景のスクロール位置
	int  bgWidth;
	int  bgHeight;

	Enemy		enemies[ENEMY_MAX];					//	敵の位置と状態(５体)
	EnemyBullet enemyBullets[ENEMY_BULLET_MAX];		//	敵弾の配列
	Rock		rocks[ROCK_MAX];
	Pickup		pickups[PICKUP_MAX];
	int			enemySpawnTimer;					//	敵の次回スポーンまでの残りフレーム
	int			rockSpawnTimer;			            //	次スポーンまでの残りフレーム管理

	int  score;
	//スコアポップアップ用の配列
	static const int POPUP_MAX = 32;
	int              popupCount;
	ScorePopupInfo popups[POPUP_MAX];

	Boss  boss;
	bool  bossActive;		//	ボス戦かどうか判定
	bool  bossIntro;			//	ボス入場アニメ中か
	int   bossTargetX;		//	入場後の停止位置X
	int   bossHpShown;		//	HP表示バー用
	int   bossTimerSpread;
	int   bossTimerAimed;
	int   bossMoveDirY;		//	 +1下  / -1上
	float bossOscPhase;		//	
	BossBullet bossBullets[BOSS_BULLET_MAX];
	static const int EXPLOSION_MAX = 32;
	Explosion explosions[EXPLOSION_MAX];


	int lastScoreHud;			//	直前に描画した合計スコア
	int lastTpCDShown;			//	前回表示した残り秒数
	int lastHPHud;				//	直前に描画したHP

	int playTimeFrames;			//	プレイ経過時間
	int lastTimeHud;			//	直前に描画した時間

	int  stageKillCount;			//	Stage1の撃破数
	bool stageCleared;			//	演出や遷移制御

	int HUD_X;
	int centerX;

	bool requestQuit;
};

//!	画像系のデータを保持する構造体
struct Assets
{
	Bmp* title;				        //　タイトル画面画像
	Bmp* background[2];		        //	ゲーム中背景画像
	Bmp* player;			        //	自機画像
	Bmp* enemy01;			        //	敵機画像１
	Bmp* enemy02;			        //	敵機画像２
	Bmp* boss;				        //	ボス画像
	Bmp* bullet;			        //	自弾画像
	Bmp* chargeBeam;		        //	チャージビーム画像
	Bmp* spreadBullet;		        //	拡散ショット画像
	Bmp* enemyBullet;		        //	敵弾画像
	Bmp* rock;				        //	岩画像
	Bmp* pickupNormal;		        //	通常弾回復
	Bmp* pickupBeam;		        //	ビーム回復
	Bmp* pickupSpread;		        //	拡散弾回復
	Bmp* tpCircle[TP_FRAMES_MAX];	//	0=大、3=小
	int tpCircleCount;				//	ロードできた枚数
	Bmp* stage_select;		        //	「STAGE SELECT」		文字列用
	Bmp* W_stage;			        //	「A : STAGE 1」		文字列用
	Bmp* S_stage;			        //	「D : STAGE 2」		文字列用
	Bmp* F_start;			        //	「F : START」			文字列用
	Bmp* tab_info;
	Bmp* arrow;				        //	「>」				文字列用
	Bmp* explosionFrames[4];		//	爆発アニメのフレーム
	int explosionCount;				//	読み込めた枚数

	Bmp* dash_ready;		//	「Dash : READY」		文字列用
	Bmp* dash_active;		//	「Dash : ACTIVE」		文字列用
	Bmp* dash_n_active;		//	「Dash : COOLDOWN」	文字列用

	Bmp* scoreHud;			//	画面右上
	Bmp* tpCdHud;			//	TP
	Bmp* hpHud;				//	体力表示
	
	Bmp* timeHud;			//	経過時間表示
	Bmp* weaponHud;			//	武器表示
	Bmp* gameOver;			//	「GAME OVER」			 文字列用
	Bmp* retry;				//	「PRESS SPACE TO RETRY」文字列用

	Bmp* popups[GameState::POPUP_MAX];		//	スコアポップアップ		文字列用


	Bmp* ammoIconNormal;
	Bmp* ammoIconBeam;
	Bmp* ammoIconSpread;
	Bmp* ammoIconNormalEmpty;
	Bmp* ammoIconBeamEmpty;
	Bmp* ammoIconSpreadEmpty;

	Bmp* uiWeaponIconNormal;
	Bmp* uiWeaponIconBeam;
	Bmp* uiWeaponIconSpread;
	Bmp* uiWeaponSelectFrame;

	Bmp* labelNormal;	//	Normal文字
	Bmp* labelCharge;	//	Charge文字
	Bmp* labelSpread;	//	Spread文字

	Bmp* heartFull;
	Bmp* heartEmpty;

	Bmp* infoimg;

	Bmp* returnStageSelect;
	Bmp* returnGame;
	Bmp* gameClear;		//	「GAME CLEAR!」文字列用
};

//!	プロトタイプ宣言
void Init  (      GameState&,		Assets&);
void Input (      GameState&, const Assets&);
void Game  (	  GameState&,		Assets&);
void Output(const GameState&, const Assets&);
void Delete(      GameState&,       Assets&);

static Bmp* MakeTextBmp(const TCHAR*, int, int, int);