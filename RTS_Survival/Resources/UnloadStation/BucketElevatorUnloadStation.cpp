// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BucketElevatorUnloadStation.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSBucketElevator, Log, All);

namespace BucketElevatorPrivate
{
	constexpr float Gravity = 980.f;
	// Bars slide down the apron with this share of the free-fall acceleration along it (friction).
	constexpr float ApronSlideGrip = 0.55f;
	constexpr float ApronEntrySpeed = 60.f;
	// Bars leave their bucket this far round the head pulley (deg), once the mouth faces down.
	constexpr float DischargeAngleDeg = 70.f;
	// Falling bars stop this far above the bin socket, inside the fill (as on the HQ crane).
	constexpr float BarStopAboveBin = 45.f;
	// Pour path from inside the hopper over its rear rim (cm, harvester forward / up from the apron tip).
	constexpr float PourStartAhead = 45.f;
	constexpr float PourStartUp = 15.f;
	constexpr float PourControlAhead = 10.f;
	constexpr float PourControlUp = 50.f;
	constexpr float BarAboveApronFloor = 5.f;

	/** @return Whether moving Delta along the loop from Prev passes Mark. */
	bool GetPassesMark(const float Prev, const float Delta, const float Mark, const float Loop)
	{
		const float Ahead = FMath::Fmod(Mark - Prev + Loop, Loop);
		return Ahead < Delta;
	}

	FVector QuadraticBezier(const FVector& A, const FVector& B, const FVector& C, const float T)
	{
		const float U = 1.f - T;
		return U * U * A + 2.f * U * T * B + T * T * C;
	}
}

ABucketElevatorUnloadStation::ABucketElevatorUnloadStation()
{
	BuildMetalDepotDefaults();
}

void ABucketElevatorUnloadStation::BuildMetalDepotDefaults()
{
	// The "rail" is the boom's ground line from the boot to the head; the base class only uses it to find the bin.
	RailControlPoints = {
		FVector(BootPulleyCentre.X, BootPulleyCentre.Y, 0.f),
		FVector(BootPulleyCentre.X, (BootPulleyCentre.Y + HeadPulleyCentre.Y) * 0.5f, 0.f),
		FVector(HeadPulleyCentre.X, HeadPulleyCentre.Y, 0.f)
	};
	bRailIsClosedLoop = false;
	RailHeight = HeadPulleyCentre.Z;

	// One bay north of the sandbags: the harvester faces away from the depot with its rear bumper on the pick point.
	FUnloadBaySettings Bay;
	Bay.PickPoint = FVector(-22.f, -470.f, 0.f);
	Bay.DockYaw = -90.f;
	Bay.RunDirection = EUnloadTrolleyRunDirection::AlongRail;
	Bay.ParkOffset = 0.f;
	BaySettings = {Bay};

	FUnloadBinSettings Metal;
	Metal.Resource = ERTSResourceType::Resource_Metal;
	Metal.SocketName = TEXT("Metal");
	Metal.FallbackLocation = FVector(-128.f, 0.f, 136.f);
	BinSettings = {Metal};

	// The old drop-off point (-92,-352) is under the boot: bay-less harvesters unload east of the depot instead,
	// ~11 m from the docked hull (their path ends up to ~4.6 m short of the point).
	bUseOverflowDropOffPoint = true;
	OverflowDropOffPoint = FVector(1050.f, -750.f, 0.f);

	// Four credited chunks per unload (one pop-up each), eight bars each.
	TripsPerUnload = 4;
}

// ---------------------------------------------------------------------------------------------------------- geometry

float ABucketElevatorUnloadStation::GetBoomLength() const
{
	return FVector::Dist(BootPulleyCentre, HeadPulleyCentre);
}

FVector ABucketElevatorUnloadStation::GetBoomDirection() const
{
	return (HeadPulleyCentre - BootPulleyCentre).GetSafeNormal();
}

FVector ABucketElevatorUnloadStation::GetBoomNormal() const
{
	// Out from the up strand: towards the bay and up (the boom lies in the YZ plane).
	const FVector Direction = GetBoomDirection();
	return FVector(0.f, -Direction.Z, Direction.Y);
}

float ABucketElevatorUnloadStation::GetLoopLength() const
{
	return 2.f * GetBoomLength() + 2.f * PI * BeltRadius;
}

void ABucketElevatorUnloadStation::GetBeltFrame(float S, FVector& OutPoint, FVector& OutTravel,
                                                FVector& OutNormal) const
{
	// Same loop as build_bucket_elevator.py's belt(): up strand, head half wrap, return strand, boot half wrap.
	const float Length = GetBoomLength();
	const FVector Along = GetBoomDirection();
	const FVector Out = GetBoomNormal();
	const float HalfWrap = PI * BeltRadius;
	S = FMath::Fmod(FMath::Fmod(S, GetLoopLength()) + GetLoopLength(), GetLoopLength());
	if (S < Length)
	{
		OutPoint = BootPulleyCentre + Along * S + Out * BeltRadius;
		OutTravel = Along;
		OutNormal = Out;
		return;
	}
	S -= Length;
	if (S < HalfWrap)
	{
		const float Angle = S / BeltRadius;
		OutNormal = Out * FMath::Cos(Angle) + Along * FMath::Sin(Angle);
		OutPoint = HeadPulleyCentre + OutNormal * BeltRadius;
		OutTravel = Along * FMath::Cos(Angle) - Out * FMath::Sin(Angle);
		return;
	}
	S -= HalfWrap;
	if (S < Length)
	{
		OutPoint = HeadPulleyCentre - Along * S - Out * BeltRadius;
		OutTravel = -Along;
		OutNormal = -Out;
		return;
	}
	const float Angle = FMath::Min(S - Length, HalfWrap) / BeltRadius;
	OutNormal = -Out * FMath::Cos(Angle) - Along * FMath::Sin(Angle);
	OutPoint = BootPulleyCentre + OutNormal * BeltRadius;
	OutTravel = Out * FMath::Sin(Angle) - Along * FMath::Cos(Angle);
}

float ABucketElevatorUnloadStation::GetLoadMark() const
{
	// The bottom of the boot wrap, inside the casing.
	return 2.f * GetBoomLength() + 1.5f * PI * BeltRadius;
}

float ABucketElevatorUnloadStation::GetDischargeMark() const
{
	return GetBoomLength() + BeltRadius * FMath::DegreesToRadians(BucketElevatorPrivate::DischargeAngleDeg);
}

FVector2D ABucketElevatorUnloadStation::GetHarvesterDockPoint(const UHarvester& Harvester) const
{
	// The rear bumper (lowest X of the hull, the mesh that has the cargo socket) docks on the pick point.
	const AActor* HarvesterActor = Harvester.GetOwner();
	const UMeshComponent* Hull = Harvester.GetCargoMeshComponent();
	if (not IsValid(HarvesterActor) || not IsValid(Hull))
	{
		return Super::GetHarvesterDockPoint(Harvester);
	}
	const FTransform HullInActor = Hull->GetComponentTransform().GetRelativeTransform(
		HarvesterActor->GetActorTransform());
	return FVector2D(Hull->CalcBounds(HullInActor).GetBox().Min.X, 0.f);
}

// ------------------------------------------------------------------------------------------------------------- setup

void ABucketElevatorUnloadStation::OnUnloadStationReady()
{
	if (M_Buckets.Num() > 0)
	{
		return;
	}
	for (int32 Index = 0; Index < BucketCount; ++Index)
	{
		FElevatorBucket Bucket;
		UStaticMeshComponent* BucketPart = CreatePart(BucketMesh, StationRoot, FName(FString::Printf(TEXT("Bucket%d"), Index)));
		UStaticMeshComponent* Bar = CreatePart(BarMesh, BucketPart, FName(FString::Printf(TEXT("BucketBar%d"), Index)));
		Bar->SetRelativeTransform(BucketBarSlot);
		Bar->SetVisibility(false);
		Bucket.Bucket = BucketPart;
		Bucket.Bar = Bar;
		M_Buckets.Add(Bucket);
	}
	UStaticMeshComponent* HeadPulley = CreatePart(PulleyMesh, StationRoot, TEXT("HeadPulley"));
	HeadPulley->SetRelativeLocation(HeadPulleyCentre);
	M_HeadPulley = HeadPulley;
	UStaticMeshComponent* BootPulley = CreatePart(PulleyMesh, StationRoot, TEXT("BootPulley"));
	BootPulley->SetRelativeLocation(BootPulleyCentre);
	M_BootPulley = BootPulley;
	M_Apron = CreatePart(ApronMesh, StationRoot, TEXT("FeedApron"));
	for (int32 Index = 0; Index < LooseBarPoolSize; ++Index)
	{
		FElevatorBar Loose;
		UStaticMeshComponent* Bar = CreatePart(BarMesh, StationRoot, FName(FString::Printf(TEXT("LooseBar%d"), Index)));
		Bar->SetRelativeScale3D(BucketBarSlot.GetScale3D());
		Bar->SetVisibility(false);
		Loose.Component = Bar;
		M_LooseBars.Add(Loose);
	}
	if (const FUnloadBayState* Bay = GetBayState(0))
	{
		const FVector* BinWorld = Bay->BinWorldLocations.Find(ERTSResourceType::Resource_Metal);
		M_BinWorld = BinWorld ? *BinWorld : GetActorTransform().TransformPositionNoScale(BinSettings[0].FallbackLocation);
	}
	M_BinStopZ = M_BinWorld.Z + BucketElevatorPrivate::BarStopAboveBin;
	UpdateBuckets();
	UpdateApron();
	UE_LOG(LogRTSBucketElevator, Log, TEXT("RTS_ELEVATOR ready buckets=%d loop_cm=%.0f boom_cm=%.0f"), M_Buckets.Num(),
	       GetLoopLength(), GetBoomLength());
}

// -------------------------------------------------------------------------------------------------------------- tick

void ABucketElevatorUnloadStation::TickUnloadStation(const float DeltaSeconds)
{
	TickState(DeltaSeconds);
	TickBelt(DeltaSeconds);
	TickLooseBars(DeltaSeconds);
	UpdateApron();
	UpdateBuckets();
}

void ABucketElevatorUnloadStation::TickState(const float DeltaSeconds)
{
	const FUnloadBayState* Bay = GetBayState(0);
	const UHarvester* Harvester = Bay ? Bay->Harvester.Get() : nullptr;
	const bool bIsDocked = Bay && Bay->bIsDocked && IsValid(Harvester);
	if (bM_IsUnloadActive && not bIsDocked)
	{
		// The harvester left (new order, died) or the station ended the unload: nothing more was credited.
		StopUnload();
	}
	switch (M_State)
	{
	case EBucketElevatorState::Idle:
	case EBucketElevatorState::RaisingApron:
		if (bIsDocked && not bM_IsUnloadActive)
		{
			StartUnload(*Harvester);
			return;
		}
		if (M_State == EBucketElevatorState::RaisingApron)
		{
			M_ApronAlpha = FMath::Max(0.f, M_ApronAlpha - DeltaSeconds / ApronSeconds);
			if (M_ApronAlpha <= 0.f)
			{
				M_State = EBucketElevatorState::Idle;
			}
		}
		return;
	case EBucketElevatorState::LoweringApron:
		M_ApronAlpha = FMath::Min(1.f, M_ApronAlpha + DeltaSeconds / ApronSeconds);
		if (M_ApronAlpha >= 1.f)
		{
			M_State = EBucketElevatorState::Feeding;
			M_FeedTimer = 0.f;
		}
		return;
	case EBucketElevatorState::Feeding:
		M_FeedTimer -= DeltaSeconds;
		if (M_FeedTimer <= 0.f && M_BarsFed < BarsPerUnload)
		{
			FeedBar();
			// One bar per bucket at full belt speed.
			M_FeedTimer += GetLoopLength() / BucketCount / BeltSpeed;
		}
		if (M_BarsFed >= BarsPerUnload && not GetHasBarsOnApron())
		{
			M_State = EBucketElevatorState::RaisingApron;
		}
		return;
	}
}

void ABucketElevatorUnloadStation::StartUnload(const UHarvester& Harvester)
{
	const AActor* HarvesterActor = Harvester.GetOwner();
	if (not IsValid(HarvesterActor))
	{
		return;
	}
	const FTransform StationTransform(GetActorRotation(), GetActorLocation());
	const FTransform HarvesterTransform(HarvesterActor->GetActorRotation(), HarvesterActor->GetActorLocation());
	FVector Forward = HarvesterActor->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	const FVector2D Rear = GetHarvesterDockPoint(Harvester);
	FVector TipWorld = HarvesterTransform.TransformPosition(FVector(Rear.X, 0.f, 0.f)) + Forward * ApronTipAheadOfRear;
	TipWorld.Z = Harvester.GetCargoWorldLocation().Z + ApronTipAboveCargo;

	// The apron swings in the boom's plane; keep only the tip's offset in that plane.
	FVector TipLocal = StationTransform.InverseTransformPosition(TipWorld);
	TipLocal.X = ApronHinge.X;
	const FVector Lowered = TipLocal - ApronHinge;
	M_ApronLoweredLength = FMath::Max(10.f, Lowered.Size());
	M_ApronLoweredDirection = Lowered.GetSafeNormal();
	const FVector TipInPlane = StationTransform.TransformPosition(TipLocal);
	M_PourStart = TipInPlane + Forward * BucketElevatorPrivate::PourStartAhead
		+ FVector(0.f, 0.f, BucketElevatorPrivate::PourStartUp);
	M_PourControl = TipInPlane + Forward * BucketElevatorPrivate::PourControlAhead
		+ FVector(0.f, 0.f, BucketElevatorPrivate::PourControlUp);

	bM_IsUnloadActive = true;
	M_BarsFed = 0;
	M_BarsLanded = 0;
	M_BootStock = 0;
	M_FeedTimer = 0.f;
	// Bars left on the belt by an earlier unload (rare) vanish, so they cannot credit this one.
	HideAllBars();
	M_State = EBucketElevatorState::LoweringApron;
	UE_LOG(LogRTSBucketElevator, Log,
	       TEXT("RTS_ELEVATOR start harvester=%s rear_x=%.0f apron_len=%.0f tip_local=(%.0f,%.0f,%.0f)"),
	       *GetNameSafe(HarvesterActor), Rear.X, M_ApronLoweredLength, TipLocal.X, TipLocal.Y, TipLocal.Z);
}

void ABucketElevatorUnloadStation::StopUnload()
{
	bM_IsUnloadActive = false;
	M_BarsFed = BarsPerUnload;
	HideAllBars();
	M_State = M_ApronAlpha > 0.f ? EBucketElevatorState::RaisingApron : EBucketElevatorState::Idle;
	UE_LOG(LogRTSBucketElevator, Log, TEXT("RTS_ELEVATOR stopped landed=%d"), M_BarsLanded);
}

void ABucketElevatorUnloadStation::HideAllBars()
{
	M_BootStock = 0;
	for (FElevatorBucket& Bucket : M_Buckets)
	{
		Bucket.bIsLoaded = false;
		if (UStaticMeshComponent* Bar = Bucket.Bar.Get())
		{
			Bar->SetVisibility(false);
		}
	}
	for (FElevatorBar& Loose : M_LooseBars)
	{
		Loose.Motion = EElevatorBarMotion::Inactive;
		if (UStaticMeshComponent* Bar = Loose.Component.Get())
		{
			Bar->SetVisibility(false);
		}
	}
}

bool ABucketElevatorUnloadStation::GetHasBarsOnApron() const
{
	for (const FElevatorBar& Loose : M_LooseBars)
	{
		if (Loose.Motion == EElevatorBarMotion::Pouring || Loose.Motion == EElevatorBarMotion::Sliding)
		{
			return true;
		}
	}
	return false;
}

bool ABucketElevatorUnloadStation::GetHasBarsInSystem() const
{
	if (M_BootStock > 0 || GetHasBarsOnApron())
	{
		return true;
	}
	for (const FElevatorBar& Loose : M_LooseBars)
	{
		if (Loose.Motion == EElevatorBarMotion::Falling)
		{
			return true;
		}
	}
	for (const FElevatorBucket& Bucket : M_Buckets)
	{
		if (Bucket.bIsLoaded)
		{
			return true;
		}
	}
	return false;
}

void ABucketElevatorUnloadStation::FeedBar()
{
	++M_BarsFed;
	for (FElevatorBar& Loose : M_LooseBars)
	{
		UStaticMeshComponent* Bar = Loose.Component.Get();
		if (Loose.Motion != EElevatorBarMotion::Inactive || not IsValid(Bar))
		{
			continue;
		}
		Loose.Motion = EElevatorBarMotion::Pouring;
		Loose.Seconds = 0.f;
		Bar->SetWorldLocation(M_PourStart);
		Bar->SetVisibility(true);
		return;
	}
	// Every loose bar is busy: this one goes straight into the boot.
	++M_BootStock;
}

void ABucketElevatorUnloadStation::TickBelt(const float DeltaSeconds)
{
	using namespace BucketElevatorPrivate;
	const bool bIsRunning = M_State != EBucketElevatorState::Idle || GetHasBarsInSystem();
	M_BeltCurrentSpeed = FMath::FInterpConstantTo(M_BeltCurrentSpeed, bIsRunning ? BeltSpeed : 0.f, DeltaSeconds,
	                                              BeltSpeed / BeltRampSeconds);
	const float Delta = M_BeltCurrentSpeed * DeltaSeconds;
	if (Delta <= 0.f || M_Buckets.Num() == 0)
	{
		return;
	}
	const float Loop = GetLoopLength();
	const float Pitch = Loop / M_Buckets.Num();
	const float LoadMark = GetLoadMark();
	const float DischargeMark = GetDischargeMark();
	for (int32 Index = 0; Index < M_Buckets.Num(); ++Index)
	{
		FElevatorBucket& Bucket = M_Buckets[Index];
		const float Prev = FMath::Fmod(M_BeltDistance + Index * Pitch, Loop);
		if (Bucket.bIsLoaded && GetPassesMark(Prev, Delta, DischargeMark, Loop))
		{
			FVector Point, Travel, Normal;
			GetBeltFrame(DischargeMark, Point, Travel, Normal);
			DischargeBucket(Bucket, Travel);
		}
		if (not Bucket.bIsLoaded && M_BootStock > 0 && GetPassesMark(Prev, Delta, LoadMark, Loop))
		{
			--M_BootStock;
			Bucket.bIsLoaded = true;
			if (UStaticMeshComponent* Bar = Bucket.Bar.Get())
			{
				Bar->SetVisibility(true);
			}
		}
	}
	M_BeltDistance = FMath::Fmod(M_BeltDistance + Delta, Loop);
	if (UStaticMeshComponent* Pulley = M_HeadPulley.Get())
	{
		// The pulley angle follows the belt without the loop wrap, so the spokes never jump.
		const FQuat Spin(FVector::XAxisVector, -Delta / BeltRadius);
		Pulley->SetRelativeRotation(Spin * Pulley->GetRelativeRotation().Quaternion());
	}
	if (UStaticMeshComponent* Pulley = M_BootPulley.Get())
	{
		const FQuat Spin(FVector::XAxisVector, -Delta / BeltRadius);
		Pulley->SetRelativeRotation(Spin * Pulley->GetRelativeRotation().Quaternion());
	}
}

void ABucketElevatorUnloadStation::DischargeBucket(FElevatorBucket& Bucket, const FVector& TravelLocal)
{
	Bucket.bIsLoaded = false;
	UStaticMeshComponent* BucketBar = Bucket.Bar.Get();
	if (IsValid(BucketBar))
	{
		BucketBar->SetVisibility(false);
	}
	for (FElevatorBar& Loose : M_LooseBars)
	{
		UStaticMeshComponent* Bar = Loose.Component.Get();
		if (Loose.Motion != EElevatorBarMotion::Inactive || not IsValid(Bar))
		{
			continue;
		}
		if (IsValid(BucketBar))
		{
			Bar->SetWorldLocationAndRotation(BucketBar->GetComponentLocation(), BucketBar->GetComponentQuat());
		}
		Loose.Motion = EElevatorBarMotion::Falling;
		Loose.Velocity = GetActorQuat().RotateVector(TravelLocal) * M_BeltCurrentSpeed;
		Bar->SetVisibility(true);
		return;
	}
	OnBarLanded();
}

void ABucketElevatorUnloadStation::TickLooseBars(const float DeltaSeconds)
{
	using namespace BucketElevatorPrivate;
	const FVector Direction = GetApronDirection();
	const FQuat StationQuat = GetActorQuat();
	const FVector ApronWorldDirection = StationQuat.RotateVector(Direction);
	const FVector FloorWorld = StationQuat.RotateVector(GetApronFloorNormal(Direction));
	const FQuat BarOnApron = StationQuat * FRotationMatrix::MakeFromXZ(Direction, GetApronFloorNormal(Direction)).ToQuat()
		* FQuat(FVector::ZAxisVector, HALF_PI);
	const FVector Tip = GetApronTipWorld();
	const float ApronLength = GetApronLength();
	for (FElevatorBar& Loose : M_LooseBars)
	{
		UStaticMeshComponent* Bar = Loose.Component.Get();
		if (Loose.Motion == EElevatorBarMotion::Inactive || not IsValid(Bar))
		{
			continue;
		}
		switch (Loose.Motion)
		{
		case EElevatorBarMotion::Pouring:
			{
				Loose.Seconds += DeltaSeconds;
				const float Alpha = FMath::Min(1.f, Loose.Seconds / PourSeconds);
				const FVector Location = QuadraticBezier(M_PourStart, M_PourControl, Tip + FloorWorld * BarAboveApronFloor,
				                                         Alpha);
				Bar->SetWorldLocationAndRotation(Location, BarOnApron);
				if (Alpha >= 1.f)
				{
					Loose.Motion = EElevatorBarMotion::Sliding;
					Loose.Distance = 0.f;
					Loose.Speed = ApronEntrySpeed;
				}
				break;
			}
		case EElevatorBarMotion::Sliding:
			{
				// Down the apron from the tip to the hinge, where it drops into the boot.
				const float Steepness = FMath::Max(0.1f, Direction.Z);
				Loose.Speed += Gravity * Steepness * ApronSlideGrip * DeltaSeconds;
				Loose.Distance += Loose.Speed * DeltaSeconds;
				if (Loose.Distance >= ApronLength)
				{
					Loose.Motion = EElevatorBarMotion::Inactive;
					Bar->SetVisibility(false);
					++M_BootStock;
					break;
				}
				const FVector Location = Tip - ApronWorldDirection * Loose.Distance + FloorWorld * BarAboveApronFloor;
				Bar->SetWorldLocationAndRotation(Location, BarOnApron);
				break;
			}
		case EElevatorBarMotion::Falling:
			{
				Loose.Velocity.Z -= Gravity * DeltaSeconds;
				const FVector Location = Bar->GetComponentLocation() + Loose.Velocity * DeltaSeconds;
				if (Location.Z <= M_BinStopZ)
				{
					Loose.Motion = EElevatorBarMotion::Inactive;
					Bar->SetVisibility(false);
					OnBarLanded();
					break;
				}
				Bar->SetWorldLocation(Location);
				break;
			}
		case EElevatorBarMotion::Inactive:
			break;
		}
	}
}

void ABucketElevatorUnloadStation::OnBarLanded()
{
	++M_BarsLanded;
	if (not bM_IsUnloadActive)
	{
		return;
	}
	const int32 BarsPerChunk = FMath::DivideAndRoundUp(BarsPerUnload, FMath::Max(1, TripsPerUnload));
	const bool bIsLastBar = M_BarsLanded >= BarsPerUnload;
	if (M_BarsLanded % BarsPerChunk != 0 && not bIsLastBar)
	{
		return;
	}
	// The last load takes whatever is left, so the harvester never waits on a rounding remainder.
	const EUnloadChunkResult Result = DeliverBayChunk(0, M_BinWorld + FVector(0.f, 0.f, BinTextHeight), bIsLastBar);
	UE_LOG(LogRTSBucketElevator, Log, TEXT("RTS_ELEVATOR chunk landed=%d result=%d"), M_BarsLanded,
	       static_cast<int32>(Result));
	if (Result == EUnloadChunkResult::CargoEmpty)
	{
		// Done: the bars still on the belt (an emptier harvester than usual) run out without crediting.
		bM_IsUnloadActive = false;
		M_BarsFed = BarsPerUnload;
		return;
	}
	if (Result == EUnloadChunkResult::Stopped)
	{
		StopUnload();
	}
}

// ----------------------------------------------------------------------------------------------------------- visuals

FVector ABucketElevatorUnloadStation::GetApronDirection() const
{
	// Swing in the YZ plane between the raised and the lowered angle (from the bay side of the boot, up).
	const FVector Stowed = ApronStowedDirection.GetSafeNormal();
	const float StowedAngle = FMath::Atan2(Stowed.Z, -Stowed.Y);
	const float LoweredAngle = FMath::Atan2(M_ApronLoweredDirection.Z, -M_ApronLoweredDirection.Y);
	const float Angle = FMath::Lerp(StowedAngle, LoweredAngle, FMath::SmoothStep(0.f, 1.f, M_ApronAlpha));
	return FVector(0.f, -FMath::Cos(Angle), FMath::Sin(Angle));
}

float ABucketElevatorUnloadStation::GetApronLength() const
{
	return FMath::Lerp(ApronMeshLength, M_ApronLoweredLength, FMath::SmoothStep(0.f, 1.f, M_ApronAlpha));
}

FVector ABucketElevatorUnloadStation::GetApronFloorNormal(const FVector& Direction) const
{
	const FVector Normal = FVector::CrossProduct(Direction, FVector::XAxisVector).GetSafeNormal();
	return Normal.Z < 0.f ? -Normal : Normal;
}

FVector ABucketElevatorUnloadStation::GetApronTipWorld() const
{
	return GetActorTransform().TransformPositionNoScale(ApronHinge + GetApronDirection() * GetApronLength());
}

void ABucketElevatorUnloadStation::UpdateApron() const
{
	UStaticMeshComponent* Apron = M_Apron.Get();
	if (not IsValid(Apron))
	{
		return;
	}
	const FVector Direction = GetApronDirection();
	Apron->SetRelativeLocationAndRotation(ApronHinge,
	                                      FRotationMatrix::MakeFromXZ(Direction, GetApronFloorNormal(Direction)).Rotator());
	Apron->SetRelativeScale3D(FVector(GetApronLength() / ApronMeshLength, 1.f, 1.f));
}

void ABucketElevatorUnloadStation::UpdateBuckets() const
{
	if (M_Buckets.Num() == 0)
	{
		return;
	}
	const float Pitch = GetLoopLength() / M_Buckets.Num();
	for (int32 Index = 0; Index < M_Buckets.Num(); ++Index)
	{
		UStaticMeshComponent* Bucket = M_Buckets[Index].Bucket.Get();
		if (not IsValid(Bucket))
		{
			continue;
		}
		FVector Point, Travel, Normal;
		GetBeltFrame(M_BeltDistance + Index * Pitch, Point, Travel, Normal);
		Bucket->SetRelativeLocationAndRotation(Point, FRotationMatrix::MakeFromXZ(Travel, Normal).Rotator());
	}
}

EUnloadTrolleyPhase ABucketElevatorUnloadStation::GetTrolleyPhase(const int32 BayIndex) const
{
	if (BayIndex != 0)
	{
		return EUnloadTrolleyPhase::Idle;
	}
	switch (M_State)
	{
	case EBucketElevatorState::LoweringApron:
		return EUnloadTrolleyPhase::LoweringToPick;
	case EBucketElevatorState::Feeding:
		return EUnloadTrolleyPhase::MovingToBin;
	case EBucketElevatorState::RaisingApron:
		return GetHasBarsInSystem() ? EUnloadTrolleyPhase::MovingToBin : EUnloadTrolleyPhase::ReturningToPark;
	case EBucketElevatorState::Idle:
		return GetHasBarsInSystem() ? EUnloadTrolleyPhase::MovingToBin : EUnloadTrolleyPhase::Idle;
	}
	return EUnloadTrolleyPhase::Idle;
}
