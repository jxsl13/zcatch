#include "rankingserver.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <base/system.h>


IRankingServer::IRankingServer()
{
    // all possible fields are invalid nicks
    CPlayerStats tmp;
    m_InvalidNicknames = tmp.keys();
}

IRankingServer::~IRankingServer()
{
    // in any case, await futures, if the derived class does not do this.
    AwaitFutures();
}

bool IRankingServer::IsValidNickname(const std::string& nickname, const std::string& prefix)
{
    if (nickname.size() == 0)
        return false; // empty string nick -> no rankings for you
    else if (m_InvalidNicknames.size() == 0)
        return true; // no invalid nicks -> your nick is valid

    for (auto& name : m_InvalidNicknames)
    {
        if (nickname == name)
            return false;
        else if (nickname == (prefix + name))
            return false;
    }
    return true;
}

bool IRankingServer::GetRanking(std::string nickname, IRankingServer::cb_stats_t callback, std::string prefix)
{
    CleanupFutures();

    if (m_DefaultConstructed || callback == nullptr || !IsValidNickname(nickname, prefix))
        return false;

    m_Futures.push_back(std::async(
        std::launch::async, [this](std::string nick, std::function<void(CPlayerStats&)> cb, std::string pref) {
            CPlayerStats stats;
            try
            {
                // lock mutex for multi threaded access
                std::lock_guard<std::mutex> lock(m_DatabaseMutex);

                stats = this->GetRankingSync(nick, pref); // get data from server
            }
            catch (const std::exception& e)
            {
                // if some unexpected error happened in GetRankingSync
                dbg_msg("IRankingServer", "Failed to retrieve ranking.");

                // if retrieving fails, nothing is donw.
                return;
            }

            // calling callback
            // this should not hrow anything.
            cb(stats); // call callback on data
        },
        nickname, callback, prefix));

    return true;
}

bool IRankingServer::DeleteRanking(std::string nickname, std::string prefix)
{
    CleanupFutures();

    if (m_DefaultConstructed || !IsValidNickname(nickname, prefix))
        return false;

    m_Futures.push_back(
        std::async(
            std::launch::async,
            [this](std::string nick, std::string pref) {
                try
                {
                    // lock mutex for multi threaded access
                    std::lock_guard<std::mutex> lock(m_DatabaseMutex);

                    this->DeleteRankingSync(nick, pref);
                }
                catch (std::exception& e)
                {
                    // failed to delete ranking
                    // adding to backlog
                    std::lock_guard<std::mutex> lock(m_BacklogMutex);
                    m_Backlog.push_back({"delete", nick, CPlayerStats(), pref});
                }
            },
            nickname, prefix));
    return true;
}

bool IRankingServer::GetTopRanking(int topNumber, std::string key, IRankingServer::cb_key_stats_vec_t callback, std::string prefix, bool biggestFirst)
{
    CleanupFutures();

    if (m_DefaultConstructed || callback == nullptr)
        return false;
    

    m_Futures.push_back(std::async(
        std::launch::async,
        [this](int topNum, std::string field, decltype(callback) cb, std::string pref, bool bigFirst) {
            std::vector<std::pair<std::string, CPlayerStats> > result;

            try
            {
                // lock mutex for multi threaded access
                std::lock_guard<std::mutex> lock(m_DatabaseMutex);

                result = this->GetTopRankingSync(topNum, field, pref, bigFirst);
            }
            catch (const std::exception& e)
            {
                dbg_msg("IRankingServer", "%s", e.what());
                return;
            }

            // if no error occurrs, call callback on the result.
            cb(result);
        },
        topNumber, key, callback, prefix, biggestFirst));

    return true;
}

bool IRankingServer::UpdateRanking(std::string nickname, CPlayerStats stats, std::string prefix)
{
    CleanupFutures();

    if (m_DefaultConstructed || !IsValidNickname(nickname, prefix))
        return false;

    m_Futures.push_back(std::async(
        std::launch::async,
        [this](std::string nick, CPlayerStats stat, std::string pref) {
            try
            {
                // lock mutex for multi threaded access
                std::lock_guard<std::mutex> lock(m_DatabaseMutex);

                // if this somehow fails and throws an error, handle backlogging
                this->UpdateRankingSync(nick, stat, pref);
            }
            catch (const std::exception& e)
            {
                dbg_msg("IRankingServer", "%s", e.what());

                std::lock_guard<std::mutex> lock(m_BacklogMutex);
                m_Backlog.push_back({"update", nick, stat, pref});
            }
        },
        nickname, stats, prefix));

    return true;
}

bool IRankingServer::SetRanking(std::string nickname, CPlayerStats stats, std::string prefix)
{
    CleanupFutures();

    if (m_DefaultConstructed || !stats.IsValid() || !IsValidNickname(nickname, prefix))
        return false;

    m_Futures.push_back(std::async(
        std::launch::async,
        [this](std::string nick, CPlayerStats stat, std::string pref) {
            try
            {
                // lock mutex for multi threaded access
                std::lock_guard<std::mutex> lock(m_DatabaseMutex);

                // if this fails, we add this pending action to our backlog.
                this->SetRankingSync(nick, stat, pref);
            }
            catch (const std::exception& e)
            {
                dbg_msg("IRankingServer", "%s", e.what());

                std::lock_guard<std::mutex> lock(m_BacklogMutex);
                m_Backlog.push_back({"set", nick, stat, pref});
            }
        },
        nickname, stats, prefix));

    return true;
}

void IRankingServer::CleanupBacklog()
{
    std::lock_guard<std::mutex> lock(m_BacklogMutex);
    if (m_Backlog.size() > 0)
    {
        // we start new threads from here.
        // if those threads fail, they will keep on waiting
        // for this mutex, until they add their failed information
        // back to the backlog.

        // even tho the backlog might be filled again by these actions, it will be ignored
        // when the ranking server is destroyed, as each element is beeing looked at once at most.
        int counter = 0;
        for (size_t i = 0; i < m_Backlog.size(); i++)
        {
            auto [action, nickname, stats, prefix] = m_Backlog.back();
            m_Backlog.pop_back();

            if (action == "update")
            {
                UpdateRanking(nickname, stats, prefix);
                counter++;
            }
            else if (action == "delete")
            {
                DeleteRanking(nickname, prefix);
                counter++;
            }
            else if (action == "set")
            {
                SetRanking(nickname, stats, prefix);
            }
        }

        dbg_msg("IRankingServer", "Cleaned up %d backlog tasks.", counter);
    }
    else
    {
        // backlog empty
    }
}

void IRankingServer::CleanupFutures()
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
                std::cerr << e.what() << '\n';
            }

            return true;
        }
        return false;
    });
    m_Futures.erase(it, m_Futures.end());
}

void IRankingServer::AwaitFutures()
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
                std::cerr << e.what() << '\n';
            }
        }
    }

    m_Futures.clear();
}

// ############################################################

CRedisRankingServer::CRedisRankingServer()
{
    m_DefaultConstructed = true;
}

CRedisRankingServer::CRedisRankingServer(std::string host, size_t port, uint32_t timeout, uint32_t reconnect_ms) : m_Host{host}, m_Port{port}
{
    m_DefaultConstructed = false;

    m_ReconnectIntervalMilliseconds = reconnect_ms;
    try
    {
        m_Client.connect(m_Host, m_Port, nullptr, timeout, 0, reconnect_ms);
        if (m_Client.is_connected())
        {
            // no reconnection handling necessary
            std::lock_guard<std::mutex> lock(m_ReconnectHandlerMutex);
            m_IsReconnectHandlerRunning = false;
            dbg_msg("REDIS", "successfully connected to %s:%lu", m_Host.c_str(), m_Port);
        }
    }
    catch (const cpp_redis::redis_error& e)
    {
        dbg_msg("REDIS", "initial connection to %s:%lu failed.", m_Host.c_str(), m_Port);
        StartReconnectHandler();
    }
}

CRedisRankingServer::~CRedisRankingServer()
{
    // we still fail to reconnect at shutdown -> force shutdown
    m_ReconnectHandlerMutex.lock();
    m_IsReconnectHandlerRunning = false;
    m_ReconnectHandlerMutex.unlock();

    // we need to wait for out futures to finish, before
    // we can disconnect from the server.
    AwaitFutures();

    if (m_Client.is_connected())
    {
        m_Client.disconnect(true);
        dbg_msg("REDIS", "disconnected from database.");
        std::cout << "[redis]: disconnected from database" << std::endl;
    }
}

void CRedisRankingServer::HandleReconnecting()
{
    while (!m_Client.is_connected())
    {
        try
        {
            m_Client.connect(m_Host, m_Port);
        }
        catch (const cpp_redis::redis_error& e)
        {
            dbg_msg("REDIS", "Reconnect failed...");
        }

        // wait
        std::this_thread::sleep_for(std::chrono::milliseconds(m_ReconnectIntervalMilliseconds));

        std::lock_guard<std::mutex> lock(m_ReconnectHandlerMutex);
        if (!m_IsReconnectHandlerRunning)
        {
            dbg_msg("REDIS", "Shutting down reconnect handler.");

            // forceful shutdown, is done, when the ranking server is
            // shutting down.
            CleanupBacklog();
            return;
        }
    }

    // connection established
    std::lock_guard<std::mutex> lock(m_ReconnectHandlerMutex);
    m_IsReconnectHandlerRunning = false;

    dbg_msg("REDIS", "Successfully reconnected!");
    // if connection established, try purging the db backlog
    CleanupBacklog();
}

void CRedisRankingServer::StartReconnectHandler()
{
    if (m_IsReconnectHandlerRunning)
        return; // already running

    std::lock_guard<std::mutex> lock(m_ReconnectHandlerMutex);
    m_IsReconnectHandlerRunning = true;

    // this needs to be pushed to the front of all futures, as it needs to be handled
    // last, as it might cause a deadlock
    m_Futures.push_front(std::async(std::launch::async, &CRedisRankingServer::HandleReconnecting, this));
}

CPlayerStats CRedisRankingServer::GetRankingSync(std::string nickname, std::string prefix)
{
    CPlayerStats stats;

    try
    {
        std::future<cpp_redis::reply> existsFuture = m_Client.exists({nickname});
        m_Client.sync_commit();

        cpp_redis::reply existsReply = existsFuture.get();

        if (existsReply.as_integer())
        {
            std::future<cpp_redis::reply> getFuture = m_Client.hmget(nickname, stats.keys(prefix));

            m_Client.sync_commit();
            cpp_redis::reply reply = getFuture.get();

            std::vector<cpp_redis::reply> result = reply.as_array();

            // set every key.
            int idx = 0;
            for (auto& key : stats.keys())
            {
                if (result.at(idx).is_null())
                {
                    // result is null
                    // key not found
                    stats.Invalidate();
                    break; // entry does not exist yet.
                }
                else if (result.at(idx).is_string())
                {
                    stats[key] = std::stoi(result.at(idx).as_string());
                }
                else if (result.at(idx).is_integer())
                {
                    stats[key] = result.at(idx).as_integer();
                }
                else
                {
                    dbg_msg("REDIS", "unknown result type.");
                    stats.Invalidate();
                    break;
                }
                idx++;
            }

            std::string rankingIndex = prefix + m_RankingKey;


            if (m_BiggestFirst)
            {                
                std::future<cpp_redis::reply> rankFuture = m_Client.zrevrank(rankingIndex, nickname);

                m_Client.sync_commit();
                cpp_redis::reply reply = rankFuture.get();

                if (!reply.is_null())
                {
                    // redis ranks are couted from 0
                    stats.SetRank(reply.as_integer() + 1);
                }
                else
                {
                    stats.Invalidate();
                }
            }
            else
            {
                std::future<cpp_redis::reply> rankFuture = m_Client.zrank(rankingIndex, nickname);

                m_Client.sync_commit();
                cpp_redis::reply reply = rankFuture.get();

                if (!reply.is_null())
                {
                    // redis ranks are couted from 0
                    stats.SetRank(reply.as_integer() + 1);
                }
                else
                {
                    stats.Invalidate();
                }
            }     
        }
        else
        {
            // not found
            stats.Invalidate();
        }
        
        return stats;
    }
    catch (const cpp_redis::redis_error& e)
    {
        if (!m_Client.is_connected())
        {
            dbg_msg("REDIS", "Lost connection.");
            StartReconnectHandler();
        }

        throw;
    }
}

IRankingServer::key_stats_vec_t CRedisRankingServer::GetTopRankingSync(int topNumber, std::string key, std::string prefix, bool biggestFirst)
{
    std::string index = prefix + key;

    try
    {
        std::future<cpp_redis::reply> existsFuture = m_Client.exists({index});
        m_Client.sync_commit();

        cpp_redis::reply existsReply = existsFuture.get();

        if (existsReply.is_integer())
        {
            if (existsReply.as_integer() != 0)
            {
                // specified index exists
                std::future<cpp_redis::reply> resultFuture;

                if (biggestFirst)
                {
                    resultFuture = m_Client.zrevrangebyscore(index, "+inf", "0", 0, topNumber);
                }
                else
                {
                    resultFuture = m_Client.zrangebyscore(index, "0", "+inf", 0, topNumber);
                }

                m_Client.sync_commit();

                cpp_redis::reply result = resultFuture.get();

                if (result.is_array())
                {
                    std::vector<std::pair<std::string, CPlayerStats> > sortedResult;

                    for (auto& r : result.as_array())
                    {
                        if (r.is_string())
                        {
                            sortedResult.push_back({r.as_string(), {/* empty*/}});
                        }
                        else
                        {
                            throw cpp_redis::redis_error("Expected string as nickname.");
                        }
                    }

                    for (auto& [nickname, stats] : sortedResult)
                    {
                        stats = GetRankingSync(nickname, prefix);
                    }

                    return sortedResult;
                }
                else
                {
                    throw cpp_redis::redis_error("Expected array return value of z[rev]rangebyscore(...)");
                }
            }
            else
            {
                throw cpp_redis::redis_error("exists: expected integer != 0");
            }
        }
        else
        {
            throw cpp_redis::redis_error("exists: expected integer reply");
        }
    }
    catch (const cpp_redis::redis_error& e)
    {
        // error is propagated to calling function.
        throw;
    }
}

void CRedisRankingServer::UpdateRankingSync(std::string nickname, CPlayerStats stats, std::string prefix)
{
    try
    {
        CPlayerStats dbStats;

        // throws exception, if connection fails
        dbStats = GetRankingSync(nickname, prefix);

        // is only invalid if player could not be found
        if (!dbStats.IsValid())
            dbStats.Reset();

        dbStats += stats;

        std::future<cpp_redis::reply> setFuture = m_Client.hmset(nickname, dbStats.GetStringPairs(prefix));

        // create/update index for every key
        std::vector<std::string> options = {};
        std::vector<std::future<cpp_redis::reply> > indexFutures;
        for (auto& key : dbStats.keys())
        {
            indexFutures.push_back(
                m_Client.zadd(prefix + key,
                              options,
                              {{std::to_string(dbStats[key]), nickname}}));
        }

        m_Client.sync_commit();
        for (auto& f : indexFutures)
        {
            cpp_redis::reply r = f.get();
            // retrieve results
            // don't do anything with it.
        }
    }
    catch (const cpp_redis::redis_error& e)
    {
        if (!m_Client.is_connected())
        {
            dbg_msg("REDIS", "Lost connection: %s", e.what());
            StartReconnectHandler();
        }
        else if (!IsValidNickname(nickname))
        {
            dbg_msg("REDIS", "Invalid nickname: %s", nickname.c_str());
            return;
        }
        else
        {
            // unexpected error
        }
        throw;
    }
}

void CRedisRankingServer::SetRankingSync(std::string nickname, CPlayerStats stats, std::string prefix)
{
    try
    {
        std::future<cpp_redis::reply> setFuture = m_Client.hmset(nickname, stats.GetStringPairs(prefix));

        // create/update index for every key
        std::vector<std::string> options = {};
        std::vector<std::future<cpp_redis::reply> > indexFutures;
        for (auto& key : stats.keys())
        {
            indexFutures.push_back(
                m_Client.zadd(prefix + key,
                              options,
                              {{std::to_string(stats[key]), nickname}}));
        }

        m_Client.sync_commit();
        for (auto& f : indexFutures)
        {
            cpp_redis::reply r = f.get();
            // retrieve results
            // don't do anything with it.
        }
    }
    catch (const cpp_redis::redis_error& e)
    {
        if (!m_Client.is_connected())
        {
            dbg_msg("REDIS", "Lost connection: %s", e.what());
            StartReconnectHandler();
        }
        else if (!IsValidNickname(nickname))
        {
            dbg_msg("REDIS", "Invalid nickname: %s", nickname.c_str());
            return;
        }
        else
        {
            // unexpected error
        }
        throw;
    }
}

void CRedisRankingServer::DeleteRankingSync(std::string nickname, std::string prefix)
{
    try
    {
        CPlayerStats stats;

        std::future<cpp_redis::reply> existsFuture = m_Client.exists({nickname});
        m_Client.sync_commit();

        cpp_redis::reply existsReply = existsFuture.get();

        if (existsReply.as_integer()) // exists
        {
            // check if type is hash
            std::future<cpp_redis::reply> typeFuture = m_Client.type(nickname);
            m_Client.sync_commit();
            cpp_redis::reply typeReply = typeFuture.get();

            if (typeReply.is_string() && typeReply.as_string() != "hash")
            {
                dbg_msg("REDIS", "Deleting '%s' failed, type is not hash: %s", nickname.c_str(), typeReply.as_string().c_str());
                return; // invalid object
            }
            else if (!typeReply.is_string())
            {
                dbg_msg("REDIS", "Deleting '%s' failed, reply is not a string", nickname.c_str());
                return;
            }

            // all keys represent individual index names
            std::future<cpp_redis::reply> allKeysFuture = m_Client.hkeys(nickname);
            m_Client.sync_commit();
            cpp_redis::reply keysReply = allKeysFuture.get();

            // contains keys that ought to be deleted
            // these keys also identify the sorted set indices
            std::vector<std::string> keys;
            if (keysReply.is_array())
            {
                for (auto& key : keysReply.as_array())
                {
                    if (key.is_string())
                    {
                        keys.push_back(key.as_string());
                    }
                    else if (key.is_integer())
                    {
                        keys.push_back(std::to_string(key.as_integer()));
                    }
                    else
                    {
                        // invalid case
                        continue;
                    }
                }
            }
            else
            {
                dbg_msg("REDIS", "Failed to retrieve all field names, return type is not an array.");
                return;
            }

            std::future<cpp_redis::reply> delFuture;
            std::vector<std::future<cpp_redis::reply> > delIndicesFutures;

            if (prefix.size() > 0)
            {
                // remove keys that don't have the correct prefix
                keys.erase(std::remove_if(keys.begin(), keys.end(), [&prefix](const std::string& key) {
                               if (key.size() < prefix.size())
                               {
                                   return true; // delete invalid key from vector
                               }

                               // expect key and prefix length to be at least equal
                               for (size_t i = 0; i < prefix.size(); i++)
                               {
                                   // prefix doesn't match
                                   if (key.at(i) != prefix.at(i))
                                       return true; // remove key, cuz invalid prefix
                               }
                               // full prefix matches, don't remove
                               return false;
                           }),
                           keys.end());

                if (keys.size() == 0)
                {
                    dbg_msg("REDIS", "No keys matching prefix '%s'. Didn't delete player '%s'", prefix.c_str(), nickname.c_str());
                    return;
                }

                // delete only specified fields with prefix
                delFuture = m_Client.hdel(nickname, keys);
            }
            else
            {
                // delete all player data
                delFuture = m_Client.del({nickname});
            }

            // remove idices

            for (auto& key : keys)
            {
                delIndicesFutures.push_back(m_Client.zrem(key, {nickname}));
            }

            m_Client.sync_commit();
            cpp_redis::reply reply = delFuture.get();

            int result = 0;
            if (reply.is_integer())
            {
                result = reply.as_integer();
            }
            else
            {
                dbg_msg("REDIS", "Invalid result, expected integer");
            }

            if (!result)
            {
                throw cpp_redis::redis_error("deletion failed");
            }

            int tmp = 0;
            for (auto& f : delIndicesFutures)
            {
                cpp_redis::reply delIndexReply = f.get();

                if (delIndexReply.is_integer())
                {
                    tmp = delIndexReply.as_integer();
                    if (!tmp)
                    {
                        dbg_msg("REDIS", "Failed to delete an index. #1");
                    }
                }
                else
                {
                    throw cpp_redis::redis_error("Failed to delete an index. #2");
                }
            }
        }
    }
    catch (const cpp_redis::redis_error& e)
    {
        if (!m_Client.is_connected())
        {
            dbg_msg("REDIS", "Lost connection: %s", e.what());
            StartReconnectHandler();
            throw;
        }
        else
        {
            dbg_msg("REDIS", "Unexpected error while trying to delete the entry of '%s': %s", nickname.c_str(), e.what());
        }
        return;
    }
}

CSQLiteRankingServer::CSQLiteRankingServer()
{
    m_DefaultConstructed = true;
    m_pDatabase = nullptr;
}

void CSQLiteRankingServer::FixPrefix(std::string& prefix)
{
    // replace whitespace with underscore
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char ch) {
        return std::isspace(static_cast<unsigned char>(ch)) ? '_' : ch;
    });

    // prefix must not start with an intger, cuz it's an invalid table name then
    if (prefix.size() > 0 && std::isdigit(prefix[0]))
    {
        prefix = std::string("_") + prefix;
    }
}

CSQLiteRankingServer::CSQLiteRankingServer(std::string filePath, std::vector<std::string> validPrefixList, int busyTimeoutMs)
{
    // needed to get keys in order to create table columns
    CPlayerStats stats;

    m_DefaultConstructed = false;
    m_pDatabase = nullptr;

    m_ValidPrefixList.reserve(validPrefixList.size());

    for (auto& prefix : validPrefixList)
    {
        FixPrefix(prefix);

        m_ValidPrefixList.push_back(prefix);
    }

    std::vector<std::string> Columns = stats.keys();
    size_t ColumnsSize = Columns.size();

    // used to consruct creation query
    std::stringstream ss;

    for (auto& p : m_ValidPrefixList)
    {
        // create table for every prefix
        std::string TableName = p + m_BaseTableName;

        ss << "CREATE TABLE IF NOT EXISTS " << TableName << "  ( ";
        ss << "Key TEXT PRIMARY KEY ASC,\n";

        for (size_t i = 0; i < ColumnsSize; i++)
        {
            ss << " " << Columns[i] << " UNSIGNED INTEGER DEFAULT 0";

            if (i < ColumnsSize - 1)
                ss << " ,\n";
        }
        ss << " );\n";

        // create indices
        for (size_t i = 0; i < ColumnsSize; i++)
        {
            ss << "CREATE INDEX IF NOT EXISTS " << TableName << "_" << Columns[i] << "_index ON " << TableName << " (" << Columns[i] << ");\n";
        }
    }

    try
    {
        m_FilePath = filePath;
        m_pDatabase = new SQLite::Database(m_FilePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        m_pDatabase->setBusyTimeout(busyTimeoutMs);
        m_pDatabase->exec(ss.str());

        dbg_msg("SQLite", "Successfully created database: '%s'", m_FilePath.c_str());
    }
    catch (const std::exception& e)
    {
        dbg_msg("SQLite", "Failed to create a database: %s", e.what());

        // got an error opening db
        // no sense in trying again
        m_DefaultConstructed = true;

        if (m_pDatabase)
        {
            delete m_pDatabase;
            m_pDatabase = nullptr;
        }
    }
}

CSQLiteRankingServer::~CSQLiteRankingServer()
{
    AwaitFutures();

    if (m_pDatabase)
    {
        delete m_pDatabase;
        m_pDatabase = nullptr;
    }
}

bool CSQLiteRankingServer::IsValidPrefix(const std::string& prefix)
{
    std::lock_guard<std::mutex> lock(m_ValidPrefixListMutex);

    for (auto& p : m_ValidPrefixList)
    {
        if (prefix == p)
            return true;
    }
    return false;
}

CPlayerStats CSQLiteRankingServer::GetRankingSync(std::string nickname, std::string prefix)
{
    FixPrefix(prefix);

    CPlayerStats stats;
    if (!IsValidPrefix(prefix))
    {
        stats.Invalidate();
        return stats;
    }
    else if(!IsValidNickname(nickname))
    {
        stats.Invalidate();
        return stats;
    }

    std::string TableName = prefix + m_BaseTableName;
    auto Columns = stats.keys();
    size_t ColumnsSize = Columns.size();

    std::stringstream ss;

    ss << "SELECT ";
    ss << "R.Key as Key , R.Rank as Rank,";

    for (size_t i = 0; i < ColumnsSize; i++)
    {
        ss << " R." << Columns[i] << " as " << Columns[i];

        if (i < ColumnsSize - 1)
            ss << " ,\n";
    }

    // order by nickname as secondary criterium
    ss << " FROM ( "
       << "SELECT "
       << "Key , "
       << "ROW_NUMBER() OVER (ORDER BY " << m_RankingKey << (m_BiggestFirst ? " DESC " : " ASC , Key ASC") << ") Rank ,";

    for (size_t i = 0; i < ColumnsSize; i++)
    {
        ss << " " << Columns[i] << " ";

        if (i < ColumnsSize - 1)
            ss << " ,\n";
    }

    ss << " FROM " << TableName << " ) as R"
       << " WHERE Key = ? ;";

    try
    {
        SQLite::Statement stmt{*m_pDatabase, ss.str()};

        // sqlite binding starts counting at 1
        stmt.bind(1, nickname);

        if (stmt.executeStep())
        {
            // get rank column
            stats.SetRank(stmt.getColumn("Rank").getInt());

            // get all the other in CPlayerStats defined column values
            for (auto& column : Columns)
            {
                stats[column] = stmt.getColumn(column.c_str()).getInt();
            }
            return stats;
        }
        else
        {
            // not found - > returns invalid player stats.
            stats.Invalidate();
            return stats;
        }
    }
    catch (const SQLite::Exception& e)
    {
        throw;
    }
}

void CSQLiteRankingServer::SetRankingSync(std::string nickname, CPlayerStats stats, std::string prefix)
{
    FixPrefix(prefix);

    if (!IsValidPrefix(prefix))
        throw SQLite::Exception("Invalid prefix(not in valid prefix list): " + prefix);
    else if(!stats.IsValid())
        throw SQLite::Exception("Invalid player statistics passed.");
    else if(!IsValidNickname(nickname, prefix))
        throw SQLite::Exception("Invalid nickname: " + nickname);
    

    std::string TableName = prefix + m_BaseTableName;
    std::vector<std::string> Columns = stats.keys();
    size_t ColumnsSize = Columns.size();

    std::stringstream ss;

    ss << "INSERT OR REPLACE INTO " << TableName << " ( ";

    ss << "Key , "; // nickname is the primary key.

    // all columns
    for (size_t i = 0; i < ColumnsSize; i++)
    {
        ss << Columns[i];
        if (i < ColumnsSize - 1)
        {
            ss << " , ";
        }
    }

    // for evry column, create a bind variable, to escape possible user input
    ss << " ) VALUES ( ";
    ss << "?1 , "; // bind nickname

    for (size_t i = 0; i < ColumnsSize; i++)
    {
        ss << "?" << (i + 2);
        if (i < ColumnsSize - 1)
        {
            ss << " , ";
        }
    }
    ss << " );";

    try
    {
        // bind values to the execution statement
        SQLite::Statement stmt{*m_pDatabase, ss.str()};

        // columns start counting at 1, not at 0.
        stmt.bind(1, nickname); // primary key

        for (size_t i = 0; i < ColumnsSize; i++)
        {
            // column position offset
            stmt.bind(i + 2, stats[Columns[i]]);
        }

        // execute statement.
        stmt.exec();
    }
    catch (const SQLite::Exception& e)
    {
        // throw and add to backlog.
        throw;
    }
}

void CSQLiteRankingServer::UpdateRankingSync(std::string nickname, CPlayerStats stats, std::string prefix)
{
    FixPrefix(prefix);

    if (!IsValidPrefix(prefix))
        throw SQLite::Exception("Invalid prefix(not in valid prefix list): " + prefix);
    else if(!stats.IsValid())
        throw SQLite::Exception("Invalid player statistics passed.");
    else if(!IsValidNickname(nickname, prefix))
        throw SQLite::Exception("Invalid nickname: " + nickname);
    
    CPlayerStats savedStats;
    std::string TableName = prefix + m_BaseTableName;
    std::vector<std::string> Columns = stats.keys();
    size_t ColumnsSize = Columns.size();

    std::stringstream ss;

    ss << "SELECT ";
    ss << "Key ,";

    for (size_t i = 0; i < ColumnsSize; i++)
    {
        ss << " " << Columns[i];

        if (i < ColumnsSize - 1)
            ss << " ,\n";
    }

    ss << " FROM " << TableName << " WHERE Key = ? ;";

    try
    {
        SQLite::Statement stmt{*m_pDatabase, ss.str()};

        // sqlite binding starts counting at 1
        stmt.bind(1, nickname);


        if (stmt.executeStep())
        {
            // found player data

            // get all the other in CPlayerStats defined column values
            for (auto& column : Columns)
            {
                savedStats[column] = stmt.getColumn(column.c_str()).getInt();
            }        
        }
        else
        {
            // no data retrieved -> player is not ranked yet.
        }

        // reset stringstream
        ss.str(std::string());
        ss.clear();

        // update stats
        if (!savedStats.IsValid())
        {
            // invalid, reset to a valid state
            savedStats.Reset();
        }

        // savedStats is either empty or has the needed data stored.
        savedStats += stats;

        // construct query in order to insert tha updated player data
        ss << "INSERT OR REPLACE INTO " << TableName << " ( ";

        ss << "Key , "; // nickname is the primary key.

        // all columns
        for (size_t i = 0; i < ColumnsSize; i++)
        {
            ss << Columns[i];
            if (i < ColumnsSize - 1)
            {
                ss << " , ";
            }
        }

        // for evry column, create a bind variable, to escape possible user input
        ss << " ) VALUES ( ";
        ss << "?1 , "; // bind nickname

        for (size_t i = 0; i < ColumnsSize; i++)
        {
            ss << "?" << (i + 2);
            if (i < ColumnsSize - 1)
            {
                ss << " , ";
            }
        }
        ss << " );";


        // bind values to the execution statement
        SQLite::Statement stmt2{*m_pDatabase, ss.str()};

        // columns start counting at 1, not at 0.
        stmt2.bind(1, nickname); // primary key

        // bind new values to their respective columns
        for (size_t i = 0; i < ColumnsSize; i++)
        {
            // column position offset
            stmt2.bind(i + 2, savedStats[Columns[i]]);
        }

        // update player data.
        stmt2.exec();
    }
    catch (const SQLite::Exception& e)
    {
        throw;
    }
}

void CSQLiteRankingServer::DeleteRankingSync(std::string nickname, std::string prefix)
{
    FixPrefix(prefix);

    if (!IsValidPrefix(prefix))
        throw SQLite::Exception("Invalid prefix(not in valid prefix list): " + prefix);
    else if(!IsValidNickname(nickname, prefix))
        throw SQLite::Exception("Invalid nickname: " + nickname);

    std::string TableName = prefix + m_BaseTableName;

    std::stringstream ss;

    ss << "DELETE FROM " << TableName << " WHERE Key = ?;";

    try
    {
        SQLite::Statement stmt{*m_pDatabase, ss.str()};

        // where key = nickname
        stmt.bind(1, nickname);

        // delete player data
        stmt.exec();
    }
    catch(const SQLite::Exception& e)
    {
        throw;
    }
}

IRankingServer::key_stats_vec_t CSQLiteRankingServer::GetTopRankingSync(int topNumber, std::string key, std::string prefix, bool biggestFirst)
{
    FixPrefix(prefix);

    if (!IsValidPrefix(prefix))
        throw SQLite::Exception("Invalid prefix: " + prefix);
    
    CPlayerStats tmpStat;
    auto Columns = tmpStat.keys();
    size_t ColumnsSize = Columns.size();

    bool validKey = false;
    for (std::string& k: Columns)
    {
        if (k == key)
        {
            validKey = true;
            break;
        }  
    }
    
    if (!validKey)
        throw SQLite::Exception("Invalid key passed: " + key);

    std::string TableName = prefix + m_BaseTableName;

    std::stringstream ss;

    ss << "SELECT Key , ";

    // all columns
    for (size_t i = 0; i < ColumnsSize; i++)
    {
        ss << Columns[i];
        if (i < ColumnsSize - 1)
        {
            ss << " , ";
        }
    }

    // order by nickname as secondary criterium
    ss << " FROM " << TableName 
    << " ORDER BY " << m_RankingKey << ( biggestFirst ? " DESC " : " ASC ") 
    << ", Key ASC " 
    << " LIMIT " << topNumber << ";";

    try
    {   
        IRankingServer::key_stats_vec_t result;
        std::string tmpName;


        SQLite::Statement stmt{*m_pDatabase, ss.str()};

        while (stmt.executeStep())
        {
            tmpName = stmt.getColumn("Key").getString();            

            for (auto &column : Columns)
            {
                tmpStat[column] = stmt.getColumn(column.c_str()).getInt();
            }
            
            result.emplace_back(tmpName, tmpStat);

            // reset tmp variables
            tmpStat.Reset();
            tmpName = {};

        }
        return result;
    }
    catch(const SQLite::Exception& e)
    {
        throw;
    }
}
