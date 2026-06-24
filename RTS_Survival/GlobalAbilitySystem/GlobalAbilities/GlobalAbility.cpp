// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbility.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UGlobalAbility::UGlobalAbility()
{
}

UWorld* UGlobalAbility::GetWorld() const
{
	if (M_PlayerController.IsValid())
	{
		return M_PlayerController.Get()->GetWorld();
	}

	if (M_GlobalAbilitiesManager.IsValid())
	{
		return M_GlobalAbilitiesManager.Get()->GetWorld();
	}

	return Super::GetWorld();
}

void UGlobalAbility::InitGlobalAbility(const int32 OwningPlayer,
                                       TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager,
                                       ACPPController* PlayerController)
{
	M_OwningPlayer = OwningPlayer;
	M_GlobalAbilitiesManager = GlobalAbilitiesManager;
	M_PlayerController = PlayerController;
	(void)EnsureIsValidGlobalAbilityManager();
}

void UGlobalAbility::OnClickedAbilityButton()
{
	ActivateAbility();
}

void UGlobalAbility::CancelAbilityActivation()
{
	M_AbilityState = EGlobalAbilityState::NotActivated;
}

void UGlobalAbility::OnClickedAbilityLocation(const FVector& TargetLocation)
{
	if (not EnsureIsValidGlobalAbilityManager())
	{
		return;
	}
	if (IsBlocked())
	{
		CancelAbilityActivation();
		return;
	}
	if (IsOwnedByPlayer() && not M_GlobalAbilitiesManager.Get()->TryPayForAbility(this))
	{
		CancelAbilityActivation();
		return;
	}
	ResetAbilityEndedBroadcast();
	ExecuteAbilityAtLocation(TargetLocation);
	M_AbilityState = EGlobalAbilityState::NotActivated;
	M_GlobalAbilitiesManager.Get()->OnAbilityFinishedExecuting(this);
}

void UGlobalAbility::ActivateAbility()
{
	if (M_AbilityState == EGlobalAbilityState::Activated)
	{
		// Already Active!
		return;
	}
	if (IsBlocked())
	{
		return;
	}
	M_AbilityState = EGlobalAbilityState::Activated;
	if (IsOwnedByPlayer() && EnsureIsValidPlayerController())
	{
		M_PlayerController.Get()->OnGlobaAbilityActivated(M_AimSettings, M_AbilitySoundSettings, this);
	}
}

void UGlobalAbility::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	CreateMarker(TargetLocation);
}

EGlobalAbility UGlobalAbility::GetAbilityType() const
{
	return M_AbilityType;
}

int32 UGlobalAbility::GetOwningPlayer() const
{
	return M_OwningPlayer;
}

void UGlobalAbility::OnInit(AActor* WorldContextActor)
{
}

void UGlobalAbility::BeginDestroy()
{
	DestroyAllMarkers();
	UObject::BeginDestroy();
}

void UGlobalAbility::CreateMarker(const FVector& ExecuteLocation)
{
	if (not IsValid(M_AbilityMarker.MarkerEffect)
		|| (M_AbilityMarker.EffectTotalTime <= 0))
	{
		// not all abilities use markers.
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	// Spawns, plays once, and is explicitly removed by a per-marker timer so overlapping ability executions cannot orphan it.
	UNiagaraComponent* SpawnedMarkerEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		M_AbilityMarker.MarkerEffect,
		ExecuteLocation, // World Position
		FRotator::ZeroRotator, // World Rotation
		FVector(1.0f), // Scale
		false, // bAutoDestroy
		true // bAutoActivate
	);
	if (not IsValid(SpawnedMarkerEffect))
	{
		return;
	}

	M_SpawnedMarkerEffects.Add(SpawnedMarkerEffect);
	OnValidMarkerSpawned(SpawnedMarkerEffect);

	TWeakObjectPtr<UGlobalAbility> WeakThis(this);
	TWeakObjectPtr<UNiagaraComponent> WeakMarkerEffect(SpawnedMarkerEffect);
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindLambda([WeakThis, WeakMarkerEffect]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		UGlobalAbility* StrongThis = WeakThis.Get();
		StrongThis->DestroyMarker(WeakMarkerEffect.Get());
	});

	FTimerHandle MarkerTimerHandle;
	World->GetTimerManager().SetTimer(MarkerTimerHandle, TimerDelegate, M_AbilityMarker.EffectTotalTime, false);
}

void UGlobalAbility::OnValidMarkerSpawned(UNiagaraComponent* MarkerEffect)
{
	if (not IsValid(MarkerEffect))
	{
		return;
	}

	MarkerEffect->SetColorParameter("MarkerColor", M_AbilityMarker.MarkerColor);
}

void UGlobalAbility::DestroyMarker(UNiagaraComponent* MarkerEffect)
{
	if (not IsValid(MarkerEffect))
	{
		RemoveTrackedMarker(MarkerEffect);
		return;
	}

	MarkerEffect->DestroyComponent();
	RemoveTrackedMarker(MarkerEffect);
}

void UGlobalAbility::DestroyAllMarkers()
{
	for (int32 MarkerIndex = M_SpawnedMarkerEffects.Num() - 1; MarkerIndex >= 0; --MarkerIndex)
	{
		UNiagaraComponent* MarkerEffect = M_SpawnedMarkerEffects[MarkerIndex];
		if (IsValid(MarkerEffect))
		{
			MarkerEffect->DestroyComponent();
		}
	}

	M_SpawnedMarkerEffects.Empty();
}

void UGlobalAbility::RemoveTrackedMarker(UNiagaraComponent* MarkerEffect)
{
	M_SpawnedMarkerEffects.RemoveAll([MarkerEffect](const TObjectPtr<UNiagaraComponent>& TrackedMarkerEffect)
	{
		return not IsValid(TrackedMarkerEffect) || TrackedMarkerEffect == MarkerEffect;
	});
}

bool UGlobalAbility::IsOwnedByPlayer() const
{
	return M_OwningPlayer == 1;
}

bool UGlobalAbility::GetIsValidGlobalAbilityManager() const
{
	return EnsureIsValidGlobalAbilityManager();
}

UGlobalAbilitiesManager* UGlobalAbility::GetGlobalAbilityManager() const
{
	return M_GlobalAbilitiesManager.Get();
}

void UGlobalAbility::BroadcastAbilityEnded()
{
	if (bM_HasBroadcastAbilityEnded)
	{
		return;
	}

	bM_HasBroadcastAbilityEnded = true;
	OnGlobalAbilityEnded.Broadcast(this);
}

void UGlobalAbility::ResetAbilityEndedBroadcast()
{
	bM_HasBroadcastAbilityEnded = false;
}

bool UGlobalAbility::EnsureIsValidGlobalAbilityManager() const
{
	if (not M_GlobalAbilitiesManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GlobalAbilitiesManager"),
			TEXT("UGlobalAbility::EnsureIsValidGlobalAbilityManager"),
			this
		);
		return false;
	}
	return true;
}

bool UGlobalAbility::IsBlocked()
{
	if (IsBlockedByRequirements() || IsBlockedByCooldown() || IsBlockedByCosts())
	{
		return true;
	}
	return false;
}

bool UGlobalAbility::IsBlockedByRequirements()
{
	if (not EnsureIsValidGlobalAbilityManager())
	{
		return true;
	}
	return not M_GlobalAbilitiesManager.Get()->QueryRequirementForAbility(this);
}

bool UGlobalAbility::IsBlockedByCosts()
{
	if (not IsOwnedByPlayer())
	{
		// Enemy controller does not use costs.
		return false;
	}
	if (not EnsureIsValidGlobalAbilityManager())
	{
		return true;
	}
	return not M_GlobalAbilitiesManager.Get()->QueryCostsForAbility(this);
}

bool UGlobalAbility::EnsureIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("UGlobalAbility::EnsureIsValidPlayerController"),
		this
	);
	return false;
}

bool UGlobalAbility::IsBlockedByCooldown()
{
	return M_AbilityCosts.CoolDownRemaining > 0;
}
