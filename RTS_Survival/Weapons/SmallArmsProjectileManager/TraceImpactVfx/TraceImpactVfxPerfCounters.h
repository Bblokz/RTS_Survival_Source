// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

class UNiagaraSystem;

/**
 * @brief What drawing trace impacts costs on the game thread, gathered for the -TraceImpactVfxMeasure scenario.
 * The scenario turns the gathering on and reads the totals after each phase.
 */
struct FTraceImpactVfxPerfCounters
{
	static bool bIsGathering;
	static uint64 DispatchCycles;
	static int32 ImpactCount;
	// Impacts per kind of weapon owner (its first native class), to see that hull and turret MGs take part.
	static TMap<FName, int32> ImpactCountByOwnerClass;

	static void Reset();

	static void RecordWeaponOwner(const UObject* WeaponOwner);

	// Logs every distinct impact system a trace weapon is configured with, once.
	static void RecordConfiguredSystem(const TCHAR* ImpactKind, const UNiagaraSystem* System, const FVector& Scale);
};

/** Adds the time it lives to the dispatch total; counts as one impact when asked to. */
struct FTraceImpactVfxScopedDispatchTimer
{
	explicit FTraceImpactVfxScopedDispatchTimer(const bool bCountAsImpact)
		: M_StartCycles(FTraceImpactVfxPerfCounters::bIsGathering ? FPlatformTime::Cycles64() : 0)
	{
		FTraceImpactVfxPerfCounters::ImpactCount +=
			(FTraceImpactVfxPerfCounters::bIsGathering && bCountAsImpact) ? 1 : 0;
	}

	~FTraceImpactVfxScopedDispatchTimer()
	{
		if (M_StartCycles != 0)
		{
			FTraceImpactVfxPerfCounters::DispatchCycles += FPlatformTime::Cycles64() - M_StartCycles;
		}
	}

private:
	uint64 M_StartCycles;
};

#endif // RTS_WITH_MAP_TESTS
