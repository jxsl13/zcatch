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
	m_PreviousAlivePlayerCount = 0;
	m_AlivePlayerCount = 0;
	m_ForcedEndRound = false;
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
	

	// check for time based win
	if(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60)
	{
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
		m_PreviousAlivePlayerCount = m_AlivePlayerCount;
		m_AlivePlayerCount = 0;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->IsNotCaught())
			{
				++m_AlivePlayerCount;
				pAlivePlayer = GameServer()->m_apPlayers[i];
			}
		}

		if(m_AlivePlayerCount == 0)		// no winner
			EndRound();
		else if(m_AlivePlayerCount == 1)	// 1 winner
		{
			pAlivePlayer->m_Score++;
			EndRound();
		}
	}
}

void CGameControllerZCATCH::OnCharacterSpawn(class CCharacter *pChr)
{	
	CCharacter& character = (*pChr);
	CPlayer& player = (*pChr->GetPlayer());

	character.IncreaseHealth(10);

	switch (g_Config.m_SvWeaponMode)
	{
	case WEAPON_HAMMER:
		character.GiveWeapon(WEAPON_HAMMER, -1);
		break;
	case WEAPON_GUN:
		character.GiveWeapon(WEAPON_GUN, 10);
		break;
	case WEAPON_SHOTGUN:
		character.GiveWeapon(WEAPON_SHOTGUN, 10);
		break;
	case WEAPON_GRENADE:
		character.GiveWeapon(WEAPON_GRENADE, 10);
		break;
	case WEAPON_LASER:
		character.GiveWeapon(WEAPON_LASER, 10);
		break;
	case WEAPON_NINJA:
		character.GiveNinja();
		break;
	default:
		character.GiveWeapon(WEAPON_HAMMER, -1);
		character.GiveWeapon(WEAPON_GUN, -1);
		character.GiveWeapon(WEAPON_SHOTGUN, -1);
		character.GiveWeapon(WEAPON_GRENADE, -1);
		character.GiveWeapon(WEAPON_LASER, -1);
		break;
	}

	// if we spawn, we should not have anyone caught from previous rounds.
	// or weird post mortem kills.
	player.ReleaseAllCaughtPlayers();
}

void CGameControllerZCATCH::OnPlayerConnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);
	int gamestate = GetGameState();
	
	// warmup
	if (gamestate == IGS_WARMUP_GAME || gamestate == IGS_WARMUP_USER)
	{
		// any kind of warmup
		dbg_msg("DEBUG", "Player %d joined the game.", player.GetCID());
		IGameController::OnPlayerConnect(pPlayer);
	}

	CPlayer *pDominatingPlayer = nullptr;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
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
	if (pDominatingPlayer)
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
	if (player.IsNotCaught())
	{
		// save last number of caught players
		m_PreviousAlivePlayerCount = m_AlivePlayerCount;

		// released players become alive
		int releasedPlayers = player.GetNumCaughtPlayers();
		player.ReleaseAllCaughtPlayers();
		m_AlivePlayerCount += releasedPlayers;

		// leaving player decreases m_AlivePlayerCount
		m_AlivePlayerCount--;

		// not enough players to play a round
		if (m_AlivePlayerCount < g_Config.m_SvPlayersToStartRound)
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
	 * CPlayer::KillCharacter, priod to these cases, where it is possible to prevent 
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
	if (m_PreviousAlivePlayerCount >= g_Config.m_SvPlayersToStartRound &&  m_AlivePlayerCount < g_Config.m_SvPlayersToStartRound)
	{
		// Fallback to Warmup!
		m_ForcedEndRound = true;
	}
	
	IGameController::Tick();
}
