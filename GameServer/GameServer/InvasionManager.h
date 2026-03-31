#pragma once

#include <vector>
#include "ScheduleManager.h"
#include "MemScript.h"
#include "User.h"

enum eInvasionType
{
	INV_SKELETON_KING = 0, // 0: Skeleton King invasion event
	INV_FIRE_DRAGON = 1, // 1: Fire Dragon King invasion
	INV_GOLDEN_MONSTER = 2, // 2: Gold Monster Invasion
	INV_WHITE_WIZARD = 3, // 3: White Mage Invasion
	INV_LUCKY_BAG = 4, // 4: New Year Lucky Bag Invasion
	INV_GOLDEN_RABBIT = 50, // 50+: Golden Rabbit Invasion
	INV_FIRE_FLAME_GHOST = 60, // 60+: Fire Flame Ghost Invasion
	INV_CURSED_SANTA = 7, // 7: The Cursed Santa Invasion
	INV_MEDUSA = 8, // 8: Medusa invasion
	INV_GOLDEN_GOBLIN = 9, // 9: Golden Goblin Invasion
	INV_PET_EGG = 10, // 10: Pet Egg Monster Invasion
};

#define MAX_INVASION_TYPE 64 // suficient pentru ce ai acum

struct INVASION_START_TIME
{
	int Year;
	int Month;
	int Day;
	int DayOfWeek;
	int Hour;
	int Minute;
	int Second;
};

struct INVASION_SPAWN_INFO
{
	int Map;
	int Class;
	int Count;
	int X1, Y1;
	int X2, Y2;
	int IsBoss;
};

struct INVASION_MESSAGE
{
	char StartMsg[128];
	char KillMsg[128];
};

class CInvasionManager
{
public:
	CInvasionManager();
	void Init();
	void Load(char* path);

	int GetRemainTimeSec(int type); // pentru UI (secunde până la următoarea invazie)

	void MainProc();                 // nou
	void StartInvasion(int type);    // nou

	void MonsterDieProc(LPOBJ lpMonster, LPOBJ lpKiller);

	void ReloadInvasions();
	void StartInvasionManual(int type);

private:
	std::vector<INVASION_START_TIME> m_InvasionStartTime[MAX_INVASION_TYPE];
	std::vector<INVASION_SPAWN_INFO> m_InvasionSpawnInfo[MAX_INVASION_TYPE];
	int m_LastStartTypeTime[MAX_INVASION_TYPE]; // ca să nu pornim de 100x
	INVASION_MESSAGE m_InvasionMessage[MAX_INVASION_TYPE];
};

extern CInvasionManager gInvasionManager;