/*
 @version	New
*/
#include "conioex_New.h"
#if (CONIOEX_VERSION != (3))
#error "conioex_New.cpp/h version mismatch"
#endif
#if (CONIOEX_REVISION != (2))
#error "conioex_New.cpp/h revision mismatch"
#endif
#include <vector>
#define	WRTX_TEXTBMP
#define CONSOLE_INPUT_MODE	(ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT)
//コンソール出力でANSI-256-colorやフルカラーを扱うために'ENABLE_VIRTUAL_TERMINAL_PROCESSING'追加
#define CONSOLE_OUTPUT_MODE	(ENABLE_PROCESSED_OUTPUT | ENABLE_LVB_GRID_WORLDWIDE | ENABLE_VIRTUAL_TERMINAL_PROCESSING)

//----------------------------------------------------------------
// ビットマップファイル操作用
#define	NUM_BMP1_PALETTE	(2)		//1ビットカラー画像のパレットは２色
#define	NUM_BMP4_PALETTE	(16)	//4ビットカラー画像のパレットは１６色
#define	NUM_BMP8_PALETTE	(256)	//8ビットカラー画像のパレットは２５６色

//MCIサウンド用構造体
typedef struct {
	int				device_type;
	MCIDEVICEID		device_id;
	char			path[MAX_PATH];
	int				repeat;
} MciSoundInfo;

static HANDLE	g_OrgOutputHandle = NULL;	//InitConio直前のハンドル
static DWORD	g_OrgOutputHandleMode = 0;
static HANDLE	g_OrgInputHandle;
static DWORD	g_OrgInputHandleMode = 0;
static CONSOLE_CURSOR_INFO	g_OrgCursorInfo = { 0 };	//オリジナルのカーソル情報
#ifdef USED2D
static LONG_PTR	g_OrgWindowStylePtr = 0;
static HANDLE	g_DisplayHandleD2D = NULL;	//ディスプレイハンドル（ダブルバッファ）
static HANDLE	g_InputHandleD2D = NULL;	//入力用ハンドル
#else
static LONG		g_OrgWindowStyle = 0;
static HANDLE	g_DisplayHandle[2] = { NULL,NULL };	//ディスプレイハンドル（ダブルバッファ）
static HANDLE	g_InputHandle = NULL;	//入力用ハンドル
static int		g_SwapFlg = 0;	//ダブルバッファ有効時のディスプレイハンドル入替用フラグ
#endif // USED2D

static DWORD	g_ConioKeyMap[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static COORD	g_ConioMousePosition = { 0, 0 };
static COORD	g_ScreenBufferSize = { 0,0 };
static WORD		g_ScreenBufferAttribute = LIGHTGRAY;

static CONSOLE_SCREEN_BUFFER_INFOEX g_ScreenBufferInfoEx = { sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
static CONSOLE_SCREEN_BUFFER_INFOEX g_OrgScreenBufferInfoEx = { sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
static CONSOLE_FONT_INFOEX g_FontSizeEx = { sizeof(CONSOLE_FONT_INFOEX) };
static CONSOLE_FONT_INFOEX g_OrgFontSizeEx = { sizeof(CONSOLE_FONT_INFOEX) };

static SMALL_RECT g_WindowSize;

//コンソールウィンドウのハンドル保存用変数
static HWND g_hConWnd = NULL;

//キー入力バッファ
static int g_KeyPress[NUM_KEYS] = {};	//プレス
static int g_KeyEdge[NUM_KEYS] = {};	//エッジ
static int g_KeyLast[NUM_KEYS] = {};		//TEMP

#ifndef USED2D
//パレットテーブル
static COLORREF	g_ConsoleColorTable[NUM_PALETTE] = { 0 };	//コンソールのパレット
static COLORREF	g_OrgColorTable[NUM_PALETTE] = { 0 };	//Conioexが起動したときのパレット

//スクリーンバッファ
//static CHAR_INFO* g_ScreenBuffer4bit = NULL;	//スクリーンバッファ
static WORD* g_ScreenBuffer4bit = NULL;	//スクリーンバッファ
//フレームバッファ
char* g_FrameBuffer4bit = NULL;	//１６色画像用フレームバッファ
#endif // !USED2D
//スクリーンバッファ(16色)の生成
static HANDLE create_screen_buffer(CONSOLE_SCREEN_BUFFER_INFOEX* pCsbix, CONSOLE_FONT_INFOEX* pCfix);
#ifndef USED2D
//RGB値（数字３文字）"０００"～"２５５"のテーブル(２４bitカラー画像用)
static char CharRGBconvTBL[3][256] = {
	{'0','0','0'},
};

//２４ビットフルカラー画像用
#define	PIXEL24ESCSIZE	(sizeof(pixel24bitEsc))	//フルカラー描画時の２文字当たりに必要なサイズ（色指定用エスケープシーケンスのバイト数）
static const char pixel24bitEsc[] = "\033[48;2;000;000;000m ";
char* g_FrameBufferFull = NULL;	//２４bitカラー画像用
static char* g_ScreenBufferFull = NULL;	//２４bitカラー画像用
static int g_ScreenBufferLineStride = 0;	//２４ビット画像用バッファの１行分のバイト数。
static void init_24bit_color_image(void);	//２４ビットフルカラー画像用の初期化
//２５６色パレット画像用
static const char pixel256Esc[] = "\x1b[48;5;000m ";	//２５６パレット番号指定用エスケープシーケンス：ESC[48;5;<パレット番号：10進数3ケタ>m
static const char palette256Esc[] = "\x1b]4;000;rgb:00/00/00\x1b\\";	//２５６パレット用RGB設定エスケープシーケンス：ESC[4;<パレット番号：10進数3ケタ;rgb:<RR>/<GG>/<BB>ESC\> ←RGB値は各16進2ケタ
static BYTE* g_FrameBuffer256 = NULL;	//８ビット/ピクセルのバッファ
static BYTE* g_ScreenBuffer256 = NULL;	//２５６パレット番号指定エスケープシーケンスが並んだバッファ
static void init_256color_image(void);	//２５６色パレット画像用の初期化
static void set_palette256(HANDLE _hCon, const COLORREF* _p256, int _num_pal);	//２５６色パレット設定
#endif // !USED2D


static RECT g_ConWinSize = {};	//コンソールのクライアント領域のサイズと、フォントサイズ。{left=w,top=h,right=fw,bottom=fh}

#ifdef USED2D
//================================================================
//	Direct2D/DirectWrite
//================================================================
ID2D1HwndRenderTarget* g_pRenderTarget = NULL;	//ウィンドウに描画する為のレンダーターゲット
ID2D1Factory* g_pD2DFactory = NULL;		//D2Dファクトリー
IDWriteFactory* g_pDWFactory = NULL;	//テキスト出力用DirectWriteファクトリー

std::vector<ID2D1Bitmap*>	g_pBmpList;		//画像用
#ifdef	WRTX_TEXTBMP
struct TextBmp {
	D2D1_RECT_F trf;
	ID2D1Bitmap* pBmp;
};
std::vector<TextBmp*>	g_pTextBmpList;	//テキスト用
#else	//WRTX_TEXTBMP
std::vector<ID2D1Bitmap*>	g_pTextBmpList;	//テキスト用
#endif	//WRTX_TEXTBMP
//char* g_FrameBuffer4bitD2D = NULL;		//１６色(4bit)画像用フレームバッファ
RGBQUAD* g_FrameBuffer32bitD2D = NULL;	//フルカラー用フレームバッファ
//RGBQUAD* g_pMask = NULL;
COORD	g_CursorPosD2D = { 0,0 };		//文字表示開始位置（カーソル位置）

//パレットテーブル
//static COLORREF	g_ConsoleColorTableD2D[NUM_D2D_PAL] = { 0 };	//コンソールのパレット
static COLORREF	g_OrgColorTableD2D[NUM_D2D_PAL] = { 0 };	//Conioexが起動したときのパレット
RGBQUAD	g_PaletteD2D[NUM_ANSI_PAL] = {};	//256色パレット：COLORREF:0x00BBGGRR->{R8,G8,B8,X}/RGBQUAD:{B8,G8,R8,A8}->0xAARRGGBB

bool g_PrintStringCompatibleMode = false;	//true=ConsoleAPIの時の１：２の文字サイズ

//prototype
bool InitD2D(int _w, int _h);
void EndD2D(void);
//================================================================
//	D2D初期化
//================================================================
bool InitD2D(int _window_width, int _window_height)
{
	HRESULT hr;	//
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
	_ASSERT(hr == S_OK);
#if false
	RECT cr;
	GetClientRect(g_hConWnd, &cr);	//クライアント領域の{0,0,幅,高さ}取得
	_window_width = cr.right;
	_window_height = cr.bottom;
	hr = g_pD2DFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(g_hConWnd, D2D1_SIZE_U{ (UINT32)_window_width, (UINT32)_window_height }), &g_pRenderTarget);
	_ASSERT(hr == S_OK);
#else
	/*
	* レンダーターゲットの種類
	* ID2D1BitmapRenderTarget ------> CreateCompatibleRenderTarget メソッドによって作成された中間テクスチャにレンダリングします。
	* ID2D1DCRenderTarget ----------> GDI デバイス コンテキストに対して描画コマンドを発行します。
	* ID2D1GdiInteropRenderTarget --> GDI 描画コマンドを受け入れることができるデバイスコンテキストへのアクセスを提供します。
	* ID2D1HwndRenderTarget --------> 描画命令をウィンドウにレンダリングします。
	*/
	//ウィンドウへの描画なので ID2D1HwndRenderTarget を使用する。
	//サイズはウィンドウと同じサイズにすること。サイズが違うと、ウィンドウに合わせて拡大・縮小がかかる。
	/////D2D1_SIZE_U renderTargetSize = { (UINT32)g_WindowSize.Right, (UINT32)g_WindowSize.Bottom };
	D2D1_SIZE_U renderTargetSize = { _window_width, _window_height };
	D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties;
	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTargetProperties;
	D2D1_PIXEL_FORMAT pixelFormat;
	pixelFormat.format = DXGI_FORMAT_UNKNOWN;	//既定の形式
	pixelFormat.alphaMode = D2D1_ALPHA_MODE_UNKNOWN;
	//pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;

	renderTargetProperties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;	//ハードウェアによる描画とソフトウェアによる描画のどちらを利用するか
	renderTargetProperties.pixelFormat = pixelFormat;				//ピクセル形式とアルファモード
	renderTargetProperties.dpiX = 0;								//それぞれ水平方向と垂直方向の DPI （Donts per Inch、ピクセル密度）を指定します。
	renderTargetProperties.dpiY = 0;								//既定の DPI を使用するには 0 を指定します。
	renderTargetProperties.usage = D2D1_RENDER_TARGET_USAGE_NONE;	//レンダーターゲットのリモート処理と GDI との互換性
	renderTargetProperties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;	//ハードウェアによる描画に必要な Direct3D の最小限の機能レベル

	RECT cr;
	GetClientRect(g_hConWnd, &cr);	//クライアント領域の{0,0,幅,高さ}取得
	renderTargetSize.width = cr.right;
	renderTargetSize.height = cr.bottom;
	hwndRenderTargetProperties.hwnd = g_hConWnd;	//ターゲットとなるウィンドウのハンドル
	hwndRenderTargetProperties.pixelSize = renderTargetSize;	//ウィンドウのクライアント領域のサイズ
	hwndRenderTargetProperties.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;	//非同期（フレーム時間まで待たない）
	//hwndRenderTargetProperties.presentOptions = D2D1_PRESENT_OPTIONS_NONE;	//フレーム同期
	//レンダーターゲット取得
	hr = g_pD2DFactory->CreateHwndRenderTarget(renderTargetProperties, hwndRenderTargetProperties, &g_pRenderTarget);
#endif // false

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWFactory));
	_ASSERT(hr == S_OK);
	//g_FrameBuffer4bitD2D = (char*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(char));
	g_FrameBuffer32bitD2D = (RGBQUAD*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(RGBQUAD));
	//g_pMask = (RGBQUAD*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(RGBQUAD));
	//デフォルト２５６パレットをANSI256色で初期化
	//memcpy_s(g_PaletteD2D, sizeof(g_PaletteD2D), ANSI_PAL256_RGB, sizeof(ANSI_PAL256_RGB));
	//最初の１６色は起動時のパレットを取り込んでいるので、１７色以降の色を取り込む。
	for (int n = 16; n < NUM_ANSI_PAL; n++) {
		g_PaletteD2D[n] = ANSI_PAL256_RGB[n];
	}
	return true;
}	//InitD2D
//==================================================================
// D2D終了
//==================================================================
void EndD2D(void)
{
	//free(g_pMask);
	if (g_FrameBuffer32bitD2D != NULL) {
		free(g_FrameBuffer32bitD2D);
		g_FrameBuffer32bitD2D = NULL;
	}

	//if (g_FrameBuffer4bitD2D != NULL) {
	//	free(g_FrameBuffer4bitD2D);
	//	g_FrameBuffer4bitD2D = NULL;
	//}

	for (ID2D1Bitmap* pbmp : g_pBmpList) {
		pbmp->Release();
	}
	g_pBmpList.clear();

#ifdef	WRTX_TEXTBMP
	for (TextBmp* pbmp : g_pTextBmpList) {
		pbmp->pBmp->Release();
		delete pbmp;
		pbmp = nullptr;
	}
#else	//WRTX_TEXTBMP
	for (ID2D1Bitmap* pbmp : g_pTextBmpList) {
		pbmp->Release();
	}
#endif	//WRTX_TEXTBMP
	g_pTextBmpList.clear();

	// IDWriteFactoryの破棄
	if (NULL != g_pDWFactory) {
		g_pDWFactory->Release();
		g_pDWFactory = NULL;
	}

	// ID2D1HwndRenderTargetの破棄
	if (NULL != g_pRenderTarget) {
		g_pRenderTarget->Release();
		g_pRenderTarget = NULL;
	}

	// ID2D1Factoryの破棄
	if (NULL != g_pD2DFactory) {
		g_pD2DFactory->Release();
		g_pD2DFactory = NULL;
	}
}	//EndD2D

//================================================================
/*
* 現在の32ビットフレームバッファ(g_FrameBuffer32bitD2D)をビットマップ(ID2D1Bitmap)に転送して、ビットマップリストに g_BmpList 追加する。
* DXGI_FORMAT_B8G8R8A8_UNORM
* DXGI_FORMAT_R8G8B8A8_UNORM
* DXGI_FORMAT_A8_UNORM
*/
static void push_screen_buffer(void)
{
	HRESULT hr = ~S_OK;
	D2D1_SIZE_U siz = { g_ScreenBufferSize.X, g_ScreenBufferSize.Y };
	ID2D1Bitmap* pD2D1_Bmp;
	hr = g_pRenderTarget->CreateBitmap(
		siz,
		//D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),	//不透明
		//D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),	//不透明
		//D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT)),	//不透明
		//D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_STRAIGHT)),	//不透明
		D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),	//不透明
		//D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_IGNORE)),	//NG
		&pD2D1_Bmp
	);
	_ASSERT(hr == S_OK);
	if (pD2D1_Bmp == NULL) {
		return;	//ビットマップ作成失敗
	}
	D2D1_RECT_U	ru = { 0,0,siz.width,siz.height };
	hr = pD2D1_Bmp->CopyFromMemory(&ru, reinterpret_cast<const void*>(g_FrameBuffer32bitD2D), g_ScreenBufferSize.X * 4);
	_ASSERT(hr == S_OK);
	//■■■■ 描画したビットマップをリストに追加 ■■■
	g_pBmpList.push_back(pD2D1_Bmp);
	//★★★★ pD2D1_Bmp の Release 不要！★★★★　＝＞　描画時に Release している ★★★★
	return;
}	//push_screen_buffer

//================================================================
//	インデックスカラー画像を現在設定されている２５６色パレットで32ビットバッファに描画する
/*
* ★★★
*	描画時に１ピクセル単位で全て３２ビットバッファに描きこんでいるのでこの処理は不要。
*	というか、今のところピクセル単位で処理しても重くは無いので、
* 	４ビットバッファが不要かも知れない・・・
*	それに、PrintImag()と併用すると、32ビットバッファを上書きしてしまうのでもややこしくなるし・・・
*	まあ、4ビット画像からの変換用に専用の32ビットバッファを持つといいけど、それではパレットから解放されないので、それもややこしい・・・
*	しかし、パレットインデックス画像の仕組みを勉強する為には良いのかもしれないので検討の余地は大きいか・・・
* ★★★
*	int src_pix_s = (g_ScreenBufferSize.X * g_ScreenBufferSize.Y * sizeof(char));
*	RGBQUAD* tmp = g_FrameBuffer32bitD2D;
*	for (int n = 0; n < src_pix_s; n++) {
*		//8ビットのパレットインデックスバッファでパレットバッファ参照して32ビットRGBQUADバッファへ書き込み
*		tmp[n] = g_PaletteD2D[_buf_8bit[n] % NUM_ANSI_PAL];
*	}
* ★★★
*/
/*
* フレームバッファをレンダー用リストに追加する
*/
void PrintFrameBuffer(void)
{
	if (g_pRenderTarget == NULL) {
		return;
	}
	push_screen_buffer();
}	//PrintFrameBuffer

//================================================================
//	文字列をID2D1Bitmapに描画する
void WriteTextA(int _xp, int _yp, const char* _text, double _scale, D2D1::ColorF _fgc, D2D1::ColorF _bgc, bool _new_line)
{
	//---- マルチバイト文字列をUnicode文字列に変換する
	int wc_count = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _text, -1, NULL, 0);	//'\0'を含む文字数が返る
	size_t wc_src_bytes = (wc_count * sizeof(wchar_t));
	wchar_t* src_txt = (wchar_t*)_malloca(wc_src_bytes);	//スタック上に確保（free不要）
	memset(src_txt, 0, wc_src_bytes);
	int disp_siz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _text, -1, src_txt, wc_count);
	WriteTextW(_xp, _yp, src_txt, _scale, _fgc, _bgc, _new_line);
}
void WriteTextW(int _xp, int _yp, const wchar_t* _text, double _scale, D2D1::ColorF _fgc, D2D1::ColorF _bgc, bool _new_line)
{
#ifdef WRTX_TEXTBMP
	size_t tx_len = wcslen(_text);	// 文字列の長さ
	if (tx_len == 0) {
		return;	//空文字列は無視
	}
#endif // WRTX_TEXTBMP
	if (_scale < 1.0) {
		_scale = 1.0;
	}
	//Windows関数の戻り値
	HRESULT hr;
	double f_w = (double)(g_FontSizeEx.dwFontSize.X);	//フォント（１ドット）の幅
	double f_h = (double)(g_FontSizeEx.dwFontSize.Y);	//フォント（１ドット）の高さ

	//テキストフォーマットの生成
	//CreateTextFormat( L"フォント名", コレクション, 太さ, スタイル, 拡縮, サイズ, ローカルネーム？, 受け取るポインタ );
	IDWriteTextFormat* pTextFormat = NULL;
	g_pDWFactory->CreateTextFormat(L"ＭＳ 明朝", NULL,
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, f_h * _scale, L"", &pTextFormat);

	// ブラシの作成
	ID2D1SolidColorBrush* pBrushFG = NULL;
	g_pRenderTarget->CreateSolidColorBrush(_fgc, &pBrushFG);	//フォントの色
	ID2D1SolidColorBrush* pBrushBG = NULL;
	g_pRenderTarget->CreateSolidColorBrush(_bgc, &pBrushBG);	//背景の色

	D2D1_RECT_F trf;
	{
		//レンダーターゲットのサイズ取得
		D2D1_SIZE_F szf = g_pRenderTarget->GetSize();
		//テキストのレイアウト（位置、幅、高さ）の生成
		IDWriteTextLayout* pTextLayout = NULL;
		// IDWriteTextLayout 取得
		hr = g_pDWFactory->CreateTextLayout(
			_text					// 文字列
			, (UINT32)wcslen(_text)	// 文字列の長さ
			, pTextFormat           // DWriteTextFormat
			, szf.width     // 枠の幅
			, szf.height    // 枠の高さ
			, &pTextLayout
		);
		_ASSERT(hr == S_OK);
		DWRITE_TEXT_METRICS mtx;	//テキストを囲む矩形の計測値
		/*
		* DWRITE_TEXT_METRICS
		* FLOAT left;			レイアウト ボックスを基準とした書式設定されたテキストの左端のポイント (グリフのオーバーハングを除く)。
		* FLOAT top;			レイアウト ボックスに対する書式設定されたテキストの最上点 (グリフのオーバーハングを除く)。
		* FLOAT width;			各行末の末尾の空白を無視した、書式設定されたテキストの幅。
		* "FLOAT widthIncludeTrailingWhitespace;"	各行末の末尾の空白を考慮した、書式設定されたテキストの幅。
		* FLOAT height;			書式設定されたテキストの高さ。	空の文字列の高さは、既定のフォントの行の高さのサイズによって決まります。
		* FLOAT layoutWidth;	レイアウトに与えられる初期幅。	テキストが折り返されたかどうかに応じて、テキスト コンテンツの幅よりも大きくなったり小さくなったりします。
		* FLOAT layoutHeight;	レイアウトに与えられる初期の高さ。文字の長さによっては、文字コンテンツの高さよりも大きくなったり小さくなったりします。
		* UINT32 maxBidiReorderingDepth;	必要なヒット テスト ボックスの最大数を計算するために使用される、任意のテキスト行の最大並べ替え数。レイアウトに双方向テキストがない場合、またはテキストがまったくない場合、最小レベルは 1 です。
		* UINT32 lineCount;		行の総数。
		*/
		// 計測
		hr = pTextLayout->GetMetrics(&mtx);
		_ASSERT(hr == S_OK);
		//_RPTN(_CRT_WARN, "%f,%f,%f,%f,\n%f,%f,\n%f,\n%d,\n%d\n",mtx.left, mtx.top, mtx.width, mtx.height,mtx.layoutWidth, mtx.layoutHeight,mtx.widthIncludingTrailingWhitespace,mtx.maxBidiReorderingDepth,mtx.lineCount);
		float left = (mtx.left + _xp) * f_w;
		float top = (mtx.top + _yp) * f_h;
		float right = left + mtx.width;
		float bottom = top + mtx.height;
		if (!_new_line) {
			//画面端で改行させない場合
			//right = left + (mtx.width * mtx.lineCount);	//行数分ヨコに拡張
			//bottom = top + (mtx.height / mtx.lineCount);	//１行分に縮小
			//再計算
			hr = g_pDWFactory->CreateTextLayout(
				_text					// 文字列
				, (UINT32)wcslen(_text)	// 文字列の長さ
				, pTextFormat           // DWriteTextFormat
				, (mtx.width * mtx.lineCount)     // 枠の幅
				, (mtx.height / mtx.lineCount)    // 枠の高さ
				, &pTextLayout
			);
			_ASSERT(hr == S_OK);
			hr = pTextLayout->GetMetrics(&mtx);
			_ASSERT(hr == S_OK);
			left = (mtx.left + _xp) * f_w;
			top = (mtx.top + _yp) * f_h;
			right = left + mtx.width;
			bottom = top + mtx.height;
		}
		//文字列を囲む矩形を作成
		if (g_PrintStringCompatibleMode) {
			trf = D2D1::RectF(left / ((f_w * 2) / f_h), top, right, bottom);	//互換モード：文字のピクセルの比率は１：２
		}
		else {
			trf = D2D1::RectF(left, top, right, bottom);
		}
		// IDWriteTextLayoutの破棄
		pTextLayout->Release();
	}

	//互換ビットマップのレンダーターゲットを作成
	ID2D1BitmapRenderTarget* p_bitmap_render_target = NULL;
#ifdef	WRTX_TEXTBMP
	D2D1_SIZE_F oTargetSize = D2D1::SizeF(trf.right - trf.left, trf.bottom - trf.top);
	hr = g_pRenderTarget->CreateCompatibleRenderTarget(oTargetSize, &p_bitmap_render_target);
#else	//WRTX_TEXTBMP
	hr = g_pRenderTarget->CreateCompatibleRenderTarget(&p_bitmap_render_target);
#endif	//WRTX_TEXTBMP
	_ASSERT(hr == S_OK);
	//▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
	//テキストレイヤー描画
	p_bitmap_render_target->BeginDraw();
#ifdef	WRTX_TEXTBMP
	D2D1_RECT_F draw_rect = D2D1::RectF(0, 0, oTargetSize.width, oTargetSize.height);
	// 背景の四角形の描画
	p_bitmap_render_target->FillRectangle(&draw_rect, pBrushBG);
	//p_bitmap_render_target->Clear();
	p_bitmap_render_target->DrawText(
		_text   // 文字列
		, (UINT32)wcslen(_text)    // 文字数
		, pTextFormat
		, &draw_rect
		, pBrushFG
		, D2D1_DRAW_TEXT_OPTIONS_NONE
		//, D2D1_DRAW_TEXT_OPTIONS_CLIP
	);
#else	//WRTX_TEXTBMP
	//p_bitmap_render_target->Clear();
	p_bitmap_render_target->DrawText(
		_text   // 文字列
		, (UINT32)wcslen(_text)    // 文字数
		, pTextFormat
		, &trf//&D2D1::RectF(0, 0, oTargetSize.width, oTargetSize.height)
		, pBrushFG
		, D2D1_DRAW_TEXT_OPTIONS_NONE
		//, D2D1_DRAW_TEXT_OPTIONS_CLIP
	);
#endif	//WRTX_TEXTBMP
	//p_bitmap_render_target->DrawRectangle(&trf, pBrushFG, 1.0f);	// デバッグ用枠(四角形)の描画
	//
	hr = p_bitmap_render_target->EndDraw();
	//▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
	_ASSERT(hr == S_OK);
	//■■■■ 描画したビットマップをリストに追加 ■■■
	ID2D1Bitmap* pD2D1_Bmp;
	p_bitmap_render_target->GetBitmap(&pD2D1_Bmp);	//描画したビットマップを取得
#ifdef	WRTX_TEXTBMP
	D2D_SIZE_F oBmpSize = pD2D1_Bmp->GetSize();
	TextBmp* ptb = new TextBmp();
	ptb->pBmp = pD2D1_Bmp;
	ptb->trf = trf;
	g_pTextBmpList.push_back(ptb);	//ビットマップをテキストレイヤーに追加
#else	//WRTX_TEXTBMP
	g_pTextBmpList.push_back(pD2D1_Bmp);	//ビットマップをテキストレイヤーに追加
#endif	//WRTX_TEXTBMP
	p_bitmap_render_target->Release();

	// テキストフォーマットの破棄
	if (pTextFormat != NULL) {
		pTextFormat->Release();
	}
	// ブラシの破棄
	if (pBrushBG != NULL) {
		pBrushBG->Release();
	}
	if (pBrushFG != NULL) {
		pBrushFG->Release();
	}

	return;
}	//WriteTextW

//================================================================
// ダブルバッファ関連
//================================================================

/**
 * @brief	ダブルバッファ初期化
 *
 * @return	バッファハンドル取得失敗
 */
int InitDoubleBuffer(void)
{
	return 0;
}

/**
 * @brief	ダブルバッファ時の描画面切替
 */
void FlipScreen(void)
{
	//Windows関数の戻り値
	HRESULT hr;
	/*
	* レンダーターゲットへの描画
	* ビットマップの準備が出来たらレンダーターゲットに渡して描画を行う。
	* デフォルトの補間モードは D2D1_BITMAP_INTERPOLATION_MODE_LINEAR になっているので、
	* 何も指定しないと拡大縮小時にぼやけてしまう。
	*/
	//left,top,right,bottom --- x,y,x,y
	const FLOAT	scale_x = g_FontSizeEx.dwFontSize.X;	//描画倍率
	const FLOAT	scale_y = g_FontSizeEx.dwFontSize.Y;	//描画倍率
	const FLOAT	opacity = 1.0f;	//透明度：不透明(1.0f)～(0.0f)透明
	//描画する矩形を作成
	//D2D1_RECT_F rf = { 0,0,((FLOAT)g_ScreenBufferSize.X - 1) * scale_x,((FLOAT)g_ScreenBufferSize.Y - 1) * scale_y };
	D2D1_RECT_F rf = { 0,0,(FLOAT)g_ScreenBufferSize.X * scale_x,(FLOAT)g_ScreenBufferSize.Y * scale_y };
	//D2D1_RECT_F drf = { 0,0,(FLOAT)g_ScreenBufferSize.X - 1,(FLOAT)g_ScreenBufferSize.Y - 1 };
	//D2D1_RECT_F srf = { 0,0,(FLOAT)g_ScreenBufferSize.X - 1,(FLOAT)g_ScreenBufferSize.Y - 1 };
	//▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
	g_pRenderTarget->BeginDraw();	//描画開始

	g_pRenderTarget->Clear();		//画面消去
	//DrawBitmap(bitmap,描画される領域のサイズと位置,不透明度,補間モード)
	for (ID2D1Bitmap* pbmp : g_pBmpList) {
		//リストに溜まっているビットマップオブジェクトを全て表示する。
		//ドット拡大してもぼやけない様に"_NEAREST_NEIGHBOR"を指定している。
		g_pRenderTarget->DrawBitmap(pbmp, &rf, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);	//補完無し
		pbmp->Release();
	}
	g_pBmpList.clear();	//リストを消去

	if (g_PrintStringCompatibleMode) {
		FLOAT bairitu = (scale_x * 2) / scale_y;	//互換モード：文字のピクセルは１ｘ２＝＞１ｘ１のドットにするとヨコに拡大される
		rf = { 0,0,(FLOAT)g_ScreenBufferSize.X * scale_x * bairitu,(FLOAT)g_ScreenBufferSize.Y * scale_y };
	}
	//else{
	//	FLOAT bairitu = (scale_x*1) / scale_y;	//ノーマルモード：文字のピクセルは１ｘ１＝＞１ｘ２のドットにするとヨコが半分になる
	//	rf = { 0,0,(FLOAT)g_ScreenBufferSize.X * scale_x * bairitu,(FLOAT)g_ScreenBufferSize.Y * scale_y };
	//}
#ifdef	WRTX_TEXTBMP
	for (TextBmp* ptb : g_pTextBmpList) {
		//リストに溜まっているテキスト用ビットマップオブジェクトを全て表示する。
		//ドット拡大してもぼやけない様に"_NEAREST_NEIGHBOR"を指定している。
		g_pRenderTarget->DrawBitmap(ptb->pBmp, &ptb->trf, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);	//補完無し
		ptb->pBmp->Release();
	}
	g_pTextBmpList.clear();	//リストを消去
#else	//WRTX_TEXTBMP
	for (ID2D1Bitmap* pbmp : g_pTextBmpList) {
		//リストに溜まっているテキスト用ビットマップオブジェクトを全て表示する。
		//ドット拡大してもぼやけない様に"_NEAREST_NEIGHBOR"を指定している。
		g_pRenderTarget->DrawBitmap(pbmp, &rf, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);	//補完無し
		pbmp->Release();
	}
	g_pTextBmpList.clear();	//リストを消去
#endif	//WRTX_TEXTBMP

	hr = g_pRenderTarget->EndDraw();	//描画終了
	//▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
	_ASSERT(hr == S_OK);
	g_PrintStringCompatibleMode = false;
	return;
}	//FlipScreen

/**
 * @brief	フォントサイズ変更
 *
 * @param	width [入力] フォントの横サイズ(1～)
 * @param	height [入力] フォントの縦サイズ(1～)
 */
void SetScreenFontSize(int _width, int _height)
{
	//g_FontSizeEx.dwFontSize.X = _width;
	//g_FontSizeEx.dwFontSize.Y = _height;
	//g_FontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	////g_FontSizeEx.FaceName = L"";
	////g_FontSizeEx.FontFamily = 
	//g_FontSizeEx.FontWeight;
	//g_FontSizeEx.nFont;

	// フォントサイズ変更
	g_FontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	g_FontSizeEx.dwFontSize.X = _width;
	g_FontSizeEx.dwFontSize.Y = _height;
	if (g_DisplayHandleD2D != NULL) {
		SetCurrentConsoleFontEx(g_DisplayHandleD2D, FALSE, &g_FontSizeEx);
	}

}	//SetScreenFontSize

//================================================================
// フレームバッファ画像描画
//================================================================
/**
 * @brief	画面（スクリーンバッファ）消去
 */
void ClearScreen(int _cc)
{
	//画像用バッファ消去
	//memset(g_FrameBuffer32bitD2D, *((DWORD*)(&g_PaletteD2D[_cc % NUM_D2D_PAL])), sizeof(RGBQUAD) * g_ScreenBufferSize.X * g_ScreenBufferSize.Y);
	for (int i = 0; i < NUM_D2D_PAL; i++) {
		g_FrameBuffer32bitD2D[i] = g_PaletteD2D[_cc % NUM_D2D_PAL];
	}
}	//ClearScreen
void ClearScreen(int _red, int _green, int _blue)
{
	RGBQUAD rgb{ _blue,_green,_red,0 };
	//rgb.rgbBlue = _blue;
	//rgb.rgbGreen = _green;
	//rgb.rgbRed = _red;
	//rgb.rgbReserved = _alpha;

	//画像用バッファ消去
	//memset(g_FrameBuffer32bitD2D, ((DWORD*)&_rgb)[0], sizeof(RGBQUAD) * g_ScreenBufferSize.X * g_ScreenBufferSize.Y);
	for (int i = 0; i < (g_ScreenBufferSize.X * g_ScreenBufferSize.Y); i++) {
		g_FrameBuffer32bitD2D[i] = rgb;
	}
}	//ClearScreen
void ClearScreen(void)
{
	//画像用バッファ消去
	ZeroMemory(g_FrameBuffer32bitD2D, sizeof(RGBQUAD) * g_ScreenBufferSize.X * g_ScreenBufferSize.Y);
}	//ClearScreen

/**
* @brief	点を打つ
*
* @param	int _x,_y：座標
* @param	RGBQUAD _rgb：色（RGBQUAD:0x00RRGGBB）
*/
void DrawPixel(int _x, int _y, RGBQUAD _rgb)
{
	if ((_x >= 0) && (_x < g_ScreenBufferSize.X) && (_y >= 0) && (_y < g_ScreenBufferSize.Y)) {
		g_FrameBuffer32bitD2D[_y * g_ScreenBufferSize.X + _x] = _rgb;
	}
}	//DrawPixel
/**
* @brief	点を打つ
*
* @param	int _x,_y：座標
* @param	int _c：色（パレット番号０～１５）
*/
void DrawPixel(int _x, int _y, unsigned char _c)
{
	DrawPixel(_x, _y, g_PaletteD2D[_c & 0xFF]);
}

#endif	//USED2D

//################################################################################################################################
//################################################################################################################################
//################################################################################################################################
//================================================================
// 初期化
//================================================================
/**
 * @brief	コンソール I/O 初期化
 *
 * @param	_width [入力] コンソールウィンドウの横サイズ(1～)
 * @param	_height [入力] コンソールウィンドウの縦サイズ(1～)
 */
void InitConio(int _width, int _height) {
	InitConioEx(_width, _height, DEF_FONTSIZE_X, DEF_FONTSIZE_Y, NULL, NULL);
}
/**
 * @brief	コンソール I/O 初期化（拡張版）
 *
 * @param	int _width ：コンソールウィンドウの横サイズ(1～)
 * @param	int _height：コンソールウィンドウの縦サイズ(1～)
 * @param	int _font_w：フォントの横サイズ(1～)
 * @param	int _font_h：フォントの縦サイズ(1～)
 * @param	bool _init_wbuf：ダブルバッファの初期化(true=する/false=しない)
 * @param	const wchar_t* _font_face_name：設定するフォントの名前(Unicode文字列)
 * @param	const COLORREF* _pal16：設定する16色パレット
 *
 * @return	無し
 */
void InitConioEx(int _width, int _height, int _font_w, int _font_h)
{
	InitConioEx(_width, _height, _font_w, _font_h, NULL, NULL, false);
}
void InitConioEx(int _width, int _height, int _font_w, int _font_h, bool _init_wbuf)
{
	InitConioEx(_width, _height, _font_w, _font_h, NULL, NULL, _init_wbuf);
}
void InitConioEx(int _width, int _height, int _font_w, int _font_h, const wchar_t* _font_face_name, const COLORREF* _pal16, bool _init_wbuf)
{
	const char* str_locale = setlocale(LC_CTYPE, "Japanese_Japan");

	//キーバッファクリア
	memset(g_KeyPress, 0, NUM_KEYS);
	memset(g_KeyEdge, 0, NUM_KEYS);
	memset(g_KeyLast, 0, NUM_KEYS);

	//----------------------------------------------------------------
	//コンソールウィンドウのウィンドウハンドル(HWND)取得＆保存
	g_hConWnd = GetConsoleWindow();
#ifdef USED2D
	//----------------------------------------------------------------
	// コマンド履歴を保存しない
	CONSOLE_HISTORY_INFO history_info;
	history_info.cbSize = sizeof(CONSOLE_HISTORY_INFO);
	history_info.HistoryBufferSize = 0;
	history_info.NumberOfHistoryBuffers = 0;
	history_info.dwFlags = 0;
	SetConsoleHistoryInfo(&history_info);

	//----------------------------------------------------------------
	// 直前のディスプレイ情報取得
	g_OrgOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);	//出力ハンドル
	GetConsoleMode(g_OrgOutputHandle, &g_OrgOutputHandleMode);	//出力コンソール情報
	g_OrgInputHandle = GetStdHandle(STD_INPUT_HANDLE);	//入力ハンドル
	GetConsoleMode(g_OrgInputHandle, &g_OrgInputHandleMode);	//入力コンソール情報
	g_InputHandleD2D = g_OrgInputHandle;	//入力は起動時と同じハンドル
	//----------------------------------------------------------------
	//画面情報を保存しておく（16色パレット含む）
	g_OrgScreenBufferInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	GetConsoleScreenBufferInfoEx(g_OrgOutputHandle, &g_OrgScreenBufferInfoEx);
	// 現在のカラーパレットを保存
	for (int n = 0; n < NUM_PALETTE; n++) {
		g_OrgColorTableD2D[n] = g_OrgScreenBufferInfoEx.ColorTable[n];
		//デフォルトパレットの最初の１６色の位置に取り込む
		g_PaletteD2D[n].rgbBlue = (g_OrgColorTableD2D[n] & 0x00FF0000) >> 16;
		g_PaletteD2D[n].rgbGreen = (g_OrgColorTableD2D[n] & 0x0000FF00) >> 8;
		g_PaletteD2D[n].rgbRed = (g_OrgColorTableD2D[n] & 0x000000FF);
		g_PaletteD2D[n].rgbReserved = (g_OrgColorTableD2D[n] & 0x0FF00000) >> 24;
	}
	//----------------------------------------------------------------
	//フォントサイズ保存：オリジナル保存
	g_OrgFontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	GetCurrentConsoleFontEx(g_OrgOutputHandle, FALSE, &g_OrgFontSizeEx);	//第2パラメータがTRUEだと画面バッファと同じサイズが返るみたいだ・・・
	//----------------------------------------------------------------
	//現在のカーソル状態保存
	GetConsoleCursorInfo(g_OrgOutputHandle, &g_OrgCursorInfo);
	//カーソル表示OFF
	CONSOLE_CURSOR_INFO cci = { sizeof(CONSOLE_CURSOR_INFO) };
	cci.bVisible = FALSE;
	SetConsoleCursorInfo(g_OrgOutputHandle, &cci);
	//----------------------------------------------------------------
	//ウィンドウの状態保存
	g_OrgWindowStylePtr = GetWindowLongPtr(g_hConWnd, GWL_STYLE);

	//----------------------------------------------------------------
	// GetSystemMetrics
	// https://learn.microsoft.com/ja-JP/windows/win32/api/winuser/nf-winuser-getsystemmetrics
	//int cx_border = GetSystemMetrics(SM_CXBORDER);
	//int cy_border = GetSystemMetrics(SM_CYBORDER);	//ウィンドウの境界線の高さ (ピクセル単位)。 
	//int cx_size = GetSystemMetrics(SM_CXSIZE);				//ウィンドウ キャプションまたはタイトル バーのボタンの幅(ピクセル単位)。
	//int cy_size = GetSystemMetrics(SM_CYSIZE);				//ウィンドウ キャプションまたはタイトル バーのボタンの高さ(ピクセル単位)。
	//int cx_size_frame = GetSystemMetrics(SM_CXSIZEFRAME);	//サイズを変更できるウィンドウの周囲のサイズ変更境界線の太さ (ピクセル単位)。 
	//int cy_size_frame = GetSystemMetrics(SM_CYSIZEFRAME);	//サイズを変更できるウィンドウの周囲のサイズ変更境界線の太さ (ピクセル単位)。 
	//				//SM_CXSIZEFRAMEは水平境界線の幅、SM_CYSIZEFRAMEは垂直境界線の高さです。
	//int cx_v_scroll = GetSystemMetrics(SM_CXVSCROLL);	//垂直スクロール バーの幅(ピクセル単位)。
	//int cy_h_scroll = GetSystemMetrics(SM_CYHSCROLL);	//水平スクロール バーの高さ(ピクセル単位)。
	////int cx_caption = GetSystemMetrics(SM_CXCAPTION);	//キャプション領域の高さ(ピクセル単位)。
	//int cy_caption = GetSystemMetrics(SM_CYCAPTION);	//キャプション領域の高さ(ピクセル単位)。
	//int cx_min = GetSystemMetrics(SM_CXMIN);	//ウィンドウの最小幅(ピクセル単位)。
	//int cy_min = GetSystemMetrics(SM_CYMIN);	//ウィンドウの最小高さ(ピクセル単位)。

	int cx_size_frame = GetSystemMetrics(SM_CXSIZEFRAME); // 境界線幅X方向
	int cy_size_frame = GetSystemMetrics(SM_CYSIZEFRAME); // 境界線幅Y方向
	int cy_caption = GetSystemMetrics(SM_CYCAPTION);     // タイトルバーの高さ
	RECT rct_1;
	{
		//クライアント領域が指定された大きさになるように計算して
		GetClientRect(g_hConWnd, &rct_1);	//現在のクライアント領域
		int w1 = rct_1.right - rct_1.left + 0;//1;
		int h1 = rct_1.bottom - rct_1.top + 0;//1;
		RECT rct_2;
		GetWindowRect(g_hConWnd, &rct_2);	//現在のウィンドウ領域
		int w2 = rct_2.right - rct_2.left + 0;//1;
		int h2 = rct_2.bottom - rct_2.top + 0;//1;
		//ウィンドウ領域とクライアント領域の差分を計算する
		int w3 = w2 - w1;
		int h3 = h2 - h1;
		//int w = _width * _font_w + w3;
		//int h = _height * _font_h + h3;
		int w = (_width * _font_w) + (cx_size_frame * 2);
		int h = (_height * _font_h) + (cy_size_frame * 2) + cy_caption;
		//指定されたサイズを持つウィンドウ領域になるようにウィンドウサイズを設定する
		//SetWindowPos(g_hConWnd, HWND_TOP, rct_2.left, rct_2.top, w, h, SWP_SHOWWINDOW /*| SWP_NOMOVE*/ );
		//SetWindowPos(g_hConWnd, HWND_TOP, rct_2.left, rct_2.top, _width * 2, _height * 2, SWP_SHOWWINDOW /*| SWP_NOMOVE*/);
		SetWindowPos(g_hConWnd, HWND_TOP, rct_2.left, rct_2.top, w, h, SWP_SHOWWINDOW /*| SWP_NOMOVE*/);
	}
	//WINDOWINFO	winfo;
	//winfo.cbSize = sizeof(WINDOWINFO);
	//GetWindowInfo(g_hConWnd, &winfo);



	//----------------------------------------------------------------
	//バッファサイズ（スクリーンバッファ｜フレームバッファ）
	g_ScreenBufferSize.X = _width;
	g_ScreenBufferSize.Y = _height;
	//フォントサイズ
	g_FontSizeEx.dwFontSize.X = _font_w;
	g_FontSizeEx.dwFontSize.Y = _font_h;
	//----------------------------------------------------------------
	// フォント情報の設定（指定があれば書体も設定する）
	g_FontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	g_FontSizeEx.FontFamily = (FF_DONTCARE | 0x00);	//書体不明＆モノスペース	//初期値？54（0x36:0b0011_0110）
	g_FontSizeEx.FontWeight = 100;	//細字	//初期値？400;
	//フォント名指定があればその名前をセットする。
	//フォント名指定が無ければ現在の値が使われる。
	//フォント名はUnicode指定（FaceNameがWCHARなので）
	if (_font_face_name != nullptr) {
		//PCONSOLE_FONT_INFOEX inf;
		memset(g_FontSizeEx.FaceName, 0, sizeof(g_FontSizeEx.FaceName));
		CopyMemory(g_FontSizeEx.FaceName, _font_face_name, LF_FACESIZE);
	}
	//----------------------------------------------------------------
	// スクリーンバッファの情報を設定（パレット含む）
	g_ScreenBufferInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	if (_pal16 != NULL) {
		//パレット指定があれば転送する。
		for (int n = 0; n < NUM_PALETTE; n++) {
			g_ScreenBufferInfoEx.ColorTable[n] = _pal16[n];
		}
	}
	else {
		//パレット指定が無ければこの直前のディスプレイ情報のパレットを設定する。
		for (int n = 0; n < NUM_PALETTE; n++) {
			g_ScreenBufferInfoEx.ColorTable[n] = g_OrgColorTableD2D[n];
		}
	}
	// バッファサイズ変更
	g_ScreenBufferSize.X = _width;
	g_ScreenBufferSize.Y = _height;
	// ウィンドウサイズ変更
	g_WindowSize.Left = 0;
	g_WindowSize.Top = 0;
	g_WindowSize.Right = _width;// - 1;
	g_WindowSize.Bottom = _height;// - 1;
	//コンソールスクリーンバッファ情報の設定
	g_ScreenBufferInfoEx.dwSize = g_ScreenBufferSize;	//文字の列と行のコンソール画面バッファーのサイズ
	g_ScreenBufferInfoEx.dwCursorPosition = { 0,0 };	//COORD{x,y}:コンソール画面バッファー内のカーソルの列座標と行座標
	g_ScreenBufferInfoEx.wAttributes = (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);	//画面バッファーの文字属性:文字=パレット#15／背景=パレット#0
	g_ScreenBufferInfoEx.srWindow = g_WindowSize;		//表示ウィンドウの左上隅と右下隅のコンソール画面のバッファー座標
	g_ScreenBufferInfoEx.dwMaximumWindowSize = g_ScreenBufferInfoEx.dwSize;	//コンソールウィンドウの最大サイズ
	g_ScreenBufferInfoEx.bFullscreenSupported = FALSE;			//全画面表示モードのサポート
	g_ScreenBufferInfoEx.ColorTable;	//コンソールの色設定:COLORREF[16]{0x00bbggrr,,,}
	//----------------------------------------------------------------
	//コンソール用バッファ作成
	//【注】ここまでに"g_ScreenBufferInfoEx"と"g_FontSizeEx"が設定済みである事。
	//出力用生成。
	g_DisplayHandleD2D = create_screen_buffer(&g_ScreenBufferInfoEx, &g_FontSizeEx);
	//入力用は起動時のと同じものを使う。
	SetConsoleMode(g_InputHandleD2D, CONSOLE_INPUT_MODE);	//入力ハンドルを入力モードに設定
	//現在の設定値でフォント変更
	//【注】コンソールバッファg_DisplayHandle[]のどれか一つが出来てからSetScreenFontSize()を呼び出す事。
	SetScreenFontSize(_font_w, _font_h);

	//クライアント領域が指定された大きさになるように計算して設定
	{
		GetClientRect(g_hConWnd, &rct_1);	//現在のクライアント領域
		int w1 = rct_1.right - rct_1.left + 1;
		int h1 = rct_1.bottom - rct_1.top + 1;
		RECT rct_2;
		GetWindowRect(g_hConWnd, &rct_2);	//現在のウィンドウ領域
		int w2 = rct_2.right - rct_2.left + 1;
		int h2 = rct_2.bottom - rct_2.top + 1;
		//ウィンドウ領域とクライアント領域の差分を計算する
		int w3 = w2 - w1;
		int h3 = h2 - h1;
		int w = _width * _font_w + w3;
		int h = _height * _font_h + h3;
		//指定されたサイズを持つウィンドウ領域になるようにウィンドウサイズを設定する
		SetWindowPos(g_hConWnd, HWND_TOP, rct_2.left, rct_2.top, w, h, SWP_SHOWWINDOW /*| SWP_NOMOVE*/);
	}

	//----------------------------------------------------------------
	//★★★　Direct2D 初期化 ★★★
	InitD2D(_width, _height);

	//ウィンドウサイズの固定
	FixWin();

	GetClientRect(g_hConWnd, &rct_1);
	//ここまでに設定済みの画面とフォントのサイズを取得
	GetConWinSize(g_ConWinSize);

	//初期化の為にキー入力呼び出し
	GetKeyAll();
	//フレーム同期の初期化
	InitFrameSync(60.0);
	return;

#else
	//----------------------------------------------------------------
	//ウィンドウサイズ変更のＯＦＦ
	g_OrgWindowStyle = FixWin();	//元のスタイルを保存
	//----------------------------------------------------------------
	// コマンド履歴を保存しない
	CONSOLE_HISTORY_INFO history_info;
	history_info.cbSize = sizeof(CONSOLE_HISTORY_INFO);
	history_info.HistoryBufferSize = 0;
	history_info.NumberOfHistoryBuffers = 0;
	history_info.dwFlags = 0;
	SetConsoleHistoryInfo(&history_info);

	//----------------------------------------------------------------
	// 直前のディスプレイ情報取得
	g_OrgOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);	//出力ハンドル
	GetConsoleMode(g_OrgOutputHandle, &g_OrgOutputHandleMode);	//出力コンソール情報
	//----------------------------------------------------------------
	//アクティブな画面バッファを設定する。
	SetConsoleActiveScreenBuffer(g_OrgOutputHandle);

	g_OrgInputHandle = GetStdHandle(STD_INPUT_HANDLE);

	GetConsoleMode(g_OrgInputHandle, &g_OrgInputHandleMode);
	//画面情報を保存しておく（16色パレット含む）
	g_OrgScreenBufferInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	GetConsoleScreenBufferInfoEx(g_OrgOutputHandle, &g_OrgScreenBufferInfoEx);
	// 現在のカラーパレットを保存
	for (int n = 0; n < NUM_PALETTE; n++) {
		g_OrgColorTable[n] = g_OrgScreenBufferInfoEx.ColorTable[n];
	}
	//フォントサイズ保存：オリジナル保存
	g_OrgFontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	GetCurrentConsoleFontEx(g_OrgOutputHandle, FALSE, &g_OrgFontSizeEx);	//第2パラメータがTRUEだと画面バッファと同じサイズが返るみたいだ・・・
	//現在のカーソル状態保存
	GetConsoleCursorInfo(g_OrgOutputHandle, &g_OrgCursorInfo);

	//----------------------------------------------------------------
	// フォント情報の設定（指定があれば書体も設定する）
	g_FontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	g_FontSizeEx.FontFamily = (FF_DONTCARE | 0x00);	//書体不明＆モノスペース	//初期値？54（0x36:0b0011_0110）
	g_FontSizeEx.FontWeight = 100;	//細字	//初期値？400;
	//フォント名指定があればその名前をセットする。
	//フォント名指定が無ければ現在の値が使われる。
	//フォント名はUnicode指定（FaceNameがWCHARなので）
	if (_font_face_name != nullptr) {
		//PCONSOLE_FONT_INFOEX inf;
		memset(g_FontSizeEx.FaceName, 0, sizeof(g_FontSizeEx.FaceName));
		CopyMemory(g_FontSizeEx.FaceName, _font_face_name, LF_FACESIZE);
	}

	//----------------------------------------------------------------
	// スクリーンバッファの情報を設定（パレット含む）
	g_ScreenBufferInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	if (_pal16 != NULL) {
		//パレット指定があれば転送する。
		for (int n = 0; n < NUM_PALETTE; n++) {
			g_ScreenBufferInfoEx.ColorTable[n] = _pal16[n];
		}
	}
	else {
		//パレット指定が無ければこの直前のディスプレイ情報のパレットを設定する。
		for (int n = 0; n < NUM_PALETTE; n++) {
			g_ScreenBufferInfoEx.ColorTable[n] = g_OrgColorTable[n];
		}
	}
	// バッファサイズ変更
	g_ScreenBufferSize.X = _width;
	g_ScreenBufferSize.Y = _height;
	// ウィンドウサイズ変更
	g_WindowSize.Left = 0;
	g_WindowSize.Top = 0;
	g_WindowSize.Right = _width;// - 1;
	g_WindowSize.Bottom = _height;// - 1;
	//コンソールスクリーンバッファ情報の設定
	g_ScreenBufferInfoEx.dwSize = g_ScreenBufferSize;	//文字の列と行のコンソール画面バッファーのサイズ
	g_ScreenBufferInfoEx.dwCursorPosition = { 0,0 };	//COORD{x,y}:コンソール画面バッファー内のカーソルの列座標と行座標
	g_ScreenBufferInfoEx.wAttributes = (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);	//画面バッファーの文字属性:文字=パレット#15／背景=パレット#0
	g_ScreenBufferInfoEx.srWindow = g_WindowSize;		//表示ウィンドウの左上隅と右下隅のコンソール画面のバッファー座標
	g_ScreenBufferInfoEx.dwMaximumWindowSize = g_ScreenBufferInfoEx.dwSize;	//コンソールウィンドウの最大サイズ
	g_ScreenBufferInfoEx.bFullscreenSupported = FALSE;			//全画面表示モードのサポート
	g_ScreenBufferInfoEx.ColorTable;	//コンソールの色設定:COLORREF[16]{0x00bbggrr,,,}

	//----------------------------------------------------------------
	//コンソール用バッファ作成
	//【注】ここまでに"g_ScreenBufferInfoEx"と"g_FontSizeEx"が設定済みである事。
	//出力用生成。
	g_DisplayHandle[0] = create_screen_buffer(&g_ScreenBufferInfoEx, &g_FontSizeEx);
	//g_DisplayHandle[0] = g_OrgOutputHandle;
	g_DisplayHandle[1] = NULL;
	//入力用は起動時のと同じものを使う。
	g_InputHandle = g_OrgInputHandle;
	SetConsoleMode(g_InputHandle, CONSOLE_INPUT_MODE);	//入力ハンドルを入力モードに設定

	//----------------------------------------------------------------
	//現在の設定値でフォント変更
	//【注】コンソールバッファg_DisplayHandle[]のどれか一つが出来てからSetScreenFontSize()を呼び出す事。
	SetScreenFontSize(_font_w, _font_h);

	//----------------------------------------------------------------
	//アクティブな画面バッファを設定する。
	SetConsoleActiveScreenBuffer(g_DisplayHandle[0]);

	//----------------------------------------------------------------
	// 16色用スクリーンバッファの配列を作成
	//g_ScreenBuffer4bit = (CHAR_INFO*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(CHAR_INFO));
	g_ScreenBuffer4bit = (WORD*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(WORD));
	//----------------------------------------------------------------
	// 16色用フレームバッファの配列を作成
	g_FrameBuffer4bit = (char*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(char));

	//----------------------------------------------------------------
	//１ピクセルのＲＧＢ値設定に使う"０００"～"２５５"の数値文字コードを作っておく。
	for (int i = 0; i < 256; i++) {
		CharRGBconvTBL[0][i] = 0x30 + (i / 100);		//０００～２５５の１００の位
		CharRGBconvTBL[1][i] = 0x30 + ((i % 100) / 10);	//０００～２５５の１０の位
		CharRGBconvTBL[2][i] = 0x30 + (i % 10);			//０００～２５５の１の位
	}

	//----------------------------------------------------------------
	//フルカラー画像用の初期化
	init_24bit_color_image();
	//----------------------------------------------------------------
	//２５６色パレット画像用の初期化
	init_256color_image();
	//ここまでに設定済みの画面とフォントのサイズを取得
	GetConWinSize(g_ConWinSize);
	//ダブルバッファ初期化
	if (_init_wbuf) {
		InitDoubleBuffer();
	}

	//ウィンドウサイズの固定
	FixWin();
	//初期化の為にキー入力呼び出し
	GetKeyAll();
	//フレーム同期の初期化
	InitFrameSync(60.0);
	return;
#endif	USED2D
}	//InitConioEx

//################################################################################################################################
//################################################################################################################################
//################################################################################################################################
/**
* @brief	conioexの終了処理
* @param	なし
* @return	なし
*/
void EndConioEx(void)
{

#ifdef USED2D
	EndD2D();
#else
	//スクリーンバッファ(24bitフルカラー)用の配列解放
	if (g_ScreenBufferFull != NULL) {
		free(g_ScreenBufferFull);
		g_ScreenBufferFull = NULL;
	}
	//フレームバッファ(24bitフルカラー)用の配列解放
	if (g_FrameBufferFull != NULL) {
		free(g_FrameBufferFull);
		g_FrameBufferFull = NULL;
	}
	//スクリーンバッファ(１６色画像)用の配列解放
	if (g_ScreenBuffer4bit) {
		free(g_ScreenBuffer4bit);
		g_ScreenBuffer4bit = NULL;
	}
	//フレームバッファ(１６色画像)用の配列解放
	if (g_FrameBuffer4bit) {
		free(g_FrameBuffer4bit);
		g_FrameBuffer4bit = NULL;
	}
#endif // USED2D

	SetConsoleActiveScreenBuffer(g_OrgOutputHandle);	//アクティブなコンソール画面バッファを切替え
	SetConsoleMode(g_OrgOutputHandle, g_OrgOutputHandleMode);	//コンソールモード復帰
	SetConsoleCursorInfo(g_OrgOutputHandle, &g_OrgCursorInfo);	//カーソル表示状態の復帰
	//フォントサイズは"SetCurrentConsoleFontEx()"が無くても戻るみたいだが・・・
	g_OrgFontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	SetCurrentConsoleFontEx(g_OrgOutputHandle, FALSE, &g_OrgFontSizeEx);	//フォントサイズを元に戻す
	//InitConioEx直前のスクリーンバッファ状態に戻す。
	g_OrgScreenBufferInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	SetConsoleScreenBufferInfoEx(g_OrgOutputHandle, &g_OrgScreenBufferInfoEx);	//これが無いとパレットが戻らない
#ifdef _DEBUG
	//TEST:設定値が格納されているか確認
	CONSOLE_SCREEN_BUFFER_INFOEX	csbiex{ sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
	GetConsoleScreenBufferInfoEx(g_OrgOutputHandle, &csbiex);
#endif // _DEBUG

#ifdef USED2D
	//ダブルバッファ削除
	if (g_DisplayHandleD2D != NULL) {
		CloseHandle(g_DisplayHandleD2D);
		g_DisplayHandleD2D = NULL;
	}
#else
	//ダブルバッファ削除
	if (g_DisplayHandle[0] != NULL) {
		CloseHandle(g_DisplayHandle[0]);
		g_DisplayHandle[0] = NULL;
	}
	if (g_DisplayHandle[1] != NULL) {
		CloseHandle(g_DisplayHandle[1]);
		g_DisplayHandle[1] = NULL;
	}
#endif // USED2D

	//window style recover.
#ifdef USED2D
	SetWindowLongPtr(g_hConWnd, GWL_STYLE, g_OrgWindowStylePtr);
#else
	SetWindowLong(g_hConWnd, GWL_STYLE, g_OrgWindowStyle);
#endif // USED2D
}	//EndConioEx

//################################################################################################################################
//################################################################################################################################
//################################################################################################################################

/**
* @brief	スクリーンバッファの生成
* @param	CONSOLE_SCREEN_BUFFER_INFOEX*:スクリーンバッファ情報のポインタ
* @param	CONSOLE_FONT_INFOEX*:フォント情報のポインタ
* @return	スクリーンバッファのハンドル
*	INVALID_HANDLE_VALUE:なら失敗
*/
static HANDLE create_screen_buffer(CONSOLE_SCREEN_BUFFER_INFOEX* pCsbix, CONSOLE_FONT_INFOEX* pCfix)
{
	HANDLE new_handle = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
#ifdef USED2D
	if (g_DisplayHandleD2D == INVALID_HANDLE_VALUE) {
		//printf("スクリーンバッファのハンドル取得に失敗しました\n");
		return INVALID_HANDLE_VALUE;
	}
#else
	if (g_DisplayHandle[0] == INVALID_HANDLE_VALUE) {
		//printf("スクリーンバッファのハンドル取得に失敗しました\n");
		return INVALID_HANDLE_VALUE;
	}
#endif // USED2D
	// フォントサイズの変更
	pCfix->cbSize = sizeof(CONSOLE_FONT_INFOEX);	//サイズは毎回設定した方が安全。
	SetCurrentConsoleFontEx(new_handle, FALSE, pCfix);
	//スクリーンバッファ情報の設定
	pCsbix->cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);	//サイズは毎回設定した方が安全。
	SetConsoleScreenBufferInfoEx(new_handle, pCsbix);
	SetConsoleMode(new_handle, CONSOLE_OUTPUT_MODE);	// バッファを上書きモードに
	//カーソル表示はＯＦＦにしておく。
	CONSOLE_CURSOR_INFO	cci;
	cci.dwSize = 1;
	cci.bVisible = FALSE;
	SetConsoleCursorInfo(new_handle, &cci);
#ifdef _DEBUG
	//TEST:設定値が格納されているか確認
	CONSOLE_SCREEN_BUFFER_INFOEX	csbiex{ sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) };
#ifdef USED2D
	GetConsoleScreenBufferInfoEx(g_DisplayHandleD2D, &csbiex);
#else
	GetConsoleScreenBufferInfoEx(g_DisplayHandle[0], &csbiex);
#endif // USED2D
#endif // _DEBUG
	return new_handle;
}	//create_screen_buffer

#ifndef USED2D
/**
 * @brief	ダブルバッファ初期化
 *
 * @return	バッファハンドル取得失敗
 */
int InitDoubleBuffer(void)
{
	// ダブルバッファ用のメモリーを確保
	if (g_DisplayHandle[0] == NULL) {
		g_DisplayHandle[0] = create_screen_buffer(&g_ScreenBufferInfoEx, &g_FontSizeEx);
		if (g_DisplayHandle[0] == INVALID_HANDLE_VALUE) {
			printf("ダブルバッファ[0]のハンドル取得に失敗しました\n");
			return -1;
		}
	}
	if (g_DisplayHandle[1] == NULL) {
		g_DisplayHandle[1] = create_screen_buffer(&g_ScreenBufferInfoEx, &g_FontSizeEx);
		if (g_DisplayHandle[1] == INVALID_HANDLE_VALUE) {
			printf("ダブルバッファ[1]のハンドル取得に失敗しました\n");
			return -1;
		}
	}
	return 0;
}	//InitDoubleBuffer

/**
 * @brief	ダブルバッファ時の描画面切替
 */
void FlipScreen(void)
{
	SetConsoleActiveScreenBuffer(g_DisplayHandle[g_SwapFlg]);	// バッファを入れ替え表示
	g_SwapFlg = (g_SwapFlg) ? 0 : 1;
}	//FlipScreen

/**
 * @brief	フォントサイズ変更
 *
 * @param	width [入力] フォントの横サイズ(1～)
 * @param	height [入力] フォントの縦サイズ(1～)
 */
void SetScreenFontSize(int width, int height)
{
	// フォントサイズ変更
	g_FontSizeEx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	g_FontSizeEx.dwFontSize.X = width;
	g_FontSizeEx.dwFontSize.Y = height;
	if (g_DisplayHandle[0] != NULL) {
		SetCurrentConsoleFontEx(g_DisplayHandle[0], FALSE, &g_FontSizeEx);
	}
	if (g_DisplayHandle[1] != NULL) {
		SetCurrentConsoleFontEx(g_DisplayHandle[1], FALSE, &g_FontSizeEx);
	}
}	//SetScreenFontSize
#endif // !USED2D

/*
* @brief	コンソールのクライアント領域のサイズとフォントサイズの取得
*
* @param	RECT& _r：結果を入れるRECT構造体の参照
*
* @return	RECT& : 結果を入れたRECT構造体の参照
*/
RECT& GetConWinSize(RECT& _r)
{
	CONSOLE_FONT_INFOEX fsx;
	fsx.cbSize = sizeof(CONSOLE_FONT_INFOEX);
#ifdef USED2D
	GetCurrentConsoleFontEx(g_DisplayHandleD2D, FALSE, &fsx);
#else
	GetCurrentConsoleFontEx(g_DisplayHandle[g_SwapFlg], FALSE, &fsx);
#endif // USED2D
	GetClientRect(g_hConWnd, &_r);
	_r.left = _r.right - _r.left;
	_r.top = _r.bottom - _r.top;
	_r.right = fsx.dwFontSize.X;
	_r.bottom = fsx.dwFontSize.Y;
	g_ConWinSize = _r;
	return _r;
}

#ifndef USED2D
/**
 * @brief	画面（スクリーンバッファ）消去
 */
void ClearScreen(void)
{
	DWORD fill_num;
	COORD screen_origin = { 0, 0 };	//初期化する値

	g_ScreenBufferAttribute = g_ScreenBufferInfoEx.wAttributes;

	FillConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg],
		g_ScreenBufferInfoEx.wAttributes,
		g_ScreenBufferInfoEx.dwSize.X * g_ScreenBufferInfoEx.dwSize.Y,
		screen_origin,
		&fill_num);
	FillConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg],
		TEXT(' '),
		g_ScreenBufferInfoEx.dwSize.X * g_ScreenBufferInfoEx.dwSize.Y,
		screen_origin,
		&fill_num);
	SetCursorPosition(g_ScreenBufferInfoEx.srWindow.Left + 1, g_ScreenBufferInfoEx.srWindow.Top + 1);
	//画像用バッファ消去
	ClearScreenBuffer(0);	//スクリーンバッファ(16色)消去
	ClearFrameBuffer();		//フレームバッファ(16色)消去
	ClearFrameBufferFull();	//フレームバッファ(24bitフルカラー)消去
}
#endif // !USED2D

//================================================================
// ウィンドウ
//================================================================
#ifdef USED2D
LONG_PTR FixWin(void)
{
	//ウィンドウサイズ変更禁止
	//HWND hCon = GetConsoleWindow();
	LONG_PTR lastStylePtr = GetWindowLongPtr(g_hConWnd, GWL_STYLE);
	LONG_PTR lStylePtr = lastStylePtr;
	lStylePtr &= ~(WS_THICKFRAME | WS_HSCROLL | WS_VSCROLL);	//'~'ビットごとの反転(１の補数)
	lStylePtr = SetWindowLongPtr(g_hConWnd, GWL_STYLE, lStylePtr);
	//SetWindowPos(hCon, NULL, 0, 0, frmb.width + 20, frmb.height, SWP_NOSIZE | SWP_NOZORDER);
	return lastStylePtr;
}	//FixWin
/**
* @brief	ウィンドウサイズを固定する
*
* @param	int _x,int _y	表示位置の指定
*
* @return	LONG	変更前の状態を返す
*/
LONG_PTR FixWin(int _x, int _y)
{
	//ウィンドウサイズ変更禁止
	//HWND hCon = GetConsoleWindow();
	LONG_PTR lastStylePtr = GetWindowLong(g_hConWnd, GWL_STYLE);
	LONG_PTR lStyle = lastStylePtr;
	lStyle &= ~(WS_THICKFRAME | WS_HSCROLL | WS_VSCROLL);	//'~'ビットごとの反転(１の補数)
	lStyle = SetWindowLongPtr(g_hConWnd, GWL_STYLE, lStyle);
	//SWP_NOSIZEを指定しているので、座標(_x,_y)のみがに変更される。
	SetWindowPos(g_hConWnd, NULL, _x, _y, -1, -1, SWP_NOSIZE | SWP_NOZORDER);
	return lastStylePtr;
}	//FixWin
#else
/**
* @brief	ウィンドウサイズを固定する
*
* @return	LONG	変更前の状態を返す
*/
LONG FixWin(void)
{
	//ウィンドウサイズ変更禁止
	//HWND hCon = GetConsoleWindow();
	LONG lastStyle = GetWindowLong(g_hConWnd, GWL_STYLE);
	LONG lStyle = lastStyle;
	lStyle &= ~(WS_THICKFRAME | WS_HSCROLL | WS_VSCROLL);	//'~'ビットごとの反転(１の補数)
	lStyle = SetWindowLong(g_hConWnd, GWL_STYLE, lStyle);
	//SetWindowPos(hCon, NULL, 0, 0, frmb.width + 20, frmb.height, SWP_NOSIZE | SWP_NOZORDER);
	return lastStyle;
}	//FixWin
/**
* @brief	ウィンドウサイズを固定する
*
* @param	int _x,int _y	表示位置の指定
*
* @return	LONG	変更前の状態を返す
*/
LONG FixWin(int _x, int _y)
{
	//ウィンドウサイズ変更禁止
	//HWND hCon = GetConsoleWindow();
	LONG lastStyle = GetWindowLong(g_hConWnd, GWL_STYLE);
	LONG lStyle = lastStyle;
	lStyle &= ~(WS_THICKFRAME | WS_HSCROLL | WS_VSCROLL);	//'~'ビットごとの反転(１の補数)
	lStyle = SetWindowLong(g_hConWnd, GWL_STYLE, lStyle);
	//SWP_NOSIZEを指定しているので、座標(_x,_y)のみがに変更される。
	SetWindowPos(g_hConWnd, NULL, _x, _y, -1, -1, SWP_NOSIZE | SWP_NOZORDER);
	return lastStyle;
}	//FixWin
#endif	//USED2D

/**
* @brief	現在のディスプレイハンドルを取得する。
*
* @return	HANDLE	現在のディスプレイハンドル
*/
HANDLE GetCurrentHandle(void) {
#ifdef USED2D
	return g_DisplayHandleD2D;
#else
	return g_DisplayHandle[g_SwapFlg];
#endif // USED2D
}

/**
* @brief	コンソールウィンドウのタイトルバーにテキストを設定
*
* @param	title [入力] ウィンドウタイトルに表示するテキスト
*/
void SetCaption(const char* title)
{
	SetConsoleTitleA(title);
}

/**
* @brief	コンソールウィンドウのタイトルバーに書式指定してテキストを設定
*
* @param	const char *_format：書式指定文字列
* @param	...：可変長引数
*/
void SetCaptionF(const char* _format, ...)
{
	va_list ap;
	va_start(ap, _format);
	size_t length = _vscprintf(_format, ap) + 1;	//'\0'含まないので＋１している
	char* buf = (char*)_malloca(length);
	vsprintf_s(buf, length, _format, ap);
	SetConsoleTitleA(buf);
	va_end(ap);
}	//SetCaptionFA

/**
* @brief	コンソールウィンドウのタイトルバーに表示されるテキストを取得
*
* @param	title [出力] 現在のウィンドウタイトルのテキスト
* @param	len [入力] ウィンドウタイトルの文字数
*
* @retval	非0	現在のウィンドウタイトルの文字数
* @retval	0	エラー
*/
int GetCaption(char* title, int len)
{
	return GetConsoleTitleA(title, len);
}	//GetCaption

#ifdef USED2D

//================================================================
//カーソル
//================================================================
/**
 * @brief	水平方向のカーソル位置を取得
 *
 * @return	現在のカーソル位置のX座標(1～)
 */
int GetCursorX(void)
{
	//return g_ScreenBufferInfoEx.dwCursorPosition.X - g_ScreenBufferInfoEx.srWindow.Left + 1;
	return	g_CursorPosD2D.X + 1;
}	//GetCursorX

/**
 * @brief	垂直方向のカーソル位置を取得
 *
 * @return	現在のカーソル位置のY座標(1～)
 */
int GetCursorY(void)
{
	//return g_ScreenBufferInfoEx.dwCursorPosition.Y - g_ScreenBufferInfoEx.srWindow.Top + 1;
	return	g_CursorPosD2D.Y + 1;
}	//GetCursorY

/**
 * @brief	カーソル位置の移動
 *
 * @param	x [入力] X座標(1～)
 * @param	y [入力] Y座標(1～)
 */
void SetCursorPosition(int _csr_x, int _csr_y)
{
	g_CursorPosD2D.X = _csr_x - 1;
	g_CursorPosD2D.Y = _csr_y - 1;
}	//SetCursorPosition

/**
 * @brief	カーソルタイプ設定
 *
 * @param	type [入力]\n
 *						NOCURSOR カーソル表示なし\n
 *						SOLIDCURSOR (非対応)\n
 *						NORMALCURSOR カーソルの通常表示\n
 */
void SetCursorType(int type)
{
}	//SetCursorType
#else

//================================================================
//カーソル
//================================================================
/**
 * @brief	水平方向のカーソル位置を取得
 *
 * @return	現在のカーソル位置のX座標(1～)
 */
int GetCursorX(void)
{
	return g_ScreenBufferInfoEx.dwCursorPosition.X - g_ScreenBufferInfoEx.srWindow.Left + 1;
}	//GetCursorX

/**
 * @brief	垂直方向のカーソル位置を取得
 *
 * @return	現在のカーソル位置のY座標(1～)
 */
int GetCursorY(void)
{
	return g_ScreenBufferInfoEx.dwCursorPosition.Y - g_ScreenBufferInfoEx.srWindow.Top + 1;
}	//GetCursorY

/**
 * @brief	カーソル位置の移動
 *
 * @param	x [入力] X座標(1～)
 * @param	y [入力] Y座標(1～)
 */
void SetCursorPosition(int x, int y)
{
	COORD lc;

	lc.X = x - 1;
	lc.Y = g_ScreenBufferInfoEx.srWindow.Top + y - 1;

	SetConsoleCursorPosition(g_DisplayHandle[g_SwapFlg], lc);
}	//SetCursorPosition

/**
 * @brief	カーソルタイプ設定
 *
 * @param	type [入力]\n
 *						NOCURSOR カーソル表示なし\n
 *						SOLIDCURSOR (非対応)\n
 *						NORMALCURSOR カーソルの通常表示\n
 */
void SetCursorType(int type)
{
	CONSOLE_CURSOR_INFO	cursor_info;
	int size = -1;

	if (size < 0) {
		if (GetConsoleCursorInfo(g_DisplayHandle[g_SwapFlg], &cursor_info)) {
			size = (int)cursor_info.dwSize;
		}
		else {
			size = 25;
		}
	}
	cursor_info.dwSize = (type < NORMALCURSOR) ? 100 : size;
	cursor_info.bVisible = (type != NOCURSOR);
	SetConsoleCursorInfo(g_DisplayHandle[g_SwapFlg], &cursor_info);
}	//SetCursorType
#endif // USED2D

/**
* @brief	マウス座標の取得
*
* @param	POINT* _mp：座標を受け取るPOINT構造体へのポインタ
*
* @return	POINT：マウスの座標（文字単位）
*
* @note		_mpにはクライアント座標が返される
*			（ポインタがNULLならクライアント座標は格納しない）
*			戻り値は文字単位に換算した座標が返される
*/
POINT GetCursorMousePos(POINT* _mp)
{
	POINT mpos = { 0,0 };	//戻り値用
	GetCursorPos(&mpos);	//現在の位置
	ScreenToClient(GetConsoleWindow(), &mpos);	//クライアント座標へ変換
	if (_mp != NULL) {
		*_mp = mpos;	//クライアント座標を返す
	}
	//文字単位の座標に変換
	mpos.x /= g_FontSizeEx.dwFontSize.X;
	mpos.y /= g_FontSizeEx.dwFontSize.Y;
	return mpos;	//文字単位の座標として返す
}	//GetCursorMousePos

#ifdef USED2D
//================================================================
//文字列描画
//================================================================
/**
 * @brief	文字列の出力（マルチバイト文字用）
 *
 * @param	_srcbuf [入力] 出力文字列配列のポインタ
 * @param	_size [入力] 出力文字数
 */
void PrintStringA(const char* _srcbuf, int _size)
{
	//表示文字数が指定文字列のサイズをオーバーしていたら補正する
	int src_len = (int)strlen(_srcbuf);
	if (_size < 0) {
		_size = src_len;
	}
	else if (_size > src_len) {
		_size = (int)strlen(_srcbuf);
	}
	g_PrintStringCompatibleMode = true;
	//void WriteTextA(int _xp, int _yp, const char* _text, double _scale, D2D1::ColorF _fgc, D2D1::ColorF _bgc, bool _new_line)
	WriteTextA(g_CursorPosD2D.X, g_CursorPosD2D.Y, _srcbuf, 1.0f, D2D1::ColorF(1, 1, 1, 1), D2D1::ColorF(0, 0, 0, 0), false);
}	//PrintStringA

/**
 * @brief	文字列の出力（Unicode文字用）
 *
 * @param	_srcbuf [入力] 出力文字列配列のポインタ
 * @param	_size [入力] 出力文字数
 */
void PrintStringW(const wchar_t* _srcbuf, int _size)
{
	//表示文字数が指定文字列のサイズをオーバーしていたら補正する
	int src_len = (int)wcslen(_srcbuf);
	if (_size < 0) {
		_size = src_len;
	}
	else if (_size > src_len) {
		_size = (int)wcslen(_srcbuf);
	}
	g_PrintStringCompatibleMode = true;
	WriteTextW(g_CursorPosD2D.X, g_CursorPosD2D.Y, _srcbuf, 1.0f, D2D1::ColorF(1, 1, 1, 1), D2D1::ColorF(0, 0, 0, 0), false);
}	//PrintStringW

#ifdef UNICODE
	/**
	 * @brief	文字列の出力(マルチバイト⇒Unicode変換版)
	 *
	 * @param	_src [入力] 出力文字列配列のポインタ
	 * @param	_size [入力] 出力文字数
	 */
void PrintString(const char* _src, int _size)
{
	//指定されたマルチバイト文字全てをワイド文字(Unicode文字)変換した場合の
	//必要なバッファーサイズ (終端の null 文字を含む) を文字単位で算出し、
	//その文字数分のバッファーを確保する。
	int wc_src_siz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _src, -1, NULL, 0);
	size_t wc_src_bytes = (wc_src_siz * sizeof(wchar_t));	//Unicode文字換算のバイト数。
	wchar_t* wc_src = (wchar_t*)_malloca(wc_src_bytes);	//スタック上に確保：free不要
	memset(wc_src, 0, wc_src_bytes);	//０クリア：書き込みが途中まででも'\0'終端文字列になる。
	//指定サイズが元の文字数をオーバーしている場合の補正
	if ((int)strlen(_src) < _size) {
		_size = (-1);	//(-1)指定で'\0'まで変換。
	}
	//指定サイズ分変換する（_size == (-1))なら'\0'まで全て変換する）
	//戻り値は変換した(バッファに書き込まれた)文字数が返る。
	//【注】(-1)指定で変換した場合、戻り値は'\0'を含む文字数になる。
	int disp_siz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _src, _size, wc_src, wc_src_siz);
	//disp_sizは'\0'を含む文字数かもしれないので、変換した文字数になる様に再計算する。
	disp_siz = (int)wcslen(wc_src);
	//DWORD num;	//実際に書き込まれた文字数を受け取る変数
	//WriteConsoleW(g_DisplayHandle[g_SwapFlg], wc_src, disp_siz, &num, NULL);
	g_PrintStringCompatibleMode = true;
	WriteTextW(g_CursorPosD2D.X, g_CursorPosD2D.Y, wc_src, 1.0f, D2D1::ColorF(1, 1, 1, 1), D2D1::ColorF(0, 0, 0, 0), false);
}	//PrintString
#endif	//UNICODE

//----------------
//文字全体
//----------------
void SetHighVideoColor(void) {}
void SetLowVideoColor(void) {}
void SetNormalVideoColor(void) {}
void SetTextBackColor(int color) {}
void SetConsoleTextColor(int color) {}
void SetTextAttribute(int attribute) {}
//----------------
//行操作
//----------------
void ClearLine(void) {}
void InsertLine(void) {}
void DeleteLine(void) {}

#else

//================================================================
//文字列描画
//================================================================
/**
 * @brief	文字列の出力（マルチバイト文字用）
 *
 * @param	_srcbuf [入力] 出力文字列配列のポインタ
 * @param	_size [入力] 出力文字数
 */
void PrintStringA(const char* _srcbuf, int _size)
{
	//表示文字数が指定文字列のサイズをオーバーしていたら補正する
	int src_len = (int)strlen(_srcbuf);
	if (_size < 0) {
		_size = src_len;
	}
	else if (_size > src_len) {
		_size = (int)strlen(_srcbuf);
	}
	DWORD num;
	WriteConsoleA(g_DisplayHandle[g_SwapFlg], _srcbuf, _size, &num, NULL);
}	//PrintStringA

/**
 * @brief	文字列の出力（Unicode文字用）
 *
 * @param	_srcbuf [入力] 出力文字列配列のポインタ
 * @param	_size [入力] 出力文字数
 */
void PrintStringW(const wchar_t* _srcbuf, int _size)
{
	//表示文字数が指定文字列のサイズをオーバーしていたら補正する
	int src_len = (int)wcslen(_srcbuf);
	if (_size < 0) {
		_size = src_len;
	}
	else if (_size > src_len) {
		_size = (int)wcslen(_srcbuf);
	}
	DWORD num;
	WriteConsoleW(g_DisplayHandle[g_SwapFlg], _srcbuf, _size, &num, NULL);
}	//PrintStringW

#ifdef UNICODE
	/**
	 * @brief	文字列の出力(マルチバイト⇒Unicode変換版)
	 *
	 * @param	_src [入力] 出力文字列配列のポインタ
	 * @param	_size [入力] 出力文字数
	 */
void PrintString(const char* _src, int _size)
{
#if true

	//指定されたマルチバイト文字全てをワイド文字(Unicode文字)変換した場合の
	//必要なバッファーサイズ (終端の null 文字を含む) を文字単位で算出し、
	//その文字数分のバッファーを確保する。
	int wc_src_siz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _src, -1, NULL, 0);
	size_t wc_src_bytes = (wc_src_siz * sizeof(wchar_t));	//Unicode文字換算のバイト数。
	wchar_t* wc_src = (wchar_t*)_malloca(wc_src_bytes);	//スタック上に確保：free不要
	memset(wc_src, 0, wc_src_bytes);	//０クリア：書き込みが途中まででも'\0'終端文字列になる。
	//指定サイズが元の文字数をオーバーしている場合の補正
	if ((int)strlen(_src) < _size) {
		_size = (-1);	//(-1)指定で'\0'まで変換。
	}
	//指定サイズ分変換する（_size == (-1))なら'\0'まで全て変換する）
	//戻り値は変換した(バッファに書き込まれた)文字数が返る。
	//【注】(-1)指定で変換した場合、戻り値は'\0'を含む文字数になる。
	int disp_siz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _src, _size, wc_src, wc_src_siz);
	//disp_sizは'\0'を含む文字数かもしれないので、変換した文字数になる様に再計算する。
	disp_siz = (int)wcslen(wc_src);
	DWORD num;	//実際に書き込まれた文字数を受け取る変数
	WriteConsoleW(g_DisplayHandle[g_SwapFlg], wc_src, disp_siz, &num, NULL);
#else
	DWORD num;
	WCHAR wide_char[256];
	memset(wide_char, 0, sizeof(wide_char));
	//MB_COMPOSITE⇒濁点・半濁点文字が２文字に変換される：例）パ⇒パ、ば⇒ば
	int ret_val = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, _src, _size, wide_char, _size);
	WriteConsoleW(g_DisplayHandle[g_SwapFlg], wide_char, ret_val, &num, NULL);
#endif // true
}	//PrintString
#endif	//UNICODE

//----------------
//文字全体
//----------------
/**
 * @brief	文字色高輝度化
 */
void SetHighVideoColor(void)
{
	SetConsoleTextAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBufferInfoEx.wAttributes |= FOREGROUND_INTENSITY);
}	//SetHighVideoColor

/**
 * @brief	文字色低輝度化
 */
void SetLowVideoColor(void)
{
	SetConsoleTextAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBufferInfoEx.wAttributes &= ~FOREGROUND_INTENSITY);
}	//SetLowVideoColor

/**
 * @brief	既定文字色設定
 */
void SetNormalVideoColor(void)
{
	SetConsoleTextAttribute(g_DisplayHandle[g_SwapFlg], LIGHTGRAY);
}	//SetNormalVideoColor

//----------------
//文字属性指定
//----------------
/**
 * @brief	文字背景色設定
 *
 * @param	color [入力] 文字背景色
 * @note
 *	背景色はenum COLORSを参照する
 */
void SetTextBackColor(int color)
{
	g_ScreenBufferInfoEx.wAttributes &= ~0x00f0;
	//g_ScreenBufferInfoEx.wAttributes |= ((color & 0x07) << 4);
	g_ScreenBufferInfoEx.wAttributes |= ((color & 0x0F) << 4);
	SetConsoleTextAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBufferInfoEx.wAttributes);
}	//SetTextBackColor

/**
 * @brief	文字色設定
 *
 * @param	color [入力] 文字色
 * @note
 *	文字色はenum COLORSを参照する
 */
void SetConsoleTextColor(int color)
{
	g_ScreenBufferInfoEx.wAttributes &= ~0x000f;
	g_ScreenBufferInfoEx.wAttributes |= (color & 0x0f);
	SetConsoleTextAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBufferInfoEx.wAttributes);
}	//SetConsoleTextColor

/**
 * @brief	文字色背景色同時設定
 *
 * @param	attribute [入力] 文字背景情報
 * @note
 *	以下の各設定値をビット毎のOR演算を用いて引数に指定する
 *	  FOREGROUND_BLUE	   0x0001 // text color contains blue.
 *	  FOREGROUND_GREEN	   0x0002 // text color contains green.
 *	  FOREGROUND_RED	   0x0004 // text color contains red.
 *	  FOREGROUND_INTENSITY 0x0008 // text color is intensified.
 *	  BACKGROUND_BLUE	   0x0010 // background color contains blue.
 *	  BACKGROUND_GREEN	   0x0020 // background color contains green.
 *	  BACKGROUND_RED	   0x0040 // background color contains red.
 *	  BACKGROUND_INTENSITY 0x0080 // background color is intensified.
 *	  COMMON_LVB_LEADING_BYTE	 0x0100 // Leading Byte of DBCS
 *	  COMMON_LVB_TRAILING_BYTE	 0x0200 // Trailing Byte of DBCS
 *	  COMMON_LVB_GRID_HORIZONTAL 0x0400 // DBCS: Grid attribute: top horizontal.
 *	  COMMON_LVB_GRID_LVERTICAL  0x0800 // DBCS: Grid attribute: left vertical.
 *	  COMMON_LVB_GRID_RVERTICAL  0x1000 // DBCS: Grid attribute: right vertical.
 *	  COMMON_LVB_REVERSE_VIDEO	 0x4000 // DBCS: Reverse fore/back ground attribute.
 *	  COMMON_LVB_UNDERSCORE 	 0x8000 // DBCS: Underscore.
 *	  COMMON_LVB_SBCSDBCS		 0x0300 // SBCS or DBCS flag.
 */
void SetTextAttribute(int attribute)
{
	SetConsoleTextAttribute(g_DisplayHandle[g_SwapFlg], attribute);
}	//SetTextAttribute
//----------------
//行操作
//----------------
/**
 * @brief	行末まで消去
 */
void ClearLine(void)
{
	DWORD fill_num;

	FillConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg],
		g_ScreenBufferInfoEx.wAttributes,
		g_ScreenBufferInfoEx.srWindow.Right - g_ScreenBufferInfoEx.dwCursorPosition.X + 1,
		g_ScreenBufferInfoEx.dwCursorPosition,
		&fill_num);
	FillConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg],
		TEXT(' '),
		g_ScreenBufferInfoEx.srWindow.Right - g_ScreenBufferInfoEx.dwCursorPosition.X + 1,
		g_ScreenBufferInfoEx.dwCursorPosition,
		&fill_num);
}	//ClearLine

/**
 * @brief	現在行に挿入
 */
void InsertLine(void)
{
	COORD	lc;
	DWORD	len;
	DWORD	num;
	LPTSTR	psz;
	LPWORD	pw;

	lc.X = g_ScreenBufferInfoEx.srWindow.Left;
	len = g_ScreenBufferInfoEx.srWindow.Right - g_ScreenBufferInfoEx.srWindow.Left + 1;
	psz = (LPTSTR)_malloca(len * sizeof(TCHAR));
	pw = (LPWORD)_malloca(len * sizeof(WORD));
	for (lc.Y = g_ScreenBufferInfoEx.srWindow.Bottom; lc.Y > g_ScreenBufferInfoEx.dwCursorPosition.Y; lc.Y--) {
		lc.Y--;
		ReadConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], pw, len, lc, &num);
		ReadConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg], psz, len, lc, &num);
		lc.Y++;
		WriteConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], pw, len, lc, &num);
		WriteConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg], psz, len, lc, &num);
	}
	FillConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBufferInfoEx.wAttributes, len, lc, &num);
	FillConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg], TEXT(' '), len, lc, &num);
}	//InsertLine

/**
 * @brief	現在行の削除
 */
void DeleteLine(void)
{
	DWORD	read_num;
	DWORD	write_num;
	DWORD	fill_num;
	COORD	calc_coord;
	DWORD	line_len;
	LPTSTR	receive_character;
	LPWORD	receive_attribute;

	calc_coord.X = g_ScreenBufferInfoEx.srWindow.Left;
	line_len = g_ScreenBufferInfoEx.srWindow.Right - g_ScreenBufferInfoEx.srWindow.Left + 1;
	receive_character = (LPTSTR)_malloca(line_len * sizeof(TCHAR));
	receive_attribute = (LPWORD)_malloca(line_len * sizeof(WORD));

	for (calc_coord.Y = g_ScreenBufferInfoEx.dwCursorPosition.Y; calc_coord.Y < g_ScreenBufferInfoEx.srWindow.Bottom; calc_coord.Y++) {
		calc_coord.Y++;
		ReadConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], receive_attribute, line_len, calc_coord, &read_num);
		ReadConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg], receive_character, line_len, calc_coord, &read_num);
		calc_coord.Y--;
		WriteConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], receive_attribute, line_len, calc_coord, &write_num);
		WriteConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg], receive_character, line_len, calc_coord, &write_num);
	}

	FillConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBufferAttribute, line_len, calc_coord, &fill_num);
	FillConsoleOutputCharacter(g_DisplayHandle[g_SwapFlg], TEXT(' '), line_len, calc_coord, &fill_num);
}	//DeleteLine
#endif // !USED2D


//================================================================
// 拡張文字列処理
//================================================================
/**
* @brief
* 半角文字を全角文字に変換する（マルチバイト版）
*
* @param	const char* _src	変換元になる文字列（マルチバイト文字）
*
* @return	char*\n
* 変換後の文字列が入っているバッファへのポインタ。\n
* 【注】戻り値にmalloc()したポインタを返すので、呼び出した側で必ずfree()する事。
*/
char* HanToZenA(const char* _src)
{
	//const char* _src = "変換するabcxyz;*@文字列1234567890";
	/*
	* 一旦Unicode文字列に変換したものを全角文字に変換してマルチバイト文字列に戻している。
	*	MultiByteToWideChar()：マルチバイト文字列からUnicode文字列へ変換
	*	 LCMapStringEx()：半角から全角へ変換
	*	 WideCharToMultiByte()：マルチバイト文字列からUbicode文字列へ変換
	*/
	//---- マルチバイト文字列をUnicode文字列に変換する
	int wc_count = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _src, -1, NULL, 0);	//'\0'を含む文字数が返る
	size_t wc_src_bytes = (wc_count * sizeof(wchar_t));
	wchar_t* src_txt = (wchar_t*)_malloca(wc_src_bytes);
	memset(src_txt, 0, wc_src_bytes);
	int disp_siz = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, _src, -1, src_txt, wc_count);
	//---- Unicode文字列の半角文字を全角文字に変換する ----
	DWORD flags = LCMAP_FULLWIDTH;		//全角文字に変換
	//	DWORD flags = LCMAP_HALFWIDTH;		//半角文字に変換
	//	DWORD flags = LCMAP_HIRAGANA;		//ひらがなに変換
	//	DWORD flags = LCMAP_KATAKANA;		//カタカナに変換
	int dest_size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, src_txt, -1, NULL, 0, NULL, NULL, 0);
	wchar_t* dest_buf = (wchar_t*)_malloca(dest_size * sizeof(wchar_t));
	memset(dest_buf, 0, dest_size * sizeof(wchar_t));
	int output_size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, src_txt, -1, dest_buf, dest_size, NULL, NULL, 0);
	//---- Unicode文字列をマルチバイト文字列に変換する ----
	//文字数を計測
	int mb_bytes = WideCharToMultiByte(CP_ACP, 0, dest_buf, -1, NULL, 0, NULL, NULL);	//'\0'含むサイズが戻る
	//変換先バッファを確保。
	char* mb_dest_buff = (char*)calloc(mb_bytes, sizeof(char));
	memset(mb_dest_buff, 0, mb_bytes);	//変換先バッファを０で初期化
	//変換
	int res = WideCharToMultiByte(CP_ACP, 0, dest_buf, -1, mb_dest_buff, mb_bytes, NULL, NULL);
	return mb_dest_buff;	//変換済み文字列バッファ(【注】動的メモリ確保したポインタ)を返す。
}	//HanToZenA

/**
* @brief
* 半角文字を全角文字に変換する（Unicode版）
*
* @param	const wchar_t* _src	変換元になる文字列（Unicode文字）
*
* @return	wchar_t*\n
* 変換後の文字列が入っているバッファへのポインタ。\n
* 【注】戻り値にmalloc()したポインタを返すので、呼び出した側で必ずfree()する事。
*/
wchar_t* HanToZenW(const wchar_t* _src)
{
	//---- Unicode文字列の半角文字を全角文字に変換する ----
	DWORD flags = LCMAP_FULLWIDTH;		//全角文字に変換
	//	DWORD flags = LCMAP_HALFWIDTH;		//半角文字に変換
	//	DWORD flags = LCMAP_HIRAGANA;		//ひらがなに変換
	//	DWORD flags = LCMAP_KATAKANA;		//カタカナに変換
	//文字数を計測
	int dest_size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, _src, -1, NULL, 0, NULL, NULL, 0);
	//変換先バッファを確保。
	wchar_t* dest_buf = (wchar_t*)calloc(dest_size, sizeof(wchar_t));
	memset(dest_buf, 0, dest_size * sizeof(wchar_t));	//変換先バッファを０で初期化
	//変換
	int output_size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, _src, -1, dest_buf, dest_size, NULL, NULL, 0);
	return dest_buf;	//変換済み文字列バッファ(【注】動的メモリ確保したポインタ)を返す。
}	//HanToZenW

/**
* @brief
* 書式指定付文字列描画（引数リスト版）（マルチバイト文字用）
*
* @param	bool _zenkaku	trueなら全ての文字を全角で出力
* 例）
*	false:"全角%d",99 -> "全角99"
*	true:"全角%d",99 -> "全角９９"
* @param	const char* _format	書式指定文字列（マルチバイト文字）
* @param	va_list _ap	任意の可変長引数
*/
void VPrintStringFA(bool _zenkaku, const char* _format, va_list _ap)
{
	size_t length = _vscprintf(_format, _ap) + 1;	//'\0'含まないので＋１している
	char* buf = (char*)_malloca(length);
	vsprintf_s(buf, length, _format, _ap);
	if (_zenkaku == true) {
		char* p = HanToZen(buf);
		PrintString(p, -1);
		free(p);
	}
	else {
		PrintString(buf, -1);
	}
}	//VPrintStringFA

/**
* @brief
* 書式指定付文字列描画（引数リスト版）（Unicode文字用）
*
* @param	bool _zenkaku	trueなら全ての文字を全角で出力
* 例）
*	false:"全角%d",99 -> "全角99"
*	true:"全角%d",99 -> "全角９９"
* @param	const wchar_t* _format	書式指定文字列（Unicode文字）
* @param	va_list _ap	任意の可変長引数
*/
void VPrintStringFW(bool _zenkaku, const wchar_t* _format, va_list _ap)
{
	size_t length = _vscwprintf(_format, _ap) + 1;	//'\0'含まないので＋１している
	wchar_t* buf = (wchar_t*)_malloca(length * sizeof(wchar_t));
	vswprintf_s(buf, length, _format, _ap);
	if (_zenkaku == true) {
		wchar_t* p = HanToZenW(buf);
		PrintStringW(p, -1);
		free(p);
	}
	else {
		PrintStringW(buf, -1);
	}
}	//VPrintStringFW

/**
* @brief
* 書式指定付文字列描画（マルチバイト文字用）
*
* @param	bool _zenkaku	trueを指定すると、全ての文字(空白' 'も含む)を全角に変換して表示する。\n
* 例）\n
*	false:"全角%d",99 -> "全角99"\n
*	 true:"全角%d",99 -> "全角９９"\n
*	false:"全角%s","あ1い2う"-> "全角あ1い2う"\n
*	 true:"全角%s","あ1い2う"-> "全角あ１い２う"\n
*	false:"全角%-4s","９"-> "全角９  "\n
*	 true:"全角%-4s","９"-> "全角９　　"\n
* 【注】文字列指定に幅指定が入っている時、全角半角交じりの文字列だと位置を合わせにくい。\n
*		変換元が数値の場合、全角/半角の区別は無いが、\n
*		変換元が文字列の場合、全角/半角が混じる場合があるので、位置合わせが難しくなる。\n
* @param	const char* _format	書式指定文字列（マルチバイト文字）
* @param	...	任意の可変長引数
*
* @note
* ※_zenkaku以降はprintf()と同じ仕様。
*/
void PrintStringFA(bool _zenkaku, const char* _format, ...)
{
	va_list ap;
	va_start(ap, _format);
	VPrintStringFA(_zenkaku, _format, ap);
	va_end(ap);
}	//PrintStringFA

/**
* @brief
* 書式指定付文字列描画（Unicode文字用）
*
* @param	bool _zenkaku	trueを指定すると、全ての文字(空白' 'も含む)を全角に変換して表示する。\n
* 例）\n
*	false:"全角%d",99 -> "全角99"\n
*	 true:"全角%d",99 -> "全角９９"\n
*	false:"全角%s","あ1い2う"-> "全角あ1い2う"\n
*	 true:"全角%s","あ1い2う"-> "全角あ１い２う"\n
*	false:"全角%-4s","９"-> "全角９  "\n
*	 true:"全角%-4s","９"-> "全角９　　"\n
* 【注】文字列指定に幅指定が入っている時、全角半角交じりの文字列だと位置を合わせにくい。\n
*		変換元が数値の場合、全角/半角の区別は無いが、\n
*		変換元が文字列の場合、全角/半角が混じる場合があるので、位置合わせが難しくなる。\n
* @param	const wchar_t* _format	書式指定文字列（Unicode文字）
* @param	...	任意の可変長引数
*
* @note
* ※_zenkaku以降はprintf()と同じ仕様。
*/
void PrintStringFW(bool _zenkaku, const wchar_t* _format, ...)
{
	va_list ap;
	va_start(ap, _format);
	VPrintStringFW(_zenkaku, _format, ap);
	va_end(ap);
}	//PrintStringFW

/**
* @brief
* 位置指定＆書式指定付文字列描画（マルチバイト文字用）
*
* @param	int _xp	座標Ｘ指定（１オリジン）
* @param	int _yp	座標Ｙ指定（１オリジン）
* @param	bool _zenkaku	trueを指定すると、全ての文字(空白' 'も含む)を全角に変換して表示する。
* @param	const wchar_t* _format	書式指定文字列（マルチバイト文字）
* @param	...	任意の可変長引数\n
*
* @note
* ※_zenkaku以降はprintf()と同じ仕様。
*/
void PosPrintStringFA(int _xp, int _yp, bool _zenkaku, const char* _format, ...)
{
	SetCursorPosition(_xp, _yp);
	va_list ap;
	va_start(ap, _format);
	VPrintStringFA(_zenkaku, _format, ap);
	va_end(ap);
}	//PosPrintStringFA

/**
* @brief
* 位置指定＆書式指定付文字列描画（Unicode文字用）
*
* @param	int _xp	座標Ｘ指定（１オリジン）
* @param	int _yp	座標Ｙ指定（１オリジン）
* @param	bool _zenkaku	trueを指定すると、全ての文字(空白' 'も含む)を全角に変換して表示する。
* @param	const wchar_t* _format	書式指定文字列（Unicode文字）
* @param	...	任意の可変長引数\n
*
* @note
* ※_zenkaku以降はprintf()と同じ仕様。
*/
void PosPrintStringFW(int _xp, int _yp, bool _zenkaku, const wchar_t* _format, ...)
{
	SetCursorPosition(_xp, _yp);
	va_list ap;
	va_start(ap, _format);
	VPrintStringFW(_zenkaku, _format, ap);
	va_end(ap);
}	//PosPrintStringFW

/**
* @brief	座標指定（画面左上隅が(0,0)座標）＋書式指定付文字列描画（マルチバイト文字用）
*
* @param	int _x	表示Ｘ座標（０オリジン）
* @param	int _y	表示Ｙ座標（０オリジン）
* @param	bool _zenkaku	trueを指定すると、全ての文字(空白' 'も含む)を全角に変換して表示する。
* @param	const char* _format	書式指定文字列（マルチバイト文字）
* @param	...	可変長引数
*/
void DrawStringFA(int _x, int _y, bool _zenkaku, const char* _format, ...)
{
#ifdef USED2D
	g_CursorPosD2D.X = _x;
	g_CursorPosD2D.Y = _y;
#else
	SetConsoleCursorPosition(g_DisplayHandle[g_SwapFlg], COORD{ (SHORT)_x,(SHORT)(g_ScreenBufferInfoEx.srWindow.Top + _y) });
#endif // USED2D
	va_list ap;
	va_start(ap, _format);
	VPrintStringFA(_zenkaku, _format, ap);
	va_end(ap);
}	//DrawStringFA

/**
* @brief	座標指定（画面左上隅が(0,0)座標）＋書式指定付文字列描画（Unicode文字用）
*
* @param	int _x	表示Ｘ座標（０オリジン）
* @param	int _y	表示Ｙ座標（０オリジン）
* @param	bool _zenkaku	trueを指定すると、全ての文字(空白' 'も含む)を全角に変換して表示する。
* @param	const wchar_t* _format	書式指定文字列（Unicode文字）
* @param	...	可変長引き数
*/
void DrawStringFW(int _x, int _y, bool _zenkaku, const wchar_t* _format, ...)
{
#ifdef USED2D
	g_CursorPosD2D.X = _x;
	g_CursorPosD2D.Y = _y;
#else
	SetConsoleCursorPosition(g_DisplayHandle[g_SwapFlg], COORD{ (SHORT)_x,(SHORT)(g_ScreenBufferInfoEx.srWindow.Top + _y) });
#endif // USED2D
	va_list ap;
	va_start(ap, _format);
	VPrintStringFW(_zenkaku, _format, ap);
	va_end(ap);
}	//DrawStringFW

//================================================================
// パレット関係
//================================================================
/**
* @brief	RGBQUAD:{R,G,B,0}型をCOLORREF:0x00BBGGRR型に変換
*
* @param	RGBQUAD rgb	RGBQUAD色
*/
COLORREF RGBQtoCREF(RGBQUAD rgb)
{
	return (rgb.rgbRed & 0x0000FF) | ((rgb.rgbGreen << 8) & 0x00FF00) | ((rgb.rgbBlue << 16) & 0xFF0000);
}	//RGBQtoCREF

/**
* @brief	COLORREF:0x00BBGGRR型をRGBQUAD:{R,G,B,0}型に変換
*
* @param	COLORREF ref	COLORREF色
*/
RGBQUAD CREFtoRGBQ(COLORREF ref)
{
	RGBQUAD rgb = { (BYTE)((ref & 0x00FF0000) >> 16)/*Blue*/,(BYTE)((ref & 0x0000FF00) >> 8)/*Green*/,(BYTE)(ref & 0x000000FF)/*Red*/,0x00/*Reserved*/ };
	return rgb;
}	//CREFtoRGBQ

/**
* @brief	パレット変換：RGBQ[16] -> COLORREF[16]
*
* @param	const RGBQUAD* _rgb	変換元１６色
* @param	COLORREF* _cref	変換先１６色
*/
COLORREF* ConvRGBQtoCREF(const RGBQUAD* _rgb, COLORREF* _cref)
{
	for (int n = 0; n < NUM_PALETTE; n++) {
		_cref[n] = RGBQtoCREF(_rgb[n]);
	}
	return _cref;
}

/**
* @brief	パレット変換：RGBQ[16] -> COLORREF[16]
*
* @param	const COLORREF* _cref	変換元１６色
* @param	RGBQUAD* _rgb	変換先１６色
*/
RGBQUAD* ConvCREFtoRGBQ(const COLORREF* _cref, RGBQUAD* _rgb)
{
	for (int n = 0; n < NUM_PALETTE; n++) {
		_rgb[n] = CREFtoRGBQ(_cref[n]);
	}
	return _rgb;
}

/**
* @brief	パレットの設定
*
* @param	COLORREF* _pal16：パレットデータへのポインタ
* @param	int  _p1：セットしたいパレットの開始番号
* @param	int  _p2：セットしたいパレットの終了番号
*/
void SetPalette(const COLORREF* _pal16, int _p1, int _p2)
{
	if (_pal16 == NULL) {
		//パレット無し
		return;
	}
#ifdef USED2D
	//値の補正
	if (_p1 < 0) { _p1 = 0; }
	if (_p2 < 0) { _p2 = 0; }
	if (_p1 >= NUM_D2D_PAL) { _p1 = NUM_D2D_PAL - 1; }
	if (_p2 >= NUM_D2D_PAL) { _p2 = NUM_D2D_PAL - 1; }
	//_p1 <= _p2にする
	if (_p1 > _p2) {
		int t = _p1;
		_p1 = _p2;
		_p2 = t;
	}
	for (int n = _p1; n <= _p2; n++) {
		g_PaletteD2D[n].rgbBlue = (_pal16[n] & 0x00FF0000) >> 16;	//00BBGGRR
		g_PaletteD2D[n].rgbGreen = (_pal16[n] & 0x0000FF00) >> 8;	//00BBGGRR
		g_PaletteD2D[n].rgbRed = (_pal16[n] & 0x000000FF);	//00BBGGRR
		g_PaletteD2D[n].rgbReserved = 0;	//00BBGGRR
	}
#else
	//値の補正
	if (_p1 < 0) { _p1 = 0; }
	if (_p2 < 0) { _p2 = 0; }
	if (_p1 >= NUM_PALETTE) { _p1 = NUM_PALETTE - 1; }
	if (_p2 >= NUM_PALETTE) { _p2 = NUM_PALETTE - 1; }
	//_p1 <= _p2にする
	if (_p1 > _p2) {
		int t = _p1;
		_p1 = _p2;
		_p2 = t;
	}
	//構造体の設定
	g_ScreenBufferInfoEx.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
	//コンソールのパレット読み込む
	GetConsoleScreenBufferInfoEx(g_DisplayHandle[0], &g_ScreenBufferInfoEx);
	//windowサイズをリセットしないと、少しづつ変化する？
	//g_ScreenBufferInfoEx.srWindow.Right = g_ScreenBufferInfoEx.dwSize.X;
	//g_ScreenBufferInfoEx.srWindow.Bottom = g_ScreenBufferInfoEx.dwSize.Y;
	g_ScreenBufferInfoEx.srWindow.Right = g_ScreenBufferInfoEx.dwMaximumWindowSize.X;
	g_ScreenBufferInfoEx.srWindow.Bottom = g_ScreenBufferInfoEx.dwMaximumWindowSize.Y;
	//パレット(COLORREF[])を転送する
	for (int n = _p1; n <= _p2; n++) {
		g_ScreenBufferInfoEx.ColorTable[n] = _pal16[n];
	}
	//コンソールにパレットを反映する
	if (g_DisplayHandle[0] != NULL) {
		SetConsoleScreenBufferInfoEx(g_DisplayHandle[0], &g_ScreenBufferInfoEx);
	}
	if (g_DisplayHandle[1] != NULL) {
		SetConsoleScreenBufferInfoEx(g_DisplayHandle[1], &g_ScreenBufferInfoEx);
	}
#endif // USED2D
}	//SetPalette

/**
* @brief	Bmpからパレット１６色の設定
*
* @param	Bmp* _pBmp：パレットデータへのポインタ
*/
void SetPalette(const Bmp* _pBmp)
{
	if (_pBmp->pal == NULL) {
		return;
	}
	if (_pBmp->numpal <= 0) {
		return;
	}
	SetPalette(_pBmp->pal, 0, _pBmp->numpal - 1);
}

//================================================================
// フレームバッファ画像描画
//================================================================
#ifndef USED2D
/**
* @brief	スクリーンバッファの初期化
*
* @param	const char _code:初期値※全ての要素がこの値で初期化される
*
* @note		転送元となる16色スクリーンバッファを指定値で埋める
*/
void ClearScreenBuffer(const char _code)
{
	//memset(g_ScreenBuffer4bit, _code, g_ScreenBufferSize.X * g_ScreenBufferSize.Y * sizeof(CHAR_INFO));
	memset(g_ScreenBuffer4bit, _code, g_ScreenBufferSize.X * g_ScreenBufferSize.Y * sizeof(WORD));
}	//ClearScreenBuffer

/**
* @brief	一括転送用バッファクリア
*
* @param	buf [入力] スクリーンバッファのポインタ
*
* @note
* スクリーンバッファはウィンドウサイズの横幅×縦幅の\n
* バイトサイズ以上のchar型配列とする\n
* スクリーンバッファの内容を全て0でクリアする
*/
void ClearFrameBuffer(char* buf)
{
	ZeroMemory(buf, sizeof(char) * g_ScreenBufferSize.X * g_ScreenBufferSize.Y);
}	//ClearFrameBuffer

/**
* @brief	一括転送用バッファクリア
*
* @param	buf [入力] スクリーンバッファのポインタ
*
* @note
* スクリーンバッファはウィンドウサイズの横幅×縦幅×24bitの\n
* バイトサイズ以上のchar型配列とする\n
* スクリーンバッファの内容を全て0でクリアする
*/
void ClearFrameBufferFull(char* buf)
{
	ZeroMemory(buf, sizeof(char) * 3 * g_ScreenBufferSize.X * g_ScreenBufferSize.Y);
}	//ClearFrameBuffer

/**
* @brief	色情報の一括転送\n
*	色情報（背景と文字色）を書き換える事で、１文字(char)を１ピクセルの様に扱う。\n
*	char値は１６色パレットのパレット番号（0x0F～0x0F）\n
*	★フォントサイズを小さくして画像表示に使う\n
*	★フォントサイズを小さくするので文字表示がほぼ出来なくなる\n
*
* @param	buf [入力] スクリーンバッファのポインタ
* 【注】InitConioで指定したスクリーンサイズの面積(タテ×ヨコ)と同じサイズの１次元配列でないといけない。
*/
void PrintFrameBuffer(const char* _buf_8bit)
{
	DWORD size = g_ScreenBufferSize.X * g_ScreenBufferSize.Y;
	DWORD num;
	WORD* dp = g_ScreenBuffer4bit;

	// 画像描画(画面外へのはみ出し処理なし)
	for (int i = 0; i < size; i++) {
		*dp = ((*_buf_8bit) << 4) | ((*_buf_8bit) & 0x0F);
		dp++;
		_buf_8bit++;
	}
	//属性のみ書き換える
	WriteConsoleOutputAttribute(g_DisplayHandle[g_SwapFlg], g_ScreenBuffer4bit, size, { 0,0 }, &num);
}	//PrintFrameBuffer

/**
* @brief	点を打つ
*
* @param	int _x,_y：座標
* @param	int _c：色（パレット番号０～１５）
* @param	int _tr：透明処理（true:色：０なら描きこまない/false：色：０を書き込む）
*/
void DrawPixel(int _x, int _y, int _c)
{
	if ((_x >= 0) && (_y >= 0) && (_y < g_ScreenBufferSize.Y) && (_x < g_ScreenBufferSize.X)) {
		//範囲内のみ処理する
		//g_ScreenBuffer4bit[_y * g_ScreenBufferSize.X + _x].Attributes = (_c & 0x0F) | ((_c << 4) & 0xF0);
		////転送開始位置がマイナス値の時は"dest_rect"全体が転送されなくなるので、０にしておく。
		//SMALL_RECT dest_rect = { (SHORT)_x, (SHORT)_y, (SHORT)(_x), (SHORT)(_y) };
		//WriteConsoleOutputA(g_DisplayHandle[g_SwapFlg], g_ScreenBuffer4bit, g_ScreenBufferSize, { (SHORT)_x,(SHORT)_y }, &dest_rect);
		g_FrameBuffer4bit[_y * g_ScreenBufferSize.X + _x] = _c;
	}
}	//DrawPixel
#endif // USED2D

//----------------------------------------------------------------
// BMP画像の描画(LoadBmp/CreateBmpChar/CreateBmpStringで生成したBmp画像)
//----------------------------------------------------------------
/**
* @brief	Bmp(4ビット/Pixel)画像の出力
*
* @param	int _xp	[入力] 表示座標（０～）
* @param	int _yp [入力] 表示座標（０～）
* @param	Bmp _bmp [入力] Bmp構造体へのポインタ
* @param	int _inv：反転フラグ：0=反転無し/BMP_HINV=水平反転/BMP_VINV=垂直反転/BMP_HVINV=水平垂直反転(１８０°回転)
* @param	bool _tr：透明フラグ：true=透明/false=不透明　--- '０'の部分を描きこまないことで透明処理をする。
*
*/
#ifdef USED2D
//static COLORREF* pal_ptr = NULL;	//[256] = {};
//static RGBQUAD* pal_rgb_ptr = NULL;	//[256] = {};
static const Bmp* bmp_ptr = NULL;
inline void pixel_copy04(const BYTE* buf, int xx, int yy, bool _tr) {
	if (bmp_ptr->pal != NULL) {
		if ((xx >= 0) && (yy >= 0) && (yy < g_ScreenBufferSize.Y) && (xx < g_ScreenBufferSize.X)) {
			//画面内である
			if ((!_tr) || (*buf != 0)) {
				//透明指定が無しor透明ピクセルでは無い
				const char* src = (char*)&bmp_ptr->pal[*buf % bmp_ptr->numpal];
				char* dest = (char*)&g_FrameBuffer32bitD2D[yy * g_ScreenBufferSize.X + xx];
				//RGBQUAD <= COLORREF
				dest[0] = src[2];
				dest[1] = src[1];
				dest[2] = src[0];
				dest[3] = src[3];
			}
		}
	}
	else if (bmp_ptr->pal_rgb != NULL) {
		if ((xx >= 0) && (yy >= 0) && (yy < g_ScreenBufferSize.Y) && (xx < g_ScreenBufferSize.X)) {
			//画面内である
			if ((!_tr) || (*buf != 0)) {
				//透明指定が無しor透明ピクセルでは無い
				//RGBQUAD <= RGBQUAD
				g_FrameBuffer32bitD2D[yy * g_ScreenBufferSize.X + xx] = bmp_ptr->pal_rgb[*buf % bmp_ptr->numpal];
			}
		}
	}
	else {
		if ((xx >= 0) && (yy >= 0) && (yy < g_ScreenBufferSize.Y) && (xx < g_ScreenBufferSize.X)) {
			//画面内である
			if ((!_tr) || (*buf != 0)) {
				//透明指定が無しor透明ピクセルでは無い
				//RGBQUAD <= COLORREF
				g_FrameBuffer32bitD2D[yy * g_ScreenBufferSize.X + xx] = g_PaletteD2D[*buf % bmp_ptr->numpal];
			}
		}
	}
}
#else
inline void pixel_copy04(const unsigned char* buf, int xx, int yy, bool _tr) {
	if ((xx >= 0) && (yy >= 0) && (yy < g_ScreenBufferSize.Y) && (xx < g_ScreenBufferSize.X)) {
		//画面内である
		if ((!_tr) || (*buf != 0)) {
			//透明指定が無しor透明ピクセルでは無い
			g_FrameBuffer4bit[yy * g_ScreenBufferSize.X + xx] = *buf;
		}
	}
}
#endif // USED2D
static void draw_bmp_bpp04(int _xp, int _yp, const Bmp* _bmp, int _inv, bool _tr)
{
#ifdef USED2D
	bmp_ptr = _bmp;
#endif // USED2D

	//フレームバッファの指定座標へ転送
	unsigned char* buf = (unsigned char*)_bmp->pixel;
	if (_inv == 0) {
		//反転回転無し（頭が上・鼻が左）
		for (int y = 0; y < _bmp->height; y++) {
			for (int x = 0; x < _bmp->width; x++) {
				pixel_copy04(buf, x + _xp, y + _yp, _tr);
				buf++;
			}
		}
	}
	else if (_inv == BMP_HINV) {
		//水平反転のみ（頭が上・鼻が右）
		for (int y = 0; y < _bmp->height; y++) {
			for (int x = _bmp->width - 1; x >= 0; x--) {
				pixel_copy04(buf, x + _xp, y + _yp, _tr);
				buf++;
			}
		}
	}
	else if (_inv == BMP_VINV) {
		//垂直反転のみ（頭が下・鼻が右）
		for (int y = _bmp->height - 1; y >= 0; y--) {
			for (int x = 0; x < _bmp->width; x++) {
				pixel_copy04(buf, x + _xp, y + _yp, _tr);
				buf++;
			}
		}
	}
	else if (_inv == BMP_HVINV) {
		//水平＋垂直反転（頭が下・鼻が左）（１８０度回転）
		for (int y = _bmp->height - 1; y >= 0; y--) {
			for (int x = _bmp->width - 1; x >= 0; x--) {
				pixel_copy04(buf, x + _xp, y + _yp, _tr);
				buf++;
			}
		}
	}
	else if (_inv & BMP_ROT90) {
		//横倒しになっている場合
		_inv &= (~BMP_ROT90);	//横倒しフラグは消す
		if (_inv == 0) {
			//横倒しのみ（頭が右・鼻が下）■▼
			for (int x = _bmp->height - 1; x >= 0; x--) {
				for (int y = 0; y < _bmp->width; y++) {
					pixel_copy04(buf, x + _xp, y + _yp, _tr);
					buf++;
				}
			}
		}
		else if (_inv == BMP_HINV) {
			//横倒し＋水平反転（頭が左・鼻が下）▼■
			for (int x = 0; x < _bmp->height; x++) {
				for (int y = 0; y < _bmp->width; y++) {
					pixel_copy04(buf, x + _xp, y + _yp, _tr);
					buf++;
				}
			}
		}
		else if (_inv == BMP_VINV) {
			//横倒し＋垂直反転（頭が右・鼻が上）■▲
			for (int x = _bmp->height - 1; x >= 0; x--) {
				for (int y = _bmp->width - 1; y >= 0; y--) {
					pixel_copy04(buf, x + _xp, y + _yp, _tr);
					buf++;
				}
			}
		}
		else if (_inv == BMP_HVINV) {
			//横倒し＋水平＋垂直反転（頭が左・鼻が上）▲■
			for (int x = 0; x < _bmp->height; x++) {
				for (int y = _bmp->width - 1; y >= 0; y--) {
					pixel_copy04(buf, x + _xp, y + _yp, _tr);
					buf++;
				}
			}
		}
	}
#ifdef USED2D
	bmp_ptr = NULL;
#endif // USED2D
}	//draw_bmp_bpp04

/**
* @brief	24ビット/Pixel画像の出力
*
* @param	int _xp	[入力] 表示座標（０～）
* @param	int _yp [入力] 表示座標（０～）
* @param	Bmp _bmp [入力] Bmp構造体へのポインタ
* @param	int _inv：反転フラグ：0=反転無し/BMP_HINV=水平反転/BMP_VINV=垂直反転/BMP_HVINV=水平垂直反転(１８０°回転)
* @param	bool _tr：透明フラグ：true=透明/false=不透明　--- '０'の部分を描きこまないことで透明処理をする。
*
*/
#ifdef USED2D
inline void pixel_copy24(const unsigned char* in_buf, int xx, int yy, int, bool _tr) {
	if ((xx >= 0) && (yy >= 0) && (yy < g_ScreenBufferSize.Y) && (xx < g_ScreenBufferSize.X)) {
		DWORD rgb = *((DWORD*)in_buf);
		//画面内である
		if ((!_tr) || ((rgb & 0x00FFFFFF) != 0)) {
			//透明指定が無しor透明ピクセルでは無い
			DWORD* dest = (DWORD*)&g_FrameBuffer32bitD2D[yy * g_ScreenBufferSize.X + xx];
			*dest = rgb;
		}
	}
	//in_buf += 3;
}
#else
inline void pixel_copy24(const unsigned char* in_buf, int xx, int yy, int x_stride, bool _tr) {
	if ((xx >= 0) && (yy >= 0) && (yy < g_ScreenBufferSize.Y) && (xx < g_ScreenBufferSize.X)) {
		//画面内である
		if ((!_tr) || ((*((DWORD*)in_buf) & 0x00FFFFFF) != 0)) {
			//透明指定が無しor透明ピクセルでは無い
			char* dest = &g_FrameBufferFull[yy * x_stride + (xx * 3)];
			dest[2] = in_buf[0];
			dest[1] = in_buf[1];
			dest[0] = in_buf[2];
		}
	}
	//in_buf += 3;
}
#endif // USED2D
static void draw_bmp_bpp24(int _xp, int _yp, const Bmp* _bmp, int _inv, bool _tr)
{
	const unsigned char* in_buf = (const unsigned char*)_bmp->pixel;	//CharRGBconvTBL[][]のindexとして扱うので、符号無しにしている。
#ifdef USED2D
	if ((in_buf == NULL) || (g_FrameBuffer32bitD2D == NULL)) {
		return;
	}
#else
	if ((in_buf == NULL) || (g_FrameBufferFull == NULL)) {
		return;
	}
#endif // USED2D
	int x_stride = (g_ScreenBufferSize.X * 3);
	if (_inv == 0) {
		//反転回転無し（頭が上・鼻が左）
		for (int y = 0; y < _bmp->height; y++) {
			for (int x = 0; x < _bmp->width; x++) {
				pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
				in_buf += 3;
			}
		}
	}
	else if (_inv == BMP_HINV) {
		//水平反転のみ（頭が上・鼻が右）
		for (int y = 0; y < _bmp->height; y++) {
			for (int x = _bmp->width - 1; x >= 0; x--) {
				pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
				in_buf += 3;
			}
		}
	}
	else if (_inv == BMP_VINV) {
		//垂直反転のみ（頭が下・鼻が右）
		for (int y = _bmp->height - 1; y >= 0; y--) {
			for (int x = 0; x < _bmp->width; x++) {
				pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
				in_buf += 3;
			}
		}
	}
	else if (_inv == BMP_HVINV) {
		//水平＋垂直反転（頭が下・鼻が左）（１８０度回転）
		for (int y = _bmp->height - 1; y >= 0; y--) {
			for (int x = _bmp->width - 1; x >= 0; x--) {
				pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
				in_buf += 3;
			}
		}
	}
	else if (_inv & BMP_ROT90) {
		//横倒しになっている場合
		_inv &= (~BMP_ROT90);	//横倒しフラグは消す
		if (_inv == 0) {
			//横倒しのみ（頭が右・鼻が下）■▼
			for (int x = _bmp->height - 1; x >= 0; x--) {
				for (int y = 0; y < _bmp->width; y++) {
					pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
					in_buf += 3;
				}
			}
		}
		else if (_inv == BMP_HINV) {
			//横倒し＋水平反転（頭が左・鼻が下）▼■
			for (int x = 0; x < _bmp->height; x++) {
				for (int y = 0; y < _bmp->width; y++) {
					pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
					in_buf += 3;
				}
			}
		}
		else if (_inv == BMP_VINV) {
			//横倒し＋垂直反転（頭が右・鼻が上）■▲
			for (int x = _bmp->height - 1; x >= 0; x--) {
				for (int y = _bmp->width - 1; y >= 0; y--) {
					pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
					in_buf += 3;
				}
			}
		}
		else if (_inv == BMP_HVINV) {
			//横倒し＋水平＋垂直反転（頭が左・鼻が上）▲■
			for (int x = 0; x < _bmp->height; x++) {
				for (int y = _bmp->width - 1; y >= 0; y--) {
					pixel_copy24(in_buf, x + _xp, y + _yp, x_stride, _tr);
					in_buf += 3;
				}
			}
		}
	}
}	//draw_bmp_bpp24

/**
* @brief	画像の出力
*
* @param	int _xp：表示座標（０～）
* @param	int _yp：表示座標（０～）
* @param	Bmp _bmp：Bmp構造体へのポインタ
* @param	int _inv：反転フラグ：0=反転無し/BMP_HINV=水平反転/BMP_VINV=垂直反転/BMP_HVINV=水平垂直反転(１８０°回転)
* @param	bool _tr：透明フラグ：true=透明/false=不透明　--- '０'の部分を描きこまないことで透明処理をする。
*
* @note		Bmp画像の色数を自動判定して描画している
*/
void DrawBmp(int _xp, int _yp, const Bmp* _bmp, int _inv, bool _tr)
{
	_ASSERT(_bmp);
	if (_bmp == NULL) { return; }
	if (_bmp->colbit == 4) {
		draw_bmp_bpp04(_xp, _yp, _bmp, _inv, _tr);
	}
	else if (_bmp->colbit == 8) {
		draw_bmp_bpp04(_xp, _yp, _bmp, _inv, _tr);
	}
	else if (_bmp->colbit == 24) {
		draw_bmp_bpp24(_xp, _yp, _bmp, _inv, _tr);
	}
}	//DrawBmp
/**
* @brief	画像の出力
*
* @param	int _xp：表示座標（０～）
* @param	int _yp：表示座標（０～）
* @param	Bmp _bmp：Bmp構造体へのポインタ
* @param	bool _tr：透明フラグ：true=透明/false=不透明　--- '０'の部分を描きこまないことで透明処理をする。
*
* @note		Bmp画像の色数を自動判定して描画している
*/
void DrawBmp(int _xp, int _yp, const Bmp* _bmp, bool _tr)
{
	DrawBmp(_xp, _yp, _bmp, 0, _tr);
}

//================================================================
// ビットマップファイル操作ユーティリティー
//================================================================
//--------------------------------
//画像のビット解像度別変換関数。
//----------------------------------------------------------------
/**
* @brief	１ビット／ピクセルの１ライン処理関数
*
* @note
*	１ピクセルの値は０～１のパレット番号
*
* @param	int w_pixels	出力ピクセル数
* @param	PBYTE dst_pxbuf		出力バッファ
* @param	int stride	入力ピクセル数
* @param	PBYTE src_ppx	入力バッファ
*
* @return	なし
*/
static void print_line_1bpp(int w_pixels, PBYTE dst_pxbuf, int stride, const PBYTE src_ppx)
{
	for (int x = 0; x < stride; x++) {
		for (int mask8 = 0b10000000; mask8 != 0; mask8 >>= 1) {
			*dst_pxbuf = (src_ppx[x] & mask8) ? 0x01 : 0x00;
			dst_pxbuf++;
			w_pixels--;
			if (w_pixels <= 0) {
				return;
			}
		}
	}
}	//print_line_1bpp

/**
* @brief	４ビット／ピクセルの１ライン処理関数
*
* @note
*	１ピクセルの値は０～１５のパレット番号
*
* @param	int w_pixels	出力ピクセル数
* @param	PBYTE dst_pxbuf		出力バッファ
* @param	int stride	入力ピクセル数
* @param	PBYTE src_ppx	入力バッファ
*
* @return	なし
*/
static void print_line_4bpp(int w_pixels, PBYTE dst_pxbuf, int stride, const PBYTE src_ppx)
{
	for (int x = 0; x < stride; x++) {
		*dst_pxbuf = (src_ppx[x] >> 4) & 0x0F;
		dst_pxbuf++;
		w_pixels--;
		if (w_pixels <= 0) {
			return;
		}
		*dst_pxbuf = src_ppx[x] & 0x0F;
		dst_pxbuf++;
		w_pixels--;
		if (w_pixels <= 0) {
			return;
		}
	}
}	//print_line_4bpp

/**
* @brief	８ビット(1byte)／ピクセルの１ライン処理関数
*
* @note
*	１ピクセルの値は０～２５５のパレット番号
*
* @param	int w_pixels	出力ピクセル数
* @param	PBYTE dst_pxbuf		出力バッファ
* @param	int stride	入力ピクセル数
* @param	PBYTE src_ppx	入力バッファ
*
* @return	なし
*/
static void print_line_8bpp(int w_pixels, PBYTE dst_pxbuf, int stride, const PBYTE src_ppx)
{
	for (int x = 0; x < w_pixels; x++) {
		*dst_pxbuf = src_ppx[x];
		dst_pxbuf++;
	}
}	//print_line_8bpp

/**
* @brief	１６ビット(2byte)／ピクセルの１ライン処理関数
*
* @note
*	１ピクセルの値は１６ビットのＲＧＢ値\n
*	16bpp\n
*	16bit / 1pixel\n
*	WORD{0b0rrrrrgggggbbbbb}\n
*	RGB値に変換する場合は・・・\n
*	(BYTE)[0x07],[0x29] -> (WORD)0x2907 -> 0b0010_1001_0000_0111 -> 0b0_01010_01000_00111 -> 0A,08,07\n
*	-> (0A*FF)/1F,(08*FF)/1F,(07*FF)/1F -> RGB(52,41,39)
*
* @param	int w_pixels	出力ピクセル数
* @param	PBYTE dst_pxbuf		出力バッファ
* @param	int stride	入力ピクセル数
* @param	PBYTE src_ppx	入力バッファ
*
* @return	なし
*/
static void print_line_16bpp(int w_pixels, PBYTE dst_pxbuf, int stride, const PBYTE src_ppx)
{
	WORD* dwpx = (WORD*)dst_pxbuf;
	WORD* swp = (WORD*)src_ppx;
	for (int x = 0; x < w_pixels; x++) {
		*dwpx = swp[x];
		dwpx++;
	}
}	//print_line_16bpp

/**
* @brief	２４ビット(3byte)／ピクセルの１ライン処理関数
*
* @note
*	１ピクセルの値は２４ビットのＲＧＢ値\n
*	24bpp\n
*	24bit / 1pixel\n
*	BYTE[BB],[GG],[RR],[??] -> DWORD{0x??RRGGBB & 0x00FFFFFF} -> (DWORD)0x00RRGGBB -> 0xRRGGBB
*
* @param	int w_pixels	出力ピクセル数
* @param	PBYTE dst_pxbuf		出力バッファ
* @param	int stride	入力ピクセル数
* @param	PBYTE src_ppx	入力バッファ
*
* @return	なし
*/
static void print_line_24bpp(int w_pixels, PBYTE dst_pxbuf, int stride, const PBYTE src_ppx)
{
	PBYTE src = src_ppx;
	for (int x = 0; x < w_pixels; x++, src += 3) {
		dst_pxbuf[0] = src[0];	//BB
		dst_pxbuf[1] = src[1];	//GG
		dst_pxbuf[2] = src[2];	//RR
		dst_pxbuf += 3;
	}
}	//print_line_24bpp

/**
* @brief	３２ビット(4byte)／ピクセル画像の１ライン処理関数
*
* @note
*	１ピクセルの値は３２ビットのＲＧＢ値\n
*	32bpp\n
*	32bit / 1pixel\n
*	(DWORD)0xaaRRGGBB
*
* @param	int w_pixels	出力ピクセル数
* @param	PBYTE dst_pxbuf		出力バッファ
* @param	int stride	入力ピクセル数
* @param	PBYTE src_ppx	入力バッファ
*
* @return	なし
*/
static void print_line_32bpp(int w_pixels, PBYTE dst_pxbuf, int stride, const PBYTE src_ppx)
{
	DWORD* ddwpx = (DWORD*)dst_pxbuf;
	DWORD* sdwp = (DWORD*)src_ppx;
	for (int x = 0; x < w_pixels; x++) {
		*ddwpx = sdwp[x];
		ddwpx++;
	}
}	//print_line_32bpp

/**
* @brief	解像度別表示関数のポインタテーブル（配列）
*
* @note	void (*fp)();	//関数ポインタ\n
* fp();	//呼出\n
* void (*fpp[])();	//関数ポインタ配列\n
* fpp[n]();	//呼出\n
*/
void(*print_line[])(int, PBYTE, int, const PBYTE) = {
	print_line_1bpp,	/**１ビット／ピクセル*/
	print_line_4bpp,	/**４ビット／ピクセル*/
	print_line_8bpp,	/**８ビット(1byte)／ピクセル*/
	print_line_16bpp,	/**１６ビット(2byte)／ピクセル*/
	print_line_24bpp,	/**２４ビット(3byte)／ピクセル*/
	print_line_32bpp	/**３２ビット(4byte)／ピクセル*/
};

/**
* @brief	関数配列インデックス用列挙子
*/
enum FUNC_NUM {
	BPP_1 = 0,	/**１ビット／ピクセル*/
	BPP_4,		/**４ビット／ピクセル*/
	BPP_8,		/**８ビット(1byte)／ピクセル*/
	BPP_16,		/**１６ビット(2byte)／ピクセル*/
	BPP_24,		/**２４ビット(3byte)／ピクセル*/
	BPP_32		/**３２ビット(4byte)／ピクセル*/
};

/**
* @brief	BMPファイルの読み込み。
*
* @note	パレットの型について：
*	COLORREF型をメモリ上の並びを
*		BYTE[]で読み取ると{[0]Red,[1]Green,[2]Blue,[3]0}となり
*		DWORD型で読み取ると{0x00BBGGRR}
*	となる。
*	RGBQUAD型をメモリ上の並びを
*		BYTE[]で読み取ると{[0]Blue,[1]Green,[2]Red,[3]0}となり
*		DWORD型で読み取ると{0x00RRGGBB}
*	となる。
*
* @note	24/32ピクセルのデータの並びについて：
*	BITMAPのピクセルデータ{24bit/pixel|32bit/pixel}は,
*		24bit/pixelの場合BYTE[3]{Blue,Green,Red}/DWORD{0x**RRGGBB}
*		32bit/pixelの場合BYTE[4]{Blue,Green,Blue,alpha?}/DWORD{0xaaRRGGBB}
*	となっている。
*
* @param	const char* _file_name		BMPファイル名（パス）
* @param	bool _palset_or_swap24RB	パレット色画像データの場合のパレット設定(true=する/false=しない)
*										24bitフルカラー画像の場合にRedとBlueを交換(true=する/false=しない)
*
* @return	Bmp*	正常に読み込めた場合はBmp構造体のポインタを返す。\n
*	【注】正常に返されたポインタは、使い終わったら必ずDeleteBmp(Bmp*)を呼び出して削除する事。
*/
Bmp* LoadBmp(const char* _file_name, bool _palset_or_swap24RB)
{
	//Bmp4構造体のメモリ領域を確保
	Bmp* pb = (Bmp*)calloc(1, sizeof(Bmp));	//callocで確保しているので領域は０クリア済み(^_^）
	if (!pb) {
		//NULLポインタ（値０）が返ってきたら失敗(;_;)
		return pb;	//Bmp4構造体の確保失敗
	}
	//-------------------------------------------------------------
	//BMPファイルを開く（ファイルオープン）
	FILE* fp = fopen(_file_name, "rb");	//リード・バイナリモード。
	if (!fp) {
		DeleteBmp(&pb);
		return (Bmp*)NULL;
	}
	BITMAPFILEHEADER bfh;
	//BITMAPFILEHEADER(14byte)の部分を読み込む。
	//	WORD	bfType;			//0x4D42(文字コード'B'と'M'が入っている。WORD（16ビット値）で見ると)
	//	DWORD	bfSize;			//BMPファイルのバイト数(131190)
	//	WORD	bfReserved1;	//予約（未使用部分）
	//	WORD	bfReserved2;	//予約（未使用部分）
	//	DWORD	bfOffBits;		//ピクセル部分までのバイト数
	size_t er = fread(&bfh, sizeof(BYTE), sizeof(BITMAPFILEHEADER), fp);
	if (!er) {
		DeleteBmp(&pb);
		fclose(fp);	//ファイル閉じる
		//読み込み失敗(;_;)
		return (Bmp*)NULL;
	}
	BITMAPINFOHEADER bih;
	//BITMAPINFOHEADER(40byte)部分を読み込む
	//以下のメンバ変数の詳細は：MSのドキュメント<"https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader">参照
	//	DWORD	   biSize;			//この構造体（BITMAPINFOHEADER）のサイズ（バイト数）
	//	LONG	   biWidth;			//この画像の幅（ピクセル数）
	//	LONG	   biHeight;		//この画像の高さ（ピクセル数）※プラス（＋）値なら「ボトムアップ（左下隅から）」マイナス（－）値なら「トップダウン（左上隅から）」でピクセルが並んでいる
	//	WORD	   biPlanes;		//常に１
	//	WORD	   biBitCount;		//１ピクセルあたりのビット数（bpp)
	//	DWORD	   biCompression;	//※bmp_utlでは圧縮形式は扱っていないので、非圧縮形式のBI_RGBだけ扱っている（BI_BITFIELDSもあるが一般的に使われていないので非対応）
	//	DWORD	   biSizeImage;		//非圧縮RGBビットマップの場合は0に出来るので、値が入っていても参照しない。biSizeImage の正しい値は biWidth，biHeight，biBitCount から計算できる
	//	LONG	   biXPelsPerMeter;	//水平解像度：単位は1m当たりのがぞ数（画素数/ｍ）※０の場合もあるので参照しない
	//	LONG	   biYPelsPerMeter;	//垂直解像度：単位は1m当たりのがぞ数（画素数/ｍ）※０の場合もあるので参照しない
	//	DWORD	   biClrUsed;		//パレットの数（カラーテーブル数）：０ならbiBitCountのビット数で表現できる最大値がテーブル数となる※詳細はMSのドキュメント参照
	//	DWORD	   biClrImportant;	//この画像を表示するのに必要な色数（パレット数）０なら全ての色が必要※bmp_utlではこの値は無視して全色必要としている
	er = fread(&bih, sizeof(BYTE), sizeof(BITMAPINFOHEADER), fp);
	if (!er) {
		DeleteBmp(&pb);
		fclose(fp);	//ファイル閉じる
		//読み込み失敗(;_;)
		return (Bmp*)NULL;
	}
	//パレット（カラーテーブル）が存在すればその領域を確保して読み込む。
	//パレットがあるかないかはbiBitCountを見て判断する
	pb->numpal = 0;	//判断前はパレット無し（０）にしておく
	pb->pal = (COLORREF*)NULL;	//判断前はパレット無し（NULL）にしておく
#ifdef USED2D
	pb->pal_rgb = (RGBQUAD*)NULL;	//判断前はパレット無し（NULL）にしておく
#endif // USED2D
	//１～８はパレット在り、それ以外は1ピクセル＝16ビット｜24ビット｜32ビット
	if (bih.biBitCount >= 1 && bih.biBitCount <= 8) {
		//１bit：２色、４bit：１６色、８bit：２５６色
		pb->numpal = (1 << bih.biBitCount);	//このビット数分左すれば必要な最大パレット数になる
		if (bih.biClrUsed > 0) {
			pb->numpal = bih.biClrUsed;	//biClrUsedに値が入っている場合は優先する。
		}
#ifndef USED2D
		//RGB値を表すRGBQUAD値の配列をパレット数の数だけ確保する
		//１色はDWORDでバイト並び順は[B][G][R][A]として格納されている
		//１色のデータをRGBQUAD構造体として読み込む※リトルエンディアンで格納されているのでDWORD型で読み込むと0xAARRGGBBとなる
		RGBQUAD* pal_rgb = (RGBQUAD*)calloc(pb->numpal, sizeof(RGBQUAD));
		if (!pal_rgb) {
			//NULLポインタ（値０）が返ってきたら失敗(;_;)
			DeleteBmp(&pb);
			fclose(fp);	//ファイル閉じる
			return (Bmp*)NULL;
		}
		//確保したサイズ分パレットデータ（RGBQUAD×パレット数）をファイルから読み込む。
		size_t er = fread(pal_rgb, pb->numpal, sizeof(RGBQUAD), fp);
		if (!er) {
			DeleteBmp(&pb);
			fclose(fp);	//ファイル閉じる
			return (Bmp*)NULL;
		}
		pb->pal = (COLORREF*)calloc(pb->numpal, sizeof(COLORREF));
		if (!pb->pal) {
			//NULLポインタ（値０）が返ってきたら失敗(;_;)
			free(pal_rgb);
			DeleteBmp(&pb);
			fclose(fp);	//ファイル閉じる
			return (Bmp*)NULL;
		}
		//RGBQUAD{B,G,B,0}:0x00RRGGBB型をCOLORREF:0x00BBGGRR型に変換
		for (int n = 0; n < pb->numpal; n++) {
			pb->pal[n] = (pal_rgb[n].rgbRed & 0x0000FF) | ((pal_rgb[n].rgbGreen << 8) & 0x00FF00) | ((pal_rgb[n].rgbBlue << 16) & 0xFF0000);
		}
#else
		//RGB値を表すRGBQUAD値の配列をパレット数の数だけ確保する
		//１色はDWORDでバイト並び順は[B][G][R][A]として格納されている
		//１色のデータをRGBQUAD構造体として読み込む※リトルエンディアンで格納されているのでDWORD型で読み込むと0xAARRGGBBとなる
		RGBQUAD* pal_rgb4 = (RGBQUAD*)_malloca(pb->numpal * sizeof(RGBQUAD));	//スタック上に確保（free不要）
		if (!pal_rgb4) {
			//NULLポインタ（値０）が返ってきたら失敗(;_;)
			DeleteBmp(&pb);
			fclose(fp);	//ファイル閉じる
			return (Bmp*)NULL;
		}
		//確保したサイズ分パレットデータ（RGBQUAD×パレット数）をファイルから読み込む。
		size_t er = fread(pal_rgb4, pb->numpal, sizeof(RGBQUAD), fp);
		if (!er) {
			DeleteBmp(&pb);
			fclose(fp);	//ファイル閉じる
			return (Bmp*)NULL;
		}
		//
		pb->pal = (COLORREF*)calloc(pb->numpal, sizeof(COLORREF));
		if (!pb->pal) {
			//NULLポインタ（値０）が返ってきたら失敗(;_;)
			DeleteBmp(&pb);
			fclose(fp);	//ファイル閉じる
			return (Bmp*)NULL;
		}
		//RGBQUAD{B,G,B,0}:0x00RRGGBB型をCOLORREF:0x00BBGGRR型に変換
		for (int n = 0; n < pb->numpal; n++) {
			pb->pal[n] = (pal_rgb4[n].rgbRed & 0x0000FF) | ((pal_rgb4[n].rgbGreen << 8) & 0x00FF00) | ((pal_rgb4[n].rgbBlue << 16) & 0xFF0000);
		}
#endif // USED2D
	}
	//残りはピクセル値なので、残りを全部読み込む
	//BITMAPFILEHEADER＋BITMAPINFOHEADER（＋パレット配列）部分を除く残りのサイズ分を全て読み込んでファイルを閉じる。
	DWORD pixel_data_size = (bfh.bfSize - bfh.bfOffBits);
	BYTE* pixel_data = (BYTE*)calloc(pixel_data_size, sizeof(BYTE));
	if (!pixel_data) {
		//NULLポインタ（値０）が返ってきたら失敗(;_;)
		DeleteBmp(&pb);
		fclose(fp);	//ファイル閉じる
		return (Bmp*)NULL;
	}
	//pfb->ppx_size = pixel_data_size;
	er = fread(pixel_data, sizeof(BYTE), pixel_data_size, fp);
	if (!er) {
		free(pixel_data);
		DeleteBmp(&pb);
		fclose(fp);	//ファイル閉じる
		return (Bmp*)NULL;
	}
	//ファイルの読み込みは終了したのでファイルは閉じておく。
	if (fp) {
		fclose(fp);
		fp = (FILE*)NULL;
	}
	//画像ピクセル部分を扱いやすいデータに変換：
	//	「１～４ビット／ピクセル」＝＞「１バイト／ピクセル」
	//	「８ビット／ピクセル」＝＞そのまま：１バイト／ピクセル
	//	「２４／ピクセル」＝＞そのまま：３バイト／ピクセル
	//	「３２／ピクセル」＝＞そのまま：４バイト／ピクセル
	PBYTE tmp_px_data = pixel_data;// + sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD)*pfb->pal_count);	//画像データの先頭にする。
	//(((biWidth * biBitCount + 31)& ~31)>>3) : ＋３１(1F)と＆~３１(E0)で０～３１ビットの値は切り上げて、>>3でビット数をバイト数にする。
	//	biWidth(必要な横ピクセル数) * biBitCount(１ピクセルのビット数) ⇒ 必要な横方向のビット数。
	//	＋31 ⇒ ４バイトの倍数にしたいので（８ビット×４バイト－１ビット：0x1F:0b00011111）を加算 ⇒ 端数が１ビットでもあれば32bit(4byte)加算になる。
	//	& ~31⇒ ＆ ～３１（0x1Fのビット反転:0001_1111 ⇒ 1110_0000:0xE0）でマスクすると、４の倍数になる。
	//	>>3 ⇒ >>1(1/2) ⇒ >>2(1/4) ⇒ >>3(1/8) ⇒ バイト数に換算
	int ppx_stride = (((bih.biWidth * bih.biBitCount + 31) & ~31) >> 3);	//有効ピクセル数を含んだ４の倍数(４バイトアライメント)のピクセル数にする。
	LONG height = bih.biHeight;	//画像の縦ピクセル数（負数ならトップダウン）
	BOOL is_top_down = (height < 0);	//負数(０未満)の場合はトップダウン。
	LONG add_stride = ppx_stride;	//Line毎に加算する値。１行分（4バイトアライメント）⇒BMPのピクセルデータは４バイトアライメントで格納されている。
	if (is_top_down) {
		//トップダウンDIB.
		height = (-height);	//トップダウンなので正の数に補正。
	}
	else {
		//ボトムアップDIB.
		tmp_px_data += (ppx_stride * (height - 1));	//一番下の行先頭にポイント。
		add_stride = (-add_stride);	//下から上に１行づつ減算してゆく。
	}
	//画素（ピクセル）のビット数に対応した変換用の関数ポインタを選択する。
	pb->colbit = bih.biBitCount;
	FUNC_NUM	bpp_num = BPP_1;
	switch (pb->colbit) {
	case	1:	bpp_num = BPP_1;	break;
	case	4:	bpp_num = BPP_4;	break;
	case	8:	bpp_num = BPP_8;	break;
	case	16:	bpp_num = BPP_16;	break;
	case	24:	bpp_num = BPP_24;	break;
	case	32:	bpp_num = BPP_32;	break;
	default:	bpp_num = BPP_8;	break;
	}
	int bypp = (pb->colbit <= 8) ? 1 : pb->colbit / 8;	//１ピクセル当たりのバイト数
	//(幅 × 高さ × 1ピクセル当たりのバイト数（但し8ビット以下は全て1バイト扱い））の領域を確保し、変換してから格納する
	pb->numpix = (bih.biWidth * height * bypp);
	pb->pixel = (char*)calloc(pb->numpix, sizeof(char));
	BYTE* tmp_ppx = (BYTE*)pb->pixel;
	if (!tmp_ppx) {
		//NULLポインタ（値０）が返ってきたら失敗(;_;)
		free(pixel_data);
		DeleteBmp(&pb);
		return (Bmp*)NULL;
	}
	//１ラインづつ画素（ピクセル）のビット数に対応したデータに変換しながらコピーする。
	for (int y = 0; y < height; y++) {
		//(int w_pixels, PBYTE dst_pxbuf, int stride, PBYTE src_ppx)
		print_line[bpp_num](bih.biWidth, tmp_ppx, ppx_stride, tmp_px_data);
		tmp_ppx += (bih.biWidth * bypp);
		tmp_px_data += add_stride;
	}
	//変換し終わったので元データは解放する
	free(pixel_data);
	//画像のサイズ（ピクセル単位）を使いやすい様に別の変数にコピーする
	pb->width = bih.biWidth;
	pb->height = bih.biHeight;
	pb->swapRB = FALSE;
	if (_palset_or_swap24RB && (pb->colbit == 24)) {
		Bmp24SwapRB(pb);	//<=swapRBはTRUEになる
	}
	if (_palset_or_swap24RB && ((pb->colbit == 4) || (pb->colbit == 8))) {
		SetPalette(pb->pal, 0, 15);	//コンソールのパレットへこのBMPのパレットを転送する（16色分転送）
	}
	//正常に読み込めたのでPicBmp構造体へのポインタを返す
	return pb;
}	//LoadBmp

/**
* @brief	使い終わったBMP画像の削除
*
* @param	Bmp** _pp_bmp	Bmp構造体ポインタ変数のアドレス。
*
* @note		渡されたBmp構造体のメンバついて：\n
*			pixelとpal（ポインタ）は確保されているメモリが解放される。\n
*			渡されたポインタ変数が指し示すBmp構造体も解放されNULLが代入される。\n
*/
void DeleteBmp(Bmp** _pp_bmp)
{
	if (_pp_bmp == NULL) {
		//NULLポインタなら何もしない
		return;
	}
	if ((*_pp_bmp) == NULL) {
		//ポインタの中身（アドレス）がNULLなら何もしない（Bmp構造体が確保されていない）
		return;
	}
	//確保したメモリ（ポインタ）が入っていれば削除する。
	if ((*_pp_bmp)->pixel != NULL) {
		//ピクセルデータ削除
		free((*_pp_bmp)->pixel);	//削除
		//Bmpの内容も消去
		//(*_pp_bmp)->pixel = NULL;
	}
	if ((*_pp_bmp)->pal != NULL) {
		//パレットデータ削除
		free((*_pp_bmp)->pal);	//削除
		//Bmpの内容も消去
		//(*_pp_bmp)->pal = NULL;
	}
	//(*_pp_bmp)->width = 0;
	//(*_pp_bmp)->height = 0;
	//(*_pp_bmp)->colbit = 0;
	//
	free(*_pp_bmp);	//Bmp本体を解放
	(*_pp_bmp) = NULL;	//NULLにしておく
	return;
}	//DeleteBmp

/**
* @brief	２４ビット画像の'Red'と'Blue'を入れ替える
*
* @note		BMPファイルの２４ビット画像の１ピクセルのRGB値の並び[B][R][G]を、<br/>
*			"conioex"の"PrintImage()"で出力する時の並び[R][G][B]に変換する。
*
* @param	Bmp構造体へのポインタ
*/
void Bmp24SwapRB(Bmp* _bmp)
{
	if (_bmp == NULL) {
		return;
	}
	if (_bmp->colbit != 24) {
		return;
	}
	if (_bmp->swapRB) {
		return;	//既に入替済み
	}
	char* pix = _bmp->pixel;
	int width = _bmp->width;
	int height = _bmp->height;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			char tmp = pix[2];	//2
			pix[2] = pix[0];	//2<-0
			pix[0] = tmp;		//0<-2
			pix += 3;
		}
	}
	_bmp->swapRB = TRUE;
}	//Bmp24SwapRB

/*
* @brief	Bmpオブジェクトの生成
*
* @param	int _width,_height：画像の幅と高さ（ピクセル数）
* @param	int _colbits：色数：１ピクセル当たりのビット数
* @param	int _numpal：パレット数：０なら、パレット用バッファ確保しない（COLORREF*palがNULLになる）
* @param	const COLORREF* const  _pal：転送元パレット：_numpalが１以上ある場合、転送元になるパレット。
*				このポインタがNULLで_numpalが１以上ある場合はパレット確保してデフォルト色をセットする。
*
* @return	Bmp*：作成したBmp構造体へのポインタ／エラーの場合はNULLを返す
*/
Bmp* CreateBmp(int _width, int _height, int _colbits, size_t _numpal, const COLORREF* const  _pal)
{
	//サイズが不正な場合は生成しない。
	if ((_width <= 0) || (_height <= 0)) {
		return (Bmp*)NULL;
	}
	Bmp* p_bmp = (Bmp*)calloc(1, sizeof(Bmp));
	_ASSERT(p_bmp);

	//=== パレット設定 ===
	//パレット数の補正（元のパレット数が間違っていたら正しい値を算出）
	switch (_colbits) {
	case	1:
		_numpal = 2;
		break;
	case	4:
		//パレット数がオーバーしていたら補正
		if (_numpal > 16) {
			_numpal = 16;
		}
		break;
	case	8:
		//パレット数がオーバーしていたら補正
		if (_numpal > 256) {
			_numpal = 256;
		}
		break;
	case	16:
	case	24:
	case	32:
		_numpal = 0;
		break;
	default:
		//パレットが必要な色ビット数じゃなければパレットは確保しないのでパレット数を０にする。
		_colbits = 4;	//範囲外は4bitColor
		_numpal = 0;	//パレットは無し
		break;
	}
	p_bmp->colbit = _colbits;		//補正済み色ビット数
	p_bmp->numpal = (int)_numpal;	//正しい値を新しく作るBmpのパレット数にセット
	//
	//パレット用バッファの確保
	p_bmp->pal = (COLORREF*)NULL;		//COLORREF*パレット（16色）データへのポインタ※無い場合はNULL
	if (p_bmp->numpal > 0) {
		//パレット数指定が１以上あるのでメモリを確保
		p_bmp->pal = (COLORREF*)calloc(p_bmp->numpal, sizeof(COLORREF));
		_ASSERT(p_bmp->pal);
		if (_pal != NULL) {
			//転送元パレット指定があればコピーする。
			memcpy(p_bmp->pal, _pal, p_bmp->numpal * sizeof(COLORREF));
		}
		else {
			//指定が無い場合はデフォルトカラーを入れておく。
			memcpy(p_bmp->pal, ANSI_PAL256_COLOR, p_bmp->numpal * sizeof(COLORREF));
		}
	}

	//=== 画像ピクセル ===
	p_bmp->width = _width;		//幅
	p_bmp->height = _height;	//高さ
	int bypp = 1;
	switch (p_bmp->colbit) {
	case	1:
	case	2:
	case	4:
	case	8:
		//１ピクセル=1バイトなのでそのまま
		break;
	case	16:
		//１ピクセル=２バイト
		bypp = 2;
		break;
	case	24:
		bypp = 3;
		break;
	case	32:
		bypp = 4;
		break;
	}
	p_bmp->numpix = (p_bmp->width * p_bmp->height) * bypp;	//画像データのバイト数
	//メモリを確保
	p_bmp->pixel = (char*)calloc(1, p_bmp->numpix);	//char*画像データへのポインタ
	_ASSERT(p_bmp->pixel);
	//
	p_bmp->swapRB = false;	//24ビット以上の画像で、RとBが入れ替わっている場合は'TRUE'になる
	//
	p_bmp->wch = 0;		//変換元の文字 wchar_t
	p_bmp->aaLv = 0;	//文字表示の時のアンチエイリアスレベル
	return p_bmp;
}	//CreateBmp

/*
* @brief	画像の分割読込
*			4ビット／８ビット／２４ビット画像のみ対応
*
* @param	const char* _path	分割元になる画像ファイル名
* @param	int _x0,_y0		分割開始する座標
* @param	int _xpix,_ypix	分割するセル画像１個の幅と高さ
* @param	int _xcount		横方向の個数
* @param	int _ycount		縦方向の個数
* @param	Bmp** _pp_bmp	分割したBmp*を入れる配列のアドレス
*
* @return	bool：読込の成否（true:成功/false:失敗）
*/
bool LoadDivBmp(const char* _path, int _x0, int _y0, size_t _xpix, size_t _ypix, size_t _xcount, size_t _ycount, Bmp** _pp_bmp)
{
	if ((_path == NULL) || (_pp_bmp == NULL)) {
		return false;
	}
	if ((_xpix * _ypix * _xcount * _ycount) == 0) {
		return false;
	}
	//_pp_bmp = ppBmp;
	Bmp* ptb = LoadBmp(_path);	//
	_ASSERT(ptb);
	ptb->pixel;

	int dest_idx = 0;
	//横⇒縦方向に取り込んでゆく
	for (int y = 0; y < _ycount; y++) {
		for (int x = 0; x < _xcount; x++) {
			//切り出しサイズで元のBmpと同じ色数の空のBmpを作る
			Bmp* pp = CreateBmp(_xpix, _ypix, ptb->colbit, ptb->numpal, ptb->pal);
			CopyBmp(pp, 0, 0, ptb, _x0 + (x * _xpix), _y0 + (y * _ypix), _xpix, _ypix, false);
			_pp_bmp[dest_idx] = pp;
			dest_idx++;
		}
	}
	return true;
}	//LoadDivBmp

/*
* @brief	Bmpの全ピクセルを初期化する
*
* @param	int	_color	パレット番号又はRGB値を指定
*				パレットのある画像の場合はパレット番号を指定する
*				フルカラーの場合は0x00RRGGBBを指定する
*/
void ClearBmp(Bmp* _p, int _color)
{
	if (_p->colbit == 4) {
		memset(_p->pixel, _color & 0xF, _p->numpix);
	}
	else if (_p->colbit == 8) {
		memset(_p->pixel, _color & 0xFF, _p->numpix);
	}
	else if (_p->colbit == 24) {
		int rr = (_color & 0xFF0000) >> 16;
		int gg = (_color & 0x00FF00) >> 8;
		int bb = (_color & 0x0000FF);
		for (int i = 0; i < _p->numpix; i += 3) {
			_p->pixel[i + 0] = bb;
			_p->pixel[i + 1] = gg;
			_p->pixel[i + 2] = rr;
		}
	}
}	//ClearBmp

/*
* @brief	Bmp画像からBmp画像への矩形転送
*			・転送先と転送元の画像の大きさは違っていても良い
*			・転送先と転送元の画像のカラービット数は同じでなければならない
*			・転送先のパレットは転送元のパレットで上書きされる（パレットのサイズが違う場合は小さい方に合わせる）
*
* @param	Bmp* _dest			転送先Bmp画像（元に画像は上書きされる）
* @param	int _dx,_dy			転送先の座標
* @param	const Bmp* _src		転送元Bmp画像
* @param	int	_sx,_sy			転送元の座標
* @param	int	_width,_height	転送元のサイズ
*/
Bmp* CopyBmp(Bmp* _dest, int _dx, int _dy, const Bmp* _src, int _sx, int _sy, int _width, int _height, bool _tr)
{
	if ((_dest == NULL) || (_src == NULL) || (_width <= 0) || (_height <= 0)) {
		return NULL;
	}
	if (_src->colbit != _dest->colbit) {
		return NULL;
	}
	//パレットのコピー
	int pal_size = _src->numpal;	//転送元のサイズ
	if (_dest->numpal < _src->numpal) {
		//転送先のサイズが小さい場合は、転送先のサイズに合わせる。
		pal_size = _dest->numpal;
	}
	memcpy(_dest->pal, _src->pal, pal_size);	//パレット転送実行
	//////memset(_dest->pixel, 0, _dest->numpix);	//ピクセル全部を０で初期化しておく（転送しない部分は０になる）
	//矩形転送 ---
	if ((_src->colbit == 4) || (_src->colbit == 8)) {
		for (int y = 0; y < _height; y++) {
			for (int x = 0; x < _width; x++) {
				int dx = _dx + x;
				int dy = _dy + y;
				if ((dx < 0) || (dx >= _dest->width) || (dy < 0) || (dy >= _dest->height)) {
					continue;
				}
				int xx = _sx + x;
				int yy = _sy + y;
				if ((xx >= 0) && (xx < _src->width) && (yy >= 0) && (yy < _src->height)) {
					//元画像の範囲内のみ転送する⇒範囲外は元の画像のまま
					char c = _src->pixel[yy * _src->width + xx];
					if ((!_tr) || (c != 0)) {
						//透明指定が無しor透明ピクセルでは無い
						//*dest_p = c;
						_dest->pixel[dy * _dest->width + dx] = c;
					}
				}
				//dest_p++;
			}
		}
	}
	else if (_src->colbit == 16) {
		//矩形ブロック転送
		for (int y = 0; y < _height; y++) {
			for (int x = 0; x < _width; x++) {
				int dx = _dx + x;
				int dy = _dy + y;
				if ((dx < 0) || (dx >= _dest->width) || (dy < 0) || (dy >= _dest->height)) {
					continue;
				}
				int xx = _sx + x;
				int yy = _sy + y;
				if ((xx >= 0) && (xx < _src->width) && (yy >= 0) && (yy < _src->height)) {
					//元画像の範囲内のみ転送する⇒範囲外は元の画像のまま
					UINT16 ui16 = ((UINT16*)_src->pixel)[yy * _src->width + xx];
					if ((!_tr) || (ui16 != 0)) {
						//透明指定が無しor透明ピクセルでは無い
						((UINT16*)_dest->pixel)[dy * _dest->width + dx] = ui16;
					}
				}
			}
		}
	}
	else if (_src->colbit == 24) {
		//矩形ブロック転送
		for (int y = 0; y < _height; y++) {
			for (int x = 0; x < _width; x++) {
				int dx = _dx + x;
				int dy = _dy + y;
				if ((dx < 0) || (dx >= _dest->width) || (dy < 0) || (dy >= _dest->height)) {
					continue;
				}
				int xx = _sx + x;
				int yy = _sy + y;
				if ((xx >= 0) && (xx < _src->width) && (yy >= 0) && (yy < _src->height)) {
					char* pd = &_dest->pixel[dy * (_dest->width * 3) + (dx * 3)];
					char* ps = &_src->pixel[yy * (_src->width * 3) + (xx * 3)];
					int c = (ps[0] | ps[1] | ps[2]);
					if ((!_tr) || (c != 0)) {
						pd[0] = ps[0];
						pd[1] = ps[1];
						pd[2] = ps[2];
					}
				}
				//dest_p += 3;
			}
		}
	}
	else if (_src->colbit == 32) {
		//矩形ブロック転送
		for (int y = 0; y < _height; y++) {
			for (int x = 0; x < _width; x++) {
				int dx = _dx + x;
				int dy = _dy + y;
				if ((dx < 0) || (dx >= _dest->width) || (dy < 0) || (dy >= _dest->height)) {
					continue;
				}
				int xx = _sx + x;
				int yy = _sy + y;
				if ((xx >= 0) && (xx < _src->width) && (yy >= 0) && (yy < _src->height)) {
					//元画像の範囲内のみ転送する⇒範囲外は元の画像のまま
					UINT32 ui32 = ((UINT32*)_src->pixel)[yy * _src->width + xx];
					if ((!_tr) || (ui32 != 0)) {
						//透明指定が無しor透明ピクセルでは無い
						((UINT32*)_dest->pixel)[dy * _dest->width + dx] = ui32;
					}
				}
			}
		}
	}
	return _dest;
}	//CopyBmp

/*
* @brief	Bmp画像の指定範囲を元に、新しいBmpを生成する。
*
* @param	const Bmp* _src		元の画像
* @param	int _xp,_yp			指定範囲の左上座標
* @param	int _width,_height	指定範囲の幅と高さ
*
* @return	Bmp*	新しく作ったBmp画像のポインタ
*/
Bmp* CreateFromBmp(const Bmp* _src, int _xp, int _yp, int _width, int _height)
{
	if ((_src == NULL) || (_width <= 0) || (_height <= 0)) {
		return NULL;
	}
	Bmp* p_dest = CreateBmp(_width, _height, _src->colbit, _src->numpal, _src->pal);
	_ASSERT(p_dest);
	CopyBmp(p_dest, _src, _xp, _yp, _width, _height, false);
	return p_dest;
}	//CreateFromBmp


//================================================================
// Bitmapフォント生成関数
// 
// 指定のフォントを使ってそのフォントで生成された文字列イメージをビットマップ画像データに変換する。
// 【注】コンソールプログラム専用⇒内部でコンソールウィンドウのハンドルを使っている。
//
// ToDo:
// パレットの何番の色を使うかを外部から出来る様にする。
// ex)２値色の時、'0'->pal[13], '1'->pal[14]とか。
//================================================================

//const DWORD Gray65[65] = {
//	//======== START ========
//	0x00000000,
//	0x00030303,0x00070707,0x000b0b0b,0x000f0f0f,0x00131313,0x00171717,0x001b1b1b,0x001f1f1f,
//	0x00232323,0x00272727,0x002b2b2b,0x002f2f2f,0x00333333,0x00373737,0x003b3b3b,0x003f3f3f,//[16]
//	0x00434343,0x00474747,0x004b4b4b,0x004f4f4f,0x00535353,0x00575757,0x005b5b5b,0x005f5f5f,
//	0x00636363,0x00676767,0x006b6b6b,0x006f6f6f,0x00737373,0x00777777,0x007b7b7b,0x007f7f7f,//[32]
//	0x00838383,0x00878787,0x008b8b8b,0x008f8f8f,0x00939393,0x00979797,0x009b9b9b,0x009f9f9f,
//	0x00a3a3a3,0x00a7a7a7,0x00ababab,0x00afafaf,0x00b3b3b3,0x00b7b7b7,0x00bbbbbb,0x00bfbfbf,//[48]
//	0x00c3c3c3,0x00c7c7c7,0x00cbcbcb,0x00cfcfcf,0x00d3d3d3,0x00d7d7d7,0x00dbdbdb,0x00dfdfdf,
//	0x00e3e3e3,0x00e7e7e7,0x00ebebeb,0x00efefef,0x00f3f3f3,0x00f7f7f7,0x00fbfbfb,0x00ffffff,//[64]
//	//======== END ========
//};
//const DWORD Gray17[17] = {
//	//======== START ========
//	0x00000000,
//	0x000f0f0f,0x001f1f1f,0x002f2f2f,0x003f3f3f,0x004f4f4f,0x005f5f5f,0x006f6f6f,0x007f7f7f,
//	0x008f8f8f,0x009f9f9f,0x00afafaf,0x00bfbfbf,0x00cfcfcf,0x00dfdfdf,0x00efefef,0x00ffffff,
//	//======== END ========
//};
//const DWORD Gray5[5] = {
//	//======== START ========
//	0x00000000,
//	0x003f3f3f,0x007f7f7f,0x00bfbfbf,0x00ffffff,
//	//======== END ========
//};
//const DWORD Gray2[2] = {
//	//======== START ========
//	0x00000000,
//	0x00ffffff,
//	//======== END ========
//};

const DWORD Gray2[2] = {
	//======== START ========
	0x00000000,
	0xFFffffff,//[ 1]
	//======== END ========
};
const DWORD Gray5[5] = {
	//======== START ========
	0x00000000,
	0x3F3f3f3f,0x7F7f7f7f,0xBFbfbfbf,0xFFffffff,//[ 4]
	//======== END ========
};
const DWORD Gray17[17] = {
	//======== START ========
	0x00000000,
	0x0F0f0f0f,0x1F1f1f1f,0x2F2f2f2f,0x3F3f3f3f,0x4F4f4f4f,0x5F5f5f5f,0x6F6f6f6f,0x7F7f7f7f,
	0x8F8f8f8f,0x9F9f9f9f,0xAFafafaf,0xBFbfbfbf,0xCFcfcfcf,0xDFdfdfdf,0xEFefefef,0xFFffffff,//[16]
	//======== END ========
};
const DWORD Gray65[65] = {
	//======== START ========
	0x00000000,
	0x03030303,0x07070707,0x0B0b0b0b,0x0F0f0f0f,0x13131313,0x17171717,0x1B1b1b1b,0x1F1f1f1f,0x23232323,0x27272727,0x2B2b2b2b,0x2F2f2f2f,0x33333333,0x37373737,0x3B3b3b3b,0x3F3f3f3f,//[16]
	0x43434343,0x47474747,0x4B4b4b4b,0x4F4f4f4f,0x53535353,0x57575757,0x5B5b5b5b,0x5F5f5f5f,0x63636363,0x67676767,0x6B6b6b6b,0x6F6f6f6f,0x73737373,0x77777777,0x7B7b7b7b,0x7F7f7f7f,//[32]
	0x83838383,0x87878787,0x8B8b8b8b,0x8F8f8f8f,0x93939393,0x97979797,0x9B9b9b9b,0x9F9f9f9f,0xA3a3a3a3,0xA7a7a7a7,0xABababab,0xAFafafaf,0xB3b3b3b3,0xB7b7b7b7,0xBBbbbbbb,0xBFbfbfbf,//[48]
	0xC3c3c3c3,0xC7c7c7c7,0xCBcbcbcb,0xCFcfcfcf,0xD3d3d3d3,0xD7d7d7d7,0xDBdbdbdb,0xDFdfdfdf,0xE3e3e3e3,0xE7e7e7e7,0xEBebebeb,0xEFefefef,0xF3f3f3f3,0xF7f7f7f7,0xFBfbfbfb,0xFFffffff,//[64]
	//======== END ========
};

#if false
//=== ビットマップフォント用グレイスケール出力コード ===
void GrayDump(void)
{
	double rgb = 0;
	const int resolution = 65;	//{2|5|17|65}
	const double step = (1.0 / (double)(resolution - 1));
	_RPTN(_CRT_WARN, "const DWORD Gray%d[%d]={\n", resolution, resolution);
	_RPT0(_CRT_WARN, "//======== START ========\n");
	_RPT0(_CRT_WARN, "0x00000000,\n");
	for (int n = 1; n < resolution; n++) {
		int rgb = (int)((step * (double)n) * 255.0);
		_RPTN(_CRT_WARN, "0x00%02x%02x%02x,", rgb, rgb, rgb);
		if ((n % 16) == 0) {
			_RPTN(_CRT_WARN, "//[%2d]\n", n);
		}
		else if (n == (resolution - 1)) {
			_RPTN(_CRT_WARN, "//[%2d]\n", n);
		}
	}
	_RPT0(_CRT_WARN, "//======== END ========\n");
	_RPT0(_CRT_WARN, "};\n");
}
#endif // false

/**
* @brief	1bppの画像を8bppの画像に変換する。
*
* @param	Bmp* _pbc	ビットマップ文字データへのポインタ
* @param	GLYPHMETRICS* _pgm	変換元文字のグリフ情報
*
* @return
* 	なし
*/
static void convert_bpp1_to_bpp8(Bmp* _pbc, const GLYPHMETRICS* _pgm)
{
	int w_pix = _pgm->gmBlackBoxX;
	int h_pix = _pgm->gmBlackBoxY;
	int stride = (_pbc->numpix / _pgm->gmBlackBoxY);
	int stride4 = (w_pix + 0b0011) & (~0b0011);			//８bpp画像の４バイト境界のバイト数
	int bits_size = stride4 * h_pix;
	char* pFontBitmap = (char*)calloc(bits_size, sizeof(char));
	_ASSERT(pFontBitmap != NULL);
	//ZeroMemory(pFontBitmap, bits_size);
	for (int y = 0; y < h_pix; y++) {
		for (int x = 0; x < stride; x++) {
			int idxSrc = (y * stride + x);
			UINT bit8 = _pbc->pixel[idxSrc];
			int idxDest = (y * stride4) + (x * 8);
			for (int bitN = 0; bitN < 8; bitN++) {
				if ((idxDest + bitN) < bits_size) {
					//pFontBitmap[idxDest + bitN] = (bit8 & (0b10000000 >> bitN)) ? 1 : 0;	//0xFF : 0x00;
					if ((bit8 & (0b10000000 >> bitN)) != 0) {
						pFontBitmap[idxDest + bitN] = 1;
					}
				}
			}
		}
	}
	//古い1bppのバッファは削除して新しく作った8bppバッファに入れ替える。
	free(_pbc->pixel);
	_pbc->pixel = pFontBitmap;
	_pbc->numpix = bits_size;
}	//convert_bpp1_to_bpp8

/**
*  @brief
* 	ビットマップ文字１文字の表示位置を調整してビットマップを作り直す。
*
* @param	BitmapChar* _pbc	ビットマップ文字のポインタ。このポインタが指すビットマップ文字データの表示位置を調整してバッファが作り直される。
* @param	GLYPHMETRICS* _pgm	変換元文字のグリフ情報
* @param	TEXTMETRIC* _ptxm	変換元フォントの計測（文字の寸法）情報
*
* @return
* 	無し
*/
static void build_bmp_char(Bmp* _pbc, const GLYPHMETRICS* _pgm, const TEXTMETRICW* _ptxm)
{
	//転送先バッファを作る
	int	dest_width = _pgm->gmCellIncX;
	int dest_height = _ptxm->tmHeight;
	int dest_buf_size = dest_width * dest_height;
	char* pDest = (char*)calloc(dest_buf_size, sizeof(char));	//転送先を全て０で初期化（ドットの無い場所（処理しない場所）は０になる）
	_ASSERT(pDest != NULL);
	//転送元サイズを計算（横幅は４の倍数）
	int width = _pgm->gmBlackBoxX;
	//int widthBytes = (width + 0b0011) & (~0b0011);	//横幅のバイト数は４の倍数に合わせる
	int height = _pgm->gmBlackBoxY;
	int stride = _pbc->numpix / _pbc->height;	//転送元バッファの横幅
	//
	for (int y = 0; y < height; y++)
	{
		int yp = (_ptxm->tmAscent - _pgm->gmptGlyphOrigin.y) + y;
		if ((yp < 0) || (yp >= dest_height))
		{
			//上下方向に範囲外なら処理しない
			continue;
		}
		for (int x = 0; x < width; x++)
		{
			int xp = _pgm->gmptGlyphOrigin.x + x;
			if ((xp < 0) || (xp >= dest_width))
			{
				//左右方向に範囲外なら処理しない
				continue;
			}
			//X,Y位置から１次元配列の読み出し位置を算出
			//int read_idx = (y * widthBytes + x);
			int read_idx = (y * stride + x);
			if ((read_idx < 0) || (read_idx >= (int)_pbc->numpix))
			{
				//転送元の範囲を超えていたら処理しない
				continue;
			}
			//unsigned char dot = _pbc->pPix[y * stride + x];
			unsigned char dot = _pbc->pixel[read_idx];	//転送元から１ピクセル読み出す
			if (dot == 0x00)
			{
				//色コード０は透明扱いなので処理しない
				continue;
			}
#ifndef USED2D
			/*
			* ToDo:パレットの何番の色を使うかを外部から操作できるようにしたい。
			* ex)２値色の時、'0'->pal[13],'1'->pal[14]とか。
			*/
			if (_pbc->aaLv == 2)
			{
				//２値の場合は｛０｜１｝なので０以外は全て'Ｆ'とする。
				dot = 0x0F;
			}
			else
			{
				//アンチエイリアスの諧調指定とdotの値を元に、１６段階(16色)の濃度に対応させた値に変換する。
				dot = (unsigned char)((double)(16.0 / (double)(_pbc->aaLv - 1)) * (double)dot);
				if (dot > 0x0F) {
					dot = 0x0F;	//パレットは１６色しかないので１５を超えない様にする。
				}
			}
#endif // USED2D
			pDest[yp * dest_width + xp] = dot;
		}
	}
	free(_pbc->pixel);
	_pbc->pixel = pDest;
	_pbc->numpix = dest_buf_size;
	_pbc->width = dest_width;
	_pbc->height = dest_height;
	return;
}	//build_bmp_char

#ifndef UNICODE
/**
* @brief	マルチバイト文字列から次の１文字を取り出しUINTに変換して返す。\n
*			渡されたポインタをマルチバイト文字換算で１文字進める。
*
* @param	BYTE** p：マルチバイト文字列のポインタのポインタ
*
* @return	UINT：マルチバイト文字コード
*/
static UINT get_MBC(BYTE** p) {
	UINT mbc = **p;	//１バイト取り込む
	if (IsDBCSLeadByte(mbc)) {
		mbc <<= 8;	//１バイト目は上位バイトへ入れる
		(*p)++;	//全角文字は２バイト目を取り込む
		mbc |= **p;	//２バイト目は下位バイトに入れる
	}
	(*p)++;	//ポインタ進める
	return mbc;
}	//get_MBC
#endif // UNICODE

/**
* @brief	文字列（ワイド文字｜Unocode文字）をBmp画像の配列として生成する。\n
*
* @param	const TCHAR* _font_name : フォント名（ワイド文字｜Unocode文字）
* @param	int _font_size : フォント・サイズ
* @param	int _bold : 太字指定：trueで太字
* @param	int _ggo : アンチ・エイリアスの諧調指定\n
*			{GGO_BITMAP,GGO_GRAY2_BITMAP,GGO_GRAY4_BITMAP,GGO_GRAY8_BITMAP}\n
*			※WIN32PAI::GetGlyphOutline()関数参照
* @param	const TCHAR* _text : 変換したい文字列（ワイド文字｜Unocode文字）
*
* @return	Bmp*	変換後のビットマップ文字の配列へのポインタ。(ターミネーターとして全メンバがNULLのBmpが入る)
*					※返されたBmpは必ずDeleteBmp()で削除する事
*
* @note
*		指定のフォントで出来たビットマップ文字Bmpの配列を作成し先頭のポインタを返す。\n
*		※ワイド文字(char)/Unocode文字(wchar_t)両対応。\n
*		※コンパイル時にTCHARがワイド文字(char)かUnocode文字(wchar_t)に切替わる。\n
*
*/
Bmp* CreateBmpChar(const TCHAR* _font_name, int _font_size, int _bold, int _ggo, const TCHAR* _text)
{
	//回転行列
	MAT2	mat2{ {0,1},{0,0},{0,0},{0,1} };
	//フォントの設定～作成
	LOGFONT	lf;
	lf.lfHeight = _font_size;
	lf.lfWidth = 0;
	lf.lfEscapement = 0;	//文字単位の回転角度左回り
	lf.lfOrientation = 0;
	if (_bold) {
		lf.lfWeight = FW_BOLD;	//太字設定
	}
	else {
		lf.lfWeight = FW_NORMAL;
	}
	lf.lfItalic = FALSE;	//斜体
	lf.lfUnderline = FALSE;
	lf.lfStrikeOut = FALSE;
#ifdef UNICODE
	lf.lfCharSet = ANSI_CHARSET;
#else
	lf.lfCharSet = SHIFTJIS_CHARSET;
#endif // UNICODE
	lf.lfOutPrecision = OUT_TT_PRECIS;	//OUT_DEFAULT_PRECIS
	lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lf.lfQuality = PROOF_QUALITY;
	lf.lfPitchAndFamily = (DEFAULT_PITCH | FF_MODERN);
	lf.lfFaceName[0] = '\0';
	//書体名をコピー（文字数制限あり）
	if (_font_name != nullptr) {
		CopyMemory(lf.lfFaceName, _font_name, LF_FACESIZE * sizeof(TCHAR));
	}
	else {
		//指定が無い時は「ＭＳ明朝」とする
		CopyMemory(lf.lfFaceName, _T("ＭＳ 明朝"), LF_FACESIZE * sizeof(TCHAR));
		//CopyMemory(lf.lfFaceName, _T(""), LF_FACESIZE * sizeof(TCHAR));
	}
	//フォント生成
	HFONT hFont = CreateFontIndirect(&lf);
	_ASSERT(hFont);
	if (hFont == NULL) {
		return	NULL;
	}
	// デバイスにフォントを選択する
	HWND hWnd = GetConsoleWindow();	//★★★このコンソールのウィンドウハンドル
	HDC hdc = GetDC(hWnd);
	HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
	//生成したフォントの計測データを取得する
	TEXTMETRICW	txm;		//変換したフォントの情報を入れる構造体
	GetTextMetricsW(hdc, &txm);	//計測データを取得
	int aa_level = 2;
	switch (_ggo)
	{
	default:	//２値
	case GGO_BITMAP:		aa_level = 2;		break;	//２値
	case GGO_GRAY2_BITMAP:	aa_level = 5;		break;	//５階調
	case GGO_GRAY4_BITMAP:	aa_level = 17;	break;	//１７階調
	case GGO_GRAY8_BITMAP:	aa_level = 65;	break;	//６５階調
	}
	//指定のフォントで出来たビットマップ文字で文字列を作成する。
	GLYPHMETRICS	gm{ 0,0,0,0,0,0 };	//グリフ設定データ
	UINT code = 0;
	//文字列の文字数を求める。
#ifdef UNICODE
	size_t length = (int)wcslen(_text);	//ワイド文字(Unicode)の文字数を数える
	size_t buff_len = length + 1;
	const TCHAR* code_ary = (TCHAR*)_text;
#else
	//マルチバイト文字の場合は全角だけ２文字分をUINTに変換する
	size_t length = _mbstrlen(_text);	//全角も半角も１文字として数える
	size_t buff_len = length + 1;
	UINT* code_ary = (UINT*)_alloca(buff_len * sizeof(UINT));	//'\0'含む文字数のBmpを確保する
	memset(code_ary, 0, buff_len * sizeof(UINT));
	const BYTE* p = (BYTE*)_text;
	for (int i = 0; (*p != '\0') && (i < length); i++) {
		code_ary[i] = get_MBC((BYTE**)&p);
	}
#endif // UNICODE
	//Bmp用バッファを文字数分確保（最後の'\0'用のBmpも含める）（全て０で初期化）
	Bmp* pBmpChr = (Bmp*)calloc(buff_len, sizeof(Bmp));
	_ASSERT(pBmpChr != NULL);
	//１文字に付き１つのBmpオブジェクトを生成してBmpの配列に格納して行く
	for (size_t txn = 0; txn < length; txn++) {
		code = (UINT)code_ary[txn];
		//これから生成する文字ビットマップデータのバイト数を取得する。
		int buff_size = GetGlyphOutline(hdc, code, _ggo, &gm, 0, NULL, &mat2);
		//if (buff_size > 0)
		if (code != 0)
		{
			//取得したサイズ分のバッファを確保する。’ ’空白の場合は０(zero)が返るが、そのままmallocする。
			pBmpChr[txn].pixel = (char*)calloc(buff_size, sizeof(char));
			//’ ’空白の場合buff_size＝０でもgmには正しい値がセットされている様だ。
			GetGlyphOutline(hdc, code, _ggo, &gm, buff_size, pBmpChr[txn].pixel, &mat2);
			if (_ggo == GGO_BITMAP)
			{
				//１bppのビットマップは表示しにくいので８bppに変換する。
				pBmpChr[txn].numpix = buff_size;		//バッファサイズ
				convert_bpp1_to_bpp8(&pBmpChr[txn], &gm);	//１ビット/ピクセル画像を８ビット/ピクセル画像に変換
				buff_size = pBmpChr[txn].numpix;
			}
			pBmpChr[txn].width = gm.gmBlackBoxX;	//横ピクセル数
			pBmpChr[txn].height = gm.gmBlackBoxY;	//縦ピクセル数
			//全ての文字画像を８ビット/ピクセルの画像として扱う
			pBmpChr[txn].colbit = 8;				//８ビット/pixel画像
			pBmpChr[txn].numpix = buff_size;		//バッファサイズ
			pBmpChr[txn].aaLv = aa_level;			//アンチエイリアスの諧調レベル
			pBmpChr[txn].wch = code;				//変換元の文字コード
			//文字位置を調整してバッファを作り直す。
			build_bmp_char(&pBmpChr[txn], &gm, &txm);
#ifdef USED2D
			//位置調整が済んだ画像にパレット(RGBQUAD型)を確保する
			pBmpChr[txn].pal = 0;	//COLORREF型は確保しない
			pBmpChr[txn].pal_rgb = (RGBQUAD*)calloc(NUM_D2D_PAL, sizeof(RGBQUAD));
			pBmpChr[txn].numpal = NUM_D2D_PAL;	//パレット数は256色固定とする
			switch (_ggo)
			{
			default:	//２値
			case GGO_BITMAP:
				memcpy_s(pBmpChr[txn].pal_rgb, NUM_D2D_PAL * sizeof(RGBQUAD), Gray2, sizeof(Gray2));
				break;	//２値
			case GGO_GRAY2_BITMAP:
				memcpy_s(pBmpChr[txn].pal_rgb, NUM_D2D_PAL * sizeof(RGBQUAD), Gray5, sizeof(Gray5));
				break;	//５階調
			case GGO_GRAY4_BITMAP:
				memcpy_s(pBmpChr[txn].pal_rgb, NUM_D2D_PAL * sizeof(RGBQUAD), Gray17, sizeof(Gray17));
				break;	//１７階調
			case GGO_GRAY8_BITMAP:
				memcpy_s(pBmpChr[txn].pal_rgb, NUM_D2D_PAL * sizeof(RGBQUAD), Gray65, sizeof(Gray65));
				break;	//６５階調
			}
#endif // USED2D
		}
	}
	//デバイスのフォント選択を解除する（元に戻す）
	SelectObject(hdc, oldFont);
	DeleteObject(hFont);
	ReleaseDC(hWnd, hdc);
	_ASSERT(pBmpChr);
	return	pBmpChr;
}	//CreateBmpChar

/**
* @brief	文字列（ワイド文字｜Unocode文字）を１枚の画像として生成する。
*
* @param	const TCHAR* _font_name	フォント名（ワイド文字｜Unocode文字）
* @param	int _font_size : フォント・サイズ
* @param	int _bold : 太字指定：trueで太字
* @param	int _ggo : アンチ・エイリアスの諧調指定\n
*			{GGO_BITMAP,GGO_GRAY2_BITMAP,GGO_GRAY4_BITMAP,GGO_GRAY8_BITMAP}\n
*			※WIN32PAI::GetGlyphOutline()関数参照
* @param	const TCHAR* _text	変換したい文字列（ワイド文字｜Unocode文字）
*
* @return	Bmp* : 変換後のビットマップ文字の配列へのポインタ。
*
* @note		指定のフォントで出来たビットマップ文字列のBmp画像を作成し、そのポインタを返す。\n
* 			CreateBmpChar()で得られたBmpの配列（１文字ごとの画像の配列）を連結して１枚の画像にし、
* 			そのバッファ（BmpString）のポインタを返す。
*			※ワイド文字(char)/Unocode文字(wchar_t)両対応。\n
*			※コンパイル時にTCHARがワイド文字(char)かUnocode文字(wchar_t)に切替わる。\n
*/
Bmp* CreateBmpString(const TCHAR* _font_name, int _font_size, int _bold, int _ggo, const TCHAR* _text)
{
	//先ず、１文字ごとの画像の配列を作成する。
	Bmp* pBc = CreateBmpChar(_font_name, _font_size, _bold, _ggo, _text);
	_ASSERT(pBc);
	int n = 0;
	int xpos = 0;
	if (pBc != NULL) {
		while (pBc[n].pixel != NULL) {
			//全ての文字を繋ぎ合わせた時の幅（ピクセル数）を計算する
			xpos += pBc[n].width;	//次の文字の横方向位置をセット
			n++;	//次の文字
		}
	}
	int width = xpos; //文字列画像全体の幅（ピクセル数）
	int height = _font_size;	//この文字列画像の高さ（ピクセル数）
	Bmp* bm_str = (Bmp*)calloc(1, sizeof(Bmp));	//Bmpオブジェクト１個生成：全ての文字Bmpを１つのBmpに集約する。
	//ZeroMemory(bm_str, sizeof(Bmp));
	bm_str->numpix = (width * height * sizeof(char));	//文字列画像のピクセルサイズ
	bm_str->pixel = (char*)calloc(bm_str->numpix, sizeof(char));	//画像バッファ確保
	bm_str->aaLv = pBc[0].aaLv;	//先頭文字[0]の諧調コードを使う
	bm_str->width = width;		//画像の幅（ピクセル）
	bm_str->height = height;	//画像高さ（ピクセル）
	//パレット作成とコピー
	bm_str->colbit = pBc[0].colbit;	//先頭文字[0]のビット数/ピクセルを使う
	bm_str->numpal = pBc[0].numpal;	//先頭文字[0]のパレット数
#ifdef USED2D
	bm_str->pal_rgb = (RGBQUAD*)calloc(pBc[0].numpal, sizeof(RGBQUAD));	//[0]と同じパレットを確保する
	memcpy_s(bm_str->pal_rgb, NUM_D2D_PAL * sizeof(RGBQUAD), pBc[0].pal_rgb, NUM_D2D_PAL * sizeof(RGBQUAD));
#endif // USED2D
	//
	n = 0;
	xpos = 0;
	while (pBc[n].pixel != NULL) {
		int pn = 0;
		for (int y = 0; y < pBc[n].height; y++) {
			for (int x = 0; x < pBc[n].width; x++) {
				int xp = (xpos + x);
				if ((xp >= 0) && (xp < bm_str->width) && (y >= 0) && (y < bm_str->height)) {
					//フレームバッファの範囲を超えてなければ書き込む
					bm_str->pixel[y * bm_str->width + xp] = pBc[n].pixel[pn];	//１ピクセル書き込む
				}
				pn++;	//次のピクセル読み出し位置
			}
		}
		xpos += pBc[n].width;	//次の横方向位置
		n++;
	}
	//確保したメモリの開放
	n = 0;
	while (pBc[n].pixel != NULL) {
		free(pBc[n].pixel);
		pBc[n].pixel = NULL;
#ifdef USED2D
		if (pBc[n].pal_rgb != NULL) {
			free(pBc[n].pal_rgb);
			pBc[n].pal_rgb = NULL;
		}
		if (pBc[n].pal != NULL) {
			free(pBc[n].pal);
			pBc[n].pal = NULL;
		}
#endif // USED2D
		n++;
	}
	free(pBc);
	return bm_str;
}	//CreateBmpString

/**
* @brief	文字列（ワイド文字｜Unocode文字）を書式指定して１枚の画像として生成する。
*
* @param	const TCHAR* _font_name	フォント名（ワイド文字｜Unocode文字）
* @param	int _font_size : フォント・サイズ
* @param	int _bold : 太字指定：trueで太字
* @param	int _ggo : アンチ・エイリアスの諧調指定\n
*			{GGO_BITMAP,GGO_GRAY2_BITMAP,GGO_GRAY4_BITMAP,GGO_GRAY8_BITMAP}\n
*			※WIN32PAI::GetGlyphOutline()関数参照
* @param	const char* _format：書式指定文字列
* @param	...：可変長引数
*
* @return	Bmp* : 変換後のビットマップ文字の配列へのポインタ。
*
* @note		指定のフォントで出来たビットマップ文字列を作成し、そのポインタを返す。\n
* 			CreateBmpStringで得られたBmpのポインタを返す。\n
*			※ワイド文字(char)/Unocode文字(wchar_t)両対応。\n
*			※コンパイル時にTCHARがワイド文字(char)かUnocode文字(wchar_t)に切替わる。\n
*/
Bmp* CreateBmpStringF(const TCHAR* _font_name, int _font_size, int _bold, int _ggo, bool _zenkaku, const TCHAR* _format, ...)
{
	Bmp* p_bmp = nullptr;
	va_list ap;
	va_start(ap, _format);
	//VPrintStringFA(_zenkaku, _format, ap);
#ifdef UNICODE
	size_t length = _vscwprintf(_format, ap) + 1;	//'\0'含まないので＋１している
	wchar_t* buf = (wchar_t*)_malloca(length * sizeof(wchar_t));
	vswprintf_s(buf, length, _format, ap);
	if (_zenkaku == true) {
		//全て全角に変換してから生成
		wchar_t* p = HanToZenW(buf);
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, _ggo, p);
		free(p);
	}
	else {
		//半角のまま生成
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, _ggo, buf);
	}
#else
	//VPrintStringFA(_zenkaku, _format, ap);
	size_t length = _vscprintf(_format, ap) + 1;	//'\0'含まないので＋１している
	char* buf = (char*)_malloca(length);
	vsprintf_s(buf, length, _format, ap);
	if (_zenkaku == true) {
		//全て全角に変換してから生成
		char* p = HanToZen(buf);
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, _ggo, p);
		free(p);
	}
	else {
		//半角のまま生成
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, _ggo, buf);
	}
#endif // UNICODE
	va_end(ap);
	return p_bmp;
}	//CreateBmpStringF

Bmp* CreateBmpStringF(const TCHAR* _font_name, int _font_size, int _bold, bool _zenkaku, const TCHAR* _format, ...)
{
	Bmp* p_bmp = nullptr;
	va_list ap;
	va_start(ap, _format);
	//VPrintStringFA(_zenkaku, _format, ap);
#ifdef UNICODE
	size_t length = _vscwprintf(_format, ap) + 1;	//'\0'含まないので＋１している
	wchar_t* buf = (wchar_t*)_malloca(length * sizeof(wchar_t));
	vswprintf_s(buf, length, _format, ap);
	if (_zenkaku == true) {
		//全て全角に変換してから生成
		wchar_t* p = HanToZenW(buf);
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, GGO_BITMAP, p);
		free(p);
	}
	else {
		//半角のまま生成
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, GGO_BITMAP, buf);
	}
#else
	//VPrintStringFA(_zenkaku, _format, ap);
	size_t length = _vscprintf(_format, ap) + 1;	//'\0'含まないので＋１している
	char* buf = (char*)_malloca(length);
	vsprintf_s(buf, length, _format, ap);
	if (_zenkaku == true) {
		//全て全角に変換してから生成
		char* p = HanToZen(buf);
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, GGO_BITMAP, p);
		free(p);
	}
	else {
		//半角のまま生成
		p_bmp = CreateBmpString(_font_name, _font_size, _bold, GGO_BITMAP, buf);
	}
#endif // UNICODE
	va_end(ap);
	return p_bmp;
}	//CreateBmpStringF

//================================================================
// キー入力関係
//================================================================
/**
 * @brief	キー情報リセット
 */
void ResetKeyMap(void)
{
	for (int count = 0; count < 8; count++) {
		g_ConioKeyMap[count] = 0;
	}
}

/**
 * @brief	キーボード・マウス入力
 *
 * @param	port [入力] ポート番号(P*_*)
 * @return	入力値
 */
int InputKeyMouse(int port)
{
	DWORD event = 0;
	DWORD read = 0;
	volatile PINPUT_RECORD input_record;
	KEY_EVENT_RECORD* key_event;
	MOUSE_EVENT_RECORD* mouse_event;

	// キーボードイベントチェック
#ifdef USED2D
	if (GetNumberOfConsoleInputEvents(g_InputHandleD2D, &event) && event)
#else
	if (GetNumberOfConsoleInputEvents(g_InputHandle, &event) && event)
#endif // USED2D
	{
		read = 0;
		input_record = (PINPUT_RECORD)_malloca(event * sizeof(INPUT_RECORD));

#ifdef USED2D
		if (ReadConsoleInput(g_InputHandleD2D, input_record, event, &read) && read)
#else
		if (ReadConsoleInput(g_InputHandle, input_record, event, &read) && read)
#endif // USED2D
		{
			//input_record = input_record;
			for (unsigned int count = 0; count < read; count++, input_record++) {
				switch (input_record->EventType) {
				case KEY_EVENT: {
					key_event = &input_record->Event.KeyEvent;
					if (key_event->wVirtualKeyCode > 0x0FF) {
						break;
					}
					if (key_event->bKeyDown) {
						g_ConioKeyMap[key_event->wVirtualKeyCode >> 5] |= (0x01 << (key_event->wVirtualKeyCode & 31));
					}
					else {
						g_ConioKeyMap[key_event->wVirtualKeyCode >> 5] &= ~(0x01 << (key_event->wVirtualKeyCode & 31));
					}
					if (key_event->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
						g_ConioKeyMap[VK_MENU >> 5] |= (0x01 << (VK_MENU & 31));
					}
					else {
						g_ConioKeyMap[VK_MENU >> 5] &= ~(0x01 << (VK_MENU & 31));
					}
					if (key_event->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
						g_ConioKeyMap[VK_CONTROL >> 5] |= (0x01 << (VK_CONTROL & 31));
					}
					else {
						g_ConioKeyMap[VK_CONTROL >> 5] &= ~(0x01 << (VK_CONTROL & 31));
					}
					if (key_event->dwControlKeyState & SHIFT_PRESSED) {
						g_ConioKeyMap[VK_SHIFT >> 5] |= (0x01 << (VK_SHIFT & 31));
					}
					else {
						g_ConioKeyMap[VK_SHIFT >> 5] &= ~(0x01 << (VK_SHIFT & 31));
					}
					break;
				}
				case MOUSE_EVENT: {
					mouse_event = &input_record->Event.MouseEvent;
					g_ConioMousePosition = mouse_event->dwMousePosition;
					if (mouse_event->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
						g_ConioKeyMap[VK_LBUTTON >> 5] |= (0x01 << (VK_LBUTTON & 31));
					}
					else {
						g_ConioKeyMap[VK_LBUTTON >> 5] &= ~(0x01 << (VK_LBUTTON & 31));
					}
					if (mouse_event->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) {
						g_ConioKeyMap[VK_MBUTTON >> 5] |= (0x01 << (VK_MBUTTON & 31));
					}
					else {
						g_ConioKeyMap[VK_MBUTTON >> 5] &= ~(0x01 << (VK_MBUTTON & 31));
					}
					if (mouse_event->dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
						g_ConioKeyMap[VK_RBUTTON >> 5] |= (0x01 << (VK_RBUTTON & 31));
					}
					else {
						g_ConioKeyMap[VK_RBUTTON >> 5] &= ~(0x01 << (VK_RBUTTON & 31));
					}
					break;
				}
				default:
					break;
				}
			}
		}
	}

	// マウス座標を返す
	switch (port) {
	case PM_CURX:
		return g_ConioMousePosition.X + 1;
	case PM_CURY:
		return g_ConioMousePosition.Y + 1;
	default:
		break;
	}
	// キー状態を返す
	return (g_ConioKeyMap[(port & 0x0FF) >> 5] & (0x01 << (port & 31))) != 0;
}

//================================================================
// 拡張キー入力
//================================================================
/**
* @brief	単一キーの入力。※このコンソールウィンドウにフォーカスが当たっている時だけ入力する。
*
* @return	SHORT\tWin32apiの"GetAsyncKeyState()"と同じ
*/
SHORT GetKey(int _vk)
{
	if (GetForegroundWindow() != g_hConWnd) {
		//フォーカスが外れている。
		return 0;
	}
	//フォーカスが当たっている時だけ入力する。
	return GetAsyncKeyState(_vk);
}	//GetKey

/**
* @brief	キー入力を待つ。※このコンソールウィンドウにフォーカスが当たっている時だけ入力する。
*
* @return	SHORT	Win32apiの"GetAsyncKeyState()"と同じ
*/
SHORT WaitKey(int _vk)
{
	SHORT k = 0;
	//フォーカスが当たっている時だけ入力する。
	do {
		k = GetKey(_vk);
	} while (!k);
	return k;
}	//GetKey

/**
* @brief	全てのキーの入力。※このコンソールウィンドウにフォーカスが当たっている時だけ入力する。\n
* この関数を呼び出した後、全てのキー入力がChkKeyEdge()/ChkKeyPress()で判定できる。
*
* @param	_vk	仮想キーコード：Pressキー（押下キー）入力判定する
* @param	_chktype	キー入力判定方法の選択：1=Edge判定/0=Press判定
*
* @return	int\n
*	キーＯＮなら１\n
*	キーＯＦＦなら０
*/
int GetKeyEx(int _vk, int _chktype)
{
	GetKeyAll();
	if (_chktype == 1) {
		//Edgeキー入力判定
		return ChkKeyEdge(_vk);
	}
	//Pressキー入力判定
	return ChkKeyPress(_vk);
}
/**
* @brief	全てのキーの入力。※このコンソールウィンドウにフォーカスが当たっている時だけ入力する。\n
* この関数を呼び出した後、全てのキー入力がChkKeyEdge()/ChkKeyPress()で判定できる。
*
* @return
*	なし
*/
void GetKeyAll(void)
{
	if (GetForegroundWindow() != g_hConWnd) {
		//フォーカスが外れている。
		return;
	}
	//フォーカスが当たっている時だけ入力する。
	//全キーを入力しEdgeとPressを作る。
	for (int vk = 0; vk < NUM_KEYS; vk++) {
		//現在の押し下げ状態をセットする。
		g_KeyPress[vk] = (int)((GetAsyncKeyState(vk) & (~0x1)) != 0);
		//前回ＯＦＦ⇒今回ＯＮの時だけＯＮにする。
		g_KeyEdge[vk] = (int)((g_KeyPress[vk] != 0) && (g_KeyLast[vk] == 0));
		//前回の状態を更新する
		g_KeyLast[vk] = g_KeyPress[vk];
	}
}	//GetKeyAll

/**
* @brief	Edgeキー入力判定：GetKeyAll()で入力したキー情報についてEdgeキー（トリガーキー）入力判定する
*
* @param	vk	仮想キーコード
*
* @return	int\n
*	キーＯＮなら１\n
*	キーＯＦＦなら０
*/
int ChkKeyEdge(int _vk) {
	return g_KeyEdge[_vk & 0xFF];
}	//ChkKeyEdge

/**
* @brief	Pressキー入力判定：GetKeyAll()で入力したキー情報についてPressキー（押下キー）入力判定する
*
* @param	vk	仮想キーコード
*
* @return	int\n
*	キーＯＮなら１\n
*	キーＯＦＦなら０
*/
int ChkKeyPress(int _vk) {
	return g_KeyPress[_vk & 0xFF];
}	//ChkKeyPress

//================================================================
// ジョイパッド入力関係
//================================================================
/**
 * @brief	ジョイパッド入力
 *
 * @param	port [入力] ポート番号(P*_*)
 * @return	入力値
 */
int InputJoystick(int port)
{
	JOYINFO	joy_info;
	int id;
	int func;

	// ゲームパッド入力
	if ((port & 0xfe00) == 0x0200) {
		id = (port & 0x01f0) >> 4;
		func = port & 0x0f;

		switch (func) {
		case 0:
		case 1:
		case 2:
		case 3:
			if (joyGetPos(id, &joy_info) != JOYERR_NOERROR) {
				return -1;
			}
			switch (func) {
			case 0:
				return joy_info.wXpos;
			case 1:
				return joy_info.wYpos;
			case 2:
				return joy_info.wZpos;
			case 3:
				return joy_info.wButtons;
			}
			break;
		default:
			break;
		}
		return 0;
	}
	return -1;
}

#if FALSE
/**
 * @brief	ジョイパッド入力(XInput対応)
 *
 * @param	port [入力] ポート番号(P*_*)
 * @return	入力値
 */
int InputJoystickX(int port)
{
	XINPUT_STATE controller_state[4];	// XInputコントローラ情報
	int id;
	int func;
	unsigned int  result;

	// ゲームパッド入力
	if ((port & 0xfe00) == 0x0200) {	//0x200～0x236
		id = (port & 0x01f0) >> 4;		//bit4～8(5bits)がコントローラ番号
		func = port & 0x0f;				//bit0～3(4bits)がボタン番号

		// Simply get the state of the controller from XInput.
		result = XInputGetState(id, &controller_state[id]);
		if (result == ERROR_SUCCESS) {
			switch (func) {
			case 0:
				return controller_state[id].Gamepad.sThumbLX;
			case 1:
				return controller_state[id].Gamepad.sThumbLY;
			case 2:
				return controller_state[id].Gamepad.bLeftTrigger;
			case 3:
				return controller_state[id].Gamepad.sThumbRX;
			case 4:
				return controller_state[id].Gamepad.sThumbRY;
			case 5:
				return controller_state[id].Gamepad.bRightTrigger;
			case 6:
				return controller_state[id].Gamepad.wButtons;
			}
		}
	}
	return -1;
}
#else	//FALSE
/**
 * @brief	ジョイパッド入力(XInput対応)
 *
 * @param	id		コントローラ番号(ID)：０～
 * @param	port [入力] ポート番号(P*_*)
 *
 * @retval	0		正常終了
 * @retval	1以上	Joystickの入力値
 * @retval	-1		エラー
 *
 * @note
 *	LRのスティックは中心から左右に移動する際のデッドゾーンが用意されている
 *	（デッドゾーンは左右に移動したとみなさないエリアのこと）
 *	 #define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
 *	 #define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
 */
int InputJoystickX(int id, int port)
{
	XINPUT_STATE controller_state;	// XInputコントローラ情報

	if (XInputGetState(id, &controller_state) != ERROR_SUCCESS) {
		return -1;
	}

	switch (port) {
	case PJX_LXPOS:
		return controller_state.Gamepad.sThumbLX;
	case PJX_LYPOS:
		return controller_state.Gamepad.sThumbLY;
	case PJX_LTRG:
		return controller_state.Gamepad.bLeftTrigger;
	case PJX_RXPOS:
		return controller_state.Gamepad.sThumbRX;
	case PJX_RYPOS:
		return controller_state.Gamepad.sThumbRY;
	case PJX_RTRG:
		return controller_state.Gamepad.bRightTrigger;
	case PJX_BTNS:
		if (controller_state.dwPacketNumber) {
			return controller_state.Gamepad.wButtons;
		}
	}
	return 0;
}	//InputJoystickX
#endif // FALSE

//================================================================
// サウンド関係
//================================================================
/**
* @brief	サウンド ファイルを開く
*
* @param	path [入力] ファイル名
* @retval	非0	サウンド ハンドル
* @retval	0	エラー
*/
int* MciOpenSound(const char* path)
{
	union {
		MCI_WAVE_OPEN_PARMSA	wave_param;
		MCI_OPEN_PARMSA			open_param;
	} MciParam;
	TCHAR error_str[256];
	const char midi_ext[] = ".mid|.midi|.rmi";
	const char wave_ext[] = ".wav|.wave";
	const char mp3_ext[] = ".mp3";
	char input_ext[_MAX_EXT];
	DWORD_PTR mci_command;
	MCIERROR mci_error;
	MciSoundInfo* sound_info;

	sound_info = (MciSoundInfo*)calloc(1, sizeof(MciSoundInfo));
	if (sound_info == NULL) {
		return 0;
	}
	ZeroMemory(sound_info, sizeof(*sound_info));
	ZeroMemory(&MciParam, sizeof(MciParam));
	_splitpath_s(path, NULL, 0, NULL, 0, NULL, 0, input_ext, sizeof(input_ext));
	_strlwr_s(input_ext, strlen(input_ext) + 1);
	mci_command = MCI_OPEN_TYPE | MCI_OPEN_ELEMENT;
	if (strstr(midi_ext, input_ext)) {
		sound_info->device_type = MCI_DEVTYPE_SEQUENCER;
		lstrcpynA(sound_info->path, path, MAX_PATH);
		MciParam.open_param.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_SEQUENCER;
		MciParam.open_param.lpstrElementName = sound_info->path;
		mci_command |= MCI_OPEN_TYPE_ID;
	}
	else if (strstr(wave_ext, input_ext)) {
		sound_info->device_type = MCI_DEVTYPE_WAVEFORM_AUDIO;
		lstrcpynA(sound_info->path, path, MAX_PATH);
		MciParam.wave_param.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_WAVEFORM_AUDIO;
		MciParam.wave_param.lpstrElementName = sound_info->path;
		mci_command |= MCI_OPEN_TYPE_ID;
		// MciParam.wave_param.dwBufferSeconds	= 60;
		// mci_command |= MCI_WAVE_OPEN_BUFFER;
	}
	else if (strstr(mp3_ext, input_ext)) {
		sound_info->device_type = MCI_DEVTYPE_DIGITAL_VIDEO;
		lstrcpynA(sound_info->path, path, MAX_PATH);
		MciParam.open_param.lpstrDeviceType = "MPEGVideo";
		MciParam.open_param.lpstrElementName = sound_info->path;
	}
	else {
		free(sound_info);
		return 0;
	}
	mci_error = mciSendCommandA(0, MCI_OPEN, mci_command, (DWORD_PTR)&MciParam);
	if (mci_error != 0) {
		free(sound_info);
		mciGetErrorString(mci_error, error_str, sizeof(error_str) / sizeof(TCHAR));
		MessageBox(NULL, error_str, NULL, MB_ICONWARNING);
		return 0;
	}
	sound_info->device_id = MciParam.open_param.wDeviceID;
	return (int*)sound_info;
}	//MciOpenSound

/**
 * @brief	サウンド ファイルを閉じる
 *
 * @param	sound_id [入力] サウンド ハンドル
 */
void MciCloseSound(int* sound_id)
{
	MciSoundInfo* sound_info;

	if (!sound_id) {
		return;
	}
	sound_info = (MciSoundInfo*)sound_id;
	if (sound_info->device_id) {
		mciSendCommand(sound_info->device_id, MCI_CLOSE, 0, 0);
		sound_info->device_id = 0;
	}
	free(sound_info);
}	//MciCloseSound

/**
 * @brief	サウンドを再生する
 *
 * @param	sound_id [入力] サウンド ハンドル
 * @param	repeat [入力] ループ有無
 */
void MciPlaySound(int* sound_id, int repeat)
{
	MciSoundInfo* sound_info;
	DWORD_PTR mci_command;
	MCI_PLAY_PARMS play_param;

	if (!sound_id) {
		return;
	}
	sound_info = (MciSoundInfo*)sound_id;
	if (!sound_info->device_id) {
		return;
	}
	sound_info->repeat = repeat;
	ZeroMemory(&play_param, sizeof(play_param));
	mci_command = 0;
	if (repeat) {
		switch (sound_info->device_type) {
		case MCI_DEVTYPE_DIGITAL_VIDEO:
			mci_command |= (MCI_FROM | MCI_DGV_PLAY_REPEAT);
			play_param.dwFrom = 0;
			break;
		case MCI_DEVTYPE_SEQUENCER:
		case MCI_DEVTYPE_WAVEFORM_AUDIO:
			break;
		default:
			break;
		}
	}
	mciSendCommand(sound_info->device_id, MCI_SEEK, MCI_SEEK_TO_START, 0);
	mciSendCommand(sound_info->device_id, MCI_PLAY, mci_command, (DWORD_PTR)&play_param);
}	//MciPlaySound

/**
 * @brief	サウンド再生を停止する
 *
 * @param	sound_id [入力] サウンド ハンドル
 */
void MciStopSound(int* sound_id)
{
	MciSoundInfo* sound_info;

	if (!sound_id) {
		return;
	}
	sound_info = (MciSoundInfo*)sound_id;
	if (!sound_info->device_id) {
		return;
	}
	sound_info->repeat = 0;
	mciSendCommand(sound_info->device_id, MCI_STOP, MCI_WAIT, 0);
	mciSendCommand(sound_info->device_id, MCI_SEEK, MCI_SEEK_TO_START, 0);
}	//MciStopSound

/**
 * @brief	サウンド再生状態の取得
 *
 * @param	sound_id [入力] サウンド ハンドル
 * @return	再生中ならば 0 以外を返す
 */
int MciCheckSound(int* sound_id)
{
	MciSoundInfo* sound_info;
	MCI_STATUS_PARMS status_param;

	if (!sound_id) {
		return 0;
	}
	sound_info = (MciSoundInfo*)sound_id;
	if (!sound_info->device_id) {
		return 0;
	}
	ZeroMemory(&status_param, sizeof(status_param));
	status_param.dwItem = MCI_STATUS_MODE;
	if (mciSendCommand(sound_info->device_id, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&status_param)) {
		return 0;
	}
	return status_param.dwReturn == MCI_MODE_PLAY;
}	//MciCheckSound

/**
 * @brief	ループ再生の強制更新
 *
 * @param	sound_id [入力] サウンド ハンドル
 * @note
 *	サウンドが停止したら同じサウンドを再生する
 *	更新時は音量設定が標準値に戻るので再設定を行う必要がある
 */
void MciUpdateSound(int* sound_id)
{
	MciSoundInfo* sound_info;
	MCI_STATUS_PARMS status_param;

	if (!sound_id) {
		return;
	}
	sound_info = (MciSoundInfo*)sound_id;
	if (!sound_info->device_id || !sound_info->repeat) {
		return;
	}

	switch (sound_info->device_type) {
	case MCI_DEVTYPE_DIGITAL_VIDEO:
		break;
	case MCI_DEVTYPE_SEQUENCER:
	case MCI_DEVTYPE_WAVEFORM_AUDIO:
		ZeroMemory(&status_param, sizeof(status_param));
		status_param.dwItem = MCI_STATUS_MODE;
		if (!mciSendCommand(sound_info->device_id, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&status_param)) {
			if (status_param.dwReturn == MCI_MODE_NOT_READY) {
			}
			else if (status_param.dwReturn == MCI_MODE_STOP) {
				if (sound_info->repeat > 0) {
					mciSendCommand(sound_info->device_id, MCI_SEEK, MCI_SEEK_TO_START, 0);
					mciSendCommand(sound_info->device_id, MCI_PLAY, 0, 0);
				}
			}
		}
		break;
	default:
		break;
	}
}	//MciUpdateSound

/**
 * @brief	再生音量を設定する
 *
 * @param	sound_id [入力] サウンド ハンドル
 * @param	percent [入力] 音量 (0 ～ 100)
 */
void MciSetVolume(int* sound_id, int percent)
{
	MciSoundInfo* sound_info;
	MCI_DGV_SETAUDIO_PARMS	audio_param;
	DWORD volume;

	if (!sound_id) {
		return;
	}
	sound_info = (MciSoundInfo*)sound_id;
	if (!sound_info->device_id) {
		return;
	}
	switch (sound_info->device_type) {
	case MCI_DEVTYPE_DIGITAL_VIDEO:
		ZeroMemory(&audio_param, sizeof(audio_param));
		audio_param.dwItem = MCI_DGV_SETAUDIO_VOLUME;
		audio_param.dwValue = percent * 10;
		mciSendCommand(sound_info->device_id, MCI_SETAUDIO, MCI_DGV_SETAUDIO_ITEM | MCI_DGV_SETAUDIO_VALUE, (DWORD_PTR)&audio_param);
		break;
	case MCI_DEVTYPE_SEQUENCER:
		volume = 0x0ffff * percent / 100;
		midiOutSetVolume(0, (DWORD)MAKELONG(volume, volume));
		break;
	case MCI_DEVTYPE_WAVEFORM_AUDIO:
		volume = 0x0ffff * percent / 100;
		waveOutSetVolume(0, (DWORD)MAKELONG(volume, volume));
		break;
	default:
		break;
	}
}	//MciSetVolume

#ifndef USED2D
//================================================================
// ２４ビット色画像
//================================================================
/**
* @brief	２４bitカラー画像用(バッファ等)の初期化
*/
static void init_24bit_color_image(void)
{
	if (g_FrameBufferFull == NULL) {
		//24ビット値の配列として確保
		g_FrameBufferFull = (char*)calloc(g_ScreenBufferSize.X * g_ScreenBufferSize.Y, sizeof(char) * 3);
	}
	if (g_ScreenBufferFull == NULL) {
		// １行分のサイズ（バイト数算出）【注】１行の最後は改行コード'\n'が１文字入っているので、最後に＋１している。
		g_ScreenBufferLineStride = (g_ScreenBufferSize.X * PIXEL24ESCSIZE + 1);
		// スクリーンバッファ(フルカラー用)を生成
		g_ScreenBufferFull = (char*)calloc(g_ScreenBufferLineStride * g_ScreenBufferSize.Y, sizeof(char));
		_ASSERT(g_ScreenBufferFull);
		char* out_buf = g_ScreenBufferFull;
		// フルカラー用にエスケープシーケンス文字列を設定
		for (int y = 0; y < g_ScreenBufferSize.Y; y++) {
			for (int x = 0; x < g_ScreenBufferSize.X; x++) {
				//色コードＲＧＢ値(各ＲＧＢ値はASCII文字の数字３ケタ)を"000"にした、１ピクセル分エスケープシーケンス文字列を書き込む。
				//sprintf_s(out_buf, PIXEL24ESCSIZE, pixel24bitEsc);	//"\x1b[48;2;000;000;000m "＝20文字
				memcpy(out_buf, pixel24bitEsc, PIXEL24ESCSIZE);
				out_buf += PIXEL24ESCSIZE;	//１ピクセル分ポインタを進める。
			}
			*out_buf = '\x0a';	//１行分の最後に改行コードLF'0A'を書き込む
			out_buf++;	//ポインタを次の行の先頭に進める
		}
		out_buf--;	//最後に'\n'が入っているので１文字戻す。
		*out_buf = '\0';	//最後の１文字を終端文字'\0'に書き換える。
	}
}	//init_24bit_color_image

/**
 * @brief	24ビット/Pixel画像の出力
 *
 * @param	buf [入力] RGB画像データ配列のポインタ(画面サイズ以上のバッファ必要)
 *
 * @note
 *	RGB画像データ配列はスクリーンの横幅×縦幅のバイト数以上の配列とし、
 *	配列の中身はRGB各1バイト(合計3バイト)を1画素としたデータにする。
 *	全ての画素は連続している必要あり。
 *	例)横80文字×縦25行の場合、80x25=200バイト以上の配列を渡す
 */
void PrintImage(const char* _buf)
{
	if ((_buf == NULL) || (g_ScreenBufferFull == NULL)) {
		return;
	}
	DWORD write_num;
	const unsigned char* in_buf = (const unsigned char*)_buf;	//CharRGBconvTBL[][]のindexとして扱うので、符号無しにしている。
	char* out_buf = g_ScreenBufferFull;
	for (int y = 0; y < g_ScreenBufferSize.Y; y++) {
		for (int x = 0; x < g_ScreenBufferSize.X; x++) {
			// R設定
			out_buf[7 + 0] = CharRGBconvTBL[0][*in_buf];	//0x30 + (*in_buf / 100);
			out_buf[7 + 1] = CharRGBconvTBL[1][*in_buf];	//0x30 + (*in_buf % 100 / 10);
			out_buf[7 + 2] = CharRGBconvTBL[2][*in_buf];	//0x30 + (*in_buf % 10);
			in_buf++;
			// G設定
			out_buf[7 + 4] = CharRGBconvTBL[0][*in_buf];	//0x30 + (*in_buf / 100);
			out_buf[7 + 5] = CharRGBconvTBL[1][*in_buf];	//0x30 + (*in_buf % 100 / 10);
			out_buf[7 + 6] = CharRGBconvTBL[2][*in_buf];	//0x30 + (*in_buf % 10);
			in_buf++;
			// B設定
			out_buf[7 + 8] = CharRGBconvTBL[0][*in_buf];	//0x30 + (*in_buf / 100); 
			out_buf[7 + 9] = CharRGBconvTBL[1][*in_buf];	//0x30 + (*in_buf % 100 / 10);
			out_buf[7 + 10] = CharRGBconvTBL[2][*in_buf];	//0x30 + (*in_buf % 10);
			in_buf++;
			out_buf += PIXEL24ESCSIZE;
		}
		out_buf++;
	}
	WriteConsoleA(g_DisplayHandle[g_SwapFlg], g_ScreenBufferFull, (g_ScreenBufferSize.X * PIXEL24ESCSIZE) * g_ScreenBufferSize.Y + (g_ScreenBufferSize.Y - 1), &write_num, NULL);
}	//PrintImage
#endif // !USED2D

//================================================================
//　256色画像【工事中】仕様が変わる可能性が大きい
//================================================================
/**
* ２５６色パレットと２５６色画像について【工事中】
*
* コンソールの２５６色対応：
* Windowsコンソールには文字色・背景色に通常１６色である。
* 現在(2022)のWindowsコンソールはASNIエスケープシーケンスに対応しているので、２５６色のパレットを設定することも出来る。
* 但し、Win32のコンソールAPIには２５６色機能はないので、色指定にはエスケープシーケンスを使う必要がある。
*
* パレット：
* 最初の１６色が通常の１６色に対応する。
* １７色～２５６色が追加の色になる。
*/

/**
* @brief	ANSI color 256.\n
* COLORREF : #00BBGGRR
*/
const COLORREF ANSI_PAL256_COLOR[NUM_ANSI_PAL] = {
0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xC0C0C0,0x808080,0xFF0000,0x00FF00,0xFFFF00,0x0000FF,0xFF00FF,0x00FFFF,0xFFFFFF,
0x000000,0x00005F,0x000087,0x0000AF,0x0000D7,0x0000FF,0x005F00,0x005F5F,0x005F87,0x005FAF,0x005FD7,0x005FFF,0x008700,0x00875F,0x008787,0x0087AF,
0x0087D7,0x0087FF,0x00AF00,0x00AF5F,0x00AF87,0x00AFAF,0x00AFD7,0x00AFFF,0x00D700,0x00D75F,0x00D787,0x00D7AF,0x00D7D7,0x00D7FF,0x00FF00,0x00FF5F,
0x00FF87,0x00FFAF,0x00FFD7,0x00FFFF,0x5F0000,0x5F005F,0x5F0087,0x5F00AF,0x5F00D7,0x5F00FF,0x5F5F00,0x5F5F5F,0x5F5F87,0x5F5FAF,0x5F5FD7,0x5F5FFF,
0x5F8700,0x5F875F,0x5F8787,0x5F87AF,0x5F87D7,0x5F87FF,0x5FAF00,0x5FAF5F,0x5FAF87,0x5FAFAF,0x5FAFD7,0x5FAFFF,0x5FD700,0x5FD75F,0x5FD787,0x5FD7AF,
0x5FD7D7,0x5FD7FF,0x5FFF00,0x5FFF5F,0x5FFF87,0x5FFFAF,0x5FFFD7,0x5FFFFF,0x870000,0x87005F,0x870087,0x8700AF,0x8700D7,0x8700FF,0x875F00,0x875F5F,
0x875F87,0x875FAF,0x875FD7,0x875FFF,0x878700,0x87875F,0x878787,0x8787AF,0x8787D7,0x8787FF,0x87AF00,0x87AF5F,0x87AF87,0x87AFAF,0x87AFD7,0x87AFFF,
0x87D700,0x87D75F,0x87D787,0x87D7AF,0x87D7D7,0x87D7FF,0x87FF00,0x87FF5F,0x87FF87,0x87FFAF,0x87FFD7,0x87FFFF,0xAF0000,0xAF005F,0xAF0087,0xAF00AF,
0xAF00D7,0xAF00FF,0xAF5F00,0xAF5F5F,0xAF5F87,0xAF5FAF,0xAF5FD7,0xAF5FFF,0xAF8700,0xAF875F,0xAF8787,0xAF87AF,0xAF87D7,0xAF87FF,0xAFAF00,0xAFAF5F,
0xAFAF87,0xAFAFAF,0xAFAFD7,0xAFAFFF,0xAFD700,0xAFD75F,0xAFD787,0xAFD7AF,0xAFD7D7,0xAFD7FF,0xAFFF00,0xAFFF5F,0xAFFF87,0xAFFFAF,0xAFFFD7,0xAFFFFF,
0xD70000,0xD7005F,0xD70087,0xD700AF,0xD700D7,0xD700FF,0xD75F00,0xD75F5F,0xD75F87,0xD75FAF,0xD75FD7,0xD75FFF,0xD78700,0xD7875F,0xD78787,0xD787AF,
0xD787D7,0xD787FF,0xD7AF00,0xD7AF5F,0xD7AF87,0xD7AFAF,0xD7AFD7,0xD7AFFF,0xD7D700,0xD7D75F,0xD7D787,0xD7D7AF,0xD7D7D7,0xD7D7FF,0xD7FF00,0xD7FF5F,
0xD7FF87,0xD7FFAF,0xD7FFD7,0xD7FFFF,0xFF0000,0xFF005F,0xFF0087,0xFF00AF,0xFF00D7,0xFF00FF,0xFF5F00,0xFF5F5F,0xFF5F87,0xFF5FAF,0xFF5FD7,0xFF5FFF,
0xFF8700,0xFF875F,0xFF8787,0xFF87AF,0xFF87D7,0xFF87FF,0xFFAF00,0xFFAF5F,0xFFAF87,0xFFAFAF,0xFFAFD7,0xFFAFFF,0xFFD700,0xFFD75F,0xFFD787,0xFFD7AF,
0xFFD7D7,0xFFD7FF,0xFFFF00,0xFFFF5F,0xFFFF87,0xFFFFAF,0xFFFFD7,0xFFFFFF,0x080808,0x121212,0x1C1C1C,0x262626,0x303030,0x3A3A3A,0x444444,0x4E4E4E,
0x585858,0x626262,0x6C6C6C,0x767676,0x808080,0x8A8A8A,0x949494,0x9E9E9E,0xA8A8A8,0xB2B2B2,0xBCBCBC,0xC6C6C6,0xD0D0D0,0xDADADA,0xE4E4E4,0xEEEEEE,
};	//ANSI_PAL256_COLOR

/**
* @brief	ANSI color 256.\n
* RGBQUAD : #00RRGGBB{bb,gg,rr,00}
*/
const RGBQUAD ANSI_PAL256_RGB[NUM_ANSI_PAL] = {
{0x00,0x00,0x00,0},{0x80,0x00,0x00,0},{0x00,0x80,0x00,0},{0x80,0x80,0x00,0},{0x00,0x00,0x80,0},{0x80,0x00,0x80,0},{0x00,0x80,0x80,0},{0xC0,0xC0,0xC0,0},{0x80,0x80,0x80,0},{0xFF,0x00,0x00,0},{0x00,0xFF,0x00,0},{0xFF,0xFF,0x00,0},{0x00,0x00,0xFF,0},{0xFF,0x00,0xFF,0},{0x00,0xFF,0xFF,0},{0xFF,0xFF,0xFF,0},
{0x00,0x00,0x00,0},{0x00,0x00,0x5F,0},{0x00,0x00,0x87,0},{0x00,0x00,0xAF,0},{0x00,0x00,0xD7,0},{0x00,0x00,0xFF,0},{0x00,0x5F,0x00,0},{0x00,0x5F,0x5F,0},{0x00,0x5F,0x87,0},{0x00,0x5F,0xAF,0},{0x00,0x5F,0xD7,0},{0x00,0x5F,0xFF,0},{0x00,0x87,0x00,0},{0x00,0x87,0x5F,0},{0x00,0x87,0x87,0},{0x00,0x87,0xAF,0},
{0x00,0x87,0xD7,0},{0x00,0x87,0xFF,0},{0x00,0xAF,0x00,0},{0x00,0xAF,0x5F,0},{0x00,0xAF,0x87,0},{0x00,0xAF,0xAF,0},{0x00,0xAF,0xD7,0},{0x00,0xAF,0xFF,0},{0x00,0xD7,0x00,0},{0x00,0xD7,0x5F,0},{0x00,0xD7,0x87,0},{0x00,0xD7,0xAF,0},{0x00,0xD7,0xD7,0},{0x00,0xD7,0xFF,0},{0x00,0xFF,0x00,0},{0x00,0xFF,0x5F,0},
{0x00,0xFF,0x87,0},{0x00,0xFF,0xAF,0},{0x00,0xFF,0xD7,0},{0x00,0xFF,0xFF,0},{0x5F,0x00,0x00,0},{0x5F,0x00,0x5F,0},{0x5F,0x00,0x87,0},{0x5F,0x00,0xAF,0},{0x5F,0x00,0xD7,0},{0x5F,0x00,0xFF,0},{0x5F,0x5F,0x00,0},{0x5F,0x5F,0x5F,0},{0x5F,0x5F,0x87,0},{0x5F,0x5F,0xAF,0},{0x5F,0x5F,0xD7,0},{0x5F,0x5F,0xFF,0},
{0x5F,0x87,0x00,0},{0x5F,0x87,0x5F,0},{0x5F,0x87,0x87,0},{0x5F,0x87,0xAF,0},{0x5F,0x87,0xD7,0},{0x5F,0x87,0xFF,0},{0x5F,0xAF,0x00,0},{0x5F,0xAF,0x5F,0},{0x5F,0xAF,0x87,0},{0x5F,0xAF,0xAF,0},{0x5F,0xAF,0xD7,0},{0x5F,0xAF,0xFF,0},{0x5F,0xD7,0x00,0},{0x5F,0xD7,0x5F,0},{0x5F,0xD7,0x87,0},{0x5F,0xD7,0xAF,0},
{0x5F,0xD7,0xD7,0},{0x5F,0xD7,0xFF,0},{0x5F,0xFF,0x00,0},{0x5F,0xFF,0x5F,0},{0x5F,0xFF,0x87,0},{0x5F,0xFF,0xAF,0},{0x5F,0xFF,0xD7,0},{0x5F,0xFF,0xFF,0},{0x87,0x00,0x00,0},{0x87,0x00,0x5F,0},{0x87,0x00,0x87,0},{0x87,0x00,0xAF,0},{0x87,0x00,0xD7,0},{0x87,0x00,0xFF,0},{0x87,0x5F,0x00,0},{0x87,0x5F,0x5F,0},
{0x87,0x5F,0x87,0},{0x87,0x5F,0xAF,0},{0x87,0x5F,0xD7,0},{0x87,0x5F,0xFF,0},{0x87,0x87,0x00,0},{0x87,0x87,0x5F,0},{0x87,0x87,0x87,0},{0x87,0x87,0xAF,0},{0x87,0x87,0xD7,0},{0x87,0x87,0xFF,0},{0x87,0xAF,0x00,0},{0x87,0xAF,0x5F,0},{0x87,0xAF,0x87,0},{0x87,0xAF,0xAF,0},{0x87,0xAF,0xD7,0},{0x87,0xAF,0xFF,0},
{0x87,0xD7,0x00,0},{0x87,0xD7,0x5F,0},{0x87,0xD7,0x87,0},{0x87,0xD7,0xAF,0},{0x87,0xD7,0xD7,0},{0x87,0xD7,0xFF,0},{0x87,0xFF,0x00,0},{0x87,0xFF,0x5F,0},{0x87,0xFF,0x87,0},{0x87,0xFF,0xAF,0},{0x87,0xFF,0xD7,0},{0x87,0xFF,0xFF,0},{0xAF,0x00,0x00,0},{0xAF,0x00,0x5F,0},{0xAF,0x00,0x87,0},{0xAF,0x00,0xAF,0},
{0xAF,0x00,0xD7,0},{0xAF,0x00,0xFF,0},{0xAF,0x5F,0x00,0},{0xAF,0x5F,0x5F,0},{0xAF,0x5F,0x87,0},{0xAF,0x5F,0xAF,0},{0xAF,0x5F,0xD7,0},{0xAF,0x5F,0xFF,0},{0xAF,0x87,0x00,0},{0xAF,0x87,0x5F,0},{0xAF,0x87,0x87,0},{0xAF,0x87,0xAF,0},{0xAF,0x87,0xD7,0},{0xAF,0x87,0xFF,0},{0xAF,0xAF,0x00,0},{0xAF,0xAF,0x5F,0},
{0xAF,0xAF,0x87,0},{0xAF,0xAF,0xAF,0},{0xAF,0xAF,0xD7,0},{0xAF,0xAF,0xFF,0},{0xAF,0xD7,0x00,0},{0xAF,0xD7,0x5F,0},{0xAF,0xD7,0x87,0},{0xAF,0xD7,0xAF,0},{0xAF,0xD7,0xD7,0},{0xAF,0xD7,0xFF,0},{0xAF,0xFF,0x00,0},{0xAF,0xFF,0x5F,0},{0xAF,0xFF,0x87,0},{0xAF,0xFF,0xAF,0},{0xAF,0xFF,0xD7,0},{0xAF,0xFF,0xFF,0},
{0xD7,0x00,0x00,0},{0xD7,0x00,0x5F,0},{0xD7,0x00,0x87,0},{0xD7,0x00,0xAF,0},{0xD7,0x00,0xD7,0},{0xD7,0x00,0xFF,0},{0xD7,0x5F,0x00,0},{0xD7,0x5F,0x5F,0},{0xD7,0x5F,0x87,0},{0xD7,0x5F,0xAF,0},{0xD7,0x5F,0xD7,0},{0xD7,0x5F,0xFF,0},{0xD7,0x87,0x00,0},{0xD7,0x87,0x5F,0},{0xD7,0x87,0x87,0},{0xD7,0x87,0xAF,0},
{0xD7,0x87,0xD7,0},{0xD7,0x87,0xFF,0},{0xD7,0xAF,0x00,0},{0xD7,0xAF,0x5F,0},{0xD7,0xAF,0x87,0},{0xD7,0xAF,0xAF,0},{0xD7,0xAF,0xD7,0},{0xD7,0xAF,0xFF,0},{0xD7,0xD7,0x00,0},{0xD7,0xD7,0x5F,0},{0xD7,0xD7,0x87,0},{0xD7,0xD7,0xAF,0},{0xD7,0xD7,0xD7,0},{0xD7,0xD7,0xFF,0},{0xD7,0xFF,0x00,0},{0xD7,0xFF,0x5F,0},
{0xD7,0xFF,0x87,0},{0xD7,0xFF,0xAF,0},{0xD7,0xFF,0xD7,0},{0xD7,0xFF,0xFF,0},{0xFF,0x00,0x00,0},{0xFF,0x00,0x5F,0},{0xFF,0x00,0x87,0},{0xFF,0x00,0xAF,0},{0xFF,0x00,0xD7,0},{0xFF,0x00,0xFF,0},{0xFF,0x5F,0x00,0},{0xFF,0x5F,0x5F,0},{0xFF,0x5F,0x87,0},{0xFF,0x5F,0xAF,0},{0xFF,0x5F,0xD7,0},{0xFF,0x5F,0xFF,0},
{0xFF,0x87,0x00,0},{0xFF,0x87,0x5F,0},{0xFF,0x87,0x87,0},{0xFF,0x87,0xAF,0},{0xFF,0x87,0xD7,0},{0xFF,0x87,0xFF,0},{0xFF,0xAF,0x00,0},{0xFF,0xAF,0x5F,0},{0xFF,0xAF,0x87,0},{0xFF,0xAF,0xAF,0},{0xFF,0xAF,0xD7,0},{0xFF,0xAF,0xFF,0},{0xFF,0xD7,0x00,0},{0xFF,0xD7,0x5F,0},{0xFF,0xD7,0x87,0},{0xFF,0xD7,0xAF,0},
{0xFF,0xD7,0xD7,0},{0xFF,0xD7,0xFF,0},{0xFF,0xFF,0x00,0},{0xFF,0xFF,0x5F,0},{0xFF,0xFF,0x87,0},{0xFF,0xFF,0xAF,0},{0xFF,0xFF,0xD7,0},{0xFF,0xFF,0xFF,0},{0x08,0x08,0x08,0},{0x12,0x12,0x12,0},{0x1C,0x1C,0x1C,0},{0x26,0x26,0x26,0},{0x30,0x30,0x30,0},{0x3A,0x3A,0x3A,0},{0x44,0x44,0x44,0},{0x4E,0x4E,0x4E,0},
{0x58,0x58,0x58,0},{0x62,0x62,0x62,0},{0x6C,0x6C,0x6C,0},{0x76,0x76,0x76,0},{0x80,0x80,0x80,0},{0x8A,0x8A,0x8A,0},{0x94,0x94,0x94,0},{0x9E,0x9E,0x9E,0},{0xA8,0xA8,0xA8,0},{0xB2,0xB2,0xB2,0},{0xBC,0xBC,0xBC,0},{0xC6,0xC6,0xC6,0},{0xD0,0xD0,0xD0,0},{0xDA,0xDA,0xDA,0},{0xE4,0xE4,0xE4,0},{0xEE,0xEE,0xEE,0},
};

#ifndef USED2D
/**
* @brief	256色バッファ関連の初期化
*
* @note	【工事中】
*/
static void init_256color_image(void)
{
	//256色ピクセルバッファの確保
	int screen_area_pixel_size = g_ScreenBufferSize.X * g_ScreenBufferSize.Y;	//面積
	if (g_FrameBuffer256 == NULL) {
		g_FrameBuffer256 = (BYTE*)calloc(screen_area_pixel_size, sizeof(BYTE));
		_ASSERT(g_FrameBuffer256);
	}
	if (g_ScreenBuffer256 == NULL) {
		g_ScreenBuffer256 = (BYTE*)calloc(screen_area_pixel_size, sizeof(pixel256Esc));	//行末改行必要？不要？
		_ASSERT(g_ScreenBuffer256);
		int idx = 0;
		for (int n = 0; n < screen_area_pixel_size; n++) {
			memcpy(&g_ScreenBuffer256[idx], pixel256Esc, sizeof(pixel256Esc));
			idx += sizeof(pixel256Esc);
		}
	}
}	//init_256color_image

/**
* @brief	現在のシステムパレットに２５６色設定
*
* @param	HANDLE _hCon	コンソールのハンドル
* @param	const COLORREF* _p256
* @param	int _num_pal
*
* @note	【工事中】
*/
static void set_palette256(HANDLE _hCon, const COLORREF* _p256, int _num_pal)
{
	if (_hCon == 0) {
		_hCon = g_DisplayHandle[g_SwapFlg];
	}
	if (_num_pal > 256) {
		_num_pal = 256;
	}
	DWORD wrn;
	const char palsetEsc[] = "\x1b]4;%03d;rgb:%02x/%02x/%02x\x07";
	char str[sizeof(palsetEsc) + 1] = { 0 };
	for (int pn = 0; pn < _num_pal; pn++) {
		//"ESC]4;<pal#>;rgb:RR/GG/BBBEL" RRGGBB:hex
		//sprintf_s(str, 23, "\x1b]4;%03d;rgb:%02x/%02x/%02x\x1b\\", pn, _p256[pn].rgbRed, _p256[pn].rgbGreen, _p256[pn].rgbBlue);
		sprintf_s(str, sizeof(str), palsetEsc,
			pn,
			((DWORD)_p256[pn]) & 0x0000FF, //B
			(((DWORD)_p256[pn]) & 0x00FF00) >> 8,	//G
			(((DWORD)_p256[pn]) & 0xFF0000) >> 16	//R
		);
		WriteConsoleA(_hCon, str, sizeof(palette256Esc), &wrn, NULL);
	}
}	//set_palette256

/**
* @brief	現在のコンソールの色をANSI-256色パレットに初期化
*
* @note	【工事中】
*/
void InitPaletteANSI256(void)
{
	if (g_DisplayHandle[0] != NULL) {
		set_palette256(g_DisplayHandle[0], ANSI_PAL256_COLOR, NUM_ANSI_PAL);
	}
	if (g_DisplayHandle[1] != NULL) {
		set_palette256(g_DisplayHandle[1], ANSI_PAL256_COLOR, NUM_ANSI_PAL);
	}
	set_palette256(GetStdHandle(STD_OUTPUT_HANDLE), ANSI_PAL256_COLOR, NUM_ANSI_PAL);	//現在のコンソールハンドルも初期化しておく
}	//InitPaletteANSI256

/**
* @brief	指定位置への１ピクセル（256色）描画
*
* @param	HANDLE _hCon	コンソールのハンドル
* @param	int _x,_y：座標
* @param	int _palidx：２５６パレット番号
*
* @note		8ビット/ピクセル（256色）を指定の座標へ１ピクセル描画する。\n
*			：
*
* @note	【工事中】
*/
void SetPixel256(int _x, int _y, int _palidx)
{
	DWORD wrn;
	char str[23] = {};
	//書式指定でエスケープシーケンス文字列の指定値にパレット番号を書き込む
	sprintf_s(str, 23, pixel256Esc, _palidx & 0xFF);
	//文字位置指定
	SetConsoleCursorPosition(g_DisplayHandle[g_SwapFlg], COORD{ (SHORT)_x,(SHORT)_y });
	//"\x1b[48;5;000m "
	WriteConsoleA(g_DisplayHandle[g_SwapFlg], str, sizeof(pixel256Esc), &wrn, NULL);
}	//SetPixel256

/**
*
* @note	【工事中】
*/
void SetPixelBuffer256(int _x, int _y, int _palidx)
{
	if (_x >= 0 && _x < g_ScreenBufferSize.X && _y >= 0 && _y < g_ScreenBufferSize.Y) {
		g_FrameBuffer256[_y * g_ScreenBufferSize.X + _x] = _palidx;
	}
}	//SetPixel256

/**
*
* @note	【工事中】
*/
void DrawPixelBuffer256(int _xp, int _yp, int _w, int _h, BYTE* _buf)
{
	for (int y = 0; y < _h; y++) {
		for (int x = 0; x < _w; x++) {
			int xx = x + _xp;
			int yy = y + _yp;
			if (xx >= 0 && xx < g_ScreenBufferSize.X && yy >= 0 && yy < g_ScreenBufferSize.Y) {
				g_FrameBuffer256[yy * g_ScreenBufferSize.X + xx] = *_buf;
			}
			_buf++;
		}
	}
	//
	int screen_area_pixel_size = g_ScreenBufferSize.X * g_ScreenBufferSize.Y;	//面積
	BYTE* dp = &g_ScreenBuffer256[0];
	BYTE* sp = &g_FrameBuffer256[0];
	for (int i = 0; i < screen_area_pixel_size; i++) {
		dp[7] = CharRGBconvTBL[0][*sp];	//'0' + (g_FrameBuffer256[i] / 100 % 10);
		dp[8] = CharRGBconvTBL[1][*sp];	//'0' + (g_FrameBuffer256[i] / 10 % 10);
		dp[9] = CharRGBconvTBL[2][*sp];	//'0' + (g_FrameBuffer256[i] % 10);
		dp += sizeof(pixel256Esc);
		sp++;
	}
	DWORD wrn;
	WriteConsoleA(g_DisplayHandle[g_SwapFlg], g_ScreenBuffer256, screen_area_pixel_size * sizeof(pixel256Esc), &wrn, NULL);
}	//SetPixel256
#endif // !USED2D

#ifdef CONIOEX_DDA_SHAPE
//================================================================
//	図形の描画（DDAで描画）
//================================================================
template<typename T>
inline void swap(T& _a, T& _b) { T tmp = _a; _a = _b; _b = tmp; }
//=== 水平線 ===
/*
* @brief	水平線を描画する
* @param	int _x1,_y1	開始座標
* @param	int _x2		終了Ｘ座標（Ｙ座標は_y1と同じ）
* @param	RGBQUAD _rgb	描画色のRGB値
*/
void DrawLineH(int _x1, int _y1, int _x2, RGBQUAD _rgb)
{
	if (_x2 < _x1) {
		//left<Rightにする
		swap(_x1, _x2);
	}
	for (; _x1 <= _x2; _x1++) {
		DrawPixel(_x1, _y1, _rgb);
	}
}
/*
* @brief	水平線を描画する
* @param	int _x1,_y1	開始座標
* @param	int _x2		終了Ｘ座標（Ｙ座標は_y1と同じ）
* @param	int _cc		カラーコード（パレットの番号）
*/
void DrawLineH(int _x1, int _y1, int _x2, int _cc)
{
	DrawLineH(_x1, _y1, _x2, g_PaletteD2D[_cc % NUM_D2D_PAL]);
}	//DrawLineH
//=== 垂直線 ===
/*
* @brief	垂直線を描画する
* @param	int _x1,_y1	開始座標
* @param	int _y2		終了Ｙ座標（Ｘ座標は_x1と同じ）
* @param	RGBQUAD _rgb	描画色のRGB値
*/
void DrawLineV(int _x1, int _y1, int _y2, RGBQUAD _rgb)
{
	if (_y2 < _y1) {
		//Top<Bottomにする
		swap(_y1, _y2);
	}
	for (; _y1 <= _y2; _y1++) {
		DrawPixel(_x1, _y1, _rgb);
	}
}	//DrawLineV
/*
* @brief	垂直線を描画する
* @param	int _x1,_y1	開始座標
* @param	int _y2		終了Ｙ座標（Ｘ座標は_x1と同じ）
* @param	int _cc		カラーコード（パレットの番号）
*/
void DrawLineV(int _x1, int _y1, int _y2, int _cc)
{
	DrawLineV(_x1, _y1, _y2, g_PaletteD2D[_cc % NUM_D2D_PAL]);
}	//DrawLineV
//=== 45度傾斜線 ===
/*
* @brief	45度の直線の描画
* @param	int _x1,_y2	開始座標
* @param	int _len	長さ
* @param	int _dir	方向
* @param	RGBQUAD _rgb	描画色のRGB値
*/
void DrawLine45(int _x1, int _y1, int _len, int _dir, RGBQUAD _rgb)
{
	//45度
	switch (_dir) {
	case 0:	//右下45度＼
		_len = _x1 + _len;
		for (; _x1 < _len; _x1++, _y1++) {
			DrawPixel(_x1, _y1, _rgb);
		}
		break;
	case 1:	//左下45度／
		_len = _y1 + _len;
		for (; _y1 < _len; _x1--, _y1++) {
			DrawPixel(_x1, _y1, _rgb);
		}
		break;
	case 2:	//右上45度／
		_len = _x1 + _len;
		for (; _x1 < _len; _x1++, _y1--) {
			DrawPixel(_x1, _y1, _rgb);
		}
		break;
	case 3:	//左上45度＼
		_len = _x1 - _len;
		for (; _x1 > _len; _x1--, _y1--) {
			DrawPixel(_x1, _y1, _rgb);
		}
		break;
	}
}
/*
* @brief	45度の直線の描画
* @param	int _x1,_y2	開始座標
* @param	int _len	長さ
* @param	int _dir	方向
* @param	int _cc		カラーコード（パレットの番号）
*/
void DrawLine45(int _x1, int _y1, int _len, int _dir, int _cc)
{
	DrawLine45(_x1, _y1, _len, _dir, g_PaletteD2D[_cc % NUM_D2D_PAL]);
}
//=== 直線(線分) ===
/*
* @brief	直線の描画
* @param	int _x1,_y2	開始座標
* @param	int _x2,_y2	終了座標
* @param	RGBQUAD _rgb	描画色のRGB値
*/
void DrawLine(int _x1, int _y1, int _x2, int _y2, RGBQUAD _rgb)
{
	if (_x1 == _x2 && _y1 == _y2) {
		DrawPixel(_x1, _y1, _rgb);	//点
		return;
	}
	else if (_y1 == _y2) {
		DrawLineH(_x1, _y1, _x2, _rgb);	//水平線
		return;
	}
	else if (_x1 == _x2) {
		DrawLineV(_x1, _y1, _y2, _rgb);	//垂直線
		return;
	}
	//DDA-line
	int dx = abs(_x2 - _x1);	//幅
	int dy = abs(_y2 - _y1);	//高
	int err = dx - dy;	//幅と高の差分(+)なら横長(-)なら縦長
	if (err == 0) {
		int area = 0;
		if (_x2 < _x1)area |= 1;
		if (_y2 < _y1)area |= 2;
		DrawLine45(_x1, _y1, dx, area, _rgb);	//45度
		return;
	}
	int sx = (_x1 < _x2) ? (1) : (-1);	//X方向の符号
	int sy = (_y1 < _y2) ? (1) : (-1);	//Y方向の符号
	do {
		DrawPixel(_x1, _y1, _rgb);
		int e2 = (err << 1);	//
		if (e2 > -dy) {
			err -= dy;
			_x1 += sx;
		}
		if (e2 < dx) {
			err += dx;
			_y1 += sy;
		}
	} while ((_x1 != _x2) || (_y1 != _y2));
	return;
}	//DrawLine
/*
* @brief	直線の描画
* @param	int _x1,_y2	開始座標
* @param	int _x2,_y2	終了座標
* @param	int _cc		カラーコード（パレットの番号）
*/
void DrawLine(int _x1, int _y1, int _x2, int _y2, int _cc)
{
	DrawLine(_x1, _y1, _x2, _y2, g_PaletteD2D[_cc % NUM_D2D_PAL]);
}	//DrawLine
//=== 矩形 ===
/*
* @brief	矩形を描画する
* @param	int _x1,_y1	左上座標
* @param	int _x2,_y2	右下座標
* @param	RGBQUAD _rgb	描画色のRGB値
*/
void DrawRect(int _x1, int _y1, int _x2, int _y2, RGBQUAD _rgb, bool _fill)
{
	if (_fill) {
		if (_y2 < _y1)swap(_y1, _y2);
		for (; _y1 <= _y2; _y1++) {
			DrawLineH(_x1, _y1, _x2, _rgb);	//Top-line
		}
	}
	else {
		DrawLineH(_x1, _y1, _x2, _rgb);	//Top-line
		DrawLineH(_x1, _y2, _x2, _rgb);	//Bottom-line
		DrawLineV(_x1, _y1, _y2, _rgb);	//Left-line
		DrawLineV(_x2, _y1, _y2, _rgb);	//Right-line
	}
}	//DrawRect
/*
* @brief	矩形を描画する
* @param	int _x1,_y1	左上座標
* @param	int _x2,_y2	右下座標
* @param	int _cc		カラーコード（パレットの番号）
*/
void DrawRect(int _x1, int _y1, int _x2, int _y2, int _cc, bool _fill) {
	DrawRect(_x1, _y1, _x2, _y2, g_PaletteD2D[_cc % NUM_D2D_PAL], _fill);
}
//=== 円形 ===
/*
* @brief	円を描画する
* @param	int _cx,_cy	中心座標
* @param	int _r		半径
* @param	RGBQUAD _rgb	描画色のRGB値
*/
void DrawCircle(int _cx, int _cy, int _r, RGBQUAD _rgb, bool _fill)
{
	int D = _r;
	int x = (D - 1);
	int y = 0;
	if (_fill) {
		while (x >= y) {
			//右回りで水平が０度：１回で８ドット（８象限分）を描く
			DrawLineH(_cx, _cy + y, _cx + x,/* _cy + y,*/ _rgb);	//第１象限：　　０°～
			DrawLineH(_cx, _cy + x, _cx + y,/* _cy + x,*/ _rgb);	//第２象限：　４５°～
			DrawLineH(_cx, _cy + x, _cx - y,/* _cy + x,*/ _rgb);	//第３象限：　９０°～
			DrawLineH(_cx, _cy + y, _cx - x,/* _cy + y,*/ _rgb);	//第３象限：１３５°～
			DrawLineH(_cx, _cy - y, _cx - x,/* _cy - y,*/ _rgb);	//第３象限：１８０°～
			DrawLineH(_cx, _cy - x, _cx - y,/* _cy - x,*/ _rgb);	//第３象限：２２５°～
			DrawLineH(_cx, _cy - x, _cx + y,/* _cy - x,*/ _rgb);	//第３象限：２７０°～
			DrawLineH(_cx, _cy - y, _cx + x,/* _cy - y,*/ _rgb);	//第３象限：３１５°～３６０°
			D -= (y << 1);
			if (D <= 0) {
				x--;
				D += (x << 1);
			}
			y++;
		}
	}
	else {
		while (x >= y) {
			//右回りで水平が０度：１回で８ドット（８象限分）を描く
			DrawPixel(_cx + x, _cy + y, _rgb);	//第１象限：　　０°～
			DrawPixel(_cx + y, _cy + x, _rgb);	//第２象限：　４５°～
			DrawPixel(_cx - y, _cy + x, _rgb);	//第３象限：　９０°～
			DrawPixel(_cx - x, _cy + y, _rgb);	//第３象限：１３５°～
			DrawPixel(_cx - x, _cy - y, _rgb);	//第３象限：１８０°～
			DrawPixel(_cx - y, _cy - x, _rgb);	//第３象限：２２５°～
			DrawPixel(_cx + y, _cy - x, _rgb);	//第３象限：２７０°～
			DrawPixel(_cx + x, _cy - y, _rgb);	//第３象限：３１５°～３６０°
			D -= (y << 1);
			if (D <= 0) {
				x--;
				D += (x << 1);
			}
			y++;
		}
	}
}	//DDA_Circle
/*
* @brief	円を描画する
* @param	int _cx,_cy	中心座標
* @param	int _r		半径
* @param	int _cc		カラーコード（パレットの番号）
*/
void DrawCircle(int _cx, int _cy, int _r, int _cc, bool _fill) {
	DrawCircle(_cx, _cy, _r, g_PaletteD2D[_cc % NUM_D2D_PAL], _fill);
}

#endif // !CONIOEX_DDA_SHAPE

//================================================================
//	フレーム同期＆計測用関数
#define	USE_NONE	0
#define	USE_MMSEC	1	//mm(ミリ)秒精度"timeGetTime()"を使う
#define	USE_QPC		2	//μ(マイクロ)秒精度"QueryPerformanceCounter"を使う
#define	USE_RDTSC	3	//"ReaD Time Stamp Counter"を使う（CPUのクロックカウンタ）
#define	FRAME_SYNC	USE_RDTSC	//USE_NONE

#if (FRAME_SYNC==USE_RDTSC)
#include	"intrin.h"
#endif // USE_RDTSC

//フレームスピード計測用
#ifdef _DEBUG
static char dbg_str[4096] = {};	//デバッグ用文字列
static int dbg_frame_count = 0;	//フレーム数
#endif // _DEBUG
static double FPS = 60.0;

#if (FRAME_SYNC==USE_RDTSC)
// 高詳細計測用
static __int64	i64_frequency = 0;
static __int64	i64_t1 = 0;
static __int64	i64_t2 = 0;
static double f_1sec = 0.0;		//１秒のカウント数
static double f_tpf = 0.0;		//１フレームのカウント数（分解能）
#ifdef _DEBUG
static double f_total = 0.0;
#endif // _DEBUG
/*
* フレーム同期
* 高解像度タイム スタンプを使って計測
*/
//初期化
void InitFrameSync(double _FPS)
{
	if (_FPS <= 0) {
		//範囲外は１FPSとする
		_FPS = 1;
	}
	FPS = _FPS;
	timeBeginPeriod(1);	//計測精度を1ミリ秒に設定
	i64_t1 = __rdtsc();
	Sleep(1000 / 10);	//計測基準として一定時間待つ（1/10秒）
	i64_t2 = __rdtsc();
	//1/10秒間のt1～t2間のカウントから１秒間のカウントを算出
	i64_frequency = (i64_t2 - i64_t1) * (__int64)10;	//1513233427/1681604920/1813127620
	f_1sec = (double)i64_frequency;	//１秒間のカウント数（分解能）
	f_tpf = (f_1sec / FPS);	//１フレームのカウント数（分解能）
}
//同期
void FrameSync(void)
{
	//フレーム待ち時間計測
	i64_t2 = __rdtsc();	//現在時間取得
	double f_frame_interval = (double)(i64_t2 - i64_t1);	//フレーム間隔(時間)算出
	double f_wait = (f_tpf - f_frame_interval);	//待ち時間を算出（待ち時間＝１フレームに必要な時間－実際に掛かった時間）
	if (f_wait > 0) {
		__int64 t2 = 0;
		do {
			//Sleep(1);
			t2 = __rdtsc();	//現在時間取得
			//このループの経過時間を測り、待ち時間以下ならループ継続する。
		} while ((double)(t2 - i64_t2) < f_wait);
#ifdef _DEBUG
		//１フレーム時間を積算（１フレーム時間＝前のフレームから今のフレームまでの経過時間＋足りなかった分の待ち時間）
		f_total += (f_frame_interval + f_wait);
	}
	else {
		//待ち時間が無かったので１フレーム時間だけを積算
		f_total += f_frame_interval;
	}
	dbg_frame_count++;	//１秒間のフレーム数カウント(デバッグ用)
	if ((f_total >= f_1sec) && (dbg_frame_count > 0)) {
		double fps = f_1sec / (f_total / (double)dbg_frame_count);
		sprintf_s(dbg_str, sizeof(dbg_str),
			"[fps:%7.2f][count:%3d][1sec:%7.2f][tpf:%7.2f][wait:%7.2f][interval:%7.2f][total:%7.2f]\n",
			(double)fps, (int)dbg_frame_count, (double)f_1sec, (double)f_tpf, (double)f_wait, (double)f_frame_interval, (double)f_total);
		OutputDebugStringA(dbg_str);
		SetCaption(dbg_str);
		dbg_frame_count = 0;
		f_total = 0.0;
#endif // _DEBUG
	}
	i64_t1 = __rdtsc();	//計測開始時間取得
}
#elif (FRAME_SYNC==USE_QPC)
// 高詳細計測用
LARGE_INTEGER	li_frequency = {};
LARGE_INTEGER	li_t1 = {};
LARGE_INTEGER	li_t2 = {};
double f_1sec = 0.0;	//１秒のカウント数
double f_tpf = 0.0;		//１フレームのカウント数（分解能）
#ifdef _DEBUG
double f_total = 0.0;
#endif // _DEBUG
/*
* フレーム同期
* 高解像度タイム スタンプを使って計測
*/
//初期化
void InitFrameSync(double _FPS)
{
	if (_FPS <= 0) {
		//範囲外は１FPSとする
		_FPS = 1;
	}
	FPS = _FPS;
	timeBeginPeriod(1);	//計測精度を1ミリ秒に設定
	//分解能：カウンタの周波数（１秒間に何カウント行うか）を取得
	if (!QueryPerformanceFrequency(&li_frequency)) {
		//失敗した場合は、10,000,000Hz(10MHz)にする。
		li_frequency.QuadPart = 1000 * 1000 * 10;	//分解能＝(1/10,000,000)
	}
	f_1sec = (double)li_frequency.QuadPart;	//１秒間のカウント数（分解能）
	f_tpf = (f_1sec / FPS);	//１フレームのカウント数（分解能）
}
//同期
void FrameSync(void)
{
	//フレーム待ち時間計測
	QueryPerformanceCounter(&li_t2);	//現在時間取得
	double f_frame_interval = (double)(li_t2.QuadPart - li_t1.QuadPart);	//フレーム間隔(時間)算出
	double f_wait = (f_tpf - f_frame_interval);	//待ち時間を算出（待ち時間＝１フレームに必要な時間－実際に掛かった時間）
	if (f_wait > 0) {
		LARGE_INTEGER t2 = {};
		do {
			//Sleep(1);
			//std::this_thread::yield();
			QueryPerformanceCounter(&t2);	//現在時間取得
			//このループの経過時間を測り、待ち時間以下ならループ継続する。
		} while ((double)(t2.QuadPart - li_t2.QuadPart) < f_wait);
#ifdef _DEBUG
		//１フレーム時間を積算（１フレーム時間＝前のフレームから今のフレームまでの経過時間＋足りなかった分の待ち時間）
		f_total += (f_frame_interval + f_wait);
	}
	else {
		//待ち時間が無かったので１フレーム時間だけを積算
		f_total += f_frame_interval;
	}
	dbg_frame_count++;	//１秒間のフレーム数カウント(デバッグ用)
	if ((f_total >= f_1sec) && (dbg_frame_count > 0)) {
		double fps = f_1sec / (f_total / (double)dbg_frame_count);
		sprintf_s(dbg_str, sizeof(dbg_str),
			"[fps:%7.2f][count:%3d][1sec:%7.2f][tpf:%7.2f][wait:%7.2f][interval:%7.2f][total:%7.2f]\n",
			(double)fps, (int)dbg_frame_count, (double)f_1sec, (double)f_tpf, (double)f_wait, (double)f_frame_interval, (double)f_total);
		OutputDebugStringA(dbg_str);
		dbg_frame_count = 0;
		f_total = 0.0;
#endif // _DEBUG
	}
	QueryPerformanceCounter(&li_t1);	//計測開始時間取得
}
#elif (FRAME_SYNC==USE_MMSEC)
DWORD	dd_frequency = 0;
DWORD	dd_t1 = 0;
DWORD	dd_t2 = 0;
double f_1sec = 0.0;	//１秒のカウント数
double f_tpf = 0.0;		//１フレームの時間（ミリ秒）
#ifdef _DEBUG
double f_total = 0.0;
#endif // _DEBUG
/*
* フレーム同期
* 高解像度タイム スタンプを使って計測
*/
//初期化
void InitFrameSync(double _FPS)
{
	if (_FPS <= 0) {
		//範囲外は１FPSとする
		_FPS = 1;
	}
	FPS = _FPS;
	timeBeginPeriod(1);				//分解能を１ミリ秒に設定
	dd_frequency = 1000;			//１秒のカウント数（１０００ミリ秒）
	f_1sec = (double)dd_frequency;	//１秒のカウント数（１０００ミリ秒）
	f_tpf = (f_1sec / FPS);	//１フレームの時間（ミリ秒）
}
//同期
void FrameSync(void)
{
	//フレーム待ち時間計測
	dd_t2 = timeGetTime();	//現在時間取得
	double f_frame_interval = (double)(dd_t2 - dd_t1);	//フレーム間隔(時間)算出
	double f_wait = (f_tpf - f_frame_interval);	//待ち時間を算出（待ち時間＝１フレームに必要な時間－実際に掛かった時間）
	if (f_wait > 0) {
		DWORD t2 = 0;
		do {
			Sleep(1);
			//std::this_thread::yield();
			t2 = timeGetTime();	//現在時間取得
			//このループの経過時間を測り、待ち時間以下ならループ継続する。
		} while ((double)(t2 - dd_t2) < f_wait);
#ifdef _DEBUG
		//１フレーム時間を積算（１フレーム時間＝前のフレームから今のフレームまでの経過時間＋足りなかった分の待ち時間）
		f_total += (f_frame_interval + f_wait);
	}
	else {
		//待ち時間が無かったので１フレーム時間だけを積算
		f_total += f_frame_interval;
	}
	dbg_frame_count++;	//１秒間のフレーム数カウント(デバッグ用)
	if ((f_total >= f_1sec) && (dbg_frame_count > 0)) {
		double fps = f_1sec / (f_total / (double)dbg_frame_count);
		sprintf_s(dbg_str, sizeof(dbg_str),
			"[fps:%7.2f][count:%3d][1sec:%7.2f][tpf:%7.2f][wait:%7.2f][interval:%7.2f][total:%7.2f]\n",
			(double)fps, (int)dbg_frame_count, (double)f_1sec, (double)f_tpf, (double)f_wait, (double)f_frame_interval, (double)f_total);
		OutputDebugStringA(dbg_str);
		dbg_frame_count = 0;
		f_total = 0.0;
#endif // _DEBUG
	}
	dd_t1 = timeGetTime();	//現在時間取得
}
#else	// USE_RDTSC
//DWORD	dd_frequency = 0;
int	dd_t1 = 0;
int	dd_t2 = 0;
int mmpf = 0;
void InitFrameSync(double _fps)
{
	if (_fps <= 0) {
		_fps = 1;
	}
	FPS = _fps;
	timeBeginPeriod(1);				//分解能を１ミリ秒に設定
	dd_t1 = timeGetTime();	//現在時間取得
	mmpf = (int)(1000.0 / FPS);
}
void FrameSync(void)
{
	//フレーム待ち時間計測
	dd_t2 = timeGetTime();	//現在時間取得
	int t_wait = (mmpf - (dd_t2 - dd_t1));
	if (t_wait > 0) {
		Sleep(t_wait);
	}
	dd_t1 = timeGetTime();	//現在時間取得
}
#endif // USE_RDTSC

/**
* @copyright (c) 2018-2019 HAL Osaka College of Technology & Design (Ihara, H.)
*/
