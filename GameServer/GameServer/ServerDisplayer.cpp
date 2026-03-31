#include "stdafx.h"
#include "ServerDisplayer.h"
#include "GameMain.h"
#include "Log.h"
#include "ServerInfo.h"
#include "SocketManager.h"
#include "User.h"
#include "BloodCastle.h"
#include "DevilSquare.h"
#include "ChaosCastle.h"
#include "IllusionTemple.h"
#include "InvasionManager.h"
#include "Crywolf.h"
#include "Kanturu.h"

static void FormatTimeHMS(int seconds, char* out, size_t outSize)
{
    if (seconds < 0) seconds = 0;
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    _snprintf_s(out, outSize, _TRUNCATE, "%02d:%02d:%02d", h, m, s);
}

static bool IsTopLogLine(const char* text)
{
    if (!text) return false;

    // sarim peste timestamp "HH:MM:SS "
    const char* p = text;
    if (strlen(text) > 9 && text[2] == ':' && text[5] == ':')
        p = text + 9;

    // AICI pui cuvintele cheie care să meargă în panoul TOP
    const char* topKeywords[] = {
        "[ServerInfo]",
        "[InvasionManager]",
        "Reload",
        "Requesting",
        "[MonsterStats]",

        0
    };

    for (int i = 0; topKeywords[i] != 0; ++i)
    {
        if (strstr(p, topKeywords[i]) != 0)
            return true;
    }
    return false;
}

static bool IsRightBottomLogLine(const char* text)
{
    if (!text) return false;

    const char* p = text;
    if (strlen(text) > 9 && text[2] == ':' && text[5] == ':')
        p = text + 9;

    // dacă vrei TOATE logurile, lași lista goală (primul element 0)
    const char* rbKeywords[] = {
        "[ObjectManager]",
        "[HardwareInfo]",
        0
    };

    if (rbKeywords[0] == 0)
        return true;

    for (int i = 0; rbKeywords[i] != 0; ++i)
    {
        if (strstr(p, rbKeywords[i]) != 0)
            return true;
    }
    return false;
}

CServerDisplayer gServerDisplayer;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CServerDisplayer::CServerDisplayer() // OK
{
    for (int n = 0; n < MAX_LOG_TEXT_LINE; n++)
    {
        memset(&this->m_log[n], 0, sizeof(this->m_log[n]));     // <- adaugă asta la loc
        memset(&this->m_logTop[n], 0, sizeof(this->m_logTop[n]));
        memset(&this->m_logRightBottom[n], 0, sizeof(this->m_logRightBottom[n]));
    }
    this->m_count = 0;
    this->m_countTop = 0;
    this->m_countRightBottom = 0;

	//this->m_font = CreateFont(50,0,0,0,FW_THIN,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_DONTCARE,"Times");
    this->m_font = CreateFont(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma");
	this->m_brush[0] = CreateSolidBrush(RGB(105,105,105));
	this->m_brush[1] = CreateSolidBrush(RGB(110,240,120));

	strcpy_s(this->m_DisplayerText[0],"STANDBY MODE");

	strcpy_s(this->m_DisplayerText[1],"ACTIVE MODE");
}

CServerDisplayer::~CServerDisplayer() // OK
{
	DeleteObject(this->m_font);
	DeleteObject(this->m_brush[0]);
	DeleteObject(this->m_brush[1]);
}

void CServerDisplayer::Init(HWND hWnd) // OK
{
	this->m_hwnd = hWnd;

	gLog.AddLog(1,"Logs");

	gLog.AddLog(gServerInfo.m_WriteChatLog,"Logs\\Chat");
	gLog.AddLog(gServerInfo.m_WriteCommandLog,"Logs\\Commands");
	gLog.AddLog(gServerInfo.m_WriteTradeLog,"Logs\\Trade");
	gLog.AddLog(gServerInfo.m_WriteConnectLog,"Logs\\Connection");
	gLog.AddLog(gServerInfo.m_WriteHackLog,"Logs\\Hack");
	gLog.AddLog(gServerInfo.m_WriteCashShopLog,"Logs\\GameShop");
	gLog.AddLog(gServerInfo.m_WriteChaosMixLog,"Logs\\ChaosMix");
	gLog.AddLog(gServerInfo.m_WriteItemDropLog, "Logs\\ItemDrop");
	gLog.AddLog(gServerInfo.m_WriteItemBagLog, "Logs\\ItemBag");
	gLog.AddLog(gServerInfo.m_WriteItemUpgradeLog, "Logs\\ItemUpgrade");
	gLog.AddLog(gServerInfo.m_WriteStalkLog, "Logs\\Stalk");
	gLog.AddLog(gServerInfo.m_WriteHitQueueLog, "Logs\\HitQueue");
	gLog.AddLog(gServerInfo.m_WriteMonsterSpawnerLog, "Logs\\MonsterSpawner");
	gLog.AddLog(1, "Logs\\CharacterStats");
	gLog.AddLog(1, "Logs\\Oficina");
}

void CServerDisplayer::Run() // OK
{
	this->SetWindowName();
	this->PaintAllInfo();
}

void CServerDisplayer::SetWindowName() // OK
{
	char buff[256];

	wsprintf(buff,"[%s] %s (PlayerCount : %d/%d) (MonsterCount : %d/%d)",GAMESERVER_VERSION,gServerInfo.m_ServerName,gObjTotalUser,gServerInfo.m_ServerMaxUserNumber,gObjTotalMonster,MAX_OBJECT_MONSTER);

	SetWindowText(this->m_hwnd,buff);
}

void CServerDisplayer::PaintAllInfo() // OK
{
    if (this->m_hwnd == 0)
        return;

    HDC hDC = GetDC(this->m_hwnd);
    if (hDC == 0)
        return;

    RECT rcClient;
    GetClientRect(this->m_hwnd, &rcClient);

    // fundal general
    FillRect(hDC, &rcClient, this->m_brush[0]);

    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    // 65% stânga = LOG, 35% dreapta = info
    int leftWidth = (int)(width * 0.65f);

    // ---------------- STÂNGA: LOG TOP + LOG BOTTOM ----------------
    RECT rcLogTop, rcLogBottom;

    rcLogTop.left = rcClient.left + 10;
    rcLogTop.top = rcClient.top + 80;
    rcLogTop.right = rcClient.left + leftWidth - 150;
    rcLogTop.bottom = rcClient.top + (height / 2) + 36;

    rcLogBottom.left = rcClient.left + 10;
    rcLogBottom.top = rcLogTop.bottom + 20;
    rcLogBottom.right = rcClient.left + leftWidth - 150;
    rcLogBottom.bottom = rcClient.bottom - 10;

    // ---------------- DREAPTA: TOP (3 panouri) + BOTTOM ----------------
    RECT rcRightTop;
    rcRightTop.left = rcLogBottom.right + 10;
    rcRightTop.top = rcClient.top + 80;
    rcRightTop.right = rcClient.right - 10;
    rcRightTop.bottom = rcClient.top + (height / 2) + 36;

    RECT rcRightBottom;
    rcRightBottom.left = rcLogBottom.right + 10;
    rcRightBottom.top = rcRightTop.bottom + 20;
    rcRightBottom.right = rcClient.right - 10;
    rcRightBottom.bottom = rcClient.bottom - 10;

    // împărțim rcRightTop în 3: EVENTS / INVASIONS / SPAWN
    RECT rcEvents = rcRightTop;
    RECT rcInvasion = rcRightTop;
    RECT rcSpawn = rcRightTop;

    int rightTopWidth = rcRightTop.right - rcRightTop.left;
    int thirdWidth = rightTopWidth / 3;

    rcEvents.right = rcEvents.left + thirdWidth - 2;
    rcInvasion.left = rcEvents.right + 4;
    rcInvasion.right = rcInvasion.left + thirdWidth - 2;
    rcSpawn.left = rcInvasion.right + 4;
    rcSpawn.right = rcRightTop.right;

    // ---------------- RAME ----------------
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);

    Rectangle(hDC, rcLogTop.left, rcLogTop.top, rcLogTop.right, rcLogTop.bottom);
    Rectangle(hDC, rcLogBottom.left, rcLogBottom.top, rcLogBottom.right, rcLogBottom.bottom);
    Rectangle(hDC, rcEvents.left, rcEvents.top, rcEvents.right, rcEvents.bottom);
    Rectangle(hDC, rcInvasion.left, rcInvasion.top, rcInvasion.right, rcInvasion.bottom);
    Rectangle(hDC, rcSpawn.left, rcSpawn.top, rcSpawn.right, rcSpawn.bottom);
    Rectangle(hDC, rcRightBottom.left, rcRightBottom.top, rcRightBottom.right, rcRightBottom.bottom);

    SelectObject(hDC, hOldPen);
    DeleteObject(hPen);

    HFONT hOldFont = 0;
    if (this->m_font != 0)
        hOldFont = (HFONT)SelectObject(hDC, this->m_font);

    SetBkMode(hDC, TRANSPARENT);

    // titlu centrat
   // font mare doar pentru titlu
    HFONT hTitleFont = CreateFont(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma");

    HFONT hOld = (HFONT)SelectObject(hDC, hTitleFont);

    const char* title = "ITZYMU GAME SERVER";

    RECT rcTitle;
    rcTitle.left = rcClient.left;
    rcTitle.right = rcClient.right;
    rcTitle.top = 0;
    rcTitle.bottom = 60;

    HBRUSH br = CreateSolidBrush(RGB(0, 4, 255));
    FillRect(hDC, &rcTitle, br);
    DeleteObject(br);

    SIZE sz;
    GetTextExtentPoint32A(hDC, title, lstrlenA(title), &sz);

    int titleX = (rcClient.right - rcClient.left - sz.cx) / 2;
    int titleY = 15;

    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(255, 255, 0));
    TextOutA(hDC, titleX, titleY, title, lstrlenA(title));

    SelectObject(hDC, hOld);
    DeleteObject(hTitleFont);


    // ---------------- TITLURI PANOURI ----------------
    SetTextColor(hDC, RGB(255, 255, 255));

    const char* titleLogTop = "Server Info";
    const char* titleLogBot = "SYSTEM LOG";
    const char* titleEvents = "EVENTS STATUS";
    const char* titleInvasions = "INVASIONS STATUS";
    const char* titleSpawn = "SPAWN";
    const char* titleRightBottom = "CONNECTION LOG";

    TextOutA(hDC, rcLogTop.left + 5, rcLogTop.top - 18, titleLogTop, (int)strlen(titleLogTop));
    TextOutA(hDC, rcLogBottom.left + 5, rcLogBottom.top - 18, titleLogBot, (int)strlen(titleLogBot));
    TextOutA(hDC, rcEvents.left + 5, rcEvents.top - 18, titleEvents, (int)strlen(titleEvents));
    TextOutA(hDC, rcInvasion.left + 5, rcInvasion.top - 18, titleInvasions, (int)strlen(titleInvasions));
    TextOutA(hDC, rcSpawn.left + 5, rcSpawn.top - 18, titleSpawn, (int)strlen(titleSpawn));
    TextOutA(hDC, rcRightBottom.left + 5, rcRightBottom.top - 18, titleRightBottom, (int)strlen(titleRightBottom));

    // ---------------- LOG PANELS ----------------
    this->LogTextPaintTop(hDC, rcLogTop);       // sus stânga
    this->LogTextPaint(hDC, rcLogBottom);    // jos stânga
    this->LogTextPaintRightBottom(hDC, rcRightBottom);  // jos dreapta

    // ---------------- EVENTS  (rcEvents) ----------------
    int  y;
    char buff[128];

    y = rcEvents.top + 5;
    SetTextColor(hDC, RGB(0, 190, 0));

    int  bcRemainSec = gBloodCastle.GetRemainTimeSec(0);
    char timeStr[16];
    SIZE as;
    int  rightX;
    const char* label;

    // Blood Castle
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(bcRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcEvents.right - 5 - as.cx;
    label = "Blood Castle :";
    TextOutA(hDC, rcEvents.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Devil Square
    int dsRemainSec = gDevilSquare.GetRemainTimeSec(0);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(dsRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcEvents.right - 5 - as.cx;
    label = "Devil Square :";
    TextOutA(hDC, rcEvents.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Chaos Castle
    int ccRemainSec = gChaosCastle.GetRemainTimeSec(0);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(ccRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcEvents.right - 5 - as.cx;
    label = "Chaos Castle :";
    TextOutA(hDC, rcEvents.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Illusion Temple
    int itRemainSec = gIllusionTemple.GetRemainTimeSec(0);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(itRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcEvents.right - 5 - as.cx;
    label = "Illusion Temple :";
    TextOutA(hDC, rcEvents.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Cry Wolf
    int cwRemainSec = gCrywolf.GetLeftTimeForEventSec();
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(cwRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcEvents.right - 5 - as.cx;
    label = "Cry Wolf :";
    TextOutA(hDC, rcEvents.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Kanturu
    int ktRemainSec = gKanturu.GetRemainTime();
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(ktRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcEvents.right - 5 - as.cx;
    label = "Kantru :";
    TextOutA(hDC, rcEvents.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // ---------------- INVASIONS (rcInvasion) ----------------
    y = rcInvasion.top + 5;
    SetTextColor(hDC, RGB(255, 190, 0));

    // Skeleton King
    int skRemainSec = gInvasionManager.GetRemainTimeSec(INV_SKELETON_KING);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(skRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcInvasion.right - 5 - as.cx;
    label = "Skeleton King :";
    TextOutA(hDC, rcInvasion.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Red Dragon
    int rdRemainSec = gInvasionManager.GetRemainTimeSec(INV_FIRE_DRAGON);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(rdRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcInvasion.right - 5 - as.cx;
    label = "Red Dragon :";
    TextOutA(hDC, rcInvasion.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Golden Invasion
    int giRemainSec = gInvasionManager.GetRemainTimeSec(INV_GOLDEN_MONSTER);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(giRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcInvasion.right - 5 - as.cx;
    label = "Golden Invasion :";
    TextOutA(hDC, rcInvasion.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // White Wizard
    int wwRemainSec = gInvasionManager.GetRemainTimeSec(INV_WHITE_WIZARD);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(wwRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcInvasion.right - 5 - as.cx;
    label = "White Wizard :";
    TextOutA(hDC, rcInvasion.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Medusa
    int msRemainSec = gInvasionManager.GetRemainTimeSec(INV_MEDUSA);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(msRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcInvasion.right - 5 - as.cx;
    label = "Medusa :";
    TextOutA(hDC, rcInvasion.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    // Cursed Santa
    int csRemainSec = gInvasionManager.GetRemainTimeSec(INV_CURSED_SANTA);
    memset(timeStr, 0, sizeof(timeStr));
    FormatTimeHMS(csRemainSec, timeStr, sizeof(timeStr));
    GetTextExtentPoint32A(hDC, timeStr, (int)strlen(timeStr), &as);
    rightX = rcInvasion.right - 5 - as.cx;
    label = "Cursed Santa :";
    TextOutA(hDC, rcInvasion.left + 5, y, label, (int)strlen(label));
    TextOutA(hDC, rightX, y, timeStr, (int)strlen(timeStr)); y += 16;

    if (hOldFont)
        SelectObject(hDC, hOldFont);

    ReleaseDC(this->m_hwnd, hDC);
}


// noul LogTextPaint: curăță panoul și desenează doar în interior
void CServerDisplayer::LogTextPaint(HDC hDC, RECT rcArea)
{
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hDC, &rcArea, br);
    DeleteObject(br);

    HRGN hRgn = CreateRectRgn(rcArea.left + 1, rcArea.top + 1,
        rcArea.right - 1, rcArea.bottom - 1);
    SelectClipRgn(hDC, hRgn);

    int lineHeight = 14;
    int maxLines = (rcArea.bottom - rcArea.top - 4) / lineHeight;

    int index = this->m_count - 1;
    if (index < 0) index = MAX_LOG_TEXT_LINE - 1;

    int drawn = 0;
    int checked = 0;
    int y = rcArea.bottom - lineHeight - 2;

    while (drawn < maxLines && checked < MAX_LOG_TEXT_LINE)
    {
        LOG_DISPLAY_INFO* lpLog = &this->m_log[index];
        checked++;

        // tot ce e "de top" NU mai apare în SYSTEM LOG
        if (IsTopLogLine(lpLog->text) || IsRightBottomLogLine(lpLog->text))
        {
            if (--index < 0) index = MAX_LOG_TEXT_LINE - 1;
            continue;
        }

        int len = (int)strlen(lpLog->text);
        if (len > 0)
        {
            COLORREF color = RGB(0, 0, 0);
            switch (lpLog->color)
            {
            case LOG_RED:   color = RGB(255, 0, 0);      break;
            case LOG_GREEN: color = RGB(0, 255, 0);      break;
            case LOG_BLUE:  color = RGB(0, 0, 255);      break;
            case LOG_DEBUG: color = RGB(208, 126, 186);  break;
            }
            SetTextColor(hDC, color);

            TextOutA(hDC, rcArea.left + 4, y, lpLog->text, len);
            y -= lineHeight;
            drawn++;
        }

        if (--index < 0) index = MAX_LOG_TEXT_LINE - 1;
    }

    SelectClipRgn(hDC, NULL);
    DeleteObject(hRgn);
}


void CServerDisplayer::LogAddText(eLogColor color, char* text, int size)
{
    size = ((size >= MAX_LOG_TEXT_SIZE) ? (MAX_LOG_TEXT_SIZE - 1) : size);

    // buffer general (SYSTEM LOG)
    memset(&this->m_log[this->m_count].text, 0, sizeof(this->m_log[this->m_count].text));
    memcpy(&this->m_log[this->m_count].text, text, size);
    this->m_log[this->m_count].color = color;
    this->m_count = (((++this->m_count) >= MAX_LOG_TEXT_LINE) ? 0 : this->m_count);

    // buffer special pentru CONNECTION LOG (fără timestamp)
    if (IsTopLogLine(text))
    {
        const char* p = text;
        if (strlen(text) > 9 && text[2] == ':' && text[5] == ':')
            p = text + 9;

        memset(&this->m_logTop[this->m_countTop].text, 0, sizeof(this->m_logTop[this->m_countTop].text));
        strncpy_s(this->m_logTop[this->m_countTop].text,
            sizeof(this->m_logTop[this->m_countTop].text),
            p, _TRUNCATE);

        this->m_logTop[this->m_countTop].color = color;
        this->m_countTop = (((++this->m_countTop) >= MAX_LOG_TEXT_LINE) ? 0 : this->m_countTop);
    }

    if (!IsTopLogLine(text) && IsRightBottomLogLine(text))
    {
        memset(&this->m_logRightBottom[this->m_countRightBottom].text, 0,
            sizeof(this->m_logRightBottom[this->m_countRightBottom].text));
        memcpy(&this->m_logRightBottom[this->m_countRightBottom].text, text, size);
        this->m_logRightBottom[this->m_countRightBottom].color = color;
        this->m_countRightBottom =
            (((++this->m_countRightBottom) >= MAX_LOG_TEXT_LINE) ? 0 : this->m_countRightBottom);
    }

    gLog.Output(LOG_GENERAL,"%s",&text[9]);
}


void CServerDisplayer::LogTextPaintFiltered(HDC hDC, RECT rcArea, bool topPanel)
{
    // fundal alb doar pentru zona de log
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hDC, &rcArea, br);
    DeleteObject(br);

    HRGN hRgn = CreateRectRgn(rcArea.left + 1, rcArea.top + 1,
        rcArea.right - 1, rcArea.bottom - 1);
    SelectClipRgn(hDC, hRgn);

    int lineHeight = 14;
    int maxLines = (rcArea.bottom - rcArea.top - 4) / lineHeight;

    int index = this->m_count - 1;
    if (index < 0) index = MAX_LOG_TEXT_LINE - 1;

    int drawn = 0;
    int y = rcArea.bottom - lineHeight - 2;

    int indexOriginal = index; // memorăm de unde am pornit

    while (drawn < maxLines)
    {
        LOG_DISPLAY_INFO* lpLog = &this->m_log[index];

        bool isTop = IsTopLogLine(lpLog->text);
        if (topPanel == isTop)
        {
            int len = (int)strlen(lpLog->text);
            if (len > 0)
            {
                COLORREF color = RGB(0, 0, 0);
                switch (lpLog->color)
                {
                case LOG_RED:   color = RGB(255, 0, 0);     break;
                case LOG_GREEN: color = RGB(0, 150, 0);     break;
                case LOG_BLUE:  color = RGB(0, 0, 255);     break;
                case LOG_DEBUG: color = RGB(208, 126, 186); break;
                }
                SetTextColor(hDC, color);
                TextOutA(hDC, rcArea.left + 4, y, lpLog->text, len);

                y -= lineHeight;
                drawn++;
            }
        }

        // următorul index
        if (--index < 0)
            index = MAX_LOG_TEXT_LINE - 1;

        // dacă am făcut un ciclu complet → STOP, nu desenăm duplicat
        if (index == indexOriginal)
            break;
    }


    SelectClipRgn(hDC, NULL);
    DeleteObject(hRgn);
}

void CServerDisplayer::LogTextPaintTop(HDC hDC, RECT rcArea)
{
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hDC, &rcArea, br);
    DeleteObject(br);

    HRGN hRgn = CreateRectRgn(rcArea.left + 1, rcArea.top + 1,
        rcArea.right - 1, rcArea.bottom - 1);
    SelectClipRgn(hDC, hRgn);

    int lineHeight = 14;
    int maxLines = (rcArea.bottom - rcArea.top - 4) / lineHeight;

    int index = this->m_countTop - 1;
    if (index < 0) index = MAX_LOG_TEXT_LINE - 1;

    int drawn = 0;
    int checked = 0; // câte intrări am verificat
    int y = rcArea.bottom - lineHeight - 2;

    const int MAX_SHOWN = 128;
    char shown[MAX_SHOWN][MAX_LOG_TEXT_SIZE] = { 0 };
    int  shownCount = 0;

    while (drawn < maxLines && checked < MAX_LOG_TEXT_LINE)
    {
        LOG_DISPLAY_INFO* lpLog = &this->m_logTop[index];
        checked++;

        if (lpLog->text[0] != '\0')
        {
            bool already = false;
            for (int i = 0; i < shownCount; ++i)
            {
                if (strcmp(shown[i], lpLog->text) == 0)
                {
                    already = true;
                    break;
                }
            }

            if (!already)
            {
                if (shownCount < MAX_SHOWN)
                    strncpy_s(shown[shownCount++], MAX_LOG_TEXT_SIZE, lpLog->text, _TRUNCATE);

                COLORREF color = RGB(0, 0, 0);
                switch (lpLog->color)
                {
                case LOG_RED:   color = RGB(255, 0, 0);      break;
                case LOG_GREEN: color = RGB(0, 150, 0);      break;
                case LOG_BLUE:  color = RGB(0, 0, 255);      break;
                case LOG_DEBUG: color = RGB(208, 126, 186);  break;
                }
                SetTextColor(hDC, color);

                int len = (int)strlen(lpLog->text);
                TextOutA(hDC, rcArea.left + 4, y, lpLog->text, len);
                y -= lineHeight;
                drawn++;
            }
        }

        if (--index < 0)
            index = MAX_LOG_TEXT_LINE - 1;
    }

    SelectClipRgn(hDC, NULL);
    DeleteObject(hRgn);
}

void CServerDisplayer::LogTextPaintRightBottom(HDC hDC, RECT rcArea)
{
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hDC, &rcArea, br);
    DeleteObject(br);

    HRGN hRgn = CreateRectRgn(rcArea.left + 1, rcArea.top + 1,
        rcArea.right - 1, rcArea.bottom - 1);
    SelectClipRgn(hDC, hRgn);

    int lineHeight = 14;
    int maxLines = (rcArea.bottom - rcArea.top - 4) / lineHeight;

    int index = this->m_countRightBottom - 1;
    if (index < 0) index = MAX_LOG_TEXT_LINE - 1;

    int drawn = 0;
    int checked = 0;
    int y = rcArea.bottom - lineHeight - 2;

    const int MAX_SHOWN = 128;
    char shown[MAX_SHOWN][MAX_LOG_TEXT_SIZE] = { 0 };
    int  shownCount = 0;

    while (drawn < maxLines && checked < MAX_LOG_TEXT_LINE)
    {
        LOG_DISPLAY_INFO* lpLog = &this->m_logRightBottom[index];
        checked++;

        if (lpLog->text[0] != '\0')
        {
            bool already = false;
            for (int i = 0; i < shownCount; ++i)
            {
                if (strcmp(shown[i], lpLog->text) == 0)
                {
                    already = true;
                    break;
                }
            }

            if (!already)
            {
                if (shownCount < MAX_SHOWN)
                    strncpy_s(shown[shownCount++], MAX_LOG_TEXT_SIZE, lpLog->text, _TRUNCATE);

                COLORREF color = RGB(0, 0, 0);
                switch (lpLog->color)
                {
                case LOG_RED:   color = RGB(255, 0, 0);      break;
                case LOG_GREEN: color = RGB(0, 150, 0);      break;
                case LOG_BLUE:  color = RGB(0, 0, 255);      break;
                case LOG_DEBUG: color = RGB(208, 126, 186);  break;
                }
                SetTextColor(hDC, color);

                int len = (int)strlen(lpLog->text);
                TextOutA(hDC, rcArea.left + 4, y, lpLog->text, len);
                y -= lineHeight;
                drawn++;
            }
        }

        if (--index < 0)
            index = MAX_LOG_TEXT_LINE - 1;
    }

    SelectClipRgn(hDC, NULL);
    DeleteObject(hRgn);
}
