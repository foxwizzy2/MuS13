// LicenceCheck.cpp
#include "stdafx.h"
#include "LicenceCheck.h"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <cstdio>

#pragma comment(lib, "wbemuuid.lib")

static const BYTE XorKey1[] = { 0x12, 0x3A, 0x5C, 0x7E, 0x91, 0xAB, 0xCD, 0xEF };
static const BYTE XorKey2[] = { 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10 };


static void ShowLicenceError(const char* msg)
{
	MessageBoxA(NULL, msg, "GameServer Licence Error", MB_ICONERROR | MB_OK);
}

static std::string Trim(const std::string& s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}


static int B64Index(char c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

static bool Base64Decode(const std::string& in, std::vector<BYTE>& out)
{
	out.clear();
	int val = 0;
	int valb = -8;

	for (size_t i = 0; i < in.size(); ++i)
	{
		char c = in[i];

		if (c == '=' || c == '\r' || c == '\n')
			break;

		int d = B64Index(c);
		if (d == -1)
			continue;

		val = (val << 6) + d;
		valb += 6;

		if (valb >= 0)
		{
			out.push_back((BYTE)((val >> valb) & 0xFF));
			valb -= 8;
		}
	}

	return !out.empty();
}

static const char B64Alphabet[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const BYTE* data, size_t len)
{
	std::string out;
	out.reserve(((len + 2) / 3) * 4);
	for (size_t i = 0; i < len; i += 3)
	{
		DWORD v = 0;
		int rem = (int)(len - i);
		v |= data[i] << 16;
		if (rem > 1) v |= data[i + 1] << 8;
		if (rem > 2) v |= data[i + 2];

		out.push_back(B64Alphabet[(v >> 18) & 0x3F]);
		out.push_back(B64Alphabet[(v >> 12) & 0x3F]);
		out.push_back(rem > 1 ? B64Alphabet[(v >> 6) & 0x3F] : '=');
		out.push_back(rem > 2 ? B64Alphabet[v & 0x3F] : '=');
	}
	return out;
}

// ----- XOR -----

static void XorWithKey(std::vector<BYTE>& data, const BYTE* key, size_t keyLen)
{
	if (keyLen == 0) return;
	for (size_t i = 0; i < data.size(); ++i)
	{
		data[i] ^= key[i % keyLen];
	}
}

// ----- WMI helpers -----

static bool InitComOnce()
{
	static bool initialized = false;
	static bool ok = false;

	if (!initialized)
	{
		HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE)
		{
			hr = CoInitializeSecurity(
				NULL,
				-1,
				NULL,
				NULL,
				RPC_C_AUTHN_LEVEL_DEFAULT,
				RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL,
				EOAC_NONE,
				NULL);

			if (SUCCEEDED(hr) || hr == RPC_E_TOO_LATE)
			{
				ok = true;
			}
		}
		initialized = true;
	}
	return ok;
}

static bool GetWmiString(const wchar_t* wclass, const wchar_t* wprop, std::string& out)
{
	out.clear();

	if (!InitComOnce()) return false;

	IWbemLocator* pLoc = NULL;
	IWbemServices* pSvc = NULL;

	HRESULT hr = CoCreateInstance(
		CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID*)&pLoc);

	if (FAILED(hr)) return false;

	hr = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"),
		NULL, NULL, 0, NULL, 0, 0, &pSvc);

	if (FAILED(hr))
	{
		pLoc->Release();
		return false;
	}

	hr = CoSetProxyBlanket(
		pSvc,
		RPC_C_AUTHN_WINNT,
		RPC_C_AUTHZ_NONE,
		NULL,
		RPC_C_AUTHN_LEVEL_CALL,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE);

	if (FAILED(hr))
	{
		pSvc->Release();
		pLoc->Release();
		return false;
	}

	wchar_t query[256];
	_snwprintf_s(query, _TRUNCATE, L"SELECT %s FROM %s", wprop, wclass);

	IEnumWbemClassObject* pEnumerator = NULL;
	hr = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t(query),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hr))
	{
		pSvc->Release();
		pLoc->Release();
		return false;
	}

	IWbemClassObject* pObj = NULL;
	ULONG ret = 0;
	hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &ret);

	if (SUCCEEDED(hr) && ret)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);

		hr = pObj->Get(wprop, 0, &vtProp, 0, 0);
		if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR)
		{
			_bstr_t b(vtProp.bstrVal);
			const char* s = (const char*)b;
			if (s) out = s;
		}

		VariantClear(&vtProp);
		pObj->Release();
	}

	pEnumerator->Release();
	pSvc->Release();
	pLoc->Release();

	return !out.empty();
}

static std::string GetCurrentHWID()
{
	std::string cpu, bios, disk;

	GetWmiString(L"Win32_Processor", L"ProcessorId", cpu);
	GetWmiString(L"Win32_BIOS", L"SerialNumber", bios);
	GetWmiString(L"Win32_DiskDrive", L"SerialNumber", disk);

	std::string raw = cpu + "-" + bios + "-" + disk;
	return Base64Encode((const BYTE*)raw.data(), raw.size());
}


static bool DecodeLicence(const std::string& licStr, std::string& outHwid, int& outDateInt)
{
	outHwid.clear();
	outDateInt = 0;

	std::vector<BYTE> data;
	if (!Base64Decode(licStr, data))
	{
		return false;
	}

	XorWithKey(data, XorKey2, sizeof(XorKey2));
	XorWithKey(data, XorKey1, sizeof(XorKey1));

	std::string decoded((const char*)data.data(), data.size());

	size_t sep = decoded.find('|');
	if (sep == std::string::npos) return false;

	std::string hwid = decoded.substr(0, sep);
	std::string dateStr = decoded.substr(sep + 1);

	dateStr = Trim(dateStr);
	if (dateStr.size() != 8) return false;

	int y = 0, m = 0, d = 0;
	if (sscanf_s(dateStr.c_str(), "%4d%2d%2d", &y, &m, &d) != 3)
	{
		return false;
	}

	outHwid = hwid;
	outDateInt = y * 10000 + m * 100 + d;
	return true;
}

static bool LoadLicenceFile(std::string& outHwid, int& outDateInt)
{
	const char* LIC_FILE = "..\\Data\\licence.izy";
	FILE* f = NULL;
	if (fopen_s(&f, LIC_FILE, "rb") != 0 || !f)
	{
		ShowLicenceError("Invalide licence or expired");
		return false;
	}

	std::string content;
	char buf[512];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
	{
		content.append(buf, n);
	}
	fclose(f);

	content = Trim(content);
	if (content.empty())
	{
		ShowLicenceError("Invalide licence or expired.");
		return false;
	}

	if (!DecodeLicence(content, outHwid, outDateInt))
	{
		ShowLicenceError("Invalide licence or expired.");
		return false;
	}

	return true;
}


bool CheckLicence()
{
	std::string licHwid;
	int expDateInt = 0;

	if (!LoadLicenceFile(licHwid, expDateInt))
	{
		return false;
	}

	SYSTEMTIME st;
	GetLocalTime(&st);
	int todayInt = st.wYear * 10000 + st.wMonth * 100 + st.wDay;

	if (todayInt > expDateInt)
	{
		ShowLicenceError("Invalide licence or expired.");
		return false;
	}

	std::string currentHwid = GetCurrentHWID();

	if (_stricmp(currentHwid.c_str(), licHwid.c_str()) != 0)
	{
		ShowLicenceError("Invalide licence or expired.");
		return false;
	}

	return true;
}
