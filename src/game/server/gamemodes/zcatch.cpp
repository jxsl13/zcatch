/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <generated/protocol.h>
#include "zcatch.h"
#include <game/server/gamemodes/zcatch/playerstats.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>


CGameControllerZCATCH::CGameControllerZCATCH(CGameContext *pGameServer) : IGameController(pGameServer), m_VoteOptionServer(pGameServer)
{
	m_pGameType = "zCatch";
	m_WeaponMode = g_Config.m_SvWeaponMode;
	m_SkillLevel = g_Config.m_SvSkillLevel;
	m_GameFlags = GAMEFLAG_SURVIVAL;

	m_PreviousIngamePlayerCount = 0;
	m_IngamePlayerCount = 0;
	m_PreviousAlivePlayersCount = 0;
	m_AlivePlayersCount = 0;

	// refresh broadcast every 9 seconds.
	m_BroadcastRefreshTime = Server()->TickSpeed() * 9;

	m_pRankingServer = nullptr;

	InitRankingServer();

	InitExtendedVoteOptionServer();
}

void CGameControllerZCATCH::InitRankingServer()
{
	if(m_pRankingServer)
	{
		delete m_pRankingServer;
		m_pRankingServer = nullptr;
	}

	std::string DatabaseType = {g_Config.m_SvDatabaseType};


	if(DatabaseType == "redis")
	{
		m_pRankingServer = new CRedisRankingServer{g_Config.m_SvDatabaseHost, static_cast<size_t>(g_Config.m_SvDatabasePort)};
	}
	else if (DatabaseType == "sqlite" || DatabaseType == "sqlite3")
	{
		m_pRankingServer = new CSQLiteRankingServer{g_Config.m_SvSQLiteFilename, {GetDatabasePrefix()}};
	}
	
}

void CGameControllerZCATCH::InitExtendedVoteOptionServer()
{
	auto none = [](int ofID, std::string& Description, std::string& Command, std::string& Reason) -> bool {return false;};
	m_VoteOptionServer.AddVoteOptionHandler("========== Personal Options ==========", "", none);

	m_VoteOptionServer.AddVoteOptionHandler(
		"Enable all server messages. Currently disabled.", 
		"/allmessages",
		[this](int ofID, std::string& Description, std::string& Command, std::string& Reason) -> bool
		{
			if(Description == "Enable all server messages. Currently disabled.")
			{
				Description = "Disable all server messages. Currently enabled.";
			}
			else
			{
				Description = "Enable all server messages. Currently disabled.";
			}

			// do the same as the chat command
			OnChatMessage(ofID, 0, ofID, Command.c_str());
			return true;
		}
	);

	if (m_pRankingServer)
	{
		m_VoteOptionServer.AddVoteOptionHandler(
			"Show my statistics.",
			"/rank",
			[this](int ofID, std::string &Description, std::string &Command, std::string &Reason) -> bool {
				// do the same as the chat command
				OnChatMessage(ofID, 0, ofID, Command.c_str());
				return false;
			});

		m_VoteOptionServer.AddVoteOptionHandler(
			"Show top 5 players.",
			"/top",
			[this](int ofID, std::string &Description, std::string &Command, std::string &Reason) -> bool {
				// do the same as the chat command
				OnChatMessage(ofID, 0, ofID, Command.c_str());
				return false;
			});

					/*"Showing statistics of " + ofNickname,
					"  Rank:   " + (stats["Score"] > 0 ? std::to_string(stats.GetRank()) : "Not ranked, yet."),
					"  Score:  " + std::to_string(stats["Score"]),
					"  Wins:   " + std::to_string(stats["Wins"]),
					"  Kills:  " + std::to_string(stats["Kills"]),
					"  Deaths: " + std::to_string(stats["Deaths"]),
					"  Ingame: " + ssIngameTime.str(), 
					"  Caught: " + ssCaughtTime.str(), 
					"  Warmup: " + ssWarmupTime.str(), 
					"  Fails:  " + std::to_string(stats["Fails"]),
					"  Shots:  " + std::to_string(stats["Shots"]),*/

		// commands starting with # will be executed, when the player join the game.
		m_VoteOptionServer.AddVoteOptionHandler("========== Statistics ==========", "", none);
		m_VoteOptionServer.AddVoteOptionHandler(
			"Showing statistics of " ,
			"#",
			[this](int ofID, std::string& Description, std::string& Command, std::string& Reason) -> bool {
				// do the same as the chat command
				std::string Nickname(Server()->ClientName(ofID));
				Description = "Showing statistics of " + Nickname;
				return true;
			});
		
		m_VoteOptionServer.AddVoteOptionHandler(
			"  Rank:   ",
			"#",
			[this](int ofID, std::string& Description, std::string& Command, std::string& Reason) -> bool {
				// do the same as the chat command
				std::string Nickname(Server()->ClientName(ofID));

				m_pRankingServer->GetRanking(Nickname, [&Description, this](CPlayerStats& stats){
					Description = "  Rank:   " + (stats["Score"] > 0 ? std::to_string(stats.GetRank()) : std::string("Not ranked, yet."));
					dbg_msg("DEBUG", "GOT RANK: %s", Description.c_str());
				}, GetDatabasePrefix());
				return true;
			});

		m_VoteOptionServer.AddVoteOptionHandler(
			"  Score:  ",
			"#",
			[this](int ofID, std::string& Description, std::string& Command, std::string& Reason) -> bool {
				// do the same as the chat command
				std::string Nickname(Server()->ClientName(ofID));

				m_pRankingServer->GetRanking(Nickname, [&Description, this](CPlayerStats& stats){
					dbg_msg("DEBUG", "GOT SCORE: %d", stats["Score"]);
					Description = "  Score:  " + std::to_string(stats["Score"]);
				}, GetDatabasePrefix());
				return true;
			});
	}
}

CGameControllerZCATCH::~CGameControllerZCATCH()
{
	delete m_pRankingServer;
}

void CGameControllerZCATCH::OnReset()
{
	for(int i : GameServer()->PlayerIDs())
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
			GameServer()->m_apPlayers[i]->Respawn();
			GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
			if(m_RoundCount == 0)
			{
				GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
			}
			GameServer()->m_apPlayers[i]->m_IsReadyToPlay = true;
		}
	}
}

void CGameControllerZCATCH::OnChatMessage(int ofID, int Mode, int toID, const char *pText)
{
	// message doesn't start with /, then it's no command message
	if(pText && pText[0] && pText[0] != '/')
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[ofID];

		// check if player is allowed to chat handle auto mute.
		// IsAllowedToChat also informs the player about him being muted.
		if(pPlayer && GameServer()->IsAllowedToChat(ofID))
		{
			IGameController::OnChatMessage(ofID, Mode, toID, pText);
		}
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
			if(tokens[0] == "welcome")
			{
				GameServer()->SendServerMessageText(ofID, "Welcome to zCatch, where you kill all of your enemies to win a round. Write '/help' for more information.");
			}
			else if (tokens[0] == "help")
			{
				if (size == 1)
				{
					GameServer()->SendServerMessage(ofID, "========== Help ==========");
					GameServer()->SendServerMessage(ofID, "/welcome - The message you get, when you join the server.");
					GameServer()->SendServerMessage(ofID, "/rules - If you want to know about zCatch's ruleset.");
					GameServer()->SendServerMessage(ofID, "/info - If to know about this mod's creators.");
					GameServer()->SendServerMessage(ofID, "/help list - To see a list of all the help screens.");

				}
				else if (size >= 2)
				{
					if(tokens[1] == "list")
					{
						GameServer()->SendServerMessage(ofID, "========== Help - List ==========");
						GameServer()->SendServerMessageText(ofID, "/help release - Learn how to release players.");
						GameServer()->SendServerMessageText(ofID, "/help warmup - Learn more about how the warmup works.");
						GameServer()->SendServerMessageText(ofID, "/help anticamper - More about how the anti camper works.");
						GameServer()->SendServerMessageText(ofID, "/help servermessages - How to enable more detailed information.");
						GameServer()->SendServerMessageText(ofID, "/help ranking - Some information about the ranking.");
						GameServer()->SendServerMessageText(ofID, "/help definitions [general|grenade|laser] - A list of zCatch terminology.");
					}
					else if(tokens[1] == "release")
					{
						GameServer()->SendServerMessage(ofID, "========== Release ==========");
						GameServer()->SendServerMessageText(ofID, "First off, releasing other players is optional and is a way of improving fair play.");
						GameServer()->SendServerMessage(ofID, "There are two ways to release a player, that you have caught:");
						GameServer()->SendServerMessageText(ofID, "The first one is to create a suicide bind like this 'bind k kill' using the F1 console. The second way is to write '/release' in the chat. The difference between both methods is that if you use your kill bind, you will kill yourself if nobody is left to be released. In contrast, the second method will not do anything if there is nobody left to be released.");
					}
					else if(tokens[1] == "warmup")
					{
						GameServer()->SendServerMessage(ofID, "========== Warmup ==========");
						char aBuf[128];
						str_format(aBuf, sizeof(aBuf), "As long as there are less than %d players ingame, warmup is enabled.", g_Config.m_SvPlayersToStartRound);
						GameServer()->SendServerMessage(ofID, aBuf);

						GameServer()->SendServerMessageText(ofID, "While in warmup, any player caught will respawn immediatly. If there are enough players for a round of zCatch, the warmup ends and players are caught normally.");
					}
					else if(tokens[1] == "anticamper")
					{
						GameServer()->SendServerMessage(ofID, "========== Anti-Camper ==========");
						char aBuf[128];
						if(g_Config.m_SvAnticamperFreeze > 0)
						{
							str_format(aBuf, sizeof(aBuf), "If you don't move for %d seconds out of the range of %d units, you will be freezed for %d seconds.", g_Config.m_SvAnticamperTime, g_Config.m_SvAnticamperRange, g_Config.m_SvAnticamperFreeze);
						}
						else
						{
							str_format(aBuf, sizeof(aBuf), "If you don't move for %d seconds out of the range of %d units, you will be killed.", g_Config.m_SvAnticamperTime, g_Config.m_SvAnticamperRange);
						}
						GameServer()->SendServerMessageText(ofID, aBuf);
						str_format(aBuf, sizeof(aBuf), "Anticamper is currently %s.", g_Config.m_SvAnticamper > 0 ? "enabled" : "disabled");	
						GameServer()->SendServerMessage(ofID, aBuf);
					}
					else if(tokens[1] == "servermessages")
					{
						GameServer()->SendServerMessage(ofID, "========== All Messages ==========");
						GameServer()->SendServerMessageText(ofID, "Type /allmessages to get all of the information about your and other player's deaths.");
						GameServer()->SendServerMessageText(ofID, "Type /allmessages again, in order to disable the extra information again.");
					}
					else if (tokens[1] == "ranking")
					{
						GameServer()->SendServerMessage(ofID, "========== Ranking ==========");
						bool rankingEnabled = m_pRankingServer != nullptr;
						if(rankingEnabled)
							GameServer()->SendServerMessage(ofID, "Ranking is currently enabled on this server.");
						else
							GameServer()->SendServerMessage(ofID, "There is currently no player ranking enabled.");

						GameServer()->SendServerMessageText(ofID, "The player ranking saves some of your playing statistics in a database. You can see your own statistics by typing the command /rank in the chat. If you want to see somone else's statistics, write /rank <nickname> instead. In order to see the top players on the server, use the /top command.");
					}
					else if(tokens[1] == "definitions" && size == 2)
					{
						GameServer()->SendServerMessage(ofID, "========== Definitions ==========");
						GameServer()->SendServerMessageText(ofID, "/help definitions general - A list of general terminology for all zCatch modes.");
						GameServer()->SendServerMessageText(ofID, "/help definitions grenade - A list of zCatch Grenade only terminology.");
						GameServer()->SendServerMessageText(ofID, "/help definitions laser - A list of zCatch Laser only terminology.");
					}
					else if(tokens[1] == "definitions" && size >= 3)
					{
						if (tokens[2] == "general")
						{
							GameServer()->SendServerMessage(ofID, "========== Definitions - General ==========");
							GameServer()->SendServerMessageText(ofID, "Camping : Waiting for an extended period in the same location(not necessary position) in order to easily catch an enemy.");
							GameServer()->SendServerMessageText(ofID, "Flooding : Sending a excessive amount of, often similar, chat messages(also called chat spam).");
							GameServer()->SendServerMessageText(ofID, "AFK : Being [A]way [F]rom the [K]eyboard for an extended period of time. It is appropriate to move these players to the spectator's team via vote.");
						}
						else if(tokens[2] == "grenade")
						{
							GameServer()->SendServerMessage(ofID, "========== Definitions - Grenade ==========");
							GameServer()->SendServerMessageText(ofID, "Spamming : Shooting an excessive amount of grenades, often without properly aiming.");
							GameServer()->SendServerMessageText(ofID, "Spraying : Shooting one or more grenades without seeing a player's position or predicting his position, often random shooting in order to hit someone that might luckily pass by.(also called random nade). It is appropriate to release the randomly caught player. Forward facing boost fades are considered as spraying, if they hit a target without hitting the speeding player at the same time. Backwards facing boost nades are not considered as spraying.");
							GameServer()->SendServerMessageText(ofID, "Lucky Shot: Hitting a player, while aiming and shooting at another player is not considered as spraying, but it is appropriate to release the caught player.");
						}
						else if(tokens[2] == "laser")
						{
							GameServer()->SendServerMessage(ofID, "========== Definitions - Laser ==========");
							GameServer()->SendServerMessageText(ofID, "zCatch Laser has no extraordinary terminology, other than the gerneral one.");
						}
						else 
						{
							// unknown second token
							throw std::invalid_argument("");
						}
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
			else if (tokens[0] == "info" && size == 1)
			{
				GameServer()->SendServerMessage(ofID, "========== Info  ==========");
				GameServer()->SendServerMessageText(ofID, "Welcome to zCatch, a completely newly created version for Teeworlds 0.7. The ground work was done erdbaer & Teetime and is used as reference. Teelevision did a great job maintaining the mod after the Instagib Laser era. Also a thank you to TeeSlayer, who ported a basic version of zCatch to Teeworlds 0.7, that has also been used as reference.");

				GameServer()->SendServerMessage(ofID, "This zCatch modification is being created by jxsl13.");
			}
			else if(tokens[0] == "rules")
			{  
				GameServer()->SendServerMessage(ofID, "========== Rules ==========");
				GameServer()->SendServerMessageText(ofID, "zCatch is a Last Man Standing game mode. The last player to be alive will win the round. Each player killed by you is considered as caught. If you die, all of your caught players are released. If you catch all of them, you win the round. As a measure of fair play, you are able to release your caught players manually in reverse order. Releasing players is optional in zCatch. Type '/help release' for more information.");
				
			}
			else if(tokens[0] == "release" && size == 1)
			{
				class CPlayer *pPlayer = GameServer()->m_apPlayers[ofID];
				if(pPlayer)
				{
					pPlayer->ReleaseLastCaughtPlayer(CPlayer::REASON_PLAYER_RELEASED, true);
				}
			}
			else if(tokens[0] == "allmessages")
			{
				class CPlayer *pPlayer = GameServer()->m_apPlayers[ofID];
				bool isAlreadyDetailed = pPlayer->m_DetailedServerMessages;
				if(isAlreadyDetailed)
				{
					pPlayer->m_DetailedServerMessages = false;
					GameServer()->SendServerMessage(ofID, "Disabled detailed server messages.");
				}
				else
				{
					pPlayer->m_DetailedServerMessages = true;
					GameServer()->SendServerMessage(ofID, "Enabled detailed server messages.");
				}
			}
			else if(tokens[0] == "rank")
			{
				if(size > 1)
				{
					std::string cmd{pText + 1};
        			auto pos = cmd.find_first_of(' ');
					std::string nickname = cmd.substr(pos+1, std::string::npos);
					// ofID requests the data of nickname(ss.str())
					RequestRankingData(ofID, nickname);
				}
				else
				{
					// size == 1
					// request your own ranking data
					RequestRankingData(ofID, {Server()->ClientName(ofID)});
				}
			}
			else if (tokens[0] == "top")
			{
				RequestTopRankingData(ofID, "Score");
			}
			else
			{
				throw std::invalid_argument("");
			}
		}
		catch (const std::invalid_argument&)
		{
			GameServer()->SendServerMessage(ofID, "No such command, please try /help");
		}
	}
	else
	{
		return;
	}
}

bool CGameControllerZCATCH::OnCallvoteOption(int ClientID, const char* pDescription, const char* pCommand, const char* pReason)
{
	if(GameServer()->m_apPlayers[ClientID] && GameServer()->m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
	{
		GameServer()->SendServerMessage(ClientID, "Spectators are not allowed to start a vote.");
		return false;
	}


	// no command passed means that a custom vote was invoked.
	if(!pCommand)
	{
		dbg_msg("DEBUG", "Player %d called CUSTOM option '%s' with command '%s' and reason '%s'", ClientID, pDescription, pCommand, pReason);
		m_VoteOptionServer.ExecuteVoteOption(ClientID, {pDescription}, {pReason});
		return true;
	}
	else
	{
		dbg_msg("DEBUG", "Player %d called option '%s' with command '%s' and reason '%s'", ClientID, pDescription, pCommand, pReason);
		return true;
	}
}
bool CGameControllerZCATCH::OnCallvoteBan(int ClientID, int KickID, const char* pReason)
{
	// check voteban
	int TimeLeft = Server()->ClientVotebannedTime(ClientID);
	if (TimeLeft > 0)
	{
		char aChatmsg[128];
		str_format(aChatmsg, sizeof(aChatmsg), "You are not allowed to vote for the next %d:%02d min.", TimeLeft / 60, TimeLeft % 60);
		GameServer()->SendServerMessage(ClientID, aChatmsg);
		return false;
	}
	else if(GameServer()->m_apPlayers[ClientID] && GameServer()->m_apPlayers[KickID])
	{
		CPlayer& votingPlayer = *(GameServer()->m_apPlayers[ClientID]);
		CPlayer& votedPlayer = *(GameServer()->m_apPlayers[KickID]);

		if(votingPlayer.GetTeam() == TEAM_SPECTATORS)
		{
			GameServer()->SendServerMessage(ClientID, "Spectators are not allowed to start a vote.");
		}
		else if(votedPlayer.IsAuthed())
		{
			GameServer()->SendServerMessage(ClientID, "An internal error occurred, please try again.");
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "'%s'(ID: %d) tried to ban you. Reason: %s", Server()->ClientName(ClientID), ClientID, pReason);
			GameServer()->SendServerMessage(KickID, aBuf);
		}
		else
		{
			// none of the conditions valid, 
			// allow voting
			return true;
		}

		// if any of the conditions is valid
		// we want to prevent the voting.
		return false;	
	}
	return true;
}
bool CGameControllerZCATCH::OnCallvoteSpectate(int ClientID, int SpectateID, const char* pReason)
{
	// check voteban
	int TimeLeft = Server()->ClientVotebannedTime(ClientID);
	if (TimeLeft > 0)
	{
		char aChatmsg[128];
		str_format(aChatmsg, sizeof(aChatmsg), "You are not allowed to vote for the next %d:%02d min.", TimeLeft / 60, TimeLeft % 60);
		GameServer()->SendServerMessage(ClientID, aChatmsg);
		return false;
	}
	else if(GameServer()->m_apPlayers[ClientID] && GameServer()->m_apPlayers[SpectateID])
	{
		CPlayer& votingPlayer = *(GameServer()->m_apPlayers[ClientID]);
		CPlayer& votedPlayer = *(GameServer()->m_apPlayers[SpectateID]);

		if(votingPlayer.GetTeam() == TEAM_SPECTATORS)
		{
			GameServer()->SendServerMessage(ClientID, "Spectators are not allowed to start a vote.");
		}
		else if(votedPlayer.IsAuthed())
		{
			GameServer()->SendServerMessage(ClientID, "An internal error occurred, please try again.");
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "'%s'(ID: %d) tried to move you to the spectators. Reason: %s", Server()->ClientName(ClientID), ClientID, pReason);
			GameServer()->SendServerMessage(SpectateID, aBuf);
		}
		else
		{
			// none of the conditions valid, 
			// allow voting
			return true;
		}

		// if any of the conditions is valid
		// we want to prevent the voting.
		return false;	
	}
	

	dbg_msg("DEBUG", "Player %d called to move %d to spectators with reason '%s'", ClientID, SpectateID, pReason);
	return true;
}

void CGameControllerZCATCH::EndRound()
{
	// Change game state
	IGameController::EndRound();

	
	CPlayer *pPlayer = nullptr;

	for (int ID : GameServer()->PlayerIDs())
	{
		pPlayer = GameServer()->m_apPlayers[ID];
		if (pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
		{

			ShowPlayerStatistics(pPlayer);
			SaveRankingData(ID);

			// do cleanup
			pPlayer->ReleaseAllCaughtPlayers(CPlayer::REASON_EVERYONE_RELEASED);
			pPlayer = nullptr;
		}
	}

}

// game
void CGameControllerZCATCH::DoWincheckRound()
{
	if(m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60)
	{
		// check for time based win
		for(int i : GameServer()->PlayerIDs())
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->IsNotCaught())
				GameServer()->m_apPlayers[i]->m_Score++;
		}

		EndRound();
	}
	else
	{
		// check for survival win
		CPlayer *pAlivePlayer = nullptr;
		

		// keeping track of any change of ingame players.		
		m_PreviousIngamePlayerCount = m_IngamePlayerCount;
		m_IngamePlayerCount = 0;

		// keep track of change of player's being caught
		m_PreviousAlivePlayersCount = m_AlivePlayersCount;
		m_AlivePlayersCount = 0;

		for (int i : GameServer()->PlayerIDs())
		{
			if (GameServer()->m_apPlayers[i])
			{
				if (GameServer()->m_apPlayers[i]->IsNotCaught())
				{
					m_AlivePlayersCount++;
					pAlivePlayer = GameServer()->m_apPlayers[i];
				}

				// players actually ingame
				if (GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					m_IngamePlayerCount++;
				}
			}
		}

		if(m_IngamePlayerCount == 1 && m_AlivePlayersCount == 0)
		{
			// this is needed if exactly one player joins the game
			// the ound is being restarted in order for that player to join the game
			// and walk around
			EndRound();
		}	
		else if(m_IngamePlayerCount > 1 && m_AlivePlayersCount == 1)	// 1 winner
		{
			// player that is alive is the winnner
			int ScorePointsEarned = CalculateScore(pAlivePlayer->GetNumTotalCaughtPlayers());

			pAlivePlayer->m_Score += ScorePointsEarned;
			pAlivePlayer->m_Wins++;

			
			// Inform everyone about the winner.
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "'%s' won the round and earned %d score point%s!", Server()->ClientName(pAlivePlayer->GetCID()), ScorePointsEarned, (ScorePointsEarned == 1 ? "" : "s"));
			GameServer()->SendServerMessage(-1, aBuf);
			EndRound();
		}
		else
		{
			// nobody won anything.
			return;
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

	// set the joining player's skin color
	UpdateSkinsOf({player.GetCID()});
}

void CGameControllerZCATCH::OnPlayerConnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);
	int ID = player.GetCID();

	// Adds custom vote options to a player's vote options.
	m_VoteOptionServer.OnPlayerConnect(ID);

	// fill player's stats with database information.
	RetrieveRankingData(ID);

	// greeting
	OnChatMessage(ID, 0, ID, "/welcome");
	
	// warmup
	if (IsGameWarmup())
	{	
		UpdateSkinsOf({ID});

		// simply allow that player to join.
		IGameController::OnPlayerConnect(pPlayer);
		return;
	}

	// add tocaught players of dominatig player.
	class CPlayer *pDominatingPlayer = ChooseDominatingPlayer(ID);

	
	if (pDominatingPlayer)
	{
		pDominatingPlayer->CatchPlayer(player.GetCID(), CPlayer::REASON_PLAYER_JOINED);
	}
	else 
	{
		if (m_IngamePlayerCount >= g_Config.m_SvPlayersToStartRound)
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
	
	// needed to do the spawning stuff.
	IGameController::OnPlayerConnect(pPlayer);
}


void CGameControllerZCATCH::OnPlayerDisconnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);
	int ID = player.GetCID();

	//m_CustomVoteOptions[ID].clear();

	// save player's statistics to the database
	SaveRankingData(ID);
	
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

	// if player voted something/someone and it did not pass
    // before leaving the server, voteban him for the remaining time.
	if(GameServer()->m_apPlayers[ID])
	{
		int Now = Server()->Tick();
    	int Timeleft = (GameServer()->m_apPlayers[ID]->m_LastVoteCall - Now) + Server()->TickSpeed() * 60 ;
    	// convert to seconds
    	Timeleft = Timeleft / Server()->TickSpeed();

    	if (GameServer()->m_apPlayers[ID]->m_LastVoteCall && Timeleft > 0)
    	{
        	Server()->AddVoteban(ID, Timeleft);
		}
	}
	

	// needed to do the disconnect handling.
	IGameController::OnPlayerDisconnect(pPlayer);
}


int CGameControllerZCATCH::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	
	CPlayer& victim = (*pVictim->GetPlayer());
	CPlayer& killer = (*pKiller);
	dbg_assert(victim.IsNotCaught(), "victim is caught even tho it should not be caught.");

	// warmup
	if (IsGameWarmup())
	{
		// not killed by enemy.
		if(victim.GetCID() == killer.GetCID())
		{
			if(Weapon == WEAPON_SELF)
			{
				victim.m_RespawnTick = Server()->Tick()+Server()->TickSpeed() * 3;
				victim.m_Deaths++;
			}
			else if(Weapon == WEAPON_WORLD)
			{
				// fell into death tiles
				victim.m_Fails++;
			}
			else if (Weapon == WEAPON_GAME)
			{
				// nothing todo here
			}
		}
		else
		{
			// killed by enemy:
			killer.CatchPlayer(victim.GetCID(), CPlayer::REASON_PLAYER_WARMUP_CAUGHT);
			killer.ReleaseLastCaughtPlayer(CPlayer::REASON_PLAYER_WARMUP_RELEASED, true);
		}

		return 0;
	}
	
	// actual game
	/**
	 * CPlayer::KillCharacter -> CCharacter::Die -> IGameController::OnCharacterDeath
	 * the cases, where victim.GetNumCurrentlyCaughtPlayers() > 0 are handled in 
	 * CPlayer::KillCharacter, prior to these cases, where it is possible to prevent 
	 * the character from actually being killed, when releasing a player, 
	 * that was killed by mistake.
	 */
	if(victim.GetCID() != killer.GetCID() && Weapon >= 0)
	{
		// someone killed me with a real weapon

		// killer is still ingame
		if (killer.IsNotCaught())
		{
			killer.CatchPlayer(victim.GetCID());
		}
		else
		{
			// if the killer was caught before he killed someone
			// victim is not being caught, but must release everyone caught.
			victim.ReleaseAllCaughtPlayers();
		}		
	}
	else if(Weapon < 0 && victim.GetCID() == killer.GetCID())
	{
		switch (Weapon)
		{
		case WEAPON_WORLD: // death tiles etc.
			// needs to be handled in here, as WEAPON_WORLD deaths are not passed to the player::KillCharacter()
			victim.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_FAILED);
			victim.m_Fails++;
			break;
		case WEAPON_SELF: // suicide
			// here we catch literally the suicides, not the releases
			GameServer()->SendServerMessage(victim.GetCID(), "Was it really necessary to kill yourself?");
			break;
		case WEAPON_GAME: // team change, etc.
			// silent
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
		if (Weapon == WEAPON_WORLD || (Weapon == WEAPON_SELF && victim.GetNumCurrentlyCaughtPlayers() == 0))
		{
			victim.m_Deaths += g_Config.m_SvSuicidePenalty;
		}		
	}
		
	if(Weapon == WEAPON_SELF)
	{
		// respawn in 3 seconds
		victim.m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*3;
	}

	// update spectator modes for dead players in survival
	if(m_GameFlags&GAMEFLAG_SURVIVAL)
	{
		for(int i : GameServer()->PlayerIDs())
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_DeadSpecMode)
				GameServer()->m_apPlayers[i]->UpdateDeadSpecMode();
	}
	
	return 0;
}


void CGameControllerZCATCH::Tick()
{

	// debugging stuff to find the reason, why people get stuck in the twilight zone :D
	for (int ID : GameServer()->PlayerIDs())
	{
		if (GameServer()->m_apPlayers[ID]->IsCaught())
		{
			int KillerID = GameServer()->m_apPlayers[ID]->GetIDCaughtBy();
			bool validID = 0 <= KillerID  && KillerID < MAX_PLAYERS;
			bool RespawnTickNotInFuture = GameServer()->m_apPlayers[ID]->m_RespawnTick < Server()->Tick();
			if(!validID && RespawnTickNotInFuture)
			{
				dbg_msg("ABORT_DEBUG", "validID: victim_id: %d killer id: %d", ID, KillerID);
				dbg_msg("ABORT_DEBUG", "RespawnTickNotInFuture: Current tick: %d respawntick: %d", Server()->Tick(), GameServer()->m_apPlayers[ID]->m_RespawnTick);
			}
		}
	}
	

	// Broadcast Refresh is only needed for solely 
	// keeping the broadcast visible, but not to push
	// actual updates.
	if (Server()->Tick() % m_BroadcastRefreshTime == 0)
	{
		// The broadcast is according to heinrich5991 kept shown
		// by the client for 10 seconds.
		RefreshBroadcast();
	}
	

	// we have a change of ingame players.
	// either someone left or joined the spectators or joined the game(from lobby or from spec).
	// these two values are updated in DoWincheckRound()
	if(m_PreviousIngamePlayerCount != m_IngamePlayerCount || m_PreviousAlivePlayersCount != m_AlivePlayersCount)
		UpdateBroadcastOfEverybody();


	// checks if there are messages to process
	// and sends those messages if needed to the requesting
	// player.
	ProcessMessageQueue();


	// if there are any changed vote option, send the updates to the users.
	m_VoteOptionServer.ProcessVoteOptionUpdates();


	// we do not want WeaponModes to be changed mid game, as it is not supported
	if (m_WeaponMode != g_Config.m_SvWeaponMode)
	{
		// reset weapon mode if somone tries to change it while the server is sunning
		g_Config.m_SvWeaponMode = m_WeaponMode;
		GameServer()->SendServerMessage(-1, "If you want to change the weapon mode, please update your configuration file and restart the server.");
	}

	if(m_SkillLevel != g_Config.m_SvSkillLevel)
	{
		g_Config.m_SvSkillLevel = m_SkillLevel;
		GameServer()->SendServerMessage(-1, "If you want to change the skill level, please update your configuration file and restart the server.");
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
	else if(player.IsNotCaught() && player.GetNumCurrentlyCaughtPlayers() > 0 && Team == TEAM_SPECTATORS)
	{
		// player is not caught and wants to join the spectators.
		player.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_JOINED_SPEC);
	}
	else if(player.GetTeam() == TEAM_SPECTATORS && Team != TEAM_SPECTATORS)
	{
		// player joins the game after being in spec

		// do vanilla respawn stuff
		IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);

		// force player to spawn
		player.BeReleased(CPlayer::REASON_PLAYER_JOINED_GAME_AGAIN);
		return;
	}
	
	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
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
	if(IsGameWarmup())
		return;
	
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
				// players ingame, minus me, minus players that I caught
				enemiesLeft = m_IngamePlayerCount - 1 - pTmpPlayer->GetNumCurrentlyCaughtPlayers();
				pTmpPlayer->SetPlayersLeftToCatch(enemiesLeft);
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
	if(IsGameWarmup())
		return;
	
	CPlayer *pTmpPlayer = nullptr;
	char aBuf[32];
	int enemiesLeft = 0;

	for (int i : GameServer()->PlayerIDs())
	{
		pTmpPlayer = GameServer()->m_apPlayers[i];
		if (pTmpPlayer)
		{
			if (pTmpPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				enemiesLeft = m_IngamePlayerCount - 1 - pTmpPlayer->GetNumCurrentlyCaughtPlayers();
				pTmpPlayer->SetPlayersLeftToCatch(enemiesLeft);

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
	if(IsGameWarmup())
		return;
	
	// basically only print the stored values of each player 
	// and keeps the broadcast alive no updates are handled
	// in here. Those should be handled at their occurrence place.
	CPlayer *pTmpPlayer = nullptr;
	char aBuf[32];
	int enemiesLeft = 0;

	for (int i : GameServer()->PlayerIDs())
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
	for (int toID : GameServer()->PlayerIDs())
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
	for (int toID : GameServer()->PlayerIDs())
	{
		if (GameServer()->m_apPlayers[toID])
		{
			// send skin update message of id to everyone
			for (int ofID : GameServer()->PlayerIDs())
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
	double alivePercentage = 0.0f, caughtPercentage = 0.0f;
	double totalTicksPlayed = 0.0f;
	char aBuf[64];

	// calculate some small player statistics
	totalTicksPlayed = static_cast<double>(pOfPlayer->m_TicksIngame + pOfPlayer->m_TicksCaught);
	if (totalTicksPlayed > 0.0)
	{
		alivePercentage = (pOfPlayer->m_TicksIngame / totalTicksPlayed) * 100.0f;
		caughtPercentage = (pOfPlayer->m_TicksCaught / totalTicksPlayed) * 100.0f;

		// send them to the player
		str_format(aBuf, sizeof(aBuf), "Ingame: %.2f%% Spectating: %.2f%% ", alivePercentage, caughtPercentage);
		GameServer()->SendServerMessage(pOfPlayer->GetCID(), aBuf);
	}
}

CPlayer* CGameControllerZCATCH::ChooseDominatingPlayer(int excludeID)
{

	std::vector<class CPlayer*> dominatingPlayers;

	class CPlayer *pTmpPlayer = nullptr;
	int tmpMaxCaughtPlayers = -1;

	// find number of catches of dominating player
	for (int i : GameServer()->PlayerIDs())
	{
		if (excludeID == i)
			continue;
		
		pTmpPlayer = GameServer()->m_apPlayers[i];
		if (pTmpPlayer && pTmpPlayer->IsNotCaught())
		{
			if (pTmpPlayer->GetNumTotalCaughtPlayers() > tmpMaxCaughtPlayers )
			{
				tmpMaxCaughtPlayers = pTmpPlayer->GetNumTotalCaughtPlayers();
			}	
		}
	}

	// if invalid number
	if (tmpMaxCaughtPlayers <= 0)
	{
		return nullptr;
	}
	
	pTmpPlayer = nullptr;

	// find all players with the same catched players streak
	for (int i : GameServer()->PlayerIDs())
	{
		pTmpPlayer = GameServer()->m_apPlayers[i];
		// totalcaughtplayers = currently caught + those who left while being caught.
		if (pTmpPlayer && pTmpPlayer->GetNumTotalCaughtPlayers() == tmpMaxCaughtPlayers)
		{
			dominatingPlayers.push_back(pTmpPlayer);
		}
		pTmpPlayer = nullptr;
	}
	
	std::size_t pickedPlayer = Server()->Tick() % dominatingPlayers.size();
	return dominatingPlayers.at(pickedPlayer);
}

std::string CGameControllerZCATCH::GetDatabasePrefix(){
	return std::to_string(g_Config.m_SvWeaponMode) + "_" + std::to_string(g_Config.m_SvSkillLevel) + "_";
}

int CGameControllerZCATCH::CalculateScore(int PlayersCaught)
{	
	// clip
	int MaxPlayersCaught = PlayersCaught < MAX_PLAYERS - 1 ? PlayersCaught : MAX_PLAYERS - 1;

	// should be calculated at compile time.
	const double normalizeFactor = std::exp((MAX_PLAYERS - 1) / 5.0f);

	return static_cast<int>(10 * std::exp(MaxPlayersCaught / 5.0f) / normalizeFactor);
}

void CGameControllerZCATCH::RetrieveRankingData(int ofID)
{
	if(m_pRankingServer == nullptr)
		return;
	
	// retrieve data from database and execute the lambda-function, that is being passed.
	m_pRankingServer->GetRanking({Server()->ClientName(ofID)}, [this, ofID](CPlayerStats& stats)
	{
		// TODO:  I kind of do not trust myself here, this might actually blow up. :D
		CPlayer* pPlayer = GameServer()->m_apPlayers[ofID];

		// if a player has been destroyed, before we enter this, this ought to be a nullptr.
		if(pPlayer)
		{
			// only if this is started before the player is being destroyed, this should finish, 
			// before the player is destroyed 
			std::lock_guard<std::mutex> lock(pPlayer->m_PreventDestruction);
			pPlayer->m_Wins += stats["Wins"];
			pPlayer->m_Score += stats["Score"];
			pPlayer->m_Kills += stats["Kills"];
			pPlayer->m_Deaths += stats["Deaths"];
			pPlayer->m_TicksCaught += stats["TicksCaught"];
			pPlayer->m_TicksIngame += stats["TicksIngame"];
			pPlayer->m_TicksWarmup += stats["TicksWarmup"];
			pPlayer->m_Shots += stats["Shots"];
			pPlayer->m_Fails += stats["Fails"];
		}

	}, GetDatabasePrefix());
}
void CGameControllerZCATCH::SaveRankingData(int ofID)
{
	if(m_pRankingServer == nullptr)
		return;
	
	CPlayer* pPlayer = GameServer()->m_apPlayers[ofID];
	
	if(pPlayer == nullptr)
		return;
	
	m_pRankingServer->UpdateRanking({Server()->ClientName(ofID)}, {
		pPlayer->m_Kills,
		pPlayer->m_Deaths,
		pPlayer->m_TicksCaught,
		pPlayer->m_TicksIngame,
		pPlayer->m_TicksWarmup,
		pPlayer->m_Score,
		pPlayer->m_Wins,
		pPlayer->m_Fails,
		pPlayer->m_Shots,
	}, [](CPlayerStats& DatabaseValues, CPlayerStats& NewValues) -> CPlayerStats
	{
		// basically we retrieve values from the database
		// can look at them, and return the CPlayerStats that we want to 
		// be saved in the database.
		return NewValues;
	}, GetDatabasePrefix());
}

void CGameControllerZCATCH::RequestRankingData(int requestingID, std::string ofNickname)
{
	if (m_pRankingServer == nullptr)
	{
		GameServer()->SendServerMessage(requestingID, "This server does not track any player statistics.");
		return;
	}
	else if(!m_pRankingServer->IsValidNickname(ofNickname, GetDatabasePrefix()))
	{
		GameServer()->SendServerMessage(requestingID, "The requested nickname is not valid.");
		return;
	}

	m_pRankingServer->GetRanking(ofNickname, [this, requestingID, ofNickname](CPlayerStats &stats) {
		if (!stats.IsValid())
		{
			std::lock_guard<std::mutex> lock(m_MessageQueueMutex);
			m_MessageQueue.push_back(
				{
					requestingID,
				 	{
						ofNickname + " is not ranked."
					}
				});
			return;
		}

		std::stringstream ssIngameTime;
		std::stringstream ssCaughtTime;
		std::stringstream ssWarmupTime;

		int SecondsIngame = stats["TicksIngame"] / SERVER_TICK_SPEED;
		ssIngameTime << std::setw(2) << std::setfill('0') << (SecondsIngame / 3600) << ":"
		<< std::setw(2) << std::setfill('0') << ((SecondsIngame / 60) % 60) << ":"
		<< std::setw(2) << std::setfill('0') << (SecondsIngame % 60)
		<< "h";

		int SecondsCaught = stats["TicksCaught"] / SERVER_TICK_SPEED;
		ssCaughtTime << std::setw(2) << std::setfill('0') << (SecondsCaught / 3600) << ":" 
		<< std::setw(2) << std::setfill('0') << ((SecondsCaught / 60) % 60) << ":" 
		<< std::setw(2) << std::setfill('0') << (SecondsCaught % 60)
		<< "h";
				
		int SecondsWarmup = stats["TicksWarmup"] / SERVER_TICK_SPEED;
		ssWarmupTime << std::setw(2) << std::setfill('0') << (SecondsWarmup / 3600) << ":" 
		<< std::setw(2) << std::setfill('0') << ((SecondsWarmup / 60) % 60) << ":" 
		<< std::setw(2) << std::setfill('0') << (SecondsWarmup % 60)
		<< "h";
		

		// fill our message queue
		std::lock_guard<std::mutex> lock(m_MessageQueueMutex);
		m_MessageQueue.push_back(
			{
				requestingID,
				{
					"Showing statistics of " + ofNickname,
					"  Rank:   " + (stats["Score"] > 0 ? std::to_string(stats.GetRank()) : "Not ranked, yet."),
					"  Score:  " + std::to_string(stats["Score"]),
					"  Wins:   " + std::to_string(stats["Wins"]),
					"  Kills:  " + std::to_string(stats["Kills"]),
					"  Deaths: " + std::to_string(stats["Deaths"]),
					"  Ingame: " + ssIngameTime.str(), 
					"  Caught: " + ssCaughtTime.str(), 
					"  Warmup: " + ssWarmupTime.str(), 
					"  Fails:  " + std::to_string(stats["Fails"]),
					"  Shots:  " + std::to_string(stats["Shots"]),
				}
			}
		);
	}, GetDatabasePrefix());
	
}


void CGameControllerZCATCH::RequestTopRankingData(int requestingID, std::string key)
{
	if (m_pRankingServer == nullptr)
	{
		GameServer()->SendServerMessage(requestingID, "This server does not track any player statistics.");
		return;
	}

	// constants
	constexpr int topNumber = 5;
	constexpr bool biggestFirst = true;

	m_pRankingServer->GetTopRanking(topNumber, key, [this, requestingID, key](std::vector<std::pair<std::string, CPlayerStats> >& data){
	
	// messages that will be shown to the player
	std::vector<std::string> messages;

	// headline, in order for the player to see, what kind of ranking is being shown.
	std::stringstream ssHeadline;
	ssHeadline << "Top " << topNumber << " by " << key;
	messages.push_back(ssHeadline.str());

	// fill messages vector with retrieved ranks
	if (data.size() == 0)
	{
		messages.push_back("No ranks available, yet!");
	}
	else
	{
		int counter = 0;
		for (auto &[nickname, stats] : data)
		{
			if (stats["Score"] > 0)
			{
				messages.push_back("[" + std::to_string(stats[key]) + "] " + nickname);
				counter++;
			}
		}
		if (counter == 0)
		{
			messages.push_back("No ranks available, yet!");
		}
		
	}

	// add messages to MessageQueue
	std::lock_guard<std::mutex> lock(m_MessageQueueMutex);
	m_MessageQueue.push_back(
		{
			requestingID, 
			messages
		}
	);
	}, GetDatabasePrefix(), biggestFirst);

}


void CGameControllerZCATCH::ProcessMessageQueue()
{

	// if we successfully lock the mutex, process all pending messages
	// if the mutex cannot be aquired, we skip this in order not to block the
	// main thread.
	if (m_MessageQueueMutex.try_lock())
	{
		if(m_MessageQueue.empty())
		{
			m_MessageQueueMutex.unlock();
			return;
		}
				
		for (auto& [requestingID, messages] : m_MessageQueue)
		{
			for (std::string& message : messages)
			{
				GameServer()->SendServerMessage(requestingID, message.c_str());
			}
			
		}

		// delete all entries.
		m_MessageQueue.clear();

		m_MessageQueueMutex.unlock();
	}
	

}

