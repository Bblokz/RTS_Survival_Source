// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "UObject/Object.h"
#include "GlobalAbility.generated.h"

UENUM(blueprintType)
enum class EGlobalAbilityState : uint8
{
	NotActivated,
	Activated,
};

/**
 * UPROPERTY(Instanced) tells Unreal to duplicate/own a unique subobject for that property 
 * and implies inline editing/export behavior,
 * while UCLASS(EditInlineNew) allows the object to be created from the Details panel
 * instead of only referencing an existing asset.
 * DefaultToInstanced makes instances of that class considered instanced by default.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGlobalAbility : public UObject
{
	GENERATED_BODY()

	friend class UGlobalAbilitiesManager;

public:
	UGlobalAbility();

	void InitGlobalAbility(const int32 OwningPlayer, TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager,
	                       ACPPController*
	                       PlayerController);

	void OnClickedAbilityButton();
	void CancelAbilityActivation();
	void OnClickedAbilityLocation(const FVector& TargetLocation);

	virtual void ActivateAbility();

	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGlobalAbility M_AbilityType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGLobalAbilityCostState M_AbilityCosts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGlobalAbilityRequirements M_AbilityRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGlobalAbilityAimSettings M_AimSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGlobalAbilitySoundSettings M_AbilitySoundSettings;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGlobalAbilityUISettings M_UISettings;

private:
	UPROPERTY(Transient)
	int32 M_OwningPlayer = INDEX_NONE;

	bool IsOwnedByPlayer() const;

	UPROPERTY(Transient)
	EGlobalAbilityState M_AbilityState = EGlobalAbilityState::NotActivated;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGlobalAbilitiesManager> M_GlobalAbilitiesManager;
	[[nodiscard]] bool EnsureIsValidGlobalAbilityManager() const;

	bool IsBlocked();
	bool IsBlockedByRequirements();
	bool IsBlockedByCosts();
	bool IsBlockedByCooldown();
};
