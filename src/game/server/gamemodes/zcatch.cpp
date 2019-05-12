/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime                                                                 */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include "zcatch.h"

#include <cmath>
#include <cstring>
#include <future>
#include <string>
#include <sstream>
#include <iomanip>


/**
 * Global variable to be accessible from static and non-static context.
 */
int m_OldMode;

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) :
		IGameController(pGameServer)
{
	m_pGameType = "zCatch";
	m_OldMode = g_Config.m_SvMode;
}

CGameController_zCatch::~CGameController_zCatch() {
	/* save all players */
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (GameServer()->m_apPlayers[i])
			SaveRanking(GameServer()->m_apPlayers[i]);
}

/* ranking system: create zcatch score table */
void CGameController_zCatch::OnInitRanking(sqlite3 *rankingDb) {
	for (int i = 1; i <= 5; ++i)
	{

		char *zErrMsg = 0;

		/* lock database access in this process */
		GameServer()->LockRankingDb();

		/* when another process uses the database, wait up to 10 seconds */
		sqlite3_busy_timeout(GameServer()->GetRankingDb(), 10000);


		char aMode[16];
		str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(i).c_str()) ;
		char aQuery[8192];
		str_format(aQuery, sizeof(aQuery), "\
			BEGIN; \
			CREATE TABLE IF NOT EXISTS %s ( \
				username TEXT PRIMARY KEY, \
				score UNSIGNED INTEGER DEFAULT 0, \
				numWins UNSIGNED INTEGER DEFAULT 0, \
				numKills UNSIGNED INTEGER DEFAULT 0, \
				numKillsWallshot UNSIGNED INTEGER DEFAULT 0, \
				numDeaths UNSIGNED INTEGER DEFAULT 0, \
				numShots UNSIGNED INTEGER DEFAULT 0, \
				highestSpree UNSIGNED INTEGER DEFAULT 0, \
				timePlayed UNSIGNED INTEGER DEFAULT 0 \
			); \
			CREATE INDEX IF NOT EXISTS %s_score_index ON %s (score); \
			CREATE INDEX IF NOT EXISTS %s_numWins_index ON %s (numWins); \
			CREATE INDEX IF NOT EXISTS %s_numKills_index ON %s (numKills); \
			CREATE INDEX IF NOT EXISTS %s_numKillsWallshot_index ON %s (numKillsWallshot); \
			CREATE INDEX IF NOT EXISTS %s_numDeaths_index ON %s (numDeaths); \
			CREATE INDEX IF NOT EXISTS %s_numShots_index ON %s (numShots); \
			CREATE INDEX IF NOT EXISTS %s_highestSpree_index ON %s (highestSpree); \
			CREATE INDEX IF NOT EXISTS %s_timePlayed_index ON %s (timePlayed); \
			\
			CREATE VIEW IF NOT EXISTS %s_top_score_View (username, score, numWins) \
			AS \
			SELECT username, score, numWins FROM %s \
			WHERE score > 0 \
			ORDER BY score DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_numWins_View (username, numWins) \
			AS \
			SELECT username, numWins FROM %s \
			WHERE numWins > 0 \
			ORDER BY numKills DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_kd_View (username, kd) \
			AS \
			SELECT username, (t.numKills + t.numKillsWallshot)/(t.numDeaths) as kd FROM %s as t \
			WHERE kd > 0 \
			ORDER BY kd DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_numKills_View (username, numKills) \
			AS \
			SELECT username, numKills FROM %s \
			WHERE numKills > 0 \
			ORDER BY numKills DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_numKillsWallShot_View (username, numKillsWallShot) \
			AS \
			SELECT username, numKillsWallShot FROM %s \
			WHERE numKillsWallshot > 0 \
			ORDER BY numKillsWallShot DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_timePlayed_View (username, timePlayed) \
			AS \
			SELECT username, timePlayed FROM %s \
			WHERE timePlayed > 0 \
			ORDER BY timePlayed DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_numShots_View (username, numShots) \
			AS \
			SELECT username, numShots FROM %s \
			WHERE numShots > 0 \
			ORDER BY numShots DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_numDeaths_View (username, numDeaths) \
			AS \
			SELECT username, numDeaths FROM %s \
			WHERE numDeaths > 0 \
			ORDER BY numDeaths DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_top_highestSpree_View (username, highestSpree) \
			AS \
			SELECT username, highestSpree FROM %s \
			WHERE highestSpree > 0 \
			ORDER BY numKillsWallShot DESC LIMIT 5; \
			\
			CREATE VIEW IF NOT EXISTS %s_avg_statistics_View \
			AS \
			SELECT  \
				ROUND(sum(t.score) * 1.0 / count(*), 4) 													AS score_avg,  \
				ROUND(sum(t.numWins) * 1.0 / count(*), 4) 													AS numWins_avg, \
				ROUND(sum(t.numKills) * 1.0 / count(*), 4) 													AS numKills_avg, \
				ROUND(sum(t.numDeaths) * 1.0 / count(*), 4) - ROUND(sum(t.numKills) * 1.0 / count(*), 4) 	AS numSuddenDeaths_avg, \
				ROUND(sum(t.numKillsWallshot) * 1.0 / count(*), 4) 											AS numKillsWallshot_avg, \
				ROUND(sum(t.numShots) * 1.0 / count(*), 4) 													AS numShots_avg, \
				ROUND(sum(t.timePlayed) * 1.0 / count(*), 4) 												AS timePlayed_avg, \
				ROUND(sum(t.highestSpree) * 1.0 / count(*), 4) 												AS highestSpree_avg \
			FROM %s as t \
				WHERE t.timePlayed >= (5 * 60); \
			\
			CREATE VIEW IF NOT EXISTS %s_total_statistics_View \
			AS \
			SELECT  \
    			sum(t.score)            				AS score_total,  \
    			sum(t.numWins)          				AS numWins_total, \
    			sum(t.numKills)         				AS numKills_total, \
    			sum(t.numDeaths) - sum(t.numKills)      AS numSuddenDeaths_total, \
    			sum(t.numKillsWallshot) 				AS numKillsWallshot_total, \
    			sum(t.numShots)         				AS numShots_total, \
    			sum(t.timePlayed)       				AS timePlayed_total \
			FROM %s as t \
    			WHERE t.timePlayed >= (5 * 60); \
			COMMIT; \
		", aMode, aMode, aMode, aMode, aMode, 
		aMode, aMode, aMode, aMode, aMode,
		aMode, aMode, aMode, aMode, aMode, 
		aMode, aMode, aMode, aMode, aMode, 
		aMode, aMode, aMode, aMode, aMode,
		aMode, aMode, aMode, aMode, aMode,
		aMode, aMode, aMode, aMode, aMode,
		aMode, aMode, aMode, aMode);

		int rc = sqlite3_exec(GameServer()->GetRankingDb(), aQuery, NULL, 0, &zErrMsg);

		/* unlock database access */
		GameServer()->UnlockRankingDb();

		/* check for error */
		if (rc != SQLITE_OK) {
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s\n", rc, zErrMsg);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			sqlite3_free(zErrMsg);
			exit(1);
		}

	}
}


void CGameController_zCatch::Tick()
{
	IGameController::Tick();

	if(m_OldMode != g_Config.m_SvMode && !GameServer()->m_World.m_Paused)
	{
		EndRound();
	}
	
}

void CGameController_zCatch::DoWincheck()
{
	if(m_GameOverTick == -1)
	{
		int Players = 0, Players_Spec = 0, Players_SpecExplicit = 0;
		int winnerId = -1;
		CPlayer *winner = NULL;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
			{
				Players++;
				if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					Players_Spec++;
				else
				{
					winnerId = i;
					winner = GameServer()->m_apPlayers[i];
				}
				if(GameServer()->m_apPlayers[i]->m_SpecExplicit)
					Players_SpecExplicit++;
			}
		}
		int Players_Ingame = Players - Players_SpecExplicit;

		if(Players_Ingame <= 1)
		{
			//Do nothing
		}
		else if((Players - Players_Spec) == 1)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;
					if(Players_Ingame < g_Config.m_SvLastStandingPlayers)
						GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				}
			}
			if(winner && winner->m_HardMode.m_Active && winner->m_HardMode.m_ModeTotalFails.m_Active && winner->m_HardMode.m_ModeTotalFails.m_Fails > winner->m_HardMode.m_ModeTotalFails.m_Max)
			{
				winner->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				winner->m_HardMode.m_ModeTotalFails.m_Fails = 0;
				GameServer()->SendChatTarget(-1, "The winner failed the hard mode.");
				GameServer()->SendBroadcast("The winner failed the hard mode.", -1);
			}
			else if(Players_Ingame < g_Config.m_SvLastStandingPlayers)
			{
				winner->HardModeRestart();
				GameServer()->SendChatTarget(-1, "Too few players to end round.");
				GameServer()->SendBroadcast("Too few players to end round.", -1);
			}
			else
			{
				
				// give the winner points
				if (winnerId > -1)
				{
					RewardWinner(winnerId);
				}
				
				// announce if winner is in hard mode
				if(winner->m_HardMode.m_Active) {
					char aBuf[256];
					auto name = GameServer()->Server()->ClientName(winnerId);
					str_format(aBuf, sizeof(aBuf), "Player '%s' won in hard mode (%s).", name, winner->m_HardMode.m_Description);
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				}
				
				EndRound();
			}
		}

		IGameController::DoWincheck(); //do also usual wincheck
	}
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	/**
	 * we do not want to try to clean the futures every tick,
	 * but at some specific occasions like at the end of a round
	 * or if a player dies.
	 */
	GameServer()->CleanFutures();


	if(!pKiller)
		return 0;

	CPlayer *victim = pVictim->GetPlayer();
	if(pKiller != victim)
	{
		/* count players playing */
		int numPlayers = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->m_SpecExplicit)
				++numPlayers;

		/*
		 * Previously you got not one score per kill, 
		 * but score based on the enemies your victim has caught:
		 * 
		 * pKiller->m_Score += min(victim->m_zCatchNumKillsInARow + 1, numPlayers);
		 */	
		
		// Now you get one score per enemy killed.
		pKiller->m_Score += 1;	
		pKiller->m_Kills += 1;
		victim->m_Deaths += 1;
		/* Check if the killer has been already killed and is in spectator (victim may died through wallshot) */
		if(pKiller->GetTeam() != TEAM_SPECTATORS && (!pVictim->m_KillerLastDieTickBeforceFiring || pVictim->m_KillerLastDieTickBeforceFiring == pKiller->m_DieTick))
		{
			++pKiller->m_zCatchNumKillsInARow;
			pKiller->AddZCatchVictim(victim->GetCID(), CPlayer::ZCATCH_CAUGHT_REASON_KILLED);
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You are caught until '%s' dies.", Server()->ClientName(pKiller->GetCID()));
			GameServer()->SendChatTarget(victim->GetCID(), aBuf);
		}
	}
	else
	{
		// selfkill/death
		if(WeaponID == WEAPON_SELF || WeaponID == WEAPON_WORLD)
		{
			victim->m_Score -= g_Config.m_SvKillPenalty;
			++victim->m_Deaths;
		}
	}

	// release all the victim's victims
	victim->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
	victim->m_zCatchNumKillsInARow = 0;
	victim->m_zCatchNumKillsReleased = 0;

	// Update colours
	OnPlayerInfoChange(victim);
	OnPlayerInfoChange(pKiller);
	
	// ranking
	++victim->m_RankCache.m_NumDeaths;
	if(pKiller != victim && WeaponID != WEAPON_GAME)
	{
		++pKiller->m_RankCache.m_NumKills;
		if (WeaponID == WEAPON_RIFLE && pVictim->m_TookBouncedWallshotDamage)
		{
			++pKiller->m_RankCache.m_NumKillsWallshot;
		}
	}

	// zCatch/TeeVi: hard mode
	if(pKiller->m_HardMode.m_Active && pKiller->m_HardMode.m_ModeKillTimelimit.m_Active)
	{
		pKiller->m_HardMode.m_ModeKillTimelimit.m_LastKillTick = Server()->Tick();
	}

	return 0;
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{
	if(g_Config.m_SvColorIndicator && pP->m_zCatchNumKillsInARow <= 20)
	{
		int Num = max(0, 160 - pP->m_zCatchNumKillsInARow * 10);
		pP->m_TeeInfos.m_ColorBody = Num * 0x010000 + 0xff00;
		if(pP->m_HardMode.m_Active)
			pP->m_TeeInfos.m_ColorFeet = 0xffff00; // red
		else
			pP->m_TeeInfos.m_ColorFeet = pP->m_zCatchNumKillsInARow == 20 ? 0x40ff00 : pP->m_TeeInfos.m_ColorBody;
		pP->m_TeeInfos.m_UseCustomColor = 1;
	}
}

void CGameController_zCatch::StartRound()
{
	
	// if sv_mode changed: restart map (with new mode then)
	if(m_OldMode != g_Config.m_SvMode)
	{
		m_OldMode = g_Config.m_SvMode;
		Server()->MapReload();
	}
	
	IGameController::StartRound();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
			GameServer()->m_apPlayers[i]->RankCacheStartPlaying();
		}
	}
}

void CGameController_zCatch::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health and armor
	pChr->IncreaseHealth(10);
	if(g_Config.m_SvMode == 2)
		pChr->IncreaseArmor(10);

	// give default weapons
	switch(g_Config.m_SvMode)
	{
	case 1: /* Instagib - Only Riffle */
		pChr->GiveWeapon(WEAPON_RIFLE, -1);
		break;
	case 2: /* All Weapons */
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		pChr->GiveWeapon(WEAPON_GUN, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_SHOTGUN, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_RIFLE, g_Config.m_SvWeaponsAmmo);
		break;
	case 3: /* Hammer */
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		break;
	case 4: /* Grenade */
		pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvGrenadeEndlessAmmo ? -1 : g_Config.m_SvWeaponsAmmo);
		break;
	case 5: /* Ninja */
		pChr->GiveNinja();
		break;
	}

	//Update colour of spawning tees
	OnPlayerInfoChange(pChr->GetPlayer());
}

void CGameController_zCatch::EndRound()
{
	/**
	 * we do not want to try to clean the futures every tick,
	 * but at some specific occasions like at the end of a round
	 * or if a player dies.
	 */
	GameServer()->CleanFutures();

	if(m_Warmup) // game can't end when we are running warmup
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		auto player = GameServer()->m_apPlayers[i];
		if(player)
		{
			
			// save ranking stats
			SaveRanking(player);
			player->RankCacheStopPlaying();

			if(!player->m_SpecExplicit)
			{
				player->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));

				if(player->m_TicksSpec != 0 || player->m_TicksIngame != 0)
				{
					char aBuf[128];
					double TimeIngame = (player->m_TicksIngame * 100.0) / (player->m_TicksIngame + player->m_TicksSpec);
					str_format(aBuf, sizeof(aBuf), "K/D: %d/%d, ingame: %.2f%%", player->m_Kills, player->m_Deaths, TimeIngame);
					GameServer()->SendChatTarget(i, aBuf);
				}
				// release all players
				player->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				player->m_zCatchNumKillsInARow = 0;
			}
			
			// zCatch/TeeVi: hard mode
			// reset hard mode
			player->ResetHardMode();
			
		}
	}

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;
}

bool CGameController_zCatch::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	if(pPlayer->m_CaughtBy >= 0)
		return false;
	return true;
}

bool CGameController_zCatch::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_SPAWN)
		m_aaSpawnPoints[0][m_aNumSpawnPoints[0]++] = Pos;
	else if(Index == ENTITY_SPAWN_RED)
		m_aaSpawnPoints[1][m_aNumSpawnPoints[1]++] = Pos;
	else if(Index == ENTITY_SPAWN_BLUE)
		m_aaSpawnPoints[2][m_aNumSpawnPoints[2]++] = Pos;

	return false;
}

/* celebration and scoring */
void CGameController_zCatch::RewardWinner(int winnerId) {
	
	CPlayer *winner = GameServer()->m_apPlayers[winnerId];
	int numEnemies = min(MAX_CLIENTS - 1, winner->m_zCatchNumKillsInARow - winner->m_zCatchNumKillsReleased);
	
	int points = enemiesKilledToPoints(numEnemies);
	
	/* set winner's ranking stats */
	++winner->m_RankCache.m_NumWins;

	/* the winner's name */
	const char *name = GameServer()->Server()->ClientName(winnerId);

	/* abort if no points */
	if (points <= 0)
	{
		/* announce in chat */
		char aBuf[96];
		str_format(aBuf, sizeof(aBuf), "Winner '%s' doesn't get any points.", name);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		return;
	}
	winner->m_RankCache.m_Points += points;
	
	/* saving is done in EndRound() */
	
	/* announce in chat */
	char aBuf[96];
	str_format(aBuf, sizeof(aBuf), "Winner '%s' gets %.2f points.", name, points/100.0f);
	GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	
}

/* save a player's ranking stats */
void CGameController_zCatch::SaveRanking(CPlayer *player)
{

	if (!GameServer()->RankingEnabled())
		return;

	/* prepare */
	player->RankCacheStopPlaying(); // so that m_RankCache.m_TimePlayed is updated

	/* check if saving is needed */
	/* because compared to simply the & operator, the && operator does not conpinue to check all the conditions
	of the if statement if one of them does not meet the criteria, so the order of the conditions decides how fast
	those are checked
	I wonder if the numKillsWallshot is actually needed due to every wallshot being a shot, meaning that the wallshot
	doesn't need to be checked*/
	if (player->m_RankCache.m_NumShots == 0 &&
	        player->m_RankCache.m_TimePlayed == 0 &&
	        player->m_RankCache.m_NumKills == 0 &&
	        player->m_RankCache.m_NumDeaths == 0 &&
	        player->m_zCatchNumKillsInARow == 0 &&
	        player->m_RankCache.m_NumWins == 0 &&
	        player->m_RankCache.m_Points == 0 &&
	        player->m_RankCache.m_NumKillsWallshot == 0)
		return;

	/* player's name */
	char *name = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(name, GameServer()->Server()->ClientName(player->GetCID()), MAX_NAME_LENGTH);

	/* give the points */
	GameServer()->AddFuture(std::async(std::launch::async, &CGameController_zCatch::SaveScore,
	                                   GameServer(), // gamecontext
	                                   name, // username
	                                   player->m_RankCache.m_Points, // score
	                                   player->m_RankCache.m_NumWins, // numWins
	                                   player->m_RankCache.m_NumKills, // numKills
	                                   player->m_RankCache.m_NumKillsWallshot, // numKillsWallshot
	                                   player->m_RankCache.m_NumDeaths, // numDeaths
	                                   player->m_RankCache.m_NumShots, // numShots
	                                   player->m_zCatchNumKillsInARow, // highestSpree
	                                   player->m_RankCache.m_TimePlayed / Server()->TickSpeed(), // timePlayed
	                                   0,
	                                   0));


	/* clean rank cache */
	player->m_RankCache.m_Points = 0;
	player->m_RankCache.m_NumWins = 0;
	player->m_RankCache.m_NumKills = 0;
	player->m_RankCache.m_NumKillsWallshot = 0;
	player->m_RankCache.m_NumDeaths = 0;
	player->m_RankCache.m_NumShots = 0;
	player->m_RankCache.m_TimePlayed = 0;
	player->RankCacheStartPlaying();
}


/**
 * @brief Saves given score for given unique name to the database.
 * @details Saves given score for given unique name to the database.
 *
 * @param GameServer CGameContext Object needed for most game related information.
 * @param name This is the unique primary key which identifies a ranking record.
 * 			   Because this function is executed within a thread, the free(name) is used at the end of this function.
 * @param score Score which is calculated with a specific and deterministic algorithm.
 * @param numWins Number of wins
 * @param numKills Number of kills
 * @param numKillsWallshot Number of wallshot kills
 * @param numDeaths Number of deaths
 * @param numShots Number of shots
 * @param highestSpree highest continuous killing spree.
 * @param timePlayed Time played on this server.
 * @param Free - whether to free(.) the *name
 */
void CGameController_zCatch::SaveScore(CGameContext* GameServer, char *name, int score, int numWins, int numKills, int numKillsWallshot, int numDeaths, int numShots, int highestSpree, int timePlayed, int GameMode, int Free) {
	// Don't save connecting players
	if (str_comp(name, "(connecting)") == 0 || str_comp(name, "(invalid)") == 0)
	{
		if (!Free)
		{
			free(name);
		}
		return;
	}

	/* debug */
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "Saving user stats of '%s' in mode: %s", name, GetGameModeTableName(GameMode).c_str());
	GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);

	/* prepare */
	const char *zTail;

	char aMode[16];
	str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(GameMode).c_str()) ;

	char aQuery[1024];
	str_format(aQuery, sizeof(aQuery), "\
		INSERT OR REPLACE INTO %s ( \
			username, score, numWins, numKills, numKillsWallshot, numDeaths, numShots, highestSpree, timePlayed \
		) \
		SELECT \
			new.username, \
			COALESCE(old.score, 0) + ?2, \
			COALESCE(old.numWins, 0) + ?3, \
			COALESCE(old.numKills, 0) + ?4, \
			COALESCE(old.numKillsWallshot, 0) + ?5, \
			COALESCE(old.numDeaths, 0) + ?6, \
			COALESCE(old.numShots, 0) + ?7, \
			MAX(COALESCE(old.highestSpree, 0), ?8), \
			COALESCE(old.timePlayed, 0) + ?9 \
		FROM ( \
			SELECT trim(?1) as username \
		) new \
		LEFT JOIN ( \
			SELECT * \
			FROM %s \
		) old ON old.username = new.username; \
		", aMode, aMode);

	sqlite3_stmt *pStmt = 0;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, std::strlen(aQuery), &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, name, std::strlen(name), 0);
		sqlite3_bind_int(pStmt, 2, score);
		sqlite3_bind_int(pStmt, 3, numWins);
		sqlite3_bind_int(pStmt, 4, numKills);
		sqlite3_bind_int(pStmt, 5, numKillsWallshot);
		sqlite3_bind_int(pStmt, 6, numDeaths);
		sqlite3_bind_int(pStmt, 7, numShots);
		sqlite3_bind_int(pStmt, 8, highestSpree);
		sqlite3_bind_int(pStmt, 9, timePlayed);

		/* lock database access in this process */
		GameServer->LockRankingDb();

		/* when another process uses the database, wait up to 1 minute */
		sqlite3_busy_timeout(GameServer->GetRankingDb(), 60000);

		/* save to database */
		switch (sqlite3_step(pStmt))
		{
		case SQLITE_DONE:
			/* nothing */
			break;
		case SQLITE_BUSY:
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "Error: could not save records (timeout).");
			break;
		default:
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		}

		/* unlock database access */
		GameServer->UnlockRankingDb();
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);

	if (!Free) {
		free(name);
	}

}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandTop(CPlayer *pPlayer, const char *category)
{
	const char *column;
	/**
	 * Compare passed category. if we have such a category,
	 * assign a column for that specific category.
	 * that column is then used to build a SQLite View name, which
	 * contains 5 entries of the top players.
	 */
	if(!str_comp_nocase("", category))
	{
		column = "score";
		return;
	}
	else if (!str_comp_nocase("score", category))
	{
		column = "score";
	}
	else if (!str_comp_nocase("wins", category))
	{
		column = "numWins";
	}
	else if (!str_comp_nocase("kills", category))
	{
		column = "numKills";
	}
	else if (!str_comp_nocase("wallshotkills", category))
	{
		column = "numKillsWallshot";
	}
	else if (!str_comp_nocase("deaths", category))
	{
		column = "numDeaths";
	}
	else if (!str_comp_nocase("shots", category))
	{
		column = "numShots";
	}
	else if (!str_comp_nocase("spree", category))
	{
		column = "highestSpree";
	}
	else if (!str_comp_nocase("time", category))
	{
		column = "timePlayed";
	} else if (!str_comp_nocase("kd", category)) {
		column = "kd";
	}
	else
	{
		GameServer()->SendChatTarget(pPlayer->GetCID(), "Usage: /top [score|wins|kills|wallshotkills|deaths|kd|shots|spree|time]");
		return;
	}

	// send data to the specific player.
	GameServer()->AddFuture(std::async(std::launch::async, 
										&CGameController_zCatch::ChatCommandTopFetchDataAndPrint, 
										GameServer(), 
										pPlayer->GetCID(), 
										column, category));
}

/* get the top players */
void CGameController_zCatch::ChatCommandTopFetchDataAndPrint(CGameContext* GameServer, int clientId, const char *column_str, const char* title_str)
{
	std::string column(column_str);
	std::string title(title_str);
	std::stringstream s;

	bool is_score_ranking = column == "score" || column == "";

	/* prepare */
	const char *zTail;
	char sqlBuf[128];

	str_format(sqlBuf, sizeof(sqlBuf), "SELECT * FROM %s_top_%s_View LIMIT 5;", GetGameModeTableName(0).c_str(), column.c_str());
	const char *zSql = sqlBuf;
	sqlite3_stmt *pStmt = 0;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, std::strlen(zSql), &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{

		/* lock database access in this process, but wait maximum 1 second */
		if (GameServer->LockRankingDb(1000))
		{

			/* when another process uses the database, wait up to 1 second */
			sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

			/* fetch from database */
			int numRows = 0;
			int rc;
			int rank	= 1;

			if (is_score_ranking)
			{
				s << "╔══════════════════   °• ♔ •°   ══════════════════╗" << "\n";
			}
			else
			{
				s << "╔═════════   °• ♔ •°   ═════════╗" << "\n";
			}

			s << "║ Top 5: " << (title == "" ? "" : title) << "\n";
			s << "║ \n";
			

			while ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
			{
				const unsigned char* name	= sqlite3_column_text(pStmt, 0);
				unsigned int value 			= sqlite3_column_int( pStmt, 1);

				// don't show in top if no score available.
				if (value == 0) {
					continue;
				}

				s << "║ " << rank++ << ". ";
				s << "[";

				if (is_score_ranking)
				{
					s << std::fixed;
					if (value % 100){	s << std::fixed << std::setprecision(2);	}
					else { 				s << std::fixed << std::setprecision(0);	}
					s << value / 100.0;

				} else if(column == "timePlayed")
				{
					s << value / 3600;
					s << ":"<< std::setw(2) << std::setfill('0');
					s << value/60 % 60;
					s << "h";
				} else
				{
					s << value;
				}
					
				s << "] " << name << " ";

				if (is_score_ranking)
				{
					unsigned int numWins 	= sqlite3_column_int( pStmt, 2);
					if (numWins > 0)
					{
						double scorePerWin 		= static_cast<double>(value / (100.0 * numWins)) ;
						double spreePerWin		= pointsToEnemiesKilled(value / numWins);

						s << "Score/Win: " << std::fixed << std::setprecision(2) << scorePerWin << " ";
						s << "Spree/Win: " << std::fixed << std::setprecision(0) << spreePerWin << " ";
					}
				}
				
				s << "\n";
				
				++numRows;
			}

			/* unlock database access */
			GameServer->UnlockRankingDb();

			s << "║ \n";
			s << "║ Requested by '" << GameServer->Server()->ClientName(clientId) << "'\n";
			if (is_score_ranking)
			{
				s << "╚══════════════════   °• ♔ •°   ══════════════════╝" << "\n";
			}
			else
			{
				s << "╚═════════   °• ♔ •°   ═════════╝" << "\n";
			}
			

			if (numRows == 0)
			{
				if (rc == SQLITE_BUSY)
				{
					GameServer->SendChatTarget(clientId, "Could not load top ranks. Try again later.");
				}
				else
				{
					GameServer->SendChatTarget(clientId, "There are no ranks");
				}
			}
			else
			{
				std::string result = s.str();
				SendLines(GameServer, result);
			}
		}
		else
		{
			GameServer->SendChatTarget(clientId, "Could not load top ranks. Try again later.");
		}
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);
}

/* when a player typed /top into the chat */
void CGameController_zCatch::OnChatCommandOwnRank(CPlayer *pPlayer)
{
	/**
	 * uses the more generalized fuction that is able to retrieve the rank of any player.
	 */
	OnChatCommandRank(pPlayer, GameServer()->Server()->ClientName(pPlayer->GetCID()));
}

/* when a player typed /rank nickname into the chat */
void CGameController_zCatch::OnChatCommandRank(CPlayer *pPlayer, const char *name, bool sendToEveryone)
{
	char *queryName = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(queryName, name, MAX_NAME_LENGTH);

	// executes function with the parameters given afterwards
	
	/**
	 * A future is something like a thread that can be executed on a different cpu core in parallel.
	 * The execution of a future depends on the C++ STL implementation, but in general a future is expected to 
	 * be executed in the future and return its result. Has the future not been executed yet, when you try to
	 * retrieve the result, the future is more or less forced to be executed due to the retrieval.
	 * There are exceptions to this behaviour, where you only check if the future did aready return a result, but
	 * don't invoke the future's execution.
	 */
	GameServer()->AddFuture(std::async(std::launch::async, 
										&CGameController_zCatch::ChatCommandRankFetchDataAndPrint, 
										GameServer(), 
										pPlayer->GetCID(), 
										queryName,
										sendToEveryone));
}

void CGameController_zCatch::ChatCommandStatsFetchDataAndPrint(CGameContext* GameServer, int ClientID, const char* cmd)
{
    std::string cmdName(cmd);
    std::string mode;
    bool toEveryone = true;


	// stringstream buffer to concatenate the string.
    std::stringstream s;
    
    if (cmdName == "avg" ||
            cmdName == "average")
    {
        mode = "avg";
    }
    else if (cmdName == "total")
    {
        mode = "total";
    }
    else
    {
    	GameServer->SendChatTarget(ClientID, "Usage: /stats [average|total]");
        return;
    }

    std::string gamemode = GetGameModeTableName(0);

    /* prepare */
    const char* zTail;
    char sqlBuf[128];

    str_format(sqlBuf, sizeof(sqlBuf), "SELECT * FROM %s_%s_statistics_View LIMIT 1;", gamemode.c_str(), mode.c_str());
    const char* zSql = sqlBuf;
    sqlite3_stmt* pStmt = 0;
    int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), zSql, std::strlen(zSql), &pStmt, &zTail);

    if (rc == SQLITE_OK)
    {

        /* lock database access in this process, but wait maximum 1 second */
        if (GameServer->LockRankingDb(1000))
        {

            /* when another process uses the database, wait up to 1 second */
            sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

            /* fetch from database */
            int numRows = 0;
            int rc;


            if ((rc = sqlite3_step(pStmt)) == SQLITE_ROW)
            {

                double score_stat 				= 0.0;
                double numWins_stat 			= 0.0;
                double numKills_stat 			= 0.0;
                double numSuddenDeaths_stat 	= 0.0;
                double numKillsWallshot_stat	= 0.0;
                double numShots_stat 			= 0.0;
                double timePlayed_stat 			= 0.0;

                /**
                 * only avalable in average statistics
                 */
                double highestSpree_avg 		= 0.0;


                score_stat 				= sqlite3_column_double(pStmt, 0);
                numWins_stat 			= sqlite3_column_double(pStmt, 1);
                numKills_stat 			= sqlite3_column_double(pStmt, 2);
                numSuddenDeaths_stat 	= sqlite3_column_double(pStmt, 3);
                numKillsWallshot_stat 	= sqlite3_column_double(pStmt, 4);
                numShots_stat 			= sqlite3_column_double(pStmt, 5);
                timePlayed_stat 		= sqlite3_column_double(pStmt, 6);

                if (mode == "avg")
                {
                    highestSpree_avg = sqlite3_column_double(pStmt, 7);
                }

                /**
                 * description to be printed for every value
                 */
                std::string desc;
                if (mode == "avg")
                {
                    desc = "╔—— Average per player statistics ——╗";
                }
                else
                {
                    desc = "╔———  Total sum of all players  ———╗";
                }

                /**
                 * create a formatted string with newlines
                 */
                int hours = static_cast<int>(timePlayed_stat) / 3600;
                int seconds = (static_cast<int>(timePlayed_stat) / 60) % 60;

                s << desc << "\n";
                s << "║  Score: " 							<< (score_stat / 100.0)	<< "\n"
                    << "║  Number of Wins: " 				<< numWins_stat			<< "\n"
                    << "║  Number of Kills: " 				<< numKills_stat 		<< "\n";

    			if (gamemode == "Laser" || gamemode == "Everything")
                {
                    s << "║  Number of Wallshot Kills: " 	<< numKillsWallshot_stat << "\n";
                }

                s << "║  Number of Sudden Deaths: " 		<< numSuddenDeaths_stat	<< "\n"
                    << "║  Number of Shots: " 				<< numShots_stat		<< "\n";

                if (mode == "avg")
                {
                    s << "║  Highest Spree: "				<< highestSpree_avg	<< "\n";
                }

                s << "║  Time played: " 		<< hours << ":" << (seconds < 10 ? "0" : "") << seconds << "h" << "\n";
                s << "║  Requested by '" << GameServer->Server()->ClientName(ClientID) << "'\n";
                s << "╚———————————————————╝" << "\n";

                ++numRows;
            }

            /* unlock database access */
            GameServer->UnlockRankingDb();

            if (numRows == 0)
            {
                if (rc == SQLITE_BUSY)
                {
                	s << "Could not load statistics. Try again later.\n";
                    toEveryone = false;
                }
                else
                {
                	s << "There are no statistics available.\n";
                    toEveryone = false;
                }
            }
        }
        else
        {
        	s << "Could not load statistics, because the database is being used.\n";
            toEveryone = false;
        }
    }
    else
    {
        /* print error */
        char aBuf[512];
        str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
        GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "statistics", aBuf);
        s << "An internal database error occurred, could not retrieve the statistics.\n";
    }

    // create string
	std::string result_string(s.str());

	if (toEveryone)
	{
		SendLines(GameServer, result_string);
	} else {
		SendLines(GameServer, result_string, ClientID);
	}

    sqlite3_finalize(pStmt);
}


void CGameController_zCatch::OnChatCommandStats(CPlayer *pPlayer, const char *cmdName)
{
	GameServer()->AddFuture(std::async(std::launch::async, 
										&CGameController_zCatch::ChatCommandStatsFetchDataAndPrint, 
										GameServer(), 
										pPlayer->GetCID(), 
										cmdName));
}

/**
 * @brief Fetch ranking records of player "name" and send to requesting player with ID clientId
 * @details Fetch ranking records of player "name" and send to requesting player with ID clientId
 *
 * @param GameServer CGameContext is needed in order to fetch most player info, e.g. ID, name, etc.
 * @param clientId Requesting player's clientId
 * @param name Unique name of the player whose rank is being requested by the player "clientId"
 */
void CGameController_zCatch::ChatCommandRankFetchDataAndPrint(CGameContext* GameServer, int clientId, char *name, bool sendToEveryone)
{

	std::string rankedName(name);
	std::string requestingName(GameServer->Server()->ClientName(clientId));

	std::stringstream s;
	std::string lines;
    std::string gamemode = GetGameModeTableName(0);
    bool toEveryone = sendToEveryone;

	/* prepare */
	const char *zTail;

	char aMode[16];
	str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(0).c_str()) ;

	char aQuery[512];
	str_format(aQuery, sizeof(aQuery), "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numKillsWallshot, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed, \
			(SELECT COUNT(*) FROM %s b WHERE b.score > a.score) + 1, \
			MAX(0, (SELECT MIN(b.score) FROM %s b WHERE b.score > a.score) - a.score) \
		FROM %s a \
		WHERE username = ?1 \
		;", aMode, aMode, aMode);

	sqlite3_stmt *pStmt = 0;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, std::strlen(aQuery), &pStmt, &zTail);

	if (rc == SQLITE_OK)
	{
		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, name, std::strlen(name), 0);

		/* lock database access in this process, but wait maximum 1 second */
		if (GameServer->LockRankingDb(1000))
		{

			/* when another process uses the database, wait up to 1 second */
			sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

			/* fetch from database */
			int row = sqlite3_step(pStmt);

			/* unlock database access */
			GameServer->UnlockRankingDb();

			/* result row was fetched */
			if (row == SQLITE_ROW)
			{

				int score 		    	= sqlite3_column_int(pStmt, 0);
				int numWins 			= sqlite3_column_int(pStmt, 1);
				int numKills 			= sqlite3_column_int(pStmt, 2);
				int numKillsWallshot 	= sqlite3_column_int(pStmt, 3);
				int numDeaths 		    = sqlite3_column_int(pStmt, 4);
				int numShots 		    = sqlite3_column_int(pStmt, 5);
				int highestSpree 		= sqlite3_column_int(pStmt, 6);
				int timePlayed 		    = sqlite3_column_int(pStmt, 7);
				int rank 		    	= sqlite3_column_int(pStmt, 8);
				int scoreToNextRank 	= sqlite3_column_int(pStmt, 9);

				// needs to have played at least x seconds to be visibly ranked
				if (timePlayed > g_Config.m_SvTimePlayedToSeeRank || numWins > 0)
				{

					s << "╔—————————  Individual Rank  —————————╗" << "\n";
					s << "║ Rank of '" << rankedName << "'";

					if (sendToEveryone)
					{
						s << " requested by '" << requestingName << "'";
					}
					s << "\n";

					s << "║ Rank: " << rank << "\n";
					s << "║ Score: ";

					s << std::fixed;
					if (score % 100){	s << std::fixed << std::setprecision(2);	}
					else { 				s << std::fixed << std::setprecision(0);	}
					s << (score / 100.0);
					s << "  and to next Rank: ";
					s << std::fixed;
					if (scoreToNextRank % 100){	s << std::setprecision(2);	}
					else { 						s << std::setprecision(0);	}
					s << scoreToNextRank / 100.0;
					s << "\n";

					s << "║ Wins: " 								<< numWins;

					if (numWins > 0)
					{
						s << " Score/Win: ";
						s << std::fixed << std::setprecision(2) 	<< score / (100.0 * numWins);
						s << " Spree/Win: ";
						s << std::fixed << std::setprecision(0) 	<< pointsToEnemiesKilled(score / numWins); 

					}
					s << "\n";

					s << "║ Kills: " 			<< numKills 					<< "\n";
					
					if (gamemode == "Laser" || gamemode == "Everything")
					{
						s << "║ Wallshot Kills: "<< numKillsWallshot 			<< "\n";
					}

					s << "║ Deaths: " 			<< numDeaths 					<< "\n";
					s << "║ Shots: " 			<< numShots 					<< "\n";
					s << "║ Highest Spree: " 	<< highestSpree 	 			<< "\n";

					s << "║ Time played: ";
					s << timePlayed / 3600;
					s << ":"<< std::setw(2) << std::setfill('0');
					s << timePlayed/60 % 60;
					s << "h";
					s << "\n";

					s <<"╚———————————————————————————╝" << "\n";

				} else {
					int remainingTimeToSeeRank = g_Config.m_SvTimePlayedToSeeRank - timePlayed;
					s << "'" << name << "' has to play for another ";
					s << remainingTimeToSeeRank;
					s << " seconds or win a round in order to see the rank.\n";
					toEveryone = false;
				}
			}

			/* database is locked */
			else if (row == SQLITE_BUSY)
			{
				s << "Could not get rank of '" << name << "'. Try again later.\n";
				toEveryone = false;
			}

			/* no result found */
			else if (row == SQLITE_DONE)
			{
				s << "'" << name << "' has no rank\n";
				toEveryone = false;
			}

		}
		else
		{
			s << "Could not get rank of '" << name << "'. Try again later.\n";
			toEveryone = false;
		}
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	lines = s.str();
	if (toEveryone)
	{
		SendLines(GameServer, lines);
	} else {
		SendLines(GameServer, lines, clientId);
	}
	

	sqlite3_finalize(pStmt);
	/*name has to be freed here, because this function is executed in a thread.*/
	free(name);
}


/**
 * @brief Merges two rankings into one TARGET ranking and deletes the SOURCE ranking.
 * @details Warning: Don't get confused if highest spree is not merged, because the highest spree is actually the maximum of both values.
 * 			This function can also merge and create a not existing TARGET, which is then inserted into the database with the given
 * 			trimmed(no whitespaces on both sides) TARGET nickname.
 * 			WARNING: Given parameters: Source and Target are freed after the execution of this function.
 *
 * @param GameServer pointer to the CGameContext /  GameServer, which contains most information about players etc.
 * @param Source char* of the name of the Source player which is deleted at the end of the merge.
 * @param Target Target player, which receives all of the Source player's achievements.
 */
void CGameController_zCatch::MergeRankingIntoTarget(CGameContext* GameServer, char* Source, char* Target)
{
	for (int i = 1; i <= 5; ++i)
	{

		int source_score = 0;
		int source_numWins = 0;
		int source_numKills = 0;
		int source_numKillsWallshot = 0;
		int source_numDeaths = 0;
		int source_numShots = 0;
		int source_highestSpree = 0;
		int source_timePlayed = 0;

		/*Sqlite stuff*/
		/*sqlite statement object*/
		sqlite3_stmt *pStmt = 0;
		int source_rc;
		int source_row;

		/*error handling*/
		int err = 0;


		/* prepare */
		const char *zTail;
		char aMode[16];
		str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(i).c_str()) ;

		char aQuery[256];
		str_format(aQuery, sizeof(aQuery), "\
		SELECT \
			a.score, \
			a.numWins, \
			a.numKills, \
			a.numKillsWallshot, \
			a.numDeaths, \
			a.numShots, \
			a.highestSpree, \
			a.timePlayed \
		FROM %s a \
		WHERE username = trim(?1)\
		;", aMode);

		/* First part: fetch all data from Source player*/
		/*check if query is ok and create statement object pStmt*/
		source_rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, std::strlen(aQuery), &pStmt, &zTail);

		if (source_rc == SQLITE_OK)
		{
			/* bind parameters in query */
			sqlite3_bind_text(pStmt, 1, Source, std::strlen(Source), 0);

			/* lock database access in this process, but wait maximum 1 second */
			if (GameServer->LockRankingDb(1000))
			{

				/* when another process uses the database, wait up to 1 second */
				sqlite3_busy_timeout(GameServer->GetRankingDb(), 1000);

				/* fetch from database */
				source_row = sqlite3_step(pStmt);

				/* unlock database access */
				GameServer->UnlockRankingDb();

				/* result row was fetched */
				if (source_row == SQLITE_ROW)
				{
					/*get results from columns*/
					source_score = sqlite3_column_int(pStmt, 0);
					source_numWins = sqlite3_column_int(pStmt, 1);
					source_numKills = sqlite3_column_int(pStmt, 2);
					source_numKillsWallshot = sqlite3_column_int(pStmt, 3);
					source_numDeaths = sqlite3_column_int(pStmt, 4);
					source_numShots = sqlite3_column_int(pStmt, 5);
					source_highestSpree = sqlite3_column_int(pStmt, 6);
					source_timePlayed = sqlite3_column_int(pStmt, 7);

					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Fetched data of '%s' in mode: %s", Source, GetGameModeTableName(i).c_str());
					GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);

				}

				/* database is locked */
				else if (source_row == SQLITE_BUSY)
				{
					/* print error */
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", Source);
					GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
					err++;
				}

				/* no result found */
				else if (source_row == SQLITE_DONE)
				{
					/* print information */
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "'%s' has no rank in mode: %s", Source, GetGameModeTableName(i).c_str());
					GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
					err++;
				}

			}
			else
			{
				/* print error */
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Could not get rank of '%s'. Try again later.", Source);
				GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
				err++;
			}
		}
		else
		{
			/* print error */
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", source_rc, sqlite3_errmsg(GameServer->GetRankingDb()));
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			err++;

		}

		/*if at least one error ocurred, free everything and do nothing.*/
		if (err > 0 ) {
			sqlite3_finalize(pStmt);
			continue;
		}

		/*Second part: add fetched data to Target player*/
		/* give the points to Target player*/
		CGameController_zCatch::SaveScore(GameServer, // gamecontext
		                                  Target, // username ---> is freed!!
		                                  source_score, // score
		                                  source_numWins, // numWins
		                                  source_numKills, // numKills
		                                  source_numKillsWallshot, // numKillsWallshot
		                                  source_numDeaths, // numDeaths
		                                  source_numShots, // numShots
		                                  source_highestSpree, // highestSpree
		                                  source_timePlayed, // timePlayed
		                                  i,
		                                  1); // don't free Target

		/*Cannot use "Target" variable from here on. */

		/*Third part: delete Source player records.*/

		// don't free Source with 1 != 0 (frees)
		DeleteRanking(GameServer, Source, i, 1);
		// Source has been freed in DeleteRanking!
		sqlite3_finalize(pStmt);
		// Freeing allocated memory is done in these functions, because they are executed as detached threads.
	}

	free(Source);
	free(Target);
}


/**
 * @brief Delete the ranking of given player name.
 * @details This function deletes the ranking of the given player represented by his/her nickname.
 * 			WARNING: This function frees the allocated memory of the "Name" parameter.
 *
 * @param GameServer Is a CGameContext Object, which hold the information about out sqlite3 database handle.
 * 					That handle is needed here to execute queries on the database.
 * @param Name Is the name of the player whose ranking score should be deleted. The name is trimmed from both sides,
 * 			   in order to have a consistent ranking and no faking or faulty deletions.
 */
void CGameController_zCatch::DeleteRanking(CGameContext* GameServer, char* Name, int GameMode, int Free) {
	const char *zTail;
	char aMode[16];
	str_format(aMode, sizeof(aMode), "%s", GetGameModeTableName(GameMode).c_str()) ;

	char aQuery[128];
	str_format(aQuery, sizeof(aQuery), "DELETE FROM %s WHERE username = trim(?1);", aMode);

	sqlite3_stmt * pStmt;
	int rc = sqlite3_prepare_v2(GameServer->GetRankingDb(), aQuery, std::strlen(aQuery), &pStmt, &zTail);
	if (rc == SQLITE_OK) {

		/* bind parameters in query */
		sqlite3_bind_text(pStmt, 1, Name, std::strlen(Name), 0);

		/* lock database access in this process */
		GameServer->LockRankingDb();

		/* when another process uses the database, wait up to 1 minute */
		sqlite3_busy_timeout(GameServer->GetRankingDb(), 60000);

		char aBuf[512];

		/* save to database */
		switch (sqlite3_step(pStmt))
		{
		case SQLITE_DONE:
			/* Print deletion success message to rcon */
			str_format(aBuf, sizeof(aBuf), "Deleting records of '%s' in mode: %s" , Name, GetGameModeTableName(GameMode).c_str());
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			break;
		case SQLITE_BUSY:

			str_format(aBuf, sizeof(aBuf), "Error: could not delete records of '%s'(timeout)." , Name);
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			break;
		default:
			str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
			GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		}

		/* unlock database access */
		GameServer->UnlockRankingDb();
	}
	else
	{
		/* print error */
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "SQL error (#%d): %s", rc, sqlite3_errmsg(GameServer->GetRankingDb()));
		GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
	}

	sqlite3_finalize(pStmt);

	if (!Free) {
		free(Name);
	}
}

/**
 * @brief Returns the mode specific name. WARNING: Do not forget to free the returned pointer after usage.
 * @details Depending on the current game mode e.g. Laser, Grenade, etc. this function returns a pointer to a
 * 			string with the name of specified mode. This is needed to constuct sqlite queries with the mode specific tables
 * 			in the database.
 * @return name of current game mode.
 */
std::string CGameController_zCatch::GetGameModeTableName(int GameMode) {

	int mode = 0;

	if (!GameMode)
	{
		// global variable declated at the top.
		mode = m_OldMode;
	}
	else
	{
		mode = GameMode;
	}

	switch (mode) {
		case 1: 
			return std::string("Laser");
		case 2: 
			return std::string("Everything");
		case 3: 
			return std::string("Hammer");
		case 4: 
			return std::string("Grenade");
		case 5: 
			return std::string("Ninja");
		default: 
			return std::string("zCatch");
		}
	
}

unsigned int CGameController_zCatch::SendLines(CGameContext* GameServer, std::string& lines, int ClientID)
{
	unsigned int linesSent = 0;
    /**
     * iterators to walk over the string.
     */
    auto line_beginning 	= lines.begin();
    auto line_end 			= lines.begin();
    auto it 				= lines.begin();

    /**
     * split at newlines and print to requesting player.
     */
    while (it != lines.end())
    {
        /**
         * An iterator implements the dereferencing operator
         * 'operator*()' which is used to access the underlying
         * element that the iterator currently resides at.
         */
        if ((*it) == '\n')
        {
            /**
             * Do not include the newline character in our string.
             */
            line_end = it;
            /// Copies string from beginning to end, excluding end.
            std::string line(line_beginning, line_end);
            line_beginning = line_end + 1;
            GameServer->SendChatTarget(ClientID, line.c_str());
        }

        /**
         * an iterator also implements the 'operator++',
         * which is, in this case, used to walk to the next
         * character in the string.
         */
        it++;
        linesSent++;
    }

    return linesSent;

}
 
int CGameController_zCatch::enemiesKilledToPoints(int enemies)
 {
	if(enemies <= 0)
	{
		return 0;
	}

	// factor to squeeze our curve between 0 and 1
 	const double normalize_factor = std::exp((MAX_CLIENTS - 1) / 5.0);
 	
 	double e = std::exp(enemies / 5.0);

 	double normalized_points = e / normalize_factor;

	// we want to return ineteger values instead of floating point values, thus
	// we multiply our normalized curve by 100 in order to be able to work with
	// two decimal places later on.
 	return static_cast<int>(std::ceil(100.0 * normalized_points));
 }

double CGameController_zCatch::pointsToEnemiesKilled(int points)
 {
	if (points <= 0)
	{
		return 0.0;
	}
	// in order to reverse the normalization, we need the factor again
 	const double normalize_factor = std::exp((MAX_CLIENTS - 1) / 5.0);

	// the points we get are 100 times our normalized points, thus 
	// in order to get them bavk between 0 amd 1, we devide them by 100
 	double normalized_points = points / 100.0;

	// reverse normalization 0 - 1 -> 0 
 	double unnormalized_points = normalized_points * normalize_factor;

 	double ln = std::log(unnormalized_points);

	// ln(exp(x/5)) = x/5 => * 5 => x
 	int enemies = static_cast<int>(ln * 5.0);
 	return enemies;
 }







