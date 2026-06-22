// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSTargetAcquisition.h"

#include "DrawDebugHelpers.h"

#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace RTSTargetAcquisitionDebug
{
	constexpr float DebugStringZOffset = 600.f;
	constexpr float DebugStringDuration = 6.f;

	FString GetStanceDebugString(const ERTSAggroBehaviour Stance)
	{
		switch (Stance)
		{
		case ERTSAggroBehaviour::Stance_HoldPosition: return TEXT("Hold Position");
		case ERTSAggroBehaviour::Stance_Aggressive: return TEXT("Aggressive");
		default: return TEXT("None");
		}
	}
}


bool URTSTargetAcquisition::EnsureIsValidGameUnitManager() const
{
	if (not M_GameUnitManager.IsValid())
	{
		const FString SafeOwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "";
		RTSFunctionLibrary::ReportError("No valid game unit manager on target acquisition component,"
			"for owner:  " + SafeOwnerName);
		return false;
	}
	return true;
}

bool URTSTargetAcquisition::EnsureIsValidRTSComponent() const
{
	if (not M_OwnerRTSComponent.IsValid())
	{
		const FString SafeOwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "";
		RTSFunctionLibrary::ReportError("No valid RTS component target acquisition component,"
			"for owner:  " + SafeOwnerName);
		return false;
	}
	return true;
}

const float URTSTargetAcquisition::TargetAcquisitionInterval = 6.29f;

URTSTargetAcquisition::URTSTargetAcquisition()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSTargetAcquisition::OnUnitIdleAndNoNewCommands()
{
}

ERTSAggroBehaviour URTSTargetAcquisition::GetEngagementStance() const
{
	return EngagementStance;
}

void URTSTargetAcquisition::SetEngagementStance(const ERTSAggroBehaviour NewStance)
{
	if (NewStance == EngagementStance)
	{
		return;
	}
	EngagementStance = NewStance;
	if constexpr (DeveloperSettings::Debugging::GTargetAcquisition_Compile_DebugSymbols)
	{
		DebugDrawAggroSearchState(GetOwnerRange() + GetAddedAggroRangeForOwningPlayer());
	}
	if (IsAggroTimerAllowed())
	{
		StartAggroTimer();
	}
	else
	{
		StopAggroTimer();
	}
}

void URTSTargetAcquisition::ActivateAcquisition()
{
	if (IsAggroTimerAllowed())
	{
		StartAggroTimer();
	}
}


void URTSTargetAcquisition::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitGameUnitManager();
	BeginPlay_InitRTSComponent();
}

void URTSTargetAcquisition::IssueAttackClosestVisibleTargetInAggroRange(AActor* TargetActor)
{
	ICommands* ICommandsOwner;
	if (not GetOwnerAsICommands(ICommandsOwner))
	{
		return;
	}
	ICommandsOwner->AttackActor(TargetActor, true);
}

ETargetPreference URTSTargetAcquisition::GetOwnerTargetPreference() const
{
	return ETargetPreference::None;
}

void URTSTargetAcquisition::OnTargetsFound(const TArray<AActor*>& Targets)
{
	if (not CanAggroEnemies())
	{
		return;
	}
	for (AActor* Target : Targets)
	{
		if (RTSFunctionLibrary::RTSIsVisibleTarget(Target, GetOwningPlayer()))
		{
			// preserves turret state changes; stores target in struct internally 	
			IssueAttackClosestVisibleTargetInAggroRange(Target);
			return;
		}
	}
}

int32 URTSTargetAcquisition::GetOwningPlayer() const
{
	if (not EnsureIsValidRTSComponent())
	{
		return 0;
	}
	return M_OwnerRTSComponent->GetOwningPlayer();
}

FVector URTSTargetAcquisition::GetOwnerLocation(bool& OutbValid) const
{
	if (not IsValid(GetOwner()))
	{
		OutbValid = false;
		return FVector::ZeroVector;
	}
	OutbValid = true;
	return GetOwner()->GetActorLocation();
}

void URTSTargetAcquisition::SearchForClosestTarget(const FVector& SearchOrigin, const float Range,
                                                   const ETargetPreference TargetPreference)
{
	if (not EnsureIsValidGameUnitManager())
	{
		return;
	}
	TWeakObjectPtr<URTSTargetAcquisition> WeakThis(this);
	// Request targets from the game unit manager
	M_GameUnitManager->RequestClosestTargets(
		SearchOrigin,
		Range,
		8,
		GetOwningPlayer(),
		TargetPreference,
		[WeakThis](const TArray<AActor*>& Targets)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTargetsFound(Targets);
			}
		});
}

bool URTSTargetAcquisition::GetOwnerAsICommands(ICommands*& OwnerICommands) const
{
	ICommands* CommandsInterface = Cast<ICommands>(GetOwner());
	if (not CommandsInterface)
	{
		RTSFunctionLibrary::ReportError("Unable to aggro to target as owner is not icommands!");
		return false;
	}
	OwnerICommands = CommandsInterface;
	return true;
}

bool URTSTargetAcquisition::CanAggroEnemies() const
{
	if (EngagementStance == ERTSAggroBehaviour::Stance_Aggressive)
	{
		return true;
	}
	return false;
}

float URTSTargetAcquisition::GetOwnerRange() const
{
	return 0.f;
}

bool URTSTargetAcquisition::IsAggroTimerAllowed() const
{
	if (EngagementStance == ERTSAggroBehaviour::Stance_Aggressive)
	{
		return true;
	}
	return false;
}

void URTSTargetAcquisition::StartAggroTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (World->GetTimerManager().IsTimerActive(M_AggroTimer))
	{
		// Idempotence.
		return;
	}
	TWeakObjectPtr<URTSTargetAcquisition> WeakThis(this);
	auto TimerLambda = [WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		TStrongObjectPtr<URTSTargetAcquisition> StrongThis(WeakThis.Get());
		bool bValidOwner = false;
		FVector OwnerLocation = StrongThis->GetOwnerLocation(bValidOwner);
		if (not bValidOwner)
		{
			return;
		}
		const float Range = StrongThis->GetOwnerRange() + StrongThis->GetAddedAggroRangeForOwningPlayer();
		const ETargetPreference TargetPref = StrongThis->GetOwnerTargetPreference();
		if (not StrongThis->IsAggroTimerAllowed())
		{
			StrongThis->StopAggroTimer();
			return;
		}
		if constexpr (DeveloperSettings::Debugging::GTargetAcquisition_Compile_DebugSymbols)
		{
			StrongThis->DebugDrawAggroSearchState(Range);
		}
		StrongThis->SearchForClosestTarget(
			OwnerLocation,
			Range,
			TargetPref
		);
	};
	FTimerDelegate TimerDel;
	TimerDel.BindLambda(TimerLambda);
	World->GetTimerManager().SetTimer(M_AggroTimer, TimerDel,
		TargetAcquisitionInterval,
		true);
}

void URTSTargetAcquisition::StopAggroTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_AggroTimer);
}

void URTSTargetAcquisition::BeginPlay_InitGameUnitManager()
{
	M_GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	// Error check.
	(void)EnsureIsValidGameUnitManager();
}

void URTSTargetAcquisition::BeginPlay_InitRTSComponent()
{
	AActor* TargetActor = GetOwner();
	if (not IsValid(TargetActor))
	{
		return;
	}
	M_OwnerRTSComponent = Cast<URTSComponent>(TargetActor->GetComponentByClass(URTSComponent::StaticClass()));
	(void)EnsureIsValidRTSComponent();
}

void URTSTargetAcquisition::DebugDrawAggroSearchState(const float AggroRange) const
{
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	const FString DebugString = FString::Printf(
		TEXT("Stance: %s\nAggro Range: %.0f"),
		*RTSTargetAcquisitionDebug::GetStanceDebugString(EngagementStance),
		AggroRange
	);
	const FVector DebugLocation = OwnerActor->GetActorLocation()
		+ FVector(0.f, 0.f, RTSTargetAcquisitionDebug::DebugStringZOffset);
	DrawDebugString(
		GetWorld(),
		DebugLocation,
		DebugString,
		nullptr,
		FColor::Cyan,
		RTSTargetAcquisitionDebug::DebugStringDuration,
		false
	);
}

float URTSTargetAcquisition::GetAddedAggroRangeForOwningPlayer() const
{
	if (GetOwningPlayer() == 1)
	{
		return DeveloperSettings::TargetAcquisition::AddedAggroRange;
	}

	return DeveloperSettings::TargetAcquisition::AddedAggroEnemyRange;
}
