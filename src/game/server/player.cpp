/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

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

}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;
}

void CPlayer::Tick()
{
	if(!IsDummy() && !Server()->ClientIngame(m_ClientID))
		return;

	Server()->SetClientScore(m_ClientID, m_Score);

	// statistics
	if (IsCaught())
	{
		m_TicksCaught++;
	}
	else if(IsNotCaught())
	{
		m_TicksAlive++;
	}
	

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
		for(int i = 0; i < MAX_CLIENTS; ++i)
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
		else if (GameServer()->m_apPlayers[m_SpectatorID])
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

	pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
	if(Server()->IsAuthed(m_ClientID))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;
	if(!GameServer()->m_pController->IsPlayerReadyMode() || m_IsReadyToPlay)
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
	if(m_RespawnDisabled && (!GetCharacter() || !GetCharacter()->IsAlive()))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_DEAD;
	if(SnappingClient != -1 && (m_Team == TEAM_SPECTATORS || m_DeadSpecMode) && (SnappingClient == m_SpectatorID))
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
		for(int i = 0; i < MAX_CLIENTS; ++i)
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

	if (GetNumCaughtPlayers() > 0 && Weapon == WEAPON_SELF)
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
		m_Spawning = true;
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
	for(int i = 0; i < MAX_CLIENTS; ++i)
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
		for(int i = 0; i < MAX_CLIENTS; ++i)
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
}

// affects oneself & caught player
bool CPlayer::CatchPlayer(int ID, int reason)
{	
	// player can be caught by me(if not already caught)
	if (GameServer()->m_apPlayers[ID] && GameServer()->m_apPlayers[ID]->BeCaught(GetCID(), reason))
	{
		// player not cauht by anybody, add him to my caught players
		m_CaughtPlayers.push_back(ID);

		// statistics
		m_Kills++;

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
			case REASON_PLAYER_JOINED:
				m_NumCaughtPlayersWhoJoined++;
				str_format(aBuf, sizeof(aBuf), "'%s' was added to your victims(%d left).", Server()->ClientName(ID), GetNumCaughtPlayers());
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

{	if (byID == GetCID())
	{
		// falling into death tiles
		// you die, you loose all of your caught players.
		ReleaseAllCaughtPlayers(REASON_PLAYER_FAILED);
		
		// do not be caught
		return false;
	}

	if (m_CaughtBy >= 0)	
	{
		// already caught by someone else
		return false;
	}
	else if(GameServer()->m_apPlayers[byID])
	{
		// not caught case: be caught by ID
		// can only be caught if player actually exists

		if(reason)
		{
			char aBuf[128];
			switch (reason)
			{
			case REASON_PLAYER_CAUGHT:
				str_format(aBuf, sizeof(aBuf), "You will be released, when '%s' dies.", Server()->ClientName(byID));
				break;
			case REASON_PLAYER_JOINED:
				str_format(aBuf, sizeof(aBuf), "You were added to \"%s's\" victims. You will be released, once \"%s\" dies.", Server()->ClientName(byID), Server()->ClientName(byID));
				break;
			default:
				break;
			}
			GameServer()->SendServerMessage(GetCID(), aBuf);
		}

		m_CaughtBy = byID;
		m_CaughtReason = reason;
		m_SpectatorID = m_CaughtBy;
		m_NumCaughtPlayersWhoJoined = 0;
		m_NumCaughtPlayersWhoLeft = 0;
		m_NumWillinglyReleasedPlayers = 0;
		m_RespawnDisabled = true;
		ReleaseAllCaughtPlayers(REASON_PLAYER_DIED);

		// statistics
		if(reason != REASON_PLAYER_JOINED)
			m_Deaths++;

		return true;
	}
	return false;
}

// only affects oneself
bool CPlayer::BeReleased(int reason)
{
	// can be released if caught
	// or if not caught and either joined the server
	// or rejoined the game after being in spec
	if (m_CaughtBy >= 0 || 
		(m_CaughtBy == NOT_CAUGHT && 
			(reason == REASON_PLAYER_JOINED || reason == REASON_PLAYER_JOINED_GAME_AGAIN)
		)
	)
	{
		// caught -> can be released -> is released
		if (reason)
		{
			char aBuf[128];
			// first message to the released player
			bool sendServerMessage = true;
			switch (reason)
			{
			case REASON_PLAYER_DIED:
				str_format(aBuf, sizeof(aBuf), "You were released, because '%s' died.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_FAILED:
				str_format(aBuf, sizeof(aBuf), "You were released, because '%s' failed miserably.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_RELEASED:
				str_format(aBuf, sizeof(aBuf), "You were released, because '%s' is a generous player.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_JOINED:
				str_format(aBuf, sizeof(aBuf), "You were released, because nobody has caught any players yet.");
				break;
			case REASON_PLAYER_JOINED_SPEC:
				str_format(aBuf, sizeof(aBuf), "You were released, because '%s' joined the spectators.", Server()->ClientName(m_CaughtBy));
				break;
			case REASON_PLAYER_JOINED_GAME_AGAIN:
				sendServerMessage = false;
				// nothing to say here, because the released player triggered the "release" himself/herself
				// actually a reset to the released state only 
				// the player was previously willingly explicitly stectating.
				break;
			case REASON_PLAYER_LEFT:
				str_format(aBuf, sizeof(aBuf), "You were released, because '%s' is leaving the game.", Server()->ClientName(m_CaughtBy));
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
		// respawn half a second after being released
		m_RespawnTick = Server()->Tick() + (Server()->TickSpeed()/2);
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
		
		// remove all caught player ids of players that do not 
		// exist anymore.
		while (!GameServer()->m_apPlayers[playerToReleaseID])
		{
			// release non existing player
			m_CaughtPlayers.pop_back();
			if(m_CaughtPlayers.size() == 0)
			{
				return NOT_CAUGHT;
			}
			// check next player
			playerToReleaseID = m_CaughtPlayers.back();
		}
		// break out of loop, if player exists


		// look at last still existing player.
		if(GameServer()->m_apPlayers[playerToReleaseID]->BeReleased(reason))
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
						   GetNumCaughtPlayers());
				GameServer()->SendServerMessage(GetCID(), aBuf);

				m_NumWillinglyReleasedPlayers++;
			}

			// re-calculate enemies left to catch
			UpdatePlayersLeftToCatch();

			if(updateSkinColors)
			{
				UpdateSkinColors();

				// as this specific case cannot be handled
				// in the zcatch mod, we need to force the sending of
				// our updated skin color in here.
				if (reason == REASON_PLAYER_RELEASED)
				{
					for (int toID = 0; toID < MAX_CLIENTS; toID++)
					{
						if (GameServer()->m_apPlayers[toID])
						{
							// send skin update message of id to everyone
							GameServer()->SendSkinChange(GetCID(), toID);
						}
					}
				}
			
			}
			return playerToReleaseID;
		}
		else
		{
			// player cannot be released
			return NOT_CAUGHT;
		}	
	}
	else
	{
		return NOT_CAUGHT;
	}
}

// remove given id from my caught players
bool CPlayer::RemoveFromCaughtPlayers(int ID, int reason)
{
	// success is true, if the player was actually in our caught players.
	bool success = false;
	// move to the end of vector
	auto it = std::remove_if(m_CaughtPlayers.begin(), m_CaughtPlayers.end(), [&](int lookAtID) {
		if (lookAtID == ID)
		{
			// remove from my caught players & release at the same time.
			if(GameServer()->m_apPlayers[lookAtID])
			{
				// if player is still online, rlease him.
				GameServer()->m_apPlayers[lookAtID]->BeReleased(reason);
			}

			success = true;
			return true;
		}
		else
		{
			return false;
		}
	});

	if(success)
	{
		// erase elements from end of vector
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

	return success;
}

bool CPlayer::BeSetFree(int reason)
{
	if (m_CaughtBy != NOT_CAUGHT)
	{
		if(GameServer()->m_apPlayers[m_CaughtBy])
		{
			// if my killer still exists, remove from his caught players
			return GameServer()->m_apPlayers[m_CaughtBy]->RemoveFromCaughtPlayers(GetCID(), reason);
		}
		else
		{
			// my killer doesn't exist, just release me.
			BeReleased(reason);
			return true;
		}
		
	}
	return false;
}

int CPlayer::ReleaseAllCaughtPlayers(int reason)
{	
	int releasedPlayers = m_CaughtPlayers.size();
	// nobody to release
	if (releasedPlayers == 0)
	{
		return releasedPlayers;
	}

	// message to the releasing player
	bool hasReasonMessage = true;
	bool isStillIngame = true;
	char aBuf[256];

	switch (reason)
	{
	case REASON_PLAYER_DIED:
		str_format(aBuf, sizeof(aBuf), "Your death caused %d player%s to be set free!", GetNumCaughtPlayers(), GetNumCaughtPlayers() > 1 ? "s" : "");
		break;
	case REASON_PLAYER_FAILED:
		str_format(aBuf, sizeof(aBuf), "Your failure caused %d player%s to be set free!", GetNumCaughtPlayers(), GetNumCaughtPlayers() > 1 ? "s" : "");
		break;
	case REASON_PLAYER_LEFT:
		// no message, because the player leaves.
		hasReasonMessage = false;
		isStillIngame = false;
		break;
	case REASON_PLAYER_JOINED_SPEC:
		str_format(aBuf, sizeof(aBuf), "Your cowardly escape caused %d player%s to be set free!", GetNumCaughtPlayers(), GetNumCaughtPlayers() > 1 ? "s" : "");
		break;
	default:
		hasReasonMessage = false;
		break;
	}

	if (hasReasonMessage)
	{
		GameServer()->SendServerMessage(GetCID(), aBuf);
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

int CPlayer::UpdatePlayersLeftToCatch()
{
	// as this updating operation is rather heavy to lift, we want updates only to happen, 
	// when someone is killed and not all the time
	m_PlayersLeftToCatch = 0;


	// if you are spectating, you have nobody left to catch
	if(GetTeam() == TEAM_SPECTATORS)
		return 0;

	int myID = GetCID();

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// player not me and player does actually exist
		if(i != myID && GameServer()->m_apPlayers[i])
		{
			// player not spectating and not caught by me
			if(GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && 
				GameServer()->m_apPlayers[i]->GetIDCaughtBy() != myID)
			{
				m_PlayersLeftToCatch++;
			}
		}
	}
	return 	m_PlayersLeftToCatch;
}

int CPlayer::GetPlayersLeftToCatch()
{
	return m_PlayersLeftToCatch;
}

int CPlayer::GetNumCaughtPlayers()
{
	return m_CaughtPlayers.size();
}

int CPlayer::GetNumCaughtPlayersWhoLeft()
{
	return m_NumCaughtPlayersWhoLeft;
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
	bool isCaught = m_CaughtBy >= 0 && m_CaughtBy < MAX_CLIENTS;
	bool notInSpec = m_Team != TEAM_SPECTATORS;
	bool cannotSpawn = m_RespawnDisabled;
	bool characterNotAlive = m_pCharacter && !m_pCharacter->IsAlive();
	return isCaught && notInSpec && (cannotSpawn || characterNotAlive);
}

bool CPlayer::IsNotCaught()
{
	bool isNotCaught = m_CaughtBy == NOT_CAUGHT;
	bool notInSpec = m_Team != TEAM_SPECTATORS;
	bool canSpawn = !m_RespawnDisabled;
	bool characterAlive = m_pCharacter && m_pCharacter->IsAlive();
	return isNotCaught && notInSpec && (canSpawn || characterAlive);
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
	m_Kills = 0;
	m_Deaths = 0;
	m_TicksCaught = 0;
	m_TicksAlive = 0;
}

unsigned int CPlayer::GetColor()
{
	// TODO: check if calculation with caught players + players who were caught, but left
	// is actually a good idea.
	int color = max(0, 160 - GetNumCaughtPlayers() * 10) * 0x010000 + 0xff00;
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
