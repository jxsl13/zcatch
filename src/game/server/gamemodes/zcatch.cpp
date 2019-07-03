/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "zcatch.h"

CGameControllerZCATCH::CGameControllerZCATCH(CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "zCatch/dev";
	m_GameFlags = GAMEFLAG_SURVIVAL;

	m_PreviousIngamePlayerCount = 0;
	m_IngamePlayerCount = 0;

	m_ForcedEndRound = false;
}

 void CGameControllerZCATCH::EndRound()
 {
	// release all players at the end of the round.
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->ReleaseAllCaughtPlayers();
		}
	}

	// Change game state
	IGameController::EndRound();
 }

// game
void CGameControllerZCATCH::DoWincheckRound()
{
	// forced end round to fallback to warmup
	if (m_ForcedEndRound)
	{
		m_ForcedEndRound = false;
		EndRound();
	}
	else if(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60)
	{
		// check for time based win
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->IsNotCaught())
				GameServer()->m_apPlayers[i]->m_Score++;
		}

		EndRound();
	}
	else
	{
		// check for survival win
		CPlayer *pAlivePlayer = 0;
		
		int alivePlayerCount = 0;
		m_PreviousIngamePlayerCount = m_IngamePlayerCount;
		m_IngamePlayerCount = 0;

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i])
			{
				if (GameServer()->m_apPlayers[i]->IsNotCaught())
				{
					++alivePlayerCount;
					pAlivePlayer = GameServer()->m_apPlayers[i];
				}
				
				if (GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					m_IngamePlayerCount++;
				}	
			}
		}

		if(alivePlayerCount == 0)
		{
			// no winner
			EndRound();
		}	
		else if(alivePlayerCount == 1)	// 1 winner
		{
			pAlivePlayer->m_Score++;
			EndRound();
		}
		else if(m_PreviousIngamePlayerCount >= g_Config.m_SvPlayersToStartRound && m_IngamePlayerCount < g_Config.m_SvPlayersToStartRound)
		{
			EndRound();
		}
	}
}

void CGameControllerZCATCH::OnCharacterSpawn(class CCharacter *pChr)
{	
	CCharacter& character = (*pChr);
	CPlayer& player = (*pChr->GetPlayer());

	// Gets weapons in Character::Spawn

	// gets health here.
	character.IncreaseHealth(10);

	// if we spawn, we should not have anyone caught from previous rounds.
	// or weird post mortem kills.
	player.ReleaseAllCaughtPlayers();

	// spawns -> joins spec
	if (player.GetWantsToJoinSpectators())
	{
		DoTeamChange(pChr->GetPlayer(), TEAM_SPECTATORS);
		player.ResetWantsToJoinSpectators();
	}
}

void CGameControllerZCATCH::OnPlayerConnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);
	int gamestate = GetGameState();
	
	// warmup
	if (gamestate == IGS_WARMUP_GAME || gamestate == IGS_WARMUP_USER)
	{
		dbg_msg("DEBUG", "Player %d joined the game.", player.GetCID());
		IGameController::OnPlayerConnect(pPlayer);
		return;
	}

	// actual game running
	CPlayer *pDominatingPlayer = nullptr;

	// find player with most kills
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// if we join, we cannot be the dominating player
		// skip ourself.
		if (player.GetCID() == i)
		{
			continue;
		}

		if (GameServer()->m_apPlayers[i])
		{
			if (!pDominatingPlayer)
			{
				pDominatingPlayer = GameServer()->m_apPlayers[i];
			}
			else if (GameServer()->m_apPlayers[i]->GetNumCaughtPlayers() > pDominatingPlayer->GetNumCaughtPlayers())
			{
				pDominatingPlayer = GameServer()->m_apPlayers[i];
			}
		}
	}

	// if the dominating player has nobody caught, we don't want them to
	if (pDominatingPlayer && pDominatingPlayer->GetNumCaughtPlayers() > 0)
	{
		dbg_msg("DEBUG", "Player %d was added as victim of player %d.", player.GetCID(), pDominatingPlayer->GetCID());
		pDominatingPlayer->CatchPlayer(player.GetCID());
	}

	// needed to do the spawning stuff.
	IGameController::OnPlayerConnect(pPlayer);
}

void CGameControllerZCATCH::OnPlayerDisconnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);
	
	// if player was ingame and not in spec when leaving
	if (player.GetTeam() != TEAM_SPECTATORS)
	{
		if (player.IsNotCaught())
		{
			// player being released 
			player.ReleaseAllCaughtPlayers();

		}
		else if (player.IsCaught())
		{
			// remove player from caught list
			GameServer()->m_apPlayers[player.GetCaughtByID()]->RemoveFromCaughtPlayers(player.GetCID());
		}
		
		// I leave, decrease number of ingame players
		m_IngamePlayerCount--;
		

		// not enough players to play a round
		if (m_IngamePlayerCount < g_Config.m_SvPlayersToStartRound)
		{
			// end round and do warmup again.
			m_ForcedEndRound = true;
		}
	}

	// needed to do the disconnect handling.
	IGameController::OnPlayerDisconnect(pPlayer);
}

int CGameControllerZCATCH::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	CPlayer& victim = (*pVictim->GetPlayer());
	CPlayer& killer = (*pKiller);
	int gamestate = GetGameState();

	// warmup
	if (gamestate == IGS_WARMUP_GAME || gamestate == IGS_WARMUP_USER)
	{
		// any kind of warmup
		dbg_msg("DEBUG", "Warmup, player %d killed by %d.", victim.GetCID(), killer.GetCID());
		// simply die & respawn
		return IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
	}
	
	// actual game
	/**
	 * CPlayer::KillCharacter -> CCharacter::Die -> IGameController::OnCharacterDeath
	 * the cases, where victim.GetNumCaughtPlayers() > 0 are handled in 
	 * CPlayer::KillCharacter, prior to these cases, where it is possible to prevent 
	 * the character from actually being killed, when releasing a player, 
	 * that was killed by mistake.
	 */
	if(victim.GetNumCaughtPlayers() == 0 && (Weapon == WEAPON_SELF || Weapon == WEAPON_GAME))
	{
		dbg_msg("DEBUG", "Player %d died a poor man's death.", victim.GetCID());
	}
	else if(victim.GetCID() != killer.GetCID() && Weapon != WEAPON_SELF && Weapon != WEAPON_GAME)
	{
		dbg_msg("DEBUG", "Player %d was caught by %d.", victim.GetCID(), killer.GetCID());
		// someone killed me with a weapon
		killer.CatchPlayer(victim.GetCID());
	}
	else if(victim.GetCID() == killer.GetCID())
	{
		// weird stuff happening
		dbg_msg("DEBUG", "I killed myself with: %d, what is going on?!", Weapon);
	}
	
	return IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
}

void CGameControllerZCATCH::Tick()
{
	IGameController::Tick();
}

void CGameControllerZCATCH::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	CPlayer& player = (*pPlayer);

	// toggle state, whether player wants or doesn't want to joint spec.
	if (player.IsCaught() && Team == TEAM_SPECTATORS)
	{
		if (player.GetWantsToJoinSpectators())
		{
			player.ResetWantsToJoinSpectators();
		}
		else
		{
			player.SetWantsToJoinSpectators();
		}
		return;
	}
	
	
	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);

}
