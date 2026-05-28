#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformMisc.h"

// When disabled, the condition and message are not evaluated.
#ifndef RTS_ENABLE_ENSURES
#define RTS_ENABLE_ENSURES 1
#endif

// When disabled, ensures only log/output. They never trigger a debugger break.
#ifndef RTS_ENSURE_ENABLE_DEBUG_BREAK
#define RTS_ENSURE_ENABLE_DEBUG_BREAK 1
#endif

// When enabled, each RTS_ENSURE / RTS_ENSUREMSGF location breaks only once.
// It still logs every time.
#ifndef RTS_ENSURE_BREAK_ONCE_PER_CALLSITE
#define RTS_ENSURE_BREAK_ONCE_PER_CALLSITE 1
#endif

#if RTS_ENABLE_ENSURES

namespace RTSEnsurePrivate
{
	FORCEINLINE void TriggerDebugBreak()
	{
#if RTS_ENSURE_ENABLE_DEBUG_BREAK
		if (!FPlatformMisc::IsDebuggerPresent())
		{
			return;
		}

		// In Shipping, UE_DEBUG_BREAK() is compiled out.
		// Use the compiler/platform breakpoint directly when a debugger is attached.
#if PLATFORM_WINDOWS
		__debugbreak();
#else
		FPlatformMisc::DebugBreak();
#endif

#endif
	}
}
#if RTS_ENSURE_BREAK_ONCE_PER_CALLSITE

#define RTS_ENSURE_DEBUG_BREAK_ONCE()			\
		do												\
		{												\
			static bool bRtsHasBrokenOnce = false;		\
			if (!bRtsHasBrokenOnce)						\
			{											\
				bRtsHasBrokenOnce = true;				\
				RTSEnsurePrivate::TriggerDebugBreak();	\
			}											\
		} while (false)

#else

		#define RTS_ENSURE_DEBUG_BREAK_ONCE()			\
		do												\
		{												\
			RTSEnsurePrivate::TriggerDebugBreak();		\
		} while (false)

#endif

#define RTS_ENSURE(Condition)																		\
	do																									\
	{																									\
		if (UNLIKELY(!(Condition)))																		\
		{																								\
			UE_LOG(LogTemp, Warning, TEXT("RTS_ENSURE failed: (%s)"), TEXT(#Condition));					\
																										\
			FPlatformMisc::LowLevelOutputDebugStringf(													\
				TEXT("RTS_ENSURE failed: (%s)\n"),														\
				TEXT(#Condition));																		\
																										\
			RTS_ENSURE_DEBUG_BREAK_ONCE();																\
		}																								\
	} while (false)

#define RTS_ENSUREMSGF(Condition, MessageFormat, ...)												\
	do																									\
	{																									\
		if (UNLIKELY(!(Condition)))																		\
		{																								\
			const FString RtsEnsureContextMessage = FString::Printf(MessageFormat, ##__VA_ARGS__);		\
																										\
			UE_LOG(																						\
				LogTemp,																				\
				Warning,																				\
				TEXT("RTS_ENSURE failed: (%s)\ncontext: %s"),											\
				TEXT(#Condition),																		\
				*RtsEnsureContextMessage);																\
																										\
			FPlatformMisc::LowLevelOutputDebugStringf(													\
				TEXT("RTS_ENSURE failed: (%s)\ncontext: %s\n"),											\
				TEXT(#Condition),																		\
				*RtsEnsureContextMessage);																\
																										\
			RTS_ENSURE_DEBUG_BREAK_ONCE();																\
		}																								\
	} while (false)

#else

	#define RTS_ENSURE(Condition) do { } while (false)
	#define RTS_ENSUREMSGF(Condition, MessageFormat, ...) do { } while (false)

#endif
