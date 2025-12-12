
// RTSDebugBreak.h
#pragma once
#include "CoreMinimal.h"
#include "HAL/PlatformMisc.h"

#if !UE_BUILD_SHIPPING
    #define RTS_ENSURE(Condition)                                            \
    do {                                                                            \
        if (UNLIKELY(!(Condition))) {                                               \
            UE_LOG(LogTemp, Warning, TEXT("RTS_SOFT_BREAK_IF failed: (%s)"),        \
                   TEXT(#Condition));                                               \
            if (FPlatformMisc::IsDebuggerPresent()) { UE_DEBUG_BREAK(); }           \
        }                                                                           \
    } while (0)
#else
    #define RTS_SOFT_BREAK_IF(Condition) do { (void)sizeof(Condition); } while (0)
#endif
