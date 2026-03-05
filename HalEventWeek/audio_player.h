#pragma once
#include <string>

// 初期化/終了
bool AudioSystem_Init();
void AudioSystem_Shutdown();

// 再生API
//  - PlaySE : 効果音（ワンショット）
//  - PlayBGM: BGM（ループ可） / StopBGM: 停止
bool PlaySE(const std::wstring& path /*.mp3/.ogg/.wav*/);
bool PlayBGM(const std::wstring& path /*.mp3/.ogg/.wav*/, bool loop = true, float volume = 0.8f);
void StopBGM();