#pragma once
#include"common.h"
void DrawHpHearts(const GameState&, const Assets&, int, int, int, float);
inline void DrawProgressBarI(int, int, int, int, float, int, int);
void DrawTeleportState(const GameState&, const Assets&);
void DrawWeaponInventory(const GameState&, const Assets&);