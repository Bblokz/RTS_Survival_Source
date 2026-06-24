// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "UObject/Object.h"
#include "GlobalAbility.generated.h"

class ACPPController;
class UGlobalAbilitiesManager;
class UNiagaraComponent;
class UGlobalAbility;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGlobalAbilityEnded, UGlobalAbility*);

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

	virtual UWorld* GetWorld() const override;

	void InitGlobalAbility(const int32 OwningPlayer, TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager,
	                       ACPPController*
	                       PlayerController);

	void OnClickedAbilityButton();
	void CancelAbilityActivation();
	void OnClickedAbilityLocation(const FVector& TargetLocation);

	virtual void ActivateAbility();

	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation);
	EGlobalAbility GetAbilityType() const;

	FOnGlobalAbilityEnded OnGlobalAbilityEnded;

protected:
	int32 GetOwningPlayer() const;
	// For custom per ability logic on init.
	virtual void OnInit(AActor* WorldContextActor);
	[[nodiscard]] bool GetIsValidGlobalAbilityManager() const;
	UGlobalAbilitiesManager* GetGlobalAbilityManager() const;
	void BroadcastAbilityEnded();
	void ResetAbilityEndedBroadcast();
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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGlobalAbilityMarker M_AbilityMarker;
	
	virtual void BeginDestroy() override;
	
private:
	UPROPERTY(Transient)
	int32 M_OwningPlayer = INDEX_NONE;
	
	void CreateMarker(const FVector& ExecuteLocation);
	void OnValidMarkerSpawned(UNiagaraComponent* MarkerEffect);
	void DestroyMarker(UNiagaraComponent* MarkerEffect);
	void DestroyAllMarkers();
	void RemoveTrackedMarker(UNiagaraComponent* MarkerEffect);

	// Multiple mission-triggered executions can overlap, so every marker must be tracked until its own cleanup timer fires.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> M_SpawnedMarkerEffects;

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

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	bool bM_HasBroadcastAbilityEnded = false;
	[[nodiscard]] bool EnsureIsValidPlayerController() const;
};
