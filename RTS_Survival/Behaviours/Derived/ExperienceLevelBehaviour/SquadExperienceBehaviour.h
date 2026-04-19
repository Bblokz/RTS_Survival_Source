// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExperienceLevelBehaviour.h"

#include "SquadExperienceBehaviour.generated.h"

class ASquadController;
class ASquadUnit;

USTRUCT()
struct FSquadExperienceUnitMovementState
{
	GENERATED_BODY()

	UPROPERTY()
	float M_BaseMaxWalkSpeed = 0.f;

	UPROPERTY()
	float M_AppliedMultiplier = 1.f;
};

/**
 * @brief Experience behaviour for squads that applies max-walk-speed multipliers to loaded squad units.
 */
UCLASS()
class RTS_SURVIVAL_API USquadExperienceBehaviour : public UExperienceLevelBehaviour
{
	GENERATED_BODY()

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnStack(UBehaviour* StackedBehaviour) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Experience Multipliers")
	float SquadMovementSpeedMultiplier = 1.0f;

private:
	void SetupInitializationForOwner();
	void RegisterSquadFullyLoadedCallback(ASquadController* SquadController);
	void HandleSquadDataLoaded();
	void ApplySquadSpeedMultiplier();
	void ApplyMultiplierToSquadUnit(ASquadUnit* SquadUnit);
	void RestoreSquadSpeedMultiplier();
	void ClearCachedSquadData();

	bool TryGetSquadController(ASquadController*& OutSquadController);
	bool TryGetSquadUnits(TArray<ASquadUnit*>& OutSquadUnits);
	bool GetIsValidSquadController() const;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SquadController;

	UPROPERTY()
	TMap<TWeakObjectPtr<ASquadUnit>, FSquadExperienceUnitMovementState> M_MovementStateBySquadUnit;

	bool bM_HasRegisteredSquadLoadedCallback = false;
	bool bM_HasAppliedMovementAtLeastOnce = false;
	bool bM_IsBehaviourActive = false;
};
