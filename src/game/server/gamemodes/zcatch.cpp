/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <generated/protocol.h>
#include "zcatch.h"
#include <game/server/gamemodes/zcatch/playerstats.h>
#include <game/version.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>

extern const char *GIT_VERSION;
extern const char *GIT_SHORTREV_HASH;


CGameControllerZCATCH::CGameControllerZCATCH(CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "zCatch";
	m_WeaponMode = g_Config.m_SvWeaponMode;
	m_SkillLevel = g_Config.m_SvSkillLevel;
	m_GameFlags = GAMEFLAG_SURVIVAL;

	m_PreviousIngamePlayerCount = 0;
	m_IngamePlayerCount = 0;

	// refresh broadcast every 9 seconds.
	m_BroadcastRefreshTime = Server()->TickSpeed() * 9;

	m_pRankingServer = nullptr;
	InitRankingServer();	

	
	// reserve double the needed space, 
	// in order to not have any reallocations
	// mid game
	m_LeftCaughtCache.reserve(MAX_CLIENTS * 2 * 2);
	
	for (size_t i = 0; i < MAX_CLIENTS; i++)
	{
		m_PlayerKickTicksCountdown[i] = NO_KICK;
	}

	ChatCommandsOnInit();


	m_DeletionRequest = CRankDeletionRequest([this](CRankDeletionRequest::DeletionType Type, std::string Nickname){
		if (m_pRankingServer == nullptr) {
			return;
		}

		CPlayer* pIngamePlayer = nullptr;
		int IngameID = -1;
		auto RequestedNickCString = Nickname.c_str();
		
		// check if nickname is online
		for (int ID : GameServer()->PlayerIDs())
		{
			if (!strcmp(RequestedNickCString, Server()->ClientName(ID)))
			{	
				pIngamePlayer = GameServer()->m_apPlayers[ID];
				if (pIngamePlayer)
				{
					IngameID = ID;
				}		
			}
		}
		

		// completely removes the player from the database.
		if (Type == CRankDeletionRequest::DELETION) 
		{
			// delete database stats
			m_pRankingServer->DeleteRanking(Nickname, GetDatabasePrefix());

			if(!pIngamePlayer) 
			{
				return;
			}

			// reset ingame stats
			pIngamePlayer->m_Kills = 0;
			pIngamePlayer->m_Deaths = 0;
			pIngamePlayer->m_TicksCaught = 0;
			pIngamePlayer->m_TicksIngame = 0;
			pIngamePlayer->m_TicksWarmup = 0;
			pIngamePlayer->m_Score = 0;
			pIngamePlayer->m_Wins = 0;
			pIngamePlayer->m_Fails = 0;
			pIngamePlayer->m_Shots = 0;
			return;
		}


		// stats are valid by default
		CPlayerStats ingameStats;

		if (!pIngamePlayer) {
			// player stats are invalid because player is offline
			ingameStats.Invalidate();
		} else {
			// player stats are valid
			ingameStats["Kills"] = pIngamePlayer->m_Kills;
			ingameStats["Deaths"] = pIngamePlayer->m_Deaths;
			ingameStats["TicksCaught"] = pIngamePlayer->m_TicksCaught;
			ingameStats["TicksIngame"] = pIngamePlayer->m_TicksIngame;
			ingameStats["TicksWarmup"] = pIngamePlayer->m_TicksWarmup;
			ingameStats["Score"] = pIngamePlayer->m_Score;
			ingameStats["Wins"] = pIngamePlayer->m_Wins;
			ingameStats["Fails"] = pIngamePlayer->m_Fails;
			ingameStats["Shots"] = pIngamePlayer->m_Shots;
		}

		// requires individual player data
		m_pRankingServer->GetRanking(
			Nickname, 
			[this, IngameID, ingameStats, Type, Nickname](CPlayerStats& stats)
			{	
				// ingame stats are more up to date compared to database stats
				if (ingameStats.IsValid())
				{
					stats = ingameStats;
				}
				
				// manipulate player's stats
				switch (Type)
				{
				case CRankDeletionRequest::SCORE_AND_WIN_RESET:
					stats["Wins"] = 0;
					[[fallthrough]];
				case CRankDeletionRequest::SCORE_RESET:
					stats["Score"] = 0;
					break;
				default:
					break;
				}

				// update ingame player's stats if online
				if (IngameID >= 0)
				{	
					// do not add, but explicitly set values.
					stats.SetHandlingMode(CPlayerStats::STATS_UPDATE_SET);
					std::lock_guard<std::mutex> lock(m_RankingRetrievalMessageQueueMutex);
					m_RankingRetrievalMessageQueue.emplace_back(IngameID, stats);
				}
				
				// update database player stats
				m_pRankingServer->SetRanking(Nickname, stats, GetDatabasePrefix());
			}, 
			GetDatabasePrefix());

	});
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

void CGameControllerZCATCH::ChatCommandsOnInit()
{
	struct Command
	{
		const char* name;
		const char* description;
		bool enabled;
	};

	bool isRankingEnabled = m_pRankingServer;

	// Add all commands, client wont sort alphabetically!
	Command commands[] = {
		{"help", "See a list of commands to help you with you questions.", true},
		{"info", "See some information about the zCatch mod.", true},
		{"version", "See the current server version.", GIT_VERSION != 0},
		{"rules", "A quick read about how zCatch is played.", true},
		{"release", "Release player. See '/help release' for more info.", true},
		{"allmessages", "Enable or disable extra messages.", true},
		{"top", "See the server's top players.", isRankingEnabled},
		{"rank", "See a player's ranking statistics.", isRankingEnabled},
	};

	for (size_t i = 0; i != sizeof(commands) / sizeof(commands[0]); ++i)
	{
		if (commands[i].enabled)
		{
			const char* name = commands[i].name;
			CommandsManager()->AddCommand(name, "", commands[i].description, [this, name](class CPlayer* pPlayer, const char* pArgs) {
				OnPlayerCommandImpl(pPlayer, name, pArgs);
			});
		}
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


void CGameControllerZCATCH::OnPlayerCommandImpl(class CPlayer* pPlayer, const char* pCommandName, const char* pArgs)
{
	int ofID = pPlayer->GetCID();

	// parse words as tokens
	std::stringstream commandLine(pArgs);
	std::string tmpString;
	std::vector<std::string> tokens;
	tokens.push_back(pCommandName);

	while (std::getline(commandLine, tmpString, ' '))
	{
		tokens.push_back(tmpString);
	}


	// There is difference between 0.7.4 and 0.7.5 clients:
	// 
	// In 0.7.4 command name included in args (i.e. if someone type 
	// "/help list" server got pCommandName=="help", pArgs="/help list")
	// 
	// In 0.7.5 command name not included in args (i.e. if someone type 
	// "/help list" server got pCommandName=="help", pArgs="list")
	//
	// So, we fix here 0.7.4 behaviour to be 0.7.5 conformant
	// TODO: remove this hack when 0.7.4 becomes unpopular. 
	if ((tokens.size() >= 2) && ("/" + tokens[0]) == (tokens[1]))
	{
		tokens.erase(tokens.begin() + 1);
	}

	size_t size = tokens.size();

	try
	{
		if (tokens[0] == "help")
		{
			if (size == 1)
			{
				GameServer()->SendServerMessage(ofID, "========== Help ==========");
				GameServer()->SendServerMessage(ofID, "/rules - If you want to know about zCatch's ruleset.");
				GameServer()->SendServerMessage(ofID, "/info - If to know about this mod's creators.");
				GameServer()->SendServerMessage(ofID, "/version - To see the server version.");
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

					GameServer()->SendServerMessageText(ofID, "The player ranking saves some of your playing statistics in a database. You can see your own statistics by typing the command /rank in the chat. If you want to see someone else's statistics, write /rank <nickname> instead. In order to see the top players on the server, use the /top command.");
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
		else if (tokens[0] == "version") {
			char aBuf[64];
			if (GIT_VERSION != 0) {
				str_format(aBuf, sizeof(aBuf), "zCatch %s", GIT_VERSION);
				GameServer()->SendServerMessage(ofID, aBuf);
			}

			if (GIT_SHORTREV_HASH != 0) {
				str_format(aBuf, sizeof(aBuf), "Hash %s", GIT_SHORTREV_HASH);
				GameServer()->SendServerMessage(ofID, aBuf);
			}	

			if (GAME_RELEASE_VERSION != 0) {
				str_format(aBuf, sizeof(aBuf), "Teeworlds %s", GAME_RELEASE_VERSION);
				GameServer()->SendServerMessage(ofID, aBuf);
			}	
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
				std::string nickname = pArgs;

				// Note: we can't use tokens here, because need to handle 
				// nicknames with spaces. 
				// 
				// There are two cases:
				// 0.7.4 client sends pArgs = "/rank foo" 
				// 0.7.5 client sends pArgs = "foo"
				// 
				// So, detect 0.7.4 case and make it 0.7.5 conformant. 
				
				
				if (nickname.find("/rank ") == 0)
				{
					// 0.7.4 case 
					// TODO: Remove this when 0.7.4 client becomes unpopular
					// Sorry guy with nickname '/rank ':( 

					auto pos = nickname.find_first_of(' ');
					nickname = nickname.substr(pos + 1, std::string::npos);
				}

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
		else if (tokens[0] == "hair") {
			GameServer()->SendServerMessageText(ofID, "In zCatch there is a single golden rule that anyone should know of. It is easily written down but is so profound that people will need years to comprehend it: No hair, no win! You will not win a round of zCatch unless your Tee has hair or another kind of decoration on top of its head.");	
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

void CGameControllerZCATCH::OnChatMessage(int ofID, int Mode, int toID, const char *pText)
{
	if (!pText)
	{
		return;
	}
	
	CPlayer* pPlayer = GameServer()->m_apPlayers[ofID];

	// Handle commands
	if(pText[0] == '/')
	{
		// There are 2 possible cases then some command handled by this function:
		// 1) This is unexistent command, so there is no appropriate handler
		// 2) In version 0.7.3 and below there are no NETMSGTYPE_CL_COMMAND msgID,
		//    so all commands are received as usual chat message
		// 
		// In both cases we can manually call OnPlayerCommandImpl to handle command.

		std::string command;
		// remove slash
		++pText;
		
		// accumulate command
		while (*pText && *pText != ' ')
		{
			command += *pText;
			++pText;
		}

		// skip whitespaces
		while (*pText && *pText == ' ')
		{
			++pText;
		}

		OnPlayerCommandImpl(pPlayer, command.c_str(), pText);
		return;
	}


	// check if player is allowed to chat handle auto mute.
	// IsAllowedToChat also informs the player about him being muted.
	if(pPlayer && GameServer()->IsAllowedToChat(ofID))
	{
		IGameController::OnChatMessage(ofID, Mode, toID, pText);
	}
}

bool CGameControllerZCATCH::OnCallvoteOption(int ClientID, const char* pDescription, const char* pCommand, const char* pReason)
{

	if(GameServer()->m_apPlayers[ClientID] && GameServer()->m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
	{
		GameServer()->SendServerMessage(ClientID, "Spectators are not allowed to start a vote.");
		return false;
	}

	int TimeLeft = Server()->ClientVotebannedTime(ClientID);

	if (TimeLeft > 0)
	{
		char aChatmsg[128];
		str_format(aChatmsg, sizeof(aChatmsg), "You are not allowed to vote for the next %d:%02d min.", TimeLeft / 60, TimeLeft % 60);
		GameServer()->SendServerMessage(ClientID, aChatmsg);
		return false;
	}

	// convert to c++ string
	std::string Command(pCommand);


	// expect the command to start at position 0.
	if(Command.find("change_map") == 0 || Command.find("sv_map") == 0)
	{
		// map change cooldown
		int CooldownInSeconds = g_Config.m_SvMapChangeCooldown * 60;
		int SecondsPassedAfterLastMapChange = (Server()->Tick() - Server()->m_LastMapChangeTick) / Server()->TickSpeed();
		dbg_msg("DEBUG", "SecondsPassedAfterLastMapChange: %d", SecondsPassedAfterLastMapChange);
		
		if(SecondsPassedAfterLastMapChange <= CooldownInSeconds)
		{
			
			int totalSecondsLeftToWait = CooldownInSeconds - SecondsPassedAfterLastMapChange;
			int minsLeftToWait = totalSecondsLeftToWait / 60;
			int secondsLeftToWait = totalSecondsLeftToWait % 60;

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Please wait for another %d:%02d mins before trying to change the map.", minsLeftToWait, secondsLeftToWait);
			GameServer()->SendServerMessage(ClientID, aBuf);
			
			return false;
		}


		CPlayer* pDominatingPlayer = ChooseDominatingPlayer();		

		if(pDominatingPlayer)
		{
			int NumCaughtPlayers = pDominatingPlayer->GetNumTotalCaughtPlayers();
			int Limit = (int)(((m_IngamePlayerCount - 1) * 0.5)); // 50% of players caught

			// can only change map if less than 50% are caught.
			dbg_msg("DEBUG", "IngamePlayers: %d, TotalCaughtPlayers: %d, CurrentlyCaughtPlayers: %d, treshold(50%%): %d", m_IngamePlayerCount, NumCaughtPlayers, pDominatingPlayer->GetNumCurrentlyCaughtPlayers(),  Limit);
			if(m_IngamePlayerCount >= g_Config.m_SvPlayersToStartRound && NumCaughtPlayers >= Limit)
			{
				GameServer()->SendServerMessage(ClientID, "You cannot change the map, while someone is currently on a winning streak.");
				return false;
			}
		}
	}
	

	dbg_msg("DEBUG", "Player %d called option '%s' with command '%s' and reason '%s'", ClientID, pDescription, pCommand, pReason);
	return true;
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
		std::string Reason{pReason};

		if(votingPlayer.GetTeam() == TEAM_SPECTATORS)
		{
			GameServer()->SendServerMessage(ClientID, "Spectators are not allowed to start a vote.");
		}
		else if(Reason == "No reason given")
		{
			// inform everyone except the voting player.
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "'%s' tried to funvote '%s'", Server()->ClientName(ClientID),Server()->ClientName(KickID));

			for(int ID: GameServer()->PlayerIDs())
			{
				if(ID == ClientID)
					continue;
				
				GameServer()->SendServerMessage(ID, aBuf);
			}

			// Tell the voting player that these votes ate not allowed.
			GameServer()->SendServerMessage(ClientID, "Could not start the kickvote because no reason was given.");

			
			return false;
		}
		else if(votedPlayer.IsAuthed())
		{
			GameServer()->SendServerMessage(ClientID, "An internal error occurred, please try again.");
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "'%s'(ID: %d) tried to ban you. Reason: %s", Server()->ClientName(ClientID), ClientID, pReason);
			GameServer()->SendServerMessage(KickID, aBuf);
			return false;
		}
		else if(!g_Config.m_SvKickvoteOnWarmup && m_IngamePlayerCount < g_Config.m_SvPlayersToStartRound)
		{
			GameServer()->SendServerMessage(ClientID, "Not enough players to start a kickvote.");
			return false;
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
		std::string Reason{pReason};

		if(votingPlayer.GetTeam() == TEAM_SPECTATORS)
		{
			GameServer()->SendServerMessage(ClientID, "Spectators are not allowed to start a vote.");
		}
		else if(Reason == "No reason given")
		{
			GameServer()->SendServerMessage(ClientID, "Please specify a reason.");
			return false;
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
		CPlayer *pAlivePlayer = 0;
		
		int alivePlayerCount = 0;


		// keeping track of any change of ingame players.		
		m_PreviousIngamePlayerCount = m_IngamePlayerCount;
		m_IngamePlayerCount = 0;

		for (int i : GameServer()->PlayerIDs())
		{
			if (GameServer()->m_apPlayers[i])
			{
				if (GameServer()->m_apPlayers[i]->IsNotCaught())
				{
					alivePlayerCount++;
					pAlivePlayer = GameServer()->m_apPlayers[i];
				}

				// players actually ingame
				if (GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					m_IngamePlayerCount++;
				}	
			}
		}


		if(m_IngamePlayerCount == 1 && alivePlayerCount == 0)
		{
			// this is needed if exactly one player joins the game
			// the ound is being restarted in order for that player to join the game
			// and walk around
			EndRound();
		}	
		else if(m_IngamePlayerCount > 1 && alivePlayerCount == 1)	// 1 winner
		{

			char aBuf[128];

			if(m_pRankingServer)
			{
				// player that is alive is the winnner
				int ScorePointsEarned = CalculateScore(pAlivePlayer->GetNumTotalCaughtPlayers());

				pAlivePlayer->m_Score += ScorePointsEarned;
				pAlivePlayer->m_Wins++;

				
				// Inform everyone about the winner.
				str_format(aBuf, sizeof(aBuf), "'%s' won the round and earned %d score point%s!", Server()->ClientName(pAlivePlayer->GetCID()), ScorePointsEarned, (ScorePointsEarned == 1 ? "" : "s"));
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), "'%s' won the round!", Server()->ClientName(pAlivePlayer->GetCID()));
			}

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

	// Gets health here.
	character.IncreaseHealth(10);

	// if we spawn, we should not have anyone caught 
	// from previous rounds or weird post mortem kills.
	player.ReleaseAllCaughtPlayers();

	// Reset joining player's skin color
	UpdateSkinsOf({player.GetCID()});

	// If the player wanted to join the spectators, 
	// let them join the spectators
	if (player.GetWantsToJoinSpectators())
	{
		DoTeamChange(pChr->GetPlayer(), TEAM_SPECTATORS);
		player.ResetWantsToJoinSpectators();
		return;
	}
}

void CGameControllerZCATCH::ChatCommandsOnPlayerConnect(CPlayer *pPlayer)
{
	class IServer* pServer = Server();
	int ID = pPlayer->GetCID();

	// Remove the clientside commands (except w, whisper & r)
	CommandsManager()->SendRemoveCommand(pServer, "all", ID);
	CommandsManager()->SendRemoveCommand(pServer, "friend", ID);
	CommandsManager()->SendRemoveCommand(pServer, "m", ID);
	CommandsManager()->SendRemoveCommand(pServer, "mute", ID);
	CommandsManager()->SendRemoveCommand(pServer, "team", ID);


	// send the initially registered commands
	CommandsManager()->OnPlayerConnect(pServer, pPlayer);
}

void CGameControllerZCATCH::OnPlayerConnect(class CPlayer *pPlayer)
{
	CPlayer& player = (*pPlayer);
	int ID = player.GetCID();

	char aClientAddr[NETADDR_MAXSTRSIZE];
	Server()->GetClientAddr(ID, aClientAddr, NETADDR_MAXSTRSIZE, true);

	int ClientVersion = Server()->GetClientVersion(ID);
	int Country = Server()->ClientCountry(ID);
	const char* Name = Server()->ClientName(ID);
	const char* Clan = Server()->ClientClan(ID);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf),"id=%d addr=%s version=%d name='%s' clan='%s' country=%d", ID, aClientAddr, ClientVersion, Name, Clan, Country);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client_enter", aBuf);

	// TrollPit handling for joining players
	// if player in troll pit, set troll status
	int TrollPitPosition = GameServer()->IsInTrollPit(ID);
	if (TrollPitPosition >= 0) 
	{
		player.SetTroll();
		GameServer()->AddIngameTroll(ID);
	}

	// send chat commands
	ChatCommandsOnPlayerConnect(pPlayer);

	// no auto kick for player yet.
	// people usually are forced to stay in spec, 
	// but if they happen to enter the game somehow, kick them
	// after x seconds.
	SetKickIn(ID, NO_KICK);
	

	// fill player's stats with database information.
	RetrieveRankingData(ID);

	// greeting
	GameServer()->SendServerMessageText(ID, "Welcome to zCatch, where you kill all of your enemies to win a round. Write '/help' for more information.");
	
	// warmup
	if (IsGameWarmup())
	{	

		// simply allow that player to join.
		IGameController::OnPlayerConnect(pPlayer);
		return;
	}
	else if(m_IngamePlayerCount > MAX_PLAYERS)
	{
		// if the server is full, we do not want the 
		// joining player to be caught by anyone, 
		// but just join the spectators instead.
		dbg_msg("DEBUG", "Player %d joined the TEAM: %d", ID, player.GetTeam());

		// force into spec
		DoTeamChange(pPlayer, TEAM_SPECTATORS);
		IGameController::OnPlayerConnect(pPlayer);
		return;
	}


	bool IsAlreadyCaught = HandleJoiningPlayerCaching(ID);
	
	if(!IsAlreadyCaught)
	{
		// add to caught players of dominatig player.
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
	}
	
	
	// needed to do the spawning stuff.
	IGameController::OnPlayerConnect(pPlayer);
}


void CGameControllerZCATCH::OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason)
{
	CPlayer& player = (*pPlayer);
	int ID = player.GetCID();

	char aClientAddr[NETADDR_MAXSTRSIZE];
	Server()->GetClientAddr(ID, aClientAddr, NETADDR_MAXSTRSIZE);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "id=%d addr=%s reason='%s'", ID, aClientAddr, pReason);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client_drop", aBuf);


	// if player is caught, associate him iwth their killer for
	// a specific time after disconnecting
	// needs to be first
	HandleLeavingPlayerCaching(ID);

	// if the player is caught, he will be released
	player.BeSetFree(CPlayer::REASON_PLAYER_LEFT);

	// release caught players in any case!
	player.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_LEFT);


	// if player voted something/someone and it did not pass
    // before leaving the server, voteban him for the remaining time.
	int Now = Server()->Tick();
    int Timeleft = (player.m_LastVoteCall - Now) + Server()->TickSpeed() * 60 ;
    // convert to seconds
    Timeleft = Timeleft / Server()->TickSpeed();

    if (player.m_LastVoteCall && Timeleft > 0)
    {
        Server()->AddVoteban(ID, Timeleft);
	}

	// remove player from troll list
	// so that no messages are send to him.
	if (player.IsTroll()) 
	{
		GameServer()->RemoveIngameTroll(ID);
	}
	
	// save player's statistics to the database
	SaveRankingData(ID);

	// needed to do the disconnect handling.
	IGameController::OnPlayerDisconnect(pPlayer, pReason);
}


int CGameControllerZCATCH::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	
	CPlayer& victim = (*pVictim->GetPlayer());
	CPlayer& killer = (*pKiller);

	int victimID = victim.GetCID();
	int killerID = killer.GetCID();


	// clear rejoin cache after the player died
	// in any case!
	HandleDyingPlayerCaching(victimID);

	// warmup
	if (IsGameWarmup())
	{
		// not killed by enemy.
		if(victimID == killerID)
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

			// no need to release anyone, as nobody is being caught in warmup
		}
		else
		{
			// killed by enemy:
			killer.CatchPlayer(victimID, CPlayer::REASON_PLAYER_WARMUP_CAUGHT);
			bool updateSkinColors = true;
			killer.ReleaseLastCaughtPlayer(CPlayer::REASON_PLAYER_WARMUP_RELEASED, updateSkinColors);
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
	if(victimID != killerID && Weapon >= 0)
	{
		// someone killed me with a real weapon

		// killer is still ingame
		if (killer.IsNotCaught())
		{
			// handles catching of players properly
			// if the player is already caught, the player won't get caught again.
			// meaning if the player is caught by two different players within the exact same tick, 
			// the player with the lower ID will get the kill.
			killer.CatchPlayer(victimID);
		}


		// if the killer was caught before he killed someone
		// victim is not being caught, but must release everyone caught.
		victim.ReleaseAllCaughtPlayers();

	}
	else if(Weapon < 0 && victimID == killerID)
	{
		// killed by non-character enemy

		switch (Weapon)
		{
		case WEAPON_WORLD: // death tiles etc.
			// needs to be handled in here, as WEAPON_WORLD deaths are not passed to the player::KillCharacter()
			victim.ReleaseAllCaughtPlayers(CPlayer::REASON_PLAYER_FAILED);
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
	UpdateBroadcastOf({victimID, killerID});

	// update colors of both players
	UpdateSkinsOf({victimID, killerID});

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
            victim.m_Score -= g_Config.m_SvSuicidePenalty * g_Config.m_SvDeathScore;
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


	if (Server()->Tick() % Server()->TickSpeed() == 0)
	{
		// do cache cleanup of left players that were caught
		// once every second.
		CleanLeftCaughtCache();
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
	if(m_PreviousIngamePlayerCount != m_IngamePlayerCount)
		UpdateBroadcastOfEverybody();


	// checks if there are messages to process
	// and sends those messages if needed to the requesting
	// player.
	ProcessMessageQueue();


	// message queue that contains the retrieved player data from the 
	// database.
	ProcessRankingRetrievalMessageQueue();


	// beginner server
	KickCountdownOnTick();


	// we do not want WeaponModes to be changed mid game, as it is not supported
	if (m_WeaponMode != g_Config.m_SvWeaponMode)
	{
		// reset weapon mode if someone tries to change it while the server is sunning
		g_Config.m_SvWeaponMode = m_WeaponMode;
		GameServer()->SendServerMessage(-1, "If you want to change the weapon mode, please update your configuration file and restart the server.");
	}
	
	if(m_SkillLevel != g_Config.m_SvSkillLevel)
	{
		g_Config.m_SvSkillLevel = m_SkillLevel;
		GameServer()->SendServerMessage(-1, "If you want to change the skill level, please update your configuration file and restart the server.");
	}


	// if  there is no ranking, we want the current killing spree
	// to be the score
	if(!m_pRankingServer)
	{
		for (int ID : GameServer()->PlayerIDs())
		{
			if(GameServer()->m_apPlayers[ID])
				GameServer()->m_apPlayers[ID]->m_Score = GameServer()->m_apPlayers[ID]->GetNumCaughtPlayersInARow();
		}
		
	}

	// process deletion requests every second.
	if (Server()->Tick() % SERVER_TICK_SPEED == 0)
	{
		m_DeletionRequest.ProcessDeletion();
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
		// player wants to join the game
		if (m_PlayerKickTicksCountdown[player.GetCID()] != NO_KICK && !player.IsAuthed())
		{
			// player that's not allowed to play on the beginner server cannot join the game.
			GameServer()->SendServerMessage(player.GetCID(), g_Config.m_SvBeginnerServerKickWarning);
			return;
		}
		else if(m_IngamePlayerCount > MAX_PLAYERS)
		{
			GameServer()->SendServerMessage(player.GetCID(), "The server is currently full, you cannot join the game.");
			return;
		}
		else
		{
			CPlayer* pDominatingPlayer = ChooseDominatingPlayer();
			if (pDominatingPlayer) {
				int caughtPlayers = pDominatingPlayer->GetNumTotalCaughtPlayers();
				int playersLeftToCatch = pDominatingPlayer->GetPlayersLeftToCatch();
				if (1 + caughtPlayers + playersLeftToCatch == MAX_PLAYERS && playersLeftToCatch <= 2) {
					// edge case where leaving caught players allow spectators to join, 
					// be caught, leave and allowing the next spectator to join.
					// the exploit can be done until the player has caught up to 15 players in total
					// if MAX_PLAYERS == 16
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "You can join the game once '%s' wins or dies.", Server()->ClientName(pDominatingPlayer->GetCID()));
					GameServer()->SendServerMessage(player.GetCID(), aBuf);
					return;
				}
			}
			


			// players joins the game after being in spec

			// do vanilla respawn stuff
			IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);

			// force player to spawn
			player.BeReleased(CPlayer::REASON_PLAYER_JOINED_GAME_AGAIN);
			return;
		}
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

void CGameControllerZCATCH::UpdateIngamePlayerCount()
{
	CPlayer* pTmpPlayer = nullptr;
	
	m_PreviousIngamePlayerCount = m_IngamePlayerCount;
	m_IngamePlayerCount = 0;

	for(int ClientID : GameServer()->PlayerIDs())
	{
		pTmpPlayer = GameServer()->m_apPlayers[ClientID];
		if(pTmpPlayer && pTmpPlayer->GetTeam() != TEAM_SPECTATORS)
			++m_IngamePlayerCount;
		pTmpPlayer = nullptr;
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
				// this is basically updated, when someone joins the spectators
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
				// this is basically updated, when someone joins the spectators
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
		// players that do not have anyone caught are considered not dominating
		// this behavior is explicitly used, do not change <= to <
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
	// choose random dominating player
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
		std::lock_guard<std::mutex> lock(m_RankingRetrievalMessageQueueMutex);

		m_RankingRetrievalMessageQueue.emplace_back(ofID, stats);

	}, GetDatabasePrefix());
}

void CGameControllerZCATCH::SaveRankingData(int ofID)
{
	if(m_pRankingServer == nullptr)
		return;
	
	CPlayer* pPlayer = GameServer()->m_apPlayers[ofID];
	
	if(pPlayer == nullptr)
		return;

	// if player leaves before his data was fetched from db.
	if(!pPlayer->m_IsRankFetched)
		return;
	
	m_pRankingServer->SetRanking({Server()->ClientName(ofID)}, {
		pPlayer->m_Kills,
		pPlayer->m_Deaths,
		pPlayer->m_TicksCaught,
		pPlayer->m_TicksIngame,
		pPlayer->m_TicksWarmup,
		pPlayer->m_Score,
		pPlayer->m_Wins,
		pPlayer->m_Fails,
		pPlayer->m_Shots,
	}, GetDatabasePrefix());
}

void CGameControllerZCATCH::RequestRankingData(int requestingID, std::string ofNickname)
{
	if (m_pRankingServer == nullptr)
	{
		std::string message = "Can't get ranking for " + ofNickname + ", because this server does not track any player statistics.";
		GameServer()->SendServerMessage(requestingID, message.c_str());
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
	constexpr bool biggestFirst = true;
	constexpr int topNumber = 5;

	m_pRankingServer->GetTopRanking(topNumber, key, 
	[this, topNumber, requestingID, key](std::vector<std::pair<std::string, CPlayerStats> >& data){
	
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
	// if the mutex cannot be aquired,we skip this in order not to block the
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

void CGameControllerZCATCH::ProcessRankingRetrievalMessageQueue()
{

	// if we successfully lock the mutex, process all pending messages
	// if the mutex cannot be aquired,we skip this in order not to block the
	// main thread.
	if (m_RankingRetrievalMessageQueueMutex.try_lock())
	{
		if(m_RankingRetrievalMessageQueue.empty())
		{
			m_RankingRetrievalMessageQueueMutex.unlock();
			return;
		}

		CPlayer* pPlayer = nullptr;	
		for (auto& [ID, stats] : m_RankingRetrievalMessageQueue)
		{
			pPlayer = GameServer()->m_apPlayers[ID];

			if (!pPlayer)
			{
				continue;
			}

			switch (stats.GetHandlingMode())
			{
			case CPlayerStats::STATS_UPDATE_SET:
				pPlayer->m_Wins = stats["Wins"];
				pPlayer->m_Score = stats["Score"];
				pPlayer->m_Kills = stats["Kills"];
				pPlayer->m_Deaths = stats["Deaths"];
				pPlayer->m_TicksCaught = stats["TicksCaught"];
				pPlayer->m_TicksIngame = stats["TicksIngame"];
				pPlayer->m_TicksWarmup = stats["TicksWarmup"];
				pPlayer->m_Shots = stats["Shots"];
				pPlayer->m_Fails = stats["Fails"];
				pPlayer->m_Rank = stats.GetRank();
				pPlayer->m_IsRankFetched = true;
				break;
			case CPlayerStats::STATS_UPDATE_ADD:
				[[fallthrough]];
			default:
				pPlayer->m_Wins += stats["Wins"];
				pPlayer->m_Score += stats["Score"];
				pPlayer->m_Kills += stats["Kills"];
				pPlayer->m_Deaths += stats["Deaths"];
				pPlayer->m_TicksCaught += stats["TicksCaught"];
				pPlayer->m_TicksIngame += stats["TicksIngame"];
				pPlayer->m_TicksWarmup += stats["TicksWarmup"];
				pPlayer->m_Shots += stats["Shots"];
				pPlayer->m_Fails += stats["Fails"];
				pPlayer->m_Rank = stats.GetRank();
				pPlayer->m_IsRankFetched = true;
				break;				
			}

			HandleBeginnerServerCondition(pPlayer);	
		}

		// delete all entries.
		m_RankingRetrievalMessageQueue.clear();

		m_RankingRetrievalMessageQueueMutex.unlock();
	}
}

void CGameControllerZCATCH::AddLeavingPlayerIPToCaughtCache(int LeavingID)
{
	CPlayer* pPlayer = GameServer()->m_apPlayers[LeavingID];
	if (pPlayer)
	{
		int caughtByID = pPlayer->GetIDCaughtBy();
		if (caughtByID >= 0)
		{
			char aBuf[NETADDR_MAXSTRSIZE];
			Server()->GetClientAddr(LeavingID, aBuf, NETADDR_MAXSTRSIZE);
			
			// expires in 1 minute
			int expirationTick = Server()->Tick() + (Server()->TickSpeed() * 60);
			std::string IP = {aBuf};
			m_LeftCaughtCache.emplace_back(IP, caughtByID, expirationTick);
		}
	}
}

void CGameControllerZCATCH::RemovePlayerIDFromCaughtCache(int DyingOrLeavingID)
{
	if (m_LeftCaughtCache.size() == 0)
	{
		return;
	}

	// if a player leaves, his caught cache is cleared
	m_LeftCaughtCache.erase(std::remove_if(
			m_LeftCaughtCache.begin(), 
			m_LeftCaughtCache.end(), 
			[DyingOrLeavingID](std::tuple<std::string, int, int>& element)-> bool 
				{
					const auto& [addr, caughtByID, expirationTick] = element;
					(void) addr;
					(void) expirationTick;
					if (DyingOrLeavingID == caughtByID)
					{
						return true;
					}
					else
					{
						return false;
					}
				}), 
			m_LeftCaughtCache.end()
		);
}

void CGameControllerZCATCH::RemoveIPOfJoiningPlayerFromCaughtCache(std::string& IP)
{
	if (m_LeftCaughtCache.size() == 0)
	{
		return;
	}


	// remove element by IP of previously left player and now joining player
	m_LeftCaughtCache.erase(std::remove_if(
			m_LeftCaughtCache.begin(), 
			m_LeftCaughtCache.end(), 
			[IP](std::tuple<std::string, int, int>& element)-> bool 
				{
					const auto& [cachedIP, caughtByID, expirationTick] = element;
					if (cachedIP == IP)
					{
						(void)caughtByID;
						(void)expirationTick;
						return true;
					}
					else
					{
						return false;
					}
				}), 
			m_LeftCaughtCache.end()
		);
}

int CGameControllerZCATCH::IsInCaughtCache(int JoiningID)
{
	if (m_LeftCaughtCache.size() == 0)
	{
		return -1;
	}
		

	char aBuf[NETADDR_MAXSTRSIZE];
	Server()->GetClientAddr(JoiningID, aBuf, NETADDR_MAXSTRSIZE);

	std::string joiningIP = {aBuf};
	int currentTick = Server()->Tick();

	for (auto& [cachedIP, caughtByID, expirationTick] : m_LeftCaughtCache)
	{
		if (joiningIP == cachedIP && currentTick < expirationTick)
		{
			return caughtByID;
		}	
	}
	
	return -1;	
}

void CGameControllerZCATCH::CleanLeftCaughtCache()
{

	if (m_LeftCaughtCache.size() > 0)
	{
		int currentTick = Server()->Tick();

		// remove expired cache elements
		m_LeftCaughtCache.erase(std::remove_if(
			m_LeftCaughtCache.begin(), 
			m_LeftCaughtCache.end(), 
			[currentTick](std::tuple<std::string, int, int>& element)-> bool 
				{
					const auto& [cachedIP, caughtByID, expirationTick] = element;
					(void)cachedIP;	// silence warnings
					(void)caughtByID;
					if (currentTick >= expirationTick)
					{
						return true;
					}
					else
					{
						return false;
					}
				}), 
			m_LeftCaughtCache.end()
		);

		
		int moreThanLimit = (int) (m_LeftCaughtCache.size() - MAX_CLIENTS * 2);
		if (moreThanLimit > 0)
		{
			auto it = m_LeftCaughtCache.begin() + moreThanLimit;
			std::vector<std::tuple<std::string, int, int> > tmpVec = {it, m_LeftCaughtCache.end()};
			m_LeftCaughtCache = tmpVec;
			 
		}	
	}

}

void CGameControllerZCATCH::HandleLeavingPlayerCaching(int LeavingID)
{
	class CPlayer& Player = *GameServer()->m_apPlayers[LeavingID];

	if (Player.IsCaught())
	{
		// if player is caught, his ip will be added to the cache assiciated to the id
		AddLeavingPlayerIPToCaughtCache(LeavingID);
	}

	// player leaves -> nobody that previously left 
	// can be caught by that player 
	// on joining anymore
	RemovePlayerIDFromCaughtCache(LeavingID);
}

void CGameControllerZCATCH::HandleDyingPlayerCaching(int DyingPlayerID)
{
	RemovePlayerIDFromCaughtCache(DyingPlayerID);	
}


bool CGameControllerZCATCH::HandleJoiningPlayerCaching(int JoiningPlayerID)
{
	int CaughtByID  = IsInCaughtCache(JoiningPlayerID);
	if (CaughtByID >= 0)
	{
		class CPlayer* pKiller = GameServer()->m_apPlayers[CaughtByID];
		if (pKiller)
		{
			// be caught
			pKiller->CatchPlayer(JoiningPlayerID, CPlayer::REASON_PLAYER_JOINED);

			// retireve IP
			char aBuf[NETADDR_MAXSTRSIZE];
			Server()->GetClientAddr(JoiningPlayerID, aBuf, NETADDR_MAXSTRSIZE);
			std::string IP = {aBuf};

			// remove ip from cache
			RemoveIPOfJoiningPlayerFromCaughtCache(IP);
			return true;
		}
	}
	
	// bool : IsAlreadyCaught
	return false;
}


void CGameControllerZCATCH::SetKickIn(int ClientID, int Seconds)
{
	if (Seconds < 0)
	{
		m_PlayerKickTicksCountdown[ClientID] = NO_KICK;
		
	}
	else
	{
		m_PlayerKickTicksCountdown[ClientID] = Seconds * Server()->TickSpeed();
	}
}

void CGameControllerZCATCH::KickCountdownOnTick()
{
	if (g_Config.m_SvBeginnerServerRankLimit == 0 && g_Config.m_SvBeginnerServerScoreLimit == 0)
		return;
	
	for (int ID : GameServer()->PlayerIDs())
	{
		if (GameServer()->m_apPlayers[ID]->IsAuthed())
		{
			m_PlayerKickTicksCountdown[ID] = NO_KICK;
			continue;
		}
		else
		{
			if (m_PlayerKickTicksCountdown[ID] < 0)
			{
				continue;
			}	
			else if (m_PlayerKickTicksCountdown[ID] > 0)
			{
				CPlayer* pPlayer = GameServer()->m_apPlayers[ID];
				if (pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
				{
					m_PlayerKickTicksCountdown[ID]--;
				}
			}
			else if(m_PlayerKickTicksCountdown[ID] == 0)
			{
				AddKickedPlayerIPToCache(ID);
				Server()->Kick(ID, g_Config.m_SvBeginnerServerKickReason);
				m_PlayerKickTicksCountdown[ID] = NO_KICK;
			}
		}
	}	

	// clear cache daily
	if (m_KickedPlayersIPCache.size() > 0 && Server()->Tick() % (24 * 3600 * Server()->TickSpeed()) == 0)
	{
		m_KickedPlayersIPCache.clear();
	}
	
}

void CGameControllerZCATCH::HandleBeginnerServerCondition(CPlayer* pPlayer)
{
	if (!pPlayer)
		return;
	
	int ID = pPlayer->GetCID();
	bool EnableKickCountdown = false;

	if (g_Config.m_SvBeginnerServerScoreLimit)
	{
		int Score = pPlayer->m_Score;
		if (g_Config.m_SvBeginnerServerScoreLimit <= Score)
		{
			EnableKickCountdown = true;
		}
	}

	if (g_Config.m_SvBeginnerServerRankLimit)
	{
		int Rank = pPlayer->m_Rank;
		if (Rank <= g_Config.m_SvBeginnerServerRankLimit)
		{
			EnableKickCountdown = true;
		}
	}

	if (CheckIPInKickedBeginnerServerCache(ID))
	{
		EnableKickCountdown = true;
	}
	

	if (EnableKickCountdown)
	{
		SetKickIn(ID, g_Config.m_SvBeginnerServerKickTimeLimit);
		AddKickedPlayerIPToCache(ID);
		// move to spec.
		pPlayer->BeSetFree(CPlayer::REASON_NONE);
		DoTeamChange(pPlayer, TEAM_SPECTATORS, false);
	}
	
}

void CGameControllerZCATCH::AddKickedPlayerIPToCache(int ClientID)
{
	// basically a ringbuffer of strings
	constexpr unsigned int CacheSize = 512;

	char aBuf[NETADDR_MAXSTRSIZE];
	Server()->GetClientAddr(ClientID, aBuf, NETADDR_MAXSTRSIZE);
	std::string IPToKick = {aBuf};

	if (m_KickedPlayersIPCache.size() > 0)
	{
		for (std::string& IP : m_KickedPlayersIPCache)
		{
			if (IP == IPToKick)
			{
				return;
			}
		}
	}
	
	// ip not in cache, add it
	if (m_KickedPlayersIPCache.size() >= CacheSize)
	{
		m_KickedPlayersIPCache.pop_front();
		m_KickedPlayersIPCache.push_back(IPToKick);
	}
	else
	{
		m_KickedPlayersIPCache.push_back(IPToKick);
	}
}

bool CGameControllerZCATCH::CheckIPInKickedBeginnerServerCache(int ClientID)
{
	if (m_KickedPlayersIPCache.size() == 0)
		return false;
	
	char aBuf[NETADDR_MAXSTRSIZE];
	Server()->GetClientAddr(ClientID, aBuf, NETADDR_MAXSTRSIZE);
	std::string CheckingIP = {aBuf};

	for (std::string& IP : m_KickedPlayersIPCache)
	{
		if (IP == CheckingIP)
		{
			return true;
		}	
	}

	return false;
	
}



	







