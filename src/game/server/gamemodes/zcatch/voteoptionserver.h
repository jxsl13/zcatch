#pragma once
#include <game/server/gamecontext.h>

#include <tuple>
#include <vector>
#include <functional>

#include "engine/message.h"

using std::string;

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

public:

    // constructor
    CVoteOptionServerExtended(CGameContext *pGameServer);

    // add a new option handler
    // pass a handler to the votes.
    // Description - Initial Description Value
    // Command - Initial command value. 
    // Callback - bool function(string& Description, string& Command, string& Reason)
    // Description and Command are references, that can be actually altered to contain new values.
    // The Description value is the value that is shown to the player.
    // Reason is only the extra data that a player can pass.
    void AddVoteOptionHandler(std::string Description, std::string Command, std::function<bool(int, std::string&, std::string&, std::string&)> Callback);

    // executes a vote option reset for the given ID
    // and updates the vote list of that player.
    void OnPlayerConnect(int PlayerID);

    // sends all three messags below
    void RefreshVoteOptions(int ofID);


    // try to execute a specific vote option of a specific player.
    bool ExecuteVoteOption(int ofID, std::string ReceivedDescription, std::string ReceivedReason);

    
protected:

    inline class IServer* Server() { return m_pGameServer->Server(); };
    inline class CGameContext* GameServer() { return m_pGameServer; };

    // reset vote options to their default commands and descriptions
    void ResetCustomVoteOptionsToDefault(int ofID);

    // sends a message to clear the client's vote list.
	void SendClearVoteOptions(int ofID);

    // send vanilla votes
	void SendDefaultVoteOptions(int toID);

    // send custom votes
    void SendCustomVoteOptions(int toID);

};
