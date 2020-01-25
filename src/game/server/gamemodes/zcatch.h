/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_ZCATCH_H
#define GAME_SERVER_GAMEMODES_ZCATCH_H
#include <initializer_list>
#include <set>
#include <tuple>
#include  <deque>
#include <game/server/gamecontroller.h>
#include <game/server/gamemodes/zcatch/rankingserver.h>

class CGameControllerZCATCH : public IGameController
{
public:
	CGameControllerZCATCH(class CGameContext *pGameServer);
	~CGameControllerZCATCH();

	// game
	virtual void Tick();
	virtual void DoWincheckRound();
	
	virtual void OnPlayerInfoChange(class CPlayer *pPlayer);
	virtual void OnPlayerConnect(class CPlayer *pPlayer);
	virtual void OnPlayerDisconnect(class CPlayer *pPlayer);

	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	virtual void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg=true);

	virtual void EndRound();


	/**
	 * Intercept chat messages in order to respond to chat commands
	 */
	virtual void OnChatMessage(int ofID, int Mode, int toID, const char *pText);

	/**
	 * Intercept callvotes in order to respond to them.
	 * Return true in order for the calling of the votes to proceed.
	 */
	virtual bool OnCallvoteOption(int ClientID, const char* pDescription, const char* pCommand, const char* pReason);
	virtual bool OnCallvoteBan(int ClientID, int KickID, const char* pReason);
	virtual bool OnCallvoteSpectate(int ClientID, int SpectateID, const char* pReason);


	virtual void OnReset();

private:

	// store initial weapon mode, in order 
	//to prevent players from changing it mid game.
	int m_WeaponMode;

	// not changable mid game.
	int m_SkillLevel;

	/**
	 * When a player enters the game, and the game is
	 * currently running, the player has to be added 
	 * as victim to the dominating player.
	 * As there can be multiple dominating players, 
	 * one has to be chosen from multiple dominating players.
	 */
	class CPlayer* ChooseDominatingPlayer(int excludeID=-1);

	/**
	 * Send server message to the individual player about
	 * their ingame statistics,
	 */
	void ShowPlayerStatistics(class CPlayer *pOfPlayer);

	/**
	 * The value of enemies left to catch is kept alive and visible,
	 * but is not updated.
	 * Info: No calculations are done in here!
	 */
	void RefreshBroadcast();

	/**
	 * This, contraty to @RefreshBroadcast, updates the enemies left to catch
	 * counter for specified/every player/s, and sends the updated value.
	 */
	void UpdateBroadcastOf(std::initializer_list<int> IDs);
	void UpdateBroadcastOfEverybody();

	/**
	 * allows the usage of UpdateSkinsOf({multiple, ids})
	 * sends to everyone that the skin information of those
	 * multiple ids has changed. (color change)
	 */
	void UpdateSkinsOf(std::initializer_list<int> IDs);
	void UpdateSkinsOfEverybody();
	
	/**
	 * Used to track, whether we reached the players count needed to
	 * end a round or whether we went below that treshold.
	 */
	int m_PreviousIngamePlayerCount;
	int m_IngamePlayerCount;

	/**
	 * Count the currently playing players that are ingame and 
	 * not in spectator mode and updates the m_IngamePlayerCount
	 * and the m_PreviousIngamePlayerCount member variables.
	 */
	void UpdateIngamePlayerCount();

	// refresh time in ticks, after how many ticks 
	// the broadcast refresh is being resent.
	int m_BroadcastRefreshTime;


	// an encapsulated class, that handles the database connection.
	IRankingServer* m_pRankingServer;

	std::vector<std::pair<int, CPlayerStats> > m_RankingRetrievalMessageQueue;
	std::mutex m_RankingRetrievalMessageQueueMutex;
	void ProcessRankingRetrievalMessageQueue();

	// initializes ranking server
	void InitRankingServer();

	// fill s player stats with data from the database
	void RetrieveRankingData(int ofID);

	// saves player data to the database
	void SaveRankingData(int ofID);

	// depending on the current mod, grenade, laser, etc, 
	// a prefix is created for the specific database columns, fields, etc.
	std::string GetDatabasePrefix();


	// based on the Total Players caught, the winner 
	// receives a specific score rating.
	// calculates an 10 * e^(x/5) / e^((MAX_PLAYERS - 1) / 5) function, 
	// that is normalized between 0 and 10 and discretized to integer values.
	int CalculateScore(int PlayersCaught);


	// if a player requests data from the database, the retrieved messages are firstly pu into this queue
	// in order for them to be handled in the main thread. As sending packages to specific clients can only be done in
	// the main thread, because teeworlds does not support multithreading on the server side.
	std::mutex m_MessageQueueMutex;
	std::vector<std::pair<int, std::vector<std::string>> > m_MessageQueue;

	// try to send messages in the MessageQueue to the requesting players.
	void ProcessMessageQueue();

	// handles the filling of the MessageQueue for the /rank <nickname> command
	void RequestRankingData(int requestingID, std::string ofNickname);

	// handles the filling of the MessageQueue for the /top command
	void RequestTopRankingData(int requestingID, std::string key);


	// left players cache
	// leaving player's ips are saved here with the player's id who caught them
	// if the catching player dies, their entry is removed from cache, if the leaving player
	// rejoins, they are caught by the same player again.
	// <leaving IP, caught by ID, expires at Tick>
	std::vector<std::tuple<std::string, int, int> > m_LeftCaughtCache;

	void AddLeavingPlayerIPToCaughtCache(int LeavingID);

	/**
	 * @brief If a player dies or leaves, his cache of players that left while being caught by him, will be cleared
	 * 
	 * @param DyingOrLeavingID 
	 */
	void RemovePlayerIDFromCaughtCache(int DyingOrLeavingID);

	/**
	 * @brief If a player joins and gets caught, his entry in the cache is being removed
	 * 
	 * @param IP 
	 */
	void RemoveIPOfJoiningPlayerFromCaughtCache(std::string& IP);

	/**
	 * @brief A joining player that previously left the game can be added
	 * 			on rejoining to the same player
	 * 
	 * @param JoiningID 
	 * @return int valid id, if in cache, -1 else
	 */
	int IsInCaughtCache(int JoiningID);

	/**
	 * @brief should be executed periodically, like every second at most
	 */
	void CleanLeftCaughtCache();

	/**
	 * @brief Handle rejoin cache when player leaves
	 * 
	 * @param LeavingID 
	 */
	void HandleLeavingPlayerCaching(int LeavingID);

	/**
	 * @brief Handle rejoin cache when player dies.
	 * 
	 * @param DyingPlayer 
	 */
	void HandleDyingPlayerCaching(int DyingPlayerID);

	/**
	 * @brief Handle rejoin cache when player joins
	 * 
	 * @param JoiningPlayer 
	 * @return returns false if no action has been taken, 
	 * returns true if a player was added to his cached catcher.
	 */
	bool HandleJoiningPlayerCaching(int JoiningPlayerID);


	/**
	 * @brief Newbie Server stuff
	 * -1 -> don't kick
	 * 0 -> kick,
	 * > 0 -> decrease counter
	 */
	int m_PlayerKickTicksCountdown[MAX_CLIENTS];
	std::deque<std::string> m_KickedPlayersIPCache;
	enum {
		NO_KICK = -1,
	};

	// Kick player in x Seconds
	void SetKickIn(int ClientID, int Seconds);

	// executed on every tick, handles countdown og KickTimer
	void KickCountdownOnTick();

	// what prevents people from joining the server
	void HandleBeginnerServerCondition(CPlayer* pPlayer);

	// a kicked player or a player deemed as not allowed to play 
	// on the server is added to the IP cache
	void AddKickedPlayerIPToCache(int ClientID);

	// Check if player is not allowed to play.
	bool CheckIPInKickedBeginnerServerCache(int ClientID);


	// Init the chat command that are to be sent to joining players
	void ChatCommandsOnInit();

	// send chat command messages
	void ChatCommandsOnPlayerConnect(CPlayer* pPlayer);


};

#endif
