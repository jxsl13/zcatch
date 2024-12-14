/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "engine/shared/config.h"
#include "entities/character.h"
#include "entities/flag.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"

#include <algorithm>


MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, bool Dummy, bool AsSpec)
{
	m_pGameServer = pGameServer;
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_LastRespawnedTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	m_pCharacter = 0;
	m_ClientID = ClientID;
	m_Team = AsSpec ? TEAM_SPECTATORS : GameServer()->m_pController->GetStartTeam();
	m_SpecMode = SPEC_FREEVIEW;
	m_SpectatorID = -1;
	m_pSpecFlag = 0;
	m_ActiveSpecSwitch = 0;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();
	m_InactivityTickCounter = 0;
	m_Dummy = Dummy;
	m_IsReadyToPlay = !GameServer()->m_pController->IsPlayerReadyMode();
	m_RespawnDisabled = GameServer()->m_pController->GetStartRespawnState();
	m_DeadSpecMode = false;
	m_Spawning = false;

	// Optimizations
	m_pGameServer->AddPlayer(m_ClientID);

	// zCatch
	m_CaughtBy = NOT_CAUGHT;
	m_CaughtReason = REASON_NONE;
	m_CaughtPlayers.reserve(MAX_PLAYERS);
	m_NumCaughtPlayersWhoJoined = 0;
	m_NumCaughtPlayersWhoLeft = 0;
	m_NumWillinglyReleasedPlayers = 0;
	m_WantsToJoinSpectators = false;
	UpdateSkinColors();
	ResetStatistics();
	m_DetailedServerMessages = false;
	m_ChatTicks = 0;
	m_PunishmentLevel = NONE;
}

CPlayer::~CPlayer()
{
	// player doesn't exist -> no need to iterate over that player anymore.
	m_pGameServer->RemovePlayer(m_ClientID);
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Tick()
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	Server()->SetClientScore(m_ClientID, m_Score);

	if (GameServer()->m_pController->IsGameRunning())
	{
		// statistics
		if (IsCaught())
		{
			m_TicksCaught++;
		}
		else if(IsNotCaught())
		{
			m_TicksIngame++;
		}

		if (g_Config.m_SvAnticamper > 0)
		{
			Anticamper();
		}
	}
	else if (GameServer()->m_pController->IsGameWarmup())
	{
		m_TicksWarmup++;
	}

	if(m_ChatTicks > 0)
		m_ChatTicks--;

	// do latency stuff
	{
		IServer::CClientInfo Info;
		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(m_pCharacter && !m_pCharacter->IsAlive())
	{
		delete m_pCharacter;
		m_pCharacter = 0;
	}

	if(!GameServer()->m_pController->IsGamePaused())
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpecMode == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick() && !m_DeadSpecMode)
			Respawn();

		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_pSpecFlag)
		{
			if(m_pSpecFlag->GetCarrier())
				m_SpectatorID = m_pSpecFlag->GetCarrier()->GetPlayer()->GetCID();
			else
				m_SpectatorID = -1;
		}

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
				m_ViewPos = m_pCharacter->GetPos();
		}
		else if(m_Spawning && m_RespawnTick <= Server()->Tick())
			TryRespawn();

		if(!m_DeadSpecMode && m_LastActionTick != Server()->Tick())
			++m_InactivityTickCounter;
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
 	}
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i : GameServer()->PlayerIDs())
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators and dead players
	if((m_Team == TEAM_SPECTATORS || m_DeadSpecMode) && m_SpecMode != SPEC_FREEVIEW)
	{
		if(m_pSpecFlag)
			m_ViewPos = m_pSpecFlag->GetPos();
		else if (m_SpectatorID >= 0 && GameServer()->m_apPlayers[m_SpectatorID])
			m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
	}
}

void CPlayer::Snap(int SnappingClient)
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	bool DontHideAdmin = Server()->IsAuthed(m_ClientID) && g_Config.m_SvHideAdmins == 0;
	pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
	if(DontHideAdmin)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;
	if(!GameServer()->m_pController->IsPlayerReadyMode() || m_IsReadyToPlay)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
	if(m_RespawnDisabled && (!GetCharacter() || !GetCharacter()->IsAlive()))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_DEAD;
	if(SnappingClient != -1 && (m_Team == TEAM_SPECTATORS || m_DeadSpecMode) && (SnappingClient == m_SpectatorID))
		if(!Server()->IsAuthed(m_ClientID) || DontHideAdmin)
			pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_WATCHING;


	pPlayerInfo->m_Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	pPlayerInfo->m_Score = m_Score;

	if(m_ClientID == SnappingClient && (m_Team == TEAM_SPECTATORS || m_DeadSpecMode))
	{
		CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpecMode = m_SpecMode;
		pSpectatorInfo->m_SpectatorID = m_SpectatorID;
		if(m_pSpecFlag)
		{
			pSpectatorInfo->m_X = m_pSpecFlag->GetPos().x;
			pSpectatorInfo->m_Y = m_pSpecFlag->GetPos().y;
		}
		else
		{
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_ClientInfo *pClientInfo = static_cast<CNetObj_De_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_CLIENTINFO, m_ClientID, sizeof(CNetObj_De_ClientInfo)));
		if(!pClientInfo)
			return;

		pClientInfo->m_Local = 0;
		pClientInfo->m_Team = m_Team;
		StrToInts(pClientInfo->m_aName, 4, Server()->ClientName(m_ClientID));
		StrToInts(pClientInfo->m_aClan, 3, Server()->ClientClan(m_ClientID));
		pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);

		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			StrToInts(pClientInfo->m_aaSkinPartNames[p], 6, m_TeeInfos.m_aaSkinPartNames[p]);
			pClientInfo->m_aUseCustomColors[p] = m_TeeInfos.m_aUseCustomColors[p];
			pClientInfo->m_aSkinPartColors[p] = m_TeeInfos.m_aSkinPartColors[p];
		}
	}
}

void CPlayer::OnDisconnect()
{
	KillCharacter();

	if(m_Team != TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i : GameServer()->PlayerIDs())
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				if(GameServer()->m_apPlayers[i]->m_DeadSpecMode)
					GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
				else
				{
					GameServer()->m_apPlayers[i]->m_SpecMode = SPEC_FREEVIEW;
					GameServer()->m_apPlayers[i]->m_SpectatorID = -1;
				}
			}
		}
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(GameServer()->m_World.m_Paused)
	{
		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		Respawn();

	if(!m_pCharacter && m_Team == TEAM_SPECTATORS && (NewInput->m_Fire&1))
	{
		if(!m_ActiveSpecSwitch)
		{
			m_ActiveSpecSwitch = true;
			if(m_SpecMode == SPEC_FREEVIEW)
			{
				CCharacter *pChar = (CCharacter *)GameServer()->m_World.ClosestEntity(m_ViewPos, 6.0f*32, CGameWorld::ENTTYPE_CHARACTER, 0);
				CFlag *pFlag = (CFlag *)GameServer()->m_World.ClosestEntity(m_ViewPos, 6.0f*32, CGameWorld::ENTTYPE_FLAG, 0);
				if(pChar || pFlag)
				{
					if(!pChar || (pFlag && pChar && distance(m_ViewPos, pFlag->GetPos()) < distance(m_ViewPos, pChar->GetPos())))
					{
						m_SpecMode = pFlag->GetTeam() == TEAM_RED ? SPEC_FLAGRED : SPEC_FLAGBLUE;
						m_pSpecFlag = pFlag;
						m_SpectatorID = -1;
					}
					else
					{
						m_SpecMode = SPEC_PLAYER;
						m_pSpecFlag = 0;
						m_SpectatorID = pChar->GetPlayer()->GetCID();
					}
				}
			}
			else
			{
				m_SpecMode = SPEC_FREEVIEW;
				m_pSpecFlag = 0;
				m_SpectatorID = -1;
			}
		}
	}
	else if(m_ActiveSpecSwitch)
		m_ActiveSpecSwitch = false;

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
		m_InactivityTickCounter = 0;
	}
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	// Any kills caused by WEAPON_WORLD
	// are not passed to KillCharacter, but directly
	// to Character::Die -> IGameController::OnCharacterDeath(handled in here or in derived class)
	
	// WEAPON_WORLD is not necessary handled in here.

	if (GetNumCurrentlyCaughtPlayers() > 0 && Weapon == WEAPON_SELF)
	{
		// release last caught player on pressing suicide key.
		ReleaseLastCaughtPlayer(REASON_PLAYER_RELEASED, true);
		
		// we don't want to die in this case.
		return;
	}
	
	if(m_pCharacter)
	{	
		m_pCharacter->Die(m_ClientID, Weapon);
		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{	
	if(m_RespawnDisabled && m_Team != TEAM_SPECTATORS)
	{
		// enable spectate mode for dead players
		m_DeadSpecMode = true;
		m_IsReadyToPlay = true;
		m_SpecMode = SPEC_PLAYER;
		UpdateDeadSpecMode();
		return;
	}

	m_DeadSpecMode = false;

	if(m_Team != TEAM_SPECTATORS)
	{
		m_Spawning = true;
	}
		
}

bool CPlayer::SetSpectatorID(int SpecMode, int SpectatorID)
{
	if((SpecMode == m_SpecMode && SpecMode != SPEC_PLAYER) ||
		(m_SpecMode == SPEC_PLAYER && SpecMode == SPEC_PLAYER && (SpectatorID == -1 || m_SpectatorID == SpectatorID || m_ClientID == SpectatorID)))
	{
		return false;
	}

	if(m_Team == TEAM_SPECTATORS)
	{
		// check for freeview or if wanted player is playing
		if(SpecMode != SPEC_PLAYER || (SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[SpectatorID] && GameServer()->m_apPlayers[SpectatorID]->GetTeam() != TEAM_SPECTATORS))
		{
			if(SpecMode == SPEC_FLAGRED || SpecMode == SPEC_FLAGBLUE)
			{
				CFlag *pFlag = (CFlag*)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_FLAG);
				while (pFlag)
				{
					if ((pFlag->GetTeam() == TEAM_RED && SpecMode == SPEC_FLAGRED) || (pFlag->GetTeam() == TEAM_BLUE && SpecMode == SPEC_FLAGBLUE))
					{
						m_pSpecFlag = pFlag;
						if (pFlag->GetCarrier())
							m_SpectatorID = pFlag->GetCarrier()->GetPlayer()->GetCID();
						else
							m_SpectatorID = -1;
						break;
					}
					pFlag = (CFlag*)pFlag->TypeNext();
				}
				if (!m_pSpecFlag)
					return false;
				m_SpecMode = SpecMode;
				return true;
			}
			m_pSpecFlag = 0;
			m_SpecMode = SpecMode;
			m_SpectatorID = SpectatorID;
			return true;
		}
	}
	else if(m_DeadSpecMode)
	{
		// check if wanted player can be followed
		if(SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[SpectatorID] && DeadCanFollow(GameServer()->m_apPlayers[SpectatorID]))
		{
			m_SpecMode = SpecMode;
			m_pSpecFlag = 0;
			m_SpectatorID = SpectatorID;
			return true;
		}
	}

	return false;
}

bool CPlayer::DeadCanFollow(CPlayer *pPlayer) const
{
	// check if wanted player is in the same team and alive
	return (!pPlayer->m_RespawnDisabled || (pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())) && pPlayer->GetTeam() == m_Team;
}

void CPlayer::UpdateDeadSpecMode()
{
	// check if actual spectator id is valid
	if(m_SpectatorID != -1 && GameServer()->m_apPlayers[m_SpectatorID] && DeadCanFollow(GameServer()->m_apPlayers[m_SpectatorID]))
		return;

	// find player to follow
	for(int i : GameServer()->PlayerIDs())
	{
		if(GameServer()->m_apPlayers[i] && DeadCanFollow(GameServer()->m_apPlayers[i]))
		{
			m_SpectatorID = i;
			return;
		}
	}

	// no one available to follow -> turn spectator mode off
	m_DeadSpecMode = false;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{	
	KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpecMode = SPEC_FREEVIEW;
	m_SpectatorID = -1;
	m_pSpecFlag = 0;
	m_DeadSpecMode = false;

	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i : GameServer()->PlayerIDs())
		{
			if(GameServer()->m_apPlayers[i] && GameServer()-> m_apPlayers[i]->m_SpecMode == SPEC_PLAYER && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
			{
				if(GameServer()->m_apPlayers[i]->m_DeadSpecMode)
					GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
				else
				{
					GameServer()->m_apPlayers[i]->m_SpecMode = SPEC_FREEVIEW;
					GameServer()->m_apPlayers[i]->m_SpectatorID = -1;
				}
			}
		}
	}
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(!GameServer()->m_pController->CanSpawn(m_Team, &SpawnPos))
		return;

	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos);
	m_LastRespawnedTick = Server()->Tick();
}

// affects oneself & caught player
bool CPlayer::CatchPlayer(int ID, int reason)
{	
	// player can be caught by me(if not already caught)
	if (GameServer()->m_apPlayers[ID] && GameServer()->m_apPlayers[ID]->BeCaught(m_ClientID, reason))
	{
		// player not cauht by anybody, add him to my caught players
		m_CaughtPlayers.push_back(ID);

		// statistics
		m_Kills++;
        if (reason == REASON_PLAYER_JOINED)
            if (g_Config.m_SvKillIncreasing == 0)
                m_Score += g_Config.m_SvKillScore;
            else
                m_Score += g_Config.m_SvKillScore * GameServer()->m_apPlayers[ID]->GetNumCurrentlyCaughtPlayers();

		if(reason)
		{
			char aBuf[128];
			bool sendReasonMessage = true;
			switch (reason)
			{
			case REASON_PLAYER_CAUGHT:
				// default catch, nothing special here to talk about.
				sendReasonMessage = false;
				break;
			case REASON_PLAYER_WARMUP_CAUGHT:
				sendReasonMessage = false;
				break;
			case REASON_PLAYER_JOINED:
				m_NumCaughtPlayersWhoJoined++;
				str_format(aBuf, sizeof(aBuf), "'%s' was added to your victims(%d left).", Server()->ClientName(ID), GetNumCurrentlyCaughtPlayers());
				break;
			default:
				break;
			}

			if (sendReasonMessage)
				GameServer()->SendServerMessage(GetCID(), aBuf);
		}

		// we caught someone, thus we get a new skin color.
		UpdateSkinColors();
		return true;
	}
	else
	{
		// catching the player failed
		return false;
	}
}

// affects oneself & all caught players
bool CPlayer::BeCaught(int byID, int reason)
{	
	dbg_assert(byID >= 0 && byID < MAX_CLIENTS && GameServer()->m_apPlayers[byID], "Being caught by invalid player ID.");
	if (byID == m_ClientID)
	{
		// falling into death tiles
		// you die, you loose all of your caught players.
		ReleaseAllCaughtPlayers(REASON_PLAYER_FAILED);
		
		// do not be caught
		return false;
	}
	else if(m_CaughtBy == NOT_CAUGHT && GameServer()->m_apPlayers[byID])
	{
		// not caught case: be caught by ID
		// can only be caught if player actually exists

		if(reason)
		{
			char aBuf[128];
			bool sendReasonMessage = true;
			switch (reason)
			{
			case REASON_PLAYER_CAUGHT:
				str_format(aBuf, sizeof(aBuf), "You will be released when '%s' dies.", Server()->ClientName(byID));
				break;
			case REASON_PLAYER_JOINED:
				str_format(aBuf, sizeof(aBuf), "You were added to \"%s\"'s victims. You will be released once \"%s\" dies.", Server()->ClientName(byID), Server()->ClientName(byID));
				break;
			case REASON_PLAYER_WARMUP_CAUGHT:
				sendReasonMessage = false;
			default:
				break;
			}
			if(sendReasonMessage)
				GameServer()->SendServerMessage(GetCID(), aBuf);
		}

		m_CaughtBy = byID;
		m_CaughtReason = reason;
		m_SpectatorID = m_CaughtBy;
		m_RespawnDisabled = true;

		// respawn at least 3 seconds after being caught.
		m_RespawnTick = Server()->Tick() + (Server()->TickSpeed() * 3);

		ReleaseAllCaughtPlayers(REASON_PLAYER_DIED);	
		
		

		// statistics
		if(reason != REASON_PLAYER_JOINED) {
            m_Deaths++;
            m_Score -= g_Config.m_SvDeathScore;
        }

		return true;
	}
	else	
	{
		// already caught by someone else
		// or player is not ingame anymore, that should catch me
		return false;
	}
}

// only affects oneself
bool CPlayer::BeReleased(int reason)
{
	// BeReleased() acts as a actual reset function, that allows players
	// to join the game, independent of whether they were caught before or not.
	// If a player joins the game and nobody has anybody caught, the player is set
	// into a state of being released.
	// the other case is that a player is reset into the released state, when rejoining the game
	// after being in the spectators team

	// can be released if caught
	// or if not caught and either joined the server
	// or rejoined the game after being in spec
	if (m_CaughtBy >= 0 || 
		(m_CaughtBy == NOT_CAUGHT && 
			(
				reason == REASON_PLAYER_JOINED || 
				reason == REASON_PLAYER_JOINED_GAME_AGAIN
			)
		)
	)
	{
		// caught -> can be released -> is released
		if (reason)
		{
			char aBuf[128];
			// first message to the released player
			bool sendServerMessage = true;
			switch(reason)
			{
			case REASON_PLAYER_DIED:
				if(m_DetailedServerMessages)
				{
					// this happens too often, as that it should be displayed on every death.
					str_format(aBuf, sizeof(aBuf), "You were released because '%s' died.", Server()->ClientName(m_CaughtBy));
				}
				else
				{
					sendServerMessage = false;	
				}
				break;
			case REASON_PLAYER_FAILED:
				str_format(aBuf, sizeof(aBuf), "You were released because '%s' failed miserably.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_RELEASED:
				str_format(aBuf, sizeof(aBuf), "You were released because '%s' is a generous player.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_WARMUP_RELEASED:
				sendServerMessage = false;
				break;
			case REASON_PLAYER_JOINED:
				str_format(aBuf, sizeof(aBuf), "You were released because nobody has caught any players yet.");
				break;
			case REASON_PLAYER_JOINED_SPEC:
				str_format(aBuf, sizeof(aBuf), "You were released because '%s' joined the spectators.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_JOINED_GAME_AGAIN:
				sendServerMessage = false;
				// nothing to say here, because the released player triggered the "release" himself/herself
				// actually a reset to the released state only 
				// the player was previously willingly explicitly stectating.
				break;
			case REASON_PLAYER_LEFT:
				str_format(aBuf, sizeof(aBuf), "You were released because '%s' has left the game.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_EVERYONE_RELEASED:
				str_format(aBuf, sizeof(aBuf), "Everyone was released!");
				break;
			default:
				break;
			}
			if(sendServerMessage)
				GameServer()->SendServerMessage(GetCID(), aBuf);
		}

		m_CaughtBy = NOT_CAUGHT;
		m_CaughtReason = REASON_NONE;
		m_SpectatorID = -1;
		m_RespawnDisabled = false;

		// as we ought to wait at least 3 seconds between kill & respawn, we check
		// whether the respawntick is already set to a higher value.
		if(m_RespawnTick < Server()->Tick() + (Server()->TickSpeed()))
			m_RespawnTick = Server()->Tick() + (Server()->TickSpeed());

		// this respawn trigger is needed, otherwise 
		// the player will respawn only when the fire key is
		// pressed, causing the player to be stuck in the "twilight zone"
		Respawn();
		return true;
	}
	else
	{
		// not caught -> cannot be released
		return false;
	}	
}

// affects oneself & caught players
int CPlayer::ReleaseLastCaughtPlayer(int reason, bool updateSkinColors)
{
	if (m_CaughtPlayers.size() > 0)
	{
		int playerToReleaseID = m_CaughtPlayers.back();
		

		// look at last still existing player.

		if(GameServer()->m_apPlayers[playerToReleaseID] && GameServer()->m_apPlayers[playerToReleaseID]->BeReleased(reason))
		{
			// player can be released
			m_CaughtPlayers.pop_back();

			/*
				Inform the releasing player about the player he/she willingly
				released. Any mass release is not handled in here, as it would flood
				the dying/failing/ etc. player with messages.
			 */
			if (reason == REASON_PLAYER_RELEASED)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "You released '%s'(%d left)",
						   Server()->ClientName(playerToReleaseID),
						   GetNumCurrentlyCaughtPlayers());
				GameServer()->SendServerMessage(m_ClientID, aBuf);
				m_NumWillinglyReleasedPlayers++;
			}
			else if (reason == REASON_PLAYER_WARMUP_RELEASED)
			{
				m_NumWillinglyReleasedPlayers++;
			}

			if(updateSkinColors)
			{
				UpdateSkinColors();

				// as this specific case cannot be handled
				// in the zcatch mod, we need to force the sending of
				// our updated skin color in here.
				if (reason == REASON_PLAYER_RELEASED || reason == REASON_PLAYER_WARMUP_RELEASED)
				{
					for (int toID : GameServer()->PlayerIDs())
					{
						if (GameServer()->m_apPlayers[toID])
						{
							// send skin update message of id to everyone
							GameServer()->SendSkinChange(m_ClientID, toID);
						}
					}
				}
			
			}
			return playerToReleaseID;
		}
		else
		{
			// player that needs to be released does not exist anymore
			// but is still popped from the vector
			m_CaughtPlayers.pop_back();

			// player cannot be released
			return NOT_CAUGHT;
		}	
	}
	else
	{
		// nobody caught, thus nobody to release
		return NOT_CAUGHT;
	}
}

// remove given id from my caught players
bool CPlayer::RemoveFromCaughtPlayers(int ID, int reason)
{
	// removes elements, but leaves garbage at the end of the vector
	auto it = std::remove_if(m_CaughtPlayers.begin(), m_CaughtPlayers.end(), [this, ID, reason](int lookAtID) {
		if (lookAtID == ID)
		{
			// remove from my caught players & release at the same time.
			if(GameServer()->m_apPlayers[lookAtID])
			{
				// if player is still online, release him/her.
				GameServer()->m_apPlayers[lookAtID]->BeReleased(reason);
			}
			return true;
		}
		else
		{
			return false;
		}
	});

	if(it != m_CaughtPlayers.end())
	{
		// erase garbage elements from end of vector
		m_CaughtPlayers.erase(it, m_CaughtPlayers.end());
		switch(reason)
		{
			case REASON_PLAYER_LEFT:
				// player was removed from my caught players, 
				// because he/she left the game
				m_NumCaughtPlayersWhoLeft++;
				break;
		}

		// only update colors if actually someone was released
		UpdateSkinColors();
	}

	return it != m_CaughtPlayers.end();
}

bool CPlayer::BeSetFree(int reason)
{
	if (m_CaughtBy != NOT_CAUGHT)
	{
		if(GameServer()->m_apPlayers[m_CaughtBy])
		{
			// if my killer still exists, remove from his caught players
			return GameServer()->m_apPlayers[m_CaughtBy]->RemoveFromCaughtPlayers(m_ClientID, reason);
		}
		else
		{
			// my killer doesn't exist, just release me.
			return BeReleased(reason);
		}
		
	}
	return false;
}

int CPlayer::ReleaseAllCaughtPlayers(int reason)
{	
	// usually we release everyone, when we die, so
	// reset these stats, when we die
	m_NumCaughtPlayersWhoJoined = 0;
	m_NumCaughtPlayersWhoLeft = 0;
	m_NumWillinglyReleasedPlayers = 0;

	int releasedPlayers = m_CaughtPlayers.size();

	// don't send messages, when nobody was released.
	if(releasedPlayers == 0)
	{
		// just update skin
		// when in warmup, your color changes, even tho you have nobody caught.
		UpdateSkinColors();
		return 0;
	}


	// message to the releasing player
	bool hasReasonMessage = true;
	bool isStillIngame = true;
	char aBuf[256];

	if (m_DetailedServerMessages)
	{
		switch(reason)
		{
		case REASON_PLAYER_DIED:
			str_format(aBuf, sizeof(aBuf), "Your death caused %d player%s to be set free!", GetNumCurrentlyCaughtPlayers(), GetNumCurrentlyCaughtPlayers() != 1 ? "s" : "");
			break;
		case REASON_PLAYER_FAILED:
			str_format(aBuf, sizeof(aBuf), "Your failure caused %d player%s to be set free!", GetNumCurrentlyCaughtPlayers(), GetNumCurrentlyCaughtPlayers() != 1 ? "s" : "");
			m_Fails++;
			break;
		case REASON_PLAYER_LEFT:
			// no message, because the player leaves.
			hasReasonMessage = false;
			isStillIngame = false;
			break;
		case REASON_PLAYER_WARMUP_CAUGHT:
			hasReasonMessage = false;
			break;
		case REASON_PLAYER_JOINED_SPEC:
			str_format(aBuf, sizeof(aBuf), "Your cowardly escape caused %d player%s to be set free!", GetNumCurrentlyCaughtPlayers(), GetNumCurrentlyCaughtPlayers() != 1 ? "s" : "");
			break;
		default:
			hasReasonMessage = false;
			break;
		}

		if (hasReasonMessage)
		{
			GameServer()->SendServerMessage(m_ClientID, aBuf);
		}
	}

	// somebody to release
	// while returned id is a valid 0 <= ID <= MAC_CLIENTS
	while(ReleaseLastCaughtPlayer(reason) >= 0);
	
	// if player did not leave the game
	if(isStillIngame)
	{
		UpdateSkinColors();
	}

	return releasedPlayers;
}

void CPlayer::SetPlayersLeftToCatch(int leftToCatch)
{
	if(leftToCatch < 0)
	{
		m_PlayersLeftToCatch = 0; // case when nobody is ingame oher than you
		dbg_msg("DEBUG", "player %d has %d players left to catch.", m_ClientID, leftToCatch);
		dbg_msg("DEBUG", "PLAYER: %s", str().c_str());
		dbg_msg("DEBUG", "INGAME PLAYERS: %lu", GameServer()->PlayerIDs().size());
		for(const int ID : GameServer()->PlayerIDs())
		{
			if(GameServer()->m_apPlayers[ID])
				dbg_msg("DEBUG", "%s", GameServer()->m_apPlayers[ID]->str().c_str());
			else{
				dbg_msg("DEBUG", "INVALID ID IN playerIDs(): %d", ID);
			}
		}

		dbg_msg("DEBUG", "CAUGHT PLAYERs: %d", GetNumCurrentlyCaughtPlayers());

		for(const int ID : m_CaughtPlayers)
		{
			if(GameServer()->m_apPlayers[ID])
				dbg_msg("DEBUG", "%s", GameServer()->m_apPlayers[ID]->str().c_str());
			else{
				dbg_msg("DEBUG", "INVALID ID IN m_CaughtPlayers: %d", ID);
			}
		}

	}
	else 
	{
		m_PlayersLeftToCatch = leftToCatch;
	}
}

int CPlayer::GetPlayersLeftToCatch()
{
	return m_PlayersLeftToCatch;
}

int CPlayer::GetNumCurrentlyCaughtPlayers()
{
	return m_CaughtPlayers.size();
}

int CPlayer::GetNumTotalCaughtPlayers()
{
	return m_CaughtPlayers.size() + m_NumCaughtPlayersWhoLeft - m_NumCaughtPlayersWhoJoined;
}

int CPlayer::GetNumCaughtPlayersWhoLeft()
{
	return m_NumCaughtPlayersWhoLeft;
}

int CPlayer::GetNumCaughtPlayersWhoJoined()
{
	return m_NumCaughtPlayersWhoJoined;
}

int CPlayer::GetNumReleasedPlayers()
{
	return m_NumWillinglyReleasedPlayers;
}

int CPlayer::GetNumCaughtPlayersInARow()
{
	return m_CaughtPlayers.size() - m_NumCaughtPlayersWhoJoined + m_NumCaughtPlayersWhoLeft + m_NumWillinglyReleasedPlayers;
}

int CPlayer::GetIDCaughtBy()
{
	return m_CaughtBy;
}

int CPlayer::GetCaughtReason()
{
	return m_CaughtReason;
}

bool CPlayer::IsCaught()
{
	return m_CaughtBy >= 0 
		&& m_CaughtBy < MAX_CLIENTS  // caught
		&& m_Team != TEAM_SPECTATORS  // not in spec
		&& (m_RespawnDisabled // cannot spawn
		|| (m_pCharacter && !m_pCharacter->IsAlive())); // 
}

bool CPlayer::IsNotCaught()
{
	return m_CaughtBy == NOT_CAUGHT // not caught
		&& m_Team != TEAM_SPECTATORS // not in spec
		&& (!m_RespawnDisabled  // can spawn
		|| (m_pCharacter && m_pCharacter->IsAlive())); // or is alive
}

bool CPlayer::GetWantsToJoinSpectators()
{
	return m_WantsToJoinSpectators;
}

void CPlayer::ResetWantsToJoinSpectators()
{
	m_WantsToJoinSpectators = false;
}

void CPlayer::SetWantsToJoinSpectators()
{
	m_WantsToJoinSpectators = true;
}

void CPlayer::ResetStatistics()
{
	m_IsRankFetched = false;
	m_Wins = 0;
	m_Score = 0;
	m_Kills = 0;
	m_Deaths = 0;
	m_TicksCaught = 0;
	m_TicksIngame = 0;
	m_Shots = 0;
	m_Fails = 0;
}

unsigned int CPlayer::GetColor()
{
	int color;
	if(GameServer()->m_pController->IsGameWarmup())
	{
		// coloration during warmup
		color = max(0, 160 - GetNumCaughtPlayersInARow() * 10) * 0x010000 + 0xff00;
	}
	else
	{
		// coloration when zCatch is running
		color = max(0, 160 - GetNumTotalCaughtPlayers() * 10) * 0x010000 + 0xff00;
	}
	return color;
}

void CPlayer::UpdateSkinColors()
{
	unsigned int color = GetColor();

	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		m_TeeInfos.m_aUseCustomColors[p] = 1;
		m_TeeInfos.m_aSkinPartColors[p] = color;
	}
}

int CPlayer::Anticamper()
{
	if(GameServer()->m_World.m_Paused || !m_pCharacter || m_Team == TEAM_SPECTATORS || m_pCharacter->IsFrozen())
	{
		m_CampTick = -1;
		m_SentCampMsg = false;
		return 0;
	}

	int AnticamperTime = g_Config.m_SvAnticamperTime;
	int AnticamperRange = g_Config.m_SvAnticamperRange;

	if(m_CampTick == -1)
	{
		m_CampPos = m_pCharacter->GetPos();
		m_CampTick = Server()->Tick() + Server()->TickSpeed()*AnticamperTime;
	}

	// Check if the player is moving
	if((abs(m_CampPos.x - m_pCharacter->GetPos().x) >= (float)AnticamperRange)|| 
		(abs(m_CampPos.y - m_pCharacter->GetPos().y) >= (float)AnticamperRange))
		{
			m_CampTick = -1;
		}

	// Send warning to the player
	if(m_CampTick <= Server()->Tick() + Server()->TickSpeed() * AnticamperTime/2 && m_CampTick != -1 && !m_SentCampMsg)
	{
		GameServer()->SendServerMessage(m_ClientID, "ANTICAMPER: Move or die");
		m_SentCampMsg = true;
	}

	// Kill him
	if((m_CampTick <= Server()->Tick()) && (m_CampTick > 0))
	{
		if(g_Config.m_SvAnticamperFreeze)
		{
			m_pCharacter->Freeze(Server()->TickSpeed()*g_Config.m_SvAnticamperFreeze);
			GameServer()->SendServerMessage(m_ClientID, "You have been frozen due to camping");
		}
		else
			m_pCharacter->Die(m_ClientID, WEAPON_GAME);
		m_CampTick = -1;
		m_SentCampMsg = false;
		return 1;
	}
	return 0;
}

std::string CPlayer::str()
{
	std::stringstream ss;
	ss << "\n" << Server()->ClientName(m_ClientID) << "\n{\n";
	ss << "Caught by: " << m_CaughtBy << "\n";
	ss << "IsCaught: " << IsCaught() << "\n";
	ss << "IsNotCaught: " << IsNotCaught() << "\n";
	ss << "CaughtReason: " << GetCaughtReason() << "\n";
	ss << "NumCurrentlyCaughtPlayers: " << GetNumCurrentlyCaughtPlayers() << "\n";
	ss << "NumTotalCaughtPlayers: " << GetNumTotalCaughtPlayers() << "\n";
	ss << "NumCaughtPlayersWhoLeft: " << GetNumCaughtPlayersWhoLeft() << "\n";
	ss << "NumCaughtPlayersWhoJoined" << GetNumCaughtPlayersWhoJoined() << "\n";
	ss << "NumReleasedPlayers: " << GetNumReleasedPlayers() << "\n}\n";
	return ss.str();
}

