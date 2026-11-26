// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionBase.h"

#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Missions/PlayerAirDrop/PlayerResourceSafetyNetSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UMissionBase::StartPlayerResourceSafetyNet()
{
	UPlayerResourceSafetyNetSubsystem* SafetyNet = UPlayerResourceSafetyNetSubsystem::Get(
		GetMissionManagerChecked());
	if (not IsValid(SafetyNet))
	{
		RTSFunctionLibrary::ReportError("Mission " + GetName() + ": no player resource safety net subsystem; the "
			"resource safety net does not run.");
		return;
	}
	// One mission owns the net at a time: a restart replaces the previous mission's ping binding.
	SafetyNet->OnDropSent.Clear();
	const float PingSeconds = PlayerResourceSafetyNet.MiniMapPingSeconds;
	SafetyNet->OnDropSent.AddWeakLambda(this, [this, PingSeconds](const FVector& DropCentre)
	{
		if (PingSeconds > 0.f)
		{
			ShowReinforcementsOnMiniMap(DropCentre, PingSeconds);
		}
	});
	// A mission instanced on a map before the setting existed can load it empty: the class default holds the stage's
	// purchases then.
	const UMissionBase* ClassDefaults = GetClass()->GetDefaultObject<UMissionBase>();
	const bool bUseClassDefaults = PlayerResourceSafetyNet.RequiredPurchases.IsEmpty() && IsValid(ClassDefaults);
	SafetyNet->StartSafetyNet(bUseClassDefaults ? ClassDefaults->PlayerResourceSafetyNet : PlayerResourceSafetyNet,
	                          this);
}

void UMissionBase::StopPlayerResourceSafetyNet()
{
	UPlayerResourceSafetyNetSubsystem* SafetyNet = UPlayerResourceSafetyNetSubsystem::Get(
		GetMissionManagerChecked());
	if (not IsValid(SafetyNet))
	{
		return;
	}
	SafetyNet->OnDropSent.Clear();
	SafetyNet->StopSafetyNet();
}

void UMissionBase::StopPlayerResourceSafetyNetIfOwner()
{
	const UWorld* World = GetWorld();
	UPlayerResourceSafetyNetSubsystem* SafetyNet = IsValid(World)
		                                               ? World->GetSubsystem<UPlayerResourceSafetyNetSubsystem>()
		                                               : nullptr;
	if (not IsValid(SafetyNet) || not SafetyNet->GetIsStartedBy(this))
	{
		return;
	}
	SafetyNet->OnDropSent.Clear();
	SafetyNet->StopSafetyNet();
}
