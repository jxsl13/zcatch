#pragma once
#include <game/server/gamecontext.h>

#include <future>
#include <mutex>
#include <tuple>
#include <vector>
#include <functional>

#include "engine/message.h"

using std::string;

class CVoteOptionServerExtended
{
public:
    using tuple_2str_fn_int_3str_t = std::tuple<std::string, std::string, std::function<void(int, std::string&, std::string&, std::string&)> >;
private:
    CGameContext* m_pGameServer;

    // contains async tasks
    std::vector<std::future<void>> m_Futures;

    // cleanup finished futures from vector
    void CleanupFutures();

    // wait until all futures finished execution.
    void AwaitFutures();


    // contains messages that are to be sent to the specific players.
    std::mutex m_VoteMessageQueueMutex;
    std::vector<std::tuple<int, int, CMsgPacker> > m_VoteMessageQueue;


    // <description>, <command>, 
    // <function> that gets a reference to the command passed as well as a reference to the description
    // the function might change both values and invoke a vote options update.
    std::vector<tuple_2str_fn_int_3str_t > m_CustomVoteOptionHandlers[MAX_CLIENTS];
    std::vector<tuple_2str_fn_int_3str_t > m_DefaultCustomVoteOptionHandlers;

public:

    CVoteOptionServerExtended(CGameContext *pGameServer);
    ~CVoteOptionServerExtended();

    // stuff that is to be executed in the game tick.
    void Tick();

    void Test(int forID);

    // resend vote options.
    void RefreshVoteOptions(int ofID);

    // reset vote options to their default commands
    // and descriptions
    void ResetCustomVoteOptionsToDefault(int ofID);

    // add a new option handler
    void AddVoteOptionHandler(std::string Description, std::string Command, std::function<void(int, std::string&, std::string&, std::string&)> Callback);


    bool ExecuteVoteOption(int ofID, std::string ReceivedDescription, std::string ReceivedReason);
protected:

    inline class IServer* Server() { return m_pGameServer->Server(); };
    inline class CGameContext* GameServer() { return m_pGameServer; };


	// add a message to clear the current vote options
    // on the client side. Message will be sent
    // in Tick()
	void PutClearVoteOptions(int ofID);

	// add a message with all of the default vote options 
    // that are either defined in the config file or
    // were added by an admin while the server is running.
	void PutDefaultVoteOptions(int toID);

    // Add a message for every custom vote 
    void PutCustomVoteOptions(int toID);

};
