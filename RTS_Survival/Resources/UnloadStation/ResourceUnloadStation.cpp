// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ResourceUnloadStation.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnloadStation, Log, All);

namespace UnloadStationPrivate
{
	// A rail pass counts as "over a bin" when it comes this close to the bin socket (cm, horizontal).
	constexpr float BinPassTolerance = 260.f;
	constexpr float BinSearchStep = 8.f;
	constexpr float ArrivalTolerance = 1.f;
	constexpr float Gravity = 980.f;
	// Falling items stop this far above the bin socket, inside the fill.
	constexpr float ItemStopAboveBin = 45.f;
	constexpr float MinTrolleySpeedFraction = 0.2f;
}

AResourceUnloadStation::AResourceUnloadStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);

	RailSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RailSpline"));
	RailSpline->SetupAttachment(StationRoot);

	RailMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RailMesh"));
	RailMeshComponent->SetupAttachment(StationRoot);
	RailMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RailMeshComponent->SetCanEverAffectNavigation(false);

	BuildDefaults();
}

void AResourceUnloadStation::BuildDefaults()
{
	// The Strike Division HQ's loop (concept B, _AssetSource/HQUnloadCrane): outer leg over the bins, round both bays,
	// back across the hood, inner leg over the bins, hairpin above the radixite bin.
	RailControlPoints = {
		FVector(-400.f, -335.f, 0.f), FVector(-43.f, -335.f, 0.f), FVector(300.f, -335.f, 0.f),
		FVector(520.f, -275.f, 0.f), FVector(731.f, -205.f, 0.f), FVector(880.f, -130.f, 0.f),
		FVector(935.f, 30.f, 0.f), FVector(880.f, 200.f, 0.f), FVector(731.f, 262.f, 0.f),
		FVector(560.f, 250.f, 0.f), FVector(445.f, 150.f, 0.f), FVector(385.f, 0.f, 0.f),
		FVector(305.f, -185.f, 0.f), FVector(-43.f, -195.f, 0.f), FVector(-400.f, -195.f, 0.f),
		FVector(-470.f, -265.f, 0.f)
	};

	FUnloadBaySettings LeftBay;
	LeftBay.PickPoint = FVector(731.f, -205.f, 0.f);
	LeftBay.RunDirection = EUnloadTrolleyRunDirection::AgainstRail;
	FUnloadBaySettings RightBay;
	RightBay.PickPoint = FVector(731.f, 262.f, 0.f);
	RightBay.RunDirection = EUnloadTrolleyRunDirection::AlongRail;
	BaySettings = {LeftBay, RightBay};

	FUnloadBinSettings Radixite;
	Radixite.Resource = ERTSResourceType::Resource_Radixite;
	Radixite.SocketName = TEXT("Radixite");
	Radixite.FallbackLocation = FVector(-322.f, -232.f, 25.f);
	FUnloadBinSettings Metal;
	Metal.Resource = ERTSResourceType::Resource_Metal;
	Metal.SocketName = TEXT("Metal");
	Metal.FallbackLocation = FVector(-43.f, -232.f, 25.f);
	BinSettings = {Radixite, Metal};

	// Pz I crystal (34 cm) and Pz I ingot (107 cm) meshes, scaled to a grab-sized load.
	const FVector CrystalScale(0.8f);
	const FVector IngotScale(0.5f);
	CrystalSlots = {
		FTransform(FRotator(15.f, 0.f, 10.f), FVector(-9.f, 0.f, -42.f), CrystalScale),
		FTransform(FRotator(-10.f, 70.f, 0.f), FVector(2.f, 5.f, -38.f), CrystalScale),
		FTransform(FRotator(5.f, 140.f, -15.f), FVector(10.f, -4.f, -44.f), CrystalScale)
	};
	BarSlots = {
		FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -44.f), IngotScale),
		FTransform(FRotator(0.f, 8.f, 0.f), FVector(2.f, 1.f, -36.f), IngotScale)
	};
}

void AResourceUnloadStation::BeginPlay()
{
	Super::BeginPlay();
	if (IsValid(RailMesh))
	{
		RailMeshComponent->SetStaticMesh(RailMesh);
	}
	BuildRailSpline();
}

void AResourceUnloadStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The cargo still on docked harvesters was never credited; they take it elsewhere (not when the world closes).
	for (int32 BayIndex = 0; EndPlayReason == EEndPlayReason::Destroyed && BayIndex < M_Bays.Num(); ++BayIndex)
	{
		if (M_Bays[BayIndex].Harvester.IsValid())
		{
			EndBayUnload(BayIndex, EUnloadEndReason::StationGone);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AResourceUnloadStation::BuildRailSpline()
{
	using namespace UnloadStationPrivate;
	RailSpline->ClearSplinePoints(false);
	const int32 PointCount = RailControlPoints.Num();
	if (PointCount < 3)
	{
		RTSFunctionLibrary::ReportError("Unload station rail needs at least 3 control points: " + GetName());
		return;
	}
	// Catmull-Rom through the control points, the same formula as build_monorail.py's catmull().
	const int32 SpanCount = bRailIsClosedLoop ? PointCount : PointCount - 1;
	auto PointAt = [this, PointCount](const int32 Index)
	{
		const int32 Wrapped = bRailIsClosedLoop
			                      ? (Index % PointCount + PointCount) % PointCount
			                      : FMath::Clamp(Index, 0, PointCount - 1);
		return FVector2D(RailControlPoints[Wrapped].X, RailControlPoints[Wrapped].Y);
	};
	int32 SplineIndex = 0;
	for (int32 Span = 0; Span < SpanCount; ++Span)
	{
		const FVector2D P0 = PointAt(Span - 1), P1 = PointAt(Span), P2 = PointAt(Span + 1), P3 = PointAt(Span + 2);
		for (int32 Step = 0; Step < RailSubdivisions; ++Step)
		{
			const float T = static_cast<float>(Step) / RailSubdivisions;
			const float T2 = T * T, T3 = T2 * T;
			const FVector2D Point = 0.5f * (2.f * P1 + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2
				+ (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
			RailSpline->AddSplinePoint(FVector(Point.X, Point.Y, RailHeight), ESplineCoordinateSpace::Local, false);
			RailSpline->SetSplinePointType(SplineIndex++, ESplinePointType::Linear, false);
		}
	}
	if (not bRailIsClosedLoop)
	{
		const FVector2D Last = PointAt(PointCount - 1);
		RailSpline->AddSplinePoint(FVector(Last.X, Last.Y, RailHeight), ESplineCoordinateSpace::Local, false);
		RailSpline->SetSplinePointType(SplineIndex, ESplinePointType::Linear, false);
	}
	RailSpline->SetClosedLoop(bRailIsClosedLoop, false);
	RailSpline->UpdateSpline();
}

void AResourceUnloadStation::InitUnloadStation(UResourceDropOff* DropOff, UStaticMeshComponent* BuildingMesh)
{
	M_DropOff = DropOff;
	M_BuildingMesh = BuildingMesh;
	if (not GetIsValidDropOff())
	{
		return;
	}
	if (RailSpline->GetNumberOfSplinePoints() == 0)
	{
		// Initialised before our own BeginPlay ran (spawned while the world itself begins play).
		if (IsValid(RailMesh))
		{
			RailMeshComponent->SetStaticMesh(RailMesh);
		}
		BuildRailSpline();
	}
	SetupBays();
	M_Trolleys.Reset();
	for (int32 BayIndex = 0; BayIndex < M_Bays.Num(); ++BayIndex)
	{
		CreateTrolley(BayIndex);
	}
	bM_IsStationReady = M_Bays.Num() > 0;
	UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD ready station=%s bays=%d rail_cm=%.0f"), *GetName(), M_Bays.Num(),
	       RailSpline->GetSplineLength());
}

FVector AResourceUnloadStation::GetBinWorldLocation(const FUnloadBinSettings& Bin) const
{
	const UStaticMeshComponent* BuildingMesh = M_BuildingMesh.Get();
	if (IsValid(BuildingMesh) && Bin.SocketName != NAME_None && BuildingMesh->DoesSocketExist(Bin.SocketName))
	{
		return BuildingMesh->GetSocketLocation(Bin.SocketName);
	}
	return GetActorTransform().TransformPositionNoScale(Bin.FallbackLocation);
}

void AResourceUnloadStation::SetupBays()
{
	M_Bays.Reset();
	const float RailLength = RailSpline->GetSplineLength();
	for (const FUnloadBaySettings& Settings : BaySettings)
	{
		FUnloadBayState Bay;
		const FVector PickWorld = GetActorTransform().TransformPositionNoScale(Settings.PickPoint);
		const float Key = RailSpline->FindInputKeyClosestToWorldLocation(PickWorld);
		Bay.BayRailDistance = RailSpline->GetDistanceAlongSplineAtSplineInputKey(Key);
		for (const FUnloadBinSettings& Bin : BinSettings)
		{
			const FVector BinWorld = GetBinWorldLocation(Bin);
			const float Offset = FindBinOffsetForBay(Bay, Settings, BinWorld);
			if (Offset < 0.f)
			{
				RTSFunctionLibrary::ReportError(FString::Printf(
					TEXT("Unload station %s: the rail never passes over the %s bin for a bay."), *GetName(),
					*Global_GetResourceTypeAsString(Bin.Resource)));
				continue;
			}
			Bay.BinOffsets.Add(Bin.Resource, Offset);
			Bay.BinWorldLocations.Add(Bin.Resource, BinWorld);
		}
		UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD bay=%d rail_distance=%.0f of %.0f bins=%d"), M_Bays.Num(),
		       Bay.BayRailDistance, RailLength, Bay.BinOffsets.Num());
		M_Bays.Add(MoveTemp(Bay));
	}
}

float AResourceUnloadStation::FindBinOffsetForBay(const FUnloadBayState& Bay, const FUnloadBaySettings& Settings,
                                                  const FVector& BinWorld) const
{
	using namespace UnloadStationPrivate;
	// Walk from the bay in its run direction; the first pass over the bin (closest point of that pass) is the stop.
	const float RailLength = RailSpline->GetSplineLength();
	const float Sign = Settings.RunDirection == EUnloadTrolleyRunDirection::AlongRail ? 1.f : -1.f;
	float BestDistance = TNumericLimits<float>::Max();
	float BestOffset = -1.f;
	for (float Offset = 0.f; Offset < RailLength; Offset += BinSearchStep)
	{
		const float RailDistance = WrapRailDistance(Bay.BayRailDistance + Sign * Offset);
		const FVector Point = RailSpline->GetLocationAtDistanceAlongSpline(RailDistance, ESplineCoordinateSpace::World);
		const float Distance2D = FVector::Dist2D(Point, BinWorld);
		if (Distance2D < BestDistance && Distance2D < BinPassTolerance)
		{
			BestDistance = Distance2D;
			BestOffset = Offset;
			continue;
		}
		if (BestOffset >= 0.f && Distance2D > BestDistance)
		{
			// Moving away again: the first pass is over.
			return BestOffset;
		}
	}
	return BestOffset;
}

UStaticMeshComponent* AResourceUnloadStation::CreatePart(UStaticMesh* Mesh, USceneComponent* Parent, const FName Name)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
	Part->SetStaticMesh(Mesh);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	Part->SetupAttachment(Parent);
	Part->RegisterComponent();
	AddInstanceComponent(Part);
	return Part;
}

void AResourceUnloadStation::CreateTrolley(const int32 BayIndex)
{
	FUnloadTrolley Trolley;
	Trolley.BayIndex = BayIndex;
	Trolley.Offset = BaySettings[BayIndex].ParkOffset;
	Trolley.TargetOffset = Trolley.Offset;
	const FString Prefix = FString::Printf(TEXT("Trolley%d_"), BayIndex);
	UStaticMeshComponent* Body = CreatePart(TrolleyBodyMesh, StationRoot, FName(Prefix + TEXT("Body")));
	UStaticMeshComponent* Rope = CreatePart(RopeMesh, Body, FName(Prefix + TEXT("Rope")));
	UStaticMeshComponent* Head = CreatePart(GrabHeadMesh, Body, FName(Prefix + TEXT("Head")));
	UStaticMeshComponent* JawRight = CreatePart(JawMesh, Head, FName(Prefix + TEXT("JawR")));
	UStaticMeshComponent* JawLeft = CreatePart(JawMesh, Head, FName(Prefix + TEXT("JawL")));
	Body->SetUsingAbsoluteRotation(true);
	Rope->SetRelativeLocation(FVector(0.f, 0.f, -RopeTopBelowRail));
	JawRight->SetRelativeLocation(FVector(0.f, JawHingeSpread, -JawHingeBelowHead));
	JawLeft->SetRelativeLocation(FVector(0.f, -JawHingeSpread, -JawHingeBelowHead));
	Trolley.Body = Body;
	Trolley.Rope = Rope;
	Trolley.Head = Head;
	Trolley.JawLeft = JawLeft;
	Trolley.JawRight = JawRight;
	for (int32 Index = 0; Index < CrystalSlots.Num(); ++Index)
	{
		UStaticMeshComponent* Item = CreatePart(CrystalMesh, Head, FName(Prefix + FString::Printf(TEXT("Crystal%d"), Index)));
		Item->SetRelativeTransform(CrystalSlots[Index]);
		Item->SetVisibility(false);
		Trolley.Crystals.Add(Item);
	}
	for (int32 Index = 0; Index < BarSlots.Num(); ++Index)
	{
		UStaticMeshComponent* Item = CreatePart(BarMesh, Head, FName(Prefix + FString::Printf(TEXT("Bar%d"), Index)));
		Item->SetRelativeTransform(BarSlots[Index]);
		Item->SetVisibility(false);
		Trolley.Bars.Add(Item);
	}
	M_Trolleys.Add(MoveTemp(Trolley));
	OnTrolleyCreated(M_Trolleys.Last());
	UpdateTrolleyVisuals(M_Trolleys.Last());
}

float AResourceUnloadStation::WrapRailDistance(const float Distance) const
{
	const float RailLength = RailSpline->GetSplineLength();
	if (RailLength <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	if (not bRailIsClosedLoop)
	{
		return FMath::Clamp(Distance, 0.f, RailLength);
	}
	return FMath::Fmod(FMath::Fmod(Distance, RailLength) + RailLength, RailLength);
}

float AResourceUnloadStation::GetRailDistanceForOffset(const FUnloadTrolley& Trolley) const
{
	const FUnloadBaySettings& Settings = BaySettings[Trolley.BayIndex];
	const float Sign = Settings.RunDirection == EUnloadTrolleyRunDirection::AlongRail ? 1.f : -1.f;
	return WrapRailDistance(M_Bays[Trolley.BayIndex].BayRailDistance + Sign * Trolley.Offset);
}

float AResourceUnloadStation::GetTravelHeadZ() const
{
	return GetActorLocation().Z + RailHeight - HeadTravelBelowRail;
}

// ---------------------------------------------------------------------------------------------------------------- bays

bool AResourceUnloadStation::TryReserveBay(UHarvester& Harvester, int32& OutBayIndex)
{
	if (not bM_IsStationReady)
	{
		return false;
	}
	for (int32 BayIndex = 0; BayIndex < M_Bays.Num(); ++BayIndex)
	{
		if (M_Bays[BayIndex].Harvester.Get() == &Harvester)
		{
			OutBayIndex = BayIndex;
			return true;
		}
	}
	// The bay whose pick point is nearest, so harvesters from the left take the left bay.
	int32 BestBay = INDEX_NONE;
	float BestDistance = TNumericLimits<float>::Max();
	const FVector HarvesterLocation = IsValid(Harvester.GetOwner()) ? Harvester.GetOwner()->GetActorLocation() : FVector::ZeroVector;
	for (int32 BayIndex = 0; BayIndex < M_Bays.Num(); ++BayIndex)
	{
		if (M_Bays[BayIndex].Harvester.IsValid())
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(GetBayPickWorldLocation(BayIndex), HarvesterLocation);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestBay = BayIndex;
		}
	}
	if (BestBay == INDEX_NONE)
	{
		return false;
	}
	FUnloadBayState& Bay = M_Bays[BestBay];
	Bay.Harvester = &Harvester;
	Bay.bIsDocked = false;
	Bay.ChunkAmount = 0;
	OutBayIndex = BestBay;
	UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD reserved bay=%d harvester=%s"), BestBay,
	       *GetNameSafe(Harvester.GetOwner()));
	return true;
}

void AResourceUnloadStation::ReleaseBay(const int32 BayIndex, const UHarvester* Harvester)
{
	if (not M_Bays.IsValidIndex(BayIndex) || M_Bays[BayIndex].Harvester.Get() != Harvester)
	{
		return;
	}
	FUnloadBayState& Bay = M_Bays[BayIndex];
	Bay.Harvester.Reset();
	Bay.DropOff.Reset();
	Bay.bIsDocked = false;
	Bay.ChunkAmount = 0;
	UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD released bay=%d"), BayIndex);
}

bool AResourceUnloadStation::GetDockTransform(const int32 BayIndex, const UHarvester& Harvester,
                                              FTransform& OutDock) const
{
	const AActor* HarvesterActor = Harvester.GetOwner();
	if (not BaySettings.IsValidIndex(BayIndex) || not IsValid(HarvesterActor))
	{
		return false;
	}
	const FUnloadBaySettings& Settings = BaySettings[BayIndex];
	const FVector CargoOffset = HarvesterActor->GetActorTransform().InverseTransformPositionNoScale(
		Harvester.GetCargoWorldLocation());
	const FRotator DockRotationLocal(0.f, Settings.DockYaw, 0.f);
	const FVector DockLocal = Settings.PickPoint - DockRotationLocal.RotateVector(FVector(CargoOffset.X, CargoOffset.Y, 0.f));
	const FTransform StationTransform(GetActorRotation(), GetActorLocation());
	FVector DockWorld = StationTransform.TransformPosition(DockLocal);
	DockWorld.Z = HarvesterActor->GetActorLocation().Z;
	const FRotator DockRotationWorld(0.f, (StationTransform.GetRotation() * DockRotationLocal.Quaternion()).Rotator().Yaw,
	                                 0.f);
	OutDock = FTransform(DockRotationWorld, DockWorld);
	return true;
}

FVector AResourceUnloadStation::GetApproachLocation(const int32 BayIndex, const UHarvester& Harvester) const
{
	FTransform Dock;
	if (not GetDockTransform(BayIndex, Harvester, Dock))
	{
		return GetActorLocation();
	}
	return Dock.GetLocation() + Dock.GetRotation().GetForwardVector() * BaySettings[BayIndex].ApproachDistance;
}

void AResourceUnloadStation::OnHarvesterDocked(const int32 BayIndex, UHarvester& Harvester, UResourceDropOff& DropOff,
                                               const ERTSResourceType Resource, const int32 CargoAmount)
{
	if (not M_Bays.IsValidIndex(BayIndex) || M_Bays[BayIndex].Harvester.Get() != &Harvester)
	{
		Harvester.Crane_OnUnloadEnded(EUnloadEndReason::StationGone);
		return;
	}
	FUnloadBayState& Bay = M_Bays[BayIndex];
	if (not Bay.BinOffsets.Contains(Resource) || CargoAmount <= 0)
	{
		// No bin for this cargo: hand it back to the old drop-off path.
		EndBayUnload(BayIndex, EUnloadEndReason::StationGone);
		return;
	}
	Bay.bIsDocked = true;
	Bay.DropOff = &DropOff;
	Bay.Resource = Resource;
	Bay.ChunkAmount = FMath::DivideAndRoundUp(CargoAmount, FMath::Max(1, TripsPerUnload));
	UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD docked bay=%d resource=%s cargo=%d chunk=%d"), BayIndex,
	       *Global_GetResourceTypeAsString(Resource), CargoAmount, Bay.ChunkAmount);
}

void AResourceUnloadStation::EndBayUnload(const int32 BayIndex, const EUnloadEndReason Reason)
{
	if (not M_Bays.IsValidIndex(BayIndex))
	{
		return;
	}
	FUnloadBayState& Bay = M_Bays[BayIndex];
	UHarvester* Harvester = Bay.Harvester.Get();
	// Clear first: the harvester's reaction may release the bay or reserve it again.
	Bay.Harvester.Reset();
	Bay.DropOff.Reset();
	Bay.bIsDocked = false;
	Bay.ChunkAmount = 0;
	UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD ended bay=%d reason=%d"), BayIndex, static_cast<int32>(Reason));
	if (IsValid(Harvester))
	{
		Harvester->Crane_OnUnloadEnded(Reason);
	}
}

bool AResourceUnloadStation::GetIsValidBay(const int32 BayIndex) const
{
	return M_Bays.IsValidIndex(BayIndex) && M_Trolleys.IsValidIndex(BayIndex);
}

bool AResourceUnloadStation::GetIsValidDropOff() const
{
	if (M_DropOff.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_DropOff", "GetIsValidDropOff", this);
	return false;
}

bool AResourceUnloadStation::GetCanUnloadResource(const ERTSResourceType Resource) const
{
	for (const FUnloadBayState& Bay : M_Bays)
	{
		if (Bay.BinOffsets.Contains(Resource))
		{
			return true;
		}
	}
	return false;
}

bool AResourceUnloadStation::GetOverflowDropOffLocation(FVector& OutLocation) const
{
	if (not bUseOverflowDropOffPoint)
	{
		return false;
	}
	OutLocation = FTransform(GetActorRotation(), GetActorLocation()).TransformPosition(OverflowDropOffPoint);
	return true;
}

bool AResourceUnloadStation::GetIsBayDocked(const int32 BayIndex) const
{
	return M_Bays.IsValidIndex(BayIndex) && M_Bays[BayIndex].bIsDocked;
}

const UHarvester* AResourceUnloadStation::GetBayHarvester(const int32 BayIndex) const
{
	return M_Bays.IsValidIndex(BayIndex) ? M_Bays[BayIndex].Harvester.Get() : nullptr;
}

EUnloadTrolleyPhase AResourceUnloadStation::GetTrolleyPhase(const int32 BayIndex) const
{
	return M_Trolleys.IsValidIndex(BayIndex) ? M_Trolleys[BayIndex].Phase : EUnloadTrolleyPhase::Idle;
}

FVector AResourceUnloadStation::GetTrolleyWorldLocation(const int32 BayIndex) const
{
	if (not GetIsValidBay(BayIndex))
	{
		return FVector::ZeroVector;
	}
	return RailSpline->GetLocationAtDistanceAlongSpline(GetRailDistanceForOffset(M_Trolleys[BayIndex]),
	                                                    ESplineCoordinateSpace::World);
}

FVector AResourceUnloadStation::GetGrabWorldLocation(const int32 BayIndex) const
{
	if (not M_Trolleys.IsValidIndex(BayIndex) || not M_Trolleys[BayIndex].Head.IsValid())
	{
		return FVector::ZeroVector;
	}
	return M_Trolleys[BayIndex].Head->GetComponentLocation();
}

FVector AResourceUnloadStation::GetBayPickWorldLocation(const int32 BayIndex) const
{
	if (not BaySettings.IsValidIndex(BayIndex))
	{
		return GetActorLocation();
	}
	return FTransform(GetActorRotation(), GetActorLocation()).TransformPosition(BaySettings[BayIndex].PickPoint);
}

// ------------------------------------------------------------------------------------------------------------ trolleys

void AResourceUnloadStation::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (not bM_IsStationReady)
	{
		return;
	}
	for (FUnloadTrolley& Trolley : M_Trolleys)
	{
		TickTrolley(Trolley, DeltaSeconds);
		UpdateTrolleyVisuals(Trolley);
	}
	TickFallingItems(DeltaSeconds);
}

void AResourceUnloadStation::EnterPhase(FUnloadTrolley& Trolley, const EUnloadTrolleyPhase NewPhase)
{
	Trolley.Phase = NewPhase;
	Trolley.PhaseSeconds = 0.f;
}

void AResourceUnloadStation::TickTrolley(FUnloadTrolley& Trolley, const float DeltaSeconds)
{
	Trolley.PhaseSeconds += DeltaSeconds;
	const FUnloadBayState& Bay = M_Bays[Trolley.BayIndex];
	const bool bIsPickPhase = Trolley.Phase == EUnloadTrolleyPhase::LoweringToPick
		|| Trolley.Phase == EUnloadTrolleyPhase::Grabbing;
	if (not Bay.bIsDocked && (bIsPickPhase || Trolley.bIsCarryingLoad))
	{
		// The harvester left (new order, died, station gone): nothing was credited for this load, so it vanishes.
		ShowLoad(Trolley, Trolley.LoadResource, false);
		Trolley.bIsCarryingLoad = false;
		Trolley.JawAlpha = 0.f;
		Trolley.TargetHookDrop = 0.f;
		Trolley.TargetOffset = BaySettings[Trolley.BayIndex].ParkOffset;
		EnterPhase(Trolley, EUnloadTrolleyPhase::ReturningToPark);
	}
	if (Trolley.Phase == EUnloadTrolleyPhase::Idle)
	{
		TickTrolleyIdle(Trolley);
		return;
	}
	if (Trolley.Phase == EUnloadTrolleyPhase::Grabbing)
	{
		Trolley.JawAlpha = FMath::Min(1.f, Trolley.JawAlpha + DeltaSeconds / GrabSeconds);
		if (Trolley.JawAlpha >= 0.5f && not Trolley.bIsCarryingLoad)
		{
			Trolley.bIsCarryingLoad = true;
			Trolley.LoadResource = Bay.Resource;
			ShowLoad(Trolley, Bay.Resource, true);
		}
	}
	else if (Trolley.Phase == EUnloadTrolleyPhase::Releasing)
	{
		Trolley.JawAlpha = FMath::Max(0.f, Trolley.JawAlpha - DeltaSeconds / ReleaseSeconds);
	}
	const bool bOffsetArrived = MoveOffsetTowardsTarget(Trolley, DeltaSeconds);
	const bool bHookArrived = MoveHookTowardsTarget(Trolley, DeltaSeconds);
	const bool bJawsDone = (Trolley.Phase != EUnloadTrolleyPhase::Grabbing || Trolley.JawAlpha >= 1.f)
		&& (Trolley.Phase != EUnloadTrolleyPhase::Releasing || Trolley.JawAlpha <= 0.f);
	if (bOffsetArrived && bHookArrived && bJawsDone)
	{
		TickTrolleyArrivals(Trolley);
	}
}

void AResourceUnloadStation::TickTrolleyIdle(FUnloadTrolley& Trolley)
{
	const FUnloadBayState& Bay = M_Bays[Trolley.BayIndex];
	if (Bay.bIsDocked)
	{
		Trolley.TargetOffset = 0.f;
		Trolley.TargetHookDrop = 0.f;
		EnterPhase(Trolley, EUnloadTrolleyPhase::MovingToBay);
	}
}

void AResourceUnloadStation::TickTrolleyArrivals(FUnloadTrolley& Trolley)
{
	switch (Trolley.Phase)
	{
	case EUnloadTrolleyPhase::MovingToBay:
		StartPickAtBay(Trolley);
		return;
	case EUnloadTrolleyPhase::LoweringToPick:
		EnterPhase(Trolley, EUnloadTrolleyPhase::Grabbing);
		return;
	case EUnloadTrolleyPhase::Grabbing:
		Trolley.TargetHookDrop = 0.f;
		EnterPhase(Trolley, EUnloadTrolleyPhase::HoistingFromPick);
		return;
	case EUnloadTrolleyPhase::HoistingFromPick:
		StartRunToBin(Trolley);
		return;
	case EUnloadTrolleyPhase::MovingToBin:
		{
			const FVector* BinWorld = M_Bays[Trolley.BayIndex].BinWorldLocations.Find(Trolley.LoadResource);
			const float BinZ = BinWorld ? BinWorld->Z : GetActorLocation().Z;
			Trolley.TargetHookDrop = FMath::Max(0.f, GetTravelHeadZ() - (BinZ + DropHeadAboveBin));
			EnterPhase(Trolley, EUnloadTrolleyPhase::LoweringToDrop);
			return;
		}
	case EUnloadTrolleyPhase::LoweringToDrop:
		EnterPhase(Trolley, EUnloadTrolleyPhase::Releasing);
		ReleaseLoadIntoBin(Trolley);
		return;
	case EUnloadTrolleyPhase::Releasing:
		Trolley.TargetHookDrop = 0.f;
		EnterPhase(Trolley, EUnloadTrolleyPhase::HoistingFromDrop);
		return;
	case EUnloadTrolleyPhase::HoistingFromDrop:
		AfterDropHoisted(Trolley);
		return;
	case EUnloadTrolleyPhase::ReturningToPark:
		EnterPhase(Trolley, EUnloadTrolleyPhase::Idle);
		return;
	case EUnloadTrolleyPhase::Idle:
		return;
	}
}

void AResourceUnloadStation::StartPickAtBay(FUnloadTrolley& Trolley)
{
	const FUnloadBayState& Bay = M_Bays[Trolley.BayIndex];
	const UHarvester* Harvester = Bay.Harvester.Get();
	if (not Bay.bIsDocked || not IsValid(Harvester))
	{
		Trolley.TargetOffset = BaySettings[Trolley.BayIndex].ParkOffset;
		EnterPhase(Trolley, EUnloadTrolleyPhase::ReturningToPark);
		return;
	}
	const float CargoZ = Harvester->GetCargoWorldLocation().Z;
	Trolley.TargetHookDrop = FMath::Max(0.f, GetTravelHeadZ() - (CargoZ + PickHeadAboveCargo));
	EnterPhase(Trolley, EUnloadTrolleyPhase::LoweringToPick);
}

void AResourceUnloadStation::StartRunToBin(FUnloadTrolley& Trolley)
{
	const float* BinOffset = M_Bays[Trolley.BayIndex].BinOffsets.Find(Trolley.LoadResource);
	if (BinOffset == nullptr)
	{
		ShowLoad(Trolley, Trolley.LoadResource, false);
		Trolley.bIsCarryingLoad = false;
		Trolley.TargetOffset = BaySettings[Trolley.BayIndex].ParkOffset;
		EnterPhase(Trolley, EUnloadTrolleyPhase::ReturningToPark);
		return;
	}
	Trolley.TargetOffset = *BinOffset;
	EnterPhase(Trolley, EUnloadTrolleyPhase::MovingToBin);
}

void AResourceUnloadStation::ReleaseLoadIntoBin(FUnloadTrolley& Trolley)
{
	FUnloadBayState& Bay = M_Bays[Trolley.BayIndex];
	DropLoadItems(Trolley);
	Trolley.bIsCarryingLoad = false;
	UHarvester* Harvester = Bay.Harvester.Get();
	if (not Bay.bIsDocked || not IsValid(Harvester))
	{
		return;
	}
	const FVector* BinWorld = Bay.BinWorldLocations.Find(Trolley.LoadResource);
	const FVector TextLocation = (BinWorld ? *BinWorld : GetActorLocation()) + FVector(0.f, 0.f, BinTextHeight);
	bool bStorageFull = false;
	const int32 Delivered = Harvester->Crane_DeliverChunk(Bay.ChunkAmount, TextLocation, bStorageFull);
	const int32 CargoLeft = Harvester->GetCraneCargoAmount();
	UE_LOG(LogRTSUnloadStation, Log, TEXT("RTS_UNLOAD delivered bay=%d amount=%d left=%d full=%d"), Trolley.BayIndex,
	       Delivered, CargoLeft, bStorageFull ? 1 : 0);
	if (bStorageFull)
	{
		EndBayUnload(Trolley.BayIndex, EUnloadEndReason::StorageFull);
		return;
	}
	if (CargoLeft <= 0)
	{
		// The harvester can drive off while the empty grab goes back up.
		EndBayUnload(Trolley.BayIndex, EUnloadEndReason::CargoEmpty);
	}
}

void AResourceUnloadStation::AfterDropHoisted(FUnloadTrolley& Trolley)
{
	const FUnloadBayState& Bay = M_Bays[Trolley.BayIndex];
	if (Bay.bIsDocked && Bay.Harvester.IsValid())
	{
		Trolley.TargetOffset = 0.f;
		EnterPhase(Trolley, EUnloadTrolleyPhase::MovingToBay);
		return;
	}
	Trolley.TargetOffset = BaySettings[Trolley.BayIndex].ParkOffset;
	EnterPhase(Trolley, EUnloadTrolleyPhase::ReturningToPark);
}

bool AResourceUnloadStation::MoveOffsetTowardsTarget(FUnloadTrolley& Trolley, const float DeltaSeconds) const
{
	using namespace UnloadStationPrivate;
	const float Remaining = Trolley.TargetOffset - Trolley.Offset;
	if (FMath::Abs(Remaining) <= ArrivalTolerance)
	{
		Trolley.Offset = Trolley.TargetOffset;
		return true;
	}
	// Ease out over the brake distance, and ease in from the last stop the same way.
	const float Fraction = FMath::Clamp(FMath::Abs(Remaining) / TrolleyBrakeDistance, MinTrolleySpeedFraction, 1.f);
	const float Step = FMath::Min(FMath::Abs(Remaining), TrolleySpeed * Fraction * DeltaSeconds);
	Trolley.Offset += FMath::Sign(Remaining) * Step;
	return false;
}

bool AResourceUnloadStation::MoveHookTowardsTarget(FUnloadTrolley& Trolley, const float DeltaSeconds) const
{
	using namespace UnloadStationPrivate;
	const float Remaining = Trolley.TargetHookDrop - Trolley.HookDrop;
	if (FMath::Abs(Remaining) <= ArrivalTolerance)
	{
		Trolley.HookDrop = Trolley.TargetHookDrop;
		return true;
	}
	const float Fraction = FMath::Clamp(FMath::Abs(Remaining) / 60.f, MinTrolleySpeedFraction, 1.f);
	const float Step = FMath::Min(FMath::Abs(Remaining), HoistSpeed * Fraction * DeltaSeconds);
	Trolley.HookDrop += FMath::Sign(Remaining) * Step;
	return false;
}

void AResourceUnloadStation::ShowLoad(FUnloadTrolley& Trolley, const ERTSResourceType Resource, const bool bShow) const
{
	TArray<TWeakObjectPtr<UStaticMeshComponent>>& Items = Resource == ERTSResourceType::Resource_Metal
		                                                      ? Trolley.Bars
		                                                      : Trolley.Crystals;
	const TArray<FTransform>& Slots = Resource == ERTSResourceType::Resource_Metal ? BarSlots : CrystalSlots;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		UStaticMeshComponent* Item = Items[Index].Get();
		if (not IsValid(Item))
		{
			continue;
		}
		if (bShow && Item->GetAttachParent() != Trolley.Head.Get())
		{
			Item->AttachToComponent(Trolley.Head.Get(), FAttachmentTransformRules::KeepRelativeTransform);
		}
		if (bShow && Slots.IsValidIndex(Index))
		{
			Item->SetRelativeTransform(Slots[Index]);
		}
		Item->SetVisibility(bShow);
	}
}

void AResourceUnloadStation::DropLoadItems(FUnloadTrolley& Trolley)
{
	using namespace UnloadStationPrivate;
	TArray<TWeakObjectPtr<UStaticMeshComponent>>& Items = Trolley.LoadResource == ERTSResourceType::Resource_Metal
		                                                      ? Trolley.Bars
		                                                      : Trolley.Crystals;
	const FVector* BinWorld = M_Bays[Trolley.BayIndex].BinWorldLocations.Find(Trolley.LoadResource);
	const float StopZ = (BinWorld ? BinWorld->Z : GetActorLocation().Z) + ItemStopAboveBin;
	for (const TWeakObjectPtr<UStaticMeshComponent>& WeakItem : Items)
	{
		UStaticMeshComponent* Item = WeakItem.Get();
		if (not IsValid(Item) || not Item->IsVisible())
		{
			continue;
		}
		Item->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		FUnloadFallingItem Falling;
		Falling.Component = Item;
		Falling.HomeParent = Trolley.Head;
		Falling.StopZ = StopZ;
		M_FallingItems.Add(Falling);
	}
}

void AResourceUnloadStation::TickFallingItems(const float DeltaSeconds)
{
	using namespace UnloadStationPrivate;
	for (int32 Index = M_FallingItems.Num() - 1; Index >= 0; --Index)
	{
		FUnloadFallingItem& Falling = M_FallingItems[Index];
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
		// Landed in the fill: hide it and hang it back in the grab for the next load.
		Item->SetVisibility(false);
		if (USceneComponent* HomeParent = Falling.HomeParent.Get())
		{
			Item->AttachToComponent(HomeParent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		M_FallingItems.RemoveAtSwap(Index);
	}
}

void AResourceUnloadStation::UpdateTrolleyVisuals(const FUnloadTrolley& Trolley) const
{
	UStaticMeshComponent* Body = Trolley.Body.Get();
	if (not IsValid(Body))
	{
		return;
	}
	const float RailDistance = GetRailDistanceForOffset(Trolley);
	const FVector Location = RailSpline->GetLocationAtDistanceAlongSpline(RailDistance, ESplineCoordinateSpace::World);
	const FVector Direction = RailSpline->GetDirectionAtDistanceAlongSpline(RailDistance, ESplineCoordinateSpace::World);
	Body->SetWorldLocationAndRotation(Location, FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	if (UStaticMeshComponent* Rope = Trolley.Rope.Get())
	{
		const float RopeLength = FMath::Max(1.f, HeadTravelBelowRail - RopeTopBelowRail + Trolley.HookDrop);
		Rope->SetRelativeScale3D(FVector(1.f, 1.f, RopeLength / RopeMeshLength));
	}
	if (UStaticMeshComponent* Head = Trolley.Head.Get())
	{
		Head->SetRelativeLocation(FVector(0.f, 0.f, -(HeadTravelBelowRail + Trolley.HookDrop)));
	}
	const float OpenRadians = FMath::DegreesToRadians(JawOpenAngle * (1.f - Trolley.JawAlpha));
	if (UStaticMeshComponent* JawRight = Trolley.JawRight.Get())
	{
		JawRight->SetRelativeRotation(FQuat(FVector::XAxisVector, OpenRadians));
	}
	if (UStaticMeshComponent* JawLeft = Trolley.JawLeft.Get())
	{
		JawLeft->SetRelativeRotation(FQuat(FVector::ZAxisVector, PI) * FQuat(FVector::XAxisVector, OpenRadians));
	}
}
