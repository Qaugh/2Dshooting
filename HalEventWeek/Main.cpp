#include "common.h"
//	ƒƒCƒ“ŠÖ”
int main()
{
	GameState game{};
	Assets    assets{};

	Init(game, assets);

	while (!game.requestQuit)
	{
		Input (game, assets);	//	“ü—Í
		Game  (game, assets);	//	XV
		Output(game, assets);	//	•`‰æ
		Sleep (16);
	}

	Delete(game, assets);
	return 0;
}