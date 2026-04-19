// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadExperienceBehaviour.h"

#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "GameFramework/CharacterMovementComponent.h"

void USquadExperienceBehaviour::OnAdded(AActor* BehaviourOwner)
{
	bM_IsBehaviourActive = true;
	SetupInitializationForOwner();
	Super::OnAdded(BehaviourOwner);
}

void USquadExperienceBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	RestoreSquadSpeedMultiplier();
	ClearCachedSquadData();
	bM_IsBehaviourActive = false;
	bM_HasAppliedMovementAtLeastOnce = false;
	Super::OnRemoved(BehaviourOwner);
}

void USquadExperienceBehaviour::OnStack(UBehaviour* StackedBehaviour)
{
	Super::OnStack(StackedBehaviour);

	SetupInitializationForOwner();
	if (not bM_HasAppliedMovementAtLeastOnce)
	{
		return;
	}

	ApplySquadSpeedMultiplier();
}

void USquadExperienceBehaviour::SetupInitializationForOwner()
{
	ASquadController* SquadController = nullptr;
	if (not TryGetSquadController(SquadController))
	{
		return;
	}

	if (SquadController->GetIsSquadFullyLoaded())
	{
		HandleSquadDataLoaded();
		return;
	}

	RegisterSquadFullyLoadedCallback(SquadController);
}

void USquadExperienceBehaviour::RegisterSquadFullyLoadedCallback(ASquadController* SquadController)
{
	if (bM_HasRegisteredSquadLoadedCallback)
	{
		return;
	}

	if (not IsValid(SquadController))
	{
		return;
	}

	const TWeakObjectPtr<USquadExperienceBehaviour> WeakThis(this);
	const TFunction<void()> SquadLoadedCallback = [WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		USquadExperienceBehaviour* StrongThis = WeakThis.Get();
		StrongThis->HandleSquadDataLoaded();
	};

	SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(SquadLoadedCallback, this);
	bM_HasRegisteredSquadLoadedCallback = true;
}

void USquadExperienceBehaviour::HandleSquadDataLoaded()
{
	if (not bM_IsBehaviourActive)
	{
		return;
	}

	if (not GetIsValidSquadController())
	{
		return;
	}

	ApplySquadSpeedMultiplier();
	bM_HasAppliedMovementAtLeastOnce = true;
}

void USquadExperienceBehaviour::ApplySquadSpeedMultiplier()
{
	if (FMath::IsNearlyEqual(SquadMovementSpeedMultiplier, 1.f))
	{
		return;
	}

	TArray<ASquadUnit*> SquadUnits;
	if (not TryGetSquadUnits(SquadUnits))
	{
		return;
	}

	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		ApplyMultiplierToSquadUnit(SquadUnit);
	}
}

void USquadExperienceBehaviour::ApplyMultiplierToSquadUnit(ASquadUnit* SquadUnit)
{
	if (not IsValid(SquadUnit))
	{
		return;
	}

	UCharacterMovementComponent* CharacterMovementComponent = SquadUnit->GetCharacterMovement();
	if (not IsValid(CharacterMovementComponent))
	{
		return;
	}

	const TWeakObjectPtr<ASquadUnit> SquadUnitPtr = SquadUnit;
	FSquadExperienceUnitMovementState& MovementState = M_MovementStateBySquadUnit.FindOrAdd(SquadUnitPtr);
	if (FMath::IsNearlyZero(MovementState.M_BaseMaxWalkSpeed))
	{
		MovementState.M_BaseMaxWalkSpeed = CharacterMovementComponent->MaxWalkSpeed;
	}

	MovementState.M_AppliedMultiplier *= SquadMovementSpeedMultiplier;
	CharacterMovementComponent->MaxWalkSpeed = MovementState.M_BaseMaxWalkSpeed * MovementState.M_AppliedMultiplier;
}

void USquadExperienceBehaviour::RestoreSquadSpeedMultiplier()
{
	for (auto It = M_MovementStateBySquadUnit.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<ASquadUnit> SquadUnitPtr = It.Key();
		const FSquadExperienceUnitMovementState MovementState = It.Value();
		if (not SquadUnitPtr.IsValid())
		{
			continue;
		}

		UCharacterMovementComponent* CharacterMovementComponent = SquadUnitPtr->GetCharacterMovement();
		if (not IsValid(CharacterMovementComponent))
		{
			continue;
		}

		CharacterMovementComponent->MaxWalkSpeed = MovementState.M_BaseMaxWalkSpeed;
	}
}

void USquadExperienceBehaviour::ClearCachedSquadData()
{
	M_MovementStateBySquadUnit.Empty();
	M_SquadController = nullptr;
	bM_HasRegisteredSquadLoadedCallback = false;
}

bool USquadExperienceBehaviour::TryGetSquadController(ASquadController*& OutSquadController)
{
	OutSquadController = M_SquadController.Get();
	if (IsValid(OutSquadController))
	{
		return true;
	}

	OutSquadController = Cast<ASquadController>(GetOwningActor());
	if (not IsValid(OutSquadController))
	{
		return false;
	}

	M_SquadController = OutSquadController;
	return true;
}

bool USquadExperienceBehaviour::TryGetSquadUnits(TArray<ASquadUnit*>& OutSquadUnits)
{
	OutSquadUnits.Reset();

	ASquadController* SquadController = nullptr;
	if (not TryGetSquadController(SquadController))
	{
		return false;
	}

	if (not SquadController->GetIsSquadFullyLoaded())
	{
		return false;
	}

	OutSquadUnits = SquadController->GetSquadUnitsChecked();
	return OutSquadUnits.Num() > 0;
}

bool USquadExperienceBehaviour::GetIsValidSquadController() const
{
	if (M_SquadController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_SquadController",
		"GetIsValidSquadController",
		GetOwningActor()
	);
	return false;
}
