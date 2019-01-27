/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime                                                                 */

#ifndef GAME_SERVER_GAMEMODES_ZCATCH_H
#define GAME_SERVER_GAMEMODES_ZCATCH_H

#include <game/server/gamecontroller.h>
#include <thread>

class CGameController_zCatch: public IGameController
{

	
	void RewardWinner(int winnerId);
	
	/* ranking system */
	static void ChatCommandTopFetchDataAndPrint(CGameContext* GameServer, int clientId, const char *column, const char* title);
	static void ChatCommandRankFetchDataAndPrint(CGameContext* GameServer, int clientId, char *name, bool sendToEveryone);
	static void SaveScore(CGameContext* GameServer, char *name, int score, int numWins, int numKills, int numKillsWallshot, int numDeaths, int numShots, int highestSpree, int timePlayed, int GameMode, int Free = 0);
	static void ChatCommandStatsFetchDataAndPrint(CGameContext* GameServer, int clientId, const char* cmd);

	/* Helper functions */
	static unsigned int SendLines(CGameContext* GameServer, std::string& lines, int ClientID = -1);

	/**
	 * Score point transformation
	 */
	static inline int enemiesKilledToPoints(int enemies);
	static inline double pointsToEnemiesKilled(int points);

public:
	CGameController_zCatch(class CGameContext *pGameServer);
	~CGameController_zCatch();
	virtual void Tick();
	virtual void DoWincheck();

	virtual void StartRound();
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual void OnPlayerInfoChange(class CPlayer *pP);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID);
	virtual bool OnEntity(int Index, vec2 Pos);
	virtual bool CanChangeTeam(CPlayer *pPlayer, int JoinTeam);
	virtual void EndRound();
	


	/* ranking system */
	virtual void SaveRanking(CPlayer *player);
	virtual void OnInitRanking(sqlite3 *rankingDb);
	virtual void OnChatCommandTop(CPlayer *pPlayer, const char *category = "");
	virtual void OnChatCommandOwnRank(CPlayer *pPlayer);
	virtual void OnChatCommandRank(CPlayer *pPlayer, const char *name, bool sendToEveryone = false);

	virtual void OnChatCommandStats(CPlayer *pPlayer, const char *cmdName);



	static void MergeRankingIntoTarget(CGameContext* GameServer, char* Source, char *Target);
	static void DeleteRanking(CGameContext* GameServer, char* Name, int GameMode = 0, int Free = 0);
	static std::string GetGameModeTableName(int GameMode = 0);

};

#endif
