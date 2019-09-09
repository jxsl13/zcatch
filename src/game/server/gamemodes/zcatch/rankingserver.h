#ifndef GAME_SERVER_RANKINGSERVER_H
#define GAME_SERVER_RANKINGSERVER_H

#include "playerstats.h"

#include <cpp_redis/cpp_redis>
#include <SQLiteCpp/SQLiteCpp.h>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <deque>

class IRankingServer
{
   public:

    // callback that is being called, when the 
    // player statistics have been retrieved successfully
    using cb_stats_t = std::function<void(CPlayerStats&)>;
    using cb_key_stats_vec_t = std::function<void(std::vector<std::pair<std::string, CPlayerStats> >&)>;

    // Return type: list of [key, stats] pairs
    using key_stats_vec_t = std::vector<std::pair<std::string, CPlayerStats> >;

    // this function is used in order to update the stats that are in the database.
    // the first argument passed is the data that is currently in the databse,
    // the second argument is the new data, that might have been aquired during the current 
    // playing sesion.
    using fn_stats_stats_ret_stats_t = std::function<CPlayerStats(CPlayerStats&, CPlayerStats&)>;

    // initializes invalid nicknames
    // backlog entries are handled, when the database has downtimes
    // updates are executed, when the server reconnects to the database again
    // depending on the BacklogUpdateFunction, a player stat is passed to be saved in the
    // database. E.g. the function returns the sum of both values, maybe the smaller value
    // for race based mods etc. etc.
    IRankingServer();


    // cleans up futures and waits for them.
    virtual ~IRankingServer();

   protected:

    // if this class has been initialized with a default constructor, 
    // it is assumed that all methods called are ignored.
    bool m_DefaultConstructed;

    // Synchronizing threads
    std::mutex m_DatabaseMutex;


     // ranking order is based on this key.
    const std::string m_RankingKey{"Score"};

    // 
    const bool m_BiggestFirst{true};


    std::vector<std::string> m_InvalidNicknames;
    bool IsValidKey(const std::string& key) const;

    // removes whitespace from string
    void trim(std::string& s);

    // saving futures for later cleanup
    std::deque<std::future<void> > m_Futures;

    // remove finished futures from vector
    void CleanupFutures();


    // when we get a disconnect, we safe out db changing actions in a backlog.
    std::mutex m_BacklogMutex;
    // action, nickname, stats data, prefix
    std::vector<std::tuple<std::string, std::string, CPlayerStats, fn_stats_stats_ret_stats_t, std::string> > m_Backlog;

    // cleanup backlog, when the conection has been established again.
    void CleanupBacklog();


    // ############################################################################################################
    // Interface that needs to be implemented

    // retrieve player data syncronously - throws exception on error, retuns invalid object if not found
    virtual CPlayerStats GetRankingSync(std::string nickname, std::string prefix) = 0;

    // set specific values synchronously
    virtual void SetRankingSync(std::string nickname, CPlayerStats stats, std::string prefix) = 0;

    // synchronous execution of ranking update
    virtual void UpdateRankingSync(std::string nickname, CPlayerStats stats, fn_stats_stats_ret_stats_t updateFunction, std::string prefix) = 0;

    // delete player's ranking synchronously
    virtual void DeleteRankingSync(std::string nickname, std::string prefix) = 0;

    // retrieve top x player ranks based on their key property(like score, wins, kills, deaths etc.) synchronously.
    virtual key_stats_vec_t GetTopRankingSync(int topNumber, std::string key, std::string prefix, bool biggestFirst) = 0;
    // ############################################################################################################

   public: 
   
    bool IsValidNickname(const std::string& nickname, const std::string& prefix = "") const;

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
    // updateFunction: Takes two arguments, the first one being the currently saved statistics in the database
    //                  and the second one being the currently passed new player stats.
    //                  This function needs to return the player stats that are to be saved in the databse.
    bool UpdateRanking(std::string nickname, CPlayerStats stats, fn_stats_stats_ret_stats_t updateFunction, std::string prefix = "");


    // if prefix is empty, the whole player is deleted.
    // returns true if an async task has been started successfulls, 
    // returns false if the provided nickname is invalid.
    bool DeleteRanking(std::string nickname, std::string prefix = "");


    // This functions can, but should not necessarily be used.
    // It can be used to synchronize execution.
    // wait for all futures to finish execution(used in destructor)
    void AwaitFutures();
};

class CRedisRankingServer : public IRankingServer
{
   private:

    // redis server host & port
    std::string m_Host;
    size_t m_Port;

    // redis client
    cpp_redis::client m_Client;

    
    std::mutex m_ReconnectHandlerMutex;
    bool m_IsReconnectHandlerRunning;
    
    int m_ReconnectIntervalMilliseconds;
    void HandleReconnecting();
    void StartReconnectHandler();

   protected:    

    // retrieve player data syncronously
    virtual CPlayerStats GetRankingSync(std::string nickname, std::string prefix = "");

    // set specific values
    virtual void SetRankingSync(std::string nickname, CPlayerStats stats, std::string prefix = "");

    // synchronous execution of ranking update
    virtual void UpdateRankingSync(std::string nickname, CPlayerStats stats, IRankingServer::fn_stats_stats_ret_stats_t updateFunction, std::string prefix = "");

    // delete player's ranking
    virtual void DeleteRankingSync(std::string nickname, std::string prefix = "");

    // retrieve top x player ranks based on their key property(like score, kills etc.).
    virtual IRankingServer::key_stats_vec_t GetTopRankingSync(int topNumber, std::string key, std::string prefix = "", bool biggestFirst = true);

   public:

    // default constructor - prevents the creation of a backlog(especially the allocation of RAM)
    // an instance of this object does nothing, it's behaving like a dummy instance
    CRedisRankingServer();

    // constructor
    CRedisRankingServer(std::string host, size_t port, uint32_t timeout = 10000, uint32_t reconnect_ms = 5000);
    
    // clean up internal stuff and wait for internal asyncronous tasks to finish.
    // might take as much time as the reconnect_ms(see the constructor parameter) to finish its tasks.
    virtual ~CRedisRankingServer();
};

class CSQLiteRankingServer : public IRankingServer
{
   private:
    std::string m_FilePath;

    SQLite::Database *m_pDatabase;

    std::mutex m_ValidPrefixListMutex;
    std::vector<std::string> m_ValidPrefixList;

    // table base name, that's added after the table prefix
    const std::string m_BaseTableName{"Ranking"};

   
    bool IsValidPrefix(const std::string& prefix);
    void FixPrefix(std::string& prefix);

   protected:
    // retrieve player data syncronously
    virtual CPlayerStats GetRankingSync(std::string nickname, std::string prefix = "");

    // set specific values
    virtual void SetRankingSync(std::string nickname, CPlayerStats stats, std::string prefix = "");

    // synchronous execution of ranking update
    virtual void UpdateRankingSync(std::string nickname, CPlayerStats stats, IRankingServer::fn_stats_stats_ret_stats_t updateFunction, std::string prefix = "");

    // delete player's ranking
    virtual void DeleteRankingSync(std::string nickname, std::string prefix = "");

    // retrieve top x player ranks based on their key property(like score, kills etc.).
    virtual IRankingServer::key_stats_vec_t GetTopRankingSync(int topNumber, std::string key, std::string prefix = "", bool biggestFirst = true);

   public:

    // dummy
    CSQLiteRankingServer();

    // all prefixes need to be defined at construction time, in ordr to create the db tables.
    CSQLiteRankingServer(std::string filePath, std::vector<std::string> validPrefixList = {{""}}, int busyTimeoutMs = 10000);
    virtual ~CSQLiteRankingServer();
};

#endif // GAME_SERVER_RANKINGSERVER_H