// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSCombatPingSubsystem.h"

#include "RTSCombatPingSettings.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"

namespace RTSCombatPingConstants
{
	constexpr uint8 PlayerOwnerIndex = 1;
}

bool URTSCombatPingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URTSCombatPingSubsystem::ReportUnitHit(const URTSComponent& HitUnitRTSComponent)
{
	if (HitUnitRTSComponent.GetOwningPlayer() != RTSCombatPingConstants::PlayerOwnerIndex)
	{
		return;
	}
	const URTSCombatPingSettings* const Settings = GetDefault<URTSCombatPingSettings>();
	if (not Settings->bEnableCombatPings)
	{
		return;
	}
	const double NowSeconds = GetRealTimeSeconds();
	const bool bWasAlreadyUnderFire = GetWasUnitHitRecently(HitUnitRTSComponent, NowSeconds, *Settings);
	M_LastHitSecondsPerUnit.Add(&HitUnitRTSComponent, NowSeconds);
	if (Settings->bOnlyPingWhenUnitComesUnderFire && bWasAlreadyUnderFire)
	{
		return;
	}
	const AActor* const HitUnit = HitUnitRTSComponent.GetOwner();
	if (not IsValid(HitUnit))
	{
		return;
	}
	RemoveStaleUnitHitRecords(NowSeconds, *Settings);
	TryAddPing(HitUnit->GetActorLocation());
}

bool URTSCombatPingSubsystem::TryAddPing(const FVector& WorldLocation)
{
	const URTSCombatPingSettings* const Settings = GetDefault<URTSCombatPingSettings>();
	const double NowSeconds = GetRealTimeSeconds();
	if (NowSeconds - M_LastPingRealTimeSeconds < Settings->GlobalMinIntervalSeconds)
	{
		return false;
	}
	RemoveExpiredPings(NowSeconds, *Settings);
	if (GetIsAreaSuppressed(WorldLocation, NowSeconds, *Settings)
		|| GetVisiblePingCount(NowSeconds, *Settings) >= Settings->MaxVisiblePings)
	{
		return false;
	}
	FRTSCombatPing& NewPing = M_Pings.AddDefaulted_GetRef();
	NewPing.M_WorldLocation = WorldLocation;
	NewPing.M_SpawnRealTimeSeconds = NowSeconds;
	M_LastPingRealTimeSeconds = NowSeconds;
	return true;
}

void URTSCombatPingSubsystem::AddPreviewPing(const FVector& WorldLocation, const float AgeNowSeconds)
{
	FRTSCombatPing& NewPing = M_Pings.AddDefaulted_GetRef();
	NewPing.M_WorldLocation = WorldLocation;
	NewPing.M_SpawnRealTimeSeconds = GetRealTimeSeconds() - AgeNowSeconds;
}

void URTSCombatPingSubsystem::AddReinforcementsPing(const FVector& WorldLocation, const float DurationSeconds)
{
	if (DurationSeconds <= 0.f)
	{
		return;
	}
	const double NowSeconds = GetRealTimeSeconds();
	M_ReinforcementsPings.RemoveAll([NowSeconds](const FRTSReinforcementsPing& Ping)
	{
		return NowSeconds - Ping.M_SpawnRealTimeSeconds > Ping.M_DurationSeconds;
	});
	FRTSReinforcementsPing& NewPing = M_ReinforcementsPings.AddDefaulted_GetRef();
	NewPing.M_WorldLocation = WorldLocation;
	NewPing.M_SpawnRealTimeSeconds = NowSeconds;
	NewPing.M_DurationSeconds = DurationSeconds;
}

double URTSCombatPingSubsystem::GetRealTimeSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.0;
}

void URTSCombatPingSubsystem::RemoveExpiredPings(const double NowSeconds, const URTSCombatPingSettings& Settings)
{
	// A ping is kept while it is drawn and while it still keeps its area quiet.
	const double KeepSeconds = FMath::Max(Settings.PingLifetimeSeconds, Settings.AreaSuppressionSeconds);
	M_Pings.RemoveAll([NowSeconds, KeepSeconds](const FRTSCombatPing& Ping)
	{
		return NowSeconds - Ping.M_SpawnRealTimeSeconds > KeepSeconds;
	});
}

bool URTSCombatPingSubsystem::GetIsAreaSuppressed(const FVector& WorldLocation, const double NowSeconds,
                                                  const URTSCombatPingSettings& Settings) const
{
	// Pings kept for their lifetime may be older than the suppression time; those no longer count.
	const float RadiusSquared = FMath::Square(Settings.AreaSuppressionRadius);
	for (const FRTSCombatPing& Ping : M_Pings)
	{
		const bool bIsRecent = NowSeconds - Ping.M_SpawnRealTimeSeconds <= Settings.AreaSuppressionSeconds;
		if (bIsRecent && FVector::DistSquared2D(Ping.M_WorldLocation, WorldLocation) <= RadiusSquared)
		{
			return true;
		}
	}
	return false;
}

int32 URTSCombatPingSubsystem::GetVisiblePingCount(const double NowSeconds,
                                                   const URTSCombatPingSettings& Settings) const
{
	int32 VisiblePingCount = 0;
	for (const FRTSCombatPing& Ping : M_Pings)
	{
		if (NowSeconds - Ping.M_SpawnRealTimeSeconds <= Settings.PingLifetimeSeconds)
		{
			++VisiblePingCount;
		}
	}
	return VisiblePingCount;
}

bool URTSCombatPingSubsystem::GetWasUnitHitRecently(const URTSComponent& HitUnitRTSComponent, const double NowSeconds,
                                                    const URTSCombatPingSettings& Settings) const
{
	const double* const LastHitSeconds = M_LastHitSecondsPerUnit.Find(&HitUnitRTSComponent);
	return LastHitSeconds != nullptr && NowSeconds - *LastHitSeconds <= Settings.UnitQuietSecondsBeforeNewPing;
}

void URTSCombatPingSubsystem::RemoveStaleUnitHitRecords(const double NowSeconds,
                                                        const URTSCombatPingSettings& Settings)
{
	// Units that died or have not been hit for a while no longer need a record.
	for (auto RecordIt = M_LastHitSecondsPerUnit.CreateIterator(); RecordIt; ++RecordIt)
	{
		if (not RecordIt.Key().IsValid()
			|| NowSeconds - RecordIt.Value() > Settings.UnitQuietSecondsBeforeNewPing)
		{
			RecordIt.RemoveCurrent();
		}
	}
}
