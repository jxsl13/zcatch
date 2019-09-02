#pragma once
#include <game/server/gamecontext.h>

#include <future>
#include <mutex>
#include <tuple>
#include <vector>

#include "engine/message.h"

class CVoteOptionServerExtended
{
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


public:
    CVoteOptionServerExtended(CGameContext *pGameServer);
    ~CVoteOptionServerExtended();

    // stuff that is to be executed in the game tick.
    void Tick();

    void Test(int forID);

    // resend vote options.
    void RefreshVoteOptions(int ofID);

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

};
