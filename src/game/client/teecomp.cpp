#include <base/color.h>
#include <base/math.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <generated/client_data.h>
#include "teecomp.h"


// returns RGB for flags, HUD elements
vec3 CTeecompUtils::GetTeamColorSaturatedRGB(int ForTeam, int LocalTeam, const CConfiguration& Config)
{
	vec3 c = GetTeamColor(ForTeam, LocalTeam, g_Config.m_TcColoredTeesTeam1, g_Config.m_TcColoredTeesTeam2, g_Config.m_TcColoredTeesMethod);
	c.s = c.s + (1.0f-c.s)/2.0f;
	c.l = c.l + (1.0f-c.l)/2.0f;
	return HslToRgb(c);
}

vec3 CTeecompUtils::GetTeamColor(int ForTeam, int LocalTeam, int Color1, int Color2, int Method)
{
	vec3 c1((Color1>>16)/255.0f, ((Color1>>8)&0xff)/255.0f, (Color1&0xff)/255.0f);
	vec3 c2((Color2>>16)/255.0f, ((Color2>>8)&0xff)/255.0f, (Color2&0xff)/255.0f);

	// Team based Colors or spectating
	if(!Method || LocalTeam == -1)
	{
		if(ForTeam == 0)
			return c1;
		return c2;
	}

	// Enemy based Colors
	if(ForTeam == LocalTeam)
		return c1;
	return c2;
}

int CTeecompUtils::GetTeamColorInt(int ForTeam, int LocalTeam, int Color1, int Color2, int Method)
{
	// Team based Colors or spectating
	if(!Method || LocalTeam == -1)
	{
		if(ForTeam == 0)
			return Color1;
		return Color2;
	}

	// Enemy based Colors
	if(ForTeam == LocalTeam)
		return Color1;
	return Color2;
}

bool CTeecompUtils::GetForcedSkinName(int ForTeam, int LocalTeam, const char*& pSkinName)
{
	// Team based Colors or spectating
	if(!g_Config.m_TcForcedSkinsMethod || LocalTeam == -1)
	{
		if(ForTeam == 0)
		{
			pSkinName = g_Config.m_TcForcedSkin1;
			return g_Config.m_TcForceSkinTeam1;
		}
		pSkinName = g_Config.m_TcForcedSkin2;
		return g_Config.m_TcForceSkinTeam2;
	}

	// Enemy based Colors
	if(ForTeam == LocalTeam)
	{
		pSkinName = g_Config.m_TcForcedSkin1;
		return g_Config.m_TcForceSkinTeam1;
	}
	pSkinName = g_Config.m_TcForcedSkin2;
	return g_Config.m_TcForceSkinTeam2;
}

bool CTeecompUtils::GetForceDmColors(int ForTeam, int LocalTeam)
{
	if(!g_Config.m_TcColoredTeesMethod || LocalTeam == -1)
	{
		if(ForTeam == 0)
			return g_Config.m_TcDmColorsTeam1;
		return g_Config.m_TcDmColorsTeam2;
	}

	if(ForTeam == LocalTeam)
		return g_Config.m_TcDmColorsTeam1;
	return g_Config.m_TcDmColorsTeam2;
}

void CTeecompUtils::ResetConfig()
{
	#define MACRO_CONFIG_INT(Name,ScriptName,Def,Min,Max,Save,Desc) g_Config.m_##Name = Def;
	#define MACRO_CONFIG_STR(Name,ScriptName,Len,Def,Save,Desc) str_copy(g_Config.m_##Name, Def, Len);
	#include "../teecomp_vars.h"
	#undef MACRO_CONFIG_INT
	#undef MACRO_CONFIG_STR
}

const char* CTeecompUtils::RgbToName(int rgb)
{
	vec3 rgb_v((rgb>>16)/255.0f, ((rgb>>8)&0xff)/255.0f, (rgb&0xff)/255.0f);
	vec3 hsl = RgbToHsl(rgb_v);

	if(hsl.l < 0.2f)
		return "Black";
	if(hsl.l > 0.9f)
		return "White";
	if(hsl.s < 0.1f)
		return "Gray";
	if(hsl.h < 20)
		return "Red";
	if(hsl.h < 45)
		return "Orange";
	if(hsl.h < 70)
		return "Yellow";
	if(hsl.h < 155)
		return "Green";
	if(hsl.h < 260)
		return "Blue";
	if(hsl.h < 335)
		return "Purple";
	return "Red";
}

const char* CTeecompUtils::TeamColorToName(int rgb)
{
	vec3 rgb_v((rgb>>16)/255.0f, ((rgb>>8)&0xff)/255.0f, (rgb&0xff)/255.0f);
	vec3 hsl = RgbToHsl(rgb_v);

	if(hsl.l < 0.2f)
		return "black team";
	if(hsl.l > 0.9f)
		return "white team";
	if(hsl.s < 0.1f)
		return "gray team";
	if(hsl.h < 20)
		return "red team";
	if(hsl.h < 45)
		return "orange team";
	if(hsl.h < 70)
		return "yellow team";
	if(hsl.h < 155)
		return "green team";
	if(hsl.h < 260)
		return "blue team";
	if(hsl.h < 335)
		return "purple team";
	return "red team";
}

void CTeecompUtils::TcReloadAsGrayScale(IGraphics::CTextureHandle* Texture, IGraphics* pGraphics)
{
	// Teecomp grayscale flags
	pGraphics->UnloadTexture(Texture); // Already loaded with full color, unload
	//*Texture = -1; // Is that okay??

	CImageInfo Info;
	if(!pGraphics->LoadPNG(&Info, g_pData->m_aImages[IMAGE_GAME_GRAY].m_pFilename, IStorage::TYPE_ALL))
		return;

	unsigned char *d = (unsigned char *)Info.m_pData;
	int Step = Info.m_Format == CImageInfo::FORMAT_RGBA ? 4 : 3;

	for(int i=0; i < Info.m_Width*Info.m_Height; i++)
	{
		int v = (d[i*Step]+d[i*Step+1]+d[i*Step+2])/3;
		d[i*Step] = v;
		d[i*Step+1] = v;
		d[i*Step+2] = v;
	}

	int aFreq[256];
	int OrgWeight;
	int NewWeight;
	int FlagX = 384;
	int FlagY = 256;
	int FlagW = 128;
	int FlagH = 256;
	int Pitch = Info.m_Width*4;

	for(int f=0; f<2; f++)
	{
		OrgWeight = 0;
		NewWeight = 192;
		for(int i=0; i<256; i++)
			aFreq[i] = 0;

		// find most common frequence
		for(int y=FlagY; y<FlagY+FlagH; y++)
			for(int x=FlagX+FlagW*f; x<FlagX+FlagW*(1+f); x++)
			{
				if(d[y*Pitch+x*4+3] > 128)
					aFreq[d[y*Pitch+x*4]]++;
			}
		
		for(int i = 1; i < 256; i++)
		{
			if(aFreq[OrgWeight] < aFreq[i])
				OrgWeight = i;
		}

		// reorder
		int InvOrgWeight = 255-OrgWeight;
		int InvNewWeight = 255-NewWeight;
		for(int y=FlagY; y<FlagY+FlagH; y++)
			for(int x=FlagX+FlagW*f; x<FlagX+FlagW*(1+f); x++)
			{
				int v = d[y*Pitch+x*4];
				if(v <= OrgWeight*1.25f) // modified for contrast
					v = (int)(((v/(float)OrgWeight) * NewWeight));
				else
					v = (int)(((v-OrgWeight)/(float)InvOrgWeight)*InvNewWeight + NewWeight);
				d[y*Pitch+x*4] = v;
				d[y*Pitch+x*4+1] = v;
				d[y*Pitch+x*4+2] = v;
			}
	}

	*Texture = pGraphics->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);
}