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
	UWorld* World  =GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_SpawnedMarkerTimer);
	}
	if (IsValid(M_SpawnedMarkerEffect))
	{
		DestroyMarker();
	}
	
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
	// Spawns, plays once, and completely deletes itself from memory when finished
	M_SpawnedMarkerEffect =  UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), 
		M_AbilityMarker.MarkerEffect, 
		ExecuteLocation, // World Position
		FRotator::ZeroRotator, // World Rotation
		FVector(1.0f), // Scale
		false, // bAutoDestroy 
		true // bAutoActivate
	);
	UWorld* World  =GetWorld();
	if (IsValid(M_SpawnedMarkerEffect) && World)
	{
		OnValidMarkerSpawned();
		TWeakObjectPtr<UGlobalAbility> WeakThis(this);
		FTimerDelegate TimderDel;
		auto Lambda = [WeakThis]()->void
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			TObjectPtr<UGlobalAbility> StrongThis = WeakThis.Get();
			StrongThis->DestroyMarker();
		};
		TimderDel.BindLambda(Lambda);
		World->GetTimerManager().SetTimer(M_SpawnedMarkerTimer, TimderDel, M_AbilityMarker.EffectTotalTime, false);
	}
}

void UGlobalAbility::OnValidMarkerSpawned()
{
	M_SpawnedMarkerEffect->SetColorParameter("MarkerColor", M_AbilityMarker.MarkerColor);
}

void UGlobalAbility::DestroyMarker()
{
	if (not IsValid(M_SpawnedMarkerEffect))
	{
		return;
	}
	M_SpawnedMarkerEffect->DestroyComponent();
	M_SpawnedMarkerEffect = nullptr;
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
