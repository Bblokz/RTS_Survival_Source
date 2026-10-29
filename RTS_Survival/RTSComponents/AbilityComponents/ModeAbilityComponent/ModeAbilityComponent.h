// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityTypes.h"
#include "ModeAbilityComponent.generated.h"

class ASquadController;
class ICommands;
class UBehaviour;
class UBehaviourComp;
struct FUnitAbilityEntry;

USTRUCT(BlueprintType)
struct FModeAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EModeAbilityType ModeType = EModeAbilityType::DefaultSniperOverwatch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSubclassOf<UBehaviour>> BehavioursToApply;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoDeactivates = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ActiveTime = 0.0f;

	// Attempts to add the abilty to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;
};


/**
 * @brief Component that registers a toggleable mode ability and manages its behaviours.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UModeAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UModeAbilityComponent();

	void ActivateMode();
	void DeactivateMode();
	bool GetIsModeActive() const;
	EModeAbilityType GetModeAbilityType() const;
	// The entry this component adds to the command card; the unit variant screen reads it from Blueprint templates.
	FUnitAbilityEntry MakeAbilityEntry() const;

	/**
	 * @brief For components created at runtime (e.g. by a technology) instead of authored on a Blueprint.
	 * @param NewSettings Applied only before the component begins play, which adds the ability to the command card.
	 */
	void InitModeAbilitySettings(const FModeAbilitySettings& NewSettings);

protected:
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FModeAbilitySettings ModeAbilitySettings;

private:
	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	bool GetIsValidOwnerCommandsInterface() const;

	UPROPERTY()
	TWeakObjectPtr<UBehaviourComp> M_OwnerBehaviourComponent;
	bool GetIsValidOwnerBehaviourComponent() const;

	bool bM_IsModeActive = false;

	FTimerHandle M_AutoDeactivateTimerHandle;

	void BeginPlay_CheckSettings() const;
	void BeginPlay_AddAbility();
	void AddAbilityToSquad(ASquadController* Squad);
	void AddAbilityToCommands();

	void ClearAutoDeactivateTimer();
	void HandleAutoDeactivate();
	void DeactivateModeInternal(const bool bShouldNotifyCommand);
	void SwapModeAbilityOnOwner(const EAbilityID OldAbility, const EAbilityID NewAbility) const;

	FString GetDebugName() const;
	void NotifyDoneExecuting(const EAbilityID AbilityId) const;
};
