// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RTS_Survival/Environment/WallActor/WallActor.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"

#include "WallGate.generated.h"

class UNavArea;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UStaticMeshComponent;
class UWallGateFrameNavModifier;
class UWallGateNavModifier;

namespace WallGatePrimitiveData
{
	// Custom primitive data float the leaf's VAT material (MF_NomadicDeploy_VAT) reads: 1 = closed rest pose, 0 = open.
	// Indices 8..34 belong to UWallHoleDamageComponent; nothing here resets the array.
	inline constexpr int32 ProgressIndex = 3;
	inline constexpr float ClosedProgress = 1.f;
	inline constexpr float OpenProgress = 0.f;
}

namespace WallGateDefaults
{
	// The enemy AI player (the Soviets when the human plays German); the human player is 1.
	inline constexpr uint8 EnemyAIPlayer = 2;
}

UENUM(BlueprintType)
enum class EWallGateState : uint8
{
	Closed,
	Opening,
	Open,
	Closing,
	// The gate died; it never moves again.
	Wrecked
};

// How a closed gate's doorway shows up in the navigation mesh.
UENUM(BlueprintType)
enum class EWallGateNavMode : uint8
{
	// Units of the allowed player and its allies plan through a closed gate, others don't (one doorway area per
	// player, see RTSWallGateNavAreas). Needs the RTS nav filters reparented onto
	// URTSWallGateNavQueryFilter; without that the gate falls back to BlockEveryone and reports an error.
	PlayerAwareArea,
	// A closed gate blocks every unit's planning (NavArea_Null); allowed units only route through it once it is open.
	BlockEveryone
};

/**
 * @brief The RTS component AWallGate creates itself, so a gate Blueprint needs no component or graph of its own:
 * same defaults as on the Rus walls (owned by the enemy AI, not registered with the game state).
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UWallGateRTSComponent : public URTSComponent
{
	GENERATED_BODY()

public:
	UWallGateRTSComponent();
};

// Where the gate is in its motion; only changed by AWallGate.
USTRUCT(BlueprintType)
struct FWallGateMotionState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WallGate")
	EWallGateState State = EWallGateState::Closed;

	// Leaf VAT progress, written to custom primitive data 3: 1 closed, 0 open.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WallGate")
	float Progress = WallGatePrimitiveData::ClosedProgress;

	// Open enough to drive through: leaf collision off and the doorway open in the navigation mesh.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WallGate")
	bool bIsPassable = false;

	// World time an allowed unit was last within OpenRadius; the gate closes CloseDelaySeconds after it.
	float LastAllowedUnitSeenTime = 0.f;
};

/**
 * @brief A gate in a wall that opens by itself for units of the allowed player (default the enemy AI) and its allies,
 * never for hostile players, and closes again when none are near. Detection runs on a timer over the unit registry
 * and reads the current relations each scan, so team changes apply within one scan; the leaf animates through its
 * baked VAT (custom primitive data 3) at 30 Hz only while moving. The doorway is a navigation area of the allowed
 * player that follows the gate state. Works with zero Blueprint graph nodes: it creates its RTS component, sets up
 * the frame's wall collision and blocks navigation at the frame's collision boxes itself.
 */
UCLASS()
class RTS_SURVIVAL_API AWallGate : public AWallActor
{
	GENERATED_BODY()

public:
	AWallGate(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	/**
	 * @brief The doorway the navigation modifier marks: the leaf's rest-pose (closed) bounds, thickened by
	 * NavDoorwayPadding across the wall only, so it does not reach into the pillars' blocked boxes.
	 * @param OutLeafSpaceBox Box in the leaf component's space.
	 * @param OutLeafTransform The leaf component's world transform.
	 * @return False while the leaf has no static mesh.
	 */
	bool GetDoorwayNavBox(FBox& OutLeafSpaceBox, FTransform& OutLeafTransform) const;

	/**
	 * @brief Boxes the frame blocks navigation with: its simple collision elements (bounding box for non-box shapes),
	 * dropping those that start above FrameNavMaxBoxBottom (a lintel over the doorway).
	 * @param OutFrameSpaceBoxes Boxes in the frame mesh's space, unpadded.
	 * @param OutFrameTransform The frame mesh component's world transform.
	 * @return False without a frame mesh with simple collision.
	 */
	bool GetFrameNavBoxes(TArray<FBox>& OutFrameSpaceBoxes, FTransform& OutFrameTransform) const;

	float GetFrameNavBoxPadding() const { return FrameNavBoxPadding; }

	UFUNCTION(BlueprintPure, Category="WallGate")
	EWallGateState GetGateState() const { return M_Motion.State; }

	// 0 closed, 1 fully open.
	UFUNCTION(BlueprintPure, Category="WallGate")
	float GetGateOpenAmount() const { return WallGatePrimitiveData::ClosedProgress - M_Motion.Progress; }

	UFUNCTION(BlueprintPure, Category="WallGate")
	bool GetIsGatePassable() const { return M_Motion.bIsPassable; }

	uint8 GetAllowedPlayer() const { return AllowedPlayer; }

	/**
	 * @brief Hands the gate to another player's side at runtime: from now on that player's units and its allies open
	 * it and plan through it while closed. A capture of the gate (its RTS owner changing) calls this by itself.
	 * @param NewAllowedPlayer Player 1..4; the neutral owner 0 keeps the gate shut for everyone.
	 */
	UFUNCTION(BlueprintCallable, Category="WallGate")
	void SetAllowedPlayer(const uint8 NewAllowedPlayer);

	// False when NavMode PlayerAwareArea fell back to BlockEveryone (RTS nav filters not gate aware); set at BeginPlay.
	bool GetUsesPlayerAwareNavArea() const { return bM_UsePlayerAwareNavArea; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnitDies(const ERTSDeathType DeathType) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WallGate")
	TObjectPtr<USceneComponent> GateRoot;

	// The moving part: mesh and VAT materials are set in the Blueprint. Collision mirrors the frame while closed.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WallGate")
	TObjectPtr<UStaticMeshComponent> GateLeafMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WallGate")
	TObjectPtr<UWallGateNavModifier> GateNavModifier;

	// NavArea_Null over the frame's collision boxes: a gate spawned at runtime has no baked navigation for its pillars.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WallGate")
	TObjectPtr<UWallGateFrameNavModifier> FrameNavModifier;

	// Units of this player and of its allies open the gate (and, with PlayerAwareArea, plan through it while closed);
	// hostile players' units never do. Left at the class default it follows the gate's RTS owner at BeginPlay (a gate
	// placed or spawned for player 3 opens for player 3); a value changed from the default is kept even when it
	// differs from the owner (the owner alone decides whose shots pass the gate). When the owner changes at runtime and
	// this was the old owner, it follows the new owner (a captured gate opens for its captor's side).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Access")
	uint8 AllowedPlayer = WallGateDefaults::EnemyAIPlayer;

	// Allowed units (the allowed player's and its allies') within this distance (2D, from the doorway centre, both
	// sides) open the gate.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Access", meta=(ClampMin="0.0"))
	float OpenRadius = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Access", meta=(ClampMin="0.05"))
	float DetectionIntervalSeconds = 0.3f;

	// Seconds without an allowed unit in range before the gate starts closing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Access", meta=(ClampMin="0.0"))
	float CloseDelaySeconds = 2.f;

	// A closing gate waits while any unit's centre is within the leaf's rest bounds grown by this much.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Access", meta=(ClampMin="0.0"))
	float DoorwayClearancePadding = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Motion", meta=(ClampMin="0.05"))
	float OpenDurationSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Motion", meta=(ClampMin="0.05"))
	float CloseDurationSeconds = 3.f;

	// Progress (1 closed, 0 open) at or below which an opening gate turns passable: leaf collision off, doorway open.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Motion", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PassableProgressThreshold = 0.15f;

	// The leaf copies this component's collision; the hole component of the frame uses the same name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Collision")
	FName FrameMeshComponentName = TEXT("StaticMesh");

	// Passed to SetupWallCollision for the frame at BeginPlay, as Rus_Wall_Concrete_Long does from its graph.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Collision")
	bool bFrameAffectsNavMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Navigation")
	EWallGateNavMode NavMode = EWallGateNavMode::PlayerAwareArea;

	// Grows the leaf's rest bounds on every side for the doorway nav area, so the thin leaf still covers whole voxels.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Navigation", meta=(ClampMin="0.0"))
	float NavDoorwayPadding = 60.f;

	// Grows each frame nav box on every side (cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Navigation", meta=(ClampMin="0.0"))
	float FrameNavBoxPadding = 40.f;

	// Frame collision boxes whose bottom lies above this height in the frame mesh's space (cm) are not blocked: a lintel
	// above the doorway must not close it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Navigation")
	float FrameNavMaxBoxBottom = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Sound")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Sound")
	TObjectPtr<USoundBase> CloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Sound")
	TObjectPtr<USoundAttenuation> GateSoundAttenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WallGate|Sound")
	TObjectPtr<USoundConcurrency> GateSoundConcurrency;

#if WITH_EDITORONLY_DATA
	// Editor preview only (0 closed, 1 open): written in the construction script; the game always starts closed.
	UPROPERTY(EditAnywhere, Category="WallGate|Preview", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PreviewOpenAmount = 0.f;
#endif

	UFUNCTION(BlueprintImplementableEvent, Category="WallGate")
	void BP_OnGateStartedOpening();

	UFUNCTION(BlueprintImplementableEvent, Category="WallGate")
	void BP_OnGateStartedClosing();

	UFUNCTION(BlueprintImplementableEvent, Category="WallGate")
	void BP_OnGateFullyOpen();

	UFUNCTION(BlueprintImplementableEvent, Category="WallGate")
	void BP_OnGateFullyClosed();

private:
	void BeginPlay_SetupFrameCollision();
	void BeginPlay_SetupLeafCollision();
	void BeginPlay_SetupNavigation();
	void BeginPlay_StartDetection();
	// An AllowedPlayer left at the class default takes the owner the gate begins play with (spawned for a player).
	void BeginPlay_ResolveAllowedPlayer();
	// Follows runtime owner changes (captures) of the gate's RTS component.
	void BeginPlay_BindOwnerChange();

	// Without index 3 in the defaults a hole write (index 8+) would zero-fill it, which reads as fully open.
	void EnsureClosedDefaultPrimitiveData() const;

	void OnDetectionTimer();
	// Any live unit of the allowed player or one of its allies (relations as they are now) within OpenRadius.
	bool GetIsAllowedUnitInRange() const;

	/**
	 * @brief Walks one player's tanks and squad units in the registry, without copying the lists.
	 * @param Player Player whose units count.
	 * @param Centre Doorway centre.
	 * @param RadiusSquared Squared 2D reach.
	 * @return True when a live unit of that player is within reach.
	 */
	bool GetIsUnitOfPlayerInRange(uint8 Player, const FVector& Centre, double RadiusSquared) const;
	bool GetIsAnyUnitInDoorway() const;

	void OnGateOwnerChanged(uint8 OldOwner, uint8 NewOwner);
	// A shut gate shows the allowed player's area right away; a passable or wrecked one keeps its open doorway and
	// picks the area up when it closes.
	void ApplyClosedNavAreaIfShut() const;

	/**
	 * @brief Reverses or starts the leaf motion from the current progress, so a reversal never snaps.
	 * @param NewState Opening or Closing.
	 */
	void StartMoving(EWallGateState NewState);
	void FinishMoving();
	void SetGatePassable(bool bNewIsPassable);
	void WriteLeafProgress(float Progress) const;
	void PlayGateSound(USoundBase* Sound) const;
	void StopGateOnDeath();

	// The leaf's rest-pose bounds in its own space; false without a static mesh.
	bool GetLeafRestBox(FBox& OutLeafSpaceBox) const;
	FVector GetDoorwayWorldCentre() const;
	UStaticMeshComponent* FindFrameMesh() const;

	// The doorway area while closed: the allowed player's gate area, or NavArea_Null for BlockEveryone and for a
	// neutral allowed player.
	TSubclassOf<UNavArea> GetClosedNavAreaClass() const;

	// PlayerAwareArea only works when every RTS nav filter derives from URTSWallGateNavQueryFilter.
	static bool GetAreRTSNavFiltersGateAware();

	bool GetIsValidGateLeafMesh() const;
	bool GetIsValidGateNavModifier() const;
	bool GetIsValidFrameNavModifier() const;
	void RefreshFrameAndDoorwayNavBounds() const;

	UPROPERTY(VisibleInstanceOnly, Category="WallGate")
	FWallGateMotionState M_Motion;

	// The frame's collision state, restored on the leaf whenever the gate stops being passable.
	TEnumAsByte<ECollisionEnabled::Type> M_LeafBlockingCollision = ECollisionEnabled::QueryAndPhysics;

	// Set at BeginPlay: PlayerAwareArea falls back to BlockEveryone when the nav filters are not gate aware.
	bool bM_UsePlayerAwareNavArea = false;

	FTimerHandle M_DetectionTimerHandle;

	FDelegateHandle M_OwnerChangedHandle;
};
