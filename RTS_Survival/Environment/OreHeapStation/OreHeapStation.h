// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CaptureMechanic/CaptureActor/CaptureActor.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageOwner.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "OreHeapStation.generated.h"

class AOreHeapStation;
class UFowComp;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOreHeapStationStateChanged, AOreHeapStation*, Station,
                                               bool, bIsHeld, int32, Player);

/** Starts neutral (owner 0, not in the unit manager); the station moves it to the capturer and back. */
UCLASS()
class RTS_SURVIVAL_API UOreHeapStationRTSComponent : public URTSComponent
{
	GENERATED_BODY()

public:
	UOreHeapStationRTSComponent();
};

/** Opens the protected InitResourceDropOff to the station (Blueprint graphs can call it, C++ owners cannot). */
UCLASS()
class RTS_SURVIVAL_API UOreHeapStationDropOff : public UResourceDropOff
{
	GENERATED_BODY()

public:
	void InitStationDropOff(const TMap<ERTSResourceType, int32>& MaxDropOffPerResource,
	                        UMeshComponent* OwnerMeshComponent, const FName DropOffSocketName);
};

USTRUCT(BlueprintType)
struct FOreHeapStationSettings
{
	GENERATED_BODY()

	// Resource the station takes from harvesters (the heap's metal).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation")
	ERTSResourceType DropOffResource = ERTSResourceType::Resource_Metal;

	// Storage the held station adds to the player's cap. Lost again, with what is stored in it, when it is knocked out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation", meta = (ClampMin = "1"))
	int32 StorageCapacity = 600;

	// Health of a held station. Damage only counts while the player holds it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation", meta = (ClampMin = "1.0"))
	float HeldHealth = 2500.f;

	// Player vision around the held station, in cm; 0 = none.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation", meta = (ClampMin = "0.0"))
	float VisionRadius = 1800.f;

	// Owner the knocked-out station falls to (the Soviets have retaken it): the pressure director then skips it.
	// 0 keeps the capturer as owner.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation")
	int32 KnockedOutOwner = 2;

	// Components with this tag show only while the station is held (the flag or panel).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation|Look")
	FName HeldTag = TEXT("HeapStationHeld");

	// Components with this tag show only while the station is knocked out (rubble, scorch).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation|Look")
	FName KnockedOutTag = TEXT("HeapStationKnockedOut");

	// Components with this tag hide while the station is knocked out (the parts that break).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation|Look")
	FName IntactTag = TEXT("HeapStationIntact");
};

/**
 * @brief Spoil-heap ore loading station (Ore Mountains): infantry capture it, then it is a forward drop-off for the
 * heap's metal. While held it is a Soviet target (registered in the unit manager) with its own health; at zero it is
 * knocked out instead of destroyed: the drop-off switches off, its storage goes, and the player can capture it again.
 * Place BP_OreHeapStation; the Blueprint needs no graph. See Docs/Campaign/OreMountainsExploration.md.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AOreHeapStation : public ACaptureActor, public IResourceStorageOwner
{
	GENERATED_BODY()

public:
	AOreHeapStation(const FObjectInitializer& ObjectInitializer);

	// ICaptureInterface
	// Held or being captured: more units than any squad has, so the order is refused instead of costing units.
	virtual int32 GetCaptureUnitAmountNeeded() const override;
	virtual void OnCaptureByPlayer(const int32 Player) override;
	// The bin carves the navmesh, so capturing units walk to the drop-off point in front of it, not to the pivot.
	virtual FVector GetCaptureLocationClosestTo(const FVector& FromLocation) override;
	// ~ICaptureInterface

	// IResourceStorageOwner
	virtual void OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType) override;
	virtual void OnResourceStorageEmpty() override;
	// ~IResourceStorageOwner

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	/** @brief Makes the station Player's (called when a capture completes; a mission may call it to hand it over). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "HeapStation")
	void OnStationCaptured(const int32 Player);

	/** @brief Knocks the held station out as if its health ran out (missions, tests). */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "HeapStation")
	void KnockOutStation();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "HeapStation")
	bool GetIsHeld() const { return bM_IsHeld; }

	/** @return The holder, or INDEX_NONE while nobody holds it. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "HeapStation")
	int32 GetHoldingPlayer() const { return M_HoldingPlayer; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "HeapStation")
	float GetStationHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "HeapStation", meta = (WorldContext = "WorldContextObject"))
	static TArray<AOreHeapStation*> GetAllHeapStations(const UObject* WorldContextObject);

	// Fires on every capture (bIsHeld true) and every knock-out (false, Player = the former holder).
	UPROPERTY(BlueprintAssignable, Category = "HeapStation")
	FOnOreHeapStationStateChanged OnHeapStationStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "HeapStation")
	void BP_OnStationStateChanged(const bool bIsHeld, const int32 Player);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HeapStation")
	FOreHeapStationSettings StationSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeapStation")
	TObjectPtr<USceneComponent> StationRoot;

	// The station look; blocks units and every player's shots. Placeholder mesh until a concept is picked.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeapStation")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	// Where harvesters unload: no mesh, no collision; move it to the side the harvesters come from.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeapStation")
	TObjectPtr<UStaticMeshComponent> DropOffPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeapStation")
	TObjectPtr<UOreHeapStationRTSComponent> StationRTSComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeapStation")
	TObjectPtr<UOreHeapStationDropOff> StationDropOff;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HeapStation")
	TObjectPtr<UFowComp> StationFowComp;

private:
	void BeginPlay_SetupStationMesh() const;
	void BeginPlay_CacheTaggedComponents();
	void ApplyLook() const;
	void SetTaggedVisible(const TArray<TObjectPtr<USceneComponent>>& Components, const bool bVisible) const;
	void InitDropOffOnce();
	void SetDropOffHeld(const bool bHeld) const;
	void SetTargetRegistered(const bool bRegister, const int32 Player);
	void StartStationVision();
	void StopStationVision() const;
	[[nodiscard]] bool GetIsValidStationRTSComponent() const;
	[[nodiscard]] bool GetIsValidStationDropOff() const;

	bool bM_IsHeld = false;
	bool bM_IsCaptureInProgress = false;
	bool bM_IsDropOffInitialised = false;
	// Set on the first capture: from then on a station that is not held shows as knocked out.
	bool bM_WasEverHeld = false;
	bool bM_IsRegisteredTarget = false;
	int32 M_HoldingPlayer = INDEX_NONE;
	int32 M_RegisteredTargetPlayer = INDEX_NONE;

	// Mirrors ACaptureActor's capture timer: the base only reports completion to Blueprint (BP_OnCapturedByPlayer).
	FTimerHandle M_StationCaptureTimerHandle;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> M_HeldComponents;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> M_KnockedOutComponents;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> M_IntactComponents;
};
