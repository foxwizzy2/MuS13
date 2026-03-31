#include "stdafx.h"
#include "MonsterStats.h"
#include "Util.h"
#include "../../pugixml/pugixml.hpp"
#include <algorithm>
#include <cstdio>

CMonsterStats gMonsterStats;

CMonsterStats::CMonsterStats()
{
    Clear();
}

void CMonsterStats::Clear()
{
    m_enabled = false;
    m_default = MONSTER_STAT_MOD();
    m_specific.clear();
}

static float ReadAttrFloatPercent(const pugi::xml_node& node, const char* name, float defVal = 0.0f)
{
    // Accept missing attr => default
    // Accept "20" or "-10"
    if (!node.attribute(name))
        return defVal;

    return node.attribute(name).as_float(defVal);
}

bool CMonsterStats::Load(const char* path)
{
    Clear();

    LogAdd(LOG_BLUE, "[MonsterStats] Loading file: %s", path);

    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(path);
    if (!res)
    {
        LogAdd(LOG_RED, "[MonsterStats] ERROR: cannot load XML (%s) at offset %d", res.description(), (int)res.offset);
        // keep disabled if file missing/bad
        return false;
    }

    pugi::xml_node root = doc.child("monsterstatinfo");
    if (!root)
    {
        LogAdd(LOG_RED, "[MonsterStats] ERROR: <monsterstatinfo> root node missing");
        return false;
    }

    m_enabled = (root.attribute("enabled").as_int(1) != 0);

    LogAdd(LOG_BLUE, "[MonsterStats] System %s", (m_enabled ? "ENABLED" : "DISABLED"));

    if (!m_enabled)
    {
        LogAdd(LOG_BLUE, "[MonsterStats] Skipping load (disabled)");
        return true;
    }

    // default
    pugi::xml_node defNode = root.child("default");
    if (defNode)
    {
        m_default.hp = ReadAttrFloatPercent(defNode, "hp", 0.0f);
        m_default.damage = ReadAttrFloatPercent(defNode, "damage", 0.0f);
        m_default.defense = ReadAttrFloatPercent(defNode, "defense", 0.0f);
        m_default.attackrate = ReadAttrFloatPercent(defNode, "attackrate", 0.0f);
        m_default.defenserate = ReadAttrFloatPercent(defNode, "defenserate", 0.0f);
    }
    else
    {
        LogAdd(LOG_RED, "[MonsterStats] WARNING: <default> node missing (all defaults are 0)");
    }

    LogAdd(LOG_BLUE,
        "[MonsterStats] Default: HP=%0.2f%% DMG=%0.2f%% DEF=%0.2f%% AR=%0.2f%% DR=%0.2f%%",
        m_default.hp, m_default.damage, m_default.defense, m_default.attackrate, m_default.defenserate);

    // specific
    int specificCount = 0;

    pugi::xml_node specNode = root.child("specific");
    if (specNode)
    {
        for (pugi::xml_node m = specNode.child("monster"); m; m = m.next_sibling("monster"))
        {
            int id = m.attribute("id").as_int(-1);
            if (id < 0)
            {
                LogAdd(LOG_RED, "[MonsterStats] WARNING: <monster> entry with invalid/missing id");
                continue;
            }

            MONSTER_STAT_MOD mod;
            mod.hp = ReadAttrFloatPercent(m, "hp", 0.0f);
            mod.damage = ReadAttrFloatPercent(m, "damage", 0.0f);
            mod.defense = ReadAttrFloatPercent(m, "defense", 0.0f);
            mod.attackrate = ReadAttrFloatPercent(m, "attackrate", 0.0f);
            mod.defenserate = ReadAttrFloatPercent(m, "defenserate", 0.0f);

            if (mod.hasAny())
            {
                m_specific[id] = mod;
                specificCount++;
            }
        }
    }
    else
    {
        LogAdd(LOG_BLUE, "[MonsterStats] No <specific> section found");
    }

    LogAdd(LOG_BLUE, "[MonsterStats] Loaded %d specific monster entries", specificCount);

    return true;
}

int CMonsterStats::ApplyPercentInt(int base, float percent)
{
    // base * (1 + p/100)
    float mul = 1.0f + (percent / 100.0f);
    float v = (float)base * mul;

    // prevent negative/zero results from extreme negative percentages
    if (v < 1.0f)
        v = 1.0f;

    return (int)(v + 0.5f); // round to nearest int
}

static float ApplyPercentFloat(float base, float percent)
{
    float mul = 1.0f + (percent / 100.0f);
    float v = base * mul;
    if (v < 1.0f) v = 1.0f;
    return v;
}

void CMonsterStats::Apply(int monsterClass,
    float& life, float& maxLife,
    int& dmgMin, int& dmgMax,
    int& defense,
    int& attackRate, int& defenseRate) const
{
    if (!m_enabled)
        return;

    const MONSTER_STAT_MOD* spec = nullptr;
    auto it = m_specific.find(monsterClass);
    if (it != m_specific.end())
        spec = &it->second;

    // Compose percent: default + specific (additive percentages)
    // Example default hp=20, specific hp=-10 => net +10%
    auto percentSum = [&](float defP, float specP) -> float { return defP + specP; };

    float hpP = percentSum(m_default.hp, spec ? spec->hp : 0.0f);
    float dmgP = percentSum(m_default.damage, spec ? spec->damage : 0.0f);
    float defP = percentSum(m_default.defense, spec ? spec->defense : 0.0f);
    float arP = percentSum(m_default.attackrate, spec ? spec->attackrate : 0.0f);
    float drP = percentSum(m_default.defenserate, spec ? spec->defenserate : 0.0f);

    float oldMax = maxLife;
    maxLife = ApplyPercentFloat(maxLife, hpP);
    life = maxLife;
    if (life > maxLife)
    {
        life = maxLife;
    }

    dmgMin = ApplyPercentInt(dmgMin, dmgP);
    dmgMax = ApplyPercentInt(dmgMax, dmgP);

    defense = ApplyPercentInt(defense, defP);
    attackRate = ApplyPercentInt(attackRate, arP);
    defenseRate = ApplyPercentInt(defenseRate, drP);
}

void CMonsterStats::Reload()
{
    LogAdd(LOG_BLUE, "[MonsterStats] Reload requested...");

    if (this->Load("..\\Data\\Custom\\MonsterStats.xml"))
    {
        LogAdd(LOG_GREEN, "[MonsterStats] Reload completed.");
    }
    else
    {
        LogAdd(LOG_RED, "[MonsterStats] Reload FAILED.");
    }
}