#ifndef __TEECOMP_HPP_
#define __TEECOMP_HPP_

#include <base/vmath.h>
#include <engine/graphics.h>

class CTeecompUtils
{
public:
	static vec3 GetTeamColor(int ForTeam, int LocalTeam, int Color1, int Color2, int Method);
	static int GetTeamColorInt(int ForTeam, int LocalTeam, int Color1, int Color2, int Method);
	static bool GetForcedSkinName(int ForTeam, int LocalTeam, const char*& SkinName);
	static bool GetForceDmColors(int ForTeam, int LocalTeam);
	static void ResetConfig();
	static const char* RgbToName(int rgb);
	static const char* TeamColorToName(int rgb);
	static void TcReloadAsGrayScale(IGraphics::CTextureHandle* Texture, IGraphics* pGraphics);
};

#endif
