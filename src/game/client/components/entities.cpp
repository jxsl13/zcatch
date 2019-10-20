/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/shared/config.h>
#include <generated/client_data.h>
#include <game/client/component.h>
#include <game/client/teecomp.h>

#include "entities.h"

CEntities::CEntities()
{
	m_Loaded = false;
}

void CEntities::DelayedInit()
{
	m_GameSkins.DelayedInit();
}

void CEntities::LoadEntities()
{
	m_GameSkins.LoadEntities();
	m_Loaded = true;
}

// game skins
CEntities::CGameSkins::CGameSkins()
{
	m_Count = 0;
}

void CEntities::CGameSkins::DelayedInit()
{
	m_DefaultTexture = g_pData->m_aImages[IMAGE_GAME].m_Id; // save default texture
	if(g_Config.m_ClCustomGameskin[0] == '\0')
	{
		g_pData->m_aImages[IMAGE_GAME].m_Id = GetDefault();
		CTeecompUtils::TcReloadAsGrayScale(&g_pData->m_aImages[IMAGE_GAME_GRAY].m_Id, Graphics(), "game.png");
	}
	else
	{
		LoadGameSkin(g_Config.m_ClCustomGameskin, IStorage::TYPE_ALL, &m_InitialTexture);
		g_pData->m_aImages[IMAGE_GAME].m_Id = m_InitialTexture;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "gameskins/%s", g_Config.m_ClCustomGameskin);
		CTeecompUtils::TcReloadAsGrayScale(&g_pData->m_aImages[IMAGE_GAME_GRAY].m_Id, Graphics(), aBuf);
	}
}

void CEntities::CGameSkins::LoadEntities()
{
	// unload all textures (don't think that's necessary)
	for(int i = 0; i < m_Count; i++)
			Graphics()->UnloadTexture(&(m_Info[i].m_aTextures));

	Storage()->ListDirectory(IStorage::TYPE_ALL, "gameskins", GameSkinScan, this);
	dbg_msg("entities", "loaded %d gameskins", m_Count);
}

IGraphics::CTextureHandle CEntities::CGameSkins::Get(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aTextures;
}

IGraphics::CTextureHandle CEntities::CGameSkins::GetDefault() const
{
	return m_DefaultTexture;
}

const char* CEntities::CGameSkins::GetName(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aName;
}

int CEntities::CGameSkins::Num() const
{
	return m_Count;
}

int CEntities::CGameSkins::GameSkinScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CEntities::CGameSkins *pSelf = (CEntities::CGameSkins *)pUser;
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;
	if(pSelf->m_Count >= MAX_TEXTURES)
		return 1;

	if(!pSelf->LoadGameSkin(pName, DirType, &pSelf->m_Info[pSelf->m_Count].m_aTextures))
		return 0;
	// write name
	str_copy(pSelf->m_Info[pSelf->m_Count].m_aName, pName, sizeof(pSelf->m_Info[pSelf->m_Count].m_aName));

	pSelf->m_Count++;
	return 0;
}

bool CEntities::CGameSkins::LoadGameSkin(const char *pName, int DirType, IGraphics::CTextureHandle *pTexture)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "gameskins/%s", pName);
	CImageInfo Info;
	if(!Graphics()->LoadPNG(&Info, aBuf, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load gameskin '%s'", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "entities", aBuf);
		return false;
	}
	dbg_msg("entities", "loaded gameskin %s", pName);

	// load entities
	Graphics()->UnloadTexture(pTexture);
	*pTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);
	return true;
}
