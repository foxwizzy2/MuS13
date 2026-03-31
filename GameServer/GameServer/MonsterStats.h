#pragma once

#include <map>
#include <cstdint>

struct MONSTER_STAT_MOD
{
    float hp;
    float damage;
    float defense;
    float attackrate;
    float defenserate;

    MONSTER_STAT_MOD()
    {
        hp = 0.0f;
        damage = 0.0f;
        defense = 0.0f;
        attackrate = 0.0f;
        defenserate = 0.0f;
    }

    bool hasAny() const
    {
        return (hp != 0.0f || damage != 0.0f || defense != 0.0f || attackrate != 0.0f || defenserate != 0.0f);
    }
};

class CMonsterStats
{
public:
    CMonsterStats();

    bool Load(const char* path);
    void Clear();

    bool IsEnabled() const { return m_enabled; }

    // Apply modifiers to already-initialized stats (base stats must be set before calling this).
    // monsterClass is the monster id/class (lpObj->Class).
    void Apply(int monsterClass,
        float& life, float& maxLife,
        int& dmgMin, int& dmgMax,
        int& defense,
        int& attackRate, int& defenseRate) const;
    void Reload();

private:
    static int ApplyPercentInt(int base, float percent);

private:
    bool m_enabled;

    MONSTER_STAT_MOD m_default;
    std::map<int, MONSTER_STAT_MOD> m_specific;
};

extern CMonsterStats gMonsterStats;
