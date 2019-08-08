#ifndef GAME_SERVER_RANKINGSERVER_H
#define GAME_SERVER_RANKINGSERVER_H

#include "playerstats.h"

#include <cpp_redis/cpp_redis>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <deque>

class CRankingServer
{
   private:

    // if this class has been initialized with a default constructor, 
    // it is assumed that all methods called are ignored.
    bool m_DefaultConstructed;

    // redis server host & port
    std::string m_Host;
    size_t m_Port;

    // redis client
    cpp_redis::client m_Client;

    std::vector<std::string> m_InvalidNicknames;
    bool IsValidNickname(const std::string& nickname, const std::string& prefix = "");

    // saving futures for later cleanup
    std::deque<std::future<void> > m_Futures;

    // remove finished futures from vector
    void CleanupFutures();

    std::mutex m_ReconnectHandlerMutex;
    bool m_IsReconnectHandlerRunning;
    
    int m_ReconnectIntervalMilliseconds;
    void HandleReconnecting();
    void StartReconnectHandler();

    // when we get a disconnect, we safe out db changing actions in a backlog.
    std::mutex m_BacklogMutex;
    // action, nickname, stats data, prefix
    std::vector<std::tuple<std::string, std::string, CPlayerStats, std::string> > m_Backlog;

    // cleanup backlog, when the conection has been established again.
    void CleanupBacklog();

    // retrieve player data syncronously
    CPlayerStats GetRankingSync(std::string nickname, std::string prefix = "");

    // set specific values
    void SetRankingSync(std::string nickname, CPlayerStats stats, std::string prefix = "");

    // synchronous execution of ranking update
    void UpdateRankingSync(std::string nickname, CPlayerStats stats, std::string prefix = "");

    // delete player's ranking
    void DeleteRankingSync(std::string nickname, std::string prefix = "");

    // retrieve top x player ranks based on their key property(like score, kills etc.).
    std::vector<std::pair<std::string, CPlayerStats> > GetTopRankingSync(int topNumber, std::string key, std::string prefix = "", bool biggestFirst = true);

   public:

    // default constructor - prevents the creation of a backlog(especially the allocation of RAM)
    // an instance of this object does nothing, it's behaving like a dummy instance
    CRankingServer();

    // clean up internal stuff and wait for internal asyncronous tasks to finish.
    // might take as much time as the reconnect_ms(see the constructor parameter) to finish its tasks.
    ~CRankingServer();


    // constructor
    CRankingServer(std::string host, size_t port, uint32_t timeout = 10000, uint32_t reconnect_ms = 5000);



    // gets data and does stuff that's defined in callback with it.
    // if no callback is provided, nothing is done.
    // returns true, if async task has been started, false if nick is invalid or if no callback has been provided of if 
    // object has been default-constructed indicating that no connection has been established.
    bool GetRanking(std::string nickname, std::function<void(CPlayerStats&)> calback = nullptr, std::string prefix = "");


    // possible keys CPlayerStats::keys()
    // returns true if an async task has been started successfully, otherwise false
    bool GetTopRanking(int topNumber, std::string key, std::function<void(std::vector<std::pair<std::string, CPlayerStats> >&)> callback = nullptr, std::string prefix = "", bool biggestFirst = true);


    // set ranking of a player to a specific value
    // returns true if an async task has been started successfulls, 
    // returns false if the provided nickname is invalid or the stats are invalid.
    bool SetRanking(std::string nickname, CPlayerStats stats, std::string prefix = "");


    // starts async execution of if nickname is valid
    // returns true if an async task has been started successfulls, 
    // returns false if the provided nickname is invalid or the stats are invalid.
    bool UpdateRanking(std::string nickname, CPlayerStats stats, std::string prefix = "");


    // if prefix is empty, the whole player is deleted.
    // returns true if an async task has been started successfulls, 
    // returns false if the provided nickname is invalid.
    bool DeleteRanking(std::string nickname, std::string prefix = "");


    // This functions can, but should not necessarily be used.
    // It can be used to synchronize execution.
    // wait for all futures to finish execution(used in destructor)
    void AwaitFutures();

};

#endif // GAME_SERVER_RANKINGSERVER_H