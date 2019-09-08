#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#include <map>
#include <ostream>
#include <string>
#include <vector>

struct CPlayerStats
{
    /**
     * If you want to change this class, all you need to do is to change the costructors
     * and the stats fields that are pushed in the constructor into m_Data.
     */
   private:
    // invalidate stats in order to signal, that there is no such data available.
    bool m_IsValid;

    // specifically set value, that needs to be calculated by the database.
    long m_Rank;

    // map containing
    std::map<std::string, int> m_Data;


   public:
   
    CPlayerStats();
    CPlayerStats(int kills, int deaths, int ticksCaught, int ticksIngame, int ticksWarmup, int score, int wins, int fails, int shots);

    // make stats invalid.
    void Invalidate();
    bool IsValid() const { return m_IsValid; };

    // reset to default/0/empty values.
    void Reset();


    void SetRank(long rank) { m_Rank = rank; };
    long GetRank() const { return m_Rank;};


    CPlayerStats& operator+=(const CPlayerStats& rhs);

    CPlayerStats operator+(const CPlayerStats& rhs);

    int& operator[](const std::string& key);

    std::vector<std::string> keys(std::string prefix = "") const;
    std::vector<int> values() const;

    std::vector<std::pair<std::string, std::string>> GetStringPairs(std::string prefix = "") const;

    size_t size() { return m_Data.size(); };
};

[[maybe_unused]] static std::ostream& operator<<(std::ostream& os, const CPlayerStats& stats)
{
    os << "PlayerStats:\n{\n";
    for (auto& [key, value] : stats.GetStringPairs())
    {
        os << "  " << key << ": " << value << std::endl;
    }
    os << "}\n";

    return os;
}
#endif // PLAYER_STATS_H
