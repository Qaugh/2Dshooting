#include "audio_player.h"

// ▼ ここに miniaudio を埋め込み（MP3/OGG を有効化）
#define MINIAUDIO_IMPLEMENTATION
#define MA_ENABLE_MP3
#define MA_ENABLE_VORBIS
#include "miniaudio.h"

#include <windows.h>  // WideCharToMultiByte

// グローバル：オーディオエンジン & BGM用サウンド
static ma_engine gEngine;
static ma_sound  gBgm;
static bool      gBgmInit = false;

// UTF-16(wstring) → UTF-8 変換（miniaudio は UTF-8 パスを想定）
static std::string ToUTF8(const std::wstring& w)
{
    if (w.empty()) return {};
    int size = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(size, '\0');
    // s.data() は const char* を返すため、&s[0] を使って char* を取得
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], size, nullptr, nullptr);
    return s;
}

bool AudioSystem_Init()
{
    ma_engine_config cfg = ma_engine_config_init();
    // 既定のデバイス（WASAPI）に接続
    if (ma_engine_init(&cfg, &gEngine) != MA_SUCCESS) return false;
    gBgmInit = false;
    return true;
}

void AudioSystem_Shutdown()
{
    if (gBgmInit) {
        ma_sound_stop(&gBgm);
        ma_sound_uninit(&gBgm);
        gBgmInit = false;
    }
    ma_engine_uninit(&gEngine);
}

bool PlaySE(const std::wstring& path)
{
    const std::string p = ToUTF8(path);
    // ワンショットはエンジンに委任（寿命管理をお任せ）
    return ma_engine_play_sound(&gEngine, p.c_str(), nullptr) == MA_SUCCESS;
}

bool PlayBGM(const std::wstring& path, bool loop, float volume)
{
    const std::string p = ToUTF8(path);

    // 既存BGMを止める
    if (gBgmInit) {
        ma_sound_stop(&gBgm);
        ma_sound_uninit(&gBgm);
        gBgmInit = false;
    }

    // ファイルから BGM を作成
    if (ma_sound_init_from_file(&gEngine, p.c_str(), 0, nullptr, nullptr, &gBgm) != MA_SUCCESS)
        return false;

    ma_sound_set_looping(&gBgm, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&gBgm, volume);
    if (ma_sound_start(&gBgm) != MA_SUCCESS) {
        ma_sound_uninit(&gBgm);
        return false;
    }
    gBgmInit = true;
    return true;
}

void StopBGM()
{
    if (!gBgmInit) return;
    ma_sound_stop(&gBgm);
    ma_sound_uninit(&gBgm);
    gBgmInit = false;
}