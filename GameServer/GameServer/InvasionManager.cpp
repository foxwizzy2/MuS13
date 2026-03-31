#include "stdafx.h"
#include "InvasionManager.h"
#include "Util.h"
#include "User.h"
#include "Notice.h"

CInvasionManager gInvasionManager;

CInvasionManager::CInvasionManager()
{
    this->Init();
}

void CInvasionManager::Init()
{
    for (int i = 0; i < MAX_INVASION_TYPE; i++)
    {
        this->m_InvasionStartTime[i].clear();
        this->m_InvasionSpawnInfo[i].clear();
        this->m_LastStartTypeTime[i] = 0;
        memset(this->m_InvasionMessage[i].StartMsg, 0, sizeof(this->m_InvasionMessage[i].StartMsg));
        memset(this->m_InvasionMessage[i].KillMsg, 0, sizeof(this->m_InvasionMessage[i].KillMsg));
    }
}

void CInvasionManager::Load(char* path)
{
    this->Init();

    CMemScript* lpMemScript = new CMemScript;

    if (lpMemScript == 0)
    {
        ErrorMessageBox("CMemScript could not be allocated.");
        return;
    }

    if (lpMemScript->SetBuffer(path) == 0)
    {
        ErrorMessageBox(lpMemScript->GetLastError());
        delete lpMemScript;
        return;
    }

    try
    {
        while (true)
        {
            if (lpMemScript->GetToken() == TOKEN_END)
                break;

            int section = lpMemScript->GetNumber();

            while (true)
            {
                int token = lpMemScript->GetToken();
                if (token == TOKEN_END)
                    break;

                // linie de închidere secțiune
                if (token == TOKEN_STRING &&
                    strcmp("end", lpMemScript->GetString()) == 0)
                    break;

                if (section == 0)
                {
                    // Type  Year Month Day DayOfWeek Hour Min Sec
                    int type = lpMemScript->GetNumber(); // folosește tokenul numeric deja citit

                    INVASION_START_TIME info;
                    info.Year = lpMemScript->GetAsNumber();
                    info.Month = lpMemScript->GetAsNumber();
                    info.Day = lpMemScript->GetAsNumber();
                    info.DayOfWeek = lpMemScript->GetAsNumber();
                    info.Hour = lpMemScript->GetAsNumber();
                    info.Minute = lpMemScript->GetAsNumber();
                    info.Second = lpMemScript->GetAsNumber();

                    if (type >= 0 && type < MAX_INVASION_TYPE)
                        this->m_InvasionStartTime[type].push_back(info);
                }
                else if (section == 1)
                {
                    // Type Map Class Count X1 Y1 X2 Y2
                    int type = lpMemScript->GetNumber();

                    INVASION_SPAWN_INFO info;
                    info.Map = lpMemScript->GetAsNumber();
                    info.Class = lpMemScript->GetAsNumber();
                    info.Count = lpMemScript->GetAsNumber();
                    info.X1 = lpMemScript->GetAsNumber();
                    info.Y1 = lpMemScript->GetAsNumber();
                    info.X2 = lpMemScript->GetAsNumber();
                    info.Y2 = lpMemScript->GetAsNumber();
                    info.IsBoss = lpMemScript->GetAsNumber();

                    if (type >= 0 && type < MAX_INVASION_TYPE)
                        this->m_InvasionSpawnInfo[type].push_back(info);
                }
                else if (section == 2) // <-- NOU
                {
                    // Type  StartMsg  KillMsg
                    int type = lpMemScript->GetNumber();

                    char startMsg[128];
                    char killMsg[128];

                    strcpy_s(startMsg, lpMemScript->GetAsString());
                    strcpy_s(killMsg, lpMemScript->GetAsString());

                    if (type >= 0 && type < MAX_INVASION_TYPE)
                    {
                        strcpy_s(this->m_InvasionMessage[type].StartMsg, startMsg);
                        strcpy_s(this->m_InvasionMessage[type].KillMsg, killMsg);
                    }
                }
            }
        }
    }
    catch (...)
    {
        ErrorMessageBox(lpMemScript->GetLastError());
    }

    delete lpMemScript;

    LogAdd(LOG_BLACK, "[InvasionManager] Loaded %s", path);
}

void CInvasionManager::MainProc()
{
    time_t now = time(0);

    for (int type = 0; type < MAX_INVASION_TYPE; type++)
    {
        if (this->m_InvasionStartTime[type].empty())
            continue;

        int remain = this->GetRemainTimeSec(type);

        // dacă mai sunt <= 5 secunde și nu am mai pornit această rundă
        if (remain <= 5 && remain >= 0)
        {
            // simplu anti-spam: nu reporni mai des de 60s
            if (now - this->m_LastStartTypeTime[type] > 60)
            {
                this->StartInvasion(type);
                this->m_LastStartTypeTime[type] = (int)now;
            }
        }
    }
}

int CInvasionManager::GetRemainTimeSec(int type)
{
    if (type < 0 || type >= MAX_INVASION_TYPE)
    {
        return 0;
    }

    if (this->m_InvasionStartTime[type].empty() != 0)
    {
        return 0;
    }

    CTime ScheduleTime;
    CScheduleManager ScheduleManager;

    for (std::vector<INVASION_START_TIME>::iterator it = this->m_InvasionStartTime[type].begin();
        it != this->m_InvasionStartTime[type].end(); it++)
    {
        ScheduleManager.AddSchedule(
            it->Year,
            it->Month,
            it->Day,
            it->Hour,
            it->Minute,
            it->Second,
            it->DayOfWeek
        );
    }

    if (ScheduleManager.GetSchedule(&ScheduleTime) == 0)
    {
        return 0;
    }

    int RemainTime = (int)difftime(ScheduleTime.GetTime(), time(0));

    return (RemainTime > 0) ? RemainTime : 0;
}


void CInvasionManager::MonsterDieProc(LPOBJ lpMonster, LPOBJ lpKiller)
{
    if (lpMonster == nullptr || lpMonster->Type != OBJECT_MONSTER)
        return;

    int classId = lpMonster->Class;
    int map = lpMonster->Map;

    for (int type = 0; type < MAX_INVASION_TYPE; type++)
    {
        if (this->m_InvasionSpawnInfo[type].empty())
            continue;

        for (auto it = this->m_InvasionSpawnInfo[type].begin(); it != this->m_InvasionSpawnInfo[type].end(); ++it)
        {
            // trimitem mesaj DOAR pentru entry-urile marcate ca boss
            if (it->Class == classId && it->Map == map && it->IsBoss == 1)
            {
                if (this->m_InvasionMessage[type].KillMsg[0] != 0)
                {
                    gNotice.GCNoticeSendToAll(0, 0, 0, 0, 0, 0,
                        this->m_InvasionMessage[type].KillMsg);
                }
                return;
            }
        }
    }
}

void CInvasionManager::ReloadInvasions()
{
    LogAdd(LOG_BLUE, "[InvasionManager] Reload requested...");

    this->Init();

    this->Load((char*)"..\\Data\\Event\\InvasionManager.dat");

    LogAdd(LOG_GREEN, "[InvasionManager] Reload completed.");
}

void CInvasionManager::StartInvasionManual(int type)
{
    if (type < 0 || type >= MAX_INVASION_TYPE)
    {
        LogAdd(LOG_BLACK, "[InvasionManager] Manual start failed: invalid type %d", type);
        return;
    }

    if (this->m_InvasionSpawnInfo[type].empty())
    {
        LogAdd(LOG_BLACK, "[InvasionManager] Manual start failed: no spawn data for type %d", type);
        return;
    }

    LogAdd(LOG_BLACK, "[InvasionManager] Manual start for invasion type %d", type);
    this->StartInvasion(type);
}
