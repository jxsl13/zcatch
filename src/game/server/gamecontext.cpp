/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/shared/memheap.h>
#include <engine/map.h>

#include <generated/server_data.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/version.h>

#include "entities/character.h"
#include "entities/projectile.h"
#include "gamemodes/ctf.h"
#include "gamemodes/dm.h"
#include "gamemodes/lms.h"
#include "gamemodes/lts.h"
#include "gamemodes/mod.h"
#include "gamemodes/tdm.h"
#include "gamemodes/zcatch.h"
#include "gamecontext.h"
#include "player.h"

#include <algorithm>

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_VoteCancelTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;

	if(Resetting==NO_RESET)
		m_pVoteOptionHeap = new CHeap();
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if(!m_Resetting)
		delete m_pVoteOptionHeap;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}


class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self)
{
	float f = angle(Source);
	CNetEvent_Damage *pEvent = (CNetEvent_Damage *)m_Events.Create(NETEVENTTYPE_DAMAGE, sizeof(CNetEvent_Damage));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = Id;
		pEvent->m_Angle = (int)(f*256.0f);
		pEvent->m_HealthAmount = HealthAmount;
		pEvent->m_ArmorAmount = ArmorAmount;
		pEvent->m_Self = Self;
	}
}

void CGameContext::CreateHammerHit(vec2 Pos, int64 Mask)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage, std::set<int>* validTargets, int64 Mask)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	bool IsPunished = m_apPlayers[Owner] && m_apPlayers[Owner]->GetPunishmentLevel() > CPlayer::PunishmentLevel::NONE;
	

	// deal damage
	CCharacter *apEnts[MAX_CLIENTS];
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;
	float MaxForce = g_pData->m_Explosion.m_MaxForce;
	int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	CPlayer* pTmpPlayer = nullptr;

	for(int i = 0; i < Num; i++)
	{
		pTmpPlayer = apEnts[i]->GetPlayer();

		if (IsPunished && pTmpPlayer != m_apPlayers[Owner])
 			continue;

		vec2 Diff = apEnts[i]->GetPos() - Pos;
		vec2 Force(0, MaxForce);
		float l = length(Diff);
		if(l)
			Force = normalize(Diff) * MaxForce;
		float Factor = 1 - clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
		if((int)(Factor * MaxDamage))
		{
			//is a nullptr
			if (!validTargets)
			{
				apEnts[i]->TakeDamage(Force * Factor, Diff*-1, (int)(Factor * MaxDamage), Owner, Weapon);
			}
			else if(pTmpPlayer && validTargets->count(pTmpPlayer->GetCID()))
			{
				// take damage if valid target
				apEnts[i]->TakeDamage(Force * Factor, Diff*-1, (int)(Factor * MaxDamage), Owner, Weapon);
			}
			
		}
			
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn));
	if(ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int64 Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::SendServerMessage(int To, const char *pText)
{
	if (pText && pText[0])
	{
		if (To >= 0 && To < MAX_CLIENTS)
			SendChat(-1, CHAT_NONE, To, pText);
		else
			SendChat(-1, CHAT_ALL, -1, pText);
	}
}

void CGameContext::SendServerMessageToEveryoneExcept(std::vector<int> IDs, const char *pText)
{
	for (int ID : PlayerIDs())
	{
		for (int unwantedID : IDs)
		{
			if (ID != unwantedID)
			{
				SendChat(-1, CHAT_NONE, ID, pText);
			}
		}
	}
}

void CGameContext::SendServerMessageText(int To, const char *pText)
{
	constexpr int LineMaxLength = 62 - 4; // line widths - '*** ' prefix

	std::stringstream ssText{pText};
	std::stringstream ssCurrentLine;
    std::string Word;
	size_t CurrentLineLength = 0;

    while (ssText >> Word) {
        if (CurrentLineLength + Word.size() > LineMaxLength) {
			// line size exceeded

			// send previously accumulated line 
			SendServerMessage(To, ssCurrentLine.str().c_str());

			// reset line buffer
			CurrentLineLength = 0;
			ssCurrentLine.str({});
        	ssCurrentLine.clear();     
        }
		// line size not exceeded
        ssCurrentLine << Word << ' ';
        CurrentLineLength += Word.size() + 1;
    }

	std::string LastLine = ssCurrentLine.str();
	if (LastLine.size() > 0)
	{
		SendServerMessage(To, LastLine.c_str());
	}
	
}

void CGameContext::SendChat(int ChatterClientID, int Mode, int To, const char *pText)
{
	// Troll pit handling
	bool isTroll = false;

	char aBuf[256];
	if(0 <= ChatterClientID && ChatterClientID < MAX_CLIENTS)
	{
		if (m_apPlayers[ChatterClientID])
		{
			isTroll = m_apPlayers[ChatterClientID]->IsTroll();
		}

		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, To, Server()->ClientName(ChatterClientID), pText);
	} else
	{
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	}

	dbg_msg("DEBUG", "ID: %d, isTroll: %d", ChatterClientID, isTroll);
	char aBufMode[32];
	if(Mode == CHAT_WHISPER)
		str_copy(aBufMode, "whisper", sizeof(aBufMode));
	else if(Mode == CHAT_TEAM)
		str_copy(aBufMode, "teamchat", sizeof(aBufMode));
	else
		str_copy(aBufMode, "chat", sizeof(aBufMode));

	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, aBufMode, aBuf);


	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = Mode;
	Msg.m_ClientID = ChatterClientID;
	Msg.m_pMessage = pText;
	Msg.m_TargetID = -1;

	if (isTroll) {
		if(Mode == CHAT_ALL)
		{
			// send to chatting troll client for visual confirmation and record message
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);

			// send message to trolls only
			for(int ID : GetIngameTrolls())
			{
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ID);
			}
		}
		else if(Mode == CHAT_TEAM)
		{
			// pack one for the recording only
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

			To = m_apPlayers[ChatterClientID]->GetTeam();

			// send to the troll clients
			for(int ID : GetIngameTrolls())
			{
				if(m_apPlayers[ID] && m_apPlayers[ID]->GetTeam() == To)
				{
					Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ID);
				}
			}
		}
		else if(Mode == CHAT_WHISPER)
		{
			// Chatter client is troll, gets own message
			Msg.m_TargetID = To;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);
			
			// send to troll client only, don't send to normal players.
			if (m_apPlayers[To] && m_apPlayers[To]->IsTroll()) {
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
			}
		}
		else // Mode == CHAT_NONE
		{
			// the server cannot be a troll, so ignore this case.
		}
		return;
	}

	// is not a troll
	if(Mode == CHAT_ALL)
	{
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else if(Mode == CHAT_TEAM)
	{
		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		To = m_apPlayers[ChatterClientID]->GetTeam();

		// send to the clients
		for(int i : PlayerIDs())
		{
			if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == To)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
	else if(Mode == CHAT_WHISPER)
	{
		// send to the clients
		Msg.m_TargetID = To;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
	else // Mode == CHAT_NONE
	{
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
	
}

void CGameContext::SendBroadcast(const char* pText, int ClientID)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendMotd(int ClientID)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendSettings(int ClientID)
{
	CNetMsg_Sv_ServerSettings Msg;
	Msg.m_KickVote = g_Config.m_SvVoteKick;
	Msg.m_KickMin = g_Config.m_SvVoteKickMin;
	Msg.m_SpecVote = g_Config.m_SvVoteSpectate;
	Msg.m_TeamLock = m_LockTeams != 0;
	Msg.m_TeamBalance = g_Config.m_SvTeambalanceTime != 0;
	Msg.m_PlayerSlots = g_Config.m_SvPlayerSlots;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendSkinChange(int ClientID, int TargetID)
{
	CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientID = ClientID;
	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		Msg.m_apSkinPartNames[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aaSkinPartNames[p];
		Msg.m_aUseCustomColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aUseCustomColors[p];
		Msg.m_aSkinPartColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aSkinPartColors[p];
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, TargetID);
}

void CGameContext::SendGameMsg(int GameMsgID, int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_GAMEMSG);
	Msg.AddInt(GameMsgID);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendGameMsg(int GameMsgID, int ParaI1, int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_GAMEMSG);
	Msg.AddInt(GameMsgID);
	Msg.AddInt(ParaI1);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_GAMEMSG);
	Msg.AddInt(GameMsgID);
	Msg.AddInt(ParaI1);
	Msg.AddInt(ParaI2);
	Msg.AddInt(ParaI3);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i : PlayerIDs())
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq()*VOTE_TIME;
	m_VoteCancelTime = time_get() + time_freq()*VOTE_CANCEL_TIME;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(m_VoteType, -1);
	m_VoteUpdate = true;
}


void CGameContext::EndVote(int Type, bool Force)
{
	m_VoteCloseTime = 0;
	m_VoteCancelTime = 0;
	if(Force)
		m_VoteCreator = -1;
	SendVoteSet(Type, -1);
}

void CGameContext::ForceVote(int Type, const char *pDescription, const char *pReason)
{
	CNetMsg_Sv_VoteSet Msg;
	Msg.m_Type = Type;
	Msg.m_Timeout = 0;
	Msg.m_ClientID = -1;
	Msg.m_pDescription = pDescription;
	Msg.m_pReason = pReason;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendVoteSet(int Type, int ToClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if(m_VoteCloseTime)
	{
		Msg.m_ClientID = m_VoteCreator;
		Msg.m_Type = Type;
		Msg.m_Timeout = (m_VoteCloseTime-time_get())/time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Type = Type;
		Msg.m_Timeout = 0;
		Msg.m_ClientID = m_VoteCreator;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ToClientID);
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes+No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

void CGameContext::AbortVoteOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime && ClientID == m_VoteClientID && (str_startswith(m_aVoteCommand, "kick ") ||
		str_startswith(m_aVoteCommand, "set_team ") || (str_startswith(m_aVoteCommand, "ban ") && Server()->IsBanned(ClientID))))
		m_VoteCloseTime = -1;
}

void CGameContext::AbortVoteOnTeamChange(int ClientID)
{
	if(m_VoteCloseTime && ClientID == m_VoteClientID && str_startswith(m_aVoteCommand, "set_team "))
		m_VoteCloseTime = -1;
}


void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if(!m_pController)
		return;

	if(	str_comp(m_pController->GetGameType(), "DM")==0 ||
		str_comp(m_pController->GetGameType(), "TDM")==0 ||
		str_comp(m_pController->GetGameType(), "CTF")==0 ||
		str_comp(m_pController->GetGameType(), "LMS")==0 ||
		str_comp(m_pController->GetGameType(), "LTS")==0)
	{
		CTuningParams p;
		if(mem_comp(&p, &m_Tuning, sizeof(p)) != 0)
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetting tuning due to pure server");
			m_Tuning = p;
		}
	}
}

void CGameContext::SendTuningParams(int ClientID)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SwapTeams()
{
	if(!m_pController->IsTeamplay())
		return;

	SendGameMsg(GAMEMSG_TEAM_SWAP, -1);

	for(int i : PlayerIDs())
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			m_pController->DoTeamChange(m_apPlayers[i], m_apPlayers[i]->GetTeam()^1, false);
	}

	m_pController->SwapTeamscore();
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	// Clean troll pit every second, offset tick by one in oder not to collide with other 
	// cleanup actions that happen in the 0th tick every second.
	if (Server()->Tick() % SERVER_TICK_SPEED == 1)
	{
		CleanTrollPit();
	}

	for(int i : PlayerIDs())
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
			EndVote(VOTE_END_ABORT, false);
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i : PlayerIDs())
					if(m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || aVoteChecked[i])	// don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i+1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES || (m_VoteUpdate && Yes >= Total/2+1))
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				if(m_VoteCreator != -1 && m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;

				EndVote(VOTE_END_PASS, m_VoteEnforce==VOTE_ENFORCE_YES);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (m_VoteUpdate && No >= (Total+1)/2) || time_get() > m_VoteCloseTime)
				EndVote(VOTE_END_FAIL, m_VoteEnforce==VOTE_ENFORCE_NO);
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}


#ifdef CONF_DEBUG
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->IsDummy())
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[i]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	int NumFailures = m_NetObjHandler.NumObjFailures();
	if(m_NetObjHandler.ValidateObj(NETOBJTYPE_PLAYERINPUT, pInput, sizeof(CNetObj_PlayerInput)) == -1)
	{
		if(g_Config.m_Debug && NumFailures != m_NetObjHandler.NumObjFailures())
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "NETOBJTYPE_PLAYERINPUT failed on '%s'", m_NetObjHandler.FailedObjOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
	}
	else
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
	{
		int NumFailures = m_NetObjHandler.NumObjFailures();
		if(m_NetObjHandler.ValidateObj(NETOBJTYPE_PLAYERINPUT, pInput, sizeof(CNetObj_PlayerInput)) == -1)
		{
			if(g_Config.m_Debug && NumFailures != m_NetObjHandler.NumObjFailures())
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "NETOBJTYPE_PLAYERINPUT corrected on '%s'", m_NetObjHandler.FailedObjOn());
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
		else
			m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
	}
}

void CGameContext::OnClientEnter(int ClientID)
{
	m_pController->OnPlayerConnect(m_apPlayers[ClientID]);

	m_VoteUpdate = true;

	// update client infos (others before local)
	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = m_apPlayers[ClientID]->GetTeam();
	NewClientInfoMsg.m_pName = Server()->ClientName(ClientID);
	NewClientInfoMsg.m_pClan = Server()->ClientClan(ClientID);
	NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientID);
	NewClientInfoMsg.m_Silent = false;

	if(g_Config.m_SvSilentSpectatorMode && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
		NewClientInfoMsg.m_Silent = true;

	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aaSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aSkinPartColors[p];
	}


	for(int i : PlayerIDs())
	{
		if(i == ClientID || !m_apPlayers[i] || (!Server()->ClientIngame(i) && !m_apPlayers[i]->IsDummy()))
			continue;

		// new info for others
		if(Server()->ClientIngame(i))
			Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);

		// existing infos for new player
		CNetMsg_Sv_ClientInfo ClientInfoMsg;
		ClientInfoMsg.m_ClientID = i;
		ClientInfoMsg.m_Local = 0;
		ClientInfoMsg.m_Team = m_apPlayers[i]->GetTeam();
		ClientInfoMsg.m_pName = Server()->ClientName(i);
		ClientInfoMsg.m_pClan = Server()->ClientClan(i);
		ClientInfoMsg.m_Country = Server()->ClientCountry(i);
		ClientInfoMsg.m_Silent = false;
		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			ClientInfoMsg.m_apSkinPartNames[p] = m_apPlayers[i]->m_TeeInfos.m_aaSkinPartNames[p];
			ClientInfoMsg.m_aUseCustomColors[p] = m_apPlayers[i]->m_TeeInfos.m_aUseCustomColors[p];
			ClientInfoMsg.m_aSkinPartColors[p] = m_apPlayers[i]->m_TeeInfos.m_aSkinPartColors[p];
		}
		Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
	}

	// local info
	NewClientInfoMsg.m_Local = 1;
	Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);

	if(Server()->DemoRecorder_IsRecording())
	{
		CNetMsg_De_ClientEnter Msg;
		Msg.m_pName = NewClientInfoMsg.m_pName;
		Msg.m_ClientID = ClientID;
		Msg.m_Team = NewClientInfoMsg.m_Team;
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	}
}

void CGameContext::OnClientConnected(int ClientID, bool Dummy, bool AsSpec)
{
	if(m_apPlayers[ClientID])
	{
		dbg_assert(m_apPlayers[ClientID]->IsDummy(), "invalid clientID");
		OnClientDrop(ClientID, "removing dummy");
	}

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, Dummy, AsSpec);

	if(Dummy)
		return;

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(m_VoteType, ClientID);

	// send motd
	SendMotd(ClientID);

	// send settings
	SendSettings(ClientID);
}

void CGameContext::OnClientTeamChange(int ClientID)
{
	if(m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
		AbortVoteOnTeamChange(ClientID);

	// mark client's projectile has team projectile
	CProjectile *p = (CProjectile *)m_World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE);
	for(; p; p = (CProjectile *)p->TypeNext())
	{
		if(p->GetOwner() == ClientID)
			p->LoseOwner();
	}
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	AbortVoteOnDisconnect(ClientID);
	m_pController->OnPlayerDisconnect(pPlayer, pReason);

	// update clients on drop
	if(Server()->ClientIngame(ClientID))
	{
		if(Server()->DemoRecorder_IsRecording())
		{
			CNetMsg_De_ClientLeave Msg;
			Msg.m_pName = Server()->ClientName(ClientID);
			Msg.m_pReason = pReason;
			Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
		}

		CNetMsg_Sv_ClientDrop Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_pReason = pReason;
		Msg.m_Silent = false;
		if(g_Config.m_SvSilentSpectatorMode && pPlayer->GetTeam() == TEAM_SPECTATORS)
			Msg.m_Silent = true;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, -1);
	}

	// mark client's projectile has team projectile
	CProjectile *p = (CProjectile *)m_World.FindFirst(CGameWorld::ENTTYPE_PROJECTILE);
	for(; p; p = (CProjectile *)p->TypeNext())
	{
		if(p->GetOwner() == ClientID)
			p->LoseOwner();
	}

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	m_VoteUpdate = true;
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(!pRawMsg)
	{
		if(g_Config.m_Debug)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
		return;
	}

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat+Server()->TickSpeed() > Server()->Tick())
				return;

			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;

			// trim right and set maximum length to 128 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(!str_utf8_is_whitespace(Code))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 127)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 20 characters per second)
			if(Length == 0 || (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed()*(Length/20) > Server()->Tick()))
				return;

			pPlayer->m_LastChat = Server()->Tick();

			// don't allow spectators to disturb players during a running game in tournament mode
			int Mode = pMsg->m_Mode;
			if((g_Config.m_SvTournamentMode == 2) &&
				pPlayer->GetTeam() == TEAM_SPECTATORS &&
				m_pController->IsGameRunning() &&
				!Server()->IsAuthed(ClientID))
			{
				if(Mode != CHAT_WHISPER)
					Mode = CHAT_TEAM;
				else if(m_apPlayers[pMsg->m_Target] && m_apPlayers[pMsg->m_Target]->GetTeam() != TEAM_SPECTATORS)
					Mode = CHAT_NONE;
			}

			if(Mode != CHAT_NONE)
				m_pController->OnChatMessage(ClientID, Mode, pMsg->m_Target, pMsg->m_pMessage);
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			int64 Now = Server()->Tick();

			if(pMsg->m_Force)
			{
				if(!Server()->IsAuthed(ClientID))
					return;
			}
			else
			{
				if((g_Config.m_SvSpamprotection && ((pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry+Server()->TickSpeed()*3 > Now) ||
					(pPlayer->m_LastVoteCall && pPlayer->m_LastVoteCall+Server()->TickSpeed()*VOTE_COOLDOWN > Now))) ||
					pPlayer->GetTeam() == TEAM_SPECTATORS || m_VoteCloseTime)
					return;

				pPlayer->m_LastVoteTry = Now;
			}

			m_VoteType = VOTE_UNKNOWN;
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			const char *pReason = pMsg->m_Reason[0] ? pMsg->m_Reason : "No reason given";

			if(str_comp_nocase(pMsg->m_Type, "option") == 0)
			{
				CVoteOptionServer *pOption = m_pVoteOptionFirst;
				while(pOption)
				{
					if(str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
					{
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
						str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						char aBuf[128];
						str_format(aBuf, sizeof(aBuf),
							"'%d:%s' voted %s '%s' reason='%s' cmd='%s' force=%d",
							ClientID, Server()->ClientName(ClientID), pMsg->m_Type,
							aDesc, pReason, aCmd, pMsg->m_Force
						);
						Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
						if(pMsg->m_Force)
						{
							Server()->SetRconCID(ClientID);
							Console()->ExecuteLine(aCmd);
							Server()->SetRconCID(IServer::RCON_CID_SERV);
							ForceVote(VOTE_START_OP, aDesc, pReason);
							return;
						}

						if(!m_pController->OnCallvoteOption(ClientID, aDesc, aCmd, pReason))
							return;

						m_VoteType = VOTE_START_OP;
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
					return;
			}
			else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
			{
				if(!g_Config.m_SvVoteKick || m_pController->GetRealPlayerNum() < g_Config.m_SvVoteKickMin)
					return;

				int KickID = str_toint(pMsg->m_Value);
				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID] || KickID == ClientID)
					return;
				else if(Server()->IsAuthed(KickID))
				{
					// ignore return value and abort vote.
					m_pController->OnCallvoteBan(ClientID, KickID, pReason);
					return;
				}

				str_format(aDesc, sizeof(aDesc), "%2d: %s", KickID, Server()->ClientName(KickID));
				if (!g_Config.m_SvVoteKickBantime)
					str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
				}
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf),
					"'%d:%s' voted %s '%d:%s' reason='%s' cmd='%s' force=%d",
					ClientID, Server()->ClientName(ClientID), pMsg->m_Type,
					KickID, Server()->ClientName(KickID), pReason, aCmd, pMsg->m_Force
				);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
				if(pMsg->m_Force)
				{
					Server()->SetRconCID(ClientID);
					Console()->ExecuteLine(aCmd);
					Server()->SetRconCID(IServer::RCON_CID_SERV);
					return;
				}

				if(!m_pController->OnCallvoteBan(ClientID, KickID, pReason))
					return;

				m_VoteType = VOTE_START_KICK;
				m_VoteClientID = KickID;
			}
			else if(str_comp_nocase(pMsg->m_Type, "spectate") == 0)
			{
				if(!g_Config.m_SvVoteSpectate)
					return;

				int SpectateID = str_toint(pMsg->m_Value);
				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS || SpectateID == ClientID)
					return;

				str_format(aDesc, sizeof(aDesc), "%2d: %s", SpectateID, Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf),
					"'%d:%s' voted %s '%d:%s' reason='%s' cmd='%s' force=%d",
					ClientID, Server()->ClientName(ClientID), pMsg->m_Type,
					SpectateID, Server()->ClientName(SpectateID), pReason, aCmd, pMsg->m_Force
				);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
				if(pMsg->m_Force)
				{
					Server()->SetRconCID(ClientID);
					Console()->ExecuteLine(aCmd);
					Server()->SetRconCID(IServer::RCON_CID_SERV);
					ForceVote(VOTE_START_SPEC, aDesc, pReason);
					return;
				}

				if(!m_pController->OnCallvoteSpectate(ClientID, SpectateID, pReason))
					return;

				m_VoteType = VOTE_START_SPEC;
				m_VoteClientID = SpectateID;
			}

			if(m_VoteType != VOTE_UNKNOWN)
			{
				m_VoteCreator = ClientID;
				StartVote(aDesc, aCmd, pReason);
				pPlayer->m_Vote = 1;
				pPlayer->m_VotePos = m_VotePos = 1;
				pPlayer->m_LastVoteCall = Now;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(!m_VoteCloseTime)
				return;

			if(pPlayer->m_Vote == 0)
			{
				CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
				if(!pMsg->m_Vote)
					return;

				pPlayer->m_Vote = pMsg->m_Vote;
				pPlayer->m_VotePos = ++m_VotePos;
				m_VoteUpdate = true;
			}
			else if(m_VoteCreator == pPlayer->GetCID())
			{
				CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
				if(pMsg->m_Vote != -1 || m_VoteCancelTime<time_get())
					return;

				m_VoteCloseTime = -1;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SETTEAM && m_pController->IsTeamChangeAllowed())
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if(pPlayer->GetTeam() == pMsg->m_Team ||
				(g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam+Server()->TickSpeed()*3 > Server()->Tick()) ||
				(pMsg->m_Team != TEAM_SPECTATORS && m_LockTeams) || pPlayer->m_TeamChangeTick > Server()->Tick())
				return;

			pPlayer->m_LastSetTeam = Server()->Tick();

			// Switch team on given client and kill/respawn him
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID) && m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
			{
				if(pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
					m_VoteUpdate = true;
				pPlayer->m_TeamChangeTick = Server()->Tick()+Server()->TickSpeed()*3;
				m_pController->DoTeamChange(pPlayer, pMsg->m_Team);
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode+Server()->TickSpeed() > Server()->Tick())
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			if(!pPlayer->SetSpectatorID(pMsg->m_SpecMode, pMsg->m_SpectatorID))
				SendGameMsg(GAMEMSG_SPEC_INVALIDID, ClientID);
		}
		else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote+Server()->TickSpeed()*3 > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();

			SendEmoticon(ClientID, pMsg->m_Emoticon);
		}
		else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			

			// killing yourself is only allowed every x seconds.
			int cooldownSecondsLeft = ((pPlayer->m_LastKill+ (Server()->TickSpeed() * g_Config.m_SvSuicideCooldown) - Server()->Tick()) / Server()->TickSpeed());

			if (pPlayer->m_LastKill && pPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				if (cooldownSecondsLeft > 0 && pPlayer->GetNumCurrentlyCaughtPlayers() == 0)
				{
					// if cooldown not yet reached
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%d second%s left before you can kill yourself again.", cooldownSecondsLeft, cooldownSecondsLeft == 1 ? "" : "s");
					
					// don't send too many mesages if the player keeps on spamming the suicide button.
					if(Server()->Tick() % Server()->TickSpeed() < Server()->TickSpeed() / 10)
						SendServerMessage(pPlayer->GetCID(), aBuf);
					return;
				} 
				else if (pPlayer->GetNumCurrentlyCaughtPlayers() == 0)
				{
					// only prevent suicides, when a player actually does one.
					pPlayer->m_LastKill = Server()->Tick();
				}
			}
			else
			{
				// no previous suicide
				pPlayer->m_LastKill = Server()->Tick();
			}

			// kill yourself
			pPlayer->KillCharacter(WEAPON_SELF);
		}
		else if (MsgID == NETMSGTYPE_CL_READYCHANGE)
		{
			if(pPlayer->m_LastReadyChange && pPlayer->m_LastReadyChange+Server()->TickSpeed()*1 > Server()->Tick())
				return;

			pPlayer->m_LastReadyChange = Server()->Tick();
			m_pController->OnPlayerReadyChange(pPlayer);
		}
		else if(MsgID == NETMSGTYPE_CL_SKINCHANGE)
		{
			if(pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo+Server()->TickSpeed()*5 > Server()->Tick())
				return;

			pPlayer->m_LastChangeInfo = Server()->Tick();
			CNetMsg_Cl_SkinChange *pMsg = (CNetMsg_Cl_SkinChange *)pRawMsg;

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_copy(pPlayer->m_TeeInfos.m_aaSkinPartNames[p], pMsg->m_apSkinPartNames[p], 24);
				pPlayer->m_TeeInfos.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
				pPlayer->m_TeeInfos.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
			}

			// update all clients
			for(int i : PlayerIDs())
			{
				if(!m_apPlayers[i] || (!Server()->ClientIngame(i) && !m_apPlayers[i]->IsDummy()) || Server()->GetClientVersion(i) < MIN_SKINCHANGE_CLIENTVERSION)
					continue;

				SendSkinChange(pPlayer->GetCID(), i);
			}

			m_pController->OnPlayerInfoChange(pPlayer);
		}
		else if (MsgID == NETMSGTYPE_CL_COMMAND)
		{
			CNetMsg_Cl_Command *pMsg = (CNetMsg_Cl_Command*)pRawMsg;
			m_pController->OnPlayerCommand(pPlayer, pMsg->m_Name, pMsg->m_Arguments);
		}
	}
	else
	{
		if (MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if(pPlayer->m_IsReadyToEnter)
				return;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set start infos
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_copy(pPlayer->m_TeeInfos.m_aaSkinPartNames[p], pMsg->m_apSkinPartNames[p], 24);
				pPlayer->m_TeeInfos.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
				pPlayer->m_TeeInfos.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
			}

			m_pController->OnPlayerInfoChange(pPlayer);

			// send vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			CVoteOptionServer *pCurrent = m_pVoteOptionFirst;
			while(pCurrent)
			{
				// count options for actual packet
				int NumOptions = 0;
				for(CVoteOptionServer *p = pCurrent; p && NumOptions < MAX_VOTE_OPTION_ADD; p = p->m_pNext, ++NumOptions);

				// pack and send vote list packet
				CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
				Msg.AddInt(NumOptions);
				while(pCurrent && NumOptions--)
				{
					Msg.AddString(pCurrent->m_aDescription, VOTE_DESC_LENGTH);
					pCurrent = pCurrent->m_pNext;
				}
				Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
			}

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReadyToEnter = true;
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
		}
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if(pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	if(pResult->NumArguments())
		pSelf->m_pController->DoPause(clamp(pResult->GetInteger(0), -1, 1000));
	else
		pSelf->m_pController->DoPause(pSelf->m_pController->IsGamePaused() ? 0 : IGameController::TIMER_INFINITE);
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
		pSelf->m_pController->DoWarmup(clamp(pResult->GetInteger(0), -1, 1000));
	else
		pSelf->m_pController->DoWarmup(0);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CHAT_ALL, -1, pResult->GetString(0));
}

void CGameContext::ConBroadcast(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments()>2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID] || !pSelf->m_pController->CanJoinTeam(Team, ClientID))
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick()+pSelf->Server()->TickSpeed()*Delay*60;
	pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[ClientID], Team);
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	pSelf->SendGameMsg(GAMEMSG_TEAM_ALL, Team, -1);

	for(int i : pSelf->PlayerIDs())
		if(pSelf->m_apPlayers[i] && pSelf->m_pController->CanJoinTeam(Team, i))
			pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[i], Team, false);
}

void CGameContext::ConSwapTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SwapTeams();
}

void CGameContext::ConShuffleTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(!pSelf->m_pController->IsTeamplay())
		return;

	int rnd = 0;
	int PlayerTeam = 0;
	int aPlayer[MAX_CLIENTS];

	for(int i : pSelf->PlayerIDs())
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			aPlayer[PlayerTeam++]=i;

	pSelf->SendGameMsg(GAMEMSG_TEAM_SHUFFLE, -1);

	//creating random permutation
	for(int i = PlayerTeam; i > 1; i--)
	{
		rnd = random_int() % i;
		int tmp = aPlayer[rnd];
		aPlayer[rnd] = aPlayer[i-1];
		aPlayer[i-1] = tmp;
	}
	//uneven Number of Players?
	rnd = PlayerTeam % 2 ? random_int() % 2 : 0;

	for(int i = 0; i < PlayerTeam; i++)
		pSelf->m_pController->DoTeamChange(pSelf->m_apPlayers[aPlayer[i]], i < (PlayerTeam+rnd)/2 ? TEAM_RED : TEAM_BLUE, false);
}

void CGameContext::ConLockTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_LockTeams ^= 1;
	pSelf->SendSettings(-1);
}

void CGameContext::ConForceTeamBalance(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ForceTeamBalance();
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if(pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if(!pSelf->Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if(!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len+1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len+1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if(!pSelf->m_VoteCloseTime)
		return;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->SendMotd(-1);
	}
}

void CGameContext::ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		if(pSelf->Server()->MaxClients() < g_Config.m_SvPlayerSlots)
			g_Config.m_SvPlayerSlots = pSelf->Server()->MaxClients();
		pSelf->SendSettings(-1);
	}
}

void CGameContext::ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		if(pSelf->m_pController)
			pSelf->m_pController->CheckGameInfo();
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "?i", CFGFLAG_SERVER|CFGFLAG_STORE, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER|CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER|CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams");
	Console()->Register("lock_teams", "", CFGFLAG_SERVER, ConLockTeams, this, "Lock/unlock teams");
	Console()->Register("force_teambalance", "", CFGFLAG_SERVER, ConForceTeamBalance, this, "Force team balance");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Register("mute", "ii?r", CFGFLAG_SERVER, ConMute, this, "Mutes a player ID for x seconds with reason.");
	Console()->Register("unmute", "i", CFGFLAG_SERVER, ConUnmute, this, "Unmutes a player by #Index");
	Console()->Register("mutes", "", CFGFLAG_SERVER, ConMutes, this, "Show all mutes");

	Console()->Register("shadowmute", "ii?r", CFGFLAG_SERVER, ConShadowMute, this, "Add a player ID to the troll pit for x seconds and an optional reason,");
	Console()->Register("shadowunmute", "i", CFGFLAG_SERVER, ConShadowUnmute, this, "Remove a player by #Index from the troll pit.");
	Console()->Register("shadowmutes", "", CFGFLAG_SERVER, ConShadowMutes, this, "Show all trolls in the troll pit.");

	Console()->Register("punish", "i?i", CFGFLAG_SERVER, ConPunishPlayer, this, "Punish player for cheating, prevents him from killing others.");
	Console()->Register("unpunish", "i", CFGFLAG_SERVER, ConUnPunishPlayer, this, "Allow player to play normally again.");
	Console()->Register("punishments", "", CFGFLAG_SERVER, ConPunishedPlayers, this, "Show punished players.");

	Console()->Register("rank_reset", "i", CFGFLAG_SERVER, ConRankReset, this, "Request the reset of the 'score' & 'wins' of the player with <ID>.");

	Console()->Register("confirm_reset", "", CFGFLAG_SERVER, ConConfirmReset, this, "Confirm the reset of the previously mentioned player by 'rank_reset'");
	Console()->Register("abort_reset", "", CFGFLAG_SERVER, ConAbortReset, this, "Abort the reset of the previously mentioned player by 'rank_reset'");
	
}

void CGameContext::OnInit()
{
	// init everything
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	// HACK: only set static size for items, which were available in the first 0.7 release
	// so new items don't break the snapshot delta
	static const int OLD_NUM_NETOBJTYPES = 23;
	for(int i = 0; i < OLD_NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);

	// select gametype
	if(str_comp_nocase(g_Config.m_SvGametype, "mod") == 0)
		m_pController = new CGameControllerMOD(this);
	else if(str_comp_nocase(g_Config.m_SvGametype, "ctf") == 0)
		m_pController = new CGameControllerCTF(this);
	else if(str_comp_nocase(g_Config.m_SvGametype, "lms") == 0)
		m_pController = new CGameControllerLMS(this);
	else if(str_comp_nocase(g_Config.m_SvGametype, "lts") == 0)
		m_pController = new CGameControllerLTS(this);
	else if(str_comp_nocase(g_Config.m_SvGametype, "tdm") == 0)
		m_pController = new CGameControllerTDM(this);
	else if(str_comp_nocase(g_Config.m_SvGametype, "zcatch") == 0)
		m_pController = new CGameControllerZCATCH(this);
	else
		m_pController = new CGameControllerZCATCH(this);

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);
	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y*pTileMap->m_Width+x].m_Index;

			if(Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
				m_pController->OnEntity(Index-ENTITY_OFFSET, Pos);
			}
		}
	}

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);

	Console()->Chain("sv_vote_kick", ConchainSettingUpdate, this);
	Console()->Chain("sv_vote_kick_min", ConchainSettingUpdate, this);
	Console()->Chain("sv_vote_spectate", ConchainSettingUpdate, this);
	Console()->Chain("sv_teambalance_time", ConchainSettingUpdate, this);
	Console()->Chain("sv_player_slots", ConchainSettingUpdate, this);

	Console()->Chain("sv_scorelimit", ConchainGameinfoUpdate, this);
	Console()->Chain("sv_timelimit", ConchainGameinfoUpdate, this);
	Console()->Chain("sv_matches_per_map", ConchainGameinfoUpdate, this);

	// clamp sv_player_slots to 0..MaxClients
	if(Server()->MaxClients() < g_Config.m_SvPlayerSlots)
		g_Config.m_SvPlayerSlots = Server()->MaxClients();

#ifdef CONF_DEBUG
	// clamp dbg_dummies to 0..MaxClients-1
	if(Server()->MaxClients() <= g_Config.m_DbgDummies)
		g_Config.m_DbgDummies = Server()->MaxClients();
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies ; i++)
			OnClientConnected(Server()->MaxClients() -i-1, true, false);
	}
#endif
}

void CGameContext::OnShutdown()
{
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CNetObj_De_TuneParams *pTuneParams = static_cast<CNetObj_De_TuneParams *>(Server()->SnapNewItem(NETOBJTYPE_DE_TUNEPARAMS, 0, sizeof(CNetObj_De_TuneParams)));
		if(!pTuneParams)
			return;

		mem_copy(pTuneParams->m_aTuneParams, &m_Tuning, sizeof(pTuneParams->m_aTuneParams));
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	for(int i : PlayerIDs())
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_World.PostSnap();
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReadyToEnter;
}

bool CGameContext::IsClientPlayer(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS;
}

bool CGameContext::IsClientSpectator(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS;
}

const char *CGameContext::GameType() const { return m_pController && m_pController->GetGameType() ? m_pController->GetGameType() : ""; }
const char *CGameContext::Version() const { return GAME_VERSION; }
const char *CGameContext::NetVersion() const { return GAME_NETVERSION; }
const char *CGameContext::NetVersionHashUsed() const { return GAME_NETVERSION_HASH_FORCED; }
const char *CGameContext::NetVersionHashReal() const { return GAME_NETVERSION_HASH; }
bool CGameContext::IsVanillaGameType() const
{
	const char* pGameType = GameType();
	return str_comp_nocase(pGameType, "DM") == 0 ||
		str_comp_nocase(pGameType, "LMS") == 0 ||
		str_comp_nocase(pGameType, "LTS") == 0 || 
		str_comp_nocase(pGameType, "TDM") == 0;
}
IGameServer *CreateGameServer() { return new CGameContext; }

bool CGameContext::UnmuteID(int ClientID)
{
	char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	std::string IP{aAddrStr};

	bool success = false;
	auto it = std::remove_if(m_Mutes.begin(), m_Mutes.end(), [&IP, &success](auto& Mute) -> bool{
		if(IP == Mute.m_IP)
			success = true;

		return IP == Mute.m_IP;
	});

	m_Mutes.erase(it, m_Mutes.end());

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s has been unmuted.", Server()->ClientName(ClientID));
	SendServerMessage(-1, aBuf);
	return success;
}

bool CGameContext::UnmuteIndex(int Index)
{
	// TODO: proper logging for mutes and votebans and troll pit
	if(Index < 0 || static_cast<int>(m_Mutes.size()) - 1 < Index)
		return false;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s has been unmuted.", m_Mutes[Index].m_Nickname.c_str());
	SendServerMessage(-1, aBuf);

	m_Mutes.erase(m_Mutes.begin() + Index);
	return true;
}

void CGameContext::AddMute(const char* pIP, int Secs, std::string Nickname, std::string Reason, bool Auto)
{
	int Pos = IsMuted(pIP);
	if(Pos > -1)
		m_Mutes[Pos].m_ExpiresTick = Server()->TickSpeed() * Secs + Server()->Tick();	// overwrite mute
	else
	{
		m_Mutes.emplace_back(pIP, Server()->Tick() + Server()->TickSpeed() * Secs, Nickname, Reason);
	}

	char aBuf[128];
	if(Secs > 0)
	{
		str_format(aBuf, sizeof(aBuf), "%s has been %smuted for %d:%02d min.", Nickname.c_str(), Auto ? "auto-" : "", Secs/60, Secs%60);
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mute", aBuf);
	SendServerMessage(-1, aBuf);
		
}

void CGameContext::AddMute(int ClientID, int Secs, std::string Nickname, std::string Reason, bool Auto)
{
	char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	AddMute(aAddrStr, Secs, Nickname, Reason, Auto);
}

int CGameContext::IsMuted(const char *pIP)
{
	CleanMutes();
	int Pos = -1;
	size_t size = m_Mutes.size();
	for (size_t i = 0; i < size; i++)
	{
		const char* currentIP = m_Mutes[i].m_IP.c_str();

		if(!str_comp_num(pIP, currentIP, sizeof(currentIP)))
		{
			Pos = i;
			break;
		}
	}
	
	return Pos;
}

int CGameContext::IsMuted(int ClientID)
{
	char aIP[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
	return IsMuted(aIP);
}

void CGameContext::CleanMutes()
{
	int CurrentTick = Server()->Tick();
	auto it = std::remove_if(m_Mutes.begin(), m_Mutes.end(), [CurrentTick](CMute& lookedAtMute) -> bool
	{
		// remove from mutes if expiration tick is reached.
		return lookedAtMute.m_ExpiresTick < CurrentTick;
	});
	m_Mutes.erase(it, m_Mutes.end());
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	int CID = pResult->GetInteger(0);
	const char *Reason = pResult->GetString(2);

	if(CID < 0 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mute", "invalid client id.");
	else
		pSelf->AddMute(CID, pResult->GetInteger(1), {pSelf->Server()->ClientName(CID)}, {Reason});
}

void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	pSelf->CleanMutes();
	char aBuf[128];
	auto& Mutes = pSelf->m_Mutes;
	int Sec;
	int Size = Mutes.size();

	for(int i = 0; i < Size; i++)
	{
		Sec = (pSelf->m_Mutes.at(i).m_ExpiresTick - pSelf->Server()->Tick())/pSelf->Server()->TickSpeed();

		if(Mutes[i].m_Reason.size() > 0)
		{
			str_format(aBuf, sizeof(aBuf), "#%d: %s for %d:%02d min; Nickname: '%s' Reason: '%s'", i, Mutes[i].m_IP.c_str(), Sec/60, Sec%60, Mutes[i].m_Nickname.c_str(), Mutes[i].m_Reason.c_str());
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "#%d: %s for %d:%02d min; Nickname: '%s'", i, Mutes[i].m_IP.c_str(), Sec/60, Sec%60, Mutes[i].m_Nickname.c_str());
		}

		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mute", aBuf);
	}
	str_format(aBuf, sizeof(aBuf), "%d mute(s)", Size);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mute", aBuf);	
}


void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	int Index = pResult->GetInteger(0);

	if(Index < 0 ||  pSelf->m_Mutes.size() - 1 < static_cast<size_t>(Index))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mute", "Invalid index!");
	}
	else
	{
		pSelf->UnmuteIndex(Index);
	}
}

// returns whether the player is allowed to chat, informs the player and mutes him if needed
bool CGameContext::IsAllowedToChat(int ClientID)
{
	CPlayer* pPlayer = m_apPlayers[ClientID];

	int i = IsMuted(ClientID);
	if(i > -1)
	{
		char aBuf[48];
		int ExpiresSeconds = (m_Mutes[i].m_ExpiresTick - Server()->Tick())/Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "You are muted for %d:%02d min.", ExpiresSeconds/60, ExpiresSeconds%60);
		SendServerMessage(ClientID, aBuf);
		return false;
	}
	//mute the player if he's spamming
	else if(g_Config.m_SvMuteDuration)
	{
		pPlayer->m_ChatTicks += g_Config.m_SvChatValue;
		if (pPlayer->m_ChatTicks > g_Config.m_SvChatThreshold)
		{
			// auto mute player.
			AddMute(ClientID, g_Config.m_SvMuteDuration, {Server()->ClientName(ClientID)}, "Automatically muted.", true);
			pPlayer->m_ChatTicks = 0;
			return false;
		}
	}
	return true;
}

int CGameContext::IsInTrollPit(const char *pIP) 
{
	// remove expired trolls
	CleanTrollPit();
	dbg_msg("IsInTrollPit", "%s: troll pit size %lu", pIP, m_TrollPit.size());
	int pos = -1;
	for (auto& troll : m_TrollPit)
	{	
		pos++;
		const char* currentIP = troll.m_IP.c_str();
		if(!str_comp_num(pIP, currentIP, sizeof(currentIP)))
		{
			dbg_msg("found troll", "found matching troll at pos %d", pos);
			return pos;
		}
	}
	return -1;
}
int CGameContext::IsInTrollPit(int ClientID)
{
	char aIP[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
	return IsInTrollPit(aIP);
}

std::vector<int> CGameContext::GetIngameTrolls() {
	std::vector <int> trolls;
	trolls.reserve(2);
	for (auto ID : PlayerIDs())
	{
		// position must not be -1
		if (IsInTrollPit(ID) != -1) {
			trolls.push_back(ID);
		}
	}

	return trolls;
}

bool CGameContext::AddToTrollPit(const char* pIP, int Secs, std::string Nickname, std::string Reason)
{
	int pos = IsInTrollPit(pIP);
	long expiresAt = Server()->Tick() + Server()->TickSpeed() * Secs;
	bool updated = false;
	if(pos > -1)
		// already in troll pit, update if new longer troll pit time was set.
		updated = m_TrollPit[pos].UpdateIfExpiresLater(expiresAt);	// overwrite
	else
	{
		m_TrollPit.emplace_back(pIP, expiresAt, Nickname, Reason);
		updated = true;
	}
	// sort by expiration time, from smallest to biggest
	std::sort(m_TrollPit.begin(), m_TrollPit.end());

	if (updated && Secs <= 0) {
		// 0 sec -> directly expires and can be removed
		CleanTrollPit();
	}

	// make ingame player a troll if there is an ingame player
	for (int ID : PlayerIDs())
	{
		char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
		Server()->GetClientAddr(ID, aAddrStr, sizeof(aAddrStr));
		if(!str_comp_num(pIP, aAddrStr, sizeof(aAddrStr)))
		{
			if (m_apPlayers[ID]) 
			{
				m_apPlayers[ID]->SetTroll();
			}
			return updated;
		}
	}
	
	return updated;
}
bool CGameContext::AddToTrollPit(int ClientID, int Secs, std::string Nickname, std::string Reason)
{
	char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	return AddToTrollPit(aAddrStr, Secs, Nickname, Reason);
}

bool CGameContext::RemoveFromTrollPitIndex(int Index)
{
	if(Index < 0 || static_cast<int>(m_TrollPit.size()) - 1 < Index)
		return false;

	m_TrollPit.erase(m_TrollPit.begin() + Index);

	// sort by expiration time, from smallest to biggest
	std::sort(m_TrollPit.begin(), m_TrollPit.end());

	return true;
}

bool CGameContext::RemoveFromTrollPitID(int ClientID)
{
	char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	std::string IP{aAddrStr};

	bool success = false;
	auto it = std::remove_if(m_TrollPit.begin(), m_TrollPit.end(), [&IP, &success](auto& troll) -> bool{
		
		bool foundIP = (IP == troll.m_IP);
		if (foundIP)
			success = true;

		return foundIP;
	});
	m_TrollPit.erase(it, m_TrollPit.end());
	
	// sort by expiration time, from smallest to biggest
	std::sort(m_TrollPit.begin(), m_TrollPit.end());
	
	return success;
}

void CGameContext::CleanTrollPit() 
{
	// the troll pit is sorted by expiration time, so we need to only 
	// remove the first trolls until we hit a trol whose troll pit time has not expied, yet.
	// the sorting happens when a new player is added to the troll pit.
	long currentTick = Server()->Tick();
	while (!m_TrollPit.empty() &&  m_TrollPit.front().IsExpired(currentTick)) {
		// remove first element
		CTroll& troll = m_TrollPit.front();
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' (addr=%s) was removed from the troll pit, expired.", troll.m_Nickname.c_str(), troll.m_IP.c_str());
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", aBuf);
		m_TrollPit.erase(m_TrollPit.begin());


		// make ingame player  not a troll anymore
		for (int ID : PlayerIDs())
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			Server()->GetClientAddr(ID, aAddrStr, sizeof(aAddrStr));
			if(!str_comp_num(troll.m_IP.c_str(), aAddrStr, sizeof(aAddrStr)))
			{
				if (m_apPlayers[ID]) 
				{
					m_apPlayers[ID]->RemoveTroll();
				}
				return;
			}
		}	
	}
}


void CGameContext::ConShadowMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	int ID = pResult->GetInteger(0);
	int Seconds = pResult->GetInteger(1);
	const char *Reason = pResult->GetString(2);
	CPlayer* pPlayer = pSelf->m_apPlayers[ID];

	if(ID < 0 || ID >= MAX_CLIENTS || !pPlayer)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", "invalid client id.");
	}
	else
	{
		std::string Nickname{pSelf->Server()->ClientName(ID)};
		char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
		pSelf->Server()->GetClientAddr(ID, aAddrStr, sizeof(aAddrStr));
		std::string IP{aAddrStr};


		bool added = pSelf->AddToTrollPit(ID, Seconds, Nickname, {Reason});
		if (!added) {
			return;
		}
		pPlayer->SetTroll();

		char aBuf[128];
		// logging
		str_format(aBuf, sizeof(aBuf), "'%s' (addr=%s) was added to the troll pit for %d seconds.", Nickname.c_str(), IP.c_str(), Seconds);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", aBuf);

		// server message
		str_format(aBuf, sizeof(aBuf), "'%s' joined the troll pit for  %02d:%02d min.", Nickname.c_str(), Seconds/60, Seconds%60);
		pSelf->SendServerMessageToEveryoneExcept(pSelf->GetIngameTrolls(), aBuf);
	}
}

void CGameContext::ConShadowMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	
	// remove expired trolls
	pSelf->CleanTrollPit();
	
	char aBuf[128];
	auto& Trolls = pSelf->m_TrollPit;	
	int Size = Trolls.size();

	for(int i = 0; i < Size; i++)
	{
		int Sec = Trolls[i].ExpiresInSecs(pSelf->Server()->Tick());
		std::string& IP = Trolls[i].m_IP;
		std::string& Nickname = Trolls[i].m_Nickname;
		std::string& Reason = Trolls[i].m_Reason;

		if(Reason.size() > 0)
		{
			str_format(aBuf, sizeof(aBuf), "#%d: %s for %d:%02d min; Nickname: '%s' Reason: '%s'", i, IP.c_str(), Sec/60, Sec%60, Nickname.c_str(), Reason.c_str());
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "#%d: %s for %d:%02d min; Nickname: '%s'", i, IP.c_str(), Sec/60, Sec%60, Nickname.c_str());
		}

		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", aBuf);
	}
	str_format(aBuf, sizeof(aBuf), "%d troll(s)", Size);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", aBuf);	
}


void CGameContext::ConShadowUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	auto& Trolls = pSelf->m_TrollPit;	
	int Index = pResult->GetInteger(0);

	if(Index < 0 ||  Trolls.size() - 1 < static_cast<size_t>(Index))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", "Invalid index!");
		return;
	}
	
	CTroll troll = Trolls.at(Index);
	pSelf->RemoveFromTrollPitIndex(Index);
	char aBuf[128];

	str_format(aBuf, sizeof(aBuf), "'%s' (addr=%s) was removed from the troll pit.", 
	troll.m_Nickname.c_str(), troll.m_IP.c_str());
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trollpit", aBuf);

	str_format(aBuf, sizeof(aBuf), "'%s' was removed from the troll pit.", 
	troll.m_Nickname.c_str());
	pSelf->SendServerMessageToEveryoneExcept(pSelf->GetIngameTrolls(), aBuf);
}



void CGameContext::AddPlayer(int ClientID) 
{
	m_PlayerIDs.insert(m_PlayerIDs.end(), ClientID); 
	std::sort(m_PlayerIDs.begin(), m_PlayerIDs.end());
}

void CGameContext::RemovePlayer(int ClientID) 
{ 
	m_PlayerIDs.erase(std::remove(m_PlayerIDs.begin(), m_PlayerIDs.end(), ClientID), m_PlayerIDs.end());
	std::sort(m_PlayerIDs.begin(), m_PlayerIDs.end());
}

const std::vector<int>& CGameContext::PlayerIDs() 
{ 
	return m_PlayerIDs; 
}


void CGameContext::ConPunishPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;

	if(pResult->NumArguments() < 0 || 2 < pResult->NumArguments())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "punish", "invalid usage, please pass one or two arguments");
		return;
	}
		
	int ClientID = pResult->GetInteger(0);
	int Level = 1;	// default punishment level

	if(pResult->NumArguments() == 2)
		Level = pResult->GetInteger(1);

	if(!pSelf->m_apPlayers[ClientID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "punish", "Invalid ID!");
		return;
	}

	if (Level < 1)
	{
		pSelf->m_apPlayers[ClientID]->SetPunishmentLevel(CPlayer::PunishmentLevel::PROJECTILES_DONT_KILL);
		Level = 1;
	}
	else
	{
		pSelf->m_apPlayers[ClientID]->SetPunishmentLevel((CPlayer::PunishmentLevel)Level);
	}
	
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' has been punished for cheating.(Level %d)", pSelf->Server()->ClientName(ClientID), Level);
	for (int ID : pSelf->PlayerIDs())
	{
		if(ID == ClientID)
			continue;
		
		pSelf->SendServerMessage(ID, aBuf);
	}	
}


void CGameContext::ConUnPunishPlayer(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	int ClientID = pResult->GetInteger(0);

	if(!pSelf->m_apPlayers[ClientID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "punish", "Invalid ID!");
		return;
	}

	// reset punishment
	pSelf->m_apPlayers[ClientID]->SetPunishmentLevel(CPlayer::PunishmentLevel::NONE);
	
	
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "'%s' has been pardoned.", pSelf->Server()->ClientName(ClientID));
	for (int ID : pSelf->PlayerIDs())
	{
		pSelf->SendServerMessage(ID, aBuf);
	}
}


void CGameContext::ConPunishedPlayers(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "punish", "Punished Players:");
	char aBuf[128];

	for (int ID : pSelf->PlayerIDs())
	{
		CPlayer::PunishmentLevel Level;
		if(pSelf->m_apPlayers[ID])
		{	
			Level = pSelf->m_apPlayers[ID]->GetPunishmentLevel();
			if(Level > CPlayer::PunishmentLevel::NONE)
			{
				str_format(aBuf, sizeof(aBuf), "ID: %d Level: %d '%s' ", ID, Level, pSelf->Server()->ClientName(ID));
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "punish", aBuf);
			}
		}	
	}
}

// Only works in zCatch, as only registered for zCatch
void CGameContext::ConRankReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	IServer *pServer = pSelf->Server();
	CGameControllerZCATCH* pControllerZCATCH = static_cast<CGameControllerZCATCH*>(pSelf->m_pController);

	int ClientID = pResult->GetInteger(0);

	if(!pSelf->m_apPlayers[ClientID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "Invalid ID!");
		return;
	}

	std::string Nickname = std::string(pServer->ClientName(ClientID));
	
	pControllerZCATCH->m_DeletionRequest.RequestDeletion(
		CRankDeletionRequest::DeletionType::SCORE_AND_WIN_RESET, 
		Nickname, 
		pServer->Tick()
	);

	std::stringstream ss;
	ss << "Please 'confirm_reset' or 'abort_reset' the RESET of 'Score' and 'Wins' values of the player '" << Nickname << "'";
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", ss.str().c_str());
	
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "You have 60 seconds to respond, this will be aborted automatically otherwise.");
}

void CGameContext::ConConfirmReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	IServer *pServer = pSelf->Server();
	CGameControllerZCATCH* pControllerZCATCH = static_cast<CGameControllerZCATCH*>(pSelf->m_pController);

	std::string Nickname = pControllerZCATCH->m_DeletionRequest.Nickname();	

	try
	{
		pControllerZCATCH->m_DeletionRequest.Confirm(pServer->Tick());
		std::stringstream ss;
		ss << "Successfully reset the 'Score' and 'Wins' of '" << Nickname << "'";
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", ss.str().c_str());
	}
	catch(const std::exception& e)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", e.what());
	}
}

void CGameContext::ConAbortReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext*)pUserData;
	IServer *pServer = pSelf->Server();
	CGameControllerZCATCH* pControllerZCATCH = static_cast<CGameControllerZCATCH*>(pSelf->m_pController);

	std::string Nickname = pControllerZCATCH->m_DeletionRequest.Nickname();	

	try
	{
		pControllerZCATCH->m_DeletionRequest.Abort(pServer->Tick());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "Aborted rank reset!");
	}
	catch(const std::exception& e)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", e.what());
	}
}


