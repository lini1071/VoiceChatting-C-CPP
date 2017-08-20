// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 또는 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.
//
#pragma once

/// definition for restrict keyword
#if defined(_MSC_VER)
	// stdafx.h : Microsoft specific
	#define RESTRICT __restrict
#elif defined(__GNUC__)
	// stdafx.h : GCC specific
	#define RESTRICT __restrict__
#endif

// Project-dependent macros but doesn't have OS-dependency.
#define DEFAULT_PORT 62217
// (probably) byte size of CPU L1 cache
#define L1_CACHE_SIZE		4096
// byte size of total CPU L1 cache
#define L1_CACHE_TOTAL_SIZE L1_CACHE_SIZE * 8
// byte size of CPU L2 cache
#define L2_CACHE_SIZE		L1_CACHE_TOTAL_SIZE
// byte size of total CPU L2 cache
///#define L2_CACHE_TOTAL_SIZE	L2_CACHE_SIZE * 8

// Define audio data macro.
// Sample duration must be shorter than 30ms.
#define BITS_PER_SAMPLE 24		// 24-bit
#define SAMPLING_RATE	48000	// 48KHz
#define SAMPLE_DURATION	0.025	// 25ms
#define DATA_SIZE		((unsigned int) ((BITS_PER_SAMPLE / 8) * SAMPLING_RATE * SAMPLE_DURATION))

// Define audio data structure.
// This structure(class) is also used in Java application,
// so it must not be aligned differently from basis
// because Java doesn't support custom memory aligning.
typedef struct AudioDatagram
{
	// IPv4 Address expression
	union {
		unsigned long long full;		// x64
		struct {
			union {
				unsigned char	charType[4];
				unsigned int	intType;
			} ipv4;
			union {
				unsigned char	charType[4];
				unsigned int	intType;
			} port;
		} sep;							// x86
	} addr;

	unsigned int	dataType;
	unsigned int	dataLength;
	unsigned char	dataArray[DATA_SIZE];

} AudioDatagram;

enum AudioDataType {
	AUDIO = 0, TEXT = 1, MISC = 2
} AudioDataType;


#if defined(_WIN32)
	#include "targetver.h"

	// Exclude some Windows header files
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <mmdeviceapi.h>		// WASAPI
	#include <StrSafe.h>			// StringCbPrintf, StringCchPrintf

	#ifdef __cplusplus	// C++

		#ifdef AUDIO_DATA_CONTAINER_USE_STL
			#include <queue>
			#define AUDIO_DATA_CONTAINER std::queue<AudioDatagram> AudioDataQueue
		#else
			// MSVC++ specific, for thread-safety
			// Visual Studio provides some concurrent containers.
			// e.g.) concurrent_vector, concurrent_queue, concurrent_unordered_map, etc.
			// These contents can be only used in C++, not C.
			#include <concurrent_queue.h>
			#define AUDIO_DATA_CONTAINER concurrency::concurrent_queue<AudioDatagram*>
		#endif

		#ifdef EVENT_HANDLE_CONTAINER_USE_STL
			#include <unordered_map>
			#define EVENT_HANDLE_CONTAINER std::unordered_map<HANDLE, HANDLE>
		#else
			#include <concurrent_unordered_map.h>
			#define EVENT_HANDLE_CONTAINER concurrency::concurrent_unordered_map<HANDLE, HANDLE>
		#endif

	#else				// C

	#endif

	// For debugging, and measuring performance
	typedef struct TMCContainer {
		LARGE_INTEGER	freq;
		LARGE_INTEGER	start;
		LARGE_INTEGER	end;
		LARGE_INTEGER	elapsed;

		TCHAR			string[( 256 - ( 4 * sizeof(LARGE_INTEGER) ) ) / sizeof(TCHAR)];
	} TMCContainer;

	// Time measuring functions declaration
	__forceinline void TimeMeasureGetFreq(TMCContainer *tmc);
	__forceinline void TimeMeasureStart(TMCContainer *tmc);
	__forceinline void TimeMeasureEnd(TMCContainer *tmc, const char func_name[]);



	// project-dependent and does have os-dependency.
	#define DEBUG_OUTPUT_FILE_PATH L"C:\Users\lini\Desktop\debug.log"

	#ifdef DEBUG_OUTPUT_FILE
		// Print out string to target file.
		#define DEBUG_OUTPUT(x) _ftprintf(DEBUG_OUTPUT_FILE, x);
	#else
		// Print out string to debug window.
		#define DEBUG_OUTPUT(x) OutputDebugString(x)
	#endif


	// Time measuring functions definition
	__forceinline void TimeMeasureGetFreq(TMCContainer *tmc)
	{
		QueryPerformanceFrequency(&(tmc->freq));
	};
	__forceinline void TimeMeasureStart(TMCContainer *tmc)
	{
		QueryPerformanceCounter(&(tmc->start));
	};
	__forceinline void TimeMeasureEnd(TMCContainer *tmc, const char func_name[])
	{
		QueryPerformanceCounter(&(tmc->end));
		tmc->elapsed.QuadPart = (tmc->end.QuadPart - tmc->start.QuadPart) * 1000;
		tmc->elapsed.QuadPart /= tmc->freq.QuadPart;

	#ifdef _WIN32

	#ifdef UNICODE
		TCHAR funct[sizeof(func_name)];
		//mbstowcs_s(0, funct, sizeof(funct), func_name, sizeof(func_name) - 1);
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, func_name, sizeof(func_name), funct, sizeof(funct));
	#else
		TCHAR *funct = func_name;
	#endif

		// Assure no CRT function is called on a thread which is made by CreateThread.
		// StringCchPrintf function is defined in <StrSafe.h>.
		StringCchPrintf(tmc->string, (256 - (4 * sizeof(LARGE_INTEGER))) / sizeof(TCHAR),
			TEXT("%s elapsed time : %.3lld\n"), funct, tmc->elapsed.QuadPart);

	#else // not Windows


		swprintf(tmc->string, (256 - (4 * sizeof(LARGE_INTEGER))) / sizeof(TCHAR),
			L"%s loop elapsed time : %.3lld\n", funct, tmc->elapsed.QuadPart);
	#endif

		DEBUG_OUTPUT(tmc->string);
	};


#elif defined(__linux__)


#endif