#include "voteoptionserver.h"
#include <algorithm>

CVoteOptionServerExtended::CVoteOptionServerExtended(CGameContext *pGameServer) : m_pGameServer{pGameServer}
{

}

CVoteOptionServerExtended::~CVoteOptionServerExtended()
{
    AwaitFutures();
}

void CVoteOptionServerExtended::Test(int forID)
{   
    RefreshVoteOptions(forID);
}

void CVoteOptionServerExtended::CleanupFutures()
{
    if (m_Futures.size() == 0)
        return;

    auto it = std::remove_if(m_Futures.begin(), m_Futures.end(), [&](std::future<void>& f) {
        if (!f.valid())
            return true;

        if (std::future_status::ready == f.wait_for(std::chrono::milliseconds(0)))
        {
            try
            {
                f.get();
            }
            catch (const std::exception& e)
            {
                dbg_msg("ERROR", "CVoteoptionsServer: Cleaning up features: %s", e.what());
            }

            return true;
        }
        return false;
    });
    m_Futures.erase(it, m_Futures.end());
}

void CVoteOptionServerExtended::AwaitFutures()
{
    for (auto& f : m_Futures)
    {
        if (f.valid())
        {
            f.wait();
            try
            {
                f.get();
            }
            catch (const std::exception& e)
            {
                dbg_msg("ERROR", "CVoteoptionsServer: Awaiting features: %s", e.what());
            }
        }
    }

    m_Futures.clear();
}


void CVoteOptionServerExtended::Tick()
{
    // messages are being sent in the main thread.
    if (m_VoteMessageQueueMutex.try_lock())
    {
        if(m_VoteMessageQueue.size() <= 0)
        {
            m_VoteMessageQueueMutex.unlock();
            return;
        }

        dbg_msg("DEBUG", "Tick: Before processing vote msg vector");
        // send messages to the player, that was requesting them.
        for (auto& [toID, Flags, Msg] : m_VoteMessageQueue)
        {
            dbg_msg("DEBUG", "Tick Data: %s ", Msg.Data());
            Server()->SendMsg(&Msg, Flags, toID);
        }

        dbg_msg("DEBUG", "Tick: After processing vote msg vector");

        // remove all elements from the container
        m_VoteMessageQueue.clear();
        
        m_VoteMessageQueueMutex.unlock();
    }
}

void CVoteOptionServerExtended::RefreshVoteOptions(int ofID)
{

    m_Futures.push_back(std::async(std::launch::async, [this](int ID){
        
        std::lock_guard<std::mutex> lock(m_VoteMessageQueueMutex);
        // TODO: lock mutex that handles the vanilla vote option access.
        
        this->PutClearVoteOptions(ID);
        this->PutDefaultVoteOptions(ID);
        this->PutCustomVoteOptions(ID);

    }, ofID));
}

void CVoteOptionServerExtended::PutClearVoteOptions(int ofID)
{
	// NON-BLOCKING - needs to lock m_VoteMessageQueueMutex before use.
    CMsgPacker Msg(NETMSGTYPE_SV_VOTECLEAROPTIONS);
    dbg_msg("DEBUG", "CLEAR_VOTE SIZE: %u", Msg.Size());
    m_VoteMessageQueue.emplace_back(ofID, MSGFLAG_VITAL, Msg);
}

void CVoteOptionServerExtended::PutDefaultVoteOptions(int toID)
{
	// NON-BLOCKING - needs to lock m_VoteMessageQueueMutex before use.
	class CVoteOptionServer *pCurrent = GameServer()->m_pVoteOptionFirst;

	while(pCurrent)
	{

        // TODO: this might cause a data race, because an admin 
        // might delete an entry, while this is run asynchronously.
        
        // add a mutex in main thread and append the delete/add msg sent by an admin client to a vector and
        // if the try_lock() succeeds in some tick, process those messages properly.

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
        m_VoteMessageQueue.emplace_back(toID, MSGFLAG_VITAL, Msg);
	}
}

void CVoteOptionServerExtended::PutCustomVoteOptions(int toID)
{
    CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
    Msg.AddInt(m_CustomVoteOptionHandlers[toID].size());

    for (auto& tuple : m_CustomVoteOptionHandlers[toID])
    {
        std::string& Description = std::get<0>(tuple);
        Msg.AddString(Description.c_str(), VOTE_DESC_LENGTH);
    }

    m_VoteMessageQueue.emplace_back(toID, MSGFLAG_VITAL, Msg);
    
}

void CVoteOptionServerExtended::AddVoteOptionHandler(std::string Description, std::string Command, std::function<void(int, std::string&, std::string&, std::string&)> Callback)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        m_CustomVoteOptionHandlers[i].push_back({Description, Command, Callback});
    }
    m_DefaultCustomVoteOptionHandlers.push_back({Description, Command, Callback});
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

bool CVoteOptionServerExtended::ExecuteVoteOption(int ofID, std::string ReceivedDescription, std::string ReceivedReason)
{
    for (auto &[Description, Command, Callback] : m_CustomVoteOptionHandlers[ofID])
    {
        if (ReceivedDescription == Description)
        {
            Callback(ofID, Description, Command, ReceivedReason);
            return true;
        }
    }
    return false;
    
}

