#include "Stdafx.h"
#include "Protocol.h"
#include "PacketManager.h"
#include "Reconnect.h"
#include "Util.h"
#include "Console.h"
#include "CustomFunctions.h"
#include "CastleSiege.h"
#include "..\\..\\Util\\CCRC32.H"
#include <Interface.h>
#include "PrintPlayer.h"
#include "HackCheck.h"
#include "Offset.h"
#include "EventSchedule.h"
#include "ScreenShot.h"
#include "Serial.h"
#include "Oficina.h"
#include "EmojiSystem.h"
//custom

#include <windows.h>
#include <intrin.h>
#include <cstring>

// runtime VA from "static" 0x00400000 image base
static DWORD GetRuntimeAddr(DWORD staticAddr)
{
	DWORD base = (DWORD)GetModuleHandleA(NULL);
	return base + (staticAddr - 0x00400000);
}

// ------------------------------
// 1) AddChatInfo (SAFE call-site hook, fara trampoline)
// ------------------------------
typedef void(__thiscall* tAddChatInfo)(void* This, int type, void* name, void* msg);
static tAddChatInfo g_AddChatInfo_Orig = 0;
static bool g_AddChatInfoHooked = false;
static bool TryReadWideText(void* p, wchar_t* outW, int outWCount)
{
	if (!p || !outW || outWCount <= 1) return false;
	outW[0] = 0;

	__try
	{
		// 1) încercăm ca pointer la buffer direct
		wchar_t* direct = (wchar_t*)p;
		if (direct[0] != 0 && direct[0] != (wchar_t)0xCCCC && direct[0] != (wchar_t)0xFEEE)
		{
			int n = 0;
			while (n < outWCount - 1 && direct[n] != 0) { outW[n] = direct[n]; n++; }
			outW[n] = 0;
			if (n > 0) return true;
		}

		// 2) încercăm ca “struct” (primul DWORD = wchar_t*)
		wchar_t* ptr = *(wchar_t**)p;
		if (ptr)
		{
			int n = 0;
			while (n < outWCount - 1 && ptr[n] != 0) { outW[n] = ptr[n]; n++; }
			outW[n] = 0;
			if (n > 0) return true;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		outW[0] = 0;
		return false;
	}

	return false;
}

static wchar_t* GetChatMsgW(void* msg)
{
	if (!msg) return nullptr;

	__try
	{
		// cele mai comune layout-uri
		wchar_t* w1 = *(wchar_t**)((BYTE*)msg + 4);
		if (w1 && w1[0]) return w1;

		wchar_t* w2 = *(wchar_t**)((BYTE*)msg + 8);
		if (w2 && w2[0]) return w2;

		wchar_t* w3 = *(wchar_t**)msg;
		if (w3 && w3[0]) return w3;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}

	return nullptr;
}

static bool LooksLikeUtf16(const wchar_t* w)
{
	// heuristic: primele 8 caractere sa fie printable si sa aiba terminator rezonabil
	if (!w) return false;
	int good = 0;
	for (int i = 0; i < 8; i++)
	{
		wchar_t c = w[i];
		if (c == 0) break;
		if (c >= 0x20 && c < 0x7F) good++;
		else if (c >= 0x00A0 && c < 0xD800) good++; // unicode basic
		else return false;
	}
	return (good >= 2);
}

static void ToUtf8FromUtf16(const wchar_t* w, char* outA, int outASize)
{
	if (!outA || outASize <= 1) return;
	outA[0] = 0;
	if (!w) return;
	WideCharToMultiByte(CP_UTF8, 0, w, -1, outA, outASize, 0, 0);
}

static bool IsValidUtf8(const char* inA)
{
	if (!inA || !inA[0]) return false;
	int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, inA, -1, nullptr, 0);
	return (wideLen > 0);
}

static bool ToUtf8FromMultiByte(UINT codepage, DWORD flags, const char* inA, char* outA, int outASize)
{
	if (!inA || !outA || outASize <= 1) return false;
	outA[0] = 0;

	wchar_t wideBuf[512];
	int wideLen = MultiByteToWideChar(codepage, flags, inA, -1, wideBuf, sizeof(wideBuf) / sizeof(wideBuf[0]));
	if (wideLen <= 0) return false;

	int outLen = WideCharToMultiByte(CP_UTF8, 0, wideBuf, -1, outA, outASize, 0, 0);
	return (outLen > 0);
}

static bool ExtractMsgTextA(void* msg, char* outA, int outSize)
{
	if (!msg || !outA || outSize <= 1) return false;
	outA[0] = 0;

	__try
	{
		char* direct = (char*)msg;
		if (direct[0])
		{
			bool hasTerminator = false;
			bool printable = true;
			for (int i = 0; i < outSize - 1; ++i)
			{
				unsigned char c = (unsigned char)direct[i];
				if (c == 0) { hasTerminator = true; break; }
				if (c < 0x20) { printable = false; break; }
			}
			if (hasTerminator && printable)
			{
				strncpy(outA, direct, outSize - 1);
				outA[outSize - 1] = 0;
				return true;
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		outA[0] = 0;
		return false;
	}

	__try
	{
		char* a1 = *(char**)((BYTE*)msg + 4);
		if (a1 && a1[0])
		{
			strncpy(outA, a1, outSize - 1);
			outA[outSize - 1] = 0;
			return true;
		}

		char* a2 = *(char**)((BYTE*)msg + 8);
		if (a2 && a2[0])
		{
			strncpy(outA, a2, outSize - 1);
			outA[outSize - 1] = 0;
			return true;
		}

		char* a3 = *(char**)msg;
		if (a3 && a3[0])
		{
			strncpy(outA, a3, outSize - 1);
			outA[outSize - 1] = 0;
			return true;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		outA[0] = 0;
		return false;
	}

	return false;
}

static bool ExtractNameText(void* p, char* outA, int outSize)
{
	if (!p || !outA || outSize <= 1) return false;
	outA[0] = 0;

	__try
	{
		char* a = (char*)((BYTE*)p + 4);
		if (!a || !a[0]) return false;
		strncpy(outA, a, outSize - 1);
		outA[outSize - 1] = 0;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		outA[0] = 0;
		return false;
	}
}

// in HookAddChatInfo, inlocuieste log-ul tau cu asta:
static void __fastcall HookAddChatInfo(void* This, void*, int type, void* name, void* msg)
{
	char nameA[64] = { 0 };
	char msgUtf8[512] = { 0 };
	char msgA[512] = { 0 };

	ExtractNameText(name, nameA, sizeof(nameA));

	if (type == 8)
	{
		wchar_t msgWBuf[512] = { 0 };
		if (TryReadWideText(msg, msgWBuf, sizeof(msgWBuf) / sizeof(msgWBuf[0])))
		{
			ToUtf8FromUtf16(msgWBuf, msgUtf8, sizeof(msgUtf8));
		}

		bool haveMsgA = ExtractMsgTextA(msg, msgA, sizeof(msgA));
		bool wroteMsg = false;

		if (msgUtf8[0] != '\0')
		{
			wroteMsg = true;
		}

		if (!wroteMsg && haveMsgA && IsValidUtf8(msgA))
		{
			strncpy(msgUtf8, msgA, sizeof(msgUtf8) - 1);
			msgUtf8[sizeof(msgUtf8) - 1] = '\0';
			wroteMsg = true;
		}

//		bool haveMsgA = ExtractMsgTextA(msg, msgA, sizeof(msgA));
//		bool wroteMsg = false;

		if (haveMsgA && IsValidUtf8(msgA))
		{
			strncpy(msgUtf8, msgA, sizeof(msgUtf8) - 1);
			msgUtf8[sizeof(msgUtf8) - 1] = '\0';
			wroteMsg = true;
		}

		if (!wroteMsg)
		{
			wchar_t* msgW = GetChatMsgW(msg);
			if (msgW && LooksLikeUtf16(msgW))
			{
				ToUtf8FromUtf16(msgW, msgUtf8, sizeof(msgUtf8));
				wroteMsg = msgUtf8[0] != '\0';
			}
		}

		if (!wroteMsg && haveMsgA)
		{
			if (!ToUtf8FromMultiByte(CP_ACP, 0, msgA, msgUtf8, sizeof(msgUtf8)))
			{
				strncpy(msgUtf8, msgA, sizeof(msgUtf8) - 1);
				msgUtf8[sizeof(msgUtf8) - 1] = '\0';
			}
		}

		Log.ConsoleOutPut(
			1, c_Green, t_Default,
			"[CHAT OK] name='%s' msg='%s'",
			nameA, msgUtf8
		);
	}

	// FOARTE IMPORTANT
	g_AddChatInfo_Orig(This, type, name, msg);
}

// patch E8 rel32 at callAddr -> newFunc
static bool PatchCallE8(DWORD callAddr, void* newFunc)
{
	BYTE* p = (BYTE*)callAddr;
	if (p[0] != 0xE8) return false;

	DWORD old;
	if (!VirtualProtect((LPVOID)callAddr, 5, PAGE_EXECUTE_READWRITE, &old))
		return false;

	DWORD rel = (DWORD)newFunc - (callAddr + 5);
	p[0] = 0xE8;
	*(DWORD*)(p + 1) = rel;

	VirtualProtect((LPVOID)callAddr, 5, old, &old);
	return true;
}

static void InitAddChatInfoHook_Safe()
{
	if (g_AddChatInfoHooked) return;

	// original function (runtime)
	g_AddChatInfo_Orig = (tAddChatInfo)GetRuntimeAddr(0x00B765CF);

	// call-site-ul pe care l-ai identificat din caller=0x00B77074 => call=0x00B7706F
	DWORD callSite = GetRuntimeAddr(0x00B7706F);

	if (PatchCallE8(callSite, (void*)&HookAddChatInfo))
	{
		g_AddChatInfoHooked = true;
		Log.ConsoleOutPut(1, c_Blue, t_Default, "[Emoji] AddChatInfo CALL patched at %p", (void*)callSite);
	}
	else
	{
		Log.ConsoleOutPut(1, c_Red, t_Default, "[Emoji] AddChatInfo CALL patch FAILED at %p", (void*)callSite);
	}
}

// ------------------------------
// 2) DrawInterfaceText (SAFE call-site scan + late hook)
// ------------------------------
typedef void(__stdcall* tDrawInterfaceText)(char* Text, int X, int Y, int Color, int Arg5, int Align, float Width, int Style);

static tDrawInterfaceText g_DrawInterfaceText_Orig = 0;
static bool g_DrawInterfaceHooked = false;
static bool g_EmojiReady = false;
static bool g_InEmojiDraw = false;
static void __stdcall HookDrawInterfaceText(char* Text, int X, int Y, int Color, int Arg5, int Align, float Width, int Style)
{
	if (Text && Text[0])
	{
		if (g_EmojiReady && !g_InEmojiDraw)
		{
			g_InEmojiDraw = true;
			char textCopy[512];
			strncpy(textCopy, Text, sizeof(textCopy) - 1);
			textCopy[sizeof(textCopy) - 1] = '\0';

			SIZE size = { (LONG)Width, 0 };
			myheart::MUEmoji::ChatEmojiProcess(X, Y, textCopy, Color, Arg5, Align, &size);

			if (g_DrawInterfaceText_Orig)
				g_DrawInterfaceText_Orig(textCopy, X, Y, Color, Arg5, Align, Width, Style);
			g_InEmojiDraw = false;
			return;
		}
		SIZE size = { (LONG)Width, 0 };
		myheart::MUEmoji::ChatEmojiProcess(X, Y, Text, Color, Arg5, Align, &size);
	}

	if (g_DrawInterfaceText_Orig)
		g_DrawInterfaceText_Orig(Text, X, Y, Color, Arg5, Align, Width, Style);
}

static void HookDrawInterfaceTextCalls(int maxPatch)
{
	if (g_DrawInterfaceHooked) return;

	HMODULE hMod = GetModuleHandleA(NULL);
	if (!hMod) return;

	// original function runtime
	DWORD targetDraw = GetRuntimeAddr(0x0088E6E9);
	g_DrawInterfaceText_Orig = (tDrawInterfaceText)targetDraw;

	Log.ConsoleOutPut(1, c_Blue, t_Default, "[Emoji] DrawInterfaceText runtime=%p", (void*)targetDraw);

	BYTE* base = (BYTE*)hMod;
	IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
	IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
	IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

	DWORD patched = 0;

	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
	{
		if (memcmp(sec[i].Name, ".text", 5) != 0) continue;

		BYTE* start = base + sec[i].VirtualAddress;
		DWORD size = sec[i].Misc.VirtualSize;

		for (DWORD off = 0; off + 6 < size; off++)
		{
			BYTE* p = start + off;

			// (1) CALL rel32: E8 xx xx xx xx
			if (p[0] == 0xE8)
			{
				DWORD rel = *(DWORD*)(p + 1);
				DWORD dest = (DWORD)(p + 5 + rel);

				if (dest == targetDraw)
				{
					if (PatchCallE8((DWORD)p, (void*)&HookDrawInterfaceText))
					{
						patched++;
						Log.ConsoleOutPut(1, c_Blue, t_Default, "[Emoji] Patched E8 call at %p", p);
						if ((int)patched >= maxPatch) { g_DrawInterfaceHooked = true; return; }
					}
				}
				continue;
			}

			// (2) CALL [imm32]: FF 15 xx xx xx xx
			if (p[0] == 0xFF && p[1] == 0x15)
			{
				DWORD ptrAddr = *(DWORD*)(p + 2);

				__try
				{
					DWORD dest = *(DWORD*)ptrAddr;
					if (dest == targetDraw)
					{
						DWORD old;
						if (VirtualProtect((LPVOID)ptrAddr, 4, PAGE_EXECUTE_READWRITE, &old))
						{
							*(DWORD*)ptrAddr = (DWORD)&HookDrawInterfaceText;
							VirtualProtect((LPVOID)ptrAddr, 4, old, &old);

							patched++;
							Log.ConsoleOutPut(1, c_Blue, t_Default, "[Emoji] Patched FF15 ptr at %p (ptr=%p)", p, (void*)ptrAddr);
							if ((int)patched >= maxPatch) { g_DrawInterfaceHooked = true; return; }
						}
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
				}
				continue;
			}
		}
	}

	g_DrawInterfaceHooked = (patched > 0);
	Log.ConsoleOutPut(1, g_DrawInterfaceHooked ? c_Blue : c_Red, t_Default,
		"[Emoji] DrawInterfaceText patched=%u (hooked=%d)", patched, g_DrawInterfaceHooked ? 1 : 0);
}

// Late hook after entering character/world (o singura data)
static bool g_LateHookDone = false;
static void TryLateHook_DrawText()
{
	if (g_LateHookDone) return;
	if (*(DWORD*)MAIN_CHARACTER_STRUCT == 0) return;
	g_LateHookDone = true;

	Log.ConsoleOutPut(1, c_Blue, t_Default, "[Emoji] Late hook: scanning DrawInterfaceText calls...");
	HookDrawInterfaceTextCalls(300);
	g_EmojiReady = g_DrawInterfaceHooked;
}

void EmojiTryInstallDrawHook()
{
	TryLateHook_DrawText();
}

//custom end
bool HardwareIdSent = false;

CrownSwitchData g_CrownSwitchData[3];

void InitProtocolCoreEx()
{
	Log.ConsoleOutPut(1, c_Red, t_Default, "[Emoji] InitProtocolCoreEx called");
	SetCompleteHook(0xE9, 0x00BE443A, &SendPacket);
	SetCompleteHook(0xE9, 0x00C48E76, &ParsePacket);

	//custom
	//g_DrawInterfaceText_Orig = (tDrawInterfaceText)GetRuntimeAddr(0x0088E6E9);
	//Log.ConsoleOutPut(1, c_Blue, t_Default, "[Emoji] InitProtocolCoreEx called");
	InitAddChatInfoHook_Safe();
	//custom end
}

BOOL ProtocolCoreEx(BYTE head, BYTE* lpMsg, int len, int Encrypt)
{
	static bool first = true;
	if (first)
	{
		first = false;
		Log.ConsoleOutPut(1, c_Red, t_Default, "ProtocolCore = %p", ProtocolCore);
	}

	int logProtocol = GetPrivateProfileInt("CONSOLE", "LogProtocol", 0, ".\\Config.ini");
	if (logProtocol == 1) {
		Log.ConsoleOutPut(1, c_Red, t_Default, "[RECVClient]: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x"
		, lpMsg[0], lpMsg[1], lpMsg[2], lpMsg[3], lpMsg[4], lpMsg[5], lpMsg[6]
		, lpMsg[7], lpMsg[8], lpMsg[9], lpMsg[10], lpMsg[11], lpMsg[12], lpMsg[13]
		, lpMsg[14], lpMsg[15], lpMsg[16], lpMsg[17], lpMsg[18], lpMsg[19]
			, lpMsg[20], lpMsg[21], lpMsg[22], lpMsg[23], lpMsg[24], lpMsg[25], lpMsg[26]
			, lpMsg[27], lpMsg[28], lpMsg[29]);
	}

	if (lpMsg[0] == 0xC1 && (head == 0xF3 || head == 0xF8 || head == 0x1C || head == 0xD6 || head == 0x15 || head == 0x10))
	{
		avoidByProtocol = GetTickCount64();
	}

	switch (head)
	{
		//custom
		// cel mai sigur trigger: când intri pe caracter (F3:03 CharacterInfo) sau când începe world traffic
		//custom end

	case 0x71:
	{
		PMSG_PING* ping = (PMSG_PING*)lpMsg;
		gInterface.ping = ping->ping;
		//Log.ConsoleOutPut(1, c_Red, t_Default, "[Ping]: %d", gInterface.ping);
	} break;

	case 0xB1:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
		case 0x00:
			GCMapServerMoveRecv((PMSG_MAP_SERVER_MOVE_RECV*)lpMsg);
			break;
		case 0x01:
			GCMapServerMoveAuthRecv((PMSG_MAP_SERVER_MOVE_AUTH_RECV*)lpMsg);
			break;
		}
	break;

	case 0xB2:
	{
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
			case 0x14:
			{
				CASTLE_SIEGE_SWITCH_STATE* lpData = (CASTLE_SIEGE_SWITCH_STATE*)lpMsg;
				g_CastleSiege.SetSwitchData(lpData->state, lpData->playerH * 512 + lpData->playerL, lpData->switchH * 512 + lpData->switchL);
				return 1;
			} break;

			case 0x15:
			{
				CROWN_ACCESS_STATE* lpData = (CROWN_ACCESS_STATE*)lpMsg;
				g_CastleSiege.SetCrownMasterData(lpData->state, lpData->accumulated_time);
				return 1;
			} break;

			case 0x16:
			{
				CROWN_STATE* lpData = (CROWN_STATE*)lpMsg;
				g_CastleSiege.SetCrownData(lpData->state);
				return 1;
			} break;

			case 0x18: //green message when click crown and register
			{
				return 1;
			} break;

			case 0x20:
			{
				GCSetCrownSwitchData(lpMsg);
				return 1;
			} break;
		}
	} 
	break;

	case 0xD8:
	{
		HandleElementalDamage(lpMsg);
	} break;

	case 0xDF:
	{
		HandleDamage(lpMsg);
	} break;

	case 0x26:
	{
		HandleHP(lpMsg);
	} break;

	case 0xF1:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
		case 0x00:
			GCConnectClientRecv((PMSG_CONNECT_CLIENT_RECV*)lpMsg);
			break;
		case 0x01:
			GCConnectAccountRecv((PMSG_CONNECT_ACCOUNT_RECV*)lpMsg);
			break;
		case 0x02:
			GCCloseClientRecv((PMSG_CLOSE_CLIENT_RECV*)lpMsg);
			break;
		}
	break;

	case 0xF3:
		switch (((lpMsg[0] == 0xC1) ? lpMsg[3] : lpMsg[4]))
		{
		case 0x00:
			GCCharacterListRecv((PMSG_CHARACTER_LIST_RECV*)lpMsg);
			break;
		case 0x03:
			GCCharacterInfoRecv((PMSG_CHARACTER_INFO_RECV*)lpMsg);
			break;
		case 0x04:
			HandleRespawn(lpMsg);
			break;
		case 0x05:
			GCCharacterLevelUpRecv((PMSG_LEVEL_UP_RECV*)lpMsg);
			break;
		case 0x06:
			HandleLevelUpPoint(lpMsg);
			break;
		case 0x51:
			GCCharacterMasterLevelUpRecv((PMSG_MASTER_LEVEL_UP_RECV*)lpMsg);
			break;
		case 0xE0:
			GCNewCharacterInfoRecv((PMSG_NEW_CHARACTER_INFO_RECV*)lpMsg);
			return 1;
		case 0xE1:
			GCNewMessageRecv((PMSG_NEW_MESSAGE_RECV*)lpMsg);
			return 1;
		case 0xE2:
			GCMessagePopupRecv((PMSG_MESSAGE_POPUP_RECV*)lpMsg);
			return 1;
		case 0xE3:
			HandleBossRanking(lpMsg);
			break;
		case 0xE5:
		{
			PMSG_PIN_AUTH* pinMsg = (PMSG_PIN_AUTH*)lpMsg;
			//Log.ConsoleOutPut(1, c_Red, t_Default, "PIN: %s", pinMsg->pin);
			WritePrivateProfileString("PIN", UserAccount, pinMsg->pin, ".\\Config.ini");
		}
			break;
		case 0xE6:
			gEventSchedule.ProcessScheduleData((PMSG_SCHEDULE_EVENTS*)lpMsg);
			break;
		case 0xE7:
			gEventSchedule.ProcessScheduleData2((PMSG_SCHEDULE_INVASIONS*)lpMsg);
			break;
		case 0xE8:
			gSS.PrintScreen();
			break;
		case 0xE9:
			gOficina.ReceiveOficinaData((PMSG_OFICINA_OPEN*)lpMsg);
			break;
		case 0xEA:
			gOficina.CloseAndResetAll();
			break;
		}
	break;
	}

	return ProtocolCore(head,lpMsg,len,Encrypt);
}

void GCMapServerMoveRecv(PMSG_MAP_SERVER_MOVE_RECV* lpMsg) // OK
{
	ReconnectOnMapServerMove(lpMsg->IpAddress, lpMsg->ServerPort);
}

void GCMapServerMoveAuthRecv(PMSG_MAP_SERVER_MOVE_AUTH_RECV* lpMsg) // OK
{
	ReconnectOnMapServerMoveAuth(lpMsg->result);
}

void GCConnectAccountRecv(PMSG_CONNECT_ACCOUNT_RECV* lpMsg) // OK
{
	ReconnectOnConnectAccount(lpMsg->result);
}

void GCCloseClientRecv(PMSG_CLOSE_CLIENT_RECV* lpMsg) // OK
{
	ReconnectOnCloseClient(lpMsg->result);
}

void GCCharacterListRecv(PMSG_CHARACTER_LIST_RECV* lpMsg) // OK
{
	ReconnectOnCharacterList();
}

void GCCharacterLevelUpRecv(PMSG_LEVEL_UP_RECV* lpMsg)
{
	LevelUpPoints = lpMsg->ViewPoint;

	char* msg = (char*)malloc(sizeof(char[64]));

	wsprintf(msg, "L:%d | M:%d | R:%d | G:%d", lpMsg->Level, lpMsg->MasterLevel, Resets, GResets);

	LevelText = msg;
}

void GCCharacterMasterLevelUpRecv(PMSG_MASTER_LEVEL_UP_RECV* lpMsg)
{
	char* msg = (char*)malloc(sizeof(char[64]));

	wsprintf(msg, "L:%d | M:%d | R:%d | G:%d", lpMsg->Level, lpMsg->MasterLevel, Resets, GResets);

	LevelText = msg;
}

void GCCharacterInfoRecv(PMSG_CHARACTER_INFO_RECV* lpMsg) // OK
{
	ReconnectOnCharacterInfo();

	*(short*)(*(DWORD*)(MAIN_VIEWPORT_STRUCT)+0x3E) = 0;
	*(BYTE*)(*(DWORD*)(MAIN_VIEWPORT_STRUCT)+0x408) = 0;
	
	Resets = lpMsg->ViewReset;
	GResets = lpMsg->ViewMasterReset;

	ViewHP = lpMsg->ViewCurHP;
	ViewMaxHP = lpMsg->ViewMaxHP;
	LevelUpPoints = lpMsg->ViewPoint;

	//Log.ConsoleOutPut(1, c_Blue, t_Default, "GCCharacterInfoRecv Life %d/%d", ViewHP, ViewMaxHP);
	//Log.ConsoleOutPut(1, c_Blue, t_Default, "Account: %s", UserAccount);

	char pin[8];
	GetPrivateProfileString("PIN", UserAccount, "-", pin, sizeof(pin), ".\\Config.ini");

	if (pin[0] != '-')
	{
		Log.ConsoleOutPut(1, c_Blue, t_Default, "Account: %s | PIN: %s", UserAccount,pin);
		PMSG_PIN_AUTH pinSend;
		pinSend.h.set(0xF3, 0xE5, sizeof(pinSend));
		strcpy_s(pinSend.pin, 8, pin);
		pinSend.pin[8] = '\0';
		DataSend((LPBYTE)&pinSend, pinSend.h.size);
	}
	else
	{
		Log.ConsoleOutPut(1, c_Blue, t_Default, "PIN does not exist for %s", UserAccount);
	}	

	char * msg = (char*)malloc(sizeof(char[64]));

	wsprintf(msg, "L:%d | M:%d | R:%d | G:%d", lpMsg->ViewLevel, lpMsg->ViewMasterLevel, lpMsg->ViewReset, lpMsg->ViewMasterReset);

	LevelText = msg;

	//if (HardwareIdSent == false)
	//{
	//	HardwareIdSent = true;

		HARDWARE_ID_SEND pResult;
		pResult.header.set(0x79, sizeof(pResult));
		ZeroMemory(pResult.HarwareId, sizeof(pResult.HarwareId));
		char* hwId = GetHardwareId();
		strcpy_s(pResult.HarwareId, 45, hwId);
		pResult.HarwareId[44] = '\0';

		DataSend((LPBYTE)&pResult, pResult.header.size);

		//Log.ConsoleOutPut(1, c_Blue, t_Default, "HarwareId %s", pResult.HarwareId);
		
		CCRC32 CRC32;
		DWORD PluginCRC32;

		if (CRC32.FileCRC("Data\\Player\\player.bmd", &PluginCRC32, 1024) == 0)
		{
			Log.ConsoleOutPut(1, c_Blue, t_Default, "CRC32.FileCRC");
			ExitProcess(0);
		}

		if (PluginCRC32 != 0x60211644)
		{
			Log.ConsoleOutPut(1, c_Blue, t_Default, "PluginCRC32 %08X", PluginCRC32);
			ExitProcess(0);
		}
	//}	
}

void GCNewCharacterInfoRecv(PMSG_NEW_CHARACTER_INFO_RECV* lpMsg) // OK
{
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+0) = lpMsg->Level;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+474) = lpMsg->LevelUpPoint;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+398) = lpMsg->Strength;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+400) = lpMsg->Dexterity;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+402) = lpMsg->Vitality;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+404) = lpMsg->Energy;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+406) = lpMsg->Leadership;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+108) = lpMsg->Life;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+116) = lpMsg->MaxLife;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+112) = lpMsg->Mana;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+120) = lpMsg->MaxMana;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+132) = lpMsg->BP;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+136) = lpMsg->MaxBP;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+124) = lpMsg->Shield;
	*(DWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+128) = lpMsg->MaxShield;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+434) = lpMsg->FruitAddPoint;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+436) = lpMsg->MaxFruitAddPoint;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+438) = lpMsg->FruitSubPoint;
	*(WORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+440) = lpMsg->MaxFruitSubPoint;
	*(QWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+8) = (QWORD)lpMsg->Experience;
	*(QWORD*)(*(DWORD*)(MAIN_CHARACTER_STRUCT)+16) = (QWORD)lpMsg->NextExperience;

	ViewHP = lpMsg->ViewCurHP;
	ViewMaxHP = lpMsg->ViewMaxHP;
	LevelUpPoints = lpMsg->ViewPoint;

	//Log.ConsoleOutPut(1, c_Blue, t_Default, "GCNewCharacterInfoRecv Life %d/%d", ViewHP, ViewMaxHP);

	Resets = lpMsg->ViewReset;
	GResets = lpMsg->ViewMasterReset;

	char* msg = (char*)malloc(sizeof(char[64]));

	wsprintf(msg, "L:%d | M:%d | R:%d | G:%d", lpMsg->ViewLevel, lpMsg->ViewMasterLevel, lpMsg->ViewReset, lpMsg->ViewMasterReset);

	LevelText = msg;
}

void DataSend(BYTE* lpMsg, int size)
{
	BYTE EncBuff[2048];

	if (gPacketManager.AddData(lpMsg, size) != 0 && gPacketManager.ExtractPacket(EncBuff) != 0)
	{
		BYTE send[2048];

		memcpy(send, EncBuff, size);

		if (EncBuff[0] == 0xC3 || EncBuff[0] == 0xC4)
		{
			if (EncBuff[0] == 0xC3)
			{
				BYTE save = EncBuff[1];

				EncBuff[1] = (*(BYTE*)(MAIN_PACKET_SERIAL))++;

				size = gPacketManager.Encrypt(&send[2], &EncBuff[1], (size - 1)) + 2;

				EncBuff[1] = save;

				send[0] = 0xC3;
				send[1] = LOBYTE(size);
			}
			else
			{
				BYTE save = EncBuff[2];

				EncBuff[2] = (*(BYTE*)(MAIN_PACKET_SERIAL))++;

				size = gPacketManager.Encrypt(&send[3], &EncBuff[2], (size - 2)) + 3;

				EncBuff[2] = save;

				send[0] = 0xC4;
				send[1] = HIBYTE(size);
				send[2] = LOBYTE(size);
			}
		}

		((void(__thiscall*)(void*, BYTE*, DWORD))0x00BE4865)((void*)0x0A1EA680, send, size);
	}
}

//WORD skillsToCheck[10] = { 41, 724, 235, 737, 215, 263, 277 };

void SendPacket(BYTE* lpMsg, DWORD size, int enc, int unk1)
{
	//Log.ConsoleOutPut(1, c_Blue, t_Default, "2: %02X | 3:%02X | 4:%02X | 5:%02X | 6:%02X | 7:%02X | 8:%02X | 9:%02X | 10:%02X", lpMsg[2], lpMsg[3], lpMsg[4], lpMsg[5], lpMsg[6], lpMsg[7], lpMsg[8], lpMsg[9], lpMsg[10]);
	
	if (lpMsg[2] == 0x19)
	{
		attackSkillCount++;
	}
	else if (lpMsg[2] == 0x1E)
	{
		durationSkillCount++;
	}
	else if (lpMsg[2] == 0xDF)
	{
		attackCount++;
	}	

	if (enc)
	{
		BYTE *send = (BYTE*)alloca(6148);
	
		memcpy(send, lpMsg, size);
	
		if (lpMsg[0] == 0xC1)
		{
			BYTE save = lpMsg[1];
	
			lpMsg[1] = (*(BYTE*)(MAIN_PACKET_SERIAL))++;
	
			size = gPacketManager.Encrypt(&send[2], &lpMsg[1], (size - 1)) + 2;
	
			lpMsg[1] = save;
	
			send[0] = 0xC3;
			send[1] = LOBYTE(size);
		}
		else if (lpMsg[0] == 0xC2)
		{
			BYTE save = lpMsg[2];
	
			lpMsg[2] = (*(BYTE*)(MAIN_PACKET_SERIAL))++;
	
			size = gPacketManager.Encrypt(&send[3], &lpMsg[2], (size - 2)) + 3;
	
			lpMsg[2] = save;
	
			send[0] = 0xC4;
			send[1] = HIBYTE(size);
			send[2] = LOBYTE(size);
		}
	
		((void(__thiscall*)(void*, BYTE*, DWORD))0x00BE4865)((void*)0x0A1EA680, send, size);
	}
	else
	{
		((void(__thiscall*)(void*, BYTE*, DWORD))0x00BE4865)((void*)0x0A1EA680, lpMsg, size);
	}
}

void ParsePacket(void* PackStream, int ChatServer, int ChatRoom)
{
	static int head, size, enc, DecSize;

	BYTE* recv = (BYTE*)alloca(11416);

	BYTE* lpMsg;

	while ((lpMsg = pParsePacket(PackStream)) != 0)
	{
		head = -1;
		size = enc = DecSize = 0;

		switch (lpMsg[0])
		{
		case 0xC1:
			head = lpMsg[2];
			size = lpMsg[1];
			enc = 0;
			break;
		case 0xC2:
			head = lpMsg[3];
			size = MAKE_NUMBERW(lpMsg[1], lpMsg[2]);
			enc = 0;
			break;
		case 0xC3:
			enc = 1;
			size = lpMsg[1];
			DecSize = gPacketManager.Decrypt(&recv[1], &lpMsg[2], size - 2);
			recv[0] = 0xC1;
			recv[1] = SET_NUMBERLB(DecSize + 2);
			size = DecSize + 2;
			lpMsg = recv;
			head = lpMsg[2];
			break;
		case 0xC4:
			enc = 1;
			size = MAKE_NUMBERW(lpMsg[1], lpMsg[2]);
			DecSize = gPacketManager.Decrypt(&recv[2], &lpMsg[3], size - 3);
			recv[0] = 0xC2;
			recv[1] = SET_NUMBERHB(DecSize + 3);
			recv[2] = SET_NUMBERLB(DecSize + 3);
			size = DecSize + 3;
			lpMsg = recv;
			head = lpMsg[3];
			break;
		}

		if (ChatServer == 1)
		{
			pChatProtocolCore(ChatRoom, head, lpMsg, size, enc);
		}
		else
		{
			ProtocolCoreEx(head, lpMsg, size, enc);
		}
	}
}

void GCNewMessageRecv(PMSG_NEW_MESSAGE_RECV* lpMsg) // OK
{
	WzMessageSend(lpMsg->type, lpMsg->message, "");
}

void GCMessagePopupRecv(PMSG_MESSAGE_POPUP_RECV* lpMsg)
{
	CustomFunctions::CallClientMessagePopup(lpMsg->message);
}

char * GetHardwareId()
{
	Log.ConsoleOutPut(1, c_Green, t_Default, "Serial %s", CSerial::GetSerial());
	HardwareId = CSerial::GetSerial();
	return HardwareId;
}

void GCSetCrownSwitchData(BYTE* Packet)
{
	CASTLE_SIEGE_SWITCH_INFO* lpMsg = (CASTLE_SIEGE_SWITCH_INFO*)Packet;

	if (lpMsg->id >= 2)
	{
		return;
	}

	g_CrownSwitchData[2].state = lpMsg->crownState;

	g_CrownSwitchData[lpMsg->id].index = lpMsg->switch_index;
	g_CrownSwitchData[lpMsg->id].state = lpMsg->state;
	g_CrownSwitchData[lpMsg->id].join_side = lpMsg->join_side;

	if (!g_CrownSwitchData[lpMsg->id].state)
	{
		g_CrownSwitchData[lpMsg->id].Reset();
	}
	else
	{
		memcpy(g_CrownSwitchData[lpMsg->id].name, lpMsg->user, 11);
		g_CrownSwitchData[lpMsg->id].name[10] = '\0';
		memcpy(g_CrownSwitchData[lpMsg->id].guild, lpMsg->guild, 9);
		g_CrownSwitchData[lpMsg->id].guild[8] = '\0';
	}

	if (!g_CrownSwitchData[2].state)
	{
		g_CrownSwitchData[2].Reset();
	}
	else
	{
		memcpy(g_CrownSwitchData[2].name, lpMsg->crownUser, 11);
		g_CrownSwitchData[2].name[10] = '\0';
		memcpy(g_CrownSwitchData[2].guild, lpMsg->crownGuild, 9);
		g_CrownSwitchData[2].guild[8] = '\0';
		g_CrownSwitchData[2].accumulated_time = lpMsg->accumulated_time;
	}
}

void HandleElementalDamage(BYTE* Packet)
{
	PMSG_ELEMENTAL_DAMAGE_RECV* lpMsg = (PMSG_ELEMENTAL_DAMAGE_RECV*)Packet;

	int aIndex = MAKE_NUMBERW(lpMsg->index[0], lpMsg->index[1]) & 0x7FFF;

	if (ViewIndex == aIndex)
	{
		ViewHP = lpMsg->ViewCurHP;
		ViewSD = lpMsg->ViewCurSD;
		//Log.ConsoleOutPut(1, c_Blue, t_Default, "HandleElementalDamage Life %d/%d", ViewHP, ViewMaxHP);
	}
}

void HandleDamage(BYTE* Packet)
{
	PMSG_DAMAGE_RECV* lpMsg = (PMSG_DAMAGE_RECV*)Packet;

	int aIndex = MAKE_NUMBERW(lpMsg->index[0], lpMsg->index[1]) & 0x7FFF;

	if (ViewIndex == aIndex)
	{
		ViewHP = lpMsg->ViewCurHP;
		ViewSD = lpMsg->ViewCurSD;
		//Log.ConsoleOutPut(1, c_Blue, t_Default, "HandleDamage Life %d/%d", ViewHP, ViewMaxHP);
	}
}

void HandleHP(BYTE* Packet)
{
	PMSG_LIFE_RECV* lpMsg = (PMSG_LIFE_RECV*)Packet;

	if (lpMsg->type == 0xFE)
	{
		ViewMaxHP = lpMsg->ViewHP;
		ViewMaxSD = lpMsg->ViewSD;
		//Log.ConsoleOutPut(1, c_Green, t_Default, "HandleHP MAX Life %d", ViewMaxHP);
	}
	else if (lpMsg->type == 0xFF)
	{
		ViewHP = lpMsg->ViewHP;
		ViewSD = lpMsg->ViewSD;
		//Log.ConsoleOutPut(1, c_Blue, t_Default, "HandleHP Life %d", ViewHP);
	}
}

void HandleRespawn(BYTE* Packet)
{
	PMSG_CHARACTER_REGEN_RECV* lpMsg = (PMSG_CHARACTER_REGEN_RECV*)Packet;

	ViewHP = lpMsg->ViewCurHP;
	ViewMP = lpMsg->ViewCurMP;
	ViewSD = lpMsg->ViewCurSD;
	ViewAG = lpMsg->ViewCurBP;

	//Log.ConsoleOutPut(1, c_Blue, t_Default, "HandleSpawn %d/%d", ViewHP, ViewMaxHP);
}

void HandleLevelUpPoint(BYTE* Packet)
{
	PMSG_LEVEL_UP_POINT_RECV* lpMsg = (PMSG_LEVEL_UP_POINT_RECV*)Packet;
	LevelUpPoints = lpMsg->ViewPoint;
}

void HandleBossRanking(BYTE* Packet)
{
	gInterface.BossTimer = GetTickCount64();

	gInterface.sRank = -1;
	
	PMSG_TARGET_BOSS_DATA* lpMsg = (PMSG_TARGET_BOSS_DATA*)Packet;

	gInterface.Count = lpMsg->Count;

	gInterface.Rank[0] = 0;
	strcpy(gInterface.Rank, lpMsg->Rank);
	gInterface.RankColor = lpMsg->RankColor;

	//Log.ConsoleOutPut(1, c_Blue, t_Default, "Boss %s", gInterface.Rank);
	//Log.ConsoleOutPut(1, c_Blue, t_Default, "Count %d", gInterface.Count);

	gInterface.Level = lpMsg->Level;

	gInterface.Life = lpMsg->Life;
	gInterface.MaxLife = lpMsg->MaxLife;

	gInterface.sRank = lpMsg->sRank;
	gInterface.sDamage = lpMsg->sDamage;

	for(int i = 0 ; i < gInterface.Count ; i++)
	{
		gInterface.Damage[i] = lpMsg->Damage[i];
		strcpy(gInterface.Name[i], lpMsg->Name[i]);
		gInterface.Name[i][10] = '\0';
	}
}

void GCConnectClientRecv(PMSG_CONNECT_CLIENT_RECV* lpMsg)
{
	ViewIndex = MAKE_NUMBERW(lpMsg->index[0], lpMsg->index[1]);
	//Log.ConsoleOutPut(1, c_Blue, t_Default, "index %d", ViewIndex);
}
