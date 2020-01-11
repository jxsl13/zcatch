#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <game/client/animstate.h>
#include <game/client/components/announcers.h>
#include <game/client/components/menus.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <generated/client_data.h>
#include "stats.h"

CStats::CStats()
{
	OnReset();
}

void CStats::CPlayerStats::Reset()
{
	m_IngameTicks		= 0;
	m_Frags				= 0;
	m_Deaths			= 0;
	m_Suicides			= 0;
	m_BestSpree			= 0;
	m_CurrentSpree		= 0;
	for(int j = 0; j < NUM_WEAPONS; j++)
	{
		m_aFragsWith[j]		= 0;
		m_aDeathsFrom[j]	= 0;
	}
	m_FlagGrabs			= 0;
	m_FlagCaptures		= 0;
	m_CarriersKilled	= 0;
	m_KillsCarrying		= 0;
	m_DeathsCarrying	= 0;
	m_CurrentHumiliation	= 0;
	// m_StatsClientID = -1;
}

void CStats::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aStats[i].Reset();

	m_Active = false;
	m_Activate = false;

	m_ScreenshotTaken = false;
	m_ScreenshotTime = -1;
}

bool CStats::IsActive() const
{
	// force statboard after three seconds of game over if autostatscreenshot is on
	if(g_Config.m_ClAutoStatScreenshot && m_ScreenshotTime > -1 && m_ScreenshotTime < time_get())
		return true;

	return m_Active;
	// return (m_Mode > 0);
}

/*
void CTeecompStats::CheckStatsClientID()
{
	if(m_StatsClientID == -1)
		m_StatsClientID = m_pClient->m_LocalClientID;

	int Prev = m_StatsClientID;
	while(!m_pClient->m_aStats[m_StatsClientID].m_Active)
	{
		m_StatsClientID++;
		m_StatsClientID %= MAX_CLIENTS;
		if(m_StatsClientID == Prev)
		{
			m_StatsClientID = -1;
			m_Mode = 0;
			break;
		}
	}
}
*/

void CStats::OnRelease()
{
	m_Active = false;
}

void CStats::ConKeyStats(IConsole::IResult *pResult, void *pUserData)
{
	CStats *pStats = (CStats *)pUserData;
	int Result = pResult->GetInteger(0);
	if(!Result)
	{
		pStats->m_Activate = false;
		pStats->m_Active = false;
	}
	else if(!pStats->m_Active)
		pStats->m_Activate = true;
}

void CStats::OnConsoleInit()
{
	Console()->Register("+stats", "", CFGFLAG_CLIENT, ConKeyStats, this, "Show stats");
}

void CStats::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		// CGameClient::CClientStats *pStats = m_pClient->m_aStats;
		CPlayerStats *pStats = this->m_aStats;

		// HolyShit sound
		if(g_Config.m_ClGSound && m_pClient->m_LocalClientID == pMsg->m_Victim && pStats[pMsg->m_Victim].m_CurrentSpree >= 5)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Ended (%d kills)", pStats[pMsg->m_Victim].m_CurrentSpree);
			m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_HOLYSHIT, 0);
					m_pClient->m_pAnnouncers->Announce("HOLY SHIT", aBuf, 2.0f);
		}			
			
		// Humiliation is not about selfkilling
		if(pMsg->m_Victim != pMsg->m_Killer && pMsg->m_Victim == m_pClient->m_LocalClientID)
		{
			if(pStats[pMsg->m_Victim].m_CurrentSpree)
				pStats[pMsg->m_Victim].m_CurrentHumiliation = 1;
			else
				pStats[pMsg->m_Victim].m_CurrentHumiliation++;
			if(pStats[pMsg->m_Victim].m_CurrentHumiliation % 7 == 0)
			{
				m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_HUMILIATION, 0);
				m_pClient->m_pAnnouncers->Announce("HUMILIATION", "Killed 7 times in a row", 3.5f);
			}
		}
		
		if(pMsg->m_Weapon != -3)	// team switch
			m_aStats[pMsg->m_Victim].m_Deaths++;
		m_aStats[pMsg->m_Victim].m_CurrentSpree = 0;
		if(pMsg->m_Weapon >= 0)
			m_aStats[pMsg->m_Victim].m_aDeathsFrom[pMsg->m_Weapon]++;
		if((pMsg->m_ModeSpecial & 1) && (pMsg->m_Weapon != -3))
			m_aStats[pMsg->m_Victim].m_DeathsCarrying++;
		if(pMsg->m_Victim != pMsg->m_Killer)
		{
			pStats[pMsg->m_Killer].m_Frags++;
			pStats[pMsg->m_Killer].m_CurrentSpree++;

			// play spree sounds
			if(g_Config.m_ClGSound && m_pClient->m_LocalClientID == pMsg->m_Killer && pStats[pMsg->m_Killer].m_CurrentSpree % 5 == 0)
			{
				pStats[pMsg->m_Killer].m_CurrentHumiliation = 0;
				int SpreeType = pStats[pMsg->m_Killer].m_CurrentSpree/5 - 1;
				switch(SpreeType)
				{
				case 0:
					m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_KILLING, 0);
					m_pClient->m_pAnnouncers->Announce("KILLING SPREE", "5 kills in a row", 3.5f);
					break;
				case 1:
					m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_RAMPAGE, 0);
					m_pClient->m_pAnnouncers->Announce("RAMPAGE", "10 kills in a row", 1.5f);
					break;
				case 2:
					m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_DOMINATING, 0);
					m_pClient->m_pAnnouncers->Announce("DOMINATING", "15 kills in a row", 3.0f);
					break;
				case 3:
					m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_UNSTOPPABLE, 0);
					m_pClient->m_pAnnouncers->Announce("UNSTOPPABLE", "20 kills in a row", 3.0f);
					break;
				case 4:
					m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_GODLIKE, 0);
					m_pClient->m_pAnnouncers->Announce("GODLIKE", "25 kills in a row", 2.5f);
					break;
				case 5:
					m_pClient->m_pSounds->Play(CSounds::CHN_GUI, SOUND_SPREE_WICKEDSICK, 0);
					m_pClient->m_pAnnouncers->Announce("WICKED SICK", "30 kills in a row", 1.8f);
					break;
				}
			}

			if(pStats[pMsg->m_Killer].m_CurrentSpree > pStats[pMsg->m_Killer].m_BestSpree)
				pStats[pMsg->m_Killer].m_BestSpree = pStats[pMsg->m_Killer].m_CurrentSpree;
			if(pMsg->m_Weapon >= 0)
				pStats[pMsg->m_Killer].m_aFragsWith[pMsg->m_Weapon]++;
			if(pMsg->m_ModeSpecial & 1)
				pStats[pMsg->m_Killer].m_CarriersKilled++;
			if(pMsg->m_ModeSpecial & 2)
				pStats[pMsg->m_Killer].m_KillsCarrying++;
		}
		else if(pMsg->m_Weapon != -3)
			m_aStats[pMsg->m_Victim].m_Suicides++;
	}
/*	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		CGameClient::CClientStats *pStats = m_pClient->m_aStats;
		
		pStats[pMsg->m_Victim].m_Deaths++;
		pStats[pMsg->m_Victim].m_CurrentSpree = 0;
		if(pMsg->m_Weapon >= 0)
			pStats[pMsg->m_Victim].m_aDeathsFrom[pMsg->m_Weapon]++;
		if(pMsg->m_ModeSpecial & 1)
			pStats[pMsg->m_Victim].m_DeathsCarrying++;
		if(pMsg->m_Victim != pMsg->m_Killer)
		{
			pStats[pMsg->m_Killer].m_Frags++;
			pStats[pMsg->m_Killer].m_CurrentSpree++;

			if(pStats[pMsg->m_Killer].m_CurrentSpree > pStats[pMsg->m_Killer].m_BestSpree)
				pStats[pMsg->m_Killer].m_BestSpree = pStats[pMsg->m_Killer].m_CurrentSpree;
			if(pMsg->m_Weapon >= 0)
				pStats[pMsg->m_Killer].m_aFragsWith[pMsg->m_Weapon]++;
			if(pMsg->m_ModeSpecial & 1)
				pStats[pMsg->m_Killer].m_CarriersKilled++;
			if(pMsg->m_ModeSpecial & 2)
				pStats[pMsg->m_Killer].m_KillsCarrying++;
		}
		else
			pStats[pMsg->m_Victim].m_Suicides++;
	}*/
}

void CStats::OnRender()
{
	// auto stat screenshot stuff
	if(g_Config.m_ClAutoStatScreenshot)
	{
		// on game over, wait three seconds
		if(m_ScreenshotTime < 0 && m_pClient->m_Snap.m_pGameData && m_pClient->m_Snap.m_pGameData->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
			m_ScreenshotTime = time_get()+time_freq()*3;

		// when rendered, take screenshot once
		if(!m_ScreenshotTaken && m_ScreenshotTime > -1 && m_ScreenshotTime+time_freq()/5 < time_get())
		{
			AutoStatScreenshot();
			m_ScreenshotTaken = true;
		}
	}

	// don't render scoreboard if menu is open
	if(m_pClient->m_pMenus->IsActive())
		return;

	// postpone the active state till the render area gets updated during the rendering
	if(m_Activate)
	{
		m_Active = true;
		m_Activate = false;
	}

	if(!IsActive())
		return;

	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;
	float w = 250.0f;
	float h = 750.0f;

	int apPlayers[MAX_CLIENTS] = {0};
	int NumPlayers = 0;
	int i;
	for(i=0; i<MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_aClients[i].m_Active)
			continue;
		if(m_pClient->m_aClients[i].m_Team == TEAM_SPECTATORS)
			continue;

		apPlayers[NumPlayers] = i;
		NumPlayers++;
	}

	for(int i=0; i<9; i++)
		if(g_Config.m_ClStatboardInfos & (1<<i))
		{
			if((1<<i) == (TC_STATS_DEATHS) && g_Config.m_ClStatboardInfos & TC_STATS_FRAGS)
			{
				w += 60; // some extra for the merge
				continue;
			}
			else if((1<<i) == (TC_STATS_BESTSPREE))
			{
				if(!(g_Config.m_ClStatboardInfos & TC_STATS_SPREE))
					w += 140; // Best spree is a long column name, add a bit more
				else
					w += 40; // The combined colunms are a bit long, add some extra
				continue;
			}
			else if((1<<i) == (TC_STATS_FLAGGRABS) && !(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS))
				continue;
			w += 100;
		}

	if((m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS) && (g_Config.m_ClStatboardInfos&TC_STATS_FLAGCAPTURES))
		w += 100;

	bool aDisplayWeapon[NUM_WEAPONS] = {false};
	bool NoDisplayedWeapon = true;
	if(g_Config.m_ClStatboardInfos & TC_STATS_WEAPS)
	{
		for(i=0; i<NumPlayers; i++)
		{
			const CPlayerStats *pStats = &m_aStats[apPlayers[i]];
			for(int j=0; j<NUM_WEAPONS; j++)
				aDisplayWeapon[j] = aDisplayWeapon[j] || pStats->m_aFragsWith[j] || pStats->m_aDeathsFrom[j];
		}
		for(i=0; i<NUM_WEAPONS; i++)
			if(aDisplayWeapon[i])
			{
				w += 80;
				NoDisplayedWeapon = false;
			}
		if(!NoDisplayedWeapon)
			w += 10;
	}

	float x = Width/2-w/2;
	float y = 200.0f;

	Graphics()->MapScreen(0, 0, Width, Height);

	Graphics()->BlendNormal();
	{
		CUIRect Rect = {x-10.f, y-10.f, w, h};
		RenderTools()->DrawRoundRect(&Rect, vec4(0,0,0,0.5f), 17.0f);
	}

	float tw;
	int px = 325;

	TextRender()->Text(0, x+10, y-5, 20.0f, Localize("Name"), -1.0f);
	const char *apHeaders[] = { "K", "D", Localize("Suicides"), Localize("Ratio"), Localize("Net", "Net score"), Localize("FPM"), Localize("Spree"), Localize("Best spree"), Localize("Grabs", "Flag grabs") };
	for(i=0; i<9; i++)
		if(g_Config.m_ClStatboardInfos & (1<<i))
		{
			const char* pText = apHeaders[i];
			// handle K:D merge (in the frags column)
			if(1<<i == TC_STATS_FRAGS && g_Config.m_ClStatboardInfos & TC_STATS_DEATHS)
			{
				pText = "K:D";
				px += 60.0f; // some extra for the merge
			}
			else if(1<<i == TC_STATS_DEATHS && g_Config.m_ClStatboardInfos & TC_STATS_FRAGS)
				continue;
			// handle spree columns merge
			if(1<<i == TC_STATS_BESTSPREE)
			{
				if(g_Config.m_ClStatboardInfos & TC_STATS_SPREE)
					continue;
				px += 40.0f; // some extra for the long name
			}
			else if(1<<i == TC_STATS_SPREE && g_Config.m_ClStatboardInfos & TC_STATS_BESTSPREE)
				px += 40.0f; // some extra for the merge
			if(1<<i == TC_STATS_FLAGGRABS && !(m_pClient->m_Snap.m_pGameData && m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS))
				continue;
			tw = TextRender()->TextWidth(0, 20.0f, pText, -1, -1.0f);
			TextRender()->Text(0, x+px-tw, y-5, 20.0f, pText, -1.0f);
			px += 100;
		}

	// sprite headers now
	if(g_Config.m_ClStatboardInfos & TC_STATS_WEAPS)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		for(i=0; i<NUM_WEAPONS; i++)
		{
			if(!aDisplayWeapon[i])
				continue;

			RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteBody);
			if(i == 0)
				RenderTools()->DrawSprite(x+px-40, y+10, g_pData->m_Weapons.m_aId[i].m_VisualSize*0.8);
			else
				RenderTools()->DrawSprite(x+px-40, y+10, g_pData->m_Weapons.m_aId[i].m_VisualSize);
			px += 80;
		}
		Graphics()->QuadsEnd();
		if(!NoDisplayedWeapon)
			px += 10;
	}

	if(m_pClient->m_Snap.m_pGameData && m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS && g_Config.m_ClStatboardInfos&TC_STATS_FLAGCAPTURES)
	{
		px -= 40;
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(-0.39f);
		RenderTools()->SelectSprite(SPRITE_FLAG_BLUE, SPRITE_FLAG_FLIP_X);
		RenderTools()->DrawSprite(x+px-10, y+12.5f, 48);
		Graphics()->QuadsSetRotation(0.39f);
		RenderTools()->SelectSprite(SPRITE_FLAG_RED);
		RenderTools()->DrawSprite(x+px+10, y+12.5f, 48);
		Graphics()->QuadsEnd();
	}

	y += 29.0f;

	float FontSize = 30.0f;
	float LineHeight = 50.0f;
	float TeeSizemod = 1.0f;
	float TeeOffset = 0.0f;

	if(NumPlayers > 14)
	{
		FontSize = 30.0f;
		LineHeight = 40.0f;
		TeeSizemod = 0.8f;
		TeeOffset = -5.0f;
	}
	for(int j=0; j<NumPlayers; j++)
	{
		// workaround
		if(j == 16)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "... %d other players", NumPlayers-j);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x+64, y, FontSize, aBuf, -1.0f);
			px += 100;
			break;
		}

		const CPlayerStats *pStats = &m_aStats[apPlayers[j]];		
		const bool HighlightedLine = apPlayers[j] == m_pClient->m_LocalClientID
			|| (m_pClient->m_Snap.m_SpecInfo.m_Active && apPlayers[j] == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID);

		// background so it's easy to find the local player or the followed one in spectator mode
		if(HighlightedLine)
		{
			CUIRect Rect = {x, y, w-20, LineHeight*0.95f};
			RenderTools()->DrawRoundRect(&Rect, vec4(1,1,1,0.25f), 17.0f);
		}

		CTeeRenderInfo Teeinfo = m_pClient->m_aClients[apPlayers[j]].m_RenderInfo;
		Teeinfo.m_Size *= TeeSizemod;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &Teeinfo, EMOTE_NORMAL, vec2(1,0), vec2(x+28, y+28+TeeOffset));

		char aBuf[128];
		CTextCursor Cursor;
		tw = TextRender()->TextWidth(0, FontSize, m_pClient->m_aClients[apPlayers[j]].m_aName, -1, -1.0f);
		TextRender()->SetCursor(&Cursor, x+64, y, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = 220;
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[apPlayers[j]].m_aName, -1);

		px = 325;
		if(g_Config.m_ClStatboardInfos & TC_STATS_FRAGS)
		{
			if(g_Config.m_ClStatboardInfos & TC_STATS_DEATHS)
			{
				px += 60;
				str_format(aBuf, sizeof(aBuf), "%d:%d", pStats->m_Frags, pStats->m_Deaths);
			}
			else
				str_format(aBuf, sizeof(aBuf), "%d", pStats->m_Frags);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		else if(g_Config.m_ClStatboardInfos & TC_STATS_DEATHS)
		{
			str_format(aBuf, sizeof(aBuf), "%d", pStats->m_Deaths);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		if(g_Config.m_ClStatboardInfos & TC_STATS_SUICIDES)
		{
			str_format(aBuf, sizeof(aBuf), "%d", pStats->m_Suicides);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		if(g_Config.m_ClStatboardInfos & TC_STATS_RATIO)
		{
			if(pStats->m_Deaths == 0)
				str_format(aBuf, sizeof(aBuf), "--");
			else
				str_format(aBuf, sizeof(aBuf), "%.2f", (float)(pStats->m_Frags)/pStats->m_Deaths);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		if(g_Config.m_ClStatboardInfos & TC_STATS_NET)
		{
			str_format(aBuf, sizeof(aBuf), "%+d", pStats->m_Frags-pStats->m_Deaths);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		if(g_Config.m_ClStatboardInfos & TC_STATS_FPM)
		{
			float Fpm = pStats->m_IngameTicks > 0 ? (float)(pStats->m_Frags * Client()->GameTickSpeed() * 60) / pStats->m_IngameTicks : 0.f;
			str_format(aBuf, sizeof(aBuf), "%.1f", Fpm);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		if(g_Config.m_ClStatboardInfos & TC_STATS_SPREE)
		{
			if(g_Config.m_ClStatboardInfos & TC_STATS_BESTSPREE)
			{
				px += 40; // extra space
				str_format(aBuf, sizeof(aBuf), "%d (%d)", pStats->m_CurrentSpree, pStats->m_BestSpree);
			}
			else
				str_format(aBuf, sizeof(aBuf), "%d", pStats->m_CurrentSpree);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		else if(g_Config.m_ClStatboardInfos & TC_STATS_BESTSPREE)
		{
			px += 40;
			str_format(aBuf, sizeof(aBuf), "%d", pStats->m_BestSpree);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		if(m_pClient->m_Snap.m_pGameData && m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS && g_Config.m_ClStatboardInfos&TC_STATS_FLAGGRABS)
		{
			str_format(aBuf, sizeof(aBuf), "%d", pStats->m_FlagGrabs);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1.0f);
			px += 100;
		}
		px -= 40;
		if(g_Config.m_ClStatboardInfos & TC_STATS_WEAPS)
		{
			const float BarHeight = 0.3f*LineHeight;
			const float Offset = 40.0f;
			const float StartX = px - Offset;
			const float RoundSize = BarHeight/2.0f;
			float EndX = StartX; // each bar will have its width incremented by the roundsize so this avoids that last one would overflow
			int TotalFrags = 0;
			int TotalDeaths = 0;
			for(i=0; i<NUM_WEAPONS; i++)
			{
				if(aDisplayWeapon[i])
				{
					EndX += 80.0f;
					TotalFrags += pStats->m_aFragsWith[i];
					TotalDeaths += pStats->m_aDeathsFrom[i];
				}					
			}
			float ExploitableLength = (EndX-StartX) - RoundSize;
			CUIRect Rect = {x + StartX, y+0.3f*LineHeight, 0.0f, BarHeight};
			for(i=0; i<NUM_WEAPONS; i++)
			{
				extern int _dummy[(int)(NUM_WEAPONS == 6)]; (void)_dummy; // static assert that there are 6 weapons
				static const vec4 Colors[NUM_WEAPONS] =
				{
					// crosshair colors
					vec4(201/255.0f, 197/255.0f, 205/255.0f, 1.0f),
					vec4(156/255.0f, 158/255.0f, 100/255.0f, 1.0f),
					vec4(98/255.0f, 80/255.0f, 46/255.0f, 1.0f),
					vec4(163/255.0f, 51/255.0f, 56/255.0f, 1.0f),
					vec4(65/255.0f, 97/255.0f, 161/255.0f, 1.0f),
					vec4(182/255.0f, 137/255.0f, 40/255.0f, 1.0f),
				};
				if(pStats->m_aFragsWith[i])
				{
					Rect.w = ExploitableLength * pStats->m_aFragsWith[i] / (float)TotalFrags;
					Rect.w += RoundSize;
					RenderTools()->DrawRoundRect(&Rect, Colors[i], RoundSize);
					Rect.w -= RoundSize;
					Rect.x += Rect.w;
				}
			}
			px = EndX + Offset;
			if(!NoDisplayedWeapon)
				px += 10;
		}

		if(m_pClient->m_Snap.m_pGameData && m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS && g_Config.m_ClStatboardInfos&TC_STATS_FLAGCAPTURES)
		{
			if(pStats->m_FlagCaptures <= 0)
			{
				tw = TextRender()->TextWidth(0, FontSize, "--", -1, -1.0f);
				TextRender()->Text(0, x-tw/2.0f+px, y, FontSize, "--", -1.0f);
			}
			else
			{
				const bool DigitalDisplay = pStats->m_FlagCaptures > 5;
				const int DisplayedFlagsCount = !DigitalDisplay ? pStats->m_FlagCaptures : 1;
				const float Space = 15.0f;
				int TempX = px - ((DisplayedFlagsCount-1)*Space/2.0f);
				TempX += 2.5f; // some offset
				if(DigitalDisplay)
					TempX -= 20;

				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
				Graphics()->QuadsBegin();
				for(int n = 0; n < DisplayedFlagsCount; n++)
				{
					Graphics()->QuadsSetRotation(0.18f);
					if(m_pClient->m_aClients[apPlayers[j]].m_Team == TEAM_RED)
						RenderTools()->SelectSprite(SPRITE_FLAG_BLUE);
					else
						RenderTools()->SelectSprite(SPRITE_FLAG_RED);
					RenderTools()->DrawSprite(x+TempX, y+25, 48);
					TempX += Space;
				}
				Graphics()->QuadsEnd();

				if(DigitalDisplay)
				{
					str_format(aBuf, sizeof(aBuf), "x%d", pStats->m_FlagCaptures);
					TextRender()->Text(0, x+TempX, y, FontSize, aBuf, -1.0f);
				}
			}
			px += 100;
		}
		y += LineHeight;
	}
}

void CStats::UpdatePlayTime(int Ticks)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pClient->m_aClients[i].m_Active && m_pClient->m_aClients[i].m_Team != TEAM_SPECTATORS)
			m_aStats[i].m_IngameTicks += Ticks;
	}
}

void CStats::OnMatchStart()
{
	OnReset();
}

void CStats::OnFlagGrab(int ClientID)
{
	m_aStats[ClientID].m_FlagGrabs++;
}

void CStats::OnFlagCapture(int ClientID)
{
	m_aStats[ClientID].m_FlagCaptures++;
}

void CStats::OnPlayerEnter(int ClientID, int Team)
{
	m_aStats[ClientID].Reset();
}

void CStats::OnPlayerLeave(int ClientID)
{
	m_aStats[ClientID].Reset();
}

void CStats::AutoStatScreenshot()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Client()->AutoStatScreenshot_Start();
}

// unused yet
/*
void CStats::RenderIndividualStats()
{
	if(m_Mode != 2)
		return;
	CheckStatsClientID();
	if(m_Mode != 2)
		return;
	int m_ClientID = m_StatsClientID;
	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;
	float w = 1200.0f;
	float x = Width/2-w/2;
	float y = 100.0f;
	float xo = 200.0f;
	float FontSize = 30.0f;
	float LineHeight = 40.0f;
	const CGameClient::CClientStats m_aStats = m_pClient->m_aStats[m_ClientID];

	Graphics()->MapScreen(0, 0, Width, Height);

	// header with name and score
	Graphics()->BlendNormal();
	// Graphics()->TextureSet(-1);
	{
		CUIRect Rect = {x-10.f, y-10.f, w, 120.0f};
		RenderTools()->DrawRoundRect(&Rect, vec4(0,0,0,0.5f), 17.0f);
	}

	CTeeRenderInfo Teeinfo = m_pClient->m_aClients[m_ClientID].m_RenderInfo;
	Teeinfo.m_Size *= 1.5f;
	RenderTools()->RenderTee(CAnimState::GetIdle(), &Teeinfo, EMOTE_NORMAL, vec2(1,0), vec2(x+xo+32, y+36));
	TextRender()->Text(0, x+xo+128, y, 48.0f, m_pClient->m_aClients[m_ClientID].m_aName, -1);

	char aBuf[64];
	if(g_Config.m_TcStatId)
	{
		str_format(aBuf, sizeof(aBuf), "%d", m_ClientID);
		TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
	}

	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score"), m_pClient->m_Snap.m_paPlayerInfos[m_ClientID]->m_Score);
	TextRender()->Text(0, x+xo, y+64, FontSize, aBuf, -1);
	int Seconds = (float)(Client()->GameTick()-m_aStats.m_JoinDate)/Client()->GameTickSpeed();
	str_format(aBuf, sizeof(aBuf), "%s: %02d:%02d", Localize("Time played"), Seconds/60, Seconds%60);
	TextRender()->Text(0, x+xo+256, y+64, FontSize, aBuf, -1);

	y += 150.0f;

	// Frags, etc. stats
	Graphics()->BlendNormal();
	// Graphics()->TextureSet(-1);
	{
		CUIRect Rect = {x-10.f, y-10.f, w, 100.0f};
		RenderTools()->DrawRoundRect(&Rect, vec4(0,0,0,0.5f), 17.0f);
	}

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f,1.0f,1.0f,1.0f);
	RenderTools()->SelectSprite(SPRITE_EYES);
	IGraphics::CQuadItem QuadItem(x+xo/2, y+40, 128, 128);
	Graphics()->QuadsDraw(&QuadItem, 1);
	Graphics()->QuadsEnd();

	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Frags"), m_aStats.m_Frags);
	TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Deaths"), m_aStats.m_Deaths);
	TextRender()->Text(0, x+xo+200.0f, y, FontSize, aBuf, -1);
	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Suicides"), m_aStats.m_Suicides);
	TextRender()->Text(0, x+xo+400.0f, y, FontSize, aBuf, -1);
	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Spree"), m_aStats.m_CurrentSpree);
	TextRender()->Text(0, x+xo+600.0f, y, FontSize, aBuf, -1);
	y += LineHeight;

	if(m_aStats.m_Deaths == 0)
		str_format(aBuf, sizeof(aBuf), "%s: --", Localize("Ratio"));
	else
		str_format(aBuf, sizeof(aBuf), "%s: %.2f", Localize("Ratio"), (float)(m_aStats.m_Frags)/m_aStats.m_Deaths);
	TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Net"), m_aStats.m_Frags-m_aStats.m_Deaths);
	TextRender()->Text(0, x+xo+200.0f, y, FontSize, aBuf, -1);
	float Fpm = (float)(m_aStats.m_Frags*60)/((float)(Client()->GameTick()-m_aStats.m_JoinDate)/Client()->GameTickSpeed());
	str_format(aBuf, sizeof(aBuf), "%s: %.1f", Localize("FPM"), Fpm);
	TextRender()->Text(0, x+xo+400.0f, y, FontSize, aBuf, -1);
	str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Best spree"), m_aStats.m_BestSpree);
	TextRender()->Text(0, x+xo+600.0f, y, FontSize, aBuf, -1);
	y+= LineHeight + 30.0f;

	// Weapon stats
	bool aDisplayWeapon[NUM_WEAPONS] = {false};
	int NumWeaps = 0;
	for(int i=0; i<NUM_WEAPONS; i++)
		if(m_aStats.m_aFragsWith[i] || m_aStats.m_aDeathsFrom[i])
		{
			aDisplayWeapon[i] = true;
			NumWeaps++;
		}

	if(NumWeaps)
	{
		Graphics()->BlendNormal();
		// Graphics()->TextureSet(-1);
		{
			CUIRect Rect = {x-10.f, y-10.f, w, LineHeight*(1+NumWeaps)+20.0f};
			RenderTools()->DrawRoundRect(&Rect, vec4(0,0,0,0.5f), 17.0f);
		}

		TextRender()->Text(0, x+xo, y, FontSize, Localize("Frags"), -1);
		TextRender()->Text(0, x+xo+200.0f, y, FontSize, Localize("Deaths"), -1);
		y += LineHeight;

		for(int i=0; i<NUM_WEAPONS; i++)
		{
			if(!aDisplayWeapon[i])
				continue;

			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();
			RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, 0);
			RenderTools()->DrawSprite(x+xo/2, y+24, g_pData->m_Weapons.m_aId[i].m_VisualSize);
			Graphics()->QuadsEnd();

			str_format(aBuf, sizeof(aBuf), "%d", m_aStats.m_aFragsWith[i]);
			TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
			str_format(aBuf, sizeof(aBuf), "%d", m_aStats.m_aDeathsFrom[i]);
			TextRender()->Text(0, x+xo+200.0f, y, FontSize, aBuf, -1);
			y += LineHeight;
		}
		y += 30.0f;
	}

	// Flag stats
	if(m_pClient->m_GameInfo.m_GameFlags&GAMEFLAG_FLAGS)
	{
		Graphics()->BlendNormal();
		// Graphics()->TextureSet(-1);
		{
			CUIRect Rect = {x-10.f, y-10.f, w, LineHeight*5+20.0f};
			RenderTools()->DrawRoundRect(&Rect, vec4(0,0,0,0.5f), 17.0f);
		}

		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_FLAG_RED);
		RenderTools()->DrawSprite(x+xo/2, y+100.0f, 192);
		Graphics()->QuadsEnd();

		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Grabs"), m_aStats.m_FlagGrabs);
		TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
		y += LineHeight;
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Captures"), m_aStats.m_FlagCaptures);
		TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
		y += LineHeight;
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Kills holding flag"), m_aStats.m_KillsCarrying);
		TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
		y += LineHeight;
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Deaths with flag"), m_aStats.m_DeathsCarrying);
		TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
		y += LineHeight;
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Carriers killed"), m_aStats.m_CarriersKilled);
		TextRender()->Text(0, x+xo, y, FontSize, aBuf, -1);
		y += LineHeight;
	}
}
*/
