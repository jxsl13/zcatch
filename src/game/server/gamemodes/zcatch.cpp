/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <generated/protocol.h>

#include "zcatch.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>


CGameControllerZCATCH::CGameControllerZCATCH(CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "zCatch/dev";
	m_GameFlags = GAMEFLAG_SURVIVAL;

	m_PreviousIngamePlayerCount = 0;
	m_IngamePlayerCount = 0;
	
}

void CGameControllerZCATCH::OnChatMessage(int ofID, int Mode, int toID, const char *pText)
{
	// message doesn't start with /, then it's no command message
	if(pText && pText[0] && pText[0] != '/')
	{
		dbg_msg("DEBUG", "Message doesn't contain any leading command character.");
		IGameController::OnChatMessage(ofID, Mode, toID, pText);
		return;
	}

	// parse words as tokens
	std::stringstream commandLine(pText + 1);
	std::string tmpString;
	std::vector<std::string> tokens;

	while(std::getline(commandLine, tmpString, ' ')) 
    { 
        tokens.push_back(tmpString); 
    } 
	int size = tokens.size();


	if (size > 0)
	{
		try
		{
			if(tokens.at(0) == "welcome")
			{
				GameServer()->SendServerMessage(ofID, "Welcome to zCatch, where you kill all of your enemies to win a round. Write '/help' for more information.");
			}
			else if (tokens.at(0) == "help")
			{
				if (size == 1)
				{
					GameServer()->SendServerMessage(ofID, "/welcome - The message you get, when you join the server.");
					GameServer()->SendServerMessage(ofID, "/rules - If you want to know about zCatch's ruleset.");
					GameServer()->SendServerMessage(ofID, "/info - If to know about this mod's creators.");
					GameServer()->SendServerMessage(ofID, "/help list - To see a list of all the help screens.");

				}
				else if (size == 2)
				{
					if(tokens.at(1) == "list")
					{
						GameServer()->SendServerMessage(ofID, "/help release - Learn how to release players.");
						GameServer()->SendServerMessage(ofID, "/help warmup - Learn more about how the warmup works.");
						GameServer()->SendServerMessage(ofID, "/help anticamper - Learn more about how the anti camper works.");
					}
					else if(tokens.at(1) == "release")
					{
						GameServer()->SendServerMessage(ofID, "There are two ways to release a player, that you have caught:");
						GameServer()->SendServerMessage(ofID, "The first one is to create a suicide bind like this 'bind k kill' using the F1 console. The second way is to write");
						GameServer()->SendServerMessage(ofID, "'/release' in the chat. The difference between both methods is that if you use your kill bind, you will kill yourself if");
						GameServer()->SendServerMessage(ofID, "nobody is left to be released. In contrast, the second method will not do anything if there is nobody left to be released.");
					}
					else if(tokens.at(1) == "warmup")
					{
						char aBuf[128];
						str_format(aBuf, sizeof(aBuf), "As long as there are less than %d players ingame, warmup is enabled.", g_Config.m_SvPlayersToStartRound);
						GameServer()->SendServerMessage(ofID, aBuf);
						GameServer()->SendServerMessage(ofID, "While in warmup, any player caught will respawn immediatly.");
						GameServer()->SendServerMessage(ofID, "If there are enough players for a round of zCatch, the warmup ends and players are caught normally.");
					}
					else if(tokens.at(1) == "anticamper")
					{
						char aBuf[128];
						if(g_Config.m_SvAnticamperFreeze > 0)
						{
							str_format(aBuf, sizeof(aBuf), "If you don't move for %d seconds out of the range of %d units, you will be freezed for %d seconds.", g_Config.m_SvAnticamperTime, g_Config.m_SvAnticamperRange, g_Config.m_SvAnticamperFreeze);
						}
						else
						{
							str_format(aBuf, sizeof(aBuf), "If you don't move for %d seconds out of the range of %d units, you will be killed.", g_Config.m_SvAnticamperTime, g_Config.m_SvAnticamperRange);
						}
						GameServer()->SendServerMessage(ofID, aBuf);
						str_format(aBuf, sizeof(aBuf), "Anticamper is currently %s.", g_Config.m_SvAnticamper > 0 ? "enabled" : "disabled");	
						GameServer()->SendServerMessage(ofID, aBuf);
					}
					else
					{
						throw std::invalid_argument("");
					}
				}
				else
				{
					throw std::invalid_argument("");
				}
				
			}
			else if (tokens.at(0) == "info" && size == 1)
			{
				GameServer()->SendServerMessage(ofID, "Welcome to zCatch, a completely newly created version for Teeworlds 0.7. The ground work was done erdbaer & Teetime");
				GameServer()->SendServerMessage(ofID, "and is used as reference. Teelevision did a great job maintaining the mod after the Instagib Laser era. Also a thank ");
				GameServer()->SendServerMessage(ofID, "you to TeeSlayer, who ported a basic version of zCatch to Teeworlds 0.7, that has also been used as reference.");
			}
			else if(tokens.at(0) == "rules")
			{  
				GameServer()->SendServerMessage(ofID, "zCatch is a Last Man Standing game mode. The last player to be alive will win the round. Each player killed by you is");
				GameServer()->SendServerMessage(ofID, "considered as caught. If you die, all of you caught players are released.");
				GameServer()->SendServerMessage(ofID, "If you catch all of them, you win the round.");
				GameServer()->SendServerMessage(ofID, "As a measure of fair play, you are able to release your caught players manually in reverse order.");
				GameServer()->SendServerMessage(ofID, "Releasing players is optional in zCatch. Type '/help release' for more information.");
			}
			else if(tokens.at(0) == "release" && size == 1)
			{
				class CPlayer *pPlayer = GameServer()->m_apPlayers[ofID];
				if(pPlayer)
				{
					pPlayer->ReleaseLastCaughtPlayer(CPlayer::REASON_PLAYER_RELEASED, true);
				}
			}
			else
			{
				return;
			}
		}
		catch (std::invalid_argument &e)
		{
			GameServer()->SendServerMessage(ofID, "No such command, please try /cmdlist or /help");
		}
	}
	else
	{
		return;
	}
}

 void CGameControllerZCATCH::EndRound()
 {
	 // don't do anything if we just switched from
	 // warmup to zCatch or from zCatch to warmup
	if(m_PreviousIngamePlayerCount != m_IngamePlayerCount)
	{
		return;
	}

	CPlayer *pPlayer = nullptr;

	for (int ID = 0; ID < MAX_CLIENTS; ID++)
	{
		pPlayer = GameServer()->m_apPlayers[ID];
		if(pPlayer)
		{
			
			ShowPlayerStatistics(pPlayer);

			// do cleanup
			pPlayer->ReleaseAllCaughtPlayers(CPlayer::REASON_EVERYONE_RELEASED);
			pPlayer->ResetStatistics();
			pPlayer = nullptr;
		}
	}

	// Change game state
	IGameController::EndRound();

	// reset skin colors
	UpdateSkinsOfEverybody();
 }

// game
void CGameControllerZCATCH::DoWincheckRound()
{
	if(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60)
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
			
			// Inform everyone about the winner.
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "'%s' won the round!", Server()->ClientName(pAlivePlayer->GetCID()));
			GameServer()->SendServerMessage(-1, aBuf);
			EndRound();
		}
		else if(m_PreviousIngamePlayerCount >= g_Config.m_SvPlayersToStartRound && m_IngamePlayerCount < g_Config.m_SvPlayersToStartRound)
		{
			// Switching back to warmup!
			dbg_msg("DEBUG", "Switching back to warm up mode.");
			// TODO: this needs to be reviewed
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
		return;
	}

	// If the player spawns, his and all of the other player's 
	// broadcast needs to be updated, because that player could have been 
	// in spec previously
	UpdateBroadcastOfEverybody();
	UpdateSkinsOf({player.GetCID()});
}

void CGameControllerZCATCH::OnPlayerConnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);

	// greeting
	OnChatMessage(0, 0, player.GetCID(), "/welcome");
	
	// warmup
	if (IsGameWarmup())
	{	
		if (g_Config.m_Debug)
		{
			dbg_msg("DEBUG", "Player %d joined the game.", player.GetCID());
		}
		
		UpdateSkinsOf({player.GetCID()});
		UpdateBroadcastOfEverybody();

		// simply allow that player to join.
		IGameController::OnPlayerConnect(pPlayer);
		return;
	}

	class CPlayer *pDominatingPlayer = ChooseDominatingPlayer(player.GetCID());

	
	if (pDominatingPlayer)
	{
		pDominatingPlayer->CatchPlayer(player.GetCID(), CPlayer::REASON_PLAYER_JOINED);
	}
	else 
	{
		if (m_IngamePlayerCount > g_Config.m_SvPlayersToStartRound 
			&& m_IngamePlayerCount == m_PreviousIngamePlayerCount)
		{
			// if the player joins & nobody has caught anybody 
			// at that exact moment, when a round is running
			// the player directly joins the game
			
			// we don't want this message to be displayed in any other case
			player.BeReleased(CPlayer::REASON_PLAYER_JOINED);
		}
		else
		{
			// no chat announcements
			// when switching game modes/states
			// or other stuff.
			player.BeReleased(); // silent join
		}
	}
	
	UpdateBroadcastOfEverybody();
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
			// release caught players
			player.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_LEFT);

		}
		else if (player.IsCaught())
		{
			// remove leaving player from caught list 
			player.BeSetFree(CPlayer::REASON_PLAYER_LEFT);
		}
	}
	else
	{
		// player was in spectator mode, nothing to do.
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
	if(victim.GetCID() != killer.GetCID() && Weapon >= 0)
	{
		// someone killed me with a real weapon
		killer.CatchPlayer(victim.GetCID());
	}
	else if(Weapon < 0 && victim.GetCID() == killer.GetCID())
	{
		switch (Weapon)
		{
		case WEAPON_WORLD: // death tiles etc.
			victim.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_FAILED);
			break;
		case WEAPON_SELF: // suicide
			// here we catch literally the suicides, not the releases
			GameServer()->SendServerMessage(victim.GetCID(), "Was it really necessary to kill yourself?");
			break;
		case WEAPON_GAME: // team change, etc.
			if (g_Config.m_Debug)
			{
				dbg_msg("DEBUG", "ID: %d was killed by the game.", victim.GetCID());
			}
			break;
		default:
			break;
		}
	}


	// send broadcast update to victim and killer
	UpdateBroadcastOf({victim.GetCID(), killer.GetCID()});

	// update colors of both players
	UpdateSkinsOf({victim.GetCID(), killer.GetCID()});

	// vanilla handling
	// do scoreing
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;
	if(pKiller == pVictim->GetPlayer())
	{
		// suicide or falling out of the map
		if (Weapon == WEAPON_WORLD || (Weapon == WEAPON_SELF && victim.GetNumCaughtPlayers() == 0))
		{
			pVictim->GetPlayer()->m_Deaths += g_Config.m_SvSuicidePenalty;
		}		
	}
		
	if(Weapon == WEAPON_SELF)
	{
		// respawn in 3 seconds
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*3.0f;
	}

	// update spectator modes for dead players in survival
	if(m_GameFlags&GAMEFLAG_SURVIVAL)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_DeadSpecMode)
				GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
	}
	
	return 0;
}


void CGameControllerZCATCH::Tick()
{
	// Broadcast Refresh is only needed for solely 
	// keeping the broadcast visible, but not to push
	// actual updates.
	int nineSeconds = Server()->TickSpeed() * 9;
	if (Server()->Tick() % nineSeconds == 0)
	{
		// The broadcast is according to heinrich5991 kept shown
		// by the client for 10 seconds.
		RefreshBroadcast();
	}
	
	IGameController::Tick();
}


void CGameControllerZCATCH::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	CPlayer& player = (*pPlayer);

	// toggle state, whether player wants or doesn't want to joint spec.
	if (player.IsCaught() && Team == TEAM_SPECTATORS)
	{
		// player wants to join spec, but is caught.

		char aBuf[128];
		if (player.GetWantsToJoinSpectators())
		{	
			str_format(aBuf, sizeof(aBuf), "You will join the game once '%s' dies.", Server()->ClientName(player.GetIDCaughtBy()));
			player.ResetWantsToJoinSpectators();
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "You will join the spectators once '%s' dies.", Server()->ClientName(player.GetIDCaughtBy()));
			player.SetWantsToJoinSpectators();
		}

		GameServer()->SendServerMessage(player.GetCID(), aBuf);
		// do not change team, while you are caught.
		return;
	}
	else if(player.IsNotCaught() && player.GetNumCaughtPlayers() > 0 && Team == TEAM_SPECTATORS)
	{
		// player is not caught and wants to join the spectators.
		player.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_JOINED_SPEC);
	}
	else if(player.GetTeam() == TEAM_SPECTATORS && Team != TEAM_SPECTATORS)
	{
		// players joins the game after being in spec

		// do vanilla respawn stuff
		IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);

		// force player to spawn
		player.BeReleased(CPlayer::REASON_PLAYER_JOINED_GAME_AGAIN);
		// TODO: check if this here needs either skinupdates or brodcast updates
		return;
	}
	
	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
	// TODO: check if this here needs either skinupdates or brodcast updates
}


void CGameControllerZCATCH::OnPlayerInfoChange(class CPlayer *pPlayer)
{
	// Player changes skin etc -> force zcatch colors
	if (pPlayer)
	{
		pPlayer->UpdateSkinColors();
		UpdateSkinsOf({pPlayer->GetCID()});
	}
}

void CGameControllerZCATCH::UpdateBroadcastOf(std::initializer_list<int> IDs)
{
	CPlayer *pTmpPlayer = nullptr;
	char aBuf[32];
	int enemiesLeft = 0;

	for (int ID : IDs)
	{
		pTmpPlayer = GameServer()->m_apPlayers[ID];
		if (pTmpPlayer)
		{
			if (pTmpPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				// this function is rather intense, so we want it to be called as 
				// few times as possible
				enemiesLeft = pTmpPlayer->UpdatePlayersLeftToCatch();

				if (enemiesLeft > 0)
				{
					str_format(aBuf, sizeof(aBuf), "%d enem%s left", enemiesLeft, enemiesLeft == 1 ? "y" : "ies");
					GameServer()->SendBroadcast(aBuf, ID);
				}
				else
				{
					GameServer()->SendBroadcast("", ID);
				}
			}
			else
			{
				// Spectating players should not receive any visible broadcasts
				// this is basically updated, when somone joins the spectators
				// in order to hide the enemies left counter
				GameServer()->SendBroadcast("", ID);
			}
		}		
		pTmpPlayer = nullptr;
	}	
}

void CGameControllerZCATCH::UpdateBroadcastOfEverybody()
{
	CPlayer *pTmpPlayer = nullptr;
	char aBuf[32];
	int enemiesLeft = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		pTmpPlayer = GameServer()->m_apPlayers[i];
		if (pTmpPlayer)
		{
			if (pTmpPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				// this function is rather intense, so we want it to be called as 
				// few times as possible
				enemiesLeft = pTmpPlayer->UpdatePlayersLeftToCatch();

				if (enemiesLeft > 0)
				{
					str_format(aBuf, sizeof(aBuf), "%d enem%s left", enemiesLeft, enemiesLeft == 1 ? "y" : "ies");
					GameServer()->SendBroadcast(aBuf, i);
				}
				else
				{
					GameServer()->SendBroadcast("", i);
				}
			}
			else
			{
				// Spectating players should not receive any visible broadcasts
				// this is basically updated, when somone joins the spectators
				// in order to hide the enemies left counter
				GameServer()->SendBroadcast("", i);
			}
		}		
		pTmpPlayer = nullptr;
	}	
}


void CGameControllerZCATCH::RefreshBroadcast()
{
	// basically only print the stored values of each player 
	// and keeps the broadcast alive no updates are handled
	// in here. Those should be handled at their occurrence place.
	CPlayer *pTmpPlayer = nullptr;
	char aBuf[32];
	int enemiesLeft = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		pTmpPlayer = GameServer()->m_apPlayers[i];

		// spectating players should not receive any broadcasts.
		if (pTmpPlayer && pTmpPlayer->GetTeam() != TEAM_SPECTATORS)
		{
			if (pTmpPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				enemiesLeft = pTmpPlayer->GetPlayersLeftToCatch();

				if (enemiesLeft > 0)
				{
					str_format(aBuf, sizeof(aBuf), "%d enem%s left", enemiesLeft, enemiesLeft == 1 ? "y" : "ies");
					GameServer()->SendBroadcast(aBuf, i);
				}
				else
				{
					GameServer()->SendBroadcast("", i);
				}
			}
			else
			{
				// spectators don't get any broadcast refreshes
			}
		}

		pTmpPlayer = nullptr;
	}
}


void CGameControllerZCATCH::UpdateSkinsOf(std::initializer_list<int> IDs)
{
	for (int toID = 0; toID < MAX_CLIENTS; toID++)
	{
		if (GameServer()->m_apPlayers[toID])
		{
			// send skin update message of id to everyone
			for (int ofID : IDs)
			{
				GameServer()->SendSkinChange(ofID, toID);
			}
		}
	}
}

void CGameControllerZCATCH::UpdateSkinsOfEverybody()
{
	for (int toID = 0; toID < MAX_CLIENTS; toID++)
	{
		if (GameServer()->m_apPlayers[toID])
		{
			// send skin update message of id to everyone
			for (int ofID = 0; ofID < MAX_CLIENTS; ofID++)
			{
				if (GameServer()->m_apPlayers[ofID])
				{
					GameServer()->SendSkinChange(ofID, toID);
				}	
			}
		}
	}
}

void CGameControllerZCATCH::ShowPlayerStatistics(class CPlayer *pOfPlayer)
{
	// release all players at the end of the round.
	float alivePercentage = 0.0f, caughtPercentage = 0.0f;
	float totalTicksPlayed = 0.0f;
	char aBuf[64];

	// calculate some small player statistics
	totalTicksPlayed = static_cast<float>(pOfPlayer->m_TicksAlive + pOfPlayer->m_TicksCaught);
	if (totalTicksPlayed > 0.0)
	{
		alivePercentage = (pOfPlayer->m_TicksAlive / totalTicksPlayed) * 100.0f;
		caughtPercentage = (pOfPlayer->m_TicksCaught / totalTicksPlayed) * 100.0f;

		// send them to the player
		str_format(aBuf, sizeof(aBuf), "Ingame: %.2f%% Spectating: %.2f%% ", alivePercentage, caughtPercentage);
		GameServer()->SendServerMessage(pOfPlayer->GetCID(), aBuf);
	}
}

class CPlayer* CGameControllerZCATCH::ChooseDominatingPlayer(int excludeID)
{

	std::vector<class CPlayer*> livingPlayers;

	class CPlayer *pTmpPlayer = nullptr;

	// fill vector with living players
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// if we join, we cannot be the dominating player
		// skip ourself.
		if (excludeID == i)
		{
			continue;
		}

		pTmpPlayer = GameServer()->m_apPlayers[i];
		if (pTmpPlayer && pTmpPlayer->IsNotCaught())
		{
			livingPlayers.push_back(pTmpPlayer);
		}
	}

	if (livingPlayers.size() == 0)
		return nullptr;
	

	// custom comparison structure
	struct {
        bool operator()(class CPlayer* a, CPlayer* b) const{
			return a->GetNumCaughtPlayers() + a->GetNumCaughtPlayersWhoLeft() > b->GetNumCaughtPlayers() + b->GetNumCaughtPlayersWhoLeft();
			}   
    } caughtMore;

	// sort by caught players
	std::sort(livingPlayers.begin(), livingPlayers.end(), caughtMore);	


	// super weird error might occur
	int mostCaughtPlayers = livingPlayers.at(0)->GetNumCaughtPlayers() + livingPlayers.at(0)->GetNumCaughtPlayersWhoLeft();
	
	// super weird stuff - erase invalid players?
	// in some weird case access to a living player causes a segfault.
	// TODO: wtf?
	auto it = livingPlayers.begin();
	for (; it != livingPlayers.end(); it++)
	{
		if (!(*it))
		{
			// erase invalid pointer
			livingPlayers.erase(it, it+1);
		}
		else if((*it)->GetNumCaughtPlayers() + (*it)->GetNumCaughtPlayersWhoLeft() != mostCaughtPlayers)
		{
			break;
		}
		
		
	}
	dbg_msg("DEBUG", "Living players count: %lu", livingPlayers.size());


	// remove player, that do not have the 
	// same amount of caught players as the dominating player.
	livingPlayers.erase(it, livingPlayers.end());

	if(livingPlayers.size() == 0)
		return nullptr;
	else if(static_cast<int>(livingPlayers.size()) == m_IngamePlayerCount)
		return nullptr;
	
	// choose random player
	class CPlayer* pChosenDominatingPlayer = livingPlayers.at(Server()->Tick() % livingPlayers.size());

	if(pChosenDominatingPlayer->GetNumCaughtPlayers() + pChosenDominatingPlayer->GetNumCaughtPlayersWhoLeft() > 0)
	{
		dbg_msg("DEBUG", "Chosen player to be added as victim of is: ID %d of %lu dominatig players.", pChosenDominatingPlayer->GetCID(), livingPlayers.size());
		return pChosenDominatingPlayer;
	}
	else
	{
		return nullptr;
	}
}





