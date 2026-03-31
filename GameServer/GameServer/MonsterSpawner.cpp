#include "StdAfx.h"
#include "MonsterSpawner.h"
#include "MemScript.h"
#include "GameMain.h"
#include "user.h"
#include "Util.h"
#include "Log.h"
#include "MonsterSetBase.h"
#include "Monster.h"
#include "Notice.h"
#include "Path.h"
#include "InvasionManager.h"

SYSTEMTIME t;

cMonsterSpawner gMonsterSpawner;

cMonsterSpawner::cMonsterSpawner()
{
}

cMonsterSpawner::~cMonsterSpawner()
{
}

void cMonsterSpawner::Load(char* path)
{
	this->isLoaded = false;

	for(int i=0; i < MAX_CONFIG; i++)
	{
		this->Group[i] = -1;
		this->Enable[i] = false;
		this->Type[i] = -1;
		this->Value[i] = -1;
		this->Hour[i] = -1;
		this->Min[i] = -1;
		this->TimeEnd[i] = -1;
		this->EventStart[i] = false;
		this->Tick[i] = -1;
	}

	this->TotalMob = 0;

	this->TotalConfig = 0;

	for(int i=0; i<MAX_MONSTER; i++)
	{
		this->MonsterIndex[i]	= -1;
		this->MonsterClass[i] = -1;

		this->Monster[i].Group	= -1;
		this->Monster[i].Mob	= -1;
		this->Monster[i].Map	= -1;
		this->Monster[i].X1		= -1;
		this->Monster[i].Y1		= -1;
		this->Monster[i].X2		= -1;
		this->Monster[i].Y2		= -1;
		this->Monster[i].type	= -1;
		this->Monster[i].value	= -1;

		this->Monster[i].generated	= false;
		this->Monster[i].dead		= false;
	}

	CMemScript* lpScript = new(std::nothrow) CMemScript;

	if(lpScript == NULL)
	{
		return;
	}

	if(lpScript->SetBuffer(path) == 0)
	{
		delete lpScript;
		return;
	}

	int Category = -1;

	try
	{
		while(true)
		{
			if( lpScript->GetToken() == 2 )
			{
				break;
			}

			Category = lpScript->GetNumber();

			while(true)
			{
				if( Category == 0 )	//-> Config
				{
					if(strcmp("end",lpScript->GetAsString()) == 0)
					{
						break;
					}

					if(this->TotalConfig >= MAX_CONFIG)
					{
						continue;
					}

					this->Group[this->TotalConfig]		= lpScript->GetNumber();
					this->Enable[this->TotalConfig]		= lpScript->GetAsNumber();
					this->Type[this->TotalConfig]		= lpScript->GetAsNumber();
					this->Value[this->TotalConfig]		= lpScript->GetAsNumber();
					this->Hour[this->TotalConfig]		= lpScript->GetAsNumber();
					this->Min[this->TotalConfig]		= lpScript->GetAsNumber();
					this->TimeEnd[this->TotalConfig]	= lpScript->GetAsNumber();
					this->TimeEnd[this->TotalConfig]	*= 60;

					strcpy(this->Name[this->TotalConfig],lpScript->GetAsString());
					strcpy(this->StartMessage[this->TotalConfig],lpScript->GetAsString());
					strcpy(this->DeathMessage[this->TotalConfig],lpScript->GetAsString());
					strcpy(this->EvadeMessage[this->TotalConfig],lpScript->GetAsString());

					/*LogAdd(LOG_DEBUG, "DEBUG MONSTER SPAWNER %d %d %d %d %d %d %d",
						this->Group[this->TotalConfig],this->Enable[this->TotalConfig],this->Type[this->TotalConfig],this->Value[this->TotalConfig],
						this->Hour[this->TotalConfig],this->Min[this->TotalConfig],this->TimeEnd[this->TotalConfig]);*/

					/*LogAdd(LOG_DEBUG, "DEBUG MONSTER SPAWNER %s %s %s %s",
						this->Name[this->TotalConfig],this->StartMessage[this->TotalConfig],this->DeathMessage[this->TotalConfig],this->EvadeMessage[this->TotalConfig]);*/
					
					this->TotalConfig++;
				}
				else if( Category == 1 ) //-> Monster
				{
					if(strcmp("end",lpScript->GetAsString()) == 0)
					{
						break;
					}

					if(this->TotalMob >= MAX_MONSTER)
					{
						break;
					}

					this->Monster[this->TotalMob].Group	= lpScript->GetNumber();
					this->Monster[this->TotalMob].Mob	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].Map	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].X1	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].Y1	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].X2	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].Y2	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].type	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].value	= lpScript->GetAsNumber();
					this->Monster[this->TotalMob].value	*= 60;

					strcpy(this->Monster[this->TotalMob].StartMessage,lpScript->GetAsString());
					strcpy(this->Monster[this->TotalMob].DeathMessage,lpScript->GetAsString());
					

					/*LogAdd(LOG_DEBUG, "DEBUG MONSTER SPAWNER %d %d %d %d %d",
						this->Monster[this->TotalMob].Mob,this->Monster[this->TotalMob].Map,this->Monster[this->TotalMob].X1,
						this->Monster[this->TotalMob].Y1,this->Monster[this->TotalMob].Group);*/

					this->TotalMob++;
				}
			}
		}
	}
	catch(...)
	{
		delete lpScript;
		MessageBox(0, path, "Error!",0);
		return;
	}

	LogAdd(LOG_BLACK,"[Monster] %s is loaded (Groups: %d | Mobs: %d)", path, this->TotalConfig, this->TotalMob);
	this->isLoaded = true;
	delete lpScript;
}

void cMonsterSpawner::AddMonster(int Code)
{
	int result = -1;
	
	gLog.Output(MONSTER_SPAWNER_LOG, "[AddMonster] Code: %d", Code);

	for(int n=0; n < gMonsterSetBase.m_count; n++)
	{
		if(gMonsterSetBase.m_MonsterSetBaseInfo[n].Type != 3 || gMonsterSetBase.m_MonsterSetBaseInfo[n].MonsterClass != this->Monster[Code].Mob ||
			gMonsterSetBase.m_MonsterSetBaseInfo[n].Map != this->Monster[Code].Map || gMonsterSetBase.m_MonsterSetBaseInfo[n].X != this->Monster[Code].X1 || gMonsterSetBase.m_MonsterSetBaseInfo[n].Y != this->Monster[Code].Y1)
		{
			continue;
		}

		result = gObjAddMonster(this->Monster[Code].Map);

		if(OBJECT_RANGE(result) == 0)
		{
			continue;
		}

		gLog.Output(MONSTER_SPAWNER_LOG, "[AddMonster] result (index): %d", result);

		LPOBJ lpObj = &gObj[result];

		if(gObjSetPosMonster(result,n) == 0)
		{
			LogAdd(LOG_RED, "gObjDel %s %s %d [Class: %d]", __FILE__, __FUNCTION__, __LINE__, gObj[result].Class);
			gObjDel(result);
			continue;
		}

		if(gObjSetMonster(result, gMonsterSetBase.m_MonsterSetBaseInfo[n].MonsterClass) == 0)
		{
			LogAdd(LOG_RED, "gObjDel %s %s %d [Class: %d]", __FILE__, __FUNCTION__, __LINE__, gObj[result].Class);
			gObjDel(result);
			continue;
		}

		lpObj->MaxRegenTime = 1000*60*60*24; //24 horas para processar regen

		this->MonsterIndex[Code] = result;
		this->MonsterClass[Code] = gObj[result].Class;
		this->Monster[Code].generated = true;

		gLog.Output(MONSTER_SPAWNER_LOG, "[AddMonster] Monster created: Map: %d | X: %d | Y: %d | Index: %d",lpObj->Map,lpObj->X,lpObj->Y,result);

		gNotice.SendMapServerGroupMsg(0, this->Monster[Code].StartMessage, lpObj->MonsterName);

		this->CheckAndSendMapEffect(&gObj[result]);

		return;
	}
	
	gLog.Output(MONSTER_SPAWNER_LOG, "[AddMonster] Monster not found in MonsterSetBase");

	if(OBJECT_RANGE(result) == 0)
	{
		LogAdd(LOG_RED, "gObjDel %s %s %d [Class: %d]", __FILE__, __FUNCTION__, __LINE__, gObj[result].Class);
		gObjDel(result);
	}
}

void cMonsterSpawner::StartMonster(int p)
{
	for(int i=0; i<this->TotalMob; i++)
	{
		if(this->Monster[i].Group == this->Group[p])
		{
			if(this->Monster[i].type == 0)
			{
				gLog.Output(MONSTER_SPAWNER_LOG, "[StartMonster] Adding type 0 monster from group %d",p);
				this->AddMonster(i);
			}
		}
	}
}

void cMonsterSpawner::EndMonster(int p)
{
	for ( int i = 0; i < MAX_MONSTER; i++)
	{
		if(this->Monster[i].Group == this->Group[p])
		{
			if ( this->MonsterIndex[i] != -1 )
			{
				if (this->Monster[i].dead == false)
				{
					gLog.Output(MONSTER_SPAWNER_LOG, "[EndMonster] Monster deleted: %d (%d) | Group: %d", gObj[this->MonsterIndex[i]].Class, this->MonsterIndex[i], p);
					LogAdd(LOG_RED, "gObjDel %s %s %d [Class: %d]", __FILE__, __FUNCTION__, __LINE__, gObj[this->MonsterIndex[i]].Class);
					gObjDel(this->MonsterIndex[i]);
				}
				
				this->MonsterIndex[i] = -1;
				this->MonsterClass[i] = -1;
				this->Monster[i].generated = false;
				this->Monster[i].dead = false;
				this->CheckAndSendMapEffect(&gObj[this->MonsterIndex[i]]);
			}
		}
	}
}

void cMonsterSpawner::KillMonster(LPOBJ lpObj, LPOBJ lpMonster)
{
	if (gMonsterSpawner.EventStatus() == false)
	{
		return;
	}

	int Code = -1;
	int i;

	for ( i = 0; i < this->TotalMob; i++)
	{
		if(this->MonsterIndex[i] == lpMonster->Index && this->MonsterClass[i] == lpMonster->Class)
		{
			Code = i;
			break;
		}		
	}

	if(Code == -1)
	{
		return;
	}

	int iMaxHitUser = gObjMonsterGetTopHitDamageUser(lpMonster);

	gNotice.SendMapServerGroupMsg(0, this->Monster[Code].DeathMessage, gObj[iMaxHitUser].Name);

	this->Monster[Code].dead = true;

	gObjMonsterDieGiveItem(lpMonster, lpObj);

	LogAdd(LOG_RED, "gObjDel %s %s %d [Class: %d]", __FILE__, __FUNCTION__, __LINE__, gObj[lpMonster->Index].Class);
	gObjDel(lpMonster->Index);

	int NextCode = Code + 1;

	if(this->Monster[NextCode].Group > -1)
	{
		if(this->Monster[NextCode].type == 2 && this->Monster[NextCode].generated == false)
		{
			gLog.Output(MONSTER_SPAWNER_LOG, "[KillMonster] Adding monster for type 2, group %d",this->Monster[NextCode].Group);
			this->AddMonster(NextCode);
		}
	}

	for ( i = 0; i < this->TotalMob; i++)
	{
		if(this->Monster[Code].Group == this->Monster[i].Group )
		{
			if(this->Monster[i].dead == false)
			{
				gLog.Output(MONSTER_SPAWNER_LOG, "[KillMonster] Return due to pending monster, group %d",this->Monster[Code].Group);
				return;
			}
		}
	}

	int ConfigCode = -1;

	for(i = 0; i < this->TotalConfig; i++)
	{
		if(this->Monster[Code].Group == this->Group[i])
		{
			if (this->EventStart[i])
			{
				ConfigCode = i;
			}
		}
	}

	if (ConfigCode >= 0)
	{
		gNotice.SendMapServerGroupMsg(0, this->DeathMessage[ConfigCode]);
		gLog.Output(MONSTER_SPAWNER_LOG, "[KillMonster] Ending invasion %d", ConfigCode);
		this->EndEvent(ConfigCode);
	}
	/*else
	{
		this->Load(gPath.GetRightPath("Monster\\MonsterSpawner.txt"));
	}*/
}

void cMonsterSpawner::StartEvent(int p)
{
	this->EventStart[p] = true;
	gNotice.SendMapServerGroupMsg(0, this->StartMessage[p]);
	gLog.Output(MONSTER_SPAWNER_LOG, "[StartEvent] Starting invasion %d", p);
	this->StartMonster(p);
}

void cMonsterSpawner::EndEvent(int p)
{
	this->Tick[p] = 0;
	this->EventStart[p] = false;
	gLog.Output(MONSTER_SPAWNER_LOG, "[EndEvent] Ending invasion %d", p);
	this->EndMonster(p);
}

void cMonsterSpawner::TickTime()
{
	if (this->isLoaded != true)
	{
		return;
	}

	int Code;
	
	for(int i = 0; i < this->TotalConfig; i++)
	{
		if (!this->Enable[i])
		{
			continue;
		}

		if(this->EventStart[i] == true)
		{
			this->Tick[i]++;

			if(this->Tick[i] >= this->TimeEnd[i])
			{
				gLog.Output(MONSTER_SPAWNER_LOG, "[TickTime] Ending invasion %d due to time expiration ( %d / %d )",i,this->Tick[i],this->TimeEnd[i]);
				this->EndEvent(i);
				gNotice.SendMapServerGroupMsg(0, this->EvadeMessage[i]);
			}

			//fazer monstro do type 1 nascer
			Code = -1;
			for ( int j=0; j < this->TotalMob; j++)
			{
				if(this->Monster[j].Group != this->Group[i] || this->Monster[j].generated)
				{
					continue;
				}

				if(this->Monster[j].type == 1 && this->Tick[i] >= this->Monster[j].value)
				{
					gLog.Output(MONSTER_SPAWNER_LOG, "[TickTime] Creating monster type 1 (time-based) in the invasion %d",i);
					gNotice.SendMapServerGroupMsg(0, this->Monster[j].StartMessage);
					this->AddMonster(j);
				}
			}
			//return;
		}
	}

	GetLocalTime(&t);

	int s;

	//Start type 2 events, and return if success
	for(s = 0; s < this->TotalConfig; s++)
	{
		if(!this->Enable[s] || this->EventStart[s] == true)
			continue;

		if(this->Type[s] == 2)
		{
			if(t.wDay == this->Value[s] && t.wHour == this->Hour[s] && t.wMinute == this->Min[s] && t.wSecond <= 3)
			{
				gLog.Output(MONSTER_SPAWNER_LOG, "[TickTime] Starting event %d type 2",s);
				this->StartEvent(s);
				return;
			}
		}
	}

	//Start type 1 events, and return if success
	for(s = 0; s < this->TotalConfig; s++)
	{
		if(!this->Enable[s] || this->EventStart[s] == true)
			continue;

		if(this->Type[s] == 1)
		{
			if(t.wDayOfWeek == this->Value[s] && t.wHour == this->Hour[s] && t.wMinute == this->Min[s] && t.wSecond <= 3)
			{
				//gLog.Output(MONSTER_SPAWNER_LOG, "[TickTime] Starting event %d type 1",s);
				this->StartEvent(s);
				return;
			}
		}
	}

	//Start type 0 events, and return if success
	for(s = 0; s < this->TotalConfig; s++)
	{
		if(!this->Enable[s] || this->EventStart[s] == true)
			continue;

		if(this->Type[s] == 0)
		{
			if(t.wHour == this->Hour[s] && t.wMinute == this->Min[s] && t.wSecond <= 3)
			{
				gLog.Output(MONSTER_SPAWNER_LOG, "[TickTime] Starting event %d type 0",s);
				this->StartEvent(s);
				return;
			}
		}
	}
}

bool cMonsterSpawner::EventStatus()
{
	for(int s=0; s < this->TotalConfig; s++)
	{
		if(this->EventStart[s] == true)
		{
			return true;
		}
	}

	return false;
}

bool acompare(TimeTable lhs, TimeTable rhs) { return lhs.Minutes < rhs.Minutes; }

struct INVASION_UI_DEF
{
	int Type;
	const char* Name;
};

static INVASION_UI_DEF g_InvasionUiDefs[16] =
{
	{ INV_GOLDEN_MONSTER,   "Golden Invasion" },      // 0
	{ INV_FIRE_DRAGON,      "Red Dragon" },           // 1
	{ INV_WHITE_WIZARD,     "White Wizard" },         // 2
	{ INV_MEDUSA,           "Medusa" },               // 3
	{ INV_CURSED_SANTA,     "Cursed Santa" },         // 4
	{ INV_GOLDEN_GOBLIN,    "Golden Goblin" },        // 5
	{ INV_PET_EGG,          "Egg Invasion" },         // 6
	{ INV_SKELETON_KING,    "Skeleton King" },        // 7
	// restul le poți umple sau lăsa libere
	{ -1,                   "" },                     // 8
	{ -1,                   "" },                     // 9
	{ -1,                   "" },                     // 10
	{ -1,                   "" },                     // 11
	{ -1,                   "" },                     // 12
	{ -1,                   "" },                     // 13
	{ -1,                   "" },                     // 14
	{ -1,                   "" },                     // 15
};

void cMonsterSpawner::GetAndSendNextInvasions(int aIndex)
{
	// Tabel local pentru sortare după minute rămase
	TimeTable MinutesTable[16];

	for (int i = 0; i < 16; i++)
	{
		MinutesTable[i].Minutes = 65000;
		ZeroMemory(MinutesTable[i].Name, 32);

		// dacă nu e definit un tip valid, sărim
		if (g_InvasionUiDefs[i].Type < 0)
			continue;

		// numele care va ajunge în client
		lstrcpynA(MinutesTable[i].Name, g_InvasionUiDefs[i].Name, 31);

		// timp rămas (secunde) din noul sistem InvasionManager
		int remainSec = gInvasionManager.GetRemainTimeSec(g_InvasionUiDefs[i].Type);

		// dacă nu avem schedule sau e trecut, nu trimitem linia (rămâne 65000)
		if (remainSec <= 0)
			continue;

		// convertim în minute (rotunjit în sus, ca să nu afișeze 0 prea devreme)
		int remainMin = (remainSec + 59) / 60;

		MinutesTable[i].Minutes = remainMin;
	}

	// sortare crescătoare după minute
	std::sort(MinutesTable, MinutesTable + 16, acompare);

	PMSG_SCHEDULE_INVASIONS InvasionsList;
	InvasionsList.h.set(0xF3, 0xE7, sizeof(PMSG_SCHEDULE_INVASIONS));

	for (int i = 0; i < 16; i++)
	{
		InvasionsList.Time = -1;
		ZeroMemory(InvasionsList.Name, 32);

		InvasionsList.Order = i;

		if (MinutesTable[i].Minutes < 65000)
		{
			// Avem un event valid → trimitem timp și nume
			lstrcpynA(InvasionsList.Name, MinutesTable[i].Name, 31);
			InvasionsList.Time = MinutesTable[i].Minutes;
		}
		else
		{
			// NU avem event → trimitem timp = -1 și nume gol
			InvasionsList.Time = -1;
			ZeroMemory(InvasionsList.Name, 32);
		}

		DataSend(aIndex, (LPBYTE)&InvasionsList, sizeof(InvasionsList));
	}
}

void cMonsterSpawner::CheckAndSendMapEffect(LPOBJ lpObj)
{
	if (this->isLoaded != true)
	{
		return;
	}

	int Code;

	for(int i = 0; i < this->TotalConfig; i++)
	{
		if (!this->Enable[i])
		{
			continue;
		}

		for ( int j=0; j < this->TotalMob; j++)
		{
			if(this->Monster[j].Group == this->Group[i])
			{
				if(this->Monster[j].Mob == 42) //Red Dragon
				{
					if (this->EventStart[i] == true && lpObj->Map == this->Monster[j].Map)
					{
						GCMapEventStateSend(lpObj->Map, 1, 1);
					}
					else
					{
						GCMapEventStateSend(lpObj->Map, 0, 1);
					}
				}

				if(this->Monster[j].Mob == 79 || this->Monster[j].Mob == 501) //Golden Dragons
				{
					if (this->EventStart[i] == true && lpObj->Map == this->Monster[j].Map)
					{
						GCMapEventStateSend(lpObj->Map, 1, 3);
					}
					else
					{
						GCMapEventStateSend(lpObj->Map, 0, 3);
					}
				}
			}
		}
	}
}