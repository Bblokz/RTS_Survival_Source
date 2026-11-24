// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SkeletalCraneUnloadStation.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSkeletalCrane, Log, All);

namespace SkeletalCranePrivate
{
	constexpr float AngleTolerance = 0.05f;
	constexpr float HoistTolerance = 0.5f;
	constexpr float HoistEaseDistance = 60.f;
	constexpr float MinEaseFraction = 0.15f;
	constexpr int32 LuffSolveIterations = 28;
	constexpr float Gravity = 980.f;
	// Falling payload stops this far below the drop point (inside the receiving cell).
	constexpr float ItemFallDepth = 160.f;
	constexpr float ItemMinStopAboveDrop = -400.f;

	float MoveAngle(const float Current, const float Target, const float Speed, const float EaseDegrees,
	                const float DeltaSeconds, bool& bOutReached)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(Current, Target);
		if (FMath::Abs(Delta) <= AngleTolerance)
		{
			bOutReached = true;
			return Target;
		}
		const float Ease = FMath::Clamp(FMath::Abs(Delta) / FMath::Max(EaseDegrees, 0.01f), MinEaseFraction, 1.f);
		const float Step = Speed * Ease * DeltaSeconds;
		bOutReached = Step >= FMath::Abs(Delta);
		return bOutReached ? Target : Current + FMath::Sign(Delta) * Step;
	}

	float MoveLinear(const float Current, const float Target, const float Speed, const float EaseDistance,
	                 const float DeltaSeconds, const float Tolerance, bool& bOutReached)
	{
		const float Delta = Target - Current;
		if (FMath::Abs(Delta) <= Tolerance)
		{
			bOutReached = true;
			return Target;
		}
		const float Ease = FMath::Clamp(FMath::Abs(Delta) / FMath::Max(EaseDistance, 0.01f), MinEaseFraction, 1.f);
		const float Step = Speed * Ease * DeltaSeconds;
		bOutReached = Step >= FMath::Abs(Delta);
		return bOutReached ? Target : Current + FMath::Sign(Delta) * Step;
	}

	float HorizontalAngle(const FVector& Vector)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Vector.Y, Vector.X));
	}
}

ASkeletalCraneUnloadStation::ASkeletalCraneUnloadStation()
{
	CraneMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("CraneMesh"));
	CraneMesh->SetupAttachment(StationRoot);
	CraneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CraneMesh->SetCanEverAffectNavigation(false);
	CraneMesh->bCastDynamicShadow = true;

	// One bay, one drop per resource; the base rail and bins are rebuilt from the layout in PostInitializeComponents.
	TripsPerUnload = 3;
	RailHeight = 0.f;
	bRailIsClosedLoop = false;
	bUseOverflowDropOffPoint = true;
	OverflowDropOffPoint = FVector(1500.f, -2600.f, 0.f);
	GrabSeconds = 0.6f;
	ReleaseSeconds = 0.45f;

	// Payload hangs at the grab_load socket.
	const FVector CrystalScale(0.6f);
	const FVector IngotScale(0.35f);
	CrystalSlots = {
		FTransform(FRotator(15.f, 0.f, 10.f), FVector(-7.f, 0.f, 28.f), CrystalScale),
		FTransform(FRotator(-10.f, 70.f, 0.f), FVector(2.f, 4.f, 34.f), CrystalScale),
		FTransform(FRotator(5.f, 140.f, -15.f), FVector(7.f, -3.f, 26.f), CrystalScale)
	};
	BarSlots = {
		FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 26.f), IngotScale),
		FTransform(FRotator(0.f, 8.f, 0.f), FVector(1.f, 1.f, 34.f), IngotScale)
	};
}

void ASkeletalCraneUnloadStation::PostInitializeComponents()
{
	ApplyLayoutToBaseSettings();
	if (IsValid(CraneSkeletalMesh) && CraneMesh->GetSkinnedAsset() != CraneSkeletalMesh)
	{
		CraneMesh->SetSkinnedAssetAndUpdate(CraneSkeletalMesh, true);
	}
	CraneMesh->SetRelativeLocationAndRotation(CraneLocation, FRotator(0.f, CraneYaw, 0.f));
	Super::PostInitializeComponents();
}

void ASkeletalCraneUnloadStation::ApplyLayoutToBaseSettings()
{
	// The rail only tells the base class that the bay reaches both receiving cells: dock -> radixite -> metal.
	RailControlPoints = {
		FVector(DockPickPoint.X, DockPickPoint.Y, 0.f),
		FVector(RadixiteDropPoint.X, RadixiteDropPoint.Y, 0.f)
	};
	// A1 drops both resources into one receiving hopper: no zero-length span.
	if (FVector::Dist2D(RadixiteDropPoint, MetalDropPoint) > 50.f)
	{
		RailControlPoints.Add(FVector(MetalDropPoint.X, MetalDropPoint.Y, 0.f));
	}
	else
	{
		// The base rail needs three points; run on past the shared drop, away from the dock.
		const FVector Away = (RadixiteDropPoint - DockPickPoint).GetSafeNormal2D();
		RailControlPoints.Add(FVector(RadixiteDropPoint.X, RadixiteDropPoint.Y, 0.f) + Away * 300.f);
	}
	bRailIsClosedLoop = false;

	FUnloadBaySettings Bay;
	Bay.PickPoint = FVector(DockPickPoint.X, DockPickPoint.Y, 0.f);
	Bay.DockYaw = DockYaw;
	Bay.ApproachDistance = DockApproachDistance;
	Bay.RunDirection = EUnloadTrolleyRunDirection::AlongRail;
	Bay.ParkOffset = 0.f;
	BaySettings = {Bay};

	FUnloadBinSettings Radixite;
	Radixite.Resource = ERTSResourceType::Resource_Radixite;
	Radixite.SocketName = NAME_None;
	Radixite.FallbackLocation = RadixiteDropPoint;
	FUnloadBinSettings Metal;
	Metal.Resource = ERTSResourceType::Resource_Metal;
	Metal.SocketName = NAME_None;
	Metal.FallbackLocation = MetalDropPoint;
	BinSettings = {Radixite, Metal};
}

// ------------------------------------------------------------------------------------------------------------ rig

void ASkeletalCraneUnloadStation::OnUnloadStationReady()
{
	bM_IsRigReady = InitRig();
	if (not bM_IsRigReady)
	{
		return;
	}
	CreatePayload();
	for (const ERTSResourceType Resource : {ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal})
	{
		const FVector& DropPoint = Resource == ERTSResourceType::Resource_Radixite ? RadixiteDropPoint : MetalDropPoint;
		FSkeletalCranePose DropPose;
		if (SolvePoseForWorldTarget(GetActorTransform().TransformPositionNoScale(DropPoint), DropPose))
		{
			M_DropPoses.Add(Resource, DropPose);
		}
	}
	M_Pose = M_RestPose;
	M_Target = M_RestPose;
	ApplyPose(M_Pose);
	UE_LOG(LogRTSSkeletalCrane, Log, TEXT("RTS_SOVCRANE ready station=%s rest_hoist=%.0f drops=%d"), *GetName(),
	       M_RestHoist, M_DropPoses.Num());
}

bool ASkeletalCraneUnloadStation::InitRig()
{
	if (not IsValid(CraneMesh) || not IsValid(CraneMesh->GetSkinnedAsset()))
	{
		RTSFunctionLibrary::ReportError("Skeletal crane unload station has no crane mesh: " + GetName());
		return false;
	}
	for (const FName Bone : {YawBone, LuffBone, TipBone, CableBone, HeadBone, JawLeftBone, JawRightBone})
	{
		if (CraneMesh->GetBoneIndex(Bone) == INDEX_NONE)
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Skeletal crane %s: bone %s is missing (see Docs/EnemyAI/SovietRefinery.md section 4)."),
				*GetName(), *Bone.ToString()));
			return false;
		}
	}
	const FReferenceSkeleton& RefSkeleton = CraneMesh->GetSkinnedAsset()->GetRefSkeleton();
	const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
	auto RestLocal = [&](const FName Bone) { return RefPose[CraneMesh->GetBoneIndex(Bone)]; };
	M_RestLocalYaw = RestLocal(YawBone);
	M_RestLocalLuff = RestLocal(LuffBone);
	M_RestLocalJawLeft = RestLocal(JawLeftBone);
	M_RestLocalJawRight = RestLocal(JawRightBone);
	for (int32 Index = 0; Index < RefPose.Num(); ++Index)
	{
		CraneMesh->ResetBoneTransformByName(RefSkeleton.GetBoneName(Index));
	}
	M_RestCSYaw = GetBoneCS(YawBone);
	M_RestCSCable = GetBoneCS(CableBone);
	M_RestCSHead = GetBoneCS(HeadBone);
	const FTransform RestTip = GetBoneCS(TipBone);
	M_RestHoist = FMath::Max(1.f, RestTip.GetLocation().Z - M_RestCSHead.GetLocation().Z);

	// The cable's length axis is the local axis that points (up or) down at rest.
	float BestAlignment = -1.f;
	for (const FVector Axis : {FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector})
	{
		const float Alignment = FMath::Abs(M_RestCSCable.GetRotation().RotateVector(Axis).Z);
		if (Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			M_CableLengthAxis = Axis;
		}
	}
	M_RestPose.Yaw = 0.f;
	M_RestPose.Luff = 0.f;
	M_RestPose.Hoist = M_RestHoist;
	M_RestPose.JawOpen = 0.f;
	return true;
}

void ASkeletalCraneUnloadStation::CreatePayload()
{
	auto MakeItem = [this](UStaticMesh* Mesh, const FTransform& Slot, const FString& Name)
	{
		UStaticMeshComponent* Item = CreatePart(Mesh, CraneMesh, FName(Name));
		Item->AttachToComponent(CraneMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, LoadSocket);
		Item->SetRelativeTransform(Slot);
		Item->SetVisibility(false);
		return Item;
	};
	for (int32 Index = 0; Index < CrystalSlots.Num() && IsValid(CrystalMesh); ++Index)
	{
		M_CrystalItems.Add(MakeItem(CrystalMesh, CrystalSlots[Index], FString::Printf(TEXT("CraneCrystal%d"), Index)));
	}
	for (int32 Index = 0; Index < BarSlots.Num() && IsValid(BarMesh); ++Index)
	{
		M_BarItems.Add(MakeItem(BarMesh, BarSlots[Index], FString::Printf(TEXT("CraneBar%d"), Index)));
	}
}

FTransform ASkeletalCraneUnloadStation::GetBoneCS(const FName Bone)
{
	return CraneMesh->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
}

void ASkeletalCraneUnloadStation::SetBoneCS(const FName Bone, const FTransform& ComponentSpace)
{
	CraneMesh->SetBoneTransformByName(Bone, ComponentSpace, EBoneSpaces::ComponentSpace);
}

void ASkeletalCraneUnloadStation::SetBoneLocal(const FName Bone, const FTransform& Local)
{
	const FName Parent = CraneMesh->GetParentBone(Bone);
	const FTransform ParentCS = Parent != NAME_None ? GetBoneCS(Parent) : FTransform::Identity;
	SetBoneCS(Bone, Local * ParentCS);
}

void ASkeletalCraneUnloadStation::ApplyYawLuff(const float Yaw, const float Luff)
{
	FTransform YawLocal = M_RestLocalYaw;
	YawLocal.SetRotation(M_RestLocalYaw.GetRotation() * FQuat(YawAxis.GetSafeNormal(), FMath::DegreesToRadians(Yaw)));
	SetBoneLocal(YawBone, YawLocal);
	FTransform LuffLocal = M_RestLocalLuff;
	LuffLocal.SetRotation(M_RestLocalLuff.GetRotation() * FQuat(LuffAxis.GetSafeNormal(), FMath::DegreesToRadians(Luff)));
	SetBoneLocal(LuffBone, LuffLocal);
}

void ASkeletalCraneUnloadStation::ApplyPose(const FSkeletalCranePose& Pose)
{
	ApplyYawLuff(Pose.Yaw, Pose.Luff);
	// The cable and grab hang plumb: they only turn with the slew, never with the luff.
	const FQuat SlewCS = GetBoneCS(YawBone).GetRotation() * M_RestCSYaw.GetRotation().Inverse();
	const FVector TipCS = GetBoneCS(TipBone).GetLocation();

	// The ropes sit in a row over the head block, so the cable turns exactly like the grab (crane_rig.json).
	const FQuat GrabTurn(FVector::UpVector, FMath::DegreesToRadians(M_AutoGrabYaw + GrabYawOffset));
	FTransform Cable = M_RestCSCable;
	Cable.SetLocation(TipCS);
	Cable.SetRotation(GrabTurn * SlewCS * M_RestCSCable.GetRotation());
	FVector CableScale = M_RestCSCable.GetScale3D();
	const float Stretch = Pose.Hoist / M_RestHoist;
	CableScale = CableScale * (FVector::OneVector - M_CableLengthAxis) + CableScale * M_CableLengthAxis * Stretch;
	Cable.SetScale3D(CableScale);
	SetBoneCS(CableBone, Cable);

	FTransform Head = M_RestCSHead;
	Head.SetRotation(GrabTurn * SlewCS * M_RestCSHead.GetRotation());
	Head.SetLocation(TipCS - FVector(0.f, 0.f, Pose.Hoist));
	SetBoneCS(HeadBone, Head);

	const float JawAngle = FMath::DegreesToRadians(CraneJawOpenAngle * FMath::Clamp(Pose.JawOpen, 0.f, 1.f));
	FTransform JawLeft = M_RestLocalJawLeft;
	JawLeft.SetRotation(M_RestLocalJawLeft.GetRotation() * FQuat(JawAxis.GetSafeNormal(), JawLeftSign * JawAngle));
	SetBoneLocal(JawLeftBone, JawLeft);
	FTransform JawRight = M_RestLocalJawRight;
	JawRight.SetRotation(M_RestLocalJawRight.GetRotation() * FQuat(JawAxis.GetSafeNormal(), JawRightSign * JawAngle));
	SetBoneLocal(JawRightBone, JawRight);
}

FVector ASkeletalCraneUnloadStation::ComponentToStation(const FVector& ComponentSpace) const
{
	return CraneMesh->GetRelativeTransform().TransformPosition(ComponentSpace);
}

FVector ASkeletalCraneUnloadStation::GetTipStationSpace(const float Yaw, const float Luff)
{
	ApplyYawLuff(Yaw, Luff);
	return ComponentToStation(GetBoneCS(TipBone).GetLocation());
}

FVector ASkeletalCraneUnloadStation::GetYawPivotStationSpace()
{
	return ComponentToStation(GetBoneCS(YawBone).GetLocation());
}

bool ASkeletalCraneUnloadStation::SolvePoseForWorldTarget(const FVector& JawBottomWorld, FSkeletalCranePose& OutPose)
{
	using namespace SkeletalCranePrivate;
	if (not bM_IsRigReady)
	{
		return false;
	}
	const FVector Target = GetActorTransform().InverseTransformPositionNoScale(JawBottomWorld);
	const FVector Pivot = GetYawPivotStationSpace();

	// Slew: how the tip's bearing moves with the yaw bone, measured on the rig.
	const float Bearing0 = HorizontalAngle(GetTipStationSpace(0.f, 0.f) - Pivot);
	const float Bearing10 = HorizontalAngle(GetTipStationSpace(10.f, 0.f) - Pivot);
	const float BearingPerDegree = FMath::FindDeltaAngleDegrees(Bearing0, Bearing10) / 10.f;
	if (FMath::Abs(BearingPerDegree) < 0.5f)
	{
		RTSFunctionLibrary::ReportError("Skeletal crane " + GetName() + ": the yaw bone does not slew the boom tip; "
			"check YawAxis against crane_rig.json.");
		ApplyPose(M_Pose);
		return false;
	}
	const float TargetBearing = HorizontalAngle(Target - Pivot);
	OutPose.Yaw = FMath::FindDeltaAngleDegrees(Bearing0, TargetBearing) / BearingPerDegree;

	// Luff: the tip's reach is monotonic over the luff range; bisect for the target's reach.
	const float TargetReach = FVector::Dist2D(Target, Pivot);
	auto ReachAt = [&](const float Luff) { return FVector::Dist2D(GetTipStationSpace(OutPose.Yaw, Luff), Pivot); };
	float Low = LuffMin;
	float High = LuffMax;
	const float ReachLow = ReachAt(Low);
	const float ReachHigh = ReachAt(High);
	const bool bReachGrowsWithLuff = ReachHigh > ReachLow;
	const float MinReach = FMath::Min(ReachLow, ReachHigh);
	const float MaxReach = FMath::Max(ReachLow, ReachHigh);
	if (TargetReach < MinReach - 1.f || TargetReach > MaxReach + 1.f)
	{
		UE_LOG(LogRTSSkeletalCrane, Warning,
		       TEXT("RTS_SOVCRANE %s: target reach %.0f outside the boom's %.0f..%.0f; using the nearest luff."),
		       *GetName(), TargetReach, MinReach, MaxReach);
	}
	for (int32 Iteration = 0; Iteration < LuffSolveIterations; ++Iteration)
	{
		const float Mid = 0.5f * (Low + High);
		const bool bTooFar = ReachAt(Mid) > TargetReach;
		if (bTooFar == bReachGrowsWithLuff)
		{
			High = Mid;
		}
		else
		{
			Low = Mid;
		}
	}
	OutPose.Luff = 0.5f * (Low + High);
	const FVector Tip = GetTipStationSpace(OutPose.Yaw, OutPose.Luff);
	OutPose.Hoist = FMath::Max(MinHoist, Tip.Z - (Target.Z + JawBottomBelowHead));
	OutPose.JawOpen = 0.f;
	ApplyPose(M_Pose);
	UE_LOG(LogRTSSkeletalCrane, Verbose, TEXT("RTS_SOVCRANE solve %s target=%s yaw=%.1f luff=%.1f hoist=%.0f"),
	       *GetName(), *Target.ToCompactString(), OutPose.Yaw, OutPose.Luff, OutPose.Hoist);
	return true;
}

// ------------------------------------------------------------------------------------------------------------ cycle

void ASkeletalCraneUnloadStation::TickUnloadStation(const float DeltaSeconds)
{
	if (not bM_IsRigReady)
	{
		return;
	}
	M_PhaseSeconds += DeltaSeconds;
	TickPhase(DeltaSeconds);
	ApplyPose(M_Pose);
	TickFallingItems(DeltaSeconds);
}

bool ASkeletalCraneUnloadStation::GetIsBayReadyForTrip() const
{
	const FUnloadBayState* Bay = GetBayState(0);
	return Bay && Bay->bIsDocked && Bay->Harvester.IsValid() && M_DropPoses.Contains(Bay->Resource);
}

float ASkeletalCraneUnloadStation::GetTravelHoistFor(const FSkeletalCranePose& Pose, const float LowerJawZStation) const
{
	float TravelJawZ = LowerJawZStation;
	TravelJawZ = FMath::Max(TravelJawZ, RadixiteDropPoint.Z);
	TravelJawZ = FMath::Max(TravelJawZ, MetalDropPoint.Z);
	TravelJawZ += TravelClearance;
	return FMath::Max(MinHoist, Pose.Hoist - (TravelJawZ - LowerJawZStation));
}

void ASkeletalCraneUnloadStation::EnterPhase(const ESkeletalCranePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseSeconds = 0.f;
	UE_LOG(LogRTSSkeletalCrane, Verbose, TEXT("RTS_SOVCRANE %s phase=%d"), *GetName(), static_cast<int32>(NewPhase));
}

void ASkeletalCraneUnloadStation::StartTripToPick()
{
	const FUnloadBayState* Bay = GetBayState(0);
	const UHarvester* Harvester = Bay ? Bay->Harvester.Get() : nullptr;
	if (not IsValid(Harvester) || not IsValid(Harvester->GetOwner()))
	{
		M_Target = M_RestPose;
		EnterPhase(ESkeletalCranePhase::ReturningToRest);
		return;
	}
	const FVector CargoWorld = Harvester->GetCargoWorldLocation();
	const FVector JawTargetWorld = CargoWorld + FVector(0.f, 0.f, PickJawBottomAboveCargo);
	if (not SolvePoseForWorldTarget(JawTargetWorld, M_PickPose))
	{
		M_Target = M_RestPose;
		EnterPhase(ESkeletalCranePhase::ReturningToRest);
		return;
	}
	// Line the jaws up along the hull: measure the jaw spread at the pick with no extra grab yaw.
	const float SavedGrabYaw = M_AutoGrabYaw;
	M_AutoGrabYaw = 0.f;
	ApplyPose(M_PickPose);
	const FVector SpreadCS = GetBoneCS(JawRightBone).GetLocation() - GetBoneCS(JawLeftBone).GetLocation();
	const FVector HullForwardCS = CraneMesh->GetComponentTransform().InverseTransformVectorNoScale(
		Harvester->GetOwner()->GetActorForwardVector());
	if (SpreadCS.Size2D() > 1.f)
	{
		float Turn = FMath::FindDeltaAngleDegrees(SkeletalCranePrivate::HorizontalAngle(SpreadCS),
		                                          SkeletalCranePrivate::HorizontalAngle(HullForwardCS));
		// The grab is symmetric: never turn more than a quarter.
		if (Turn > 90.f)
		{
			Turn -= 180.f;
		}
		else if (Turn < -90.f)
		{
			Turn += 180.f;
		}
		M_AutoGrabYaw = Turn;
	}
	else
	{
		M_AutoGrabYaw = SavedGrabYaw;
	}
	ApplyPose(M_Pose);
	M_PickJawZ = GetActorTransform().InverseTransformPositionNoScale(JawTargetWorld).Z;
	M_Target = M_PickPose;
	M_Target.Hoist = GetTravelHoistFor(M_PickPose, M_PickJawZ);
	M_Target.JawOpen = PickJawOpen;
	EnterPhase(ESkeletalCranePhase::SlewingToPick);
}

void ASkeletalCraneUnloadStation::StartTripToDrop()
{
	const FSkeletalCranePose* DropPose = M_DropPoses.Find(M_LoadResource);
	if (DropPose == nullptr)
	{
		M_Target = M_RestPose;
		EnterPhase(ESkeletalCranePhase::ReturningToRest);
		return;
	}
	const float DropJawZ = M_LoadResource == ERTSResourceType::Resource_Radixite ? RadixiteDropPoint.Z : MetalDropPoint.Z;
	M_Target = *DropPose;
	M_Target.Hoist = GetTravelHoistFor(*DropPose, DropJawZ);
	M_Target.JawOpen = 0.f;
	EnterPhase(ESkeletalCranePhase::SlewingToDrop);
}

bool ASkeletalCraneUnloadStation::MoveTowardsTarget(const float DeltaSeconds)
{
	using namespace SkeletalCranePrivate;
	bool bYaw = false, bLuff = false, bHoist = false, bJaw = false;
	M_Pose.Yaw = MoveAngle(M_Pose.Yaw, M_Target.Yaw, SlewSpeed, EaseDegrees, DeltaSeconds, bYaw);
	M_Pose.Luff = MoveAngle(M_Pose.Luff, M_Target.Luff, LuffSpeed, EaseDegrees * 0.5f, DeltaSeconds, bLuff);
	M_Pose.Hoist = MoveLinear(M_Pose.Hoist, M_Target.Hoist, CraneHoistSpeed, HoistEaseDistance, DeltaSeconds,
	                          HoistTolerance, bHoist);
	const float JawSeconds = M_Target.JawOpen > M_Pose.JawOpen ? ReleaseSeconds : GrabSeconds;
	M_Pose.JawOpen = FMath::FInterpConstantTo(M_Pose.JawOpen, M_Target.JawOpen, DeltaSeconds,
	                                          1.f / FMath::Max(JawSeconds, 0.05f));
	bJaw = FMath::IsNearlyEqual(M_Pose.JawOpen, M_Target.JawOpen, 0.001f);
	return bYaw && bLuff && bHoist && bJaw;
}

void ASkeletalCraneUnloadStation::TickPhase(const float DeltaSeconds)
{
	switch (M_Phase)
	{
	case ESkeletalCranePhase::Idle:
		if (GetIsBayReadyForTrip())
		{
			StartTripToPick();
		}
		return;
	case ESkeletalCranePhase::SlewingToPick:
		if (not GetIsBayReadyForTrip())
		{
			M_Target = M_RestPose;
			EnterPhase(ESkeletalCranePhase::ReturningToRest);
			return;
		}
		if (MoveTowardsTarget(DeltaSeconds))
		{
			M_Target.Hoist = M_PickPose.Hoist;
			EnterPhase(ESkeletalCranePhase::LoweringToPick);
		}
		return;
	case ESkeletalCranePhase::LoweringToPick:
		if (not GetIsBayReadyForTrip())
		{
			M_Target = M_RestPose;
			EnterPhase(ESkeletalCranePhase::ReturningToRest);
			return;
		}
		if (MoveTowardsTarget(DeltaSeconds))
		{
			M_Target.JawOpen = 0.f;
			EnterPhase(ESkeletalCranePhase::Grabbing);
		}
		return;
	case ESkeletalCranePhase::Grabbing:
		if (MoveTowardsTarget(DeltaSeconds))
		{
			const FUnloadBayState* Bay = GetBayState(0);
			M_LoadResource = Bay ? Bay->Resource : ERTSResourceType::Resource_None;
			bM_IsCarrying = GetIsBayReadyForTrip();
			ShowLoad(M_LoadResource, bM_IsCarrying);
			M_Target.Hoist = GetTravelHoistFor(M_PickPose, M_PickJawZ);
			EnterPhase(ESkeletalCranePhase::HoistingFromPick);
		}
		return;
	case ESkeletalCranePhase::HoistingFromPick:
		if (MoveTowardsTarget(DeltaSeconds))
		{
			if (bM_IsCarrying)
			{
				StartTripToDrop();
				return;
			}
			M_Target = M_RestPose;
			EnterPhase(ESkeletalCranePhase::ReturningToRest);
		}
		return;
	case ESkeletalCranePhase::SlewingToDrop:
		if (MoveTowardsTarget(DeltaSeconds))
		{
			if (const FSkeletalCranePose* DropPose = M_DropPoses.Find(M_LoadResource))
			{
				M_Target.Hoist = DropPose->Hoist;
			}
			EnterPhase(ESkeletalCranePhase::LoweringToDrop);
		}
		return;
	case ESkeletalCranePhase::LoweringToDrop:
		if (MoveTowardsTarget(DeltaSeconds))
		{
			M_Target.JawOpen = 1.f;
			EnterPhase(ESkeletalCranePhase::Releasing);
		}
		return;
	case ESkeletalCranePhase::Releasing:
		if (MoveTowardsTarget(DeltaSeconds))
		{
			ReleaseLoad();
			const FSkeletalCranePose* DropPose = M_DropPoses.Find(M_LoadResource);
			const float DropJawZ = M_LoadResource == ERTSResourceType::Resource_Radixite
				                       ? RadixiteDropPoint.Z
				                       : MetalDropPoint.Z;
			M_Target.Hoist = DropPose ? GetTravelHoistFor(*DropPose, DropJawZ) : M_RestHoist;
			EnterPhase(ESkeletalCranePhase::HoistingFromDrop);
		}
		return;
	case ESkeletalCranePhase::HoistingFromDrop:
		if (MoveTowardsTarget(DeltaSeconds))
		{
			if (GetIsBayReadyForTrip())
			{
				StartTripToPick();
				return;
			}
			M_Target = M_RestPose;
			EnterPhase(ESkeletalCranePhase::ReturningToRest);
		}
		return;
	case ESkeletalCranePhase::ReturningToRest:
		if (GetIsBayReadyForTrip())
		{
			StartTripToPick();
			return;
		}
		if (MoveTowardsTarget(DeltaSeconds))
		{
			EnterPhase(ESkeletalCranePhase::Idle);
		}
		return;
	}
}

void ASkeletalCraneUnloadStation::ReleaseLoad()
{
	using namespace SkeletalCranePrivate;
	const FVector& DropPoint = M_LoadResource == ERTSResourceType::Resource_Radixite ? RadixiteDropPoint : MetalDropPoint;
	const FVector DropWorld = GetActorTransform().TransformPositionNoScale(DropPoint);
	const TArray<TWeakObjectPtr<UStaticMeshComponent>>& Items =
		M_LoadResource == ERTSResourceType::Resource_Radixite ? M_CrystalItems : M_BarItems;
	for (const TWeakObjectPtr<UStaticMeshComponent>& Item : Items)
	{
		if (not Item.IsValid() || not Item->IsVisible())
		{
			continue;
		}
		Item->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		FSkeletalCraneFallingItem Falling;
		Falling.Component = Item;
		Falling.StopZ = DropWorld.Z - ItemFallDepth;
		M_FallingItems.Add(Falling);
	}
	if (bM_IsCarrying)
	{
		// Every landed grab credits one chunk; on the last one the harvester leaves and the crane goes back to rest.
		DeliverBayChunk(0, DropWorld + FVector(0.f, 0.f, BinTextHeight));
	}
	bM_IsCarrying = false;
}

void ASkeletalCraneUnloadStation::ShowLoad(const ERTSResourceType Resource, const bool bShow)
{
	for (const TWeakObjectPtr<UStaticMeshComponent>& Item : M_CrystalItems)
	{
		if (Item.IsValid())
		{
			Item->SetVisibility(bShow && Resource == ERTSResourceType::Resource_Radixite);
		}
	}
	for (const TWeakObjectPtr<UStaticMeshComponent>& Item : M_BarItems)
	{
		if (Item.IsValid())
		{
			Item->SetVisibility(bShow && Resource == ERTSResourceType::Resource_Metal);
		}
	}
}

void ASkeletalCraneUnloadStation::TickFallingItems(const float DeltaSeconds)
{
	using namespace SkeletalCranePrivate;
	for (int32 Index = M_FallingItems.Num() - 1; Index >= 0; --Index)
	{
		FSkeletalCraneFallingItem& Falling = M_FallingItems[Index];
		UStaticMeshComponent* Item = Falling.Component.Get();
		if (not IsValid(Item))
		{
			M_FallingItems.RemoveAtSwap(Index);
			continue;
		}
		Falling.VelocityZ -= Gravity * DeltaSeconds;
		FVector Location = Item->GetComponentLocation();
		Location.Z += Falling.VelocityZ * DeltaSeconds;
		if (Location.Z > Falling.StopZ)
		{
			Item->SetWorldLocation(Location);
			continue;
		}
		// Landed: hide it and hang it back in the grab for the next load.
		Item->SetVisibility(false);
		Item->AttachToComponent(CraneMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, LoadSocket);
		const int32 CrystalIndex = M_CrystalItems.IndexOfByKey(Falling.Component);
		const int32 BarIndex = M_BarItems.IndexOfByKey(Falling.Component);
		if (CrystalSlots.IsValidIndex(CrystalIndex))
		{
			Item->SetRelativeTransform(CrystalSlots[CrystalIndex]);
		}
		else if (BarSlots.IsValidIndex(BarIndex))
		{
			Item->SetRelativeTransform(BarSlots[BarIndex]);
		}
		M_FallingItems.RemoveAtSwap(Index);
	}
}

FVector ASkeletalCraneUnloadStation::GetCraneGrabWorldLocation() const
{
	if (not bM_IsRigReady)
	{
		return GetActorLocation();
	}
	return CraneMesh->GetBoneLocationByName(HeadBone, EBoneSpaces::WorldSpace);
}

EUnloadTrolleyPhase ASkeletalCraneUnloadStation::GetTrolleyPhase(const int32 BayIndex) const
{
	if (BayIndex != 0)
	{
		return EUnloadTrolleyPhase::Idle;
	}
	switch (M_Phase)
	{
	case ESkeletalCranePhase::SlewingToPick: return EUnloadTrolleyPhase::MovingToBay;
	case ESkeletalCranePhase::LoweringToPick: return EUnloadTrolleyPhase::LoweringToPick;
	case ESkeletalCranePhase::Grabbing: return EUnloadTrolleyPhase::Grabbing;
	case ESkeletalCranePhase::HoistingFromPick: return EUnloadTrolleyPhase::HoistingFromPick;
	case ESkeletalCranePhase::SlewingToDrop: return EUnloadTrolleyPhase::MovingToBin;
	case ESkeletalCranePhase::LoweringToDrop: return EUnloadTrolleyPhase::LoweringToDrop;
	case ESkeletalCranePhase::Releasing: return EUnloadTrolleyPhase::Releasing;
	case ESkeletalCranePhase::HoistingFromDrop: return EUnloadTrolleyPhase::HoistingFromDrop;
	case ESkeletalCranePhase::ReturningToRest: return EUnloadTrolleyPhase::ReturningToPark;
	default: return EUnloadTrolleyPhase::Idle;
	}
}
