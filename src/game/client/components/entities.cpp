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
	m_Count = 0;
	m_Loaded = false;
}

void CEntities::DelayedInit()
{
	m_DefaultTexture = g_pData->m_aImages[IMAGE_GAME].m_Id; // save default texture
	if(g_Config.m_ClCustomGameskin[0] == '\0')
	{
		g_pData->m_aImages[IMAGE_GAME].m_Id = m_pClient->m_pEntities->GetDefault();
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

IGraphics::CTextureHandle CEntities::Get(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aTextures;
}

IGraphics::CTextureHandle CEntities::GetDefault() const
{
	return m_DefaultTexture;
}

const char* CEntities::GetName(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aName;
}

int CEntities::Num() const
{
	return m_Count;
}

void CEntities::LoadEntities()
{
	// unload all textures (don't think that's necessary)
	for(int i = 0; i < m_Count; i++)
			Graphics()->UnloadTexture(&(m_Info[i].m_aTextures));

	Storage()->ListDirectory(IStorage::TYPE_ALL, "gameskins", EntityScan, this);
	dbg_msg("entities", "loaded %d gameskins", m_Count);
	m_Loaded = true;
}

int CEntities::EntityScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	CEntities *pSelf = (CEntities *)pUser;
	if(IsDir || !str_endswith(pName, ".png"))
		return 0;

	/* CImageInfo Info;
	if(!pSelf->Graphics()->LoadPNG(&Info, aFilePath, DirType))
	{
		str_format(aFilePath, sizeof(aFilePath), "failed to load gameskin '%s'", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "entities", aFilePath);
		return 0;
	}
	dbg_msg("entities", "loaded gameskin %s", pName);

	// load entities
	if(pSelf->m_Count >= MAX_TEXTURES)
		return 0;
	pSelf->Graphics()->UnloadTexture(&pSelf->m_Info[pSelf->m_Count].m_aTextures);
	pSelf->m_Info[pSelf->m_Count].m_aTextures = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);
 */
	if(pSelf->m_Count >= MAX_TEXTURES)
		return 1;

	if(!pSelf->LoadEntity(pName, DirType, &pSelf->m_Info[pSelf->m_Count].m_aTextures))
		return 0;

	// write name
	str_copy(pSelf->m_Info[pSelf->m_Count].m_aName, pName, sizeof(pSelf->m_Info[pSelf->m_Count].m_aName));

	pSelf->m_Count++;
	return 0;
}

bool CEntities::LoadEntity(const char *pName, int DirType, IGraphics::CTextureHandle *pTexture)
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
