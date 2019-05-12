/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */

#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/fifo.h>

#include <mastersrv/mastersrv.h>

#include "register.h"
#include "server.h"

#include <string.h>
#include <string>
#include <map>

#if defined(CONF_FAMILY_WINDOWS)
	#define _WIN32_WINNT 0x0501
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

static const char *StrUTF8Ltrim(const char *pStr)
{
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) && (Code < 0x2028 || Code > 0x202F) &&
			(Code < 0x205F || Code > 0x2064) && (Code < 0x206A || Code > 0x206F) && (Code < 0xFE00 || Code > 0xFE0F) &&
			Code != 0xFEFF && (Code < 0xFFF9 || Code > 0xFFFC))
		{
			return pStrOld;
		}
	}
	return pStr;
}

static void StrUTF8Rtrim(char *pStr)
{
	const char *p = pStr;
	const char *pEnd = 0;
	while(*p)
	{
		const char *pStrOld = p;
		int Code = str_utf8_decode(&p);

		// check if unicode is not empty
		if(Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) && (Code < 0x2028 || Code > 0x202F) &&
			(Code < 0x205F || Code > 0x2064) && (Code < 0x206A || Code > 0x206F) && (Code < 0xFE00 || Code > 0xFE0F) &&
			Code != 0xFEFF && (Code < 0xFFF9 || Code > 0xFFFC))
		{
			pEnd = 0;
		}
		else if(pEnd == 0)
			pEnd = pStrOld;
	}
	if(pEnd != 0)
		*(const_cast<char *>(pEnd)) = 0;
}


CSnapIDPool::CSnapIDPool()
{
	Reset();
}

void CSnapIDPool::Reset()
{
	for(int i = 0; i < MAX_IDS; i++)
	{
		m_aIDs[i].m_Next = i+1;
		m_aIDs[i].m_State = 0;
	}

	m_aIDs[MAX_IDS-1].m_Next = -1;
	m_FirstFree = 0;
	m_FirstTimed = -1;
	m_LastTimed = -1;
	m_Usage = 0;
	m_InUsage = 0;
}


void CSnapIDPool::RemoveFirstTimeout()
{
	int NextTimed = m_aIDs[m_FirstTimed].m_Next;

	// add it to the free list
	m_aIDs[m_FirstTimed].m_Next = m_FirstFree;
	m_aIDs[m_FirstTimed].m_State = 0;
	m_FirstFree = m_FirstTimed;

	// remove it from the timed list
	m_FirstTimed = NextTimed;
	if(m_FirstTimed == -1)
		m_LastTimed = -1;

	m_Usage--;
}

int CSnapIDPool::NewID()
{
	int64 Now = time_get();

	// process timed ids
	while(m_FirstTimed != -1 && m_aIDs[m_FirstTimed].m_Timeout < Now)
		RemoveFirstTimeout();

	int ID = m_FirstFree;
	dbg_assert(ID != -1, "id error");
	if(ID == -1)
		return ID;
	m_FirstFree = m_aIDs[m_FirstFree].m_Next;
	m_aIDs[ID].m_State = 1;
	m_Usage++;
	m_InUsage++;
	return ID;
}

void CSnapIDPool::TimeoutIDs()
{
	// process timed ids
	while(m_FirstTimed != -1)
		RemoveFirstTimeout();
}

void CSnapIDPool::FreeID(int ID)
{
	if(ID < 0)
		return;
	dbg_assert(m_aIDs[ID].m_State == 1, "id is not alloced");

	m_InUsage--;
	m_aIDs[ID].m_State = 2;
	m_aIDs[ID].m_Timeout = time_get()+time_freq()*5;
	m_aIDs[ID].m_Next = -1;

	if(m_LastTimed != -1)
	{
		m_aIDs[m_LastTimed].m_Next = ID;
		m_LastTimed = ID;
	}
	else
	{
		m_FirstTimed = ID;
		m_LastTimed = ID;
	}
}


void CServerBan::InitServerBan(IConsole *pConsole, IStorage *pStorage, CServer* pServer)
{
	CNetBan::Init(pConsole, pStorage);

	m_pServer = pServer;

	// overwrites base command, todo: improve this
	Console()->Register("ban", "s?ir", CFGFLAG_SERVER|CFGFLAG_STORE, ConBanExt, this, "Ban player with ip/client id for x minutes for any reason");
}

template<class T>
int CServerBan::BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason)
{
	// validate address
	if(Server()->m_RconClientID >= 0 && Server()->m_RconClientID < MAX_CLIENTS &&
		Server()->m_aClients[Server()->m_RconClientID].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(NetMatch(pData, Server()->m_NetServer.ClientAddr(Server()->m_RconClientID)))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (you can't ban yourself)");
			return -1;
		}

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(i == Server()->m_RconClientID || Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed >= Server()->m_RconAuthLevel && NetMatch(pData, Server()->m_NetServer.ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}
	else if(Server()->m_RconClientID == IServer::RCON_CID_VOTE)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed != CServer::AUTHED_NO && NetMatch(pData, Server()->m_NetServer.ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}

	int Result = Ban(pBanPool, pData, Seconds, pReason);
	if(Result != 0)
		return Result;

	// drop banned clients
	typename T::CDataType Data = *pData;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			continue;

		if(NetMatch(&Data, Server()->m_NetServer.ClientAddr(i)))
		{
			CNetHash NetHash(&Data);
			char aBuf[256];
			MakeBanInfo(pBanPool->Find(&Data, &NetHash), aBuf, sizeof(aBuf), MSGTYPE_PLAYER);
			Server()->m_NetServer.Drop(i, aBuf);
		}
	}

	return Result;
}

int CServerBan::BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason)
{
	return BanExt(&m_BanAddrPool, pAddr, Seconds, pReason);
}

int CServerBan::BanRange(const CNetRange *pRange, int Seconds, const char *pReason)
{
	if(pRange->IsValid())
		return BanExt(&m_BanRangePool, pRange, Seconds, pReason);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban failed (invalid range)");
	return -1;
}

void CServerBan::ConBanExt(IConsole::IResult *pResult, void *pUser)
{
	CServerBan *pThis = static_cast<CServerBan *>(pUser);
	const int defaultMinutes = 5;

	const char *pStr = pResult->GetString(0);
	int Minutes = pResult->NumArguments()>1 ? clamp(pResult->GetInteger(1), 0, 44640) : defaultMinutes;
	const char *pReason = pResult->NumArguments()>2 ? pResult->GetString(2) : "No reason given";
	
	// check if time was given or a reason instead
	const char *time;
	if(pResult->NumArguments() > 1)
	{
		// check if number given
		time = pResult->GetString(1);
		int i = str_length(time) - 1;
		for(; i >= 0; --i)
			if(time[i] < '0' || '9' < time[i])
				break;
		// in case that no number was given
		if(!(str_length(time) && i < 0))
		{
			Minutes = defaultMinutes;
			if(pResult->NumArguments() > 2)
			{ // add to reason
				char *newReason = (char*)malloc(sizeof(char) * (str_length(time) + str_length(pReason) + 2));
				mem_copy(newReason, time, sizeof(char) * str_length(time));
				newReason[str_length(time)] = ' ';
				mem_copy(newReason + str_length(time) + 1, pReason, sizeof(char) * (str_length(pReason) + 1));
				pReason = newReason;
			}
			else
			{ // the time is the actual reason
				pReason = time;
			}
		}
	}

	int CID = -1;
	if(StrAllnum(pStr))
		CID = str_toint(pStr);
	else
	{
		NETADDR Addr;
		if(net_addr_from_str(&Addr, pStr) == 0)
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(pThis->NetMatch(&Addr, pThis->Server()->m_NetServer.ClientAddr(i)))
				{
					CID = i;
					break;
				}
	}

	if(StrAllnum(pStr))
	{
		int ClientID = str_toint(pStr);
		if(ClientID < 0 || ClientID >= MAX_CLIENTS || pThis->Server()->m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid client id)");
		else
			pThis->BanAddr(pThis->Server()->m_NetServer.ClientAddr(ClientID), Minutes*60, pReason);
	}
	else
		ConBan(pResult, pUser);
}


void CServer::CClient::Reset()
{
	// reset input
	for(int i = 0; i < 200; i++)
		m_aInputs[i].m_GameTick = -1;
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = CClient::SNAPRATE_INIT;
	m_Score = 0;
}

CServer::CServer() : m_DemoRecorder(&m_SnapshotDelta)
{
	m_TickSpeed = SERVER_TICK_SPEED;

	m_pGameServer = 0;

	m_CurrentGameTick = 0;
	m_RunServer = 1;

	m_pCurrentMapData = 0;
	m_CurrentMapSize = 0;

	m_MapReload = 0;

	m_RconClientID = IServer::RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_SUBADMIN;
	
	// when starting there are no admins
	m_numLoggedInAdmins = 0;
	
	m_Votebans = NULL;
	m_InfoTexts = NULL;
	m_InfoTextInterval = -1;
	
	m_aErrorShutdownReason[0] = 0;

	Init();
}

CServer::~CServer()
{
	// delte votebans
	while(m_Votebans != NULL)
	{
		CVoteban *tmp = m_Votebans->m_Next;
		delete m_Votebans;
		m_Votebans = tmp;
	}
	
	// delte info texts
	while(m_InfoTexts != NULL)
	{
		CInfoText *tmp = m_InfoTexts->m_Next;
		delete m_InfoTexts;
		m_InfoTexts = tmp;
	}
}


void CServer::UpdateLoggedInAdmins()
{
	bool gotAny = (m_numLoggedInAdmins > 0);
	m_numLoggedInAdmins = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State == CClient::STATE_INGAME && m_aClients[i].m_Authed >= AUTHED_SUBADMIN)
		{
			++m_numLoggedInAdmins;
		}
	}
	if(gotAny != (m_numLoggedInAdmins > 0))
	{
		UpdateServerInfo();
	}
}

int CServer::TrySetClientName(int ClientID, const char *pName)
{
	char aTrimmedName[64];

	// trim the name
	str_copy(aTrimmedName, StrUTF8Ltrim(pName), sizeof(aTrimmedName));
	StrUTF8Rtrim(aTrimmedName);

	// check if new and old name are the same
	if(m_aClients[ClientID].m_aName[0] && str_comp(m_aClients[ClientID].m_aName, aTrimmedName) == 0)
		return 0;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "'%s' -> '%s'", pName, aTrimmedName);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
	pName = aTrimmedName;


	// check for empty names
	if(!pName[0])
		return -1;

	// make sure that two clients doesn't have the same name
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(i != ClientID && m_aClients[i].m_State >= CClient::STATE_READY)
		{
			if(str_comp(pName, m_aClients[i].m_aName) == 0)
				return -1;
		}

	// set the client name
	str_copy(m_aClients[ClientID].m_aName, pName, MAX_NAME_LENGTH);
	return 0;
}



void CServer::SetClientName(int ClientID, const char *pName)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	if(!pName)
		return;

	char aNameTry[MAX_NAME_LENGTH];
	str_copy(aNameTry, pName, MAX_NAME_LENGTH);
	if(TrySetClientName(ClientID, aNameTry))
	{
		// auto rename
		for(int i = 1;; i++)
		{
			str_format(aNameTry, MAX_NAME_LENGTH, "(%d)%s", i, pName);
			if(TrySetClientName(ClientID, aNameTry) == 0)
				break;
		}
	}
}

void CServer::SetClientClan(int ClientID, const char *pClan)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pClan)
		return;

	str_copy(m_aClients[ClientID].m_aClan, pClan, MAX_CLAN_LENGTH);
}

void CServer::SetClientCountry(int ClientID, int Country)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_Country = Country;
}

void CServer::SetClientScore(int ClientID, int Score)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;
	m_aClients[ClientID].m_Score = Score;
}

void CServer::Kick(int ClientID, const char *pReason)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "invalid client id to kick");
		return;
	}
	else if(m_RconClientID == ClientID)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick yourself");
 		return;
	}
	else if(m_aClients[ClientID].m_Authed > m_RconAuthLevel)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "kick command denied");
 		return;
	}

	m_NetServer.Drop(ClientID, pReason);
}

/*int CServer::Tick()
{
	return m_CurrentGameTick;
}*/

int64 CServer::TickStartTime(int Tick)
{
	return m_GameStartTime + (time_freq()*Tick)/SERVER_TICK_SPEED;
}

/*int CServer::TickSpeed()
{
	return SERVER_TICK_SPEED;
}*/

int CServer::Init()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aClients[i].m_State = CClient::STATE_EMPTY;
		m_aClients[i].m_aName[0] = 0;
		m_aClients[i].m_aClan[0] = 0;
		m_aClients[i].m_Country = -1;
		m_aClients[i].m_Snapshots.Init();
	}

	AdjustVotebanTime(m_CurrentGameTick);
	m_CurrentGameTick = 0;

	return 0;
}

void CServer::SetRconCID(int ClientID)
{
	m_RconClientID = ClientID;
}

int CServer::GetAuthedState(int ClientID)
{
	return m_aClients[ClientID].m_Authed;
}

const char *CServer::GetAuthName(int ClientID)
{
	int level =  m_aClients[ClientID].m_Authed;

	switch (level)
	{
	case AUTHED_ADMIN:
		return "AUTHED_ADMIN";
	case AUTHED_SUBADMIN:
		return m_aClients[ClientID].m_SubAdminAuthName.c_str();
	case AUTHED_MOD:
		return "AUTHED_MOD";
	case AUTHED_NO:
		return "AUTHED_NO";
	default:
		return "default";
	}
}

bool CServer::IsAuthed(int ClientID)
{
	return m_aClients[ClientID].m_Authed;
}

int CServer::GetClientInfo(int ClientID, CClientInfo *pInfo)
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "client_id is not valid");
	dbg_assert(pInfo != 0, "info can not be null");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		pInfo->m_pName = m_aClients[ClientID].m_aName;
		pInfo->m_Latency = m_aClients[ClientID].m_Latency;
		return 1;
	}
	return 0;
}

void CServer::GetClientAddr(int ClientID, char *pAddrStr, int Size)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		net_addr_str(m_NetServer.ClientAddr(ClientID), pAddrStr, Size, false);
}


const char *CServer::ClientName(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "(invalid)";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aName;
	else
		return "(connecting)";

}

const char *CServer::ClientClan(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aClan;
	else
		return "";
}

int CServer::ClientCountry(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return -1;
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_Country;
	else
		return -1;
}

bool CServer::ClientIngame(int ClientID)
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME;
}

int CServer::MaxClients() const
{
	return m_NetServer.MaxClients();
}

int CServer::SendMsg(CMsgPacker *pMsg, int Flags, int ClientID)
{
	return SendMsgEx(pMsg, Flags, ClientID, false);
}

int CServer::SendMsgEx(CMsgPacker *pMsg, int Flags, int ClientID, bool System)
{
	CNetChunk Packet;
	if(!pMsg)
		return -1;

	mem_zero(&Packet, sizeof(CNetChunk));

	Packet.m_ClientID = ClientID;
	Packet.m_pData = pMsg->Data();
	Packet.m_DataSize = pMsg->Size();

	// HACK: modify the message id in the packet and store the system flag
	*((unsigned char*)Packet.m_pData) <<= 1;
	if(System)
		*((unsigned char*)Packet.m_pData) |= 1;

	if(Flags&MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags&MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	// write message to demo recorder
	if(!(Flags&MSGFLAG_NORECORD))
		m_DemoRecorder.RecordMessage(pMsg->Data(), pMsg->Size());

	if(!(Flags&MSGFLAG_NOSEND))
	{
		if(ClientID == -1)
		{
			// broadcast
			int i;
			for(i = 0; i < MAX_CLIENTS; i++)
				if(m_aClients[i].m_State == CClient::STATE_INGAME)
				{
					Packet.m_ClientID = i;
					m_NetServer.Send(&Packet);
				}
		}
		else
			m_NetServer.Send(&Packet);
	}
	return 0;
}

void CServer::DoSnapshot()
{
	GameServer()->OnPreSnap();

	// create snapshot for demo recording
	if(m_DemoRecorder.IsRecording())
	{
		char aData[CSnapshot::MAX_SIZE];
		int SnapshotSize;

		// build snap and possibly add some messages
		m_SnapshotBuilder.Init();
		GameServer()->OnSnap(-1);
		SnapshotSize = m_SnapshotBuilder.Finish(aData);

		// write snapshot
		m_DemoRecorder.RecordSnapshot(Tick(), aData, SnapshotSize);
	}

	// create snapshots for all clients
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// client must be ingame to recive snapshots
		if(m_aClients[i].m_State != CClient::STATE_INGAME)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_RECOVER && (Tick()%50) != 0)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_INIT && (Tick()%10) != 0)
			continue;

		{
			char aData[CSnapshot::MAX_SIZE];
			CSnapshot *pData = (CSnapshot*)aData;	// Fix compiler warning for strict-aliasing
			char aDeltaData[CSnapshot::MAX_SIZE];
			char aCompData[CSnapshot::MAX_SIZE];
			int SnapshotSize;
			int Crc;
			static CSnapshot EmptySnap;
			CSnapshot *pDeltashot = &EmptySnap;
			int DeltashotSize;
			int DeltaTick = -1;
			int DeltaSize;

			m_SnapshotBuilder.Init();

			GameServer()->OnSnap(i);

			// finish snapshot
			SnapshotSize = m_SnapshotBuilder.Finish(pData);
			Crc = pData->Crc();

			// remove old snapshos
			// keep 3 seconds worth of snapshots
			m_aClients[i].m_Snapshots.PurgeUntil(m_CurrentGameTick-SERVER_TICK_SPEED*3);

			// save it the snapshot
			m_aClients[i].m_Snapshots.Add(m_CurrentGameTick, time_get(), SnapshotSize, pData, 0);

			// find snapshot that we can preform delta against
			EmptySnap.Clear();

			{
				DeltashotSize = m_aClients[i].m_Snapshots.Get(m_aClients[i].m_LastAckedSnapshot, 0, &pDeltashot, 0);
				if(DeltashotSize >= 0)
					DeltaTick = m_aClients[i].m_LastAckedSnapshot;
				else
				{
					// no acked package found, force client to recover rate
					if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_FULL)
						m_aClients[i].m_SnapRate = CClient::SNAPRATE_RECOVER;
				}
			}

			// create delta
			DeltaSize = m_SnapshotDelta.CreateDelta(pDeltashot, pData, aDeltaData);

			if(DeltaSize)
			{
				// compress it
				int SnapshotSize;
				const int MaxSize = MAX_SNAPSHOT_PACKSIZE;
				int NumPackets;

				SnapshotSize = CVariableInt::Compress(aDeltaData, DeltaSize, aCompData);
				NumPackets = (SnapshotSize+MaxSize-1)/MaxSize;

				for(int n = 0, Left = SnapshotSize; Left; n++)
				{
					int Chunk = Left < MaxSize ? Left : MaxSize;
					Left -= Chunk;

					if(NumPackets == 1)
					{
						CMsgPacker Msg(NETMSG_SNAPSINGLE);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick-DeltaTick);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n*MaxSize], Chunk);
						SendMsgEx(&Msg, MSGFLAG_FLUSH, i, true);
					}
					else
					{
						CMsgPacker Msg(NETMSG_SNAP);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick-DeltaTick);
						Msg.AddInt(NumPackets);
						Msg.AddInt(n);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n*MaxSize], Chunk);
						SendMsgEx(&Msg, MSGFLAG_FLUSH, i, true);
					}
				}
			}
			else
			{
				CMsgPacker Msg(NETMSG_SNAPEMPTY);
				Msg.AddInt(m_CurrentGameTick);
				Msg.AddInt(m_CurrentGameTick-DeltaTick);
				SendMsgEx(&Msg, MSGFLAG_FLUSH, i, true);
			}
		}
	}

	GameServer()->OnPostSnap();
}

int CServer::NewClientNoAuthCallback(int ClientID, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	pThis->m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].Reset();

	pThis->SendMap(ClientID);

	return 0;
}

int CServer::NewClientCallback(int ClientID, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	pThis->m_aClients[ClientID].m_State = CClient::STATE_AUTH;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].Reset();
	return 0;
}

int CServer::DelClientCallback(int ClientID, const char *pReason, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pThis->m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "client dropped. cid=%d addr=%s reason='%s'", ClientID, aAddrStr,	pReason);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

	// TeeHistorian
	pThis->GameServer()->OnClientEngineDrop(ClientID, pReason);

	// notify the mod about the drop
	if(pThis->m_aClients[ClientID].m_State >= CClient::STATE_READY)
		pThis->GameServer()->OnClientDrop(ClientID, pReason);

	pThis->m_aClients[ClientID].m_State = CClient::STATE_EMPTY;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_Snapshots.PurgeAll();
	
	// could have been an admin
	pThis->UpdateLoggedInAdmins();
	
	return 0;
}

void CServer::GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, int *pMapCrc)
{
	str_copy(pMapName, GetMapName(), MapNameSize);
	*pMapSize = m_CurrentMapSize;
	*pMapCrc = m_CurrentMapCrc;
}

void CServer::SendMap(int ClientID)
{
		CMsgPacker Msg(NETMSG_MAP_CHANGE);
		Msg.AddString(GetMapName(), 0);
		Msg.AddInt(m_CurrentMapCrc);
		Msg.AddInt(m_CurrentMapSize);
		SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID, true);
}

void CServer::SendConnectionReady(int ClientID)
{
	CMsgPacker Msg(NETMSG_CON_READY);
	SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID, true);
}

void CServer::SendRconLine(int ClientID, const char *pLine)
{
	CMsgPacker Msg(NETMSG_RCON_LINE);
	Msg.AddString(pLine, 512);
	SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
}

void CServer::SendRconLineAuthed(const char *pLine, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	static volatile int ReentryGuard = 0;
	int i;

	if(ReentryGuard) return;
	ReentryGuard++;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY && pThis->m_aClients[i].m_Authed >= pThis->m_RconAuthLevel)
			pThis->SendRconLine(i, pLine);
	}

	ReentryGuard--;
}

void CServer::SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_ADD);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	Msg.AddString(pCommandInfo->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
	Msg.AddString(pCommandInfo->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
	SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
}

void CServer::SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_REM);
	Msg.AddString(pCommandInfo->m_pName, 256);
	SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
}

void CServer::UpdateClientRconCommands()
{
	int ClientID = Tick() % MAX_CLIENTS;

	if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY && m_aClients[ClientID].m_Authed)
	{
		int ConsoleAccessLevel = m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : (AUTHED_SUBADMIN ? IConsole::ACCESS_LEVEL_SUBADMIN : IConsole::ACCESS_LEVEL_MOD);
		for(int i = 0; i < MAX_RCONCMD_SEND && m_aClients[ClientID].m_pRconCmdToSend; ++i)
		{
			SendRconCmdAdd(m_aClients[ClientID].m_pRconCmdToSend, ClientID);
			m_aClients[ClientID].m_pRconCmdToSend = m_aClients[ClientID].m_pRconCmdToSend->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
		}
	}
}

void CServer::ProcessClientPacket(CNetChunk *pPacket)
{
	int ClientID = pPacket->m_ClientID;
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);

	// unpack msgid and system flag
	int Msg = Unpacker.GetInt();
	int Sys = Msg&1;
	Msg >>= 1;

	if(Unpacker.Error())
		return;

	if(Sys)
	{
		// system message
		if(Msg == NETMSG_INFO)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_AUTH)
			{
				const char *pVersion = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(str_comp(pVersion, GameServer()->NetVersion()) != 0)
				{
					// wrong version
					char aReason[256];
					str_format(aReason, sizeof(aReason), "Wrong version. Server is running '%s' and client '%s'", GameServer()->NetVersion(), pVersion);
					m_NetServer.Drop(ClientID, aReason);
					return;
				}

				const char *pPassword = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(g_Config.m_Password[0] != 0 && str_comp(g_Config.m_Password, pPassword) != 0)
				{
					// wrong password
					m_NetServer.Drop(ClientID, "Wrong password");
					return;
				}

				m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
				SendMap(ClientID);
			}
		}
		else if(Msg == NETMSG_REQUEST_MAP_DATA)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) == 0 || m_aClients[ClientID].m_State < CClient::STATE_CONNECTING)
				return;

			int Chunk = Unpacker.GetInt();
			unsigned int ChunkSize = 1024-128;
			unsigned int Offset = Chunk * ChunkSize;
			int Last = 0;

			// drop faulty map data requests
			if(Chunk < 0 || Offset > m_CurrentMapSize)
				return;

			if(Offset+ChunkSize >= m_CurrentMapSize)
			{
				ChunkSize = m_CurrentMapSize - Offset;
				//if (ChunkSize < 0)
				//	ChunkSize = 0;
				Last = 1;
			}

			CMsgPacker Msg(NETMSG_MAP_DATA);
			Msg.AddInt(Last);
			Msg.AddInt(m_CurrentMapCrc);
			Msg.AddInt(Chunk);
			Msg.AddInt(ChunkSize);
			Msg.AddRaw(&m_pCurrentMapData[Offset], ChunkSize);
			SendMsgEx(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID, true);

			if(g_Config.m_Debug)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "sending chunk %d with size %d", Chunk, ChunkSize);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
		else if(Msg == NETMSG_READY)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_CONNECTING)
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player is ready. ClientID=%d addr=%s secure=%s", ClientID, aAddrStr, m_NetServer.HasSecurityToken(ClientID)?"yes":"no");
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
				m_aClients[ClientID].m_State = CClient::STATE_READY;
				GameServer()->OnClientConnected(ClientID);
				SendConnectionReady(ClientID);
			}
		}
		else if(Msg == NETMSG_ENTERGAME)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_READY && GameServer()->IsClientReady(ClientID))
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player has entered the game. ClientID=%x addr=%s", ClientID, aAddrStr);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				m_aClients[ClientID].m_State = CClient::STATE_INGAME;
				GameServer()->OnClientEnter(ClientID);

				// TeeHistorian
				GameServer()->OnClientEngineJoin(ClientID);
            }
		}
		else if(Msg == NETMSG_INPUT)
		{
			CClient::CInput *pInput;
			int64 TagTime;

			m_aClients[ClientID].m_LastAckedSnapshot = Unpacker.GetInt();
			int IntendedTick = Unpacker.GetInt();
			int Size = Unpacker.GetInt();

			// check for errors
			if(Unpacker.Error() || Size/4 > MAX_INPUT_SIZE)
				return;

			if(m_aClients[ClientID].m_LastAckedSnapshot > 0)
				m_aClients[ClientID].m_SnapRate = CClient::SNAPRATE_FULL;

			if(m_aClients[ClientID].m_Snapshots.Get(m_aClients[ClientID].m_LastAckedSnapshot, &TagTime, 0, 0) >= 0)
				m_aClients[ClientID].m_Latency = (int)(((time_get()-TagTime)*1000)/time_freq());

			// add message to report the input timing
			// skip packets that are old
			if(IntendedTick > m_aClients[ClientID].m_LastInputTick)
			{
				int TimeLeft = ((TickStartTime(IntendedTick)-time_get())*1000) / time_freq();

				CMsgPacker Msg(NETMSG_INPUTTIMING);
				Msg.AddInt(IntendedTick);
				Msg.AddInt(TimeLeft);
				SendMsgEx(&Msg, 0, ClientID, true);
			}

			m_aClients[ClientID].m_LastInputTick = IntendedTick;

			pInput = &m_aClients[ClientID].m_aInputs[m_aClients[ClientID].m_CurrentInput];

			if(IntendedTick <= Tick())
				IntendedTick = Tick()+1;

			pInput->m_GameTick = IntendedTick;

			for(int i = 0; i < Size/4; i++)
				pInput->m_aData[i] = Unpacker.GetInt();

			mem_copy(m_aClients[ClientID].m_LatestInput.m_aData, pInput->m_aData, MAX_INPUT_SIZE*sizeof(int));

			m_aClients[ClientID].m_CurrentInput++;
			m_aClients[ClientID].m_CurrentInput %= 200;

			// call the mod with the fresh input data
			if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
				GameServer()->OnClientDirectInput(ClientID, m_aClients[ClientID].m_LatestInput.m_aData);
		}
		else if(Msg == NETMSG_RCON_CMD)
		{
			const char *pCmd = Unpacker.GetString();

			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0 && m_aClients[ClientID].m_Authed)
			{
				
				// try to find first space
				const char *delimiter = strchr(pCmd, ' ');
				
				if(m_aClients[ClientID].m_Authed != AUTHED_SUBADMIN
					|| (
						m_aClients[ClientID].m_Authed == AUTHED_SUBADMIN
						&& delimiter != NULL
						&& m_aClients[ClientID].m_SubAdminAuthPass == std::string(pCmd, delimiter - pCmd)
						&& (pCmd = delimiter + 1) != NULL)
					)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientID=%d rcon='%s'", ClientID, pCmd);
					Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
					m_RconClientID = ClientID;
					m_RconAuthLevel = m_aClients[ClientID].m_Authed;
					Console()->SetAccessLevel(m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : (m_aClients[ClientID].m_Authed == AUTHED_SUBADMIN ? IConsole::ACCESS_LEVEL_SUBADMIN : IConsole::ACCESS_LEVEL_MOD));
					Console()->ExecuteLineFlag(pCmd, CFGFLAG_SERVER, ClientID);
					Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
					m_RconClientID = IServer::RCON_CID_SERV;
					m_RconAuthLevel = AUTHED_SUBADMIN;
				}
				else if(m_aClients[ClientID].m_Authed == AUTHED_SUBADMIN)
				{
					if(m_aClients[ClientID].m_SubAdminCommandPassFails >= 3)
					{
						// logout
						rconLogClientOut(ClientID, "Too many wrong passwords. You were logged out.");
					}
					else
					{
						++m_aClients[ClientID].m_SubAdminCommandPassFails;
						SendRconLine(ClientID, "Wrong password.");
					}
				}
			} 
			else
			{
				
			}

		}
		else if(Msg == NETMSG_RCON_AUTH)
		{
			const char *pPw;
			Unpacker.GetString(); // login name, not used
			pPw = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			
			// try matching username:password login
			const char *delimiter = strchr(pPw, ':');
			std::string username, password;
			loginiterator loginit;
			bool loginWithUsername = (delimiter != NULL);
			if(loginWithUsername)
			{
				username.assign(pPw, delimiter - pPw);
				password.assign(delimiter + 1);
			}
			
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0)
			{
				if(g_Config.m_SvRconPassword[0] == 0 && g_Config.m_SvRconModPassword[0] == 0 && logins.empty())
				{
					SendRconLine(ClientID, "No rcon password set on server. Set sv_rcon_password and/or sv_rcon_mod_password or add logins using add_login to enable the remote console.");
				}
				else if(g_Config.m_SvRconPassword[0] && str_comp(pPw, g_Config.m_SvRconPassword) == 0)
				{
					CMsgPacker Msg(NETMSG_RCON_AUTH_STATUS);
					Msg.AddInt(1);	//authed
					Msg.AddInt(1);	//cmdlist
					SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
					
					m_aClients[ClientID].m_Authed = AUTHED_ADMIN;
					int SendRconCmds = Unpacker.GetInt();
					if(Unpacker.Error() == 0 && SendRconCmds)
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_ADMIN, CFGFLAG_SERVER);
					SendRconLine(ClientID, "Admin authentication successful. Full remote console access granted.");
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientID=%d authed (admin)", ClientID);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					UpdateLoggedInAdmins();
				}
				else if(g_Config.m_SvRconModPassword[0] && str_comp(pPw, g_Config.m_SvRconModPassword) == 0)
				{
					CMsgPacker Msg(NETMSG_RCON_AUTH_STATUS);
					Msg.AddInt(1);	//authed
					Msg.AddInt(1);	//cmdlist
					SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);

					m_aClients[ClientID].m_Authed = AUTHED_MOD;
					int SendRconCmds = Unpacker.GetInt();
					if(Unpacker.Error() == 0 && SendRconCmds)
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_MOD, CFGFLAG_SERVER);
					SendRconLine(ClientID, "Moderator authentication successful. Limited remote console access granted.");
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientID=%d authed (moderator)", ClientID);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				}
				else if(loginWithUsername
					&& !logins.empty()
					&& (loginit = logins.find(username)) != logins.end()
					&& loginit->second == password)
				{
					CMsgPacker Msg(NETMSG_RCON_AUTH_STATUS);
					Msg.AddInt(1);	//authed
					Msg.AddInt(1);	//cmdlist
					SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);
					
					m_aClients[ClientID].m_Authed = AUTHED_SUBADMIN;
					m_aClients[ClientID].m_SubAdminAuthName = loginit->first;
					m_aClients[ClientID].m_SubAdminAuthPass = loginit->second;
					m_aClients[ClientID].m_SubAdminCommandPassFails = 0;
					
					int SendRconCmds = Unpacker.GetInt();
					if(Unpacker.Error() == 0 && SendRconCmds)
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_SUBADMIN, CFGFLAG_SERVER);
					SendRconLine(ClientID, "Subadmin authentication successful. Limited remote console access granted.");
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientID=%d authed (subadmin:%s)", ClientID, loginit->first.c_str());
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					UpdateLoggedInAdmins();
				}
				else if(g_Config.m_SvRconMaxTries)
				{
					m_aClients[ClientID].m_AuthTries++;
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Wrong password %d/%d.", m_aClients[ClientID].m_AuthTries, g_Config.m_SvRconMaxTries);
					SendRconLine(ClientID, aBuf);
					if(m_aClients[ClientID].m_AuthTries >= g_Config.m_SvRconMaxTries)
					{
						if(!g_Config.m_SvRconBantime)
							m_NetServer.Drop(ClientID, "Too many remote console authentication tries");
						else
							m_ServerBan.BanAddr(m_NetServer.ClientAddr(ClientID), g_Config.m_SvRconBantime*60, "Too many remote console authentication tries");
					}
				}
				else
				{
					SendRconLine(ClientID, "Wrong password.");
				}
			}
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker Msg(NETMSG_PING_REPLY);
			SendMsgEx(&Msg, 0, ClientID, true);
		}
		else
		{
			if(g_Config.m_Debug)
			{
				char aHex[] = "0123456789ABCDEF";
				char aBuf[512];

				for(int b = 0; b < pPacket->m_DataSize && b < 32; b++)
				{
					aBuf[b * 3] = aHex[((const unsigned char *)pPacket->m_pData)[b] >> 4];
					aBuf[b * 3 + 1] = aHex[((const unsigned char *)pPacket->m_pData)[b] & 0xf];
					aBuf[b * 3 + 2] = ' ';
					aBuf[b * 3 + 3] = 0;
				}

				char aBufMsg[256];
				str_format(aBufMsg, sizeof(aBufMsg), "strange message ClientID=%d msg=%d data_size=%d", ClientID, Msg, pPacket->m_DataSize);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBufMsg);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
	}
	else
	{
		// game message
		if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State >= CClient::STATE_READY)
			GameServer()->OnMessage(Msg, &Unpacker, ClientID);
	}
}

void CServer::SendServerInfo(const NETADDR *pAddr, int Token)
{
	CNetChunk Packet;
	CPacker p;
	char aBuf[128];

	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	p.Reset();

	p.AddRaw(SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO));
	str_format(aBuf, sizeof(aBuf), "%d", Token);
	p.AddString(aBuf, 6);

	p.AddString(GameServer()->Version(), 32);
	// send the alternative server name when a admin is online
	p.AddString((m_numLoggedInAdmins && str_length(g_Config.m_SvNameAdmin)) ? g_Config.m_SvNameAdmin : g_Config.m_SvName, 64);
	p.AddString(GetMapName(), 32);

	// gametype
	p.AddString(GameServer()->GameType(), 16);

	// flags
	int i = 0;
	if(g_Config.m_Password[0]) // password set
		i |= SERVER_FLAG_PASSWORD;
	str_format(aBuf, sizeof(aBuf), "%d", i);
	p.AddString(aBuf, 2);

	str_format(aBuf, sizeof(aBuf), "%d", PlayerCount); p.AddString(aBuf, 3); // num players
	str_format(aBuf, sizeof(aBuf), "%d", m_NetServer.MaxClients()-g_Config.m_SvSpectatorSlots); p.AddString(aBuf, 3); // max players
	str_format(aBuf, sizeof(aBuf), "%d", ClientCount); p.AddString(aBuf, 3); // num clients
	str_format(aBuf, sizeof(aBuf), "%d", m_NetServer.MaxClients()); p.AddString(aBuf, 3); // max clients

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			p.AddString(ClientName(i), MAX_NAME_LENGTH); // client name
			p.AddString(ClientClan(i), MAX_CLAN_LENGTH); // client clan
			str_format(aBuf, sizeof(aBuf), "%d", m_aClients[i].m_Country); p.AddString(aBuf, 6); // client country
			str_format(aBuf, sizeof(aBuf), "%d", m_aClients[i].m_Score); p.AddString(aBuf, 6); // client score
			str_format(aBuf, sizeof(aBuf), "%d", GameServer()->IsClientPlayer(i)?1:0); p.AddString(aBuf, 2); // is player?
		}
	}

	Packet.m_ClientID = -1;
	Packet.m_Address = *pAddr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = p.Size();
	Packet.m_pData = p.Data();
	m_NetServer.Send(&Packet);
}

void CServer::UpdateServerInfo()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			SendServerInfo(m_NetServer.ClientAddr(i), -1);
	}
}


void CServer::PumpNetwork()
{
	CNetChunk Packet;

	m_NetServer.Update();

	// process packets
	while(m_NetServer.Recv(&Packet))
	{
		if(Packet.m_ClientID == -1)
		{
			// stateless
			if(!m_Register.RegisterProcessPacket(&Packet))
			{
				if(Packet.m_DataSize == sizeof(SERVERBROWSE_GETINFO)+1 &&
					mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
				{
					SendServerInfo(&Packet.m_Address, ((unsigned char *)Packet.m_pData)[sizeof(SERVERBROWSE_GETINFO)]);
				}
			}
		}
		else
			ProcessClientPacket(&Packet);
	}

	m_ServerBan.Update();
	m_Econ.Update();
#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Update();
#endif
}

char *CServer::GetMapName()
{
	// get the name of the map without his path
	char *pMapShortName = &g_Config.m_SvMap[0];
	for(int i = 0; i < str_length(g_Config.m_SvMap)-1; i++)
	{
		if(g_Config.m_SvMap[i] == '/' || g_Config.m_SvMap[i] == '\\')
			pMapShortName = &g_Config.m_SvMap[i+1];
	}
	return pMapShortName;
}

int CServer::LoadMap(const char *pMapName)
{
	//DATAFILE *df;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);

	/*df = datafile_load(buf);
	if(!df)
		return 0;*/

	// check for valid standard map
	if(!m_MapChecker.ReadAndValidateMap(Storage(), aBuf, IStorage::TYPE_ALL))
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mapchecker", "invalid standard map");
		return 0;
	}

	if(!m_pMap->Load(aBuf))
		return 0;

	// stop recording when we change map
	m_DemoRecorder.Stop();

	// reinit snapshot ids
	m_IDPool.TimeoutIDs();

	// get the crc of the map
	m_CurrentMapCrc = m_pMap->Crc();
	char aBufMsg[256];
	str_format(aBufMsg, sizeof(aBufMsg), "%s crc is %08x", aBuf, m_CurrentMapCrc);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);

	str_copy(m_aCurrentMap, pMapName, sizeof(m_aCurrentMap));
	//map_set(df);

	// load complete map into memory for download
	{
		IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		m_CurrentMapSize = (unsigned int)io_length(File);
		if (m_pCurrentMapData)
			mem_free(m_pCurrentMapData);
		m_pCurrentMapData = (unsigned char *)mem_alloc(m_CurrentMapSize, 1);
		io_read(File, m_pCurrentMapData, m_CurrentMapSize);
		io_close(File);
	}
	return 1;
}

void CServer::InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, IConsole *pConsole)
{
	m_Register.Init(pNetServer, pMasterServer, pConsole);
}

int CServer::Run()
{
	//
	m_PrintCBIndex = Console()->RegisterPrintCallback(g_Config.m_ConsoleOutputLevel, SendRconLineAuthed, this);

	// load map
	if(!LoadMap(g_Config.m_SvMap))
	{
		dbg_msg("server", "failed to load map. mapname='%s'", g_Config.m_SvMap);
		return -1;
	}

	// start server
	NETADDR BindAddr;
	if(g_Config.m_Bindaddr[0] && net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
	{
		// sweet!
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = g_Config.m_SvPort;
	}
	else
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = g_Config.m_SvPort;
	}

	if(!m_NetServer.Open(BindAddr, &m_ServerBan, g_Config.m_SvMaxClients, g_Config.m_SvMaxClientsPerIP, 0))
	{
		dbg_msg("server", "couldn't open socket. port %d might already be in use", g_Config.m_SvPort);
		return -1;
	}

	m_NetServer.SetCallbacks(NewClientCallback, NewClientNoAuthCallback, DelClientCallback, this);

	m_Econ.Init(Console(), &m_ServerBan);

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Init(Console(), g_Config.m_SvInputFifo, CFGFLAG_SERVER);
#endif

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", g_Config.m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	GameServer()->OnInit();
	if(ErrorShutdown())
	{
		return 1;
	}
	str_format(aBuf, sizeof(aBuf), "version %s", GameServer()->NetVersion());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// process pending commands
	m_pConsole->StoreCommands(false);

	// start game
	{
		int64 ReportTime = time_get();
		int ReportInterval = 3;

		m_Lastheartbeat = 0;
		m_GameStartTime = time_get();

		if(g_Config.m_Debug)
		{
			str_format(aBuf, sizeof(aBuf), "baseline memory usage %dk", mem_stats()->allocated/1024);
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}

		while(m_RunServer)
		{
			int64 t = time_get();
			int NewTicks = 0;

			// load new map TODO: don't poll this
			if(str_comp(g_Config.m_SvMap, m_aCurrentMap) != 0 || m_MapReload)
			{
				m_MapReload = 0;

				// load map
				if(LoadMap(g_Config.m_SvMap))
				{
					// new map loaded
					GameServer()->OnShutdown();

					for(int c = 0; c < MAX_CLIENTS; c++)
					{
						if(m_aClients[c].m_State <= CClient::STATE_AUTH)
							continue;

						SendMap(c);
						m_aClients[c].Reset();
						m_aClients[c].m_State = CClient::STATE_CONNECTING;
					}

					m_GameStartTime = time_get();
					AdjustVotebanTime(m_CurrentGameTick);
					m_CurrentGameTick = 0;
					Kernel()->ReregisterInterface(GameServer());
					GameServer()->OnInit();
					if(ErrorShutdown())
					{
						break;
					}
					UpdateServerInfo();
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "failed to load map. mapname='%s'", g_Config.m_SvMap);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					str_copy(g_Config.m_SvMap, m_aCurrentMap, sizeof(g_Config.m_SvMap));
				}
			}

			while(t > TickStartTime(m_CurrentGameTick+1))
			{
				m_CurrentGameTick++;
				NewTicks++;

				// apply new input
				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					if(m_aClients[c].m_State == CClient::STATE_EMPTY)
						continue;
					for(int i = 0; i < 200; i++)
					{
						if(m_aClients[c].m_aInputs[i].m_GameTick == Tick())
						{
							if(m_aClients[c].m_State == CClient::STATE_INGAME)
								GameServer()->OnClientPredictedInput(c, m_aClients[c].m_aInputs[i].m_aData);
							break;
						}
					}
				}

				GameServer()->OnTick();
				if(ErrorShutdown())
				{
					break;
				}
			}

			// snap game
			if(NewTicks)
			{
				if(g_Config.m_SvHighBandwidth || (m_CurrentGameTick%2) == 0)
					DoSnapshot();

				UpdateClientRconCommands();
			}

			// master server stuff
			m_Register.RegisterUpdate(m_NetServer.NetType());

			PumpNetwork();

			if(ReportTime < time_get())
			{
				if(g_Config.m_Debug)
				{
					/*
					static NETSTATS prev_stats;
					NETSTATS stats;
					netserver_stats(net, &stats);

					perf_next();

					if(config.dbg_pref)
						perf_dump(&rootscope);

					dbg_msg("server", "send=%8d recv=%8d",
						(stats.send_bytes - prev_stats.send_bytes)/reportinterval,
						(stats.recv_bytes - prev_stats.recv_bytes)/reportinterval);

					prev_stats = stats;
					*/
				}

				ReportTime += time_freq()*ReportInterval;
			}

			// wait for incomming data
			net_socket_read_wait(m_NetServer.Socket(), 5);
		}
	}

	const char *pDisconnectReason = "Server shutdown";
	if(ErrorShutdown())
	{
		pDisconnectReason = m_aErrorShutdownReason;
	}
	// disconnect all clients on shutdown
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			m_NetServer.Drop(i, pDisconnectReason);

		m_Econ.Shutdown();
	}
#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Shutdown();
#endif

	GameServer()->OnShutdown();
	m_pMap->Unload();

	if(m_pCurrentMapData)
		mem_free(m_pCurrentMapData);
	return 0;
}

// returns the time in seconds that the client is votebanned or 0 if he isn't
int CServer::ClientVotebannedTime(int ClientID)
{
	CVoteban **v = IsVotebannedAddr(m_NetServer.ClientAddr(ClientID));
	if(v != NULL && (*v)->m_Expire > Tick())
		return ((*v)->m_Expire - Tick()) / TickSpeed();
	return 0;
}

// decreases the time of all votebans by the given offset of ticks
void CServer::AdjustVotebanTime(int offset)
{
	CleanVotebans();
	CVoteban *v = m_Votebans;
	while(v != NULL)
	{
		v->m_Expire -= offset;
		v = v->m_Next;
	}
}

// adds a new voteban for a specific address
void CServer::AddVotebanAddr(const NETADDR *addr, int expire)
{
	CVoteban **v = IsVotebannedAddr(addr);
	// create new
	if(!v)
	{
		CVoteban *v = new CVoteban;
		v->m_Addr = *addr;
		v->m_Expire = expire;
		// insert front
		v->m_Next = m_Votebans;
		m_Votebans = v;
	}
	// update existing entry
	else
		(*v)->m_Expire = expire;
}

// adds a new voteban for a client's address
void CServer::AddVoteban(int ClientID, int time)
{
	int expire = Tick() + time * TickSpeed();
	AddVotebanAddr(m_NetServer.ClientAddr(ClientID), expire);
}

// removes a voteban from a client's address
void CServer::RemoveVotebanClient(int ClientID)
{
	RemoveVotebanAddr(m_NetServer.ClientAddr(ClientID));
}

// removes a voteban on an address
void CServer::RemoveVotebanAddr(const NETADDR *addr)
{
	CVoteban **v = IsVotebannedAddr(addr);
	if(v != NULL)
		RemoveVoteban(v);
}

// removes a voteban
void CServer::RemoveVoteban(CVoteban **v)
{
	CVoteban *next = (*v)->m_Next;
	delete *v;
	*v = next;
}

// returns the voteban with the given address if it exists
CServer::CVoteban **CServer::IsVotebannedAddr(const NETADDR *addr)
{
	CVoteban **v = &m_Votebans;
	while(*v != NULL)
	{
		// only check ip-type and ip, not port
		if((*v)->m_Addr.type == addr->type && !mem_comp(&(*v)->m_Addr.ip, &addr->ip, sizeof(unsigned char[16])))
			return v;
		v = &(*v)->m_Next;
	}
	return NULL;
}

// removes expired votebans
void CServer::CleanVotebans()
{
	CVoteban **v = &m_Votebans;
	while(*v != NULL)
	{
		if((*v)->m_Expire <= Tick())
		{
			CVoteban *next = (*v)->m_Next;
			delete *v;
			*v = next;
		}
		else
			v = &(*v)->m_Next;
	}
}

void CServer::ConVoteban(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	int ClientID = pResult->GetInteger(0);
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || pThis->m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
		return;
	}
	int time = (pResult->NumArguments() > 1) ? pResult->GetInteger(1) : 300;
	pThis->AddVoteban(ClientID, time);
	// message to console and chat
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' has been banned from voting for %d:%02d min.", pThis->ClientName(ClientID), time/60, time%60);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	pThis->GameServer()->InformPlayers(aBuf);
}

void CServer::ConUnvoteban(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	
	// index to unvoteban
	int index = pResult->GetInteger(0);
	
	CVoteban **v = &pThis->m_Votebans;
	for(int c = 0; *v != NULL; ++c)
	{
		// index found
		if(index == c)
		{
			char aBuf[128], aAddrStr[NETADDR_MAXSTRSIZE];
			// print to console
			net_addr_str(&(*v)->m_Addr, aAddrStr, sizeof(aAddrStr), false);
			str_format(aBuf, sizeof(aBuf), "%s has been un-votebanned.", aAddrStr);
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
			// remove ban
			pThis->RemoveVoteban(v);
			// don't look any further
			return;
		}
		v = &(*v)->m_Next;
	}
	
	// not found
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "index was not found, please use 'votebans' to obtain an index");
}

void CServer::ConUnvotebanClient(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	int ClientID = pResult->GetInteger(0);
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || pThis->m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Invalid ClientID");
		return;
	}
	pThis->RemoveVotebanClient(ClientID);
	// message to console
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' has been un-votebanned.", pThis->ClientName(ClientID));
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
}

void CServer::ConVotebans(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	char aBuf[128];
	char aAddrStr[NETADDR_MAXSTRSIZE];
	int time;
	int count = 0;
	
	pThis->CleanVotebans();
	CVoteban *v = pThis->m_Votebans;
	NETADDR addr;
	while(v != NULL)
	{
		addr.type = v->m_Addr.type;
		mem_copy(addr.ip, v->m_Addr.ip, sizeof(unsigned char[16]));
		net_addr_str(&addr, aAddrStr, sizeof(aAddrStr), false);
		time = (v->m_Expire - pThis->Tick()) / pThis->TickSpeed();
		str_format(aBuf, sizeof(aBuf), "#%d addr=%s time=%d:%02d min", count++, aAddrStr, time/60, time%60);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		v = v->m_Next;
	}
	
	str_format(aBuf, sizeof(aBuf), "%d votebanned ip(s)", count);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
}

void CServer::ConAddLogin(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	char aBuf[128];
	std::string username(pResult->GetString(0)),
		password(pResult->GetString(1));
	
	// insert if doesn't exist
	if(pThis->logins.find(username) == pThis->logins.end())
	{
		pThis->logins[username] = password;
		
		str_format(aBuf, sizeof(aBuf), "Added login for '%s'.", username.c_str());
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	}
	else
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Did not overwrite existing login.");
	}
}

void CServer::ConRemoveLogin(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	char aBuf[128];
	loginiterator loginit;
	std::string username(pResult->GetString(0));
	
	// delete if exists
	if((loginit = pThis->logins.find(username)) != pThis->logins.end())
	{
		// check if someone is logged in with this login
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY && pThis->m_aClients[i].m_Authed == CServer::AUTHED_SUBADMIN && pThis->m_aClients[i].m_SubAdminAuthName == loginit->first)
			{
				pThis->rconLogClientOut(i, "You were logged out.");
			}
		}
		
		// finally delete
		pThis->logins.erase(loginit);
		
		str_format(aBuf, sizeof(aBuf), "Removed login '%s'.", username.c_str());
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
	}
	else
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "No login with that username.");
	}
}

void CServer::ConAddInfo(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	int interval = pResult->GetInteger(0);
	if(interval < 1 || interval > 1440)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Interval must be between 1 and 1440.");
		return;
	}
	
	// add info text
	CInfoText *t = new CInfoText;
	t->m_Interval = interval;
	t->m_Text = std::string(pResult->GetString(1));
	// insert front
	t->m_Next = pThis->m_InfoTexts;
	pThis->m_InfoTexts = t;
	
	pThis->UpdateInfoTexts();
	
	// message to console
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Added the following info text: %s", t->m_Text.c_str());
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
}

void CServer::ConRemoveInfo(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	int i = pResult->GetInteger(0);
	int count = 0;
	bool removed = false;
	
	CInfoText **t = &(pThis->m_InfoTexts);
	while(*t != NULL)
	{
		if(count++ == i)
		{
			// remove
			CInfoText *next = (*t)->m_Next;
			delete *t;
			*t = next;
			removed = true;
			break;
		}
		t = &((*t)->m_Next);
	}
	
	pThis->UpdateInfoTexts();
	
	if(removed)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Info text removed");
	else
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Info text not found");
}

void CServer::ConListInfo(IConsole::IResult *pResult, void *pUser)
{
	CServer* pThis = static_cast<CServer *>(pUser);
	char aBuf[128];
	int count = 0;
	
	CInfoText *t = pThis->m_InfoTexts;
	while(t != NULL)
	{
		str_format(aBuf, sizeof(aBuf), "#%d interval=%d min text='%s'", count++, t->m_Interval, t->m_Text.c_str());
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		t = t->m_Next;
	}
	
	str_format(aBuf, sizeof(aBuf), "%d info text(s)", count);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
}

void CServer::UpdateInfoTexts()
{
	// in case that there are no info texts
	if(m_InfoTexts == NULL)
	{
		m_InfoTextInterval = -1; // no interval
		return;
	}
	
	// need some random numbers later
	init_rand();
	
	// update interval, set it to LCM(all text intervals)
	m_InfoTextInterval = 1; // lowest possible interval
	CInfoText *t = m_InfoTexts;
	while(t != NULL)
	{
		m_InfoTextInterval = lcm(m_InfoTextInterval, t->m_Interval);
		t->m_IntervalTicks = TickSpeed() * 60 * t->m_Interval; // min to ticks
		t->m_NextTick = rand() % t->m_IntervalTicks; // variance
		t = t->m_Next;
	}
	m_InfoTextInterval *= TickSpeed() * 60; // min to ticks
	
	// count total number of messages per interval
	int numMsg = 0;
	t = m_InfoTexts;
	while(t != NULL)
	{
		numMsg += m_InfoTextInterval / t->m_IntervalTicks;
		t = t->m_Next;
	}
	
	// interval between messages
	m_InfoTextMsgInterval = m_InfoTextInterval / numMsg;
	// additional pause to sync interval and msg interval
	m_InfoTextIntervalPause = m_InfoTextInterval % numMsg;
}

std::string CServer::GetNextInfoText()
{
	CInfoText *selectedText = NULL,
		*t = m_InfoTexts;
	// the counter r keeps track of how many equally due texts there are and actually helps so that every text has the same chance even though there are r-1 random decisions
	int r = 1;
	while(t != NULL)
	{
		// use this text if none is selected or this one is more due than the previously selected
		// in the case that both are equally due, select one random
		if(selectedText == NULL
			|| t->m_NextTick < selectedText->m_NextTick
			|| (t->m_NextTick == selectedText->m_NextTick && (rand() % ++r) == 0))
			selectedText = t;
		t = t->m_Next;
	}
	
	// return empty string if no text applies
	if(selectedText == NULL)
		return std::string();
	
	// update tick and return string
	selectedText->m_NextTick += selectedText->m_IntervalTicks;
	return selectedText->m_Text;
}

void CServer::ConKick(IConsole::IResult *pResult, void *pUser)
{
	if(pResult->NumArguments() > 1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Kicked (%s)", pResult->GetString(1));
		((CServer *)pUser)->Kick(pResult->GetInteger(0), aBuf);
	}
	else
		((CServer *)pUser)->Kick(pResult->GetInteger(0), "Kicked by console");
}

void CServer::ConStatus(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[1024];
	char aAddrStr[NETADDR_MAXSTRSIZE];
	CServer* pThis = static_cast<CServer *>(pUser);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			net_addr_str(pThis->m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), true);
			if(pThis->m_aClients[i].m_State == CClient::STATE_INGAME)
			{
				char bBuf[32];
				str_format(bBuf, sizeof(bBuf), "(Subadmin:%s)", pThis->m_aClients[i].m_SubAdminAuthName.c_str());
				const char *pAuthStr = pThis->m_aClients[i].m_Authed == CServer::AUTHED_ADMIN ? "(Admin)" :
										pThis->m_aClients[i].m_Authed == CServer::AUTHED_SUBADMIN ? bBuf :
										pThis->m_aClients[i].m_Authed == CServer::AUTHED_MOD ? "(Mod)" : "";
				const char *pAimBotStr = pThis->GameServer()->IsClientAimBot(i) ? "[aimbot]" : "";

				const char *securelyConnected = pThis->m_NetServer.HasSecurityToken(i) ? "Yes" : "No";

				str_format(aBuf, sizeof(aBuf), "id:%2d  addr:%21s secure:%-3s  score:%03d name: '%s' clan: '%s' %s %s", 
					i, 
					aAddrStr,
					securelyConnected,
					pThis->m_aClients[i].m_Score, 
					pThis->m_aClients[i].m_aName, 
					pThis->m_aClients[i].m_aClan, 
					pAuthStr, 
					pAimBotStr);
			}
			else
				str_format(aBuf, sizeof(aBuf), "id=%2d addr=%21s connecting", i, aAddrStr);
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}
	}
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", "Quick help: kick <id> <reason=''> | ban <id> <min=5> <reason=''> | voteban <id> <sec=300> | kill <id>");
}

void CServer::ConShutdown(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_RunServer = 0;
}

void CServer::DemoRecorder_HandleAutoStart()
{
	if(g_Config.m_SvAutoDemoRecord)
	{
		m_DemoRecorder.Stop();
		char aFilename[128];
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/auto/%s_%s.demo", g_Config.m_SvAutoDemoPrefix, aDate);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "foobar", aFilename);
		m_DemoRecorder.Start(Storage(), m_pConsole, aFilename, GameServer()->NetVersion(), m_aCurrentMap, m_CurrentMapCrc, "server");
		if(g_Config.m_SvAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/server", "autorecord", ".demo", g_Config.m_SvAutoDemoMax);
		}
	}
}

void CServer::MapReload()
{
	m_MapReload = 1;
}

bool CServer::DemoRecorder_IsRecording()
{
	return m_DemoRecorder.IsRecording();
}

void CServer::ConRecord(IConsole::IResult *pResult, void *pUser)
{
	CServer* pServer = (CServer *)pUser;
	char aFilename[128];

	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/demo_%s.demo", aDate);
	}
	pServer->m_DemoRecorder.Start(pServer->Storage(), pServer->Console(), aFilename, pServer->GameServer()->NetVersion(), pServer->m_aCurrentMap, pServer->m_CurrentMapCrc, "server");
}

void CServer::ConStopRecord(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_DemoRecorder.Stop();
}

void CServer::ConMapReload(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_MapReload = 1;
}

void CServer::ConLogout(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;
	pServer->rconLogClientOut(pServer->m_RconClientID);
}

void CServer::rconLogClientOut(int ClientID, const char *msg)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State != CServer::CClient::STATE_EMPTY && IsAuthed(ClientID))
	{
		CMsgPacker Msg(NETMSG_RCON_AUTH_STATUS);
		Msg.AddInt(0);	//authed
		Msg.AddInt(0);	//cmdlist
		SendMsgEx(&Msg, MSGFLAG_VITAL, ClientID, true);

		m_aClients[ClientID].m_Authed = AUTHED_NO;
		m_aClients[ClientID].m_AuthTries = 0;
		m_aClients[ClientID].m_pRconCmdToSend = 0;

		SendRconLine(ClientID, msg);
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "ClientID=%d logged out", ClientID);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		UpdateLoggedInAdmins();
	}
}

void CServer::ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CServer *)pUserData)->UpdateServerInfo();
}

void CServer::ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CServer *)pUserData)->m_NetServer.SetMaxClientsPerIP(pResult->GetInteger(0));
}

void CServer::ConchainModCommandUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	if(pResult->NumArguments() == 2)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		const IConsole::CCommandInfo *pInfo = pThis->Console()->GetCommandInfo(pResult->GetString(0), CFGFLAG_SERVER, false);
		int OldAccessLevel = 0;
		if(pInfo)
			OldAccessLevel = pInfo->GetAccessLevel();
		pfnCallback(pResult, pCallbackUserData);
		if(pInfo && OldAccessLevel != pInfo->GetAccessLevel())
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(pThis->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY || pThis->m_aClients[i].m_Authed != CServer::AUTHED_MOD ||
					(pThis->m_aClients[i].m_pRconCmdToSend && str_comp(pResult->GetString(0), pThis->m_aClients[i].m_pRconCmdToSend->m_pName) >= 0))
					continue;

				if(OldAccessLevel == IConsole::ACCESS_LEVEL_ADMIN)
					pThis->SendRconCmdAdd(pInfo, i);
				else
					pThis->SendRconCmdRem(pInfo, i);
			}
		}
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->Console()->SetPrintOutputLevel(pThis->m_PrintCBIndex, pResult->GetInteger(0));
	}
}

void CServer::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGameServer = Kernel()->RequestInterface<IGameServer>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	// register console commands
	Console()->Register("kick", "i?r", CFGFLAG_SERVER, ConKick, this, "Kick player with specified id for any reason");
	Console()->Register("status", "", CFGFLAG_SERVER, ConStatus, this, "List players");
	Console()->Register("shutdown", "", CFGFLAG_SERVER, ConShutdown, this, "Shut down");
	Console()->Register("logout", "", CFGFLAG_SERVER, ConLogout, this, "Logout of rcon");

	Console()->Register("record", "?s", CFGFLAG_SERVER|CFGFLAG_STORE, ConRecord, this, "Record to a file");
	Console()->Register("stoprecord", "", CFGFLAG_SERVER, ConStopRecord, this, "Stop recording");

	Console()->Register("reload", "", CFGFLAG_SERVER, ConMapReload, this, "");

	Console()->Chain("sv_name", ConchainSpecialInfoupdate, this);
	Console()->Chain("sv_name_admin", ConchainSpecialInfoupdate, this);
	Console()->Chain("password", ConchainSpecialInfoupdate, this);

	Console()->Chain("sv_max_clients_per_ip", ConchainMaxclientsperipUpdate, this);
	Console()->Chain("mod_command", ConchainModCommandUpdate, this);
	Console()->Chain("console_output_level", ConchainConsoleOutputLevelUpdate, this);
	
	Console()->Register("voteban", "i?i", CFGFLAG_SERVER, ConVoteban, this, "Voteban a player by id");
	Console()->Register("unvoteban", "i", CFGFLAG_SERVER, ConUnvoteban, this, "Remove voteban by index in list votebans");
	Console()->Register("unvoteban_client", "i", CFGFLAG_SERVER, ConUnvotebanClient, this, "Remove voteban by player id");
	Console()->Register("votebans", "", CFGFLAG_SERVER, ConVotebans, this, "Show all votebans");
	
	Console()->Register("add_login", "ss", CFGFLAG_SERVER, ConAddLogin, this, "Add a subadmin login. The rcon password will be user:pass with no additional spaces.", IConsole::ACCESS_LEVEL_ADMIN);
	Console()->Register("remove_login", "s", CFGFLAG_SERVER, ConRemoveLogin, this, "Remove a subadmin login", IConsole::ACCESS_LEVEL_ADMIN);
	
	Console()->Register("add_info", "is", CFGFLAG_SERVER, ConAddInfo, this, "Add a info text that is printed in the chat repeatedly in the given interval of minutes.");
	Console()->Register("remove_info", "i", CFGFLAG_SERVER, ConRemoveInfo, this, "Remove a info text");
	Console()->Register("list_info", "", CFGFLAG_SERVER, ConListInfo, this, "Show all info texts");

	// register console commands in sub parts
	m_ServerBan.InitServerBan(Console(), Storage(), this);
	m_pGameServer->OnConsoleInit();
}


int CServer::SnapNewID()
{
	return m_IDPool.NewID();
}

void CServer::SnapFreeID(int ID)
{
	m_IDPool.FreeID(ID);
}


void *CServer::SnapNewItem(int Type, int ID, int Size)
{
	dbg_assert(Type >= 0 && Type <=0xffff, "incorrect type");
	dbg_assert(ID >= 0 && ID <=0xffff, "incorrect id");
	return ID < 0 ? 0 : m_SnapshotBuilder.NewItem(Type, ID, Size);
}

void CServer::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

static CServer *CreateServer() { return new CServer(); }

int main(int argc, const char **argv) // ignore_convention
{
#if defined(CONF_FAMILY_WINDOWS)
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0) // ignore_convention
		{
			ShowWindow(GetConsoleWindow(), SW_HIDE);
			break;
		}
	}
#endif

	if (secure_random_init() != 0)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	CServer *pServer = CreateServer();
	IKernel *pKernel = IKernel::Create();

	// create the components
	IEngine *pEngine = CreateEngine("Teeworlds");
	IEngineMap *pEngineMap = CreateEngineMap();
	IGameServer *pGameServer = CreateGameServer();
	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER|CFGFLAG_ECON);
	IEngineMasterServer *pEngineMasterServer = CreateEngineMasterServer();
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv); // ignore_convention
	IConfig *pConfig = CreateConfig();

	pServer->InitRegister(&pServer->m_NetServer, pEngineMasterServer, pConsole);

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pServer); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap*>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pEngineMap));
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pGameServer);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfig);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMasterServer*>(pEngineMasterServer)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMasterServer*>(pEngineMasterServer));

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfig->Init();
	pEngineMasterServer->Init();
	pEngineMasterServer->Load();

	// register all console commands
	pServer->RegisterCommands();

	/* This banmaster is added into the source of the server that i'm able to ban players even if no banmaster.cfg is used.
	 * Often serverhoster doesn't add this file because they don't know what it is for and remove it, not
	 * because they don't want it. If so, set sv_global_bantime to 0 or use a custom banmasters.cfg with "clear_banmasters"
	 * in first line or in normal config.
	 * ## For a Teeworlds without bots \o/ ##
	 */
	pConsole->ExecuteLine("add_banmaster banmaster.teetw.de");

	// execute autoexec file
	pConsole->ExecuteFile("autoexec.cfg");

	// parse the command line arguments
	if(argc > 1) // ignore_convention
		pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention

	// restore empty config strings to their defaults
	pConfig->RestoreStrings();

	pEngine->InitLogfile();

	// run the server
	dbg_msg("server", "starting...");
	pServer->Run();

	// free
	delete pServer;
	delete pKernel;
	delete pEngineMap;
	delete pGameServer;
	delete pConsole;
	delete pEngineMasterServer;
	delete pStorage;
	delete pConfig;
	return 0;
}

void CServer::SetErrorShutdown(const char *pReason)
{
	str_copy(m_aErrorShutdownReason, pReason, sizeof(m_aErrorShutdownReason));
}
