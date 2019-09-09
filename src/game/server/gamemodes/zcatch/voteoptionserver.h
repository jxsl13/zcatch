#pragma once
#include <game/server/gamecontext.h>

#include <functional>
#include <future>
#include <string>
#include <tuple>
#include <vector>

#include "engine/message.h"


class CVoteOptionServerExtended
{
public:
    using tuple_2str_fn_int_3str_t = std::tuple<std::string, std::string, std::function<bool(int, std::string&, std::string&, std::string&)> >;
private:
    CGameContext* m_pGameServer;

    
    // <description>, <command>, 
    // <function> that gets a reference to the command passed as well as a reference to the description
    // the function might change both values and invoke a vote options update.
    std::vector<tuple_2str_fn_int_3str_t > m_CustomVoteOptionHandlers[MAX_CLIENTS];
    std::vector<tuple_2str_fn_int_3str_t > m_DefaultCustomVoteOptionHandlers;

    std::mutex m_CustomVoteOptionHandlersMutex[MAX_CLIENTS];
    std::mutex m_DefaultCustomVoteOptionHandlersMutex;

    // <ID, Msg, ExecutionTick>
    std::vector<std::tuple<int, CMsgPacker, int> >    m_VoteOptionUpdateQueue;
    std::mutex                                  m_VoteOptionUpdateQueueMutex;

public:

    // constructor
    CVoteOptionServerExtended(CGameContext *pGameServer);

    ~CVoteOptionServerExtended();

    // add a new option handler
    // pass a handler to the votes.
    // Description - Initial Description Value
    // Command - Initial command value. 
    // Callback - bool function(string& Description, string& Command, string& Reason)
    // Description and Command are references, that can be actually altered to contain new values.
    // The Description value is the value that is shown to the player.
    // Reason is only the extra data that a player can pass.
    void AddVoteOptionHandler(std::string Description, std::string Command, std::function<bool(int, std::string&, std::string&, std::string&)> Callback);

    void RefreshVoteOptions(int ofID, int TicksOffset = 0);

    // executes a vote option reset for the given ID
    // and updates the vote list of that player.
    // executes the refresh of the vote option list in TicksOffset ticks.
    void OnPlayerConnect(int PlayerID, int RefreshTicksOffset = 100);

    // try to execute a specific vote option of a specific player.
    void ExecuteVoteOption(int ofID, std::string ReceivedDescription, std::string ReceivedReason, int RefreshTicksOffset = 50);

    // executed in main thread.
    // tries to send new vote updates to the specific players.
    void ProcessVoteOptionUpdates();

    
protected:

    inline class IServer* Server() { return m_pGameServer->Server(); };
    inline class CGameContext* GameServer() { return m_pGameServer; };

    // reset vote options to their default commands and descriptions
    void ResetCustomVoteOptionsToDefaultSync(int ofID);

    // sends all three messags below(Clear, DefaultVotes, CustomVotes)
    void RefreshVoteOptionsSync(int ofID, int ExecutionTick);

    // sends a message to clear the client's vote list.
	void SendClearVoteOptionsSync(int toID, int ExecutionTick);

    // send vanilla votes
	void SendDefaultVoteOptionsSync(int toID, int ExecutionTick);

    // send custom votes
    void SendCustomVoteOptionsSync(int toID, int ExecutionTick);

    // synchronous execution of resetting the custom vote options  etc.
    void OnPlayerConnectSync(int PlayerID, int RefreshTick);
    
    // execute the callback, that matches the Description.
    void ExecuteVoteOptionSync(int ofID, std::string& ReceivedDescription, std::string& ReceivedReason, int RefreshOffset);

private:

    // saving futures for later cleanup
    std::vector<std::future<void> > m_Futures;

    // remove finished futures from vector
    void CleanupFutures();

    // wait for all futures to finish execution(used in destructor)
    void AwaitFutures();

};
