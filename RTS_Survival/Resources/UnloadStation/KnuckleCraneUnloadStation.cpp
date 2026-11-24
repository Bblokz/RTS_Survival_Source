// Copyright (C) Bas Blokzijl - All rights reserved.

#include "KnuckleCraneUnloadStation.h"

#include "Components/StaticMeshComponent.h"

namespace KnuckleCranePrivate
{
	// Tip path round the slew pivot: angle from the hopper (+X) through the open +Y side to the bed (deg) and reach
	// (cm). Folded in halfway, so the boom visibly knuckles while it slews. Same as build_knuckle_crane.py.
	constexpr int32 PathPointCount = 7;
	constexpr float PathAngles[PathPointCount] = {0.f, 30.f, 60.f, 90.f, 120.f, 150.f, 180.f};
	constexpr float PathReaches[PathPointCount] = {391.f, 365.f, 300.f, 255.f, 300.f, 365.f, 396.f};

	// Rotation that points a part's +X along a direction in the slew plane (U along the boom, Z up).
	FRotator PlaneRotation(const FVector2D& From, const FVector2D& To)
	{
		return FRotator(FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X)), 0.f, 0.f);
	}

	FVector2D Rotate(const FVector2D& Point, const float Radians)
	{
		const float Cos = FMath::Cos(Radians), Sin = FMath::Sin(Radians);
		return FVector2D(Point.X * Cos - Point.Y * Sin, Point.X * Sin + Point.Y * Cos);
	}

	FVector ToTurret(const FVector2D& Plane)
	{
		return FVector(Plane.X, 0.f, Plane.Y);
	}
}

AKnuckleCraneUnloadStation::AKnuckleCraneUnloadStation()
{
	BuildRefineryDefaults();
}

void AKnuckleCraneUnloadStation::BuildRefineryDefaults()
{
	using namespace KnuckleCranePrivate;
	// The rail is the tip's path; open, from the bay (hopper) to the bed.
	RailControlPoints.Reset();
	for (int32 Index = 0; Index < PathPointCount; ++Index)
	{
		const float Radians = FMath::DegreesToRadians(PathAngles[Index]);
		RailControlPoints.Add(FVector(SlewPivot.X + PathReaches[Index] * FMath::Cos(Radians),
		                              SlewPivot.Y + PathReaches[Index] * FMath::Sin(Radians), 0.f));
	}
	bRailIsClosedLoop = false;
	RailHeight = 500.f;

	// One bay on the old drop-off point in front of the truck nose; the harvester faces away from the refinery.
	FUnloadBaySettings Bay;
	Bay.PickPoint = FVector(611.f, 13.f, 0.f);
	Bay.DockYaw = 0.f;
	Bay.RunDirection = EUnloadTrolleyRunDirection::AlongRail;
	// Halfway along the path: the idle boom waits folded up over the open side.
	Bay.ParkOffset = 536.f;
	BaySettings = {Bay};

	FUnloadBinSettings Radixite;
	Radixite.Resource = ERTSResourceType::Resource_Radixite;
	Radixite.SocketName = TEXT("Radixite");
	Radixite.FallbackLocation = FVector(-176.f, 13.f, 137.f);
	BinSettings = {Radixite};

	// Harvesters that find the bay taken unload the old way off to the side the boom never swings over. Their path
	// ends up to ~4.6 m short of the point (acceptance + agent radius), so it sits 11 m from the docked hull.
	bUseOverflowDropOffPoint = true;
	OverflowDropOffPoint = FVector(1000.f, -1050.f, 0.f);

	// The 10.7 m tip path at 260 cm/s is a ~4.5 s slew each way; one unload takes about as long as at the HQ.
	TrolleySpeed = 260.f;
	TrolleyBrakeDistance = 200.f;
	HeadTravelBelowRail = 70.f;
	RopeTopBelowRail = 16.f;
	DropHeadAboveBin = 150.f;
}

void AKnuckleCraneUnloadStation::OnTrolleyCreated(FUnloadTrolley& Trolley)
{
	const FString Prefix = FString::Printf(TEXT("Arm%d_"), Trolley.BayIndex);
	FKnuckleCraneArm Arm;
	UStaticMeshComponent* Turret = CreatePart(TurretMesh, StationRoot, FName(Prefix + TEXT("Turret")));
	Arm.Turret = Turret;
	Arm.Boom1 = CreatePart(Boom1Mesh, Turret, FName(Prefix + TEXT("Boom1")));
	Arm.Boom2 = CreatePart(Boom2Mesh, Turret, FName(Prefix + TEXT("Boom2")));
	Arm.Ram1Cylinder = CreatePart(Ram1CylinderMesh, Turret, FName(Prefix + TEXT("Ram1Cyl")));
	Arm.Ram1Rod = CreatePart(Ram1RodMesh, Turret, FName(Prefix + TEXT("Ram1Rod")));
	Arm.Ram2Cylinder = CreatePart(Ram2CylinderMesh, Turret, FName(Prefix + TEXT("Ram2Cyl")));
	Arm.Ram2Rod = CreatePart(Ram2RodMesh, Turret, FName(Prefix + TEXT("Ram2Rod")));
	if (M_Arms.Num() <= Trolley.BayIndex)
	{
		M_Arms.SetNum(Trolley.BayIndex + 1);
	}
	M_Arms[Trolley.BayIndex] = Arm;
}

void AKnuckleCraneUnloadStation::UpdateTrolleyVisuals(const FUnloadTrolley& Trolley) const
{
	Super::UpdateTrolleyVisuals(Trolley);
	UStaticMeshComponent* Body = Trolley.Body.Get();
	if (not IsValid(Body) || not M_Arms.IsValidIndex(Trolley.BayIndex))
	{
		return;
	}
	const FVector TipWorld = Body->GetComponentLocation();
	PoseArm(M_Arms[Trolley.BayIndex], TipWorld);
	// The rope pair hangs in the boom's plane, not along the path.
	if (const UStaticMeshComponent* Turret = M_Arms[Trolley.BayIndex].Turret.Get())
	{
		Body->SetWorldRotation(FRotator(0.f, Turret->GetComponentRotation().Yaw, 0.f));
	}
}

void AKnuckleCraneUnloadStation::PoseArm(const FKnuckleCraneArm& Arm, const FVector& TipWorld) const
{
	using namespace KnuckleCranePrivate;
	UStaticMeshComponent* Turret = Arm.Turret.Get();
	if (not IsValid(Turret))
	{
		return;
	}
	const FVector TipLocal = GetActorTransform().InverseTransformPositionNoScale(TipWorld);
	const FVector2D Offset(TipLocal.X - SlewPivot.X, TipLocal.Y - SlewPivot.Y);
	const float SlewYaw = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
	Turret->SetRelativeLocationAndRotation(SlewPivot, FRotator(0.f, SlewYaw, 0.f));

	// Two-link solve in the slew plane (U = reach from the axis, Z = height above the turret origin), knuckle up.
	const FVector2D Shoulder(0.f, ShoulderHeight);
	const FVector2D Tip(Offset.Size(), TipLocal.Z - SlewPivot.Z);
	const FVector2D ToTip = Tip - Shoulder;
	const float MinReach = FMath::Abs(Boom1Length - Boom2Length) + 1.f;
	const float MaxReach = Boom1Length + Boom2Length - 1.f;
	const float Reach = FMath::Clamp(ToTip.Size(), MinReach, MaxReach);
	const float CosInner = (Boom1Length * Boom1Length + Reach * Reach - Boom2Length * Boom2Length)
		/ (2.f * Boom1Length * Reach);
	const float Boom1Radians = FMath::Atan2(ToTip.Y, ToTip.X) + FMath::Acos(FMath::Clamp(CosInner, -1.f, 1.f));
	const FVector2D Knuckle = Shoulder + Boom1Length * FVector2D(FMath::Cos(Boom1Radians), FMath::Sin(Boom1Radians));
	const float Boom2Radians = FMath::Atan2(Tip.Y - Knuckle.Y, Tip.X - Knuckle.X);

	if (UStaticMeshComponent* Boom1 = Arm.Boom1.Get())
	{
		Boom1->SetRelativeLocationAndRotation(ToTurret(Shoulder),
		                                      FRotator(FMath::RadiansToDegrees(Boom1Radians), 0.f, 0.f));
	}
	if (UStaticMeshComponent* Boom2 = Arm.Boom2.Get())
	{
		Boom2->SetRelativeLocationAndRotation(ToTurret(Knuckle),
		                                      FRotator(FMath::RadiansToDegrees(Boom2Radians), 0.f, 0.f));
	}

	// Rams: each cylinder sits on its eye and points at the rod's eye; the rod points back, so they telescope.
	const FVector2D Ram1A = Ram1TurretPoint;
	const FVector2D Ram1B = Shoulder + Rotate(Ram1Boom1Point, Boom1Radians);
	const FVector2D Ram2A = Shoulder + Rotate(Ram2Boom1Point, Boom1Radians);
	const FVector2D Ram2B = Knuckle + Rotate(Ram2Boom2Point, Boom2Radians);
	auto PlaceRam = [](UStaticMeshComponent* Cylinder, UStaticMeshComponent* Rod, const FVector2D& A,
	                   const FVector2D& B)
	{
		if (IsValid(Cylinder))
		{
			Cylinder->SetRelativeLocationAndRotation(ToTurret(A), PlaneRotation(A, B));
		}
		if (IsValid(Rod))
		{
			Rod->SetRelativeLocationAndRotation(ToTurret(B), PlaneRotation(B, A));
		}
	};
	PlaceRam(Arm.Ram1Cylinder.Get(), Arm.Ram1Rod.Get(), Ram1A, Ram1B);
	PlaceRam(Arm.Ram2Cylinder.Get(), Arm.Ram2Rod.Get(), Ram2A, Ram2B);
}
