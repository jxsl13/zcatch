/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/storage.h>
#include <game/client/component.h>
#include <engine/shared/config.h>

#include "entities.h"

void CEntities::OnInit()
{
	m_Count = 0;
	m_Loaded = false;
}

IGraphics::CTextureHandle CEntities::Get(int Index) const
{
	return m_Info[clamp(Index, 0, m_Count)].m_aTextures;
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
	m_Count = 0;
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

	dbg_msg("entities", "loaded gameskin %s", pName);

	char aFilePath[512];
	str_format(aFilePath, sizeof(aFilePath), "gameskins/%s", pName);
	CImageInfo Info;
	if(!pSelf->Graphics()->LoadPNG(&Info, aFilePath, DirType))
	{
		str_format(aFilePath, sizeof(aFilePath), "failed to load gameskin '%s'", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "entities", aFilePath);
		return 0;
	}

	// load entities
	if(pSelf->m_Count >= MAX_TEXTURES)
		return 0;
	pSelf->Graphics()->UnloadTexture(&pSelf->m_Info[pSelf->m_Count].m_aTextures);
	pSelf->m_Info[pSelf->m_Count].m_aTextures = pSelf->Graphics()->LoadTextureRaw(Info.m_Width, Info.m_Height, Info.m_Format, Info.m_pData, Info.m_Format, 0);
	mem_free(Info.m_pData);

	// write name
	str_copy(pSelf->m_Info[pSelf->m_Count].m_aName, pName, sizeof(pSelf->m_Info[pSelf->m_Count].m_aName));

	pSelf->m_Count++;
	return 0;
}
