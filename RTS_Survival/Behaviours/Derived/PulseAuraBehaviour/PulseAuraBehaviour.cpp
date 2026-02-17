// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/Behaviours/Derived/PulseAuraBehaviour/PulseAuraBehaviour.h"

#include "Engine/World.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Utils/AOE/FRTS_AOE.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPulseAuraBehaviour::UPulseAuraBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::Timed;
	bM_UsesTick = true;
}

void UPulseAuraBehaviour::OnAdded(AActor* BehaviourOwner)
{
	Super::OnAdded(BehaviourOwner);

	if (not IsValid(BehaviourOwner))
	{
		return;
	}

	M_OwningActor = BehaviourOwner;
	M_OwningRTSComponent = BehaviourOwner->FindComponentByClass<URTSComponent>();
	if (GetIsValidOwningRTSComponent())
	{
		M_OwningPlayer = M_OwningRTSComponent->GetOwningPlayer();
	}

	if (UWorld* World = BehaviourOwner->GetWorld())
	{
		M_RadiusSubsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>();
	}

	ShowAuraRadius();
	ExecutePulse();
}

void UPulseAuraBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	Super::OnRemoved(BehaviourOwner);

	HideAuraRadius();
	M_OwningRTSComponent = nullptr;
	M_RadiusSubsystem = nullptr;
	M_OwningActor = nullptr;
	M_TimeSinceLastPulseSeconds = 0.f;
	M_OwningPlayer = 0;
}

void UPulseAuraBehaviour::OnTick(const float DeltaTime)
{
	Super::OnTick(DeltaTime);

	if (not GetIsValidOwningActor())
	{
		return;
	}

	RefreshPulseTimer(DeltaTime);
}

bool UPulseAuraBehaviour::GetShouldApplyToOverlappedActor(const AActor* OverlappedActor) const
{
	return IsValid(OverlappedActor);
}

void UPulseAuraBehaviour::RefreshPulseTimer(const float DeltaTime)
{
	const float SafePulseIntervalSeconds = FMath::Max(
		1.f,
		static_cast<float>(M_PulseAuraSettings.PulseIntervalSeconds));

	M_TimeSinceLastPulseSeconds += DeltaTime;
	if (M_TimeSinceLastPulseSeconds < SafePulseIntervalSeconds)
	{
		return;
	}

	M_TimeSinceLastPulseSeconds = 0.f;
	ExecutePulse();
}

void UPulseAuraBehaviour::ExecutePulse()
{
	if (not GetIsValidOwningActor())
	{
		return;
	}

	if (M_PulseAuraSettings.BehavioursToApply.IsEmpty())
	{
		return;
	}

	const FVector PulseOrigin = M_OwningActor->GetActorLocation() + M_PulseAuraSettings.RadiusOffset;
	const TWeakObjectPtr<UPulseAuraBehaviour> WeakThis(this);
	FRTS_AOE::ApplyToActorsAsync(
		M_OwningActor.Get(),
		PulseOrigin,
		M_PulseAuraSettings.Radius,
		GetResolvedOverlapRule(),
		[WeakThis](AActor& FoundActor)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			if (not WeakThis->GetShouldApplyToOverlappedActor(&FoundActor))
			{
				return;
			}

			WeakThis->ApplyBehavioursToTargetActor(FoundActor);
		});
}

void UPulseAuraBehaviour::ApplyBehavioursToTargetActor(AActor& TargetActor) const
{
	UBehaviourComp* TargetBehaviourComp = TargetActor.FindComponentByClass<UBehaviourComp>();
	if (not IsValid(TargetBehaviourComp))
	{
		return;
	}

	for (const TSubclassOf<UBehaviour>& BehaviourClassToApply : M_PulseAuraSettings.BehavioursToApply)
	{
		if (not IsValid(BehaviourClassToApply))
		{
			continue;
		}

		TargetBehaviourComp->AddBehaviour(BehaviourClassToApply);
	}
}

ETriggerOverlapLogic UPulseAuraBehaviour::GetResolvedOverlapRule() const
{
	if (M_OwningPlayer == 1)
	{
		return M_PulseAuraSettings.OverlapRule;
	}

	switch (M_PulseAuraSettings.OverlapRule)
	{
	case ETriggerOverlapLogic::OverlapPlayer:
		return ETriggerOverlapLogic::OverlapEnemy;
	case ETriggerOverlapLogic::OverlapEnemy:
		return ETriggerOverlapLogic::OverlapPlayer;
	default:
		return ETriggerOverlapLogic::OverlapBoth;
	}
}

void UPulseAuraBehaviour::ShowAuraRadius()
{
	if (not GetIsValidOwningActor() || not GetIsValidRadiusSubsystem())
	{
		return;
	}

	M_AuraRadiusId = M_RadiusSubsystem->CreateRTSRadius(
		M_OwningActor->GetActorLocation() + M_PulseAuraSettings.RadiusOffset,
		M_PulseAuraSettings.Radius,
		M_PulseAuraSettings.RadiusType,
		0.f);
	if (M_AuraRadiusId < 0)
	{
		return;
	}

	M_RadiusSubsystem->AttachRTSRadiusToActor(M_AuraRadiusId, M_OwningActor.Get(), M_PulseAuraSettings.RadiusOffset);
}

void UPulseAuraBehaviour::HideAuraRadius()
{
	if (M_AuraRadiusId < 0)
	{
		return;
	}

	if (not GetIsValidRadiusSubsystem())
	{
		M_AuraRadiusId = INDEX_NONE;
		return;
	}

	M_RadiusSubsystem->HideRTSRadiusById(M_AuraRadiusId);
	M_AuraRadiusId = INDEX_NONE;
}

bool UPulseAuraBehaviour::GetIsValidOwningActor() const
{
	if (M_OwningActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwningActor",
		"GetIsValidOwningActor",
		this);
	return false;
}

bool UPulseAuraBehaviour::GetIsValidOwningRTSComponent() const
{
	if (M_OwningRTSComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwningRTSComponent",
		"GetIsValidOwningRTSComponent",
		this);
	return false;
}

bool UPulseAuraBehaviour::GetIsValidRadiusSubsystem() const
{
	if (M_RadiusSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_RadiusSubsystem",
		"GetIsValidRadiusSubsystem",
		this);
	return false;
}

