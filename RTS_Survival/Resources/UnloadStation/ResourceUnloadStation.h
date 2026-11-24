// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnloadStationTypes.h"

#include "ResourceUnloadStation.generated.h"

class UHarvester;
class UResourceDropOff;
class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * @brief Overhead monorail crane that unloads harvesters at a resource drop-off (the HQ's loop crane).
 * Harvesters reserve a bay, reverse in, and one trolley per bay carries the cargo chunk by chunk into the bins.
 * Every chunk is credited through the drop-off the moment it lands; the load in the air is only visual.
 * @note Spawned by ANomadicVehicle (M_UnloadStationClass) with the building and attached to the building mesh, so
 * its frame is the building mesh's frame. Defaults are the Strike Division HQ's loop (see Docs/Units/HQUnloadCrane.md).
 */
UCLASS()
class RTS_SURVIVAL_API AResourceUnloadStation : public AActor
{
	GENERATED_BODY()

public:
	AResourceUnloadStation();

	virtual void Tick(float DeltaSeconds) override;

	/** @brief Links the station to the drop-off whose bins it fills and whose sockets give the bin locations. */
	void InitUnloadStation(UResourceDropOff* DropOff, UStaticMeshComponent* BuildingMesh);

	/**
	 * @brief Claims a free bay for a harvester on its way to the drop-off; a full station leaves it on the old path.
	 * @param Harvester The harvester that wants to unload here.
	 * @param OutBayIndex The claimed bay.
	 * @return Whether a bay was claimed (or the harvester already held one).
	 */
	bool TryReserveBay(UHarvester& Harvester, int32& OutBayIndex);

	/** @brief Frees the bay if this harvester holds it; any unload in progress stops without crediting more. */
	void ReleaseBay(int32 BayIndex, const UHarvester* Harvester);

	/**
	 * @brief The world transform the harvester's actor must reach so its cargo stands at the bay's grab point.
	 * @param BayIndex The bay the harvester holds.
	 * @param Harvester Its cargo socket decides the offset, so differently sized harvesters line up the same.
	 * @param OutDock Actor location (at the harvester's current height) and rotation.
	 * @return False for an invalid bay.
	 */
	bool GetDockTransform(int32 BayIndex, const UHarvester& Harvester, FTransform& OutDock) const;

	/** @return The point ahead of the bay where the harvester lines up before reversing in (world). */
	FVector GetApproachLocation(int32 BayIndex, const UHarvester& Harvester) const;

	/**
	 * @brief The harvester stands in its bay: its trolley starts carrying the cargo to the bins.
	 * @param BayIndex The bay it reserved.
	 * @param Harvester The docked harvester.
	 * @param DropOff The drop-off that credits each landed chunk.
	 * @param Resource The cargo type it unloads.
	 * @param CargoAmount How much it carries; split into TripsPerUnload chunks.
	 */
	void OnHarvesterDocked(int32 BayIndex, UHarvester& Harvester, UResourceDropOff& DropOff,
	                       ERTSResourceType Resource, int32 CargoAmount);

	/** @return Whether the station has a bin for this resource; other cargo goes to the drop-off point the old way. */
	bool GetCanUnloadResource(ERTSResourceType Resource) const;

	int32 GetBayCount() const { return M_Bays.Num(); }
	bool GetIsBayDocked(int32 BayIndex) const;
	const UHarvester* GetBayHarvester(int32 BayIndex) const;
	EUnloadTrolleyPhase GetTrolleyPhase(int32 BayIndex) const;
	FVector GetTrolleyWorldLocation(int32 BayIndex) const;
	FVector GetGrabWorldLocation(int32 BayIndex) const;
	FVector GetBayPickWorldLocation(int32 BayIndex) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleDefaultsOnly, Category="Unload Station")
	TObjectPtr<USceneComponent> StationRoot;

	// Rail centreline; built at BeginPlay from RailControlPoints the same way the rail mesh was modelled.
	UPROPERTY(VisibleDefaultsOnly, Category="Unload Station")
	TObjectPtr<USplineComponent> RailSpline;

	// The static rail, hangers and supports.
	UPROPERTY(VisibleDefaultsOnly, Category="Unload Station")
	TObjectPtr<UStaticMeshComponent> RailMeshComponent;

	// Control points of the rail loop (station frame, cm; Z ignored). Densified with Catmull-Rom, RailSubdivisions
	// points per span, exactly as _AssetSource/HQUnloadCrane/build_monorail.py builds the rail mesh.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Rail")
	TArray<FVector> RailControlPoints;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Rail", meta=(ClampMin="2"))
	int32 RailSubdivisions = 14;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Rail")
	bool bRailIsClosedLoop = true;

	// Height of the rail's centre (cm above the building origin); the trolley mesh's origin sits there.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Rail")
	float RailHeight = 480.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Bays")
	TArray<FUnloadBaySettings> BaySettings;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Bays")
	TArray<FUnloadBinSettings> BinSettings;

	// How many crane trips one full unload takes; the cargo is split evenly over them.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Timing", meta=(ClampMin="1"))
	int32 TripsPerUnload = 3;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Timing", meta=(ClampMin="10"))
	float TrolleySpeed = 380.f;

	// Within this distance of a stop the trolley slows down (ease in/out).
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Timing", meta=(ClampMin="1"))
	float TrolleyBrakeDistance = 160.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Timing", meta=(ClampMin="10"))
	float HoistSpeed = 190.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Timing", meta=(ClampMin="0.05"))
	float GrabSeconds = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Timing", meta=(ClampMin="0.05"))
	float ReleaseSeconds = 0.35f;

	// Grab head top while travelling, below the rail centre (cm). The rope mesh hangs from RopeTopOffset.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Hoist")
	float HeadTravelBelowRail = 90.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Hoist")
	float RopeTopBelowRail = 60.f;

	// Length of the rope mesh as modelled (cm); it is scaled along Z to the current drop.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Hoist", meta=(ClampMin="1"))
	float RopeMeshLength = 100.f;

	// The grab head's top stops this far above the harvester's cargo socket when picking.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Hoist")
	float PickHeadAboveCargo = 95.f;

	// The grab head's top stops this far above the bin socket when dropping.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Hoist")
	float DropHeadAboveBin = 200.f;

	// Where the jaws hinge below the head top, and how far they spread (cm, Y).
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Grab")
	float JawHingeBelowHead = 14.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Grab")
	float JawHingeSpread = 13.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Grab")
	float JawOpenAngle = 30.f;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> RailMesh;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> TrolleyBodyMesh;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> RopeMesh;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> GrabHeadMesh;

	// One jaw, hinge at its origin, hanging along -Z and opening towards +Y; the second jaw is it turned 180 deg.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> JawMesh;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> CrystalMesh;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Meshes")
	TObjectPtr<UStaticMesh> BarMesh;

	// Where the payload items hang in the closed grab (relative to the head top).
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Payload")
	TArray<FTransform> CrystalSlots;

	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Payload")
	TArray<FTransform> BarSlots;

	// A pop-up over the bin on every landed load.
	UPROPERTY(EditDefaultsOnly, Category="Unload Station|Payload")
	float BinTextHeight = 260.f;

private:
	TArray<FUnloadBayState> M_Bays;
	TArray<FUnloadTrolley> M_Trolleys;
	TArray<FUnloadFallingItem> M_FallingItems;

	TWeakObjectPtr<UResourceDropOff> M_DropOff;
	TWeakObjectPtr<UStaticMeshComponent> M_BuildingMesh;
	bool bM_IsStationReady = false;

	void BuildRailSpline();
	void BuildDefaults();
	void SetupBays();
	FVector GetBinWorldLocation(const FUnloadBinSettings& Bin) const;
	float FindBinOffsetForBay(const FUnloadBayState& Bay, const FUnloadBaySettings& Settings,
	                          const FVector& BinWorld) const;
	void CreateTrolley(int32 BayIndex);
	UStaticMeshComponent* CreatePart(UStaticMesh* Mesh, USceneComponent* Parent, const FName Name);

	float GetRailDistanceForOffset(const FUnloadTrolley& Trolley) const;
	float WrapRailDistance(float Distance) const;
	float GetTravelHeadZ() const;

	void TickTrolley(FUnloadTrolley& Trolley, float DeltaSeconds);
	void TickTrolleyIdle(FUnloadTrolley& Trolley);
	void TickTrolleyArrivals(FUnloadTrolley& Trolley);
	bool MoveOffsetTowardsTarget(FUnloadTrolley& Trolley, float DeltaSeconds) const;
	bool MoveHookTowardsTarget(FUnloadTrolley& Trolley, float DeltaSeconds) const;
	void EnterPhase(FUnloadTrolley& Trolley, EUnloadTrolleyPhase NewPhase);
	void StartPickAtBay(FUnloadTrolley& Trolley);
	void StartRunToBin(FUnloadTrolley& Trolley);
	void ReleaseLoadIntoBin(FUnloadTrolley& Trolley);
	void AfterDropHoisted(FUnloadTrolley& Trolley);
	void ShowLoad(FUnloadTrolley& Trolley, ERTSResourceType Resource, bool bShow) const;
	void DropLoadItems(FUnloadTrolley& Trolley);
	void TickFallingItems(float DeltaSeconds);
	void UpdateTrolleyVisuals(const FUnloadTrolley& Trolley) const;
	void EndBayUnload(int32 BayIndex, EUnloadEndReason Reason);

	bool GetIsValidBay(int32 BayIndex) const;
	bool GetIsValidDropOff() const;
};
