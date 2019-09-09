#include "voteoptionserver.h"
#include <algorithm>
#include <chrono>


CVoteOptionServerExtended::CVoteOptionServerExtended(CGameContext *pGameServer) : m_pGameServer{pGameServer}
{
}

CVoteOptionServerExtended::~CVoteOptionServerExtended()
{
    AwaitFutures();
}

void CVoteOptionServerExtended::AddVoteOptionHandler(std::string Description, std::string Command, std::function<bool(int, std::string&, std::string&, std::string&)> Callback)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        m_CustomVoteOptionHandlers[i].push_back({Description, Command, Callback});
    }
    m_DefaultCustomVoteOptionHandlers.push_back({Description, Command, Callback});
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
                dbg_msg("ERROR", "Cleaning up features: %s", e.what());
            }

            return true;
        }
        return false;
    });
    dbg_msg("DEBUG", "CLEANED %ld FUTURES.", m_Futures.end() - it);
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
                dbg_msg("ERROR", "Awaiting features: %s", e.what());
            }
        }
    }

    m_Futures.clear();
}

void CVoteOptionServerExtended::SendClearVoteOptionsSync(int toID, int ExecutionTick)
{
    CMsgPacker Msg(NETMSGTYPE_SV_VOTECLEAROPTIONS);
    m_VoteOptionUpdateQueue.emplace_back(toID, Msg, ExecutionTick);
}

void CVoteOptionServerExtended::SendDefaultVoteOptionsSync(int toID, int ExecutionTick)
{
	struct CVoteOptionServer *pCurrent = GameServer()->m_pVoteOptionFirst;

	while(pCurrent)
	{

		// count options for actual packet
		int NumOptions = 0;
		for(struct CVoteOptionServer *p = pCurrent; p && NumOptions < MAX_VOTE_OPTION_ADD; p = p->m_pNext, ++NumOptions);

		// pack and add vote list packet
		CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
		Msg.AddInt(NumOptions);
		while(pCurrent && NumOptions--)
		{
			Msg.AddString(pCurrent->m_aDescription, VOTE_DESC_LENGTH);
			pCurrent = pCurrent->m_pNext;
		}

        m_VoteOptionUpdateQueue.emplace_back(toID, Msg, ExecutionTick);
	}
}

void CVoteOptionServerExtended::SendCustomVoteOptionsSync(int toID, int ExecutionTick)
{
    CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
    Msg.AddInt(m_CustomVoteOptionHandlers[toID].size());

    for (auto& tuple : m_CustomVoteOptionHandlers[toID])
    {
        std::string& Description = std::get<0>(tuple);
        Msg.AddString(Description.c_str(), VOTE_DESC_LENGTH);
    }

    m_VoteOptionUpdateQueue.emplace_back(toID, Msg, ExecutionTick);
}

void CVoteOptionServerExtended::RefreshVoteOptionsSync(int ofID, int ExecutionTick)
{

    dbg_msg("DEBUG", "REFRESHING VOTE OPTIONS OF ID %d @Tick%d @ExecutionTick:%d", ofID, m_pGameServer->Server()->Tick(), ExecutionTick);
    // clear votes
    SendClearVoteOptionsSync(ofID, ExecutionTick);

    // add default vote options
    SendDefaultVoteOptionsSync(ofID, ExecutionTick);

    // add custm vote options.
    SendCustomVoteOptionsSync(ofID, ExecutionTick);
}

void CVoteOptionServerExtended::RefreshVoteOptions(int ofID, int RefreshTicksOffset)
{
    int ExecuteAtTick = m_pGameServer->Server()->Tick() + RefreshTicksOffset;

    m_Futures.push_back(std::async(std::launch::async, [this](int ID, int ExecutionTick){
        // for reading to the custom vote options.
        std::lock_guard<std::mutex> CustomVoteOptionsLock(m_CustomVoteOptionHandlersMutex[ID]);
        // for appending messages to the queue.
        std::lock_guard<std::mutex> VoteOptionUpdateQueueLock(m_VoteOptionUpdateQueueMutex);

        this->RefreshVoteOptionsSync(ID, ExecutionTick);

    }, ofID, ExecuteAtTick));
}

void CVoteOptionServerExtended::ExecuteVoteOptionSync(int ofID, std::string& ReceivedDescription, std::string& ReceivedReason, int RefreshTick)
{
    for (auto& [Description, Command, Callback] : m_CustomVoteOptionHandlers[ofID])
    {
        if (ReceivedDescription == Description)
        {
            // only refresh votes if the callback requests it by returning true.
            if(Callback(ofID, Description, Command, ReceivedReason))
                RefreshVoteOptionsSync(ofID, RefreshTick);
            return;
        }
    }
}

void CVoteOptionServerExtended::ExecuteVoteOption(int ofID, const std::string ReceivedDescription, const std::string ReceivedReason, int RefreshTicksOffset)
{
    int ExecutionTick = m_pGameServer->Server()->Tick() + RefreshTicksOffset;
    m_Futures.push_back(std::async(std::launch::async, [this](int ID, std::string Description, std::string Reason, int RefreshTick)
    {
        // for reading & writing to the custom vote options.
        std::lock_guard<std::mutex> CustomVoteOptionsLock(m_CustomVoteOptionHandlersMutex[ID]);
        // for appending messages to the queue.(refresh)
        std::lock_guard<std::mutex> VoteOptionUpdateQueueLock(m_VoteOptionUpdateQueueMutex);

        this->ExecuteVoteOptionSync(ID, Description, Reason, RefreshTick);

    }, ofID, ReceivedDescription, ReceivedReason, ExecutionTick));
}

void CVoteOptionServerExtended::ResetCustomVoteOptionsToDefaultSync(int ofID)
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

void CVoteOptionServerExtended::OnPlayerConnectSync(int PlayerID, int RefreshTick)
{
    ResetCustomVoteOptionsToDefaultSync(PlayerID);
    std::string Reason = {};
    for (auto& [Description, Command, Callback] : m_CustomVoteOptionHandlers[PlayerID])
    {
        if (Command.size() > 0 && Command[0] == '#') // commands starting with # are executed when a player joins.
        {
            Callback(PlayerID, Description, Command, Reason);
        }
    }

    RefreshVoteOptionsSync(PlayerID, RefreshTick);
}

void CVoteOptionServerExtended::OnPlayerConnect(int PlayerID, int RefreshTicksOffset)
{
    int ExecuteAtTick = m_pGameServer->Server()->Tick() + RefreshTicksOffset;
    
    m_Futures.push_back(std::async(std::launch::async, [this](int ID, int ExecutionTick){
        
        // for reading the default options.
        std::lock_guard<std::mutex> DefaultVoteOptionsLock(m_DefaultCustomVoteOptionHandlersMutex);
        // for writing to the custom vote options.
        std::lock_guard<std::mutex> CustomVoteOptionsLock(m_CustomVoteOptionHandlersMutex[ID]);
        // for appending messages to the queue.
        std::lock_guard<std::mutex> VoteOptionUpdateQueueLock(m_VoteOptionUpdateQueueMutex);

        this->OnPlayerConnectSync(ID, ExecutionTick);
    }, PlayerID, ExecuteAtTick));
}

void CVoteOptionServerExtended::ProcessVoteOptionUpdates()
{
    // if any finished futures exist, remove them.
    CleanupFutures();

    if (m_VoteOptionUpdateQueueMutex.try_lock())
    {
        if(m_VoteOptionUpdateQueue.size() > 0)
        {
            auto it = std::remove_if(m_VoteOptionUpdateQueue.begin(), m_VoteOptionUpdateQueue.end(), 
            [this](auto& tuple) -> bool
            {
                auto& [toID, Msg, ExecutionTick] = tuple;

                if (ExecutionTick <= m_pGameServer->Server()->Tick())
                {
                    // execute if we passed the execution tick and remove from list
                    Server()->SendMsg(&Msg, MSGFLAG_VITAL, toID);
                    dbg_msg("DEBUG", "Executing VoteOptionUpdate @Tick:%d", m_pGameServer->Server()->Tick()); 
                    return true;  
                }
                return false;
            });

            m_VoteOptionUpdateQueue.erase(it, m_VoteOptionUpdateQueue.end());
        }
        m_VoteOptionUpdateQueueMutex.unlock();
    }
}


