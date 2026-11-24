// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ResourceUnloadStation.h"

#include "SkeletalCraneUnloadStation.generated.h"

class UPoseableMeshComponent;
class USkeletalMesh;

/** What the pedestal crane is doing for its one bay. */
enum class ESkeletalCranePhase : uint8
{
	Idle,
	SlewingToPick,
	LoweringToPick,
	Grabbing,
	HoistingFromPick,
	SlewingToDrop,
	LoweringToDrop,
	Releasing,
	HoistingFromDrop,
	ReturningToRest
};

/** Slew, luff, hoist and jaw state of the crane (degrees, cm, 0 = closed .. 1 = open). */
struct FSkeletalCranePose
{
	float Yaw = 0.f;
	float Luff = 0.f;
	// Vertical distance from the boom tip down to the grab head origin.
	float Hoist = 0.f;
	float JawOpen = 0.f;
};

/** A payload item falling out of the opened grab. */
struct FSkeletalCraneFallingItem
{
	TWeakObjectPtr<UStaticMeshComponent> Component;
	float VelocityZ = 0.f;
	float StopZ = 0.f;
};

/**
 * @brief The Soviet refinery's unload station: a pedestal crane (SK_SovRef_Crane, a station copy of SovietCrane_1)
 * posed from C++ through a UPoseableMeshComponent. One bay: the T-34 Breaker reverses in, the crane slews and luffs over
 * its hopper, lowers the grab, closes, hoists, slews over the receiving cell of the cargo's resource, opens and credits
 * one chunk through the drop-off. TripsPerUnload trips per unload.
 * @note No trolleys: the bay is driven from TickUnloadStation. The base class rail only runs dock -> radixite drop ->
 * metal drop so the bins register. The aim is solved numerically on the rig (no hand-entered geometry), see
 * Docs/EnemyAI/SovietRefinery.md section 4 for the bone contract.
 */
UCLASS()
class RTS_SURVIVAL_API ASkeletalCraneUnloadStation : public AResourceUnloadStation
{
	GENERATED_BODY()

public:
	ASkeletalCraneUnloadStation();

	virtual void PostInitializeComponents() override;
	virtual EUnloadTrolleyPhase GetTrolleyPhase(int32 BayIndex) const override;

	/** @return The grab head's world location (tests). */
	FVector GetCraneGrabWorldLocation() const;
	bool GetIsRigReady() const { return bM_IsRigReady; }
	bool GetHasDropPose(const ERTSResourceType Resource) const { return M_DropPoses.Contains(Resource); }
	// Layout in the station (building) frame, for test cameras.
	FVector GetDockPickPointLocal() const { return DockPickPoint; }
	FVector GetCraneLocationLocal() const { return CraneLocation; }
	FVector GetDropPointLocal(const ERTSResourceType Resource) const
	{
		return Resource == ERTSResourceType::Resource_Metal ? MetalDropPoint : RadixiteDropPoint;
	}
	/** @return The slew/luff/hoist the crane would use to put its closed jaws at this world point (tests, debugging). */
	bool SolvePoseForWorldTarget(const FVector& JawBottomWorld, FSkeletalCranePose& OutPose);

protected:
	virtual bool GetUsesTrolleys() const override { return false; }
	virtual void OnUnloadStationReady() override;
	virtual void TickUnloadStation(float DeltaSeconds) override;

	UPROPERTY(VisibleDefaultsOnly, Category="Crane")
	TObjectPtr<UPoseableMeshComponent> CraneMesh;

	// ---------------------------------------------------------------- layout (station = building frame, cm, deg)

	// Where the docked harvester's hopper centre stands (Z ignored).
	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout")
	FVector DockPickPoint = FVector(2375.f, -1935.f, 0.f);

	// Yaw of the docked harvester; it reverses in against this direction.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout")
	float DockYaw = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout", meta=(ClampMin="300"))
	float DockApproachDistance = 900.f;

	// Where the closed jaws' bottom stops before opening over each resource's receiving cell.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout")
	FVector RadixiteDropPoint = FVector(1280.f, -536.f, 1216.f);

	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout")
	FVector MetalDropPoint = FVector(1280.f, -97.f, 1271.f);

	// The crane's pedestal foot (top of its pad or platform) and the yaw of the crane asset.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout")
	FVector CraneLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Layout")
	float CraneYaw = 0.f;

	// ---------------------------------------------------------------- rig contract (SK_SovRef_Crane)

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	TObjectPtr<USkeletalMesh> CraneSkeletalMesh;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName YawBone = TEXT("crane_yaw");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName LuffBone = TEXT("boom_pitch");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName TipBone = TEXT("boom_tip");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName CableBone = TEXT("cable");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName HeadBone = TEXT("grab_head");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName JawLeftBone = TEXT("jaw_l");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName JawRightBone = TEXT("jaw_r");

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FName LoadSocket = TEXT("grab_load");

	// Local rotation axes (bone space) of the driven bones.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FVector YawAxis = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FVector LuffAxis = FVector(1.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	FVector JawAxis = FVector(0.f, 1.f, 0.f);

	// Luff range about LuffAxis (deg, rest = 0, + = up for SK_SovRef_Crane); the solver searches inside it.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float LuffMin = -28.f;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float LuffMax = 16.f;

	// Jaw angle about JawAxis when fully open (rest = closed); left uses +sign, right the opposite unless overridden.
	// SK_SovRef_Crane: 0 = the shrunk, crossed "closed" pose that fits the hopper; 33.8 = the source rig's clamshell.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float CraneJawOpenAngle = 60.f;

	// How far the jaws open (fraction of CraneJawOpenAngle) while lowering into the hopper: wide open they would hit its
	// ends (slot 1.45 m along the hull).
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig", meta=(ClampMin="0", ClampMax="1"))
	float PickJawOpen = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float JawLeftSign = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float JawRightSign = -1.f;

	// Grab head origin down to the bottom of the closed jaws (cm, positive).
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float JawBottomBelowHead = 197.25f;

	// Extra yaw of the grab about its cable; added to the automatic alignment with the docked hull.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig")
	float GrabYawOffset = 0.f;

	// Shortest cable (cm, tip to head origin) the hoist allows.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Rig", meta=(ClampMin="10"))
	float MinHoist = 120.f;

	// ---------------------------------------------------------------- motion

	UPROPERTY(EditDefaultsOnly, Category="Crane|Timing", meta=(ClampMin="1"))
	float SlewSpeed = 24.f;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Timing", meta=(ClampMin="0.5"))
	float LuffSpeed = 8.f;

	UPROPERTY(EditDefaultsOnly, Category="Crane|Timing", meta=(ClampMin="10"))
	float CraneHoistSpeed = 240.f;

	// Fraction of the remaining angle/distance at which the motion eases into its stop.
	UPROPERTY(EditDefaultsOnly, Category="Crane|Timing", meta=(ClampMin="1"))
	float EaseDegrees = 12.f;

	// The jaws travel this far above the higher of pick and drop while slewing (cm).
	UPROPERTY(EditDefaultsOnly, Category="Crane|Timing")
	float TravelClearance = 180.f;

	// The jaws' bottom stops this far above the harvester's cargo socket when picking (negative = inside the hopper).
	UPROPERTY(EditDefaultsOnly, Category="Crane|Timing")
	float PickJawBottomAboveCargo = 5.f;

private:
	ESkeletalCranePhase M_Phase = ESkeletalCranePhase::Idle;
	FSkeletalCranePose M_Pose;
	FSkeletalCranePose M_Target;
	FSkeletalCranePose M_RestPose;
	float M_PhaseSeconds = 0.f;
	bool bM_IsRigReady = false;
	bool bM_IsCarrying = false;
	ERTSResourceType M_LoadResource = ERTSResourceType::Resource_None;

	// Rest local transforms of the driven bones and the rest component-space data the pose is built from.
	FTransform M_RestLocalYaw;
	FTransform M_RestLocalLuff;
	FTransform M_RestLocalJawLeft;
	FTransform M_RestLocalJawRight;
	FTransform M_RestCSYaw;
	FTransform M_RestCSCable;
	FTransform M_RestCSHead;
	FVector M_CableLengthAxis = FVector(0.f, 1.f, 0.f);
	float M_RestHoist = 300.f;
	// Grab yaw that lines the jaws up with the docked hull at the pick.
	float M_AutoGrabYaw = 0.f;

	// Solved poses for the bay pick (refreshed on every dock) and the two drops.
	FSkeletalCranePose M_PickPose;
	// Station-space height of the jaws' bottom at the current pick.
	float M_PickJawZ = 0.f;
	TMap<ERTSResourceType, FSkeletalCranePose> M_DropPoses;

	TArray<TWeakObjectPtr<UStaticMeshComponent>> M_CrystalItems;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> M_BarItems;
	TArray<FSkeletalCraneFallingItem> M_FallingItems;

	void ApplyLayoutToBaseSettings();
	bool InitRig();
	void CreatePayload();

	void ApplyPose(const FSkeletalCranePose& Pose);
	void ApplyYawLuff(float Yaw, float Luff);
	FTransform GetBoneCS(FName Bone);
	void SetBoneCS(FName Bone, const FTransform& ComponentSpace);
	void SetBoneLocal(FName Bone, const FTransform& Local);

	/** @brief Tip location in station space for a slew/luff, measured on the rig (restores nothing). */
	FVector GetTipStationSpace(float Yaw, float Luff);
	FVector GetYawPivotStationSpace();
	FVector ComponentToStation(const FVector& ComponentSpace) const;

	float GetTravelHoistFor(const FSkeletalCranePose& Pose, float LowerJawZStation) const;

	void EnterPhase(ESkeletalCranePhase NewPhase);
	void TickPhase(float DeltaSeconds);
	bool MoveTowardsTarget(float DeltaSeconds);
	void StartTripToPick();
	void StartTripToDrop();
	void ReleaseLoad();
	void ShowLoad(ERTSResourceType Resource, bool bShow);
	void TickFallingItems(float DeltaSeconds);
	bool GetIsBayReadyForTrip() const;
};
