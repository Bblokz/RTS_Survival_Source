// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/Missions/MissionProps/PropSinkDeath/MissionPropSinkDeath.h"

#include "MissionFuelCrane.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Steps of one unloading cycle; the crane loops SlewToPickup..HoistFromDrop while unloading. */
enum class EMissionFuelCranePhase : uint8
{
	Idle,
	SlewToPickup,
	LowerToPickup,
	AttachCargo,
	HoistFromPickup,
	SlewToDrop,
	LowerToDrop,
	ReleaseCargo,
	HoistFromDrop
};

/** Crane rig pose in the actor's local frame. */
struct FMissionFuelCranePose
{
	// Jib bearing relative to the actor's forward axis.
	float YawDegrees = 0.f;
	// Horizontal distance from the mast axis to the trolley.
	float TrolleyRadius = 600.f;
	// Cable length from the trolley sheaves down to the hook block.
	float HookDepth = 175.f;
};

/** One eased move of the rig from Start to Target (a pause when both are equal). */
struct FMissionFuelCraneMove
{
	FMissionFuelCranePose Start;
	FMissionFuelCranePose Target;
	// Signed yaw sweep; not always the shortest path so the jib can avoid a sector.
	float YawDelta = 0.f;
	float Duration = 1.f;
	float Elapsed = 0.f;
};

/** What the hook is doing with cargo in the current cycle. */
struct FMissionFuelCraneCarryState
{
	bool bIsCarrying = false;
	// Index into M_SourcePieces of the piece the hook is going to pick up.
	int32 TargetSourceIndex = INDEX_NONE;
	// World height of the carried piece, used to stack pieces on top of each other.
	float CargoHeight = 0.f;
};

/** Small decaying pendulum swing of cable + hook after the jib stops slewing. */
struct FMissionFuelCraneSwayState
{
	float AmplitudeDegrees = 0.f;
	float TimeSeconds = 0.f;
};

/**
 * @brief Set-dressing tower crane that loops: slew to a truck, lift a cargo piece, swing it over and stack it.
 * Place the Blueprint child (BP_MissionFuelCrane, meshes set on its components) and call StartUnloading with
 * the parked truck; the truck's own "Cargo*" static mesh components are lifted off (hidden) one by one.
 * It is destructible: padded hit boxes over the mast and jib take the shots (right-click attacks it), and at zero
 * health it halts, smokes, sinks into the ground over DeathSettings.SinkSeconds and leaves a scorch mark.
 * @note StartUnloading: call from the mission once the truck has parked (or with null for a pick-up-point loop).
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AMissionFuelCrane : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	AMissionFuelCrane(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/** Halts the crane, turns its hit boxes off and starts the smoke + sink death (once). */
	virtual void OnUnitDies(const ERTSDeathType DeathType) override;

	/**
	 * @brief Starts (or restarts) the unloading loop from this truck's cargo pieces.
	 * Falls back to a crane-owned pallet pile on the truck bed or at PickupPointLocation when the truck
	 * is null, dead, carries no reachable "Cargo*" components, or is parked out of reach.
	 * @param Truck The parked cargo truck; may be null.
	 */
	UFUNCTION(BlueprintCallable, Category="FuelCrane")
	void StartUnloading(AActor* Truck);

	/** Freezes the crane where it is; a piece on the hook stays on the hook. */
	UFUNCTION(BlueprintCallable, Category="FuelCrane")
	void StopUnloading();

	UFUNCTION(BlueprintPure, Category="FuelCrane")
	bool GetIsUnloading() const;

	/** @return Cargo pieces placed on the drop stack since BeginPlay; never reset when the stack restarts. */
	UFUNCTION(BlueprintPure, Category="FuelCrane")
	int32 GetUnloadedCount() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------ rig layout
	// Height of the slewing ring (top of the base mesh) above the actor origin.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="100"))
	float SlewPivotHeight = 840.f;

	// Height of the trolley rail (underside of the jib bottom chords) above the slewing ring.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig")
	float TrolleyRailHeight = 30.f;

	// Distance from the trolley pivot down to the hoist sheaves where the cable starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="0"))
	float CableOriginOffset = 30.f;

	// Distance from the hook mesh pivot (cable end) down to the lift eye where the sling hangs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="0"))
	float HookEyeOffset = 78.f;

	// Height of the four-leg sling between the lift eye and the top of the cargo.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="10"))
	float SlingHeight = 70.f;

	// The sling legs land this fraction inside the cargo footprint corners.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="0.1", ClampMax="1.0"))
	float SlingFootprintFraction = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="0"))
	float MinTrolleyRadius = 160.f;

	// Trolley stop near the jib tip; cargo farther than this is never picked from the truck.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="100"))
	float MaxTrolleyRadius = 1180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="10"))
	float MinHookDepth = 60.f;

	// Cable length while slewing; the default keeps the cargo bottom ~4.3 m above the crane's ground.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig", meta=(ClampMin="10"))
	float TravelHookDepth = 175.f;

	// Pose shown in the editor and before StartUnloading.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig")
	float IdleJibYaw = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Rig")
	float IdleTrolleyRadius = 700.f;

	// ------------------------------------------------------------------ cargo
	// Static mesh components on the truck whose name starts with this are lifted off, e.g. ZIL "Cargo_0".."Cargo_5".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo")
	FString TruckCargoComponentPrefix = TEXT("Cargo");

	// Fallback pile centre on the truck (truck-local, bed surface) when the truck has no cargo components.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo")
	FVector TruckBedLocalOffset = FVector(-133.f, 0.f, 153.f);

	// Fallback pile centre (crane-local, ground) used without a usable truck.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(MakeEditWidget=true))
	FVector PickupPointLocation = FVector(900.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(ClampMin="1", ClampMax="8"))
	int32 PickupPileCount = 4;

	// Mesh of the crane-owned fallback pile (the truck's own cargo meshes are used when there is a truck).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo")
	TObjectPtr<UStaticMesh> CargoMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo")
	FVector CargoScale = FVector(0.88f);

	// Centre of the drop stack (crane-local); its height is found by a ground trace at BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(MakeEditWidget=true))
	FVector DropStackLocation = FVector(-450.f, -250.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(ClampMin="1", ClampMax="4"))
	int32 StackColumns = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(ClampMin="1", ClampMax="4"))
	int32 StackRows = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(ClampMin="1", ClampMax="3"))
	int32 StackLayers = 2;

	// Centre distance between stack (and fallback pile) cells.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(ClampMin="20"))
	float StackSpacing = 125.f;

	// The stack is cleared (while the hook is at the truck) before it would exceed this many pieces.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo", meta=(ClampMin="1", ClampMax="16"))
	int32 MaxStackCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo")
	bool bTraceDropGround = true;

	// Starts the pick-up-point loop on BeginPlay without waiting for StartUnloading.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Cargo")
	bool bStartWithoutTruckOnBeginPlay = false;

	// ------------------------------------------------------------------ slewing route
	// Long swings (pick-up <-> drop) pass through PreferredSlewViaYaw instead of taking the shortest way.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing")
	bool bUsePreferredSlewSide = true;

	// Crane-local bearing the jib sweeps through on long swings (-90 = actor left / -Y side).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(EditCondition="bUsePreferredSlewSide"))
	float PreferredSlewViaYaw = -90.f;

	// Swings shorter than this always take the shortest way (small corrections between truck pieces).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0", ClampMax="180"))
	float PreferredSideMinSwing = 45.f;

	// ------------------------------------------------------------------ timing
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="1"))
	float SlewSpeedDegreesPerSecond = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="1"))
	float TrolleySpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="1"))
	float HoistSpeed = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0"))
	float AttachPauseSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0"))
	float ReleasePauseSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0.05"))
	float MinMoveSeconds = 0.6f;

	// Scales every phase: 2 plays the loop twice as fast.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0.1", ClampMax="5"))
	float SpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0", ClampMax="10"))
	float SwayKickDegrees = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0.05"))
	float SwayFrequencyHz = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Timing", meta=(ClampMin="0"))
	float SwayDamping = 1.2f;

	// ------------------------------------------------------------------ damage
	// Half extents of the invisible box over the mast that takes weapon hits (base at the actor origin).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Damage")
	FVector MastHitBoxExtent = FVector(200.f, 200.f, 470.f);

	// Padding added around the jib mesh bounds for the hit box that slews with the jib.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Damage")
	FVector JibHitBoxPadding = FVector(40.f, 60.f, 60.f);

	// Smoke, 8 s sink and the scorch decal left under the mast.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelCrane|Damage")
	FMissionPropSinkDeathSettings DeathSettings;

private:
	// ------------------------------------------------------------------ components (meshes set in the BP child)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> M_Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_BaseMesh = nullptr;

	// Yaws the whole upper works around the mast axis.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> M_SlewPivot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_JibMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_TrolleyMesh = nullptr;

	// At the hoist sheaves; tilts cable + hook for the pendulum sway.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> M_SwayPivot = nullptr;

	// 1 m long cable mesh (pivot top), scaled in Z to the hook depth.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_CableMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_HookMesh = nullptr;

	// Unit sling (apex at pivot, legs to +-50, +-50, -100), scaled to the carried cargo footprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_SlingMesh = nullptr;

	// Copy of the lifted piece riding on the hook.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> M_CarriedCargoMesh = nullptr;

	// Invisible weapon hit box up the mast (the lattice itself lets most shots through).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> M_MastHitBox = nullptr;

	// Invisible weapon hit box along the jib; slews with it. Does not block the cursor (see ConfigureHitBox).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="FuelCrane|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> M_JibHitBox = nullptr;

	// Smoke and decal assets loaded at BeginPlay so the death does not hitch.
	UPROPERTY()
	TArray<TObjectPtr<UObject>> M_PreloadedDeathAssets;

	bool bM_DeathStarted = false;

	/**
	 * @param bBlockCursor Visibility block, so hovering/right-clicking it attacks the crane. Off for the jib: it
	 * swings over the parked truck and the wall gap, where it would steal clicks meant for them.
	 */
	void ConfigureHitBox(UBoxComponent* HitBox, bool bBlockCursor) const;
	void ApplyHitBoxLayout() const;
	void SetHitBoxesEnabled(bool bEnabled) const;
	void StartCraneDeath();

	bool GetIsValidRig() const;

	// Fixed pool (MaxStackCount) of placed pieces; reused when the stack restarts.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_StackPieces;

	// Fixed pool (PickupPileCount) of the crane-owned fallback pile.
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_PilePieces;

	// Pieces the hook picks from: the truck's cargo components or M_PilePieces. Weak: the truck owns its pieces.
	UPROPERTY()
	TArray<TWeakObjectPtr<UStaticMeshComponent>> M_SourcePieces;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Truck = nullptr;

	// True while M_Truck must stay alive for the current sources (its cargo or a pile on its bed).
	bool bM_DependsOnTruck = false;
	// True when M_SourcePieces are the truck's own components (restored on EndPlay).
	bool bM_SourcesAreTruckCargo = false;

	bool bM_IsUnloading = false;
	// Set when the truck is lost: finish placing the carried piece, hoist, then halt.
	bool bM_FinishCycleThenStop = false;

	int32 M_UnloadedCount = 0;
	int32 M_NextStackSlot = 0;

	// Per stack cell (columns x rows): ground height and current top of the placed pieces.
	TArray<float> M_StackCellGroundZ;
	TArray<float> M_StackCellTopZ;

	EMissionFuelCranePhase M_Phase = EMissionFuelCranePhase::Idle;
	FMissionFuelCranePose M_CurrentPose;
	FMissionFuelCraneMove M_Move;
	FMissionFuelCraneCarryState M_Carry;
	FMissionFuelCraneSwayState M_Sway;

	void ConfigureMovingPart(UStaticMeshComponent* Part) const;
	void ApplyRigLayout() const;
	FMissionFuelCranePose GetIdlePose() const;
	void ApplyPose(const FMissionFuelCranePose& Pose);
	void TickSway(float DeltaSeconds);
	void SetSwayRotation(float AmplitudeDegrees) const;
	void SnapSwayToRest();

	void BeginPlay_CreatePiecePools();
	void BeginPlay_InitStackGround();
	UStaticMeshComponent* CreatePoolPiece();

	// ------------------------------------------------------------------ sources
	void RestoreTruckCargoVisibility();
	bool GatherTruckCargoPieces(AActor* Truck);
	bool UsePileAt(const FTransform& PileFrame, bool bTraceGround);
	int32 ChooseNextSourcePiece();
	int32 CountVisibleSources() const;
	void RefillSources();
	bool GetIsTruckLost() const;
	void OnTruckLost();

	// ------------------------------------------------------------------ geometry helpers
	float GetHorizontalRadiusFromMast(const FVector& WorldPoint) const;
	bool GetIsWithinReach(const FVector& WorldPoint) const;

	/**
	 * @brief Rig pose that puts the trolley over a world point with the given cable length.
	 * @param WorldPoint Point the hook should hang above (only its XY is used).
	 * @param HookDepth Cable length for the pose.
	 * @return Pose with yaw/radius toward the point; radius clamped to the trolley rail.
	 */
	FMissionFuelCranePose GetPoseAbove(const FVector& WorldPoint, float HookDepth) const;

	/** @return Cable length that puts the hook's lift eye at this world height (clamped to the rig). */
	float GetHookDepthForEyeAt(float EyeWorldZ) const;

	static FVector GetPieceTopCenter(const UStaticMeshComponent* Piece);

	/**
	 * @brief Transform that stands a cargo mesh upright with its bounds' bottom centre on a world point.
	 * @param Mesh Cargo mesh (its local bounds may be off-centre, like the airport barrel stacks).
	 * @param Scale Component scale of the piece.
	 * @param YawDegrees World yaw of the piece.
	 * @param BottomCenter World point the bottom centre lands on.
	 * @return World transform for the piece component.
	 */
	static FTransform GetCargoTransformOnPoint(const UStaticMesh* Mesh, const FVector& Scale, float YawDegrees,
	                                           const FVector& BottomCenter);

	float TraceGroundZ(const FVector& WorldPoint, float FallbackZ) const;
	int32 GetEffectiveMaxStack() const;
	int32 GetStackCellCount() const;
	FVector GetStackSlotBottomCenter(int32 SlotIndex) const;

	// ------------------------------------------------------------------ cycle
	void BeginPhase(EMissionFuelCranePhase Phase);
	void OnPhaseFinished();
	void StartMove(EMissionFuelCranePhase Phase, const FMissionFuelCranePose& Target, bool bAllowPreferredSide);
	void StartPause(EMissionFuelCranePhase Phase, float Seconds);
	float GetSlewDelta(float FromYaw, float ToYaw, bool bAllowPreferredSide) const;
	void Halt();

	void StartSlewToPickup();
	void StartLowerToPickup();
	void StartAttachCargo();
	void StartSlewToDrop();
	void StartLowerToDrop();
	void StartReleaseCargo();
	void StartHoist(EMissionFuelCranePhase Phase);

	bool AttachSourcePiece(int32 SourceIndex);
	void SizeSlingToPiece(const UStaticMeshComponent* Piece) const;
	void PlaceCarriedCargoOnStack();
	void ClearStack();
	static void CopyMaterials(const UStaticMeshComponent* From, UStaticMeshComponent* To);
};
