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

// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.
#include <stdbool.h>
#include <stdint.h>

/// Project-dependent macros but doesn't have OS-dependency.

// default integer value of port number
#define DEFAULT_PORT 62217
// (probably) byte size of CPU L1 cache
#define L1_CACHE_SIZE		4096
// byte size of total CPU L1 cache
#define L1_CACHE_TOTAL_SIZE L1_CACHE_SIZE * 8
// byte size of CPU L2 cache
#define L2_CACHE_SIZE		L1_CACHE_TOTAL_SIZE
// byte size of total CPU L2 cache
#define L2_CACHE_TOTAL_SIZE	L2_CACHE_SIZE * 8
// define buffer size as half of L2 cache size
#define BUFFER_SIZE	(L2_CACHE_TOTAL_SIZE) / 2

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
	union addr {
		struct sep {
			union ipv4 {
				unsigned char	charType[4];
				uint32_t		longType;
			} ipv4;
			union port {
				unsigned char	charType[4];
				uint32_t		longType;
			} port;
		} sep;					// x86
#if defined(_M_X64) || defined(__amd64__)
		uint64_t full;			// x64
#endif
	} addr;

	unsigned long	dataTime;
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
	// Windows 헤더 파일:
	#include <windows.h>
	#include <strsafe.h>

	#ifdef __cplusplus	// C++

		#define DATA_USE_SMART_POINTER
		#ifdef	DATA_USE_SMART_POINTER
		#include <memory>	// shared_ptr
		typedef std::shared_ptr<AudioDatagram> AudioDatagramPtr;
		#endif

		#ifdef AUDIO_DATA_CONTAINER_USE_STL
			#include <queue>
			#define AUDIO_DATA_CONTAINER std::queue<AudioDatagram> AudioDataQueue
		#else
			// MSVC++ specific, for thread-safety
			// Visual Studio provides some concurrent containers.
			// e.g.) concurrent_vector, concurrent_queue, concurrent_unordered_map, etc.
			// And this contents can be only used in C++, not C.
			#include <concurrent_queue.h>
			#define AUDIO_DATA_CONTAINER concurrency::concurrent_queue<AudioDatagramPtr>
		#endif

	#else				// C
	typedef AudioDatagram* AudioDatagramPtr;
	#endif

	// project-dependent and does have os-dependency.
	#define DEBUG_OUTPUT_FILE_PATH L"C:\Users\lini\Desktop\debug.log"

	#ifdef DEBUG_OUTPUT_FILE
	// Print out string to target file.
	#define DEBUG_OUTPUT(x) _ftprintf(DEBUG_OUTPUT_FILE, x);
	#else
	// Print out string to debug window.
	#define DEBUG_OUTPUT(x) OutputDebugString(x)
	#endif

#elif defined(__linux__)

#endif
