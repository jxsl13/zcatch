#ifndef __TEECOMP_HPP_
#define __TEECOMP_HPP_

#include <base/vmath.h>
#include <engine/graphics.h>

class CTeecompUtils
{
public:
	static vec3 GetTeamColorSaturatedRGB(int ForTeam, int LocalTeam, const CConfiguration& Config);
	static inline vec3 GetTeamColorRGB(int ForTeam, int LocalTeam, const CConfiguration& Config)
		{ return GetTeamColor(ForTeam, LocalTeam, Config.m_TcColoredTeesTeam1, Config.m_TcColoredTeesTeam2, Config.m_TcColoredTeesMethod); }
	static inline vec3 GetTeamColorHSL(int ForTeam, int LocalTeam, const CConfiguration& Config)
		{ return GetTeamColor(ForTeam, LocalTeam, Config.m_TcColoredTeesTeam1Hsl, Config.m_TcColoredTeesTeam2Hsl, Config.m_TcColoredTeesMethod); }

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
