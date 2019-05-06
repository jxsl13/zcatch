/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/tl/array.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/demo.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>
#include <game/client/render.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>
#include <game/collision.h>
#include <game/client/components/auto_tile.h>

#include "maplayers.h"

CMapLayers::CMapLayers(int t)
{
	m_Type = t;
	m_CurrentLocalTick = 0;
	m_LastLocalTick = 0;
	m_EnvelopeUpdate = false;
	m_AutolayerUpdate = false;
	m_TeleUpdate = true;
	m_pMenuMap = 0;
	m_pMenuLayers = 0;
	m_OnlineStartTime = 0;
	m_pTilesetPainter = 0;
	m_pDoodadsPainter = 0;
	m_pTeleTiles = 0;
	m_pAutoTiles = 0;
	m_pAutoDoodads = 0;
}

void CMapLayers::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_ONLINE)
		m_OnlineStartTime = Client()->LocalTime(); // reset time for non-scynchronized envelopes
}

void CMapLayers::LoadBackgroundMap()
{
	if(!g_Config.m_ClShowMenuMap)
		return;

	int HourOfTheDay = time_houroftheday();
	char aBuf[128];
	// check for the appropriate day/night map
	str_format(aBuf, sizeof(aBuf), "ui/themes/%s_%s.map", g_Config.m_ClMenuMap, (HourOfTheDay >= 6 && HourOfTheDay < 18) ? "day" : "night");
	if(!m_pMenuMap->Load(aBuf, m_pClient->Storage()))
	{
		// fall back on generic map
		str_format(aBuf, sizeof(aBuf), "ui/themes/%s.map", g_Config.m_ClMenuMap);
		if(!m_pMenuMap->Load(aBuf, m_pClient->Storage()))
		{
			// fall back on day/night alternative map
			str_format(aBuf, sizeof(aBuf), "ui/themes/%s_%s.map", g_Config.m_ClMenuMap, (HourOfTheDay >= 6 && HourOfTheDay < 18) ? "night" : "day");
			if(!m_pMenuMap->Load(aBuf, m_pClient->Storage()))
			{
				str_format(aBuf, sizeof(aBuf), "map '%s' not found", g_Config.m_ClMenuMap);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
				return;
			}
		}
	}

	str_format(aBuf, sizeof(aBuf), "loaded map '%s'", g_Config.m_ClMenuMap);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);

	m_pMenuLayers->Init(Kernel(), m_pMenuMap);
	RenderTools()->RenderTilemapGenerateSkip(m_pMenuLayers);
	m_pClient->m_pMapimages->OnMenuMapLoad(m_pMenuMap);
	LoadEnvPoints(m_pMenuLayers, m_lEnvPointsMenu);
}

void CMapLayers::OnInit()
{
	if(m_Type == TYPE_BACKGROUND)
	{
		m_pMenuLayers = new CLayers;
		m_pMenuMap = CreateEngineMap();

		LoadBackgroundMap();
	}

	m_pEggTiles = 0;
}

static void PlaceEggDoodads(int LayerWidth, int LayerHeight, CTile* aOutTiles, CTile* aGameLayerTiles, int ItemWidth, int ItemHeight, const int* aImageTileID, int ImageTileIDCount, int Freq)
{
	for(int y = 0; y < LayerHeight-ItemHeight; y++)
	{
		for(int x = 0; x < LayerWidth-ItemWidth; x++)
		{
			bool Overlap = false;
			bool ObstructedByWall = false;
			bool HasGround = true;

			for(int iy = 0; iy < ItemHeight; iy++)
			{
				for(int ix = 0; ix < ItemWidth; ix++)
				{
					int Tid = (y+iy) * LayerWidth + (x+ix);
					int DownTid = (y+iy+1) * LayerWidth + (x+ix);

					if(aOutTiles[Tid].m_Index != 0)
					{
						Overlap = true;
						break;
					}

					if(aGameLayerTiles[Tid].m_Index == 1)
					{
						ObstructedByWall = true;
						break;
					}

					if(iy == ItemHeight-1 && aGameLayerTiles[DownTid].m_Index != 1)
					{
						HasGround = false;
						break;
					}
				}
			}

			if(!Overlap && !ObstructedByWall && HasGround && random_int()%Freq == 0)
			{
				const int BaskerStartID = aImageTileID[random_int()%ImageTileIDCount];

				for(int iy = 0; iy < ItemHeight; iy++)
				{
					for(int ix = 0; ix < ItemWidth; ix++)
					{
						int Tid = (y+iy) * LayerWidth + (x+ix);
						aOutTiles[Tid].m_Index = BaskerStartID + iy * 16 + ix;
					}
				}
			}
		}
	}
}

void CMapLayers::OnMapLoad()
{
	if(Layers())
		LoadEnvPoints(Layers(), m_lEnvPoints);
	LoadPainters(Layers());
	m_TeleUpdate = true;

	// easter time, place eggs
	if(m_pClient->IsEaster())
	{
		CMapItemLayerTilemap* pGameLayer = Layers()->GameLayer();
		if(m_pEggTiles)
			mem_free(m_pEggTiles);

		m_EggLayerWidth = pGameLayer->m_Width;
		m_EggLayerHeight = pGameLayer->m_Height;
		m_pEggTiles = (CTile*)mem_alloc(sizeof(CTile) * m_EggLayerWidth * m_EggLayerHeight,1);
		mem_zero(m_pEggTiles, sizeof(CTile) * m_EggLayerWidth * m_EggLayerHeight);
		CTile* aGameLayerTiles = (CTile*)Layers()->Map()->GetData(pGameLayer->m_Data);

		// first pass: baskets
		static const int s_aBasketIDs[] = {
			38,
			86
		};

		static const int s_BasketCount = sizeof(s_aBasketIDs)/sizeof(s_aBasketIDs[0]);
		PlaceEggDoodads(m_EggLayerWidth, m_EggLayerHeight, m_pEggTiles, aGameLayerTiles, 3, 2, s_aBasketIDs, s_BasketCount, 250);

		// second pass: double eggs
		static const int s_aDoubleEggIDs[] = {
			9,
			25,
			41,
			57,
			73,
			89
		};

		static const int s_DoubleEggCount = sizeof(s_aDoubleEggIDs)/sizeof(s_aDoubleEggIDs[0]);
		PlaceEggDoodads(m_EggLayerWidth, m_EggLayerHeight, m_pEggTiles, aGameLayerTiles, 2, 1, s_aDoubleEggIDs, s_DoubleEggCount, 100);

		// third pass: eggs
		static const int s_aEggIDs[] = {
			1, 2, 3, 4, 5,
			17, 18, 19, 20,
			33, 34, 35, 36,
			49, 50,     52,
			65, 66,
				82,
				98
		};

		static const int s_EggCount = sizeof(s_aEggIDs)/sizeof(s_aEggIDs[0]);
		PlaceEggDoodads(m_EggLayerWidth, m_EggLayerHeight, m_pEggTiles, aGameLayerTiles, 1, 1, s_aEggIDs, s_EggCount, 30);
	}
}

void CMapLayers::OnShutdown()
{
	if(m_pEggTiles)
	{
		mem_free(m_pEggTiles);
		m_pEggTiles = 0;
	}
>>>>>>> ee147dedd631a5a430695beac40a666f92a75825
}

void CMapLayers::LoadEnvPoints(const CLayers *pLayers, array<CEnvPoint>& lEnvPoints)
{
	lEnvPoints.clear();

	// get envelope points
	CEnvPoint *pPoints = 0x0;
	{
		int Start, Num;
		pLayers->Map()->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);

		if(!Num)
			return;

		pPoints = (CEnvPoint *)pLayers->Map()->GetItem(Start, 0, 0);
	}

	// get envelopes
	int Start, Num;
	pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
	if(!Num)
		return;


	for(int env = 0; env < Num; env++)
	{
		CMapItemEnvelope *pItem = (CMapItemEnvelope *)pLayers->Map()->GetItem(Start+env, 0, 0);

		if(pItem->m_Version >= 3)
		{
			for(int i = 0; i < pItem->m_NumPoints; i++)
				lEnvPoints.add(pPoints[i + pItem->m_StartPoint]);
		}
		else
		{
			// backwards compatibility
			for(int i = 0; i < pItem->m_NumPoints; i++)
			{
				// convert CEnvPoint_v1 -> CEnvPoint
				CEnvPoint_v1 *pEnvPoint_v1 = &((CEnvPoint_v1 *)pPoints)[i + pItem->m_StartPoint];
				CEnvPoint p;

				p.m_Time = pEnvPoint_v1->m_Time;
				p.m_Curvetype = pEnvPoint_v1->m_Curvetype;

				for(int c = 0; c < pItem->m_Channels; c++)
				{
					p.m_aValues[c] = pEnvPoint_v1->m_aValues[c];
					p.m_aInTangentdx[c] = 0;
					p.m_aInTangentdy[c] = 0;
					p.m_aOutTangentdx[c] = 0;
					p.m_aOutTangentdy[c] = 0;
				}

				lEnvPoints.add(p);
			}
		}
	}
}

void CMapLayers::EnvelopeUpdate()
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		m_CurrentLocalTick = pInfo->m_CurrentTick;
		m_LastLocalTick = pInfo->m_CurrentTick;
		m_EnvelopeUpdate = true;
	}
}

void CMapLayers::EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser)
{
	CMapLayers *pThis = (CMapLayers *)pUser;
	pChannels[0] = 0;
	pChannels[1] = 0;
	pChannels[2] = 0;
	pChannels[3] = 0;

	CEnvPoint *pPoints = 0;
	CLayers *pLayers = 0;
	{
		if(pThis->Client()->State() == IClient::STATE_ONLINE || pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		{
			pLayers = pThis->Layers();
			pPoints = pThis->m_lEnvPoints.base_ptr();
		}
		else
		{
			pLayers = pThis->m_pMenuLayers;
			pPoints = pThis->m_lEnvPointsMenu.base_ptr();
		}
	}

	int Start, Num;
	pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);

	if(Env >= Num)
		return;

	CMapItemEnvelope *pItem = (CMapItemEnvelope *)pLayers->Map()->GetItem(Start+Env, 0, 0);

	float Time = 0.0f;
	if(pThis->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = pThis->DemoPlayer()->BaseInfo();

		if(!pInfo->m_Paused || pThis->m_EnvelopeUpdate)
		{
			if(pThis->m_CurrentLocalTick != pInfo->m_CurrentTick)
			{
				pThis->m_LastLocalTick = pThis->m_CurrentLocalTick;
				pThis->m_CurrentLocalTick = pInfo->m_CurrentTick;
			}

			Time = mix(pThis->m_LastLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->m_CurrentLocalTick / (float)pThis->Client()->GameTickSpeed(),
						pThis->Client()->IntraGameTick());
		}

		pThis->RenderTools()->RenderEvalEnvelope(pPoints + pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
	else if(pThis->Client()->State() != IClient::STATE_OFFLINE)
	{
		if(pThis->m_pClient->m_Snap.m_pGameData && !(pThis->m_pClient->m_Snap.m_pGameData->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
		{
			if(pItem->m_Version < 2 || pItem->m_Synchronized)
			{
				Time = mix((pThis->Client()->PrevGameTick()-pThis->m_pClient->m_Snap.m_pGameData->m_GameStartTick) / (float)pThis->Client()->GameTickSpeed(),
							(pThis->Client()->GameTick()-pThis->m_pClient->m_Snap.m_pGameData->m_GameStartTick) / (float)pThis->Client()->GameTickSpeed(),
							pThis->Client()->IntraGameTick());
			}
			else
				Time = pThis->Client()->LocalTime()-pThis->m_OnlineStartTime;
		}

		pThis->RenderTools()->RenderEvalEnvelope(pPoints + pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
	else
	{
		Time = pThis->Client()->LocalTime();
		pThis->RenderTools()->RenderEvalEnvelope(pPoints + pItem->m_StartPoint, pItem->m_NumPoints, 4, Time+TimeOffset, pChannels);
	}
}

void CMapLayers::OnRender()
{
	if((Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK && !m_pMenuMap))
		return;

	CLayers *pLayers = 0;
	if(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		pLayers = Layers();
	else if(m_pMenuMap->IsLoaded())
		pLayers = m_pMenuLayers;

	if(!pLayers)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = *m_pClient->m_pCamera->GetCenter();

	bool PassedGameLayer = false;
	int GameLayerId = -1;
	CMapItemGroup *pGameGroup = NULL;
	CMapItemGroup *pRaceGroup = NULL;

	for(int g = 0; g < pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = pLayers->GetGroup(g);

		// don't print race group
		char aName[12];
		IntsToStr(pGroup->m_aName, sizeof(aName)/sizeof(int), aName);
		if(str_comp(aName, "#race") == 0)
		{
			pRaceGroup = pGroup;
			continue;
		}

		if(!g_Config.m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float Points[4];
			RenderTools()->MapScreenToGroup(Center.x, Center.y, pLayers->GameGroup(), m_pClient->m_pCamera->GetZoom());
			Graphics()->GetScreen(&Points[0], &Points[1], &Points[2], &Points[3]);
			float x0 = (pGroup->m_ClipX - Points[0]) / (Points[2]-Points[0]);
			float y0 = (pGroup->m_ClipY - Points[1]) / (Points[3]-Points[1]);
			float x1 = ((pGroup->m_ClipX+pGroup->m_ClipW) - Points[0]) / (Points[2]-Points[0]);
			float y1 = ((pGroup->m_ClipY+pGroup->m_ClipH) - Points[1]) / (Points[3]-Points[1]);

			if(x1 < 0.0f || x0 > 1.0f || y1 < 0.0f || y0 > 1.0f)
				continue;

			Graphics()->ClipEnable((int)(x0*Graphics()->ScreenWidth()), (int)(y0*Graphics()->ScreenHeight()),
				(int)((x1-x0)*Graphics()->ScreenWidth()), (int)((y1-y0)*Graphics()->ScreenHeight()));
		}

		RenderTools()->MapScreenToGroup(Center.x, Center.y, pGroup, m_pClient->m_pCamera->GetZoom());

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = pLayers->GetLayer(pGroup->m_StartLayer+l);
			bool Render = false;
			bool IsGameLayer = false;

			if(pLayer == (CMapItemLayer*)pLayers->GameLayer())
			{
				IsGameLayer = true;
				PassedGameLayer = 1;
				GameLayerId = l;
				pGameGroup = pGroup;
			}

			if(m_Type == -1)
				Render = true;
			else if(m_Type == 0)
			{
				if(PassedGameLayer && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))
					return;
				Render = true;
			}
			else
			{
				if(PassedGameLayer && !IsGameLayer)
					Render = true;
			}

			if(!Render)
				continue;

			// skip rendering if detail layers is not wanted
			if(!(pLayer->m_Flags&LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail && !IsGameLayer && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)))
			{
				if(pLayer->m_Type == LAYERTYPE_TILES && Input()->KeyIsPressed(KEY_LCTRL) && Input()->KeyIsPressed(KEY_LSHIFT) && Input()->KeyPress(KEY_KP_0))
				{
					CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
					CTile *pTiles = (CTile *)pLayers->Map()->GetData(pTMap->m_Data);
					CServerInfo CurrentServerInfo;
					Client()->GetServerInfo(&CurrentServerInfo);
					char aFilename[256];
					str_format(aFilename, sizeof(aFilename), "dumps/tilelayer_dump_%s-%d-%d-%dx%d.txt", CurrentServerInfo.m_aMap, g, l, pTMap->m_Width, pTMap->m_Height);
					IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
					if(File)
					{
						for(int y = 0; y < pTMap->m_Height; y++)
						{
							for(int x = 0; x < pTMap->m_Width; x++)
								io_write(File, &(pTiles[y*pTMap->m_Width + x].m_Index), sizeof(pTiles[y*pTMap->m_Width + x].m_Index));
							io_write_newline(File);
						}
						io_close(File);
					}
				}

				if(Render && !IsGameLayer
					&& !(g_Config.m_GfxGameTiles >= 2 && !g_Config.m_GfxKeepBackgroundAlways)
					&& !(g_Config.m_GfxGameTiles >= 2 && m_Type != TYPE_BACKGROUND && g_Config.m_GfxKeepBackgroundAlways)
					)
				{
				// if(!IsGameLayer)
				// {
					if(pLayer->m_Type == LAYERTYPE_TILES)
					{
						CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
						if(pTMap->m_Image == -1)
							Graphics()->TextureClear();
						else
							Graphics()->TextureSet(m_pClient->m_pMapimages->Get(pTMap->m_Image));

						CTile *pTiles = (CTile *)pLayers->Map()->GetData(pTMap->m_Data);
						Graphics()->BlendNone();
						vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);
						RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
														EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
						Graphics()->BlendNormal();
						RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
														EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
					}
					else if(pLayer->m_Type == LAYERTYPE_QUADS)
					{
						CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
						if(pQLayer->m_Image == -1)
							Graphics()->TextureClear();
						else
							Graphics()->TextureSet(m_pClient->m_pMapimages->Get(pQLayer->m_Image));

						CQuad *pQuads = (CQuad *)pLayers->Map()->GetDataSwapped(pQLayer->m_Data);

						//Graphics()->BlendNone();
						//RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_OPAQUE, EnvelopeEval, this);
						Graphics()->BlendNormal();
						RenderTools()->RenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this);
					}
				}
			}

			// eggs
			if(m_pClient->IsEaster())
			{
				CMapItemLayer *pNextLayer = pLayers->GetLayer(pGroup->m_StartLayer+l+1);
				if(m_pEggTiles && (l+1) < pGroup->m_NumLayers && pNextLayer == (CMapItemLayer*)pLayers->GameLayer())
				{
					Graphics()->TextureSet(m_pClient->m_pMapimages->GetEasterTexture());
					Graphics()->BlendNormal();
					RenderTools()->RenderTilemap(m_pEggTiles, m_EggLayerWidth, m_EggLayerHeight, 32.0f, vec4(1,1,1,1), LAYERRENDERFLAG_TRANSPARENT, EnvelopeEval, this, -1, 0);
				}
			}
		}
		if(!g_Config.m_GfxNoclip)
			Graphics()->ClipDisable();
	}

	// render automap/game tiles
	if(g_Config.m_GfxGameTiles && GameLayerId > -1)
	{
		RenderTools()->MapScreenToGroup(Center.x, Center.y, pGameGroup, m_pClient->m_pCamera->GetZoom());

		// useless calculation to get the game layer
		CMapItemLayer *pLayer = pLayers->GetLayer(pGameGroup->m_StartLayer+GameLayerId);
		CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
		dbg_assert(pLayer->m_Type == LAYERTYPE_TILES || pTMap->m_Image == -1, "not the game layer");

		CTile *pTiles;
		vec4 Color;
		{
			if(g_Config.m_GfxGameTiles == 1)
				Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a*0.8f/255.0f); // bit of alpha
			else if(g_Config.m_GfxGameTiles == 2)
				Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a*0.9f/255.0f); // lil bit of alpha
			else
				Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);
		}

		if(g_Config.m_GfxGameTiles == 3 && m_Type != TYPE_BACKGROUND)
		{
			dbg_assert(pLayers && (bool)pLayers->Map(), "no map");
			if(m_AutolayerUpdate) // need to update auto ressources
			{
				m_pClient->m_pMapimages->LoadAutoMapres();
				LoadPainters(pLayers);
				m_AutolayerUpdate = false;
			}

			// render doodads first
			if(g_Config.m_GfxAutomapDoodads && m_pAutoDoodads)
			{
				pTiles = m_pAutoDoodads;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetAutoDoodads());
				Graphics()->BlendNone();
				RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
												EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
				Graphics()->BlendNormal();
				RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
											EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
			}

			if(m_pAutoTiles)
			{
				pTiles = m_pAutoTiles;
				Graphics()->TextureSet(m_pClient->m_pMapimages->GetAutoTiles());
				Graphics()->BlendNone();
				RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
												EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
				Graphics()->BlendNormal();
				RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
												EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
			}
		}

		// the game tiles now
		Graphics()->TextureSet(g_Config.m_GfxGameTiles == 3 ? m_pClient->m_pMapimages->GetAutoEntities() : m_pClient->m_pMapimages->GetEntities());
		Graphics()->BlendNone();
		pTiles = (CTile *)pLayers->Map()->GetData(pTMap->m_Data);
		RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
										EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
		Graphics()->BlendNormal();
		RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
										EnvelopeEval, this, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);

		// and the race tiles
		if(pRaceGroup)
		{
			for(int l = 0; l < pRaceGroup->m_NumLayers; l++)
			{
				CMapItemLayer *pLayer = pLayers->GetLayer(pRaceGroup->m_StartLayer+l);
				if(pLayer->m_Type != LAYERTYPE_TILES)
					continue;
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
				
				char aName[12];
				IntsToStr(pTilemap->m_aName, sizeof(aName)/sizeof(int), aName);

				// teleport layer
				if(str_comp(aName, "#tele") == 0)
				{
					const int Width = pTilemap->m_Width;
					const int Height = pTilemap->m_Height;
					dbg_assert(Width <= pTMap->m_Width && Height <= pTMap->m_Height, "teleport layer is too big");
					if(m_TeleUpdate) // need to reload teleport stuff
					{
						delete m_pTeleTiles;

						CTile *pTeleTiles = (CTile *)pLayers->Map()->GetData(pTilemap->m_Data);

						// arrange tiles
						for(int i = 0 ; i < Width*Height; i++)
						{
							if(pTeleTiles[i].m_Index)
							{
								if(pTiles[i].m_Index == 26)
									pTeleTiles[i].m_Index = pTeleTiles[i].m_Index*2 - 1;
								else if(pTiles[i].m_Index == 27)
									pTeleTiles[i].m_Index = pTeleTiles[i].m_Index*2 - 2;
								else
									dbg_msg("automap", "Strange teleport, game ID = %d", pTiles[i].m_Index);
							}
						}
						m_pTeleTiles = (CTile *)malloc(sizeof(CTile) * Width * Height);
						mem_copy(m_pTeleTiles, pTeleTiles, sizeof(CTile) * Width * Height);
						m_TeleUpdate = false;
					}

					if(m_pTeleTiles)
					{
						// vec4 Color = vec4(1.f,1.f,1.f,1.f);
						Graphics()->TextureSet(m_pClient->m_pMapimages->GetTeleEntities());
						Graphics()->BlendNone();
						RenderTools()->RenderTilemap(m_pTeleTiles, Width, Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
														EnvelopeEval, this, pTilemap->m_ColorEnv, pTilemap->m_ColorEnvOffset);
						Graphics()->BlendNormal();
						RenderTools()->RenderTilemap(m_pTeleTiles, Width, Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
														EnvelopeEval, this, pTilemap->m_ColorEnv, pTilemap->m_ColorEnvOffset);
					}
				}
			}
		}
	}

	if(!g_Config.m_GfxNoclip)
		Graphics()->ClipDisable();

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}

// on auto mapres change
void CMapLayers::ReloadPainters()
{
	m_AutolayerUpdate = true;
}

void CMapLayers::LoadPainters(CLayers *pLayers)
{
	if(m_pTilesetPainter)
	{
		delete(m_pTilesetPainter);
		m_pTilesetPainter = 0;
		m_pAutoTiles = 0;
	}
	if(m_pDoodadsPainter)
	{
		delete(m_pDoodadsPainter);
		m_pDoodadsPainter = 0;
		m_pAutoDoodads = 0;
	}
	delete(m_pAutoTiles);
	delete(m_pAutoDoodads);

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s_main", g_Config.m_GfxAutomapLayer);
	LoadAutomapperRules(pLayers, aBuf);
	str_format(aBuf, sizeof(aBuf), "%s_doodads", g_Config.m_GfxAutomapLayer);
	LoadAutomapperRules(pLayers, aBuf);
	
	// fill m_pAutoTiles
	if(m_pTilesetPainter)
	{
		CMapItemLayerTilemap *pTMap = pLayers->GameLayer();
		CTile *pTilesOriginal = ((CTile *)pLayers->Map()->GetData(pTMap->m_Data));
		m_pAutoTiles = (CTile *)malloc(sizeof(CTile) * pTMap->m_Width * pTMap->m_Height);
		mem_copy(m_pAutoTiles, pTilesOriginal, sizeof(CTile) * pTMap->m_Width * pTMap->m_Height);

		// cleanup game tiles from anything but hookable and unhookable
		int MaxIndex = pTMap->m_Width * pTMap->m_Height;	
		for(int i = 0 ; i < MaxIndex; i++)
		{
			if(m_pAutoTiles[i].m_Index != CCollision::COLFLAG_SOLID && m_pAutoTiles[i].m_Index != (CCollision::COLFLAG_NOHOOK|CCollision::COLFLAG_SOLID)) // hookable and unhookable
			{
				m_pAutoTiles[i].m_Index = 0;
				m_pAutoTiles[i].m_Flags = 0;
			}
		}

		// proceed tiles
		m_pTilesetPainter->Proceed(m_pAutoTiles, pTMap->m_Width, pTMap->m_Height, 0);

		if(m_pDoodadsPainter)
		{
			// copy the collision from the tiles
			m_pAutoDoodads = (CTile *)malloc(sizeof(CTile) * pTMap->m_Width * pTMap->m_Height);
			mem_copy(m_pAutoDoodads, m_pAutoTiles, sizeof(CTile) * pTMap->m_Width * pTMap->m_Height);

			// proceed
			m_pDoodadsPainter->Proceed(m_pAutoDoodads, pTMap->m_Width, pTMap->m_Height, 0, 1);
		}
	}
}

// void CMapLayers::LoadTeleports(CLayers *pLayers)
// {
// 	delete(m_pTeleTiles);
// }

void CMapLayers::LoadAutomapperRules(CLayers *pLayers, const char* pName)
{
	// read file data into buffer
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "editor/automap/%s.json", pName);
	IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
		return;
	int FileSize = (int)io_length(File);
	char *pFileData = (char *)mem_alloc(FileSize, 1);
	io_read(File, pFileData, FileSize);
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
	mem_free(pFileData);

	if(pJsonData == 0)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, aBuf, aError);
		return;
	}

	// generate configurations
	const json_value &rTileset = (*pJsonData)[(const char *)IAutoMapper::GetTypeName(IAutoMapper::TYPE_TILESET)];
	if(rTileset.type == json_array)
	{
		m_pTilesetPainter = new CTilesetPainter(pLayers);
		m_pTilesetPainter->Load(rTileset);
	}
	else
	{
		const json_value &rDoodads = (*pJsonData)[(const char *)IAutoMapper::GetTypeName(IAutoMapper::TYPE_DOODADS)];
		if(rDoodads.type == json_array)
		{
			m_pDoodadsPainter = new CDoodadsPainter(pLayers);
			m_pDoodadsPainter->Load(rDoodads);
		}
	}

	// clean up
	json_value_free(pJsonData);
	// if(m_pTilesetPainter && g_Config.m_Debug)
	// {
	// 	str_format(aBuf, sizeof(aBuf),"loaded %s.json (%s)", pName, IAutoMapper::GetTypeName(m_pTilesetPainter->GetType()));
	// 	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);
	// }
}

void CMapLayers::ConchainBackgroundMap(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CMapLayers*)pUserData)->BackgroundMapUpdate();
}

void CMapLayers::ConchainAutomapperReload(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		((CMapLayers*)pUserData)->ReloadPainters();
	}
}

void CMapLayers::Con_Automap(IConsole::IResult *pResult, void *pUserData)
{
	CMapLayers *pSelf = (CMapLayers *)pUserData;
	if(!pResult->GetString(0)[0])
	{
		g_Config.m_GfxGameTiles = 0;
		return;
	}
	g_Config.m_GfxGameTiles = 3;
	str_copy(g_Config.m_GfxAutomapLayer, pResult->GetString(0), sizeof(g_Config.m_GfxAutomapLayer));
	pSelf->ReloadPainters();
}


void CMapLayers::OnConsoleInit()
{
	Console()->Chain("cl_menu_map", ConchainBackgroundMap, this);
	Console()->Chain("cl_show_menu_map", ConchainBackgroundMap, this);
	Console()->Chain("gfx_automap_layer", ConchainAutomapperReload, this);

	Console()->Register("automap", "s", CFGFLAG_CLIENT, Con_Automap, this, "Automap the specified layer");
}

void CMapLayers::BackgroundMapUpdate()
{
	if(m_Type == TYPE_BACKGROUND && m_pMenuMap)
	{
		// unload map
		m_pMenuMap->Unload();

		LoadBackgroundMap();
	}
}
