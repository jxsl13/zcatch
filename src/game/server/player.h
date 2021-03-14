/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include "alloc.h"
#include <vector>
#include <mutex>
#include <sstream>


enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, int ClientID, bool Dummy, bool AsSpec = false);
	~CPlayer();

	void Init(int CID);

	void TryRespawn();
	void Respawn();
	void SetTeam(int Team, bool DoChatMsg=true);
	int GetTeam() const { return m_Team; };
	int GetCID() const { return m_ClientID; };
	bool IsDummy() const { return m_Dummy; }

	void Tick();
	void PostTick();
	void Snap(int SnappingClient);

	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput);
	void OnDisconnect();

	void KillCharacter(int Weapon = WEAPON_GAME);
	CCharacter *GetCharacter();

	// zCatch
	
	bool m_IsRankFetched; // set to true if data was successfully or not successfully fetched from db.
	int m_Wins; // number of wins
	int m_Score; // specific value based on players killed
	int m_Kills; // players killed
	int m_Deaths; // died
	int m_TicksWarmup; // playing warmup
	int m_TicksCaught; // 
	int m_TicksIngame; // ticks ingame
	int m_Shots; // number of shot projectiles
	int m_Fails; // how often the player fell down
	int m_Rank;
	void ResetStatistics();

	enum EReleaseReason {
		REASON_NONE = 0,

		// release reasons
		// #### one by one player release START
		REASON_PLAYER_RELEASED,
		REASON_PLAYER_WARMUP_RELEASED,

		// player's state is reset to released when 
		// joining the game after having been in spec
		REASON_PLAYER_JOINED_GAME_AGAIN, 
		// #### one by one player release END

		// #### all at once player release START
		REASON_PLAYER_DIED,
		REASON_PLAYER_FAILED,
		REASON_PLAYER_LEFT,
		REASON_PLAYER_JOINED_SPEC,
		REASON_EVERYONE_RELEASED, // End of round
		// #### all at once player release END

		// catch reasons
		REASON_PLAYER_CAUGHT,
		REASON_PLAYER_WARMUP_CAUGHT,

		// caught & released reasons
		REASON_PLAYER_JOINED,
	};

	// whether the player wants to receive more detailed server messages.
	bool m_DetailedServerMessages;

	bool CatchPlayer(int ID, int reason=REASON_PLAYER_CAUGHT);
	bool IsCaught();
	bool IsNotCaught();
	int ReleaseLastCaughtPlayer(int reason=REASON_NONE, bool updateSkinColors=false);
	int ReleaseAllCaughtPlayers(int reason=REASON_NONE);

	// return internally saved value
	int GetPlayersLeftToCatch();
	
	// set this externally
	void SetPlayersLeftToCatch(int leftToCatch);

	// forcefully remove player from another player's victims
	// used to set 
	bool BeSetFree(int reason=REASON_PLAYER_LEFT);

	// who caught me
	int GetIDCaughtBy();

	// why was I caught
	int GetCaughtReason();

	// how many players did I catch
	int GetNumCurrentlyCaughtPlayers();

	// players who are caught + players who left that were caught
	int GetNumTotalCaughtPlayers();

	// how many of my caught player did leave the game
	int GetNumCaughtPlayersWhoLeft();

	// get the number of player sthat joine dthe game and were caught then
	int GetNumCaughtPlayersWhoJoined();

	// how many players were released by the players willingly
	int GetNumReleasedPlayers();

	// how many players were killed in a row.
	int GetNumCaughtPlayersInARow();

	// set my state to released, doesn't remove from
	// possible caught list of another player
	bool BeReleased(int reason=REASON_NONE);
	
	// handle auto joining spectators flag
	bool GetWantsToJoinSpectators();
	void SetWantsToJoinSpectators();
	void ResetWantsToJoinSpectators();

	void UpdateSkinColors();

	enum PunishmentLevel {
		NONE = 0,
		PROJECTILES_DONT_KILL,
	};
	PunishmentLevel GetPunishmentLevel() {return m_PunishmentLevel; }
	void SetPunishmentLevel(PunishmentLevel level) {m_PunishmentLevel = level;}


	// Administrative properties of this player.
	inline bool IsAuthed(){ return Server()->IsAuthed(m_ClientID);};

	// Anti chat spam
	int m_ChatTicks;

	// Chat related troll pit (troll communication sphere)
	inline void SetTroll() { m_Troll = true; }
	inline void RemoveTroll() { m_Troll = false; }
	inline bool IsTroll() { return m_Troll; }

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	// used for snapping to just update latency if the scoreboard is active
	int m_aActLatency[MAX_CLIENTS];

	// used for spectator mode
	int GetSpectatorID() const { return m_SpectatorID; }
	bool SetSpectatorID(int SpecMode, int SpectatorID);
	bool m_DeadSpecMode;
	bool DeadCanFollow(CPlayer *pPlayer) const;
	void UpdateDeadSpecMode();

	bool m_IsReadyToEnter;
	bool m_IsReadyToPlay;

	bool m_RespawnDisabled;

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
	int m_LastReadyChange;

	// TODO: clean this up
	struct
	{
		char m_aaSkinPartNames[NUM_SKINPARTS][24];
		int m_aUseCustomColors[NUM_SKINPARTS];
		int m_aSkinPartColors[NUM_SKINPARTS];
	} m_TeeInfos;

	int m_RespawnTick;
	int m_LastRespawnedTick;
	int m_DieTick;
	int m_ScoreStartTick;
	int m_LastActionTick;
	int m_TeamChangeTick;

	int m_InactivityTickCounter;

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

	// for printing purposes
	std::string str();

private:
	CCharacter *m_pCharacter;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	int m_ClientID;
	int m_Team;
	bool m_Dummy;

	// zCatch
	int m_CaughtBy;
	int m_CaughtReason;

	int m_PlayersLeftToCatch;
	enum { NOT_CAUGHT = -1};

	bool BeCaught(int byID, int reason=REASON_NONE);
	
	// remove a specific player from my caught players
	// and release him/her
	bool RemoveFromCaughtPlayers(int ID, int reason=REASON_NONE);


	std::vector<int> m_CaughtPlayers;
	// preventing rejoin exploitation.
	int m_NumCaughtPlayersWhoLeft;
	int m_NumCaughtPlayersWhoJoined;
	int m_NumWillinglyReleasedPlayers;

	bool m_WantsToJoinSpectators;

	unsigned int GetColor();

	PunishmentLevel m_PunishmentLevel;

	//Anticamper
	int Anticamper();
	bool m_SentCampMsg;
	int m_CampTick;
	vec2 m_CampPos;


	// used for spectator mode
	int m_SpecMode;
	int m_SpectatorID;
	class CFlag *m_pSpecFlag;
	bool m_ActiveSpecSwitch;

	// This flag is used to put players out of the normal
	// chat sphere and into another sphere where only trolls 
	// can communicate with other trolls.
	bool m_Troll;
};

#endif
