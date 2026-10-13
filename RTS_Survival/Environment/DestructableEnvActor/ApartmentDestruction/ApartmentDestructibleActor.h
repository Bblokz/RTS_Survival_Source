// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ApartmentCollectionAttributes.h"
#include "ApartmentFootprint.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Environment/DestructableEnvActor/CrushDestructionTypes/ECrushDestructionType.h"
#include "RTS_Survival/Interfaces/SelectionProxyOwner.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverSocketSource.h"

#include "ApartmentDestructibleActor.generated.h"

class AApartmentGarrisonSelectable;
class ASquadController;
class ASquadUnit;
class UApartmentFootprintNavModifier;
class UBoxComponent;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UNavArea;

/** @brief Weapon damage each kind of part takes before it breaks out of the building. */
USTRUCT(BlueprintType)
struct FApartmentPartHealth
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float ExteriorWall = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float InteriorWall = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float Floor = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float Roof = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float Stairs = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float Detail = 60.f;

	float GetHealth(EApartmentPartType PartType) const;
};

/** @brief Tuning of how weapon hits break parts out of an apartment geometry collection. */
USTRUCT(BlueprintType)
struct FApartmentDestructionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FApartmentPartHealth PartHealth;

	// Parts within this distance of a hit, other than the part that was hit, take a share of the damage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float SplashRadius = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SplashDamageShare = 0.35f;

	// Speed given to a part when it breaks loose, along the shot direction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float BreakingPieceSpeed = 450.f;

	// Everything below this height above the actor's origin is pinned, so the building cannot topple as a whole.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float AnchoredGroundStoreyHeight = 310.f;
};

/** @brief Tuning of how vehicles break an apartment by driving into it. */
USTRUCT(BlueprintType)
struct FApartmentCrushSettings
{
	GENERATED_BODY()

	// Which vehicles break parts by driving into the building; the navigation areas decide who can get there at all.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush")
	ECrushDestructionType CrushDestructionType = ECrushDestructionType::HeavyTank;

	// Damage dealt to every part a vehicle touches each crush update.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush", meta = (ClampMin = "0.0"))
	float DamagePerUpdate = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush", meta = (ClampMin = "0.02"))
	float UpdateInterval = 0.1f;

	// Speed given to the parts a vehicle pushes over, along its direction of travel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crush", meta = (ClampMin = "0.0"))
	float PushSpeed = 350.f;
};

/**
 * @brief One cover position of an apartment, the counterpart of a mesh socket on other cover objects. The name
 * decides the kind of cover: "cargo" a firing step (trench cover), "crouch", "standing" or "prone".
 */
USTRUCT(BlueprintType)
struct FApartmentCoverSocket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	FName SocketName = TEXT("crouch");

	// Where the soldier holds, relative to the actor; he looks along the forward (X) axis.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover", meta = (MakeEditWidget = true))
	FTransform LocalTransform = FTransform::Identity;

	// Slots (as numbered in the collection) the position depends on: the wall in front of the soldier and the
	// floor under him. When one of them breaks the socket is no longer offered. Left empty, the parts right in
	// front of and under the socket are looked up when the game starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	TArray<int32> SupportSlotIds;
};

/** @brief How the player empties an apartment its soldiers are in. */
USTRUCT(BlueprintType)
struct FApartmentGarrisonSettings
{
	GENERATED_BODY()

	// The front door, relative to the actor, with the forward (X) axis pointing out into the street. Soldiers
	// ordered out of the building gather in front of it. Left at identity, they gather in front of the building.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison", meta = (MakeEditWidget = true))
	FTransform ExitLocalTransform = FTransform::Identity;

	// How far in front of the door the first squad gathers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison", meta = (ClampMin = "0.0"))
	float ExitGatherDistance = 350.f;

	// Each further squad gathers this much further out, so they do not stand in one another.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison", meta = (ClampMin = "0.0"))
	float ExitSquadSpacing = 220.f;
};

/** @brief Who of the player is inside an apartment right now, and the actor the player selects it through. */
USTRUCT()
struct FApartmentGarrisonState
{
	GENERATED_BODY()

	// Soldiers of the player that hold one of the building's cover sockets.
	UPROPERTY()
	TArray<TWeakObjectPtr<ASquadUnit>> M_Soldiers;

	// Exists only while M_Soldiers is not empty.
	UPROPERTY()
	TObjectPtr<AApartmentGarrisonSelectable> M_Selectable;

	// The selectable is removed a tick after the last soldier left: he may leave from inside its own command.
	FTimerHandle M_UpdateSelectableTimerHandle;
};

/**
 * @brief Base of the destructible apartment Blueprints: set ApartmentCollection to a collection built with
 * RTS.Apartment.BuildGeometryCollection (pivot on the ground inside the footprint) and list its CoverSockets.
 * Weapons and vehicles break parts off by part health, the footprint marks the navigation mesh by itself, and
 * a cover socket is withdrawn as soon as a part it depends on is gone. While the player has soldiers on its
 * sockets the building can be selected and ordered to send them out of the door (Exit Cargo).
 */
UCLASS()
class RTS_SURVIVAL_API AApartmentDestructibleActor : public AActor, public IRTSCoverSocketSource,
                                                     public IRTSSelectionProxyOwner
{
	GENERATED_BODY()

public:
	AApartmentDestructibleActor();

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	virtual void GetUsableCoverSockets(TArray<FRTSActorCoverSocket>& OutCoverSockets) const override;
	virtual void OnSoldierCoverSocketChanged(ASquadUnit* SquadUnit, const bool bIsOnSocket) override;

	virtual AActor* GetSelectionProxyActor() const override;

	/** @brief Orders every squad of the player that has a soldier inside to walk out and gather at the door. */
	void EvacuateGarrison();

	// Soldiers of the player that hold a cover socket of this building.
	int32 GetGarrisonedSoldierCount() const;

	/**
	 * @brief Damages every part of the building that reaches into the box, as a vehicle of that size would.
	 * @param WorldBox The volume the vehicle occupies.
	 * @param PushDirection Direction the vehicle travels in; broken parts are pushed along it.
	 * @return The number of standing parts the box touched.
	 */
	int32 CrushInsideBox(const FBox& WorldBox, const FVector& PushDirection);

	/** @brief Recomputes the footprint from the collection; call after assigning another collection from script. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Navigation")
	void RebuildFootprint();

	int32 GetUsableCoverSocketCount() const;
	int32 GetStandingPartCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// The building; applied to the geometry component whenever the actor is constructed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Apartment")
	TObjectPtr<UGeometryCollection> ApartmentCollection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FApartmentDestructionSettings DestructionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FApartmentCrushSettings CrushSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover")
	TArray<FApartmentCoverSocket> CoverSockets;

	// How far in front of a cover socket, at ground level, the soldier leaves the navigation mesh to take it.
	// Must reach from the socket through the wall to where infantry can walk.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cover", meta = (ClampMin = "0.0"))
	float CoverApproachDistance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison")
	FApartmentGarrisonSettings GarrisonSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	FApartmentFootprintSettings FootprintSettings;

	// Area of the band around the walls; the default keeps light and medium vehicles out and lets infantry in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSoftClassPtr<UNavArea> OuterBandNavArea;

	// Area of the walls and the rooms right behind them; the default lets only heavy tanks in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSoftClassPtr<UNavArea> InnerRingNavArea;

	// Area of everything deeper inside; the default blocks every unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	TSubclassOf<UNavArea> InnerCoreNavArea;

private:
	/** @brief One breakable slot of the building while the game runs. */
	struct FApartmentSlot
	{
		int32 ClusterTransformIndex = INDEX_NONE;
		int32 SlotId = ApartmentCollectionAttributes::NoSlot;
		EApartmentPartType PartType = EApartmentPartType::Detail;
		// In the space of the geometry component, at rest.
		FBox LocalBounds = FBox(ForceInit);
		// Centre of mass at rest; a break is aimed here.
		FVector RestLocation = FVector::ZeroVector;
		// Where the collection reports the slot's transform while it still stands; it moves once the slot falls.
		FVector RestTransformLocation = FVector::ZeroVector;
		TArray<int32> LeafTransformIndices;
		float Health = 0.f;
		bool bBroken = false;
		// The ground floor and the rubble around the building stay pinned: loose, they would drop through the
		// terrain they lie in.
		bool bIndestructible = false;
	};

	/** @brief Which slots a cover socket depends on, resolved once when the game starts. */
	struct FApartmentCoverSocketState
	{
		TArray<int32> SupportSlotIndices;
		bool bUsable = true;
	};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection;
	bool GetIsValidGeometryCollection() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UApartmentFootprintNavModifier> M_OuterBandNav;
	bool GetIsValidOuterBandNav() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UApartmentFootprintNavModifier> M_InnerRingNav;
	bool GetIsValidInnerRingNav() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UApartmentFootprintNavModifier> M_InnerCoreNav;
	bool GetIsValidInnerCoreNav() const;

	// Covers the footprint and its band; only tells which vehicles are close enough to crush anything.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> M_CrushSensor;
	bool GetIsValidCrushSensor() const;

	// Vehicles inside the sensor; they are polled because a geometry collection reports no overlaps of its own.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_CrushingVehicles;

	TArray<FApartmentSlot> M_Slots;
	TArray<FApartmentCoverSocketState> M_CoverSocketStates;
	// Item index of the collection's root, the body every slot is a child of until the first parts break off.
	int32 M_RootItemIndex = INDEX_NONE;
	// Set while slots broke since the cover finder was last told which sockets are left.
	bool bM_CoverSocketsNeedRefresh = false;
	// Parts that fall because their support went never report it; for a while after damage they are looked for.
	float M_StructureCheckUntilWorldSeconds = 0.f;

	FTimerHandle M_CrushTimerHandle;
	FTimerHandle M_StructureCheckTimerHandle;

	UPROPERTY()
	FApartmentGarrisonState M_Garrison;

	// Spawns the selectable when the first soldier of the player is inside and removes it when none is left.
	void UpdateGarrisonSelectable();
	void SpawnGarrisonSelectable();
	// Squads of the player with at least one soldier inside, each once.
	void GatherGarrisonedSquads(TArray<ASquadController*>& OutSquads) const;
	// Where the squad with this index gathers after it was ordered out.
	FVector GetGarrisonExitLocation(int32 SquadIndex) const;

	UFUNCTION()
	void OnCrushSensorBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                               const FHitResult& SweepResult);

	void CrushUpdate();
	void CrushUnderVehicle(const AActor& Vehicle);
	bool GetPassesCrushGate(const AActor* OtherActor) const;
	void SetupCrushSensorCollision() const;

	// Makes this actor's part health the only thing that breaks parts; falling debris breaks nothing.
	void BeginPlay_SetupWeaponOnlyDamage();
	void BeginPlay_BuildSlots();
	void BeginPlay_AnchorGroundStorey();
	void BeginPlay_BindCoverSockets();

	/**
	 * @brief Weapons only report where they hit, and that can be slightly off the surface.
	 * @param ImpactLocation Where the weapon reported its hit.
	 * @param ShotDirection Direction the shot travelled in.
	 * @param OutHit The hit on the geometry collection.
	 * @return Whether the building was found along the shot.
	 */
	bool TryFindHitPiece(const FVector& ImpactLocation, const FVector& ShotDirection, FHitResult& OutHit) const;

	FVector GetImpactLocation(const FDamageEvent& DamageEvent, const AActor* DamageCauser) const;
	FVector GetShotDirection(const FDamageEvent& DamageEvent, const AActor* DamageCauser,
	                         const FVector& ImpactLocation) const;

	/**
	 * @brief Takes health from the part at the impact and a share from the parts around it.
	 * @param WorldImpact Where the shot landed on the building.
	 * @param ShotDirection Direction the shot travelled in; parts that break are pushed along it.
	 * @param DamageAmount Damage of the shot.
	 */
	void DamagePartsAround(const FVector& WorldImpact, const FVector& ShotDirection, float DamageAmount);

	void DamageSlot(int32 SlotIndex, float DamageAmount, const FVector& PushVelocity);
	void BreakSlot(int32 SlotIndex, const FVector& PushVelocity);

	/**
	 * @brief Once parts have broken off, the standing rest is no longer the collection's root but a body the
	 * physics made for it, and a break has to be addressed to that body.
	 * @param Slot A slot that still stands.
	 * @return Item index of the body the slot is part of right now.
	 */
	int32 FindBodyItemOfSlot(const FApartmentSlot& Slot) const;

	// Finds slots that moved from where they stood, marks them broken and keeps the cover sockets in step.
	void CheckStructure();
	void StartStructureChecks();
	void RefreshCoverSocketsIfNeeded();
	// The slots right in front of and under a socket, for sockets that do not name their own.
	void FindSupportSlots(const FApartmentCoverSocket& CoverSocket, TArray<int32>& OutSlotIndices) const;
};
