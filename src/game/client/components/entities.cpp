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
	m_Particles.DelayedInit();
	m_Cursors.DelayedInit();
}

void CEntities::LoadEntities()
{
	m_GameSkins.LoadEntities();
	m_Particles.LoadEntities();
	m_Cursors.LoadEntities();
	m_Loaded = true;
}

// texture-type abstract entity
IGraphics::CTextureHandle CEntities::CTextureEntity::Get(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aTextures;
}

IGraphics::CTextureHandle CEntities::CTextureEntity::GetDefault() const
{
	return m_DefaultTexture;
}

const char* CEntities::CTextureEntity::GetName(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aName;
}

int CEntities::CTextureEntity::Num() const
{
	return m_Count;
}

int CEntities::CTextureEntity::EntityScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CEntities::CTextureEntity *pSelf = (CEntities::CTextureEntity *)pUser;
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;
	if(pSelf->m_Count >= MAX_TEXTURES)
		return 1;

	if(!pSelf->LoadEntity(pName, DirType, &pSelf->m_Info[pSelf->m_Count].m_aTextures))
		return 0;
	// write name
	str_copy(pSelf->m_Info[pSelf->m_Count].m_aName, pName, sizeof(pSelf->m_Info[pSelf->m_Count].m_aName));

	pSelf->m_Count++;
	return 0;
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
		LoadEntity(g_Config.m_ClCustomGameskin, IStorage::TYPE_ALL, &m_InitialTexture);
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

	Storage()->ListDirectory(IStorage::TYPE_ALL, "gameskins", EntityScan, this);
	dbg_msg("entities", "loaded %d gameskins", m_Count);
}
bool CEntities::CGameSkins::LoadEntity(const char *pName, int DirType, IGraphics::CTextureHandle *pTexture)
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
void CEntities::CGameSkins::Reload(int id)
{
	if(id < 0)
	{
		g_pData->m_aImages[IMAGE_GAME].m_Id = GetDefault();
		CTeecompUtils::TcReloadAsGrayScale(&g_pData->m_aImages[IMAGE_GAME_GRAY].m_Id, Graphics(), "game.png");
	}
	else
	{
		char aBuf[512];
		g_pData->m_aImages[IMAGE_GAME].m_Id = Get(id);
		str_format(aBuf, sizeof(aBuf), "gameskins/%s", g_Config.m_ClCustomGameskin);
		CTeecompUtils::TcReloadAsGrayScale(&g_pData->m_aImages[IMAGE_GAME_GRAY].m_Id, Graphics(), aBuf);
	}
}

// particles
CEntities::CParticles::CParticles()
{
	m_Count = 0;
}
void CEntities::CParticles::DelayedInit()
{
	m_DefaultTexture = g_pData->m_aImages[IMAGE_PARTICLES].m_Id; // save default texture
	if(g_Config.m_ClCustomParticles[0] == '\0')
	{
		g_pData->m_aImages[IMAGE_PARTICLES].m_Id = GetDefault();
	}
	else
	{
		LoadEntity(g_Config.m_ClCustomParticles, IStorage::TYPE_ALL, &m_InitialTexture);
		g_pData->m_aImages[IMAGE_PARTICLES].m_Id = m_InitialTexture;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "particles/%s", g_Config.m_ClCustomParticles);
	}
}
void CEntities::CParticles::LoadEntities()
{
	// unload all textures (don't think that's necessary)
	for(int i = 0; i < m_Count; i++)
			Graphics()->UnloadTexture(&(m_Info[i].m_aTextures));

	Storage()->ListDirectory(IStorage::TYPE_ALL, "particles", EntityScan, this);
	dbg_msg("entities", "loaded %d particles", m_Count);
}
bool CEntities::CParticles::LoadEntity(const char *pName, int DirType, IGraphics::CTextureHandle *pTexture)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "particles/%s", pName);
	CImageInfo Info;
	if(!Graphics()->LoadPNG(&Info, aBuf, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load particles '%s'", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "entities", aBuf);
		return false;
	}
	dbg_msg("entities", "loaded particles %s", pName);

	// load entities
	Graphics()->UnloadTexture(pTexture);
	*pTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);
	return true;
}
void CEntities::CParticles::Reload(int id)
{
	if(id < 0)
		g_pData->m_aImages[IMAGE_PARTICLES].m_Id = GetDefault();
	else
		g_pData->m_aImages[IMAGE_PARTICLES].m_Id = Get(id);
}

// cursors
CEntities::CCursors::CCursors()
{
	m_Count = 0;
}
void CEntities::CCursors::DelayedInit()
{
	m_DefaultTexture = g_pData->m_aImages[IMAGE_CURSOR].m_Id; // save default texture
	if(g_Config.m_ClCustomCursor[0] == '\0')
	{
		g_pData->m_aImages[IMAGE_CURSOR].m_Id = GetDefault();
	}
	else
	{
		LoadEntity(g_Config.m_ClCustomCursor, IStorage::TYPE_ALL, &m_InitialTexture);
		g_pData->m_aImages[IMAGE_CURSOR].m_Id = m_InitialTexture;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "cursors/%s", g_Config.m_ClCustomCursor);
	}
}
void CEntities::CCursors::LoadEntities()
{
	// unload all textures (don't think that's necessary)
	for(int i = 0; i < m_Count; i++)
			Graphics()->UnloadTexture(&(m_Info[i].m_aTextures));

	Storage()->ListDirectory(IStorage::TYPE_ALL, "cursors", EntityScan, this);
	dbg_msg("entities", "loaded %d cursors", m_Count);
}
bool CEntities::CCursors::LoadEntity(const char *pName, int DirType, IGraphics::CTextureHandle *pTexture)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "cursors/%s", pName);
	CImageInfo Info;
	if(!Graphics()->LoadPNG(&Info, aBuf, DirType))
	{
		str_format(aBuf, sizeof(aBuf), "failed to load cursor '%s'", pName);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "entities", aBuf);
		return false;
	}
	dbg_msg("entities", "loaded cursor %s", pName);

	// load entities
	Graphics()->UnloadTexture(pTexture);
	*pTexture = Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);
	return true;
}
void CEntities::CCursors::Reload(int id)
{
	if(id < 0)
		g_pData->m_aImages[IMAGE_CURSOR].m_Id = GetDefault();
	else
		g_pData->m_aImages[IMAGE_CURSOR].m_Id = Get(id);
}