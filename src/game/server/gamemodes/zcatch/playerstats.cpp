#include "playerstats.h"


CPlayerStats::CPlayerStats(): CPlayerStats(0, 0, 0, 0, 0, 0, 0, 0)
{
    
}

void CPlayerStats::Reset()
{
    m_IsValid = true;
    for (auto& [key, value] : m_Data)
    {
        value = 0;
    }
}

void CPlayerStats::Invalidate() {
    Reset();
    m_IsValid = false; 
    
}

CPlayerStats::CPlayerStats(int kills, int deaths, int ticksCaught, int ticksIngame, int ticksWarmup, int score, int fails, int shots) : m_IsValid{true}
{
    m_Data = {
        {"Kills", kills},
        {"Deaths", deaths},
        {"TicksCaught", ticksCaught},
        {"TicksIngame", ticksIngame},
        {"TicksWarmup", ticksWarmup},
        {"Score", score},
        {"Fails", fails},
        {"Shots", shots}
    };
}

std::vector<std::string> CPlayerStats::keys(std::string prefix) const
{
    std::vector<std::string> v;
    v.reserve(m_Data.size());

    for (auto& [key, value] : m_Data)
    {
        if(prefix.size() > 0)
        {
            v.push_back(prefix + key);
        }
        else
        {
            v.push_back(key);
        }       
    }

    return v;
}

std::vector<int> CPlayerStats::values() const
{
    std::vector<int> v;
    v.reserve(m_Data.size());

    for (auto& [key, value] : m_Data)
    {
        v.push_back(value);      
    }
    return v;
}

std::vector<std::pair<std::string, std::string>> CPlayerStats::GetStringPairs(std::string prefix) const
{
    std::vector<std::pair<std::string, std::string> > v;
    v.reserve(m_Data.size());

    for (auto& [key, value] : m_Data)
    {
        v.emplace_back(prefix + key, std::to_string(value));
    }

    return v;
}

CPlayerStats& CPlayerStats::operator+=(const CPlayerStats& rhs)
{
    for (auto&& [key, value] : m_Data)
    {
        value += rhs.m_Data.at(key);
    }   
    return (*this);
}

CPlayerStats CPlayerStats::operator+(const CPlayerStats& rhs)
{
    return CPlayerStats() += rhs;
}

int& CPlayerStats::operator[](const std::string& key)
{
    return m_Data.at(key);
}

