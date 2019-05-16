/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */
#include <new>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/console.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/collision.h>
#include <game/gamecore.h>
/*#include "gamemodes/dm.h"
#include "gamemodes/tdm.h"
#include "gamemodes/ctf.h"
#include "gamemodes/mod.h"*/
#include "gamemodes/zcatch.h"


// needed for server.h include to access
// server stuff like admin status etc..
#include <engine/masterserver.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/snapshot.h>
#include <engine/server/register.h>
#include <engine/server/server.h>
#include <algorithm>
#include <cmath>

#include <bitset>
#include <utility>

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
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;

	if(Resetting==NO_RESET)
	{
		m_pVoteOptionHeap = new CHeap();
		
		/* ranking system */
		m_RankingDb = NULL;
	}
	
	for(int i = 0; i < MAX_MUTES; i++)
		m_aMutes[i].m_aIP[0] = 0;
	
	// zCatch/TeeVi: hard mode
	m_HardModes.clear();
	m_HardModes.push_back({"ammo210", false, true});
	m_HardModes.push_back({"ammo15", false, true});
	m_HardModes.push_back({"overheat", true, true});
	m_HardModes.push_back({"hookkill", true, true});
	m_HardModes.push_back({"fail0", false, true});
	m_HardModes.push_back({"fail3", false, true});
	m_HardModes.push_back({"5s", true, true});
	m_HardModes.push_back({"10s", true, true});
	m_HardModes.push_back({"20s", true, true});
	m_HardModes.push_back({"double", true, true});

    m_TeeHistorianActive = false;
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
	
	WaitForFutures();
	if (!m_Resetting)
	{
		delete m_pVoteOptionHeap;
		
		/* close ranking db */
		if (RankingEnabled())
		{
			sqlite3_close(m_RankingDb);
		}
	}
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;
	sqlite3 *rankingDb = m_RankingDb;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
	m_RankingDb = rankingDb;
}


class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount)
{
	float a = 3 * 3.14159f / 2 + Angle;
	//float a = get_angle(dir);
	float s = a-pi/3;
	float e = a+pi/3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, float(i+1)/float(Amount+2));
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd));
		if(pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f*256.0f);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 Pos)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, bool limitVictims, const bool *victims, int ownerLastDieTickBeforceFiring)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	if (!NoDamage)
	{
		// deal damage
		CCharacter *apEnts[MAX_CLIENTS];
		float Radius = 135.0f;
		float InnerRadius = 48.0f;
		int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		bool someoneWasHit = false;
		for(int i = 0; i < Num; i++)
		{
			if(!limitVictims || victims[apEnts[i]->GetPlayer()->GetCID()])
			{
				vec2 Diff = apEnts[i]->m_Pos - Pos;
				vec2 ForceDir(0,1);
				float l = length(Diff);
				if(l)
					ForceDir = normalize(Diff);
				l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
				float Dmg = 6 * l;
				if((int)Dmg)
				{
					apEnts[i]->m_KillerLastDieTickBeforceFiring = ownerLastDieTickBeforceFiring;
					apEnts[i]->TakeDamage(ForceDir*Dmg*2, (int)Dmg, Owner, Weapon);
					someoneWasHit = true;
				}
			}
		}
		// give the owner the ammo back if he hit someone else or himself (like rocketjump)
		if(someoneWasHit)
		{
			CCharacter *ownerChar = GetPlayerChar(Owner);
			if(ownerChar)
				ownerChar->GiveAmmo(Weapon, 1);
		}
		else
		{
			// zCatch/TeeVi: hard mode
			auto ownerChar = GetPlayerChar(Owner);
			if(ownerChar && ownerLastDieTickBeforceFiring == ownerChar->GetPlayer()->m_DieTick)
				ownerChar->GetPlayer()->HardModeFailedShot();
		}
	}
}

/*
void create_smoke(vec2 Pos)
{
	// create the event
	EV_EXPLOSION *pEvent = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(pEvent)
	{
		pEvent->x = (int)Pos.x;
		pEvent->y = (int)Pos.y;
	}
}*/

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

void CGameContext::CreateSound(vec2 Pos, int Sound, int Mask)
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

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if (Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	if(Target == -2)
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	else
	{
		int Flag = MSGFLAG_VITAL;
		if(Target != -1)
			Flag |= MSGFLAG_NORECORD;
		Server()->SendPackMsg(&Msg, Flag, Target);
	}
}


void CGameContext::SendChatTarget(int To, const char *pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
}


void CGameContext::SendPrivateMessage(int From, int To, const char *pText)
{
	// prepare message
	CNetMsg_Sv_Chat M;
	M.m_Team = CHAT_SPEC;
	M.m_ClientID = From;
	M.m_pMessage = pText;
	
	Server()->SendPackMsg(&M, MSGFLAG_VITAL, From);
	Server()->SendPackMsg(&M, MSGFLAG_VITAL, To);
}


void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText)
{
	char aBuf[256];
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Team!=CHAT_ALL?"teamchat":"chat", aBuf);

	if(Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	}
	else
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			//if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team)
			if(m_apPlayers[i] && ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS && m_apPlayers[ChatterClientID] && m_apPlayers[ChatterClientID]->m_SpecExplicit == m_apPlayers[i]->m_SpecExplicit)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
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


void CGameContext::SendBroadcast(const char *pText, int ClientID)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq()*25;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(-1);
	m_VoteUpdate = true;
}


void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if(m_VoteCloseTime)
	{
		Msg.m_Timeout = (m_VoteCloseTime-time_get())/time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
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

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime && ((!str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientID) ||
		(!str_comp_num(m_aVoteCommand, "set_team ", 9) && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteCloseTime = -1;
}


void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if(!m_pController)
		return;

	if(	str_comp(m_pController->m_pGameType, "DM")==0 ||
		str_comp(m_pController->m_pGameType, "TDM")==0 ||
		str_comp(m_pController->m_pGameType, "CTF")==0)
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
	
	SendChat(-1, CGameContext::CHAT_ALL, "Teams were swapped");

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			m_apPlayers[i]->SetTeam(m_apPlayers[i]->GetTeam()^1, false);
	}

	(void)m_pController->CheckTeamBalance();
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	if(m_TeeHistorianActive)
	{
		int Error = aio_error(m_pTeeHistorianFile);
		if(Error)
		{
			dbg_msg("teehistorian", "error writing to file, err=%d", Error);
			Server()->SetErrorShutdown("teehistorian io error");
		}

		if(!m_TeeHistorian.Starting())
		{
			m_TeeHistorian.EndInputs();
			m_TeeHistorian.EndTick();
		}
		m_TeeHistorian.BeginTick(Server()->Tick());
		m_TeeHistorian.BeginPlayers();
	}

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	if (m_TeeHistorianActive)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i] && m_apPlayers[i]->GetCharacter())
			{
				CNetObj_CharacterCore Char;
				m_apPlayers[i]->GetCharacter()->GetCore().Write(&Char);
				m_TeeHistorian.RecordPlayer(i, &Char);
			}
			else
			{
				m_TeeHistorian.RecordDeadPlayer(i);
			}
		}
		m_TeeHistorian.EndPlayers();
		m_TeeHistorian.BeginInputs();
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
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
		{
			SendChat(-1, CGameContext::CHAT_ALL, "Vote aborted");
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					/* zCatch - Allow voting from players in spectators (needed or the last 2 players ingame can kick the whole server),
					 * but deny votes from players who are explicit in spec
					*/
					if(!m_apPlayers[i] || m_apPlayers[i]->m_SpecExplicit || aVoteChecked[i])	// don't count in votes by spectators
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

				if(Yes >= Total/2+1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= (Total+1)/2)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote passed");

				if(m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChat(-1, CGameContext::CHAT_ALL, "Vote failed");
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}
	
	// info messages
	// execute if interval is given and message interval is due, respecting the pause
	if(Server()->GetInfoTextInterval() > 0
		&& ((Server()->Tick() % Server()->GetInfoTextInterval()) - Server()->GetInfoTextIntervalPause()) % Server()->GetInfoTextMsgInterval() == 0)
	{
		SendChat(-1, CGameContext::CHAT_ALL, Server()->GetNextInfoText().c_str());
	}

	// bot detection
	// it is based on the behaviour of some bots to shoot at a player's _exact_ position
	// check each player, check only if an admin is online
	if(g_Config.m_SvBotDetection && Server()->GetNumLoggedInAdmins())
	{
		char aBuf[128], bBuf[64];
		const vec2 *pos, *posVictim;
		float d, precision;
		CCharacter *ci, *cj;
		CPlayer *p;
		
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
		
			// abort if player is not ingame or already detected as a bot
			if(!(p = m_apPlayers[i]) || p->m_IsAimBot || !(ci = GetPlayerChar(i)))
				continue;
			
			// check against every other player
			for(int j = 0; j < MAX_CLIENTS; ++j)
			{

				if(j != i && (cj = GetPlayerChar(j)))
				{
					int indexAdd = 0;
					vec2 target(p->m_LatestActivity.m_TargetX, p->m_LatestActivity.m_TargetY);
					
					// fast aiming bot detection
					if(g_Config.m_SvBotDetection&BOT_DETECTION_FAST_AIM
						&& p->m_AimBotTargetSpeed > 300.0 // only fast movements
						&& (d = cj->HowCloseToXRecently(ci->m_Pos + target, posVictim, p->m_AimBotLastDetection)) < 16.0
						&& (precision = p->m_AimBotTargetSpeed * (256.0 - d * d)) >= 50000.0
						&& !( // don't detect same constellation twice
							ci->m_Pos == p->m_AimBotLastDetectionPos
							&& *posVictim == p->m_AimBotLastDetectionPosVictim
						)
					)//if
					{
						indexAdd = min(3, (int)(precision / 50000));
						p->m_AimBotLastDetectionPos = ci->m_Pos;
						// prepare console output
						str_format(bBuf, sizeof(bBuf), "precision=%d speed=%d distance=%d", (int)precision, (int)p->m_AimBotTargetSpeed, (int)d);
					}
					
					// follow bot detection
					else if(g_Config.m_SvBotDetection&BOT_DETECTION_FOLLOW
						&& cj->NetworkClipped(i) == 0 // needs to be in sight
						&& ci->AimedAtCharRecently(target, cj, pos, posVictim, p->m_AimBotLastDetection)
						&& !( // don't detect same constellation twice
							*pos == p->m_AimBotLastDetectionPos
							&& *posVictim == p->m_AimBotLastDetectionPosVictim
						)
						// don't detect horizontal dragging
						&& !(
							pos->y == p->m_AimBotLastDetectionPos.y
							&& posVictim->y == p->m_AimBotLastDetectionPosVictim.y
						)
					)//if
					{
						indexAdd = 1;
						p->m_AimBotLastDetectionPos = *pos;
						// prepare console output
						bBuf[0] = 0;
					}
					
					// detected
					if(indexAdd > 0)
					{
						p->m_AimBotLastDetection = Server()->Tick();
						p->m_AimBotLastDetectionPosVictim = *posVictim;
						p->m_AimBotIndex += indexAdd;
						p->m_AimBotRange = max(p->m_AimBotRange, (int)length(target));
						// log to console
						str_format(aBuf, sizeof(aBuf), "player=%d victim=%d index=%d %s", i, j, p->m_AimBotIndex, bBuf);
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "botdetect", aBuf);
						// don't check other players
						break;
					}
					
				}
			}
			
			// check if threshold is exceeded
			if(p->m_AimBotIndex >= 5)
			{
				p->m_IsAimBot = Server()->Tick();
				// alert the admins
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "+++ '%s' (id=%d,range=%d) might be botting +++", Server()->ClientName(i), i, p->m_AimBotRange);
				for(int j = 0; j < MAX_CLIENTS; ++j)
					if(Server()->IsAuthed(j))
						SendChatTarget(j, aBuf);
			}
			
			// reduce once every seconds (tolerance)
			if(((Server()->Tick() % Server()->TickSpeed()) == 0) && p->m_AimBotIndex)
			{
				if(!(--p->m_AimBotIndex))
				{
					p->m_AimBotRange = 0;
				}
			}
		}
	}

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[MAX_CLIENTS-i-1]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
	{
		CNetObj_PlayerInput *fixedInput =  (CNetObj_PlayerInput *)pInput;
		// teehistorian doesn't receive this value properly, 
		// thus we need to provide it on our own, as the client doesn't change the weapon properly.
		CCharacter *character = m_apPlayers[ClientID]->GetCharacter(); 

		// A character is returned if the character is alive and exists.
		if (character)
		{
			int currentWeapon = character->GetWeapon();
			// We want to have the current game mode's weapon 
			// to be the players default weapon.
			fixedInput->m_WantedWeapon = currentWeapon;
		}
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)fixedInput);

	}
		
    if (m_TeeHistorianActive)
        m_TeeHistorian.RecordPlayerInput(ClientID, (CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientEnter(int ClientID)
{
	CPlayer *p = m_apPlayers[ClientID];
	//world.insert_entity(&players[client_id]);
	p->Respawn();
	
	/* begin zCatch */
	CPlayer *leader = NULL;
	
	int NumReady = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(IsClientReady(i))
			NumReady++;
	}

	// sv_allow_join 1: Allow new players to join the game without need to wait for the next round
	if(g_Config.m_SvAllowJoin == 1)
	{
		p->m_SpecExplicit = (NumReady > 2);
		p->SetTeamDirect(p->m_SpecExplicit ? TEAM_SPECTATORS : m_pController->ClampTeam(1));
		SendBroadcast("You can join the game", ClientID);
	}
	// sv_allow_join 2: The player will join when the player with the most kills dies
	else if(g_Config.m_SvAllowJoin == 2)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(m_apPlayers[i] && ((leader && m_apPlayers[i]->m_zCatchNumKillsInARow > leader->m_zCatchNumKillsInARow) || (!leader && m_apPlayers[i]->m_zCatchNumKillsInARow)))
				leader = m_apPlayers[i];
		if(leader)
			leader->AddZCatchVictim(ClientID, CPlayer::ZCATCH_CAUGHT_REASON_JOINING);
		else
			p->m_SpecExplicit = false;
	}
	
	/* end zCatch */
	
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), m_pController->GetTeamName(p->GetTeam()));
	SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), p->GetTeam());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	m_VoteUpdate = true;
	
	/* zCatch begin */
	SendChatTarget(ClientID, "Welcome to zCatch! Type /info for more.");
	if(g_Config.m_SvAllowJoin == 2 && leader)
	{
		char buf[128];
		str_format(buf, sizeof(buf), "You will join the game when '%s' dies", Server()->ClientName(leader->GetCID()));
		SendChatTarget(ClientID, buf);
	}
	/* zCatch end */
}

void CGameContext::OnClientConnected(int ClientID)
{
	// Check which team the player should be on
	const int StartTeam = g_Config.m_SvTournamentMode ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientID);

	// Potential cleanup, taken from DDNet, if slot is still not taken (might as well be a double free vulnerability)
	if(m_apPlayers[ClientID])
		delete m_apPlayers[ClientID];

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, StartTeam);
	//players[client_id].init(client_id);
	//players[client_id].client_id = client_id;

	(void)m_pController->CheckTeamBalance();

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		if(ClientID >= MAX_CLIENTS-g_Config.m_DbgDummies)
			return;
	}
#endif

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientID);

	// send motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

	// access CServer through ban server
	if (!GetBanServer()->Server()->m_NetServer.HasSecurityToken(ClientID)) {
		SendChatTarget(ClientID, "Warning: You are not securely connected to this server.");
	}
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{

    if (m_apPlayers[ClientID]->m_CaughtBy > CPlayer::ZCATCH_NOT_CAUGHT)
        m_apPlayers[m_apPlayers[ClientID]->m_CaughtBy]->ReleaseZCatchVictim(ClientID);

    // if player voted something/someone and it did not pass
    // before leaving the server, voteban him for the remaining time.
    int64 Now = Server()->Tick();
    int Timeleft = m_apPlayers[ClientID]->m_LastVoteCall + Server()->TickSpeed() * 60 - Now;
    // convert to seconds
    Timeleft = static_cast<int>(Timeleft / Server()->TickSpeed());
    if (m_apPlayers[ClientID]->m_LastVoteCall && Timeleft > 0)
    {
        GetBanServer()->Server()->AddVoteban(ClientID, Timeleft);
	}


	AbortVoteKickOnDisconnect(ClientID);
	m_apPlayers[ClientID]->OnDisconnect(pReason);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	(void)m_pController->CheckTeamBalance();
	m_VoteUpdate = true;

	// update spectator modes
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_SpectatorID == ClientID)
			m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
	}
}

void CGameContext::OnClientEngineJoin(int ClientID)
{
    if (m_TeeHistorianActive)
    {
        m_TeeHistorian.RecordPlayerJoin(ClientID);
    }
}

void CGameContext::OnClientEngineDrop(int ClientID, const char *pReason)
{
    if (m_TeeHistorianActive)
    {
        m_TeeHistorian.RecordPlayerDrop(ClientID, pReason);
    }
}

// returns whether the player is allowed to chat, informs the player and mutes him if needed
bool CGameContext::MuteValidation(CPlayer *player)
{
	int i, ClientID = player->GetCID();
	if((i = Muted(ClientID)) > -1)
	{
		char aBuf[48];
		int Expires = (m_aMutes[i].m_Expires - Server()->Tick())/Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "You are muted for %d:%02d min.", Expires/60, Expires%60);
		SendChatTarget(ClientID, aBuf);
		return false;
	}
	//mute the player if he's spamming
	else if(g_Config.m_SvMuteDuration && ((player->m_ChatTicks += g_Config.m_SvChatValue) > g_Config.m_SvChatThreshold))
	{
		AddMute(ClientID, g_Config.m_SvMuteDuration, true);
		player->m_ChatTicks = 0;
		return false;
	}
	return true;
}

void CGameContext::OnMessage(int MsgID, CUnpacker* pUnpacker, int ClientID)
{
    void* pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
    CPlayer* pPlayer = m_apPlayers[ClientID];

    if(m_TeeHistorianActive)
	{
		if(m_NetObjHandler.TeeHistorianRecordMsg(MsgID))
		{
			m_TeeHistorian.RecordPlayerMessage(ClientID, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}
	}

    if (pPlayer)
    {
        if (!pRawMsg)
        {
            if (g_Config.m_Debug)
            {
                char aBuf[256];
                str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
                Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
            }
            // player sent a malformed message.
            pPlayer->AddWeirdMessage(MsgID);
            return;
        }

        if (MsgID == NETMSGTYPE_CL_SAY)
        {
            CNetMsg_Cl_Say* pMsg = (CNetMsg_Cl_Say*)pRawMsg;
            int Team = pMsg->m_Team;
            if (Team)
            {
                Team = pPlayer->GetTeam();
            }
            else
            {
                Team = CGameContext::CHAT_ALL;
            }

            if (g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() > Server()->Tick())
            {
                return;
            }

            pPlayer->m_LastChat = Server()->Tick();

            // check for invalid chars
            unsigned char* pMessage = (unsigned char*)pMsg->m_pMessage;
            while (*pMessage)
            {
                if (*pMessage < 32)
                {
                    *pMessage = ' ';
                }
                pMessage++;
            }

            /* begin zCatch*/
            if (!str_comp_num("/", pMsg->m_pMessage, 1))
            {
                if (!str_comp_nocase("info", pMsg->m_pMessage + 1))
                {
                    char aBuf[128];
                    str_format(aBuf, sizeof(aBuf), "zCatch %s by erd and Teetime, modified by Teelevision, maintained by jxsl13. See /help.", ZCATCH_VERSION);
                    SendChatTarget(ClientID, aBuf);
                    SendChatTarget(ClientID, "Players you catch (kill) join again when you die. Catch everyone to win.");
                    if (g_Config.m_SvLastStandingPlayers > 2)
                    {
                        str_format(aBuf, sizeof(aBuf), "If there are less than %d players, the round does not end and all players are released instead.", g_Config.m_SvLastStandingPlayers);
                        SendChatTarget(ClientID, aBuf);
                    }
                }
                else if (!str_comp_nocase("cmdlist", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "Use /help to learn about chat commands.");
                }
                else if (!str_comp_nocase("help", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "Help topics: [1] game, [2] releasing, [3] PMs, [4] ranking, [5] hard mode");
                    SendChatTarget(ClientID, "/help [1-5]: show help topic");
                    SendChatTarget(ClientID, "/info: show info about this mod");
                }
                else if (!str_comp_nocase("help 1", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "--- Help 1 / 5 ---");
                    SendChatTarget(ClientID, "Players you catch (kill) join again when you die. Catch everyone to win.");
                    SendChatTarget(ClientID, "/kills: list of players you caught");
                    SendChatTarget(ClientID, "/victims: who is waiting for your death");
                }
                else if (!str_comp_nocase("help 2", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "--- Help 2 / 5 ---");
                    SendChatTarget(ClientID, "On suicide via console the last victim is released instead. You die if there is noone to release. The console command for suicide is 'kill'.");
                }
                else if (!str_comp_nocase("help 3", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "--- Help 3 / 5 ---");
                    SendChatTarget(ClientID, "You can write private messages:");
                    SendChatTarget(ClientID, "/t <name> <msg>: write PM to <name>");
                    SendChatTarget(ClientID, "/ti <id> <msg>: write PM via ID");
                }
                else if (!str_comp_nocase("help 4", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "--- Help 4 / 5 ---");
                    if (RankingEnabled())
                    {
                        SendChatTarget(ClientID, "The ranking system saves various stats about players. The stats are updated at the end of a round and on leaving the server.");
                        SendChatTarget(ClientID, "/top <score|wins|kills|wallshotkills|deaths|kd|shots|spree|time>: display top 5 players");
                        SendChatTarget(ClientID, "/rank Displays own rank without showing it to other players.");
                        SendChatTarget(ClientID, "/rank <player name>: display own/players's rank and share it with other players.");
                        SendChatTarget(ClientID, "/stats [average|total]: display some general ranking statistics.");
                    }
                    else
                    {
                        SendChatTarget(ClientID, "The ranking system is disabled on this server.");
                    }
                }
                else if (!str_comp_nocase("help 5", pMsg->m_pMessage + 1))
                {
                    SendChatTarget(ClientID, "--- Help 5 / 5 ---");
                    SendChatTarget(ClientID, "The hard mode is an extra challenge for good players. You cannot leave it.");
                    SendChatTarget(ClientID, "/hard: join random mode");
                    SendChatTarget(ClientID, "/hard <modes>: join mode(s)");
                    SendChatTarget(ClientID, "/hard list: list all available modes");
                }
                else if (!str_comp_nocase("victims", pMsg->m_pMessage + 1))
                {
                    if (pPlayer->m_zCatchNumVictims)
                    {
                        char aBuf[256], bBuf[256];
                        CPlayer::CZCatchVictim* v = pPlayer->m_ZCatchVictims;
                        str_format(aBuf, sizeof(aBuf), "%d player(s) await your death: ", pPlayer->m_zCatchNumVictims);
                        while (v != NULL)
                        {
                            str_format(bBuf, sizeof(bBuf), (v == pPlayer->m_ZCatchVictims) ? "%s '%s'%s" : "%s, '%s'%s", aBuf, Server()->ClientName(v->ClientID), (v->Reason == CPlayer::ZCATCH_CAUGHT_REASON_JOINING) ? " (joined the game)" : "");
                            str_copy(aBuf, bBuf, sizeof(aBuf));
                            v = v->prev;
                        }
                        SendChatTarget(ClientID, aBuf);
                    }
                    else
                    {
                        SendChatTarget(ClientID, "No one awaits your death.");
                    }
                }
                else if (!str_comp_nocase("kills", pMsg->m_pMessage + 1))
                {
                    if (pPlayer->m_zCatchNumKillsInARow)
                    {
                        char aBuf[256];
                        str_format(aBuf, sizeof(aBuf), "You caught %d player(s) since your last death.", pPlayer->m_zCatchNumKillsInARow);
                        SendChatTarget(ClientID, aBuf);
                    }
                    else
                    {
                        SendChatTarget(ClientID, "You caught no one since your last death.");
                    }
                }
                // tell / PM someone privately
                else if (!str_comp_nocase_num("t ", pMsg->m_pMessage + 1, 2) || !str_comp_nocase_num("ti ", pMsg->m_pMessage + 1, 3))
                {
                    const char* recipientStart, *msgStart;
                    int recipient = -1;

                    // by name
                    if (!str_comp_nocase_num("t ", pMsg->m_pMessage + 1, 2))
                    {
                        int recipientNameLength;
                        const char* recipientName;
                        recipientStart = str_skip_whitespaces((char*)pMsg->m_pMessage + 3);
                        // check _all_ players (there might be partly identical names)
                        for (int i = 0; i < MAX_CLIENTS; ++i)
                        {
                            if (m_apPlayers[i]
                                    && (recipientName = Server()->ClientName(i))
                                    && (recipientNameLength = str_length(recipientName))
                                    && !str_comp_num(recipientName, recipientStart, recipientNameLength)
                                    && recipientStart[recipientNameLength] == ' '
                               )
                            {
                                if (recipient >= 0)
                                {
                                    SendChatTarget(ClientID, "Could not deliver private message. More than one player could be addressed.");
                                    return;
                                }
                                msgStart = recipientStart + recipientNameLength + 1;
                                recipient = i;
                            }
                        }
                    }

                    // by id
                    else if (!str_comp_nocase_num("ti ", pMsg->m_pMessage + 1, 3))
                    {
                        recipientStart = str_skip_whitespaces((char*)pMsg->m_pMessage + 4);
                        // check if int given
                        for (const char* c = recipientStart; *c != ' '; ++c)
                        {
                            if (*c < '0' || '9' < *c)
                            {
                                SendChatTarget(ClientID, "No id given, syntax is: /ti id message.");
                                return;
                            }
                        }
                        int i = str_toint(recipientStart);
                        if (i >= MAX_CLIENTS)
                        {
                            SendChatTarget(ClientID, "Invalid id, syntax is: /ti id message.");
                            return;
                        }
                        if (m_apPlayers[i])
                        {
                            recipient = i;
                            msgStart = str_skip_whitespaces(str_skip_to_whitespace((char*)recipientStart));
                        }
                    }

                    if (recipient >= 0)
                    {
                        if (MuteValidation(pPlayer))
                        {
                            // prepare message
                            const char* msgForm = "/PM -> %s / %s";
                            int len = 32 + MAX_NAME_LENGTH + str_length(msgStart);
                            char* msg = (char*)malloc(len * sizeof(char));
                            str_format(msg, len * sizeof(char), msgForm, Server()->ClientName(recipient), msgStart);

                            // send message
                            SendPrivateMessage(ClientID, recipient, msg);

                            // tidy up
                            free(msg);
                        }
                    }
                    else
                    {
                        SendChatTarget(ClientID, "Could not deliver private message. Player not found.");
                    }
                }
                /* ranking system */
                else if (RankingEnabled() && (!str_comp_nocase_num("top ", pMsg->m_pMessage + 1, 4) || !str_comp_nocase_num("top5 ", pMsg->m_pMessage + 1, 5)))
                {
                    char* category = str_skip_whitespaces((char*)pMsg->m_pMessage + 5);
                    int length = str_length(category);
                    /* trim right */
                    while (length > 0 && str_skip_whitespaces(category + length - 1) >= (category + length))
                    {
                        --length;
                        category[length] = 0;
                    }
                    m_pController->OnChatCommandTop(pPlayer, category);
                }
                else if (RankingEnabled() && (!str_comp_nocase_num("top", pMsg->m_pMessage + 1, 3) || !str_comp_nocase_num("top5", pMsg->m_pMessage + 1, 4)))
                {
                    // Firstly compare the longer strings an if nothing was matched, fallback to this
                    // simple /top or /top5 command
                    m_pController->OnChatCommandTop(pPlayer, "score");
                }

                else if (RankingEnabled() && (!str_comp_nocase("rank", pMsg->m_pMessage + 1)))
                {
                    m_pController->OnChatCommandOwnRank(pPlayer);
                }
                else if (RankingEnabled() && (!str_comp_nocase_num("rank ", pMsg->m_pMessage + 1, 5)))
                {
                    char* name = str_skip_whitespaces((char*)pMsg->m_pMessage + 6);
                    int length = str_length(name);
                    /* trim right */
                    while (length > 0 && str_skip_whitespaces(name + length - 1) >= (name + length))
                    {
                        --length;
                        name[length] = 0;
                    }
                    m_pController->OnChatCommandRank(pPlayer, name, true);
                }
                else if (!str_comp_nocase_num("stats", pMsg->m_pMessage + 1, 5) ||
                         !str_comp_nocase_num("stats ", pMsg->m_pMessage + 1, 6))
                {
                    if (RankingEnabled())
                    {
                        // either avg, average, total
                        char* cmd_name = str_skip_whitespaces((char*)pMsg->m_pMessage + 6);
                        int length = str_length(cmd_name);
                        /* trim right */
                        while (length > 0 && str_skip_whitespaces(cmd_name + length - 1) >= (cmd_name + length))
                        {
                            --length;
                            cmd_name[length] = '\0';
                        }

                        m_pController->OnChatCommandStats(pPlayer, cmd_name);
                    }
                    else
                    {
                        SendChatTarget(ClientID, "Ranking is disabled on this server, thus 'stats' is not available.");
                    }

                }

                // hard mode
                else if (g_Config.m_SvAllowHardMode == 1 && (!str_comp_nocase_num("hard", pMsg->m_pMessage + 1, 4) || !str_comp_nocase_num("hard ", pMsg->m_pMessage + 1, 5)))
                {
                    if (pPlayer->m_HardMode.m_Active)
                    {
                        SendChatTarget(ClientID, "You already are in hard mode.");
                    }
                    else
                    {
                        char* optionStart, *m = (char*)pMsg->m_pMessage + 5;

                        pPlayer->ResetHardMode();

                        // to print the hard modes in the chat later
                        auto addMode = [pPlayer](const char* mode)
                        {
                            auto d = &pPlayer->m_HardMode.m_Description;
                            if (*d[0] != 0)
                            {
                                str_append(*d, " ", sizeof(*d));
                            }
                            str_append(*d, mode, sizeof(*d));
                        };

                        // read options from the command and assign those hard modes
                        while (*m)
                        {
                            optionStart = str_skip_whitespaces(m);
                            m = str_skip_to_whitespace(optionStart);

                            // no mode is longer than this
                            if ((m - optionStart) > 30)
                            {
                                SendChatTarget(ClientID, "That mode is bullshit!");
                                return;
                            }

                            // get mode
                            char mode[32];
                            str_copy(mode, optionStart, m - optionStart + 1);

                            // add mode
                            if (!pPlayer->AddHardMode(mode))
                            {
                                // unknown mode
                                char aBuf[256];
                                str_format(aBuf, sizeof(aBuf), "'%s' is not a hard mode, dumbass!", mode);
                                SendChatTarget(ClientID, aBuf);

                                // give out list of hard modes
                                char mBuf[256], mBuf2[256];
                                mBuf[0] = 0;
                                for (auto it = m_HardModes.begin(); it != m_HardModes.end(); ++it)
                                {
                                    if ((g_Config.m_SvMode == 1 && it->laser) || (g_Config.m_SvMode == 4 && it->grenade))
                                    {
                                        str_format(mBuf2, sizeof(mBuf2), "%s %s", mBuf, it->name);
                                        str_copy(mBuf, mBuf2, sizeof(mBuf));
                                    }
                                }
                                str_format(aBuf, sizeof(aBuf), "Hard modes:%s", mBuf);
                                SendChatTarget(ClientID, aBuf);

                                return;
                            }

                            addMode(mode);
                        }

                        // none mode was added above
                        // assign random
                        if (!pPlayer->m_HardMode.m_Active)
                        {
                            addMode(pPlayer->AddRandomHardMode());
                        }

                        // player has to start over again
                        pPlayer->KillCharacter();

                        char aBuf[256];
                        str_format(aBuf, sizeof(aBuf), "'%s' entered hard mode (%s)", Server()->ClientName(ClientID), pPlayer->m_HardMode.m_Description);
                        SendChat(-1, CGameContext::CHAT_ALL, aBuf);

                    }

                    // inform player about the hard modes he got
                    char aBuf[256];
                    auto mode = &pPlayer->m_HardMode;
                    if (mode->m_ModeAmmoLimit > 0 || mode->m_ModeAmmoRegenFactor > 0)
                    {
                        str_format(aBuf, sizeof(aBuf), "Hard mode: ammo limit of %d and %dx regen time", mode->m_ModeAmmoLimit, mode->m_ModeAmmoRegenFactor);
                        SendChatTarget(ClientID, aBuf);
                    }
                    if (mode->m_ModeHookWhileKilling)
                    {
                        SendChatTarget(ClientID, "Hard mode: hook players while catching them");
                    }
                    if (mode->m_ModeWeaponOverheats.m_Active)
                    {
                        SendChatTarget(ClientID, "Hard mode: your weapon kills you if overheating");
                    }
                    if (mode->m_ModeTotalFails.m_Active)
                    {
                        str_format(aBuf, sizeof(aBuf), "Hard mode: you are allowed to fail %d shots", mode->m_ModeTotalFails.m_Max);
                        SendChatTarget(ClientID, aBuf);
                    }
                    if (mode->m_ModeKillTimelimit.m_Active)
                    {
                        str_format(aBuf, sizeof(aBuf), "Hard mode: you have %d seconds between catching players", mode->m_ModeKillTimelimit.m_TimeSeconds);
                        SendChatTarget(ClientID, aBuf);
                    }
                    if (mode->m_ModeDoubleKill.m_Active)
                    {
                        SendChatTarget(ClientID, "Hard mode: hit everyone two times in a row");
                    }
                }

                else
                {
                    SendChatTarget(ClientID, "Unknown command, try /info");
                }
            }
            else
            {
                // send to chat
                if (MuteValidation(pPlayer))
                {
                    SendChat(ClientID, Team, pMsg->m_pMessage);
                }
            }
            /* end zCatch */
        }
        else if (MsgID == NETMSGTYPE_CL_CALLVOTE)
        {
            if (g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + Server()->TickSpeed() * 3 > Server()->Tick())
            {
                return;
            }

            int64 Now = Server()->Tick();
            pPlayer->m_LastVoteTry = Now;
            // zCatch - Only people which are explicit in spectators can't vote!
            if (pPlayer->m_SpecExplicit) //zCatch
            {
                SendChatTarget(ClientID, "Spectators aren't allowed to start a vote.");
                return;
            }

            if (m_VoteCloseTime)
            {
                SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
                return;
            }

            char aChatmsg[512];

            // check voteban
            int left = Server()->ClientVotebannedTime(ClientID);
            if (left)
            {
                str_format(aChatmsg, sizeof(aChatmsg), "You are not allowed to vote for the next %d:%02d min.", left / 60, left % 60);
                SendChatTarget(ClientID, aChatmsg);
                return;
            }

            int Timeleft = pPlayer->m_LastVoteCall + Server()->TickSpeed() * 60 - Now;
            if (pPlayer->m_LastVoteCall && Timeleft > 0)
            {
                str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote", (Timeleft / Server()->TickSpeed()) + 1);
                SendChatTarget(ClientID, aChatmsg);
                return;
            }

            char aDesc[VOTE_DESC_LENGTH] = {0};
            char aCmd[VOTE_CMD_LENGTH] = {0};
            CNetMsg_Cl_CallVote* pMsg = (CNetMsg_Cl_CallVote*)pRawMsg;
            const char* pReason = pMsg->m_Reason[0] ? pMsg->m_Reason : "No reason given";

            if (str_comp_nocase(pMsg->m_Type, "option") == 0)
            {
                CVoteOptionServer* pOption = m_pVoteOptionFirst;
                while (pOption)
                {
                    if (str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
                    {
                        str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(ClientID),
                                   pOption->m_aDescription, pReason);
                        str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
                        str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
                        break;
                    }

                    pOption = pOption->m_pNext;
                }

                if (!pOption)
                {
                    str_format(aChatmsg, sizeof(aChatmsg), "'%s' isn't an option on this server", pMsg->m_Value);
                    SendChatTarget(ClientID, aChatmsg);
                    return;
                }
            }
            else if (g_Config.m_SvVoteForceReason && !pMsg->m_Reason[0])
            {
                SendChatTarget(ClientID, "You must give a reason for your vote");
                return;
            }
            else if (str_comp_nocase(pMsg->m_Type, "kick") == 0)
            {
                if (!g_Config.m_SvVoteKick)
                {
                    SendChatTarget(ClientID, "Server does not allow voting to kick players");
                    return;
                }

                if (g_Config.m_SvVoteKickMin)
                {
                    int PlayerNum = 0;
                    for (int i = 0; i < MAX_CLIENTS; ++i)
                        if (m_apPlayers[i] && !m_apPlayers[i]->m_SpecExplicit) // zCatch - Count all players which are not explicit in spectator
                        {
                            ++PlayerNum;
                        }

                    if (PlayerNum < g_Config.m_SvVoteKickMin)
                    {
                        str_format(aChatmsg, sizeof(aChatmsg), "Kick voting requires %d players on the server", g_Config.m_SvVoteKickMin);
                        SendChatTarget(ClientID, aChatmsg);
                        return;
                    }
                }

                int KickID = str_toint(pMsg->m_Value);
                if (KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
                {
                    SendChatTarget(ClientID, "Invalid client id to kick");
                    return;
                }
                if (KickID == ClientID)
                {
                    SendChatTarget(ClientID, "You can't kick yourself");
                    return;
                }
                if (Server()->IsAuthed(KickID))
                {
                    SendChatTarget(ClientID, "You can't kick admins");
                    char aBufKick[128];
                    str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you (%s)", Server()->ClientName(ClientID), pReason);
                    SendChatTarget(KickID, aBufKick);
                    return;
                }

                str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to kick '%s' (%s)", Server()->ClientName(ClientID), Server()->ClientName(KickID), pReason);
                str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
                if (!g_Config.m_SvVoteKickBantime)
                {
                    str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
                }
                else
                {
                    char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
                    Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
                    str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
                }
            }
            else if (str_comp_nocase(pMsg->m_Type, "spectate") == 0)
            {
                if (!g_Config.m_SvVoteSpectate)
                {
                    SendChatTarget(ClientID, "Server does not allow voting to move players to spectators");
                    return;
                }

                int SpectateID = str_toint(pMsg->m_Value);
                if (SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
                {
                    SendChatTarget(ClientID, "Invalid client id to move");
                    return;
                }
                if (SpectateID == ClientID)
                {
                    SendChatTarget(ClientID, "You can't move yourself");
                    return;
                }

                str_format(aChatmsg, sizeof(aChatmsg), "'%s' called for vote to move '%s' to spectators (%s)", Server()->ClientName(ClientID), Server()->ClientName(SpectateID), pReason);
                str_format(aDesc, sizeof(aDesc), "move '%s' to spectators", Server()->ClientName(SpectateID));
                str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
            }

            if (aCmd[0])
            {
                SendChat(-1, CGameContext::CHAT_ALL, aChatmsg);
                StartVote(aDesc, aCmd, pReason);
                pPlayer->m_Vote = 1;
                pPlayer->m_VotePos = m_VotePos = 1;
                m_VoteCreator = ClientID;
                pPlayer->m_LastVoteCall = Now;
            }
        }
        else if (MsgID == NETMSGTYPE_CL_VOTE)
        {
            if (!m_VoteCloseTime)
            {
                return;
            }

            if (pPlayer->m_Vote == 0)
            {
                CNetMsg_Cl_Vote* pMsg = (CNetMsg_Cl_Vote*)pRawMsg;
                if (!pMsg->m_Vote)
                {
                    return;
                }

                pPlayer->m_Vote = pMsg->m_Vote;
                pPlayer->m_VotePos = ++m_VotePos;
                m_VoteUpdate = true;
            }
        }
        else if (MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
        {
            CNetMsg_Cl_SetTeam* pMsg = (CNetMsg_Cl_SetTeam*)pRawMsg;

            if (pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * 3 > Server()->Tick()))
            {
                return;
            }

            if (pMsg->m_Team != TEAM_SPECTATORS && m_LockTeams)
            {
                pPlayer->m_LastSetTeam = Server()->Tick();
                SendBroadcast("Teams are locked", ClientID);
                return;
            }

            if (pPlayer->m_TeamChangeTick > Server()->Tick())
            {
                pPlayer->m_LastSetTeam = Server()->Tick();
                int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick()) / Server()->TickSpeed();
                char aBuf[128];
                str_format(aBuf, sizeof(aBuf), "Time to wait before changing team: %02d:%02d", TimeLeft / 60, TimeLeft % 60);
                SendBroadcast(aBuf, ClientID);
                return;
            }

            // Switch team on given client and kill/respawn him
            if (m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
            {
                if (m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
                {
                    pPlayer->m_LastSetTeam = Server()->Tick();
                    pPlayer->SetTeam(pMsg->m_Team);
                }
                else
                {
                    char aBuf[128];
                    pPlayer->m_zCatchJoinSpecWhenReleased = !pPlayer->m_zCatchJoinSpecWhenReleased;
                    str_format(aBuf, sizeof(aBuf), "You will join the %s when '%s' dies.", m_pController->GetTeamName(pPlayer->m_zCatchJoinSpecWhenReleased ? TEAM_SPECTATORS : TEAM_RED), Server()->ClientName(pPlayer->m_CaughtBy));
                    SendChatTarget(ClientID, aBuf);
                    return;
                }
            }
            else
            {
                char aBuf[128];
                str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", Server()->MaxClients() - g_Config.m_SvSpectatorSlots);
                SendBroadcast(aBuf, ClientID);
            }
        }
        else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
        {
            CNetMsg_Cl_SetSpectatorMode* pMsg = (CNetMsg_Cl_SetSpectatorMode*)pRawMsg;

            if (pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_SpectatorID == pMsg->m_SpectatorID || ClientID == pMsg->m_SpectatorID ||
                    (g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode + Server()->TickSpeed() * 3 > Server()->Tick()))
            {
                return;
            }

            pPlayer->m_LastSetSpectatorMode = Server()->Tick();
            if (pMsg->m_SpectatorID != SPEC_FREEVIEW && (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
            {
                SendChatTarget(ClientID, "Invalid spectator id used");
            }
            else
            {
                pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
            }
        }
        else if (MsgID == NETMSGTYPE_CL_STARTINFO)
        {
            if (pPlayer->m_IsReady)
            {
                return;
            }

            CNetMsg_Cl_StartInfo* pMsg = (CNetMsg_Cl_StartInfo*)pRawMsg;
            pPlayer->m_LastChangeInfo = Server()->Tick();

            // set start infos
            Server()->SetClientName(ClientID, pMsg->m_pName);
            Server()->SetClientClan(ClientID, pMsg->m_pClan);
            Server()->SetClientCountry(ClientID, pMsg->m_Country);
            str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
            pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
            pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
            pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
            m_pController->OnPlayerInfoChange(pPlayer);

            // send vote options
            CNetMsg_Sv_VoteClearOptions ClearMsg;
            Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

            CNetMsg_Sv_VoteOptionListAdd OptionMsg;
            int NumOptions = 0;
            OptionMsg.m_pDescription0 = "";
            OptionMsg.m_pDescription1 = "";
            OptionMsg.m_pDescription2 = "";
            OptionMsg.m_pDescription3 = "";
            OptionMsg.m_pDescription4 = "";
            OptionMsg.m_pDescription5 = "";
            OptionMsg.m_pDescription6 = "";
            OptionMsg.m_pDescription7 = "";
            OptionMsg.m_pDescription8 = "";
            OptionMsg.m_pDescription9 = "";
            OptionMsg.m_pDescription10 = "";
            OptionMsg.m_pDescription11 = "";
            OptionMsg.m_pDescription12 = "";
            OptionMsg.m_pDescription13 = "";
            OptionMsg.m_pDescription14 = "";
            CVoteOptionServer* pCurrent = m_pVoteOptionFirst;
            while (pCurrent)
            {
                switch (NumOptions++)
                {
                    case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
                    case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
                    case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
                    case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
                    case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
                    case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
                    case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
                    case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
                    case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
                    case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
                    case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
                    case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
                    case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
                    case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
                    case 14:
                    {
                        OptionMsg.m_pDescription14 = pCurrent->m_aDescription;
                        OptionMsg.m_NumOptions = NumOptions;
                        Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
                        OptionMsg = CNetMsg_Sv_VoteOptionListAdd();
                        NumOptions = 0;
                        OptionMsg.m_pDescription1 = "";
                        OptionMsg.m_pDescription2 = "";
                        OptionMsg.m_pDescription3 = "";
                        OptionMsg.m_pDescription4 = "";
                        OptionMsg.m_pDescription5 = "";
                        OptionMsg.m_pDescription6 = "";
                        OptionMsg.m_pDescription7 = "";
                        OptionMsg.m_pDescription8 = "";
                        OptionMsg.m_pDescription9 = "";
                        OptionMsg.m_pDescription10 = "";
                        OptionMsg.m_pDescription11 = "";
                        OptionMsg.m_pDescription12 = "";
                        OptionMsg.m_pDescription13 = "";
                        OptionMsg.m_pDescription14 = "";
                    }
                }
                pCurrent = pCurrent->m_pNext;
            }
            if (NumOptions > 0)
            {
                OptionMsg.m_NumOptions = NumOptions;
                Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
            }

            // send tuning parameters to client
            SendTuningParams(ClientID);

            // client is ready to enter
            pPlayer->m_IsReady = true;
            CNetMsg_Sv_ReadyToEnter m;
            Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
        }
        else if (MsgID == NETMSGTYPE_CL_CHANGEINFO)
        {
            if (g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo + Server()->TickSpeed() * 5 > Server()->Tick())
            {
                return;
            }

            CNetMsg_Cl_ChangeInfo* pMsg = (CNetMsg_Cl_ChangeInfo*)pRawMsg;
            pPlayer->m_LastChangeInfo = Server()->Tick();

            // set infos
            char aOldName[MAX_NAME_LENGTH];
            str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));
            Server()->SetClientName(ClientID, pMsg->m_pName);
            if (str_comp(aOldName, Server()->ClientName(ClientID)) != 0 && Muted(ClientID) == -1)
            {
                char aChatText[256];
                str_format(aChatText, sizeof(aChatText), "'%s' changed name to '%s'", aOldName, Server()->ClientName(ClientID));
                SendChat(-1, CGameContext::CHAT_ALL, aChatText);
            }
            Server()->SetClientClan(ClientID, pMsg->m_pClan);
            Server()->SetClientCountry(ClientID, pMsg->m_Country);
            str_copy(pPlayer->m_TeeInfos.m_SkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_SkinName));
            pPlayer->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
            pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
            pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
            m_pController->OnPlayerInfoChange(pPlayer);
        }
        else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
        {
            CNetMsg_Cl_Emoticon* pMsg = (CNetMsg_Cl_Emoticon*)pRawMsg;

            if (g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote + Server()->TickSpeed() * 3 > Server()->Tick())
            {
                return;
            }

            pPlayer->m_LastEmote = Server()->Tick();

            SendEmoticon(ClientID, pMsg->m_Emoticon);
        }
        else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
        {
            /* begin zCatch*/
            if (pPlayer->GetTeam() == TEAM_SPECTATORS || (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * 3 > Server()->Tick()) ||
                    (pPlayer->m_LastKillTry + Server()->TickSpeed() * 3 > Server()->Tick()))
            {
                return;
            }

            // if releasing is enabled and the player has caught someone.
            if (g_Config.m_SvAllowRelease == 1 && pPlayer->HasZCatchVictims())
            {
                int lastVictim = pPlayer->LastZCatchVictim();
                pPlayer->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL, 1, true);
                int nextToRelease = pPlayer->LastZCatchVictim();
                char aBuf[128], bBuf[128];
                str_format(bBuf, sizeof(bBuf), ", next: %s", Server()->ClientName(nextToRelease));
                str_format(aBuf, sizeof(aBuf), "You released '%s'. (%d left%s)", Server()->ClientName(lastVictim), pPlayer->m_zCatchNumVictims, pPlayer->m_zCatchNumVictims > 0 ? bBuf : "");
                SendChatTarget(ClientID, aBuf);
                str_format(aBuf, sizeof(aBuf), "You were released by '%s'.", Server()->ClientName(ClientID));
                SendChatTarget(lastVictim, aBuf);

                // console release log
                str_format(aBuf, sizeof(aBuf), "release killer='%d:%s' victim='%d:%s'" , ClientID, Server()->ClientName(ClientID), lastVictim, Server()->ClientName(lastVictim));
                Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

                // Send release information to the same killer's victims
                // Message flood risk on first killed players!
                //if(pPlayer->m_zCatchNumVictims > 0)
                //{
                //	CPlayer::CZCatchVictim *v = pPlayer->m_ZCatchVictims;
                //	str_format(aBuf, sizeof(aBuf), "'%s' released '%s'", Server()->ClientName(ClientID), Server()->ClientName(lastVictim));
                //	while(v != NULL)
                //	{
                //		SendChatTarget(v->ClientID, aBuf);
                //		v = v->prev;
                //	}
                //}


                return;
            }
            else if (g_Config.m_SvSuicideTime == 0)
            {
                SendChatTarget(ClientID, "Suicide is not allowed.");
            }
            else if (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed()*g_Config.m_SvSuicideTime > Server()->Tick())
            {
                char aBuf[128];
                str_format(aBuf, sizeof(aBuf), "Only one suicide every %d seconds is allowed.", g_Config.m_SvSuicideTime);
                SendChatTarget(ClientID, aBuf);
            }
            else if (pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_FreezeTicks)
            {
                SendChatTarget(ClientID, "You can't kill yourself while you're frozen.");
            }
            else
            {
                pPlayer->m_LastKill = Server()->Tick();
                pPlayer->KillCharacter(WEAPON_SELF);
                return;
            }
            pPlayer->m_LastKillTry = Server()->Tick();
            /* end zCatch*/
        }
        else if (MsgID == NETMSGTYPE_CL_VERSION)
        {
            // retrieve the data the teeworlds and not the hacky way.
            CNetMsg_Cl_Version* pMsg = (CNetMsg_Cl_Version*)pRawMsg;
            // add client id to the received client IDs of that secific player.
            pPlayer->AddClientVersion(pMsg->m_ClientVersionNumber);
        }
        else
        {
        	// receiving a weird message, 
        	// if it does not correspond to anything that we know.
        	pPlayer->AddWeirdMessage(MsgID);
        }
    }
}

void CGameContext::AddMute(const char* pIP, int Secs)
{
	int Pos = Muted(pIP);
	if(Pos > -1)	
		m_aMutes[Pos].m_Expires = Server()->TickSpeed() * Secs + Server()->Tick();	// overwrite mute
	else
		for(int i = 0; i < MAX_MUTES; i++)	// find free slot
			if(!m_aMutes[i].m_aIP[0])
			{
				str_copy(m_aMutes[i].m_aIP, pIP, sizeof(m_aMutes[i].m_aIP));
				m_aMutes[i].m_Expires = Server()->TickSpeed() * Secs + Server()->Tick();
				break;
			}
}

void CGameContext::AddMute(int ClientID, int Secs, bool Auto)
{
	char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	AddMute(aAddrStr, Secs);
	
	char aBuf[128];
	if(Secs > 0)
		str_format(aBuf, sizeof(aBuf), "%s has been %smuted for %d:%02d min.", Server()->ClientName(ClientID), Auto ? "auto-" : "", Secs/60, Secs%60);
	else
		str_format(aBuf, sizeof(aBuf), "%s has been unmuted.", Server()->ClientName(ClientID));
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	SendChatTarget(-1, aBuf);
}

int CGameContext::Muted(const char *pIP)
{
	CleanMutes();
	int Pos = -1;
	if(!pIP[0])
		return -1;
	for(int i = 0; i < MAX_MUTES; i++)
		if(!str_comp_num(pIP, m_aMutes[i].m_aIP, sizeof(m_aMutes[i].m_aIP)))
		{
			Pos = i;
			break;
		}
	return Pos;
}

int CGameContext::Muted(int ClientID)
{
	char aIP[NETADDR_MAXSTRSIZE] = {0};
	Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
	return Muted(aIP);
}

void CGameContext::CleanMutes()
{
	for(int i = 0; i < MAX_MUTES; i++)
		if(m_aMutes[i].m_Expires < Server()->Tick())
			m_aMutes[i].m_aIP[0] = 0;
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
	pSelf->m_pController->TogglePause();
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
		pSelf->m_pController->DoWarmup(pResult->GetInteger(0));
	else
		pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments()>2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick()+pSelf->Server()->TickSpeed()*Delay*60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);

	(void)pSelf->m_pController->CheckTeamBalance();
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

	int CounterRed = 0;
	int CounterBlue = 0;
	int PlayerTeam = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			++PlayerTeam;
	PlayerTeam = (PlayerTeam+1)/2;
	
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were shuffled");

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
		{
			if(CounterRed == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
			else if(CounterBlue == PlayerTeam)
				pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
			else
			{	
				if(rand() % 2)
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_BLUE, false);
					++CounterBlue;
				}
				else
				{
					pSelf->m_apPlayers[i]->SetTeam(TEAM_RED, false);
					++CounterRed;
				}
			}
		}
	}

	(void)pSelf->m_pController->CheckTeamBalance();
}

void CGameContext::ConLockTeams(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_LockTeams ^= 1;
	if(pSelf->m_LockTeams)
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were locked");
	else
		pSelf->SendChat(-1, CGameContext::CHAT_ALL, "Teams were unlocked");
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
	while(*pDescription && *pDescription == ' ')
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

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				str_format(aBuf, sizeof(aBuf), "admin forced server option '%s' (%s)", pValue, pReason);
				pSelf->SendChatTarget(-1, aBuf);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if (!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}

		str_format(aBuf, sizeof(aBuf), "admin moved '%s' to spectator (%s)", pSelf->Server()->ClientName(SpectateID), pReason);
		pSelf->SendChatTarget(-1, aBuf);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf);
	}
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
	str_format(aBuf, sizeof(aBuf), "admin forced vote %s", pResult->GetString(0));
	pSelf->SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = g_Config.m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(pSelf->m_apPlayers[i])
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = pResult->GetInteger(0);
	if(CID < 0 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
	else
		pSelf->AddMute(CID, pResult->GetInteger(1));
}

void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[128];
	int Sec, Count = 0;
	pSelf->CleanMutes();
	for(int i = 0; i < MAX_MUTES; i++)
	{
		if(pSelf->m_aMutes[i].m_aIP[0] == 0)
			continue;
		
		Sec = (pSelf->m_aMutes[i].m_Expires - pSelf->Server()->Tick())/pSelf->Server()->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "#%d: %s for %d:%02d min", i, pSelf->m_aMutes[i].m_aIP, Sec/60, Sec%60);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		Count++;
	}
	str_format(aBuf, sizeof(aBuf), "%d mute(s)", Count);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);	
}

void CGameContext::ConUnmuteID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = pResult->GetInteger(0);
	if(CID < 0 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
	}
	else
		pSelf->AddMute(CID, 0);
}

void CGameContext::ConUnmuteIP(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int MuteID = pResult->GetInteger(0);
	char aBuf[128];
	
	if(MuteID < 0 || MuteID >= MAX_MUTES || pSelf->Muted(pSelf->m_aMutes[MuteID].m_aIP) == -1)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "mute not found");
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "unmuted %s", pSelf->m_aMutes[MuteID].m_aIP);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		pSelf->AddMute(pSelf->m_aMutes[MuteID].m_aIP, 0);
	}
}

void CGameContext::ConKill(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int CID = pResult->GetInteger(0);
	if(CID < 0 || CID >= MAX_CLIENTS || !pSelf->m_apPlayers[CID])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
		return;
	}
	if(pSelf->m_apPlayers[CID]->GetCharacter() == 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Player is already dead");
		return;
	}
	pSelf->m_apPlayers[CID]->KillCharacter();
	// message to console and chat
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' has been killed by admin.", pSelf->Server()->ClientName(CID));
	pSelf->SendChatTarget(-1, aBuf);
}

/**
 * @brief Hooks the function CGameController_zCatch::MergeRankingIntoTarget(...) to the specific command
 * in the remote console.
 * @details [long description]
 *
 * @param pResult [description]
 * @param pUserData [description]
 */
void CGameContext::ConMergeRecords(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;

	// Get strings
	std::string Source(pResult->GetString(0));
	std::string Target(pResult->GetString(1));

	if (Source.compare(Target) == 0) {
		// if both strings are equal, do nothing, otherwise this would delete all records of given player.

		/* print info */
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Merging was not successful, because both nicks are equal.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		return;
	}
	char *source = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(source, Source.c_str(), MAX_NAME_LENGTH);

	char *target = (char*)malloc(MAX_NAME_LENGTH);
	str_copy(target, Target.c_str(), MAX_NAME_LENGTH);

	pSelf->AddFuture(std::async(std::launch::async, &CGameController_zCatch::MergeRankingIntoTarget, pSelf, source, target));

}


/**
 * @brief Merges records of two players using their current ingame client IDs
 * @details  \see{CGameContext::ConMergeRecords}
 *
 * @param pResult Console Input
 * @param pUserData Context data(?)
 */
void CGameContext::ConMergeRecordsId(IConsole::IResult *pResult, void *pUserData) {
	CGameContext *pSelf = (CGameContext *)pUserData;
	// Get IDs
	int Source(pResult->GetInteger(0));
	int Target(pResult->GetInteger(1));

	/*Invalid input handling */
	if (Source == Target ||
	        Source < 0 ||
	        Target < 0 ||
	        Source >= MAX_CLIENTS ||
	        Target >= MAX_CLIENTS) {
		/* print info */
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Merging was not successful, because either both IDs are equal or ID's are not valid.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		return;
	}

	char* source = (char*)malloc(sizeof(char) * MAX_NAME_LENGTH);
	char* target = (char*)malloc(sizeof(char) * MAX_NAME_LENGTH);

	str_copy(source, pSelf->Server()->ClientName(Source), MAX_NAME_LENGTH);
	str_copy(target, pSelf->Server()->ClientName(Target), MAX_NAME_LENGTH);

	/*More invalid input handling depending on what ClientName() returned*/
	if (str_comp(source, "(invalid)") == 0 ||
	        str_comp(target, "(invalid)") == 0 ||
	        str_comp(source, "(connecting)") == 0 ||
	        str_comp(target, "(connecting)") == 0) {
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Merging was not successful, because either both one or both IDs are invalid or one of them has not connected to the server yet. Please try again.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
		free(source);
		free(target);
		return;
	}
	pSelf->AddFuture(std::async(std::launch::async, &CGameController_zCatch::MergeRankingIntoTarget, pSelf, source, target));
	// source and target are freed after execution of the thread within the function MergeRankingIntoTarget

}

/**
 * @brief Prints all unique flags of all players.
 */
void CGameContext::ConFlags(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	char aBuf[128];

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pSelf->m_apPlayers[i])
		{
			std::string name(pSelf->Server()->ClientName(i));
			std::string clan(pSelf->Server()->ClientClan(i));

			str_format(aBuf, sizeof(aBuf), "Showing flags for player: ID:%2d '%s' '%s':", i, name.c_str(), clan.c_str());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", aBuf);

			std::vector<int> flags = pSelf->m_apPlayers[i]->GetUniqueFlags();
			for(int &flag : flags)
			{
				str_format(aBuf, sizeof(aBuf), "Bitmask: %032s Value: %10d", std::bitset<32>(flag).to_string().c_str(), flag);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", aBuf);
			}

			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", "");
		}
	}
}

/**
 * @brief Prints all unique flags of all players.
 */
void CGameContext::ConFlagsById(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	int id(pResult->GetInteger(0));
	char aBuf[128];
	
	// check if id is valid.
	if(id < 0 || id >= MAX_CLIENTS)
	{	
		// the given id is in the invalid range.
		// print this information to the admin remote console.
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", "Invalid id.");
		// don't do anything anymore.
		return;
	}
	
	// check if a player with the given id actually exists.
	if(pSelf->m_apPlayers[id])
		{
			std::string name(pSelf->Server()->ClientName(id));
			std::string clan(pSelf->Server()->ClientClan(id));

			str_format(aBuf, sizeof(aBuf), "Showing flags for player: ID:%2d '%s' '%s':", id, name.c_str(), clan.c_str());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", aBuf);


			std::vector<int> flags = pSelf->m_apPlayers[id]->GetUniqueFlags();
			for(int &flag : flags)
			{
				str_format(aBuf, sizeof(aBuf), "Bitmask: %032s Value: %10d", std::bitset<32>(flag).to_string().c_str(), flag);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", aBuf);
			}

			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", "");
		}
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Flags", "No such player id online.");
		}
}


/**
 * @brief Prints all unique client version of each player.
 */
void CGameContext::ConClientVersions(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	char aBuf[128];

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pSelf->m_apPlayers[i])
		{
			std::string name(pSelf->Server()->ClientName(i));
			std::string clan(pSelf->Server()->ClientClan(i));

			str_format(aBuf, sizeof(aBuf), "Showing client version(s) for player: ID:%2d '%s' '%s':", i, name.c_str(), clan.c_str());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", aBuf);
			
			std::vector<int> versions = pSelf->m_apPlayers[i]->GetUniqueClientVersions();

			// no information received, we assume that it's vanilla
			if (versions.empty())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", "[?]");
				continue;
			}

			// version information received, we print them.
			for(int &version : versions)
			{
				str_format(aBuf, sizeof(aBuf), "Version: %d", version);
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", aBuf);
			}

			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", "");
		}
	}
}

/**
 * @brief Prints all unique client versions of given player.
 */
void CGameContext::ConClientVersionsById(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	int id(pResult->GetInteger(0));
	char aBuf[128];
	
	// check if id is valid.
	if(id < 0 || id >= MAX_CLIENTS)
	{	
		// the given id is in the invalid range.
		// print this information to the admin remote console.
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", "Invalid id.");
		// don't do anything anymore.
		return;
	}
	
	if(pSelf->m_apPlayers[id])
		{
			std::string name(pSelf->Server()->ClientName(id));
			std::string clan(pSelf->Server()->ClientClan(id));

			str_format(aBuf, sizeof(aBuf), "Showing client version(s) for player: ID:%2d '%s' '%s':", id, name.c_str(), clan.c_str());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", aBuf);

			std::vector<int> versions = pSelf->m_apPlayers[id]->GetUniqueClientVersions();

			// no information received, we assume that it's vanilla
			if (versions.empty())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", "Most likely vanilla, because no version information received.");

			}
			else
			{
				// version information received, we print them.
				for(int &version : versions)
				{
					str_format(aBuf, sizeof(aBuf), "Version: %d", version);
					pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", aBuf);
				}
			}
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", "");
		}
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Client version", "No such player id online.");
		}
}

void CGameContext::ConWeirdMessages(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	char aBuf[128];

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(pSelf->m_apPlayers[i])
		{
			std::string name(pSelf->Server()->ClientName(i));
			std::string clan(pSelf->Server()->ClientClan(i));

			str_format(aBuf, sizeof(aBuf), "Showing weird messages for player: ID:%2d '%s' '%s':", i, name.c_str(), clan.c_str());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", aBuf);
			
			std::vector<std::pair<int, int>> messages = pSelf->m_apPlayers[i]->GetUniqueWeirdMessageOccurrences();

			if (messages.empty())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", "None");
				continue;
			}

			for(auto& pair : messages)
			{	
				if (pair.first >= 0 && pair.first < NUM_NETMSGTYPES)
				{
					// message id is known, but data is corrupted -> most likely data corruption
					str_format(aBuf, sizeof(aBuf), "MessageID: %d Occurrences: %d [caused by a bad internet connection]", pair.first, pair.second);
				}
				else
				{
					// message id is unknow (could also be caused by a bad internet connection)
					str_format(aBuf, sizeof(aBuf), "MessageID: %d Occurrences: %d", pair.first, pair.second);
				}
				
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", aBuf);
			}
		}
	}
}

void CGameContext::ConWeirdMessagesById(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	int id(pResult->GetInteger(0));
	char aBuf[128];
	
	// check if id is valid.
	if(id < 0 || id >= MAX_CLIENTS)
	{	
		// the given id is in the invalid range.
		// print this information to the admin remote console.
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", "Invalid id.");
		// don't do anything anymore.
		return;
	}
	
	if(pSelf->m_apPlayers[id])
		{
			std::string name(pSelf->Server()->ClientName(id));
			std::string clan(pSelf->Server()->ClientClan(id));

			str_format(aBuf, sizeof(aBuf), "Showing Weird client messages for player: ID:%2d '%s' '%s':", id, name.c_str(), clan.c_str());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", aBuf);

			std::vector<std::pair<int, int>> messages = pSelf->m_apPlayers[id]->GetUniqueWeirdMessageOccurrences();

			if (messages.empty())
			{
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", "None");
				return;
			}

			for(auto& pair : messages)
			{
				if (pair.first >= 0 && pair.first < NUM_NETMSGTYPES)
				{
					// message id is known, but data is corrupted -> most likely data corruption
					str_format(aBuf, sizeof(aBuf), "MessageID: %d Occurrences: %d [caused by a bad internet connection]", pair.first, pair.second);
				}
				else
				{
					// message id is unknow (could also be caused by a bad internet connection)
					str_format(aBuf, sizeof(aBuf), "MessageID: %d Occurrences: %d", pair.first, pair.second);
				}
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", aBuf);
			}
		}
		else
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Weird client messages", "No such player id online.");
		}
}

void CGameContext::ConShowCursorPositionByID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	int id(pResult->GetInteger(0));
	char aBuf[128];

    // check if id is valid.
    if (id < 0 || id >= MAX_CLIENTS)
    {
        // the given id is in the invalid range.
        // print this information to the admin remote console.
        pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", "Invalid id.");
        // don't do anything anymore.
        return;
    }

    if (pSelf->m_apPlayers[id])
    {
        std::string name(pSelf->Server()->ClientName(id));
        std::string clan(pSelf->Server()->ClientClan(id));
        pSelf->m_apPlayers[id]->EnableCursorVisibility();
        str_format(aBuf, sizeof(aBuf), "Enabling cursor tracking for player: ID:%2d '%s' '%s':", id, name.c_str(), clan.c_str());
        pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", aBuf);

    }
    else
    {
        pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", "No such player id online.");
    }
}

void CGameContext::ConHideCursorPositionByID(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	int id(pResult->GetInteger(0));
	char aBuf[128];

    // check if id is valid.
    if (id < 0 || id >= MAX_CLIENTS)
    {
        // the given id is in the invalid range.
        // print this information to the admin remote console.
        pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", "Invalid id.");
        // don't do anything anymore.
        return;
    }

    if (pSelf->m_apPlayers[id])
    {
        std::string name(pSelf->Server()->ClientName(id));
        std::string clan(pSelf->Server()->ClientClan(id));
        pSelf->m_apPlayers[id]->DisableCursorVisibility();
        str_format(aBuf, sizeof(aBuf), "Disabled cursor tracking for player: ID:%2d '%s' '%s':", id, name.c_str(), clan.c_str());
        pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", aBuf);

    }
    else
    {
        pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", "No such player id online.");
    }
}

void CGameContext::ConResetCursorPositionVisibility(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = static_cast<CGameContext *>(pUserData);
	char aBuf[128];

    for (int id = 0; id < MAX_CLIENTS; ++id)
    {
        if (pSelf->m_apPlayers[id] && pSelf->m_apPlayers[id]->IsCursorVisible())
        {
            std::string name(pSelf->Server()->ClientName(id));
            std::string clan(pSelf->Server()->ClientClan(id));
            pSelf->m_apPlayers[id]->DisableCursorVisibility();
            str_format(aBuf, sizeof(aBuf), "Disabled cursor tracking for player: ID:%2d '%s' '%s':", id, name.c_str(), clan.c_str());
            pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Cursor position", aBuf);

        }
    }
}


void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER|CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER|CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");
	Console()->Register("set_team_all", "i", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team");
	Console()->Register("swap_teams", "", CFGFLAG_SERVER, ConSwapTeams, this, "Swap the current teams");
	Console()->Register("shuffle_teams", "", CFGFLAG_SERVER, ConShuffleTeams, this, "Shuffle the current teams");
	Console()->Register("lock_teams", "", CFGFLAG_SERVER, ConLockTeams, this, "Lock/unlock teams");

	Console()->Register("merge_records", "ss", CFGFLAG_SERVER, ConMergeRecords, this, "Merge two records into the target username and delete source records: merge_records <source nickname> <target nickname>", IConsole::ACCESS_LEVEL_ADMIN);
	Console()->Register("merge_records_id", "ii", CFGFLAG_SERVER, ConMergeRecordsId, this, "Merge two records into the target ID and delete source ID's records: merge_records <source ID> <target ID>", IConsole::ACCESS_LEVEL_ADMIN);
	
	Console()->Register("flags_all", "", CFGFLAG_SERVER, ConFlags, this, "Show all unique sent player flags.");
	Console()->Register("flags", "i", CFGFLAG_SERVER, ConFlagsById, this, "Show all unique sent player flags of given player: flags <ID>");

	Console()->Register("clients_all", "", CFGFLAG_SERVER, ConClientVersions, this, "Show player client version number(s).");
	Console()->Register("clients", "i", CFGFLAG_SERVER, ConClientVersionsById, this, "Show player client's version number(s): clients <ID>");

	Console()->Register("weird_messages_all", "", CFGFLAG_SERVER, ConWeirdMessages, this, "Show a list of non-regular message ids sent by each client & their occurrences.");
	Console()->Register("weird_messages_id", "", CFGFLAG_SERVER, ConWeirdMessagesById, this, "Show a list of non-regular message ids sent by one client(ID) & their occurrences.");

	Console()->Register("cursor_show_id", "i", CFGFLAG_SERVER, ConShowCursorPositionByID, this, "Shows the current player's cursor position indicated by a laser.");
    Console()->Register("cursor_hide_id", "i", CFGFLAG_SERVER, ConHideCursorPositionByID, this, "Hides the current player's cursor position.");
    Console()->Register("cursor_reset_all", "", CFGFLAG_SERVER, ConResetCursorPositionVisibility, this, "Hides all visible cursor position indicators.");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Register("mute", "ii", CFGFLAG_SERVER, ConMute, this, "Mutes a player for x sec");
	Console()->Register("unmute", "i", CFGFLAG_SERVER, ConUnmuteID, this, "Unmutes a player by its id");
	Console()->Register("unmuteid", "i", CFGFLAG_SERVER, ConUnmuteID, this, "Unmutes a player by its client id");
	Console()->Register("unmuteip", "i", CFGFLAG_SERVER, ConUnmuteIP, this, "Removes a mute by its index");
	Console()->Register("mutes", "", CFGFLAG_SERVER, ConMutes, this, "Show all mutes");
	
	Console()->Register("kill", "i", CFGFLAG_SERVER, ConKill, this, "Kill a player by id");
		
	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
    
	m_GameUuid = RandomUuid();
	Console()->SetTeeHistorianCommandCallback(CommandCallback, this);

	//if(!data) // only load once
		//data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);

	char aMapName[128];
	int MapSize;
	int MapCrc;
	Server()->GetMapInfo(aMapName, sizeof(aMapName), &MapSize, &MapCrc);

	/* open ranking system db */
	if (g_Config.m_SvRanking == 1)
	{
		int rc = sqlite3_open(g_Config.m_SvRankingFile, &m_RankingDb);
		if (rc){
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Can't open database (#%d): %s\n", rc, sqlite3_errmsg(m_RankingDb));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", aBuf);
			sqlite3_close(m_RankingDb);
			exit(1);
		}
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ranking", "SQLite3 database opened");
		/* wait up to 5 seconds if the db is used */
		sqlite3_busy_timeout(m_RankingDb, 5000);
	}

    m_pController = new CGameController_zCatch(this);
	
	// This is only set when the server starts.
	m_TeeHistorianActive = g_Config.m_SvTeeHistorian;
	if(m_TeeHistorianActive)
	{
		char aGameUuid[UUID_MAXSTRSIZE];
		FormatUuid(m_GameUuid, aGameUuid, sizeof(aGameUuid));

		char aFilename[64];
		str_format(aFilename, sizeof(aFilename), "teehistorian/%s.teehistorian", aGameUuid);

		IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
		{
			dbg_msg("teehistorian", "failed to open '%s'", aFilename);
			Server()->SetErrorShutdown("teehistorian open error");
			return;
		}
		else
		{
			dbg_msg("teehistorian", "recording to '%s'", aFilename);
		}
		m_pTeeHistorianFile = aio_new(File);

		char aVersion[128];
		str_format(aVersion, sizeof(aVersion), "%s", GAME_VERSION);
		
		CTeeHistorian::CGameInfo GameInfo;
		GameInfo.m_GameUuid = m_GameUuid;
		GameInfo.m_pServerVersion = aVersion;
		GameInfo.m_StartTime = time(0);

		GameInfo.m_pServerName = g_Config.m_SvName;
		GameInfo.m_ServerPort = g_Config.m_SvPort;
		GameInfo.m_pGameType = m_pController->m_pGameType;

		GameInfo.m_pConfig = &g_Config;
		GameInfo.m_pTuning = Tuning();
		GameInfo.m_pUuids = &g_UuidManager;

		GameInfo.m_pMapName = aMapName;
		GameInfo.m_MapSize = MapSize;
		GameInfo.m_MapCrc = MapCrc;

		m_TeeHistorian.Reset(&GameInfo, TeeHistorianWrite, this);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			int Level = Server()->GetAuthedState(i);
			if(Level)
			{
				m_TeeHistorian.RecordAuthInitial(i, Level, Server()->GetAuthName(i));
			}
		}
	}


	/* ranking system */
	if (RankingEnabled())
	{
		m_pController->OnInitRanking(m_RankingDb);
	}

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


#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			OnClientConnected(MAX_CLIENTS-i-1);
		}
	}
#endif
}

void CGameContext::OnShutdown()
{
    if(m_TeeHistorianActive)
	{
		m_TeeHistorian.Finish();
		aio_close(m_pTeeHistorianFile);
		aio_wait(m_pTeeHistorianFile);
		int Error = aio_error(m_pTeeHistorianFile);
		if(Error)
		{
			dbg_msg("teehistorian", "error closing file, err=%d", Error);
			Server()->SetErrorShutdown("teehistorian close error");
		}
		aio_free(m_pTeeHistorianFile);
	}
	
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
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD|MSGFLAG_NOSEND, ClientID);
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS ? false : true;
}

bool CGameContext::IsClientAimBot(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsAimBot;
}

/* wait given ms to lock ranking db, if time is negative wait until database is unlocked */
bool CGameContext::LockRankingDb(int ms)
{
	if (ms < 0)
	{
		m_RankingDbMutex.lock();
		return true;
	}
	return m_RankingDbMutex.try_lock_for(std::chrono::milliseconds(ms));
}

/* unlock ranking db */
void CGameContext::UnlockRankingDb()
{
	m_RankingDbMutex.unlock();
}

CUuid CGameContext::GameUuid() { return m_GameUuid; }
const char *CGameContext::GameType() { return m_pController && m_pController->m_pGameType ? m_pController->m_pGameType : ""; }
const char *CGameContext::Version() { return GAME_VERSION; }
const char *CGameContext::NetVersion() { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::TeeHistorianWrite(const void *pData, int DataSize, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	aio_write(pSelf->m_pTeeHistorianFile, pData, DataSize);
}

void CGameContext::CommandCallback(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser)
{
    CGameContext *pSelf = (CGameContext *)pUser;
    if (pSelf->m_TeeHistorianActive)
    {
        pSelf->m_TeeHistorian.RecordConsoleCommand(ClientID, FlagMask, pCmd, pResult);
    }
}
