// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CrewRepairProgress.h"

#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_CrewRepairProgress::SyncRepairProgress(const float RemainingSeconds, const float TotalSeconds)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_RemainingSecondsAtSync = FMath::Max(RemainingSeconds, 0.f);
	M_TotalSeconds = FMath::Max(TotalSeconds, M_RemainingSecondsAtSync);
	M_SyncWorldTime = World->GetTimeSeconds();
	ApplyProgressAtTime(M_SyncWorldTime);
}

void UW_CrewRepairProgress::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	ApplyProgressAtTime(World->GetTimeSeconds());
}

void UW_CrewRepairProgress::ApplyProgressAtTime(const double WorldTimeSeconds) const
{
	if (M_TotalSeconds <= 0.f || not GetIsValidProgressBar())
	{
		return;
	}
	// The crew works at one second of repair per game second, so the remaining work drops linearly between syncs.
	const float SecondsSinceSync = static_cast<float>(FMath::Max(WorldTimeSeconds - M_SyncWorldTime, 0.0));
	const float RemainingSeconds = FMath::Max(M_RemainingSecondsAtSync - SecondsSinceSync, 0.f);
	M_ProgressBar->SetPercent(1.f - RemainingSeconds / M_TotalSeconds);
}

bool UW_CrewRepairProgress::GetIsValidProgressBar() const
{
	if (IsValid(M_ProgressBar))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ProgressBar"),
		TEXT("UW_CrewRepairProgress::GetIsValidProgressBar"),
		this
	);
	return false;
}
