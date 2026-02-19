#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <ExDisp.h>
#define _CRT_SECURE_NO_WARNINGS
#include <cassert>
#include <cstdlib>
#include <atomic>
#include <deque>
#include <thread>

#define my_assert(expression) do \
	if (!(expression)) \
	{ \
		WCHAR message[MAX_LOADSTRING]; \
		if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), 0, message, MAX_LOADSTRING, nullptr)) \
			LoadString(hInst, IDS_UNKNOWN_ERROR, message, MAX_LOADSTRING); \
		MessageBox(nullptr, message, nullptr, MB_OK); \
		return 0; \
	} \
while (false)
