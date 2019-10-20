/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/storage.h>
#include <game/client/component.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>

#include "mapimages.h"

CMapImages::CMapImages()
{
	m_Info[MAP_TYPE_GAME].m_Count = 0;
	m_Info[MAP_TYPE_MENU].m_Count = 0;
	// m_EntitesTextures = -1;
	m_EasterIsLoaded = false;
}

void CMapImages::LoadMapImages(IMap *pMap, class CLayers *pLayers, int MapType)
{
	if(MapType < 0 || MapType >= NUM_MAP_TYPES)
		return;

	// unload all textures
	for(int i = 0; i < m_Info[MapType].m_Count; i++)
		Graphics()->UnloadTexture(&(m_Info[MapType].m_aTextures[i]));
	m_Info[MapType].m_Count = 0;

	int Start;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &m_Info[MapType].m_Count);
	m_Info[MapType].m_Count = clamp(m_Info[MapType].m_Count, 0, int(MAX_TEXTURES));

	// load new textures
	for(int i = 0; i < m_Info[MapType].m_Count; i++)
	{
		int TextureFlags = 0;
		bool FoundQuadLayer = false;
		bool FoundTileLayer = false;
		for(int k = 0; k < pLayers->NumLayers(); k++)
		{
			const CMapItemLayer * const pLayer = pLayers->GetLayer(k);
			if(!FoundQuadLayer && pLayer->m_Type == LAYERTYPE_QUADS && ((const CMapItemLayerQuads *)pLayer)->m_Image == i)
				FoundQuadLayer = true;
			if(!FoundTileLayer && pLayer->m_Type == LAYERTYPE_TILES && ((const CMapItemLayerTilemap *)pLayer)->m_Image == i)
				FoundTileLayer = true;
		}
		if(FoundTileLayer)
			TextureFlags = FoundQuadLayer ? IGraphics::TEXLOAD_MULTI_DIMENSION : IGraphics::TEXLOAD_ARRAY_256;

		CMapItemImage *pImg = (CMapItemImage *)pMap->GetItem(Start+i, 0, 0);
		if(pImg->m_External || (pImg->m_Version > 1 && pImg->m_Format != CImageInfo::FORMAT_RGB && pImg->m_Format != CImageInfo::FORMAT_RGBA))
		{
			char Buf[256];
			char *pName = (char *)pMap->GetData(pImg->m_ImageName);
			str_format(Buf, sizeof(Buf), "mapres/%s.png", pName);
			m_Info[MapType].m_aTextures[i] = Graphics()->LoadTexture(Buf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, TextureFlags);
		}
		else
		{
			void *pData = pMap->GetData(pImg->m_ImageData);
			m_Info[MapType].m_aTextures[i] = Graphics()->LoadTextureRaw(pImg->m_Width, pImg->m_Height, pImg->m_Version == 1 ? CImageInfo::FORMAT_RGBA : pImg->m_Format, pData, CImageInfo::FORMAT_RGBA, TextureFlags);
			pMap->UnloadData(pImg->m_ImageData);
		}
	}
	LoadAutoMapres();	

	// load game entities
	Graphics()->UnloadTexture(&m_EntitiesTexture);
	Graphics()->UnloadTexture(&m_AutoEntitiesTexture);
	m_EntitiesTexture = Graphics()->LoadTexture("editor/entities_clear.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);
	if(!m_EntitiesTexture.IsValid())
		m_EntitiesTexture = Graphics()->LoadTexture("editor/entities.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);

	m_AutoEntitiesTexture = Graphics()->LoadTexture("editor/entities_auto.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);
	if(!m_AutoTilesTexture.IsValid())
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "automapper", "Failed to load entities_auto.png");

	m_TeleEntitiesTexture = Graphics()->LoadTexture("editor/entities_teleport.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);
	if(!m_TeleEntitiesTexture.IsValid())
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "automapper", "Failed to load entities_teleport.png");

	// easter time, preload easter tileset
	if(m_pClient->IsEaster())
		GetEasterTexture();
}

void CMapImages::LoadAutoMapres()
{
	Graphics()->UnloadTexture(&m_AutoTilesTexture);
	Graphics()->UnloadTexture(&m_AutoDoodadsTexture);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "mapres/%s_main.png", g_Config.m_GfxAutomapLayer);
	m_AutoTilesTexture = Graphics()->LoadTexture(aBuf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);
	str_format(aBuf, sizeof(aBuf), "mapres/%s_doodads.png", g_Config.m_GfxAutomapLayer);
	m_AutoDoodadsTexture = Graphics()->LoadTexture(aBuf, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);

	if(!m_AutoTilesTexture.IsValid())
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "automapper", "Failed to load auto tiles");
	if(!m_AutoDoodadsTexture.IsValid())
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "automapper", "Failed to load auto doodads");
}


void CMapImages::OnMapLoad()
{
	LoadMapImages(Kernel()->RequestInterface<IMap>(), Layers(), MAP_TYPE_GAME);
}

void CMapImages::OnMenuMapLoad(IMap *pMap)
{
	CLayers MenuLayers;
	MenuLayers.Init(Kernel(), pMap);
	LoadMapImages(pMap, &MenuLayers, MAP_TYPE_MENU);
}

IGraphics::CTextureHandle CMapImages::GetEasterTexture()
{
	if(!m_EasterIsLoaded)
	{
		m_EasterTexture = Graphics()->LoadTexture("mapres/easter.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_ARRAY_256);
		if(!m_EasterTexture.IsValid())
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "mapimages", "Failed to load easter.png");
		m_EasterIsLoaded = true;
	}
	return m_EasterTexture;
}

IGraphics::CTextureHandle CMapImages::Get(int Index) const
{
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return m_Info[MAP_TYPE_GAME].m_aTextures[clamp(Index, 0, m_Info[MAP_TYPE_GAME].m_Count)];
	return m_Info[MAP_TYPE_MENU].m_aTextures[clamp(Index, 0, m_Info[MAP_TYPE_MENU].m_Count)];
}


IGraphics::CTextureHandle CMapImages::GetEntities() const
{
	return m_EntitiesTexture;
}
IGraphics::CTextureHandle CMapImages::GetAutoEntities() const
{
	return m_AutoEntitiesTexture;
}
IGraphics::CTextureHandle CMapImages::GetTeleEntities() const
{
	return m_TeleEntitiesTexture;
}
IGraphics::CTextureHandle CMapImages::GetAutoTiles() const
{
	return m_AutoTilesTexture;
}
IGraphics::CTextureHandle CMapImages::GetAutoDoodads() const
{
	return m_AutoDoodadsTexture;
}

int CMapImages::Num() const
{
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return m_Info[MAP_TYPE_GAME].m_Count;
	return m_Info[MAP_TYPE_MENU].m_Count;
}
