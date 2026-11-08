// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/CargoParachuteDescentComponent.h"
#include "GA_BuildingParachuteDrop.generated.h"

class AAircraftMaster;
class ABuildingExpansion;
class UStaticMesh;
struct FStreamableHandle;

/** One building that comes down in the drop. */
USTRUCT(BlueprintType)
struct FBuildingParachuteDropPayload
{
	GENERATED_BODY()

	// The exact expansion Blueprint; field bunkers share their type with a pit, so the type alone is not enough.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<ABuildingExpansion> BuildingClass;

	// Landing spot relative to the target: X along the flight, Y to its right.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D LandingOffset = FVector2D::ZeroVector;

	// Facing relative to the flight direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float YawOffset = 0.f;

	// Canopies and fall of this load.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCargoParachuteDescentSettings Descent;
};

USTRUCT(BlueprintType)
struct FBuildingParachuteDropSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AAircraftMaster> AircraftClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBuildingParachuteDropPayload> Payloads;

	// Used for payloads without their own canopy mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> CanopyMesh = nullptr;

	// Raises the transport to at least this height above the target so the canopies have room to open and fall;
	// 0 keeps the mission's aircraft height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MinFlightHeightAboveTarget = 6000.f;

	// Seconds between two loads leaving the aircraft.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float DropInterval = 0.9f;

	// How far below the aircraft a load appears (its canopies need room).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float DropBelowAircraftOffset = 1500.f;

	// Flight distance after the target point before the aircraft disappears; 0 uses the approach distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float ExitDistanceAfterTarget = 0.f;
};

/**
 * @brief Designer ability: a transport flies straight over the target and drops building expansions (pits, field
 * bunkers) that come down under big cargo canopies, land upright on the landscape and then work as the owner's own
 * standalone expansion. A dropped expansion is built through the field defense path (no parent building), which is
 * how the HQ's field defenses already exist without an owner: StartFieldDefenseConstruction then
 * FinishFieldDefenseConstruction, while it is still in the aircraft.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_BuildingParachuteDrop : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void BeginDestroy() override;

	bool GetHasActiveDrop() const { return not M_DropRuns.IsEmpty(); }
	// Buildings this ability dropped (landed or still falling); for tests.
	TArray<TWeakObjectPtr<ABuildingExpansion>> GetDroppedBuildings() const { return M_DroppedBuildings; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FBuildingParachuteDropSettings M_BuildingDropSettings;

	struct FBuildingDropRun
	{
		int32 RunId = INDEX_NONE;
		FVector TargetLocation = FVector::ZeroVector;
		FVector PassOverLocation = FVector::ZeroVector;
		FVector RunDirection = FVector::ForwardVector;
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		bool bM_HasReachedTarget = false;
		int32 NextPayloadIndex = 0;
		int32 LoadsInAir = 0;
		FTimerHandle DropTimerHandle;
	};

	TArray<FBuildingDropRun> M_DropRuns;

	UPROPERTY(Transient)
	int32 M_NextRunId = 0;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ABuildingExpansion>> M_DroppedBuildings;

	TArray<TSharedPtr<FStreamableHandle>> M_LoadHandles;

	FBuildingDropRun* FindRun(int32 RunId);
	void OnAssetsLoaded(int32 RunId);
	void OnAircraftOverTarget(int32 RunId);
	void OnDropTimer(int32 RunId);
	void DropPayload(FBuildingDropRun& Run, const FBuildingParachuteDropPayload& Payload);

	/**
	 * @brief Builds the expansion as a standalone field defense at once, so it falls complete; owner, collision
	 * and navigation are set for the fall.
	 * @return The expansion, or null when its class could not be spawned.
	 */
	ABuildingExpansion* SpawnBuiltExpansion(UClass* BuildingClass, const FVector& Location, const FRotator& Rotation) const;
	void SetExpansionAirborne(ABuildingExpansion& Expansion, bool bAirborne) const;
	void OnPayloadLanded(int32 RunId, AActor* Load, bool bLanded);
	void ReleaseAircraftIfAllDropped(const FBuildingDropRun& Run) const;
	void FinishRunIfDone(int32 RunId);

	UFUNCTION()
	void OnSpawnedAircraftDestroyed(AActor* DestroyedActor);
};
