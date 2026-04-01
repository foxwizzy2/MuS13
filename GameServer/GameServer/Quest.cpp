// Quest.cpp: implementation of the CQuest class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Quest.h"
#include "GameMain.h"
#include "MemScript.h"
#include "QuestObjective.h"
#include "QuestReward.h"
#include "Util.h"
#include "ForThTree.h"
CQuest gQuest;

static BYTE GetClientQuestState(BYTE questState)
{
	// Old client quest UI is unstable with state 3 (QUEST_CANCEL); expose only 0..2.
	return (questState > QUEST_FINISH) ? QUEST_NORMAL : questState;
}
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQuest::CQuest() // OK
{
}

CQuest::~CQuest() // OK
{

}

void CQuest::Load(char* path) // OK
{
	CMemScript* lpMemScript = new CMemScript;

	if(lpMemScript == 0)
	{
		ErrorMessageBox(MEM_SCRIPT_ALLOC_ERROR,path);
		return;
	}

	if(lpMemScript->SetBuffer(path) == 0)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
		delete lpMemScript;
		return;
	}

	m_QuestInfo.clear();

	try
	{
		while(true)
		{
			if(lpMemScript->GetToken() == TOKEN_END)
			{
				break;
			}

			if(strcmp("end",lpMemScript->GetString()) == 0)
			{
				break;
			}

			QUEST_INFO info;

			memset(&info,0,sizeof(info));

			info.Index = lpMemScript->GetNumber();

			info.StartType = lpMemScript->GetAsNumber();

			info.MonsterClass = lpMemScript->GetAsNumber();

			info.CurrentState = lpMemScript->GetAsNumber();

			info.RequireIndex = lpMemScript->GetAsNumber();

			info.RequireState = lpMemScript->GetAsNumber();

			info.RequireMinLevel = lpMemScript->GetAsNumber();

			info.RequireMaxLevel = lpMemScript->GetAsNumber();

			for(int n=0;n < MAX_CLASS;n++){info.RequireClass[n] = lpMemScript->GetAsNumber();}
			
			m_QuestInfo.add(info);
		}
	}
	catch(...)
	{
		ErrorMessageBox(lpMemScript->GetLastError());
	}

	delete lpMemScript;
}

QUEST_INFO* CQuest::GetInfoByIndex(LPOBJ lpObj,int QuestIndex) // OK
{
	for(int n=0;n < this->m_QuestInfo.size();n++)
	{
		const auto lpInfo = &m_QuestInfo[n];
		if(lpInfo->Index != QuestIndex)
		{
			continue;
		}

		if(this->CheckQuestRequisite(lpObj,lpInfo) == 0)
		{
			continue;
		}

		return lpInfo;
	}

	return 0;
}

bool CQuest::AddQuestList(LPOBJ lpObj,int QuestIndex,int QuestState) // OK
{
	if(QuestIndex < 0 || QuestIndex >= MAX_QUEST_LIST || QuestIndex >= m_QuestInfo.size())
	{
		return 0;
	}

	lpObj->Quest[QuestIndex].questState = QuestState;

	return 1;
}

BYTE CQuest::GetQuestList(LPOBJ lpObj,int QuestIndex) // OK
{
	if(QuestIndex < 0 || QuestIndex >= MAX_QUEST_LIST || QuestIndex >= m_QuestInfo.size())
	{
		LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][GetQuestList] invalid QuestIndex=%d max_list=%d loaded_quests=%d name=%s",
			QuestIndex, MAX_QUEST_LIST, (int)m_QuestInfo.size(), lpObj->Name);
		return 0;
	}

	const int startQuestByte = QuestIndex/4*4;
	if ((startQuestByte + 3) >= MAX_QUEST_LIST)
	{
		LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][GetQuestList] invalid packed range start=%d name=%s",
			startQuestByte, lpObj->Name);
		return 0;
	}

	const BYTE q0 = GetClientQuestState(lpObj->Quest[startQuestByte].questState);
	const BYTE q1 = GetClientQuestState(lpObj->Quest[startQuestByte + 1].questState);
	const BYTE q2 = GetClientQuestState(lpObj->Quest[startQuestByte + 2].questState);
	const BYTE q3 = GetClientQuestState(lpObj->Quest[startQuestByte + 3].questState);

	return q0 | (q1 << 2) | (q2 << 4) | (q3 << 6);
}

bool CQuest::CheckQuestRequisite(LPOBJ lpObj,QUEST_INFO* lpInfo) // OK
{
	if(this->CheckQuestListState(lpObj,lpInfo->Index,lpInfo->CurrentState) == 0)
	{
		return 0;
	}

	if(lpInfo->RequireIndex != -1 && this->CheckQuestListState(lpObj,lpInfo->RequireIndex,lpInfo->RequireState) == 0)
	{
		return 0;
	}

	if (lpInfo->RequireMinLevel != -1 && lpInfo->RequireMinLevel > (lpObj->Level + lpObj->MasterLevel))
	{
		return 0;
	}

	if(lpInfo->RequireMaxLevel != -1 && lpInfo->RequireMaxLevel < (lpObj->Level + lpObj->MasterLevel))
	{
		return 0;
	}

	if(lpInfo->RequireClass[lpObj->Class] == 0 || lpInfo->RequireClass[lpObj->Class] > (lpObj->ChangeUp+1))
	{
		return 0;
	}

	return 1;
}

bool CQuest::CheckQuestListState(LPOBJ lpObj,int QuestIndex,int QuestState) // OK
{
	if(QuestIndex < 0 || QuestIndex >= MAX_QUEST_LIST || QuestIndex >= m_QuestInfo.size())
	{
		return 0;
	}

	return lpObj->Quest[QuestIndex].questState == QuestState;
}

long CQuest::GetQuestRewardLevelUpPoint(LPOBJ lpObj) // OK
{
	int point = 0;

	for(int n=0;n < m_QuestInfo.size();n++)
	{
		if(this->CheckQuestListState(lpObj,n,QUEST_FINISH) != 0)
		{
			point += gQuestReward.GetQuestRewardPoint(lpObj,n);
		}
	}

	return point;
}

bool CQuest::NpcTalk(LPOBJ lpNpc,LPOBJ lpObj) // OK
{
	for(int n=0;n < m_QuestInfo.size();n++)
	{
		QUEST_INFO* lpInfo = &m_QuestInfo[n];

		if(lpInfo->StartType != 0)
		{
			continue;
		}

		if(lpInfo->MonsterClass != lpNpc->Class)
		{
			continue;
		}

		if(this->CheckQuestRequisite(lpObj,lpInfo) == 0)
		{
			continue;
		}

		lpObj->Interface.use = 1;
		lpObj->Interface.type = INTERFACE_QUEST;
		lpObj->Interface.state = 0;

		this->GCQuestStateSend(lpObj->Index,lpInfo->Index);

		this->GCQuestKillCountSend(lpObj->Index,lpInfo->Index);
		return 1;
	}

	return 0;
}

void CQuest::CGQuestInfoRecv(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][CGQuestInfoRecv] disconnected aIndex=%d", aIndex);
		return;
	}

	LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][CGQuestInfoRecv] aIndex=%d name=%s SendQuestInfo=%d LoadQuestKillCount=%d QuestKillCountIndex=%d",
		aIndex, lpObj->Name, lpObj->SendQuestInfo, lpObj->LoadQuestKillCount, lpObj->QuestKillCountIndex);

	this->GCQuestInfoSend(aIndex);
}

void CQuest::CGQuestStateRecv(PMSG_QUEST_STATE_RECV* lpMsg,int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][CGQuestStateRecv] disconnected aIndex=%d", aIndex);
		return;
	}

	LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][CGQuestStateRecv] aIndex=%d name=%s QuestIndex=%d QuestState=%d",
		aIndex, lpObj->Name, lpMsg->QuestIndex, lpMsg->QuestState);

	QUEST_INFO* lpInfo = this->GetInfoByIndex(lpObj,lpMsg->QuestIndex);

	if(lpInfo == 0)
	{
		LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][CGQuestStateRecv] GetInfoByIndex returned null for QuestIndex=%d (aIndex=%d name=%s)",
			lpMsg->QuestIndex, aIndex, lpObj->Name);
		return;
	}

	if(gQuestObjective.CheckQuestObjective(lpObj,lpInfo->Index) == 0)
	{
		this->GCQuestResultSend(aIndex,lpInfo->Index,0xFF,this->GetQuestList(lpObj,lpInfo->Index));
		return;
	}

	if(lpInfo->CurrentState == QUEST_NORMAL)
	{
		gQuestObjective.RemoveQuestObjective(lpObj,lpInfo->Index);
		gQuestReward.InsertQuestReward(lpObj,lpInfo->Index);
		this->AddQuestList(lpObj,lpInfo->Index,QUEST_ACCEPT);
		gQuestObjective.InitQuestObjectiveKillCount(lpObj,lpInfo->Index);
		this->GCQuestResultSend(aIndex,lpInfo->Index,0x00,this->GetQuestList(lpObj,lpInfo->Index));
		return;
	}

	if(lpInfo->CurrentState == QUEST_ACCEPT)
	{
		gQuestObjective.RemoveQuestObjective(lpObj,lpInfo->Index);
		gQuestReward.InsertQuestReward(lpObj,lpInfo->Index);
		this->AddQuestList(lpObj,lpInfo->Index,QUEST_FINISH);
		gQuestObjective.InitQuestObjectiveKillCount(lpObj,lpInfo->Index);
		this->GCQuestResultSend(aIndex,lpInfo->Index,0x00,this->GetQuestList(lpObj,lpInfo->Index));
		return;
	}

	if(lpInfo->CurrentState == QUEST_FINISH)
	{
		gQuestObjective.RemoveQuestObjective(lpObj,lpInfo->Index);
		gQuestReward.InsertQuestReward(lpObj,lpInfo->Index);
		this->AddQuestList(lpObj,lpInfo->Index,QUEST_FINISH);
		gQuestObjective.InitQuestObjectiveKillCount(lpObj,lpInfo->Index);
		this->GCQuestResultSend(aIndex,lpInfo->Index,0xFF,this->GetQuestList(lpObj,lpInfo->Index));
		return;
	}

	if(lpInfo->CurrentState == QUEST_CANCEL)
	{
		gQuestObjective.RemoveQuestObjective(lpObj,lpInfo->Index);
		gQuestReward.InsertQuestReward(lpObj,lpInfo->Index);
		this->AddQuestList(lpObj,lpInfo->Index,QUEST_ACCEPT);
		gQuestObjective.InitQuestObjectiveKillCount(lpObj,lpInfo->Index);
		this->GCQuestResultSend(aIndex,lpInfo->Index,0x00,this->GetQuestList(lpObj,lpInfo->Index));
		return;
	}
}

void CQuest::CGQuestNpcWarewolfRecv(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(lpObj->Interface.use != 0)
	{
		return;
	}

	//if(lpObj->X < 57 || lpObj->X > 67 || lpObj->Y < 234 || lpObj->Y > 244)
	//{
	//	return;
	//}

	if(this->CheckQuestListState(lpObj,5,QUEST_ACCEPT) != 0 || this->CheckQuestListState(lpObj,5,QUEST_FINISH) != 0)
	{
		gObjMoveGate(aIndex,256);
	}
}

void CQuest::CGQuestNpcKeeperRecv(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(gObjIsConnectedGP(aIndex) == 0)
	{
		return;
	}

	if(lpObj->Interface.use != 0)
	{
		return;
	}

	if(lpObj->X < 114 || lpObj->X > 124 || lpObj->Y < 163 || lpObj->Y > 173)
	{
		return;
	}

	if(this->CheckQuestListState(lpObj,6,QUEST_ACCEPT) != 0 || this->CheckQuestListState(lpObj,6,QUEST_FINISH) != 0)
	{
		gObjMoveGate(aIndex,257);
	}
}

void CQuest::GCQuestInfoSend(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->SendQuestInfo != 0)
	{
		LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][GCQuestInfoSend] skipped (already sent) aIndex=%d name=%s",
			aIndex, lpObj->Name);
		return;
	}

	PMSG_QUEST_INFO_SEND pMsg;

	pMsg.header.set(0xA0,sizeof(pMsg));

	// QuestInfo stores packed quest states (4 quests / byte), so count must be the packed-byte count.
	// Sending 200 here can make clients iterate beyond QuestInfo[50].
	pMsg.count = sizeof(pMsg.QuestInfo);
	
	for (int i=0; i<50; i++)
	{
		const auto questNumber = i*4;
		const BYTE q0 = GetClientQuestState(lpObj->Quest[questNumber].questState);
		const BYTE q1 = GetClientQuestState(lpObj->Quest[questNumber + 1].questState);
		const BYTE q2 = GetClientQuestState(lpObj->Quest[questNumber + 2].questState);
		const BYTE q3 = GetClientQuestState(lpObj->Quest[questNumber + 3].questState);
		pMsg.QuestInfo[i] = q0 | (q1 << 2) | (q2 << 4) | (q3 << 6);
	}

	LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][GCQuestInfoSend] aIndex=%d name=%s packet_size=%d count=%d first_bytes=%u,%u,%u,%u",
		aIndex, lpObj->Name, pMsg.header.size, pMsg.count, pMsg.QuestInfo[0], pMsg.QuestInfo[1], pMsg.QuestInfo[2], pMsg.QuestInfo[3]);

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	lpObj->SendQuestInfo = 1;
}

void CQuest::GCQuestStateSend(int aIndex,int QuestIndex) // OK
{
	this->GCQuestInfoSend(aIndex);

	PMSG_QUEST_STATE_SEND pMsg;

	pMsg.header.set(0xA1,sizeof(pMsg));

	pMsg.QuestIndex = QuestIndex;

	pMsg.QuestState = this->GetQuestList(&gObj[aIndex],QuestIndex);

	LogAdd(eLogColor::LOG_DEBUG, "[QuestDebug][GCQuestStateSend] aIndex=%d name=%s QuestIndex=%d QuestStatePacked=%d",
		aIndex, gObj[aIndex].Name, pMsg.QuestIndex, pMsg.QuestState);

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CQuest::GCQuestResultSend(int aIndex,int QuestIndex,int QuestResult,int QuestState) // OK
{
	PMSG_QUEST_RESULT_SEND pMsg;

	pMsg.header.set(0xA2,sizeof(pMsg));

	pMsg.QuestIndex = QuestIndex;

	pMsg.QuestResult = QuestResult;

	pMsg.QuestState = QuestState;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CQuest::GCQuestRewardSend(int aIndex,int QuestReward,int QuestAmount) // OK
{
	this->GCQuestInfoSend(aIndex);

	PMSG_QUEST_REWARD_SEND pMsg;

	pMsg.header.set(0xA3,sizeof(pMsg));

	pMsg.index[0] = SET_NUMBERHB(aIndex);

	pMsg.index[1] = SET_NUMBERLB(aIndex);

	pMsg.QuestReward = QuestReward;

	pMsg.QuestAmount = QuestAmount;

	pMsg.ViewPoint = gObj[aIndex].LevelUpPoint;

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);

	MsgSendV2(&gObj[aIndex],(BYTE*)&pMsg,pMsg.header.size);
}

void CQuest::GCQuestKillCountSend(int aIndex,int QuestIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->QuestKillCountIndex != QuestIndex)
	{
		return;
	}

	if(this->CheckQuestListState(lpObj,QuestIndex,QUEST_ACCEPT) == 0)
	{
		return;
	}

	PMSG_QUEST_KILL_COUNT_SEND pMsg;

	pMsg.header.set(0xA4,sizeof(pMsg));

	pMsg.QuestResult = 1;

	pMsg.QuestIndex = QuestIndex;

	memcpy(pMsg.QuestKillCount,lpObj->QuestKillCount,sizeof(pMsg.QuestKillCount));

	DataSend(aIndex,(BYTE*)&pMsg,pMsg.header.size);
}

void CQuest::DGQuestKillCountRecv(SDHP_QUEST_KILL_COUNT_RECV* lpMsg) // OK
{
	if(gObjIsAccountValid(lpMsg->index,lpMsg->account) == 0)
	{
		LogAdd(LOG_RED,"[DGQuestKillCountRecv] Invalid Account [%d](%s)",lpMsg->index,lpMsg->account);
		CloseClient(lpMsg->index);
		return;
	}

	LPOBJ lpObj = &gObj[lpMsg->index];

	if(lpObj->LoadQuestKillCount != 0)
	{
		return;
	}

	lpObj->LoadQuestKillCount = 1;

	lpObj->QuestKillCountIndex = lpMsg->QuestIndex;

	for(int n=0;n < MAX_QUEST_KILL_COUNT;n++)
	{
		lpObj->QuestKillCount[n].MonsterClass = lpMsg->MonsterClass[n];
		lpObj->QuestKillCount[n].KillCount = lpMsg->KillCount[n];
	}
}

void CQuest::GDQuestKillCountSend(int aIndex) // OK
{
	if(gObjIsAccountValid(aIndex,gObj[aIndex].Account) == 0)
	{
		return;
	}

	if(gObj[aIndex].LoadQuestKillCount != 0)
	{
		return;
	}

	SDHP_QUEST_KILL_COUNT_SEND pMsg;

	pMsg.header.set(0x0C,0x00,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,gObj[aIndex].Account,sizeof(pMsg.account));

	memcpy(pMsg.name,gObj[aIndex].Name,sizeof(pMsg.name));

	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}

void CQuest::GDQuestKillCountSaveSend(int aIndex) // OK
{
	LPOBJ lpObj = &gObj[aIndex];

	if(lpObj->LoadQuestKillCount == 0)
	{
		return;
	}

	SDHP_QUEST_KILL_COUNT_SAVE_SEND pMsg;

	pMsg.header.set(0x0C,0x30,sizeof(pMsg));

	pMsg.index = aIndex;

	memcpy(pMsg.account,lpObj->Account,sizeof(pMsg.account));

	memcpy(pMsg.name,lpObj->Name,sizeof(pMsg.name));
	if (lpObj->QuestKillCountIndex >= 7 && lpObj->QuestKillCountIndex <= 9)
	{
		pMsg.QuestIndex = lpObj->QuestKillCountIndex;

		for (int n = 0; n < MAX_QUEST_KILL_COUNT; n++)
		{
			pMsg.MonsterClass[n] = lpObj->QuestKillCount[n].MonsterClass;
			pMsg.KillCount[n] = 0;
		}
	}
	else
	{
		pMsg.QuestIndex = lpObj->QuestKillCountIndex;

		for (int n = 0; n < MAX_QUEST_KILL_COUNT; n++)
		{
			pMsg.MonsterClass[n] = lpObj->QuestKillCount[n].MonsterClass;
			pMsg.KillCount[n] = lpObj->QuestKillCount[n].KillCount;
		}
	}
	
	gDataServerConnection.DataSend((BYTE*)&pMsg,pMsg.header.size);
}
