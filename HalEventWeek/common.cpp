#include"common.h"
//	文字Bmpの生成を楽にするやつ、CreateBmpStringからMakeTextBmpに変換
Bmp* MakeTextBmp(const TCHAR* text, int size, int bold , int ggo )
{
	const TCHAR* kFont = TEXT("MS ゴシック");	//	使いたいフォントに
	return CreateBmpString(kFont, size, bold, ggo, text);
}