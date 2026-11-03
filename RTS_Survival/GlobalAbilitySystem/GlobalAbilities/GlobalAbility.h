// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
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
#if RTS_WITH_MAP_TESTS
	friend struct FGlobalAbilityTestAccess;
#endif

public:
	UGlobalAbility();

	virtual UWorld* GetWorld() const override;

	void InitGlobalAbility(const int32 OwningPlayer, TWeakObjectPtr<UGlobalAbilitiesManager> GlobalAbilitiesManager,
	                       ACPPController*
	                       PlayerController);

	void OnClickedAbilityButton();
	void CancelAbilityActivation();
	void OnClickedAbilityLocation(const FVector& TargetLocation);
	void OnClickedAbilityLine(const FVector& LineStart, const FVector& LineEnd);

	virtual void ActivateAbility();

	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation);

	/**
	 * @brief Player execution of an ability aimed as a strip. Enemy AI and missions only give a point, so strip
	 * abilities also have to handle ExecuteAbilityAtLocation.
	 * @param LineStart Ground location the flight starts at.
	 * @param LineEnd Ground location the flight ends at.
	 */
	virtual void ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd);

	/**
	 * @brief Lets the aim preview show what the execution will really do for the strip the player is drawing.
	 * @param LineStart Ground location the flight would start at.
	 * @param LineEnd Ground location the flight would end at.
	 * @param OutDropPoints Ground locations where something will land; left empty when the ability has none.
	 */
	virtual void GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
	                                TArray<FVector>& OutDropPoints) const;

	EGlobalAbility GetAbilityType() const;
	const FGlobalAbilityAimSettings& GetAimSettings() const { return M_AimSettings; }
	const FGLobalAbilityCostState& GetAbilityCosts() const;
	const FGlobalAbilityUISettings& GetUISettings() const;

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

	// Checks and pays for a clicked execution; false cancels the activation.
	bool TryStartClickedExecution();
	void FinishClickedExecution();

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
