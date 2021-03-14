/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/console.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <game/layers.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"

#include <string>
#include <vector>
#include <set>

/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/
class CGameContext : public IGameServer
{
	IServer *m_pServer;
	class IConsole *m_pConsole;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;

	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConPause(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeamAll(IConsole::IResult *pResult, void *pUserData);
	static void ConSwapTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConShuffleTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConLockTeams(IConsole::IResult *pResult, void *pUserData);
	static void ConForceTeamBalance(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// mutes
	static void ConMute(IConsole::IResult *pResult, void *pUserData);
	static void ConUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConMutes(IConsole::IResult *pResult, void *pUserData);

	// shadowmutes, troll pit
	static void ConShadowMute(IConsole::IResult *pResult, void *pUserData);
	static void ConShadowUnmute(IConsole::IResult *pResult, void *pUserData);
	static void ConShadowMutes(IConsole::IResult *pResult, void *pUserData);

	// players being punished
	static void ConPunishPlayer(IConsole::IResult *pResult, void *pUserData);
	static void ConUnPunishPlayer(IConsole::IResult *pResult, void *pUserData);
	static void ConPunishedPlayers(IConsole::IResult *pResult, void *pUserData);


	// Request reset of the ranking of a specific player ID
	// Sends a confirmation or abort request
	static void ConRankReset(IConsole::IResult *pResult, void *pUserData);
	
	// Confirm reset and reset the player's ranking
	static void ConConfirmReset(IConsole::IResult *pResult, void *pUserData);

	// Abort reset request.
	static void ConAbortReset(IConsole::IResult *pResult, void *pUserData);

	CGameContext(int Resetting);
	void Construct(int Resetting);

	bool m_Resetting;


	// We want to minimize the number of iterations, that are needed to find all
	// valid players. Thus every valid player is added at join time to the set
	// and removed when leaving.
	std::vector<int> m_PlayerIDs;


public:
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }

	CGameContext();
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	class CPlayer *m_apPlayers[MAX_CLIENTS];

	class IGameController *m_pController;
	CGameWorld m_World;

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);

	int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason);
	void EndVote(int Type, bool Force);
	void ForceVote(int Type, const char *pDescription, const char *pReason);
	void SendVoteSet(int Type, int ToClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteOnDisconnect(int ClientID);
	void AbortVoteOnTeamChange(int ClientID);

	int m_VoteCreator;
	int m_VoteType;
	int64 m_VoteCloseTime;
	int64 m_VoteCancelTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_VoteClientID;
	int m_NumVoteOptions;
	int m_VoteEnforce;
	enum
	{
		VOTE_ENFORCE_UNKNOWN=0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,

		VOTE_TIME=25,
		VOTE_CANCEL_TIME = 10,

		MIN_SKINCHANGE_CLIENTVERSION = 0x0703,
		MIN_RACE_CLIENTVERSION = 0x0704,
	};
	class CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage, std::set<int>* validTargets=nullptr, int64 Mask=-1);
 	void CreateHammerHit(vec2 Pos, int64 Mask=-1);
	void CreatePlayerSpawn(vec2 Pos);
	void CreateDeath(vec2 Pos, int Who);
	void CreateSound(vec2 Pos, int Sound, int64 Mask=-1);

	// network
	void SendChat(int ChatterClientID, int Mode, int To, const char *pText);
	virtual void SendServerMessage(int To, const char *pText);
	virtual void SendServerMessageText(int toID, const char *pText);
	virtual void SendServerMessageToEveryoneExcept(std::vector<int> IDs, const char *pText);

	void SendBroadcast(const char *pText, int ClientID);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendMotd(int ClientID);
	void SendSettings(int ClientID);
	void SendSkinChange(int ClientID, int TargetID);

	void SendGameMsg(int GameMsgID, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID);

	//
	void CheckPureTuning();
	void SendTuningParams(int ClientID);

	//
	void SwapTeams();

	// engine events
	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnShutdown();

	virtual void OnTick();
	virtual void OnPreSnap();
	virtual void OnSnap(int ClientID);
	virtual void OnPostSnap();

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID);

	virtual void OnClientConnected(int ClientID, bool AsSpec) { OnClientConnected(ClientID, false, AsSpec); }
	void OnClientConnected(int ClientID, bool Dummy, bool AsSpec);
	void OnClientTeamChange(int ClientID);
	virtual void OnClientEnter(int ClientID);
	virtual void OnClientDrop(int ClientID, const char *pReason);
	virtual void OnClientDirectInput(int ClientID, void *pInput);
	virtual void OnClientPredictedInput(int ClientID, void *pInput);

	virtual bool IsClientReady(int ClientID) const;
	virtual bool IsClientPlayer(int ClientID) const;
	virtual bool IsClientSpectator(int ClientID) const;

	virtual const char *GameType() const;
	virtual const char *Version() const;
	virtual const char *NetVersion() const;
	virtual const char *NetVersionHashUsed() const;
	virtual const char *NetVersionHashReal() const;

	bool IsVanillaGameType() const;


	// mutes
	struct CMute
	{
		CMute(const char* pIP, int ExpirationTick, std::string Nickname = {}, std::string Reason = {}) : 
			m_IP{pIP}, m_ExpiresTick{ExpirationTick}, m_Nickname{Nickname}, m_Reason{Reason}
		{
		};
		std::string m_IP;
		int m_ExpiresTick;
		std::string m_Nickname;
		std::string m_Reason;
	};

	std::vector<CMute> m_Mutes;
	void AddMute(const char* pIP, int Secs, std::string Nickname, std::string Reason, bool Auto = false);
	void AddMute(int ClientID, int Secs, std::string Nickname, std::string Reason, bool Auto = false);

	bool UnmuteIndex(int Index);
	bool UnmuteID(int ClientID);

	// returns -1 if is not mutes, otherwise returns the 
	// index of position, at which to find the mute
	int IsMuted(const char *pIP);
	int IsMuted(int ClientID);

	// remove expired mutes
	void CleanMutes();

	// handles auto mute & checks whether a player is muted.
	bool IsAllowedToChat(int ClientID);

	struct CTroll
	{
		std::string m_IP;
		long m_ExpiresAtTick;
		std::string m_Nickname;
		std::string m_Reason;
		// constructor
		CTroll(const char* pIP, int ExpirationTick, std::string Nickname = {}, std::string Reason = {}) : m_IP{pIP}, m_ExpiresAtTick{ExpirationTick}, m_Nickname{Nickname}, m_Reason{Reason}{};
		inline constexpr bool operator< (const CTroll& rhs) const
		{ 
			return m_ExpiresAtTick < rhs.m_ExpiresAtTick; 
		}
		inline constexpr bool IsExpired(long currentTick) {
			return m_ExpiresAtTick < currentTick;
		}
		inline constexpr long ExpiresInSecs(long currentTick, int tickSpeed=SERVER_TICK_SPEED) {
			long diff = m_ExpiresAtTick - currentTick;
			if (diff <= 0)
				return 0;
			
			return diff / tickSpeed;
		}
		inline constexpr bool UpdateIfExpiresLater(long expiresAtTick) {
			if (expiresAtTick <= m_ExpiresAtTick)
			{	
				return false;
			}
			m_ExpiresAtTick = expiresAtTick;
			return true;
		}

	};
	std::vector<CTroll> m_TrollPit;
	std::vector<int> m_IngameTrolls;

	// Iterates ovr all players and checks if the players are trolls, adds them to the m_IngameTrolls list
	void UpdateIngameTrolls();

	// Add trolls manually to the list where you
	// e.g. OnPlayerConnect in order to avoid calls to a lot of heavy functions
	void AddIngameTroll(int ID);

	// remove a troll from the list 
	// e.g. when they leave
	void RemoveIngameTroll(int ID);

	// get the list of trolls 
	// that are targets of chat messages
	// this whole construct makes this function call, that is often called more light weight.
	const std::vector<int>& GetIngameTrolls();
	int IsInTrollPit(const char *pIP);
	int IsInTrollPit(int ClientID);

	bool AddToTrollPit(const char* pIP, int Secs, std::string Nickname, std::string Reason = {});
	bool AddToTrollPit(int ClientID, int Secs, std::string Nickname, std::string Reason = {});

	bool RemoveFromTrollPitIndex(int Index);
	bool RemoveFromTrollPitID(int ClientID);

	void CleanTrollPit();

	void UpdateTrollStatus();


	void AddPlayer(int ClientID);
	void RemovePlayer(int ClientID);
	const std::vector<int>& PlayerIDs();
};

inline int64 CmaskAll() { return -1; }
inline int64 CmaskOne(int ClientID) { return (int64)1<<ClientID; }
inline int64 CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64 Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }
#endif
