/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

// this include should perhaps be removed
#include "entities/character.h"
#include "gamecontext.h"
#include <set>
#include <vector>
#include <bitset>
#include <algorithm>
#include <map>

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, int ClientID, int Team);
	~CPlayer();

	void Init(int CID);

	void TryRespawn();
	void Respawn();
	void SetTeam(int Team, bool DoChatMsg=true);
	void SetTeamDirect(int Team); //zCatch
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };

	void Tick();
	void PostTick();
	void Snap(int SnappingClient);

	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput);
	void OnDisconnect(const char *pReason);

	void KillCharacter(int Weapon = WEAPON_GAME);
	CCharacter *GetCharacter();

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	/**
     * @brief Unique flags that the client sends to the server.
     * Keeps track of all unique sent player flags
     */
    std::set<int> m_PlayerUniqueFlags;

    /**
     * @brief Returns a non-mutable vector created from the set of all unique flags.
     */
    const std::vector<int> GetUniqueFlags() const;


    /**
     * @brief could be interesting if a client sends multiple client versions.
     */
    std::set<int> m_ClientVersions;

    /**
     * @brief returns a non-mutable empty vector if the client is a vanilla client.
     * Returns one client version if the client is some kind of ddnet based client.
     * Returns multiple versions if the client sends multiple client versions.
     */
    const std::vector<int> GetUniqueClientVersions() const;

    /**
     * @brief Add a sent client version to the players set of unique
     * client versions.
     */
    void AddClientVersion(int version){m_ClientVersions.insert(version);};

    /**
     * Key = MessageID
     * Value = How many of these messages were received?
     */
    std::map<int, int> m_WeirdClientMessages;

    /**
     * @brief      Adds a weird message tat was received by the server.
     *			   Increases data(counter, how many were received) that's within the map.
     * @param[in]  MessageID  The message id
     */
    void AddWeirdMessage(int MessageID);

    /**
     * @brief      Gets the unique weird messages occurrences.
     *
     * @return     Returns the a sorted(by key values) vector of unknown messageIDs and
     * 				how often those were sent by that particular player.
     */
    const std::vector<std::pair<int, int>> GetUniqueWeirdMessageOccurrences() const;

    bool m_IsMousePositionVisible;

    void EnableCursorVisibility(){m_IsMousePositionVisible = true;}
    void DisableCursorVisibility(){m_IsMousePositionVisible = false;}
    bool IsCursorVisible() { return m_IsMousePositionVisible;}

	// used for snapping to just update latency if the scoreboard is active
	int m_aActLatency[MAX_CLIENTS];

	// used for spectator mode
	int m_SpectatorID;

	bool m_IsReady;

	//
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetTeam;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastKill;

	// TODO: clean this up
	struct
	{
		char m_SkinName[64];
		int m_UseCustomColor;
		int m_ColorBody;
		int m_ColorFeet;
	} m_TeeInfos;

	int m_RespawnTick;
	int m_DieTick;
	int m_Score;
	int m_ScoreStartTick;
	bool m_ForceBalanced;
	int m_LastActionTick;
	int m_TeamChangeTick;
	struct
	{
		int m_TargetX;
		int m_TargetY;
	} m_LatestActivity;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;
	
	//zCatch:
	int m_CaughtBy;
	bool m_SpecExplicit;
	int m_Deaths;
	int m_Kills;
	int m_LastKillTry;
	
	int m_TicksSpec;
	int m_TicksIngame;
	int m_ChatTicks;
	//Anticamper
	int Anticamper();
	bool m_SentCampMsg;
	int m_CampTick;
	vec2 m_CampPos;
	
	// zCatch/TeeVi
	enum
	{
		ZCATCH_NOT_CAUGHT = -1,
		ZCATCH_RELEASE_ALL = -1,
		ZCATCH_CAUGHT_REASON_JOINING = 0,
		ZCATCH_CAUGHT_REASON_KILLED,
	};
	struct CZCatchVictim
	{
		int ClientID;
		int Reason;
		CZCatchVictim *prev;
	};
	CZCatchVictim *m_ZCatchVictims;
	int m_zCatchNumVictims;
	int m_zCatchNumKillsInARow;
	int m_zCatchNumKillsReleased;
	bool m_zCatchJoinSpecWhenReleased;
	void AddZCatchVictim(int ClientID, int reason = ZCATCH_CAUGHT_REASON_JOINING);
	void ReleaseZCatchVictim(int ClientID, int limit = 0, bool manual = false);
	bool HasZCatchVictims() { return (m_ZCatchVictims != NULL); }
	int LastZCatchVictim() { return HasZCatchVictims() ? m_ZCatchVictims->ClientID : -1; }
	
	/* ranking system */
	struct {
		int m_Points;
		int m_NumWins;
		int m_NumKills;
		int m_NumKillsWallshot;
		int m_NumDeaths;
		int m_NumShots;
		int m_TimePlayed; // ticks
		int m_TimeStartedPlaying; // tick
	} m_RankCache;
	void RankCacheStartPlaying();
	void RankCacheStopPlaying();
	
	// zCatch/TeeVi hard mode
	struct {
		bool m_Active;
		char m_Description[256];
		unsigned int m_ModeAmmoLimit;
		unsigned int m_ModeAmmoRegenFactor;
		struct {
			bool m_Active;
			CCharacter *m_Character;
		} m_ModeDoubleKill;
		struct {
			bool m_Active;
			int m_TimeSeconds;
			int m_LastKillTick;
		} m_ModeKillTimelimit;
		bool m_ModeHookWhileKilling;
		struct {
			bool m_Active;
			unsigned int m_Heat;
		} m_ModeWeaponOverheats;
		struct {
			bool m_Active;
			unsigned int m_Max;
			unsigned int m_Fails;
		} m_ModeTotalFails;
		bool m_ModeInvisibleProjectiles; // TODO
		bool m_ModeInvisiblePlayers; // TODO
	} m_HardMode;
	bool AddHardMode(const char*);
	const char* AddRandomHardMode();
	void ResetHardMode();
	void HardModeRestart();
	void HardModeFailedShot();
	
	// bot detection
	int m_IsAimBot;
	int m_AimBotIndex;
	int m_AimBotRange;
	float m_AimBotTargetSpeed;
	vec2 m_CurrentTarget;
	vec2 m_LastTarget;
	int m_AimBotLastDetection;
	vec2 m_AimBotLastDetectionPos;
	vec2 m_AimBotLastDetectionPosVictim;
	
private:
	CCharacter *m_pCharacter;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;
};

#endif
