#include "voteoptionserver.h"
#include <algorithm>

CVoteOptionServerExtended::CVoteOptionServerExtended(CGameContext *pGameServer) : m_pGameServer{pGameServer}
{
}

void CVoteOptionServerExtended::AddVoteOptionHandler(std::string Description, std::string Command, std::function<bool(int, std::string&, std::string&, std::string&)> Callback)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        m_CustomVoteOptionHandlers[i].push_back({Description, Command, Callback});
    }
    m_DefaultCustomVoteOptionHandlers.push_back({Description, Command, Callback});
}

void CVoteOptionServerExtended::SendClearVoteOptions(int ofID)
{
    CMsgPacker Msg(NETMSGTYPE_SV_VOTECLEAROPTIONS);
    Server()->SendMsg(&Msg, MSGFLAG_VITAL, ofID);
}

void CVoteOptionServerExtended::SendDefaultVoteOptions(int toID)
{
	class CVoteOptionServer *pCurrent = GameServer()->m_pVoteOptionFirst;

	while(pCurrent)
	{

		// count options for actual packet
		int NumOptions = 0;
		for(CVoteOptionServer *p = pCurrent; p && NumOptions < MAX_VOTE_OPTION_ADD; p = p->m_pNext, ++NumOptions);

		// pack and add vote list packet
		CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
		Msg.AddInt(NumOptions);
		while(pCurrent && NumOptions--)
		{
			Msg.AddString(pCurrent->m_aDescription, VOTE_DESC_LENGTH);
			pCurrent = pCurrent->m_pNext;
		}

        Server()->SendMsg(&Msg, MSGFLAG_VITAL, toID);
	}
}

void CVoteOptionServerExtended::SendCustomVoteOptions(int toID)
{
    CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
    Msg.AddInt(m_CustomVoteOptionHandlers[toID].size());

    for (auto& tuple : m_CustomVoteOptionHandlers[toID])
    {
        std::string& Description = std::get<0>(tuple);
        Msg.AddString(Description.c_str(), VOTE_DESC_LENGTH);
    }

    Server()->SendMsg(&Msg, MSGFLAG_VITAL, toID);    
}

void CVoteOptionServerExtended::RefreshVoteOptions(int ofID)
{

    // clear votes
    SendClearVoteOptions(ofID);

    // add default vote options
    SendDefaultVoteOptions(ofID);

    // add custm vote options.
    SendCustomVoteOptions(ofID);

}

bool CVoteOptionServerExtended::ExecuteVoteOption(int ofID, std::string ReceivedDescription, std::string ReceivedReason)
{
    for (auto &[Description, Command, Callback] : m_CustomVoteOptionHandlers[ofID])
    {
        if (ReceivedDescription == Description)
        {
            // only refresh votes if the callback requests it by returning true.
            if(Callback(ofID, Description, Command, ReceivedReason))
                RefreshVoteOptions(ofID);
            return true;
        }
    }
    return false;
}

void CVoteOptionServerExtended::ResetCustomVoteOptionsToDefault(int ofID)
{
    size_t size = m_DefaultCustomVoteOptionHandlers.size();
    for (size_t i = 0; i < size; i++)
    {
        auto& [DefaultDescription, DefaultCommand, DefaultCallback] = m_DefaultCustomVoteOptionHandlers[i];
        auto& [Description, Command, Callback] = m_CustomVoteOptionHandlers[ofID][i];

        Description = DefaultDescription;
        Command = DefaultCommand;
        Callback = DefaultCallback;
    }
    
}

void CVoteOptionServerExtended::OnPlayerConnect(int PlayerID)
{
    ResetCustomVoteOptionsToDefault(PlayerID);
    RefreshVoteOptions(PlayerID);
}


