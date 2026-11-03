// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMenuBattleCamera.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RTSMenuBattleArena.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSMenuBattleCameraPrivate
{
	constexpr float MinShotSeconds = 5.f;
	constexpr float MaxShotSeconds = 8.5f;
	constexpr float FocusEaseSpeed = 1.2f;
	constexpr float ChaseEaseSpeed = 2.5f;
	constexpr float GroundTraceHalfHeightCm = 20000.f;
	constexpr float MinGroundClearanceCm = 250.f;

	// Below this alignment of an axis with the inward direction either side of the arena is as good.
	constexpr float MinInwardAlignment = 0.25f;

	// Framing distance grows with the spread of the fighting so a battle that has grown still fits the frame.
	constexpr float MinDistanceScale = 0.75f;
	// The lines are wide: past this the camera frames part of the fight instead of shrinking every unit to a dot.
	constexpr float MaxFramingDistanceCm = 13000.f;
	constexpr float SpreadToDistance = 1.35f;
	constexpr float SpreadEaseSpeed = 0.6f;

	constexpr float OrbitDegreesPerSecond = 4.5f;
	constexpr float OrbitStartSpreadDegrees = 45.f;
	constexpr float OrbitFieldOfView = 55.f;

	constexpr float FlankDistanceScale = 0.85f;
	constexpr float FlankStartBackScale = 0.35f;
	constexpr float FlankHeightScale = 0.6f;
	constexpr float FlankDollyCmPerSecond = 220.f;
	constexpr float FlankFieldOfView = 50.f;

	constexpr float BehindLineBackCm = 2400.f;
	constexpr float BehindLineSideCm = 800.f;
	constexpr float BehindLineHeightCm = 750.f;
	constexpr float BehindLinePushCmPerSecond = 160.f;
	constexpr float BehindLineFieldOfView = 60.f;

	constexpr float ChaseBackCm = 1900.f;
	constexpr float ChaseSideCm = 750.f;
	constexpr float ChaseHeightCm = 850.f;
	constexpr float ChaseLookAheadCm = 900.f;
	constexpr float ChaseFieldOfView = 65.f;

	constexpr float ColumnSideCm = 2800.f;
	constexpr float ColumnBackCm = 700.f;
	constexpr float ColumnHeightCm = 700.f;
	constexpr float ColumnLookAheadCm = 1300.f;
	constexpr float ColumnFieldOfView = 55.f;

	constexpr float OverviewBackScale = 0.9f;
	constexpr float OverviewSideScale = 0.55f;
	constexpr float OverviewHeightScale = 2.4f;
	constexpr float OverviewDriftCmPerSecond = 140.f;
	constexpr float OverviewFieldOfView = 55.f;

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}
}

ARTSMenuBattleCamera::ARTSMenuBattleCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// The menu UI may pause the world; the camera keeps filming the frozen battle smoothly.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	if (UCameraComponent* Camera = GetCameraComponent())
	{
		Camera->bConstrainAspectRatio = false;
		Camera->SetFieldOfView(RTSMenuBattleCameraPrivate::OverviewFieldOfView);
	}
}

void ARTSMenuBattleCamera::BeginCoverage(ARTSMenuBattleArena& Arena, const int32 Seed, const FVector& MapCentre)
{
	M_Arena = &Arena;
	M_InwardDirection = (MapCentre - Arena.GetActorLocation()).GetSafeNormal2D();
	M_ShotStream.Initialize(Seed);
	M_GermanSubjects.Reset();
	M_SovietSubjects.Reset();
	M_SmoothedFocus = Arena.GetActorLocation();
	bM_IsCovering = true;
	// Opening shot: alongside a column driving up to its line.
	StartShot(ERTSMenuBattleShot::ColumnTrack);
	UpdateShot(0.f);
}

void ARTSMenuBattleCamera::SetSubjects(const TArray<TWeakObjectPtr<AActor>>& GermanUnits,
                                       const TArray<TWeakObjectPtr<AActor>>& SovietUnits)
{
	const bool bHadSubjects = GetHasAliveSubject();
	M_GermanSubjects = GermanUnits;
	M_SovietSubjects = SovietUnits;
	if (bHadSubjects)
	{
		return;
	}
	// The first units arrived: centre on them at once instead of easing in from the arena origin.
	FVector Centre;
	if (GetSubjectsCentre(nullptr, Centre))
	{
		M_SmoothedFocus = Centre;
		M_ShotState.bIsCut = true;
	}
}

void ARTSMenuBattleCamera::StopCoverage()
{
	bM_IsCovering = false;
	M_GermanSubjects.Reset();
	M_SovietSubjects.Reset();
}

void ARTSMenuBattleCamera::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (not bM_IsCovering || not GetIsValidArena())
	{
		return;
	}
	const float ShotSeconds = GetTimeSeconds() - M_ShotState.StartTimeSeconds;
	const bool bChaseLost = M_ShotState.Shot == ERTSMenuBattleShot::ChaseUnit && not M_ShotState.ChaseTarget.IsValid();
	if (ShotSeconds >= M_ShotState.DurationSeconds || bChaseLost)
	{
		StartShot(PickNextShot());
	}
	UpdateShot(DeltaSeconds);
}

bool ARTSMenuBattleCamera::GetIsValidArena() const
{
	if (M_Arena.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Arena", "GetIsValidArena", this);
	return false;
}

float ARTSMenuBattleCamera::GetTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
}

void ARTSMenuBattleCamera::StartShot(const ERTSMenuBattleShot Shot)
{
	using namespace RTSMenuBattleCameraPrivate;
	M_ShotState = FRTSMenuBattleShotState();
	M_ShotState.Shot = Shot;
	M_ShotState.StartTimeSeconds = GetTimeSeconds();
	M_ShotState.DurationSeconds = M_ShotStream.FRandRange(MinShotSeconds, MaxShotSeconds);
	const FVector Forward = M_Arena.IsValid() ? M_Arena->GetBattleForward() : FVector::ForwardVector;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	// The camera stands on the arena's outer side and looks toward the map, never at the landscape's edge.
	const bool bFreeSide = Shot == ERTSMenuBattleShot::Orbit || Shot == ERTSMenuBattleShot::ChaseUnit;
	M_ShotState.SideSign = bFreeSide
		                       ? (M_ShotStream.FRand() < 0.5f ? -1.f : 1.f)
		                       : PickOutwardSign(Right);
	const float OutwardYaw = (-M_InwardDirection).Rotation().Yaw;
	M_ShotState.OrbitStartYaw = M_InwardDirection.IsNearlyZero()
		                            ? M_ShotStream.FRandRange(0.f, 360.f)
		                            : OutwardYaw + M_ShotStream.FRandRange(-OrbitStartSpreadDegrees, OrbitStartSpreadDegrees);
	// German line looks along Forward: film from behind the line whose view runs inward.
	M_ShotState.LineSide = PickOutwardSign(Forward) < 0.f ? ERTSMenuBattleSide::German : ERTSMenuBattleSide::Soviet;
	M_ShotState.bIsCut = true;
	if (Shot == ERTSMenuBattleShot::ColumnTrack)
	{
		// Either column; the camera still keeps to the arena's outer flank (SideSign).
		M_ShotState.LineSide = M_ShotStream.FRand() < 0.5f ? ERTSMenuBattleSide::German : ERTSMenuBattleSide::Soviet;
		FVector Centre;
		const bool bGerman = M_ShotState.LineSide == ERTSMenuBattleSide::German;
		M_ShotState.ChaseAnchor = GetSubjectsCentre(bGerman ? &M_GermanSubjects : &M_SovietSubjects, Centre)
			                          ? Centre
			                          : M_Arena->GetLineCenter(M_ShotState.LineSide);
	}
	if (Shot == ERTSMenuBattleShot::ChaseUnit)
	{
		AActor* Target = PickChaseTarget();
		M_ShotState.ChaseTarget = Target;
		M_ShotState.ChaseAnchor = IsValid(Target) ? Target->GetActorLocation() : M_SmoothedFocus;
	}
}

float ARTSMenuBattleCamera::PickOutwardSign(const FVector& Axis) const
{
	const float InwardAmount = FVector::DotProduct(Axis.GetSafeNormal2D(), M_InwardDirection);
	if (FMath::Abs(InwardAmount) < RTSMenuBattleCameraPrivate::MinInwardAlignment)
	{
		return M_ShotStream.FRand() < 0.5f ? -1.f : 1.f;
	}
	return InwardAmount > 0.f ? -1.f : 1.f;
}

ERTSMenuBattleShot ARTSMenuBattleCamera::PickNextShot() const
{
	static const ERTSMenuBattleShot Shots[] = {
		ERTSMenuBattleShot::Orbit, ERTSMenuBattleShot::FlankDolly, ERTSMenuBattleShot::BehindLine,
		ERTSMenuBattleShot::ChaseUnit, ERTSMenuBattleShot::HighOverview, ERTSMenuBattleShot::ColumnTrack
	};
	constexpr int32 ShotCount = UE_ARRAY_COUNT(Shots);
	// Never the same shot twice in a row, and no chase without a vehicle to chase.
	for (int32 Attempt = 0; Attempt < ShotCount * 2; ++Attempt)
	{
		const ERTSMenuBattleShot Candidate = Shots[M_ShotStream.RandRange(0, ShotCount - 1)];
		const bool bRepeats = Candidate == M_ShotState.Shot;
		const bool bNeedsSubject = Candidate == ERTSMenuBattleShot::ChaseUnit || Candidate == ERTSMenuBattleShot::ColumnTrack;
		const bool bChaseWithoutSubject = bNeedsSubject && not GetHasAliveSubject();
		if (not bRepeats && not bChaseWithoutSubject)
		{
			return Candidate;
		}
	}
	return ERTSMenuBattleShot::Orbit;
}

bool ARTSMenuBattleCamera::GetHasAliveSubject() const
{
	using namespace RTSMenuBattleCameraPrivate;
	for (const TArray<TWeakObjectPtr<AActor>>* Side : {&M_GermanSubjects, &M_SovietSubjects})
	{
		for (const TWeakObjectPtr<AActor>& Unit : *Side)
		{
			if (GetIsAlive(Unit))
			{
				return true;
			}
		}
	}
	return false;
}

AActor* ARTSMenuBattleCamera::PickChaseTarget()
{
	using namespace RTSMenuBattleCameraPrivate;
	TArray<AActor*> Candidates;
	for (const TArray<TWeakObjectPtr<AActor>>* Side : {&M_GermanSubjects, &M_SovietSubjects})
	{
		for (const TWeakObjectPtr<AActor>& Unit : *Side)
		{
			if (GetIsAlive(Unit))
			{
				Candidates.Add(Unit.Get());
			}
		}
	}
	return Candidates.Num() > 0 ? Candidates[M_ShotStream.RandRange(0, Candidates.Num() - 1)] : nullptr;
}

bool ARTSMenuBattleCamera::GetSubjectsCentre(const TArray<TWeakObjectPtr<AActor>>* OnlySide, FVector& OutCentre) const
{
	using namespace RTSMenuBattleCameraPrivate;
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const TArray<TWeakObjectPtr<AActor>>* Side : {&M_GermanSubjects, &M_SovietSubjects})
	{
		if (OnlySide != nullptr && OnlySide != Side)
		{
			continue;
		}
		for (const TWeakObjectPtr<AActor>& Unit : *Side)
		{
			if (GetIsAlive(Unit))
			{
				Sum += Unit->GetActorLocation();
				++Count;
			}
		}
	}
	if (Count == 0)
	{
		return false;
	}
	OutCentre = Sum / static_cast<float>(Count);
	return true;
}

FVector ARTSMenuBattleCamera::GetFocusTarget() const
{
	FVector Centre;
	if (GetSubjectsCentre(nullptr, Centre))
	{
		return Centre;
	}
	// After the last unit died the camera keeps looking where the fighting ended.
	return M_SmoothedFocus;
}

void ARTSMenuBattleCamera::UpdateShot(const float DeltaSeconds)
{
	const FVector FocusTarget = GetFocusTarget();
	const float SpreadTarget = GetSubjectsSpread(FocusTarget);
	M_SmoothedSpread = M_ShotState.bIsCut || M_SmoothedSpread <= 0.f
		                   ? SpreadTarget
		                   : FMath::FInterpTo(M_SmoothedSpread, SpreadTarget, DeltaSeconds,
		                                      RTSMenuBattleCameraPrivate::SpreadEaseSpeed);
	// A cut frames the fighting where it is now; within a shot the centre eases along.
	M_SmoothedFocus = M_ShotState.bIsCut
		                  ? FocusTarget
		                  : FMath::VInterpTo(M_SmoothedFocus, FocusTarget, DeltaSeconds,
		                                     RTSMenuBattleCameraPrivate::FocusEaseSpeed);
	FVector Location;
	FVector LookAt;
	const float ShotSeconds = GetTimeSeconds() - M_ShotState.StartTimeSeconds;
	const float FieldOfView = ComputeShot(ShotSeconds, DeltaSeconds, Location, LookAt);
	Location = KeepAboveGround(Location);
	SetActorLocationAndRotation(Location, (LookAt - Location).Rotation());
	if (UCameraComponent* Camera = GetCameraComponent())
	{
		Camera->SetFieldOfView(FieldOfView);
	}
	if (M_ShotState.bIsCut)
	{
		MarkCameraCutForRenderer();
	}
	M_ShotState.bIsCut = false;
}

void ARTSMenuBattleCamera::MarkCameraCutForRenderer() const
{
	// Occlusion results of the previous shot would hide the landscape behind what stood in front of that camera,
	// a one-frame flash of bare sky on the new shot.
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}
	PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
}

float ARTSMenuBattleCamera::ComputeShot(const float ShotSeconds, const float DeltaSeconds, FVector& OutLocation,
                                        FVector& OutLookAt)
{
	switch (M_ShotState.Shot)
	{
	case ERTSMenuBattleShot::Orbit:
		return ComputeOrbit(ShotSeconds, OutLocation, OutLookAt);
	case ERTSMenuBattleShot::FlankDolly:
		return ComputeFlankDolly(ShotSeconds, OutLocation, OutLookAt);
	case ERTSMenuBattleShot::BehindLine:
		return ComputeBehindLine(ShotSeconds, OutLocation, OutLookAt);
	case ERTSMenuBattleShot::ChaseUnit:
		return ComputeChase(DeltaSeconds, OutLocation, OutLookAt);
	case ERTSMenuBattleShot::HighOverview:
		return ComputeHighOverview(ShotSeconds, OutLocation, OutLookAt);
	case ERTSMenuBattleShot::ColumnTrack:
		return ComputeColumnTrack(DeltaSeconds, OutLocation, OutLookAt);
	}
	return ComputeHighOverview(ShotSeconds, OutLocation, OutLookAt);
}

float ARTSMenuBattleCamera::ComputeOrbit(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const
{
	using namespace RTSMenuBattleCameraPrivate;
	const float Distance = GetFramingDistance();
	const float Height = GetFramingHeight();
	const float Yaw = M_ShotState.OrbitStartYaw + M_ShotState.SideSign * OrbitDegreesPerSecond * ShotSeconds;
	const FVector Direction = FRotator(0.f, Yaw, 0.f).Vector();
	OutLocation = M_SmoothedFocus + Direction * Distance + FVector::UpVector * Height;
	OutLookAt = M_SmoothedFocus;
	return OrbitFieldOfView;
}

float ARTSMenuBattleCamera::ComputeFlankDolly(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const
{
	using namespace RTSMenuBattleCameraPrivate;
	const float Distance = GetFramingDistance();
	const float Height = GetFramingHeight();
	const FVector Forward = M_Arena->GetBattleForward();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	// Starts level with one end of the fighting and slides along the battle axis toward the other.
	const float AlongStart = -FlankStartBackScale * Distance * M_ShotState.SideSign;
	const float Along = AlongStart + M_ShotState.SideSign * FlankDollyCmPerSecond * ShotSeconds;
	OutLocation = M_SmoothedFocus + Right * M_ShotState.SideSign * Distance * FlankDistanceScale
		+ Forward * Along + FVector::UpVector * Height * FlankHeightScale;
	OutLookAt = M_SmoothedFocus;
	return FlankFieldOfView;
}

float ARTSMenuBattleCamera::ComputeBehindLine(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const
{
	using namespace RTSMenuBattleCameraPrivate;
	const bool bGermanLine = M_ShotState.LineSide == ERTSMenuBattleSide::German;
	const TArray<TWeakObjectPtr<AActor>>* Line = bGermanLine ? &M_GermanSubjects : &M_SovietSubjects;
	FVector LineCentre;
	if (not GetSubjectsCentre(Line, LineCentre))
	{
		LineCentre = M_Arena->GetLineCenter(M_ShotState.LineSide);
	}
	const FVector Forward = M_Arena->GetBattleForward();
	const FVector TowardEnemy = bGermanLine ? Forward : -Forward;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, TowardEnemy);
	OutLocation = LineCentre - TowardEnemy * (BehindLineBackCm - BehindLinePushCmPerSecond * ShotSeconds)
		+ Right * M_ShotState.SideSign * BehindLineSideCm + FVector::UpVector * BehindLineHeightCm;
	// Over the line's shoulders at the fighting.
	OutLookAt = M_SmoothedFocus;
	return BehindLineFieldOfView;
}

float ARTSMenuBattleCamera::ComputeChase(const float DeltaSeconds, FVector& OutLocation, FVector& OutLookAt)
{
	using namespace RTSMenuBattleCameraPrivate;
	const AActor* Target = M_ShotState.ChaseTarget.Get();
	if (not IsValid(Target))
	{
		return ComputeOrbit(0.f, OutLocation, OutLookAt);
	}
	M_ShotState.ChaseAnchor = M_ShotState.bIsCut
		                          ? Target->GetActorLocation()
		                          : FMath::VInterpTo(M_ShotState.ChaseAnchor, Target->GetActorLocation(), DeltaSeconds,
		                                             ChaseEaseSpeed);
	const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetRight = FVector::CrossProduct(FVector::UpVector, TargetForward);
	OutLocation = M_ShotState.ChaseAnchor - TargetForward * ChaseBackCm + TargetRight * M_ShotState.SideSign * ChaseSideCm
		+ FVector::UpVector * ChaseHeightCm;
	OutLookAt = M_ShotState.ChaseAnchor + TargetForward * ChaseLookAheadCm;
	return ChaseFieldOfView;
}

float ARTSMenuBattleCamera::ComputeColumnTrack(const float DeltaSeconds, FVector& OutLocation, FVector& OutLookAt)
{
	using namespace RTSMenuBattleCameraPrivate;
	const bool bGerman = M_ShotState.LineSide == ERTSMenuBattleSide::German;
	FVector ColumnCentre;
	if (not GetSubjectsCentre(bGerman ? &M_GermanSubjects : &M_SovietSubjects, ColumnCentre))
	{
		return ComputeOrbit(0.f, OutLocation, OutLookAt);
	}
	// Moves with the column at its pace: the anchor eases after the column's centre like the chase camera.
	M_ShotState.ChaseAnchor = M_ShotState.bIsCut
		                          ? ColumnCentre
		                          : FMath::VInterpTo(M_ShotState.ChaseAnchor, ColumnCentre, DeltaSeconds, ChaseEaseSpeed);
	const FVector Forward = M_Arena->GetBattleForward();
	const FVector TowardEnemy = bGerman ? Forward : -Forward;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	OutLocation = M_ShotState.ChaseAnchor + Right * M_ShotState.SideSign * ColumnSideCm - TowardEnemy * ColumnBackCm
		+ FVector::UpVector * ColumnHeightCm;
	OutLookAt = M_ShotState.ChaseAnchor + TowardEnemy * ColumnLookAheadCm;
	return ColumnFieldOfView;
}

float ARTSMenuBattleCamera::ComputeHighOverview(const float ShotSeconds, FVector& OutLocation, FVector& OutLookAt) const
{
	using namespace RTSMenuBattleCameraPrivate;
	const float Distance = GetFramingDistance();
	const float Height = GetFramingHeight();
	const FVector Forward = M_Arena->GetBattleForward();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Back = M_ShotState.LineSide == ERTSMenuBattleSide::German ? -Forward : Forward;
	OutLocation = M_SmoothedFocus + Back * Distance * OverviewBackScale
		+ Right * M_ShotState.SideSign * (Distance * OverviewSideScale - OverviewDriftCmPerSecond * ShotSeconds)
		+ FVector::UpVector * Height * OverviewHeightScale;
	OutLookAt = M_SmoothedFocus;
	return OverviewFieldOfView;
}

float ARTSMenuBattleCamera::GetSubjectsSpread(const FVector& Centre) const
{
	using namespace RTSMenuBattleCameraPrivate;
	float Spread = 0.f;
	for (const TArray<TWeakObjectPtr<AActor>>* Side : {&M_GermanSubjects, &M_SovietSubjects})
	{
		for (const TWeakObjectPtr<AActor>& Unit : *Side)
		{
			if (GetIsAlive(Unit))
			{
				Spread = FMath::Max(Spread, FVector::Dist2D(Unit->GetActorLocation(), Centre));
			}
		}
	}
	return Spread;
}

float ARTSMenuBattleCamera::GetFramingDistance() const
{
	using namespace RTSMenuBattleCameraPrivate;
	const float BaseDistance = M_Arena->GetSettings().CameraDistance;
	return FMath::Max(BaseDistance * MinDistanceScale, FMath::Min(M_SmoothedSpread * SpreadToDistance, MaxFramingDistanceCm));
}

float ARTSMenuBattleCamera::GetFramingHeight() const
{
	const FRTSMenuBattleArenaSettings& Settings = M_Arena->GetSettings();
	// Same downward angle as the arena's base framing, at whatever distance the spread asks for.
	return Settings.CameraHeight * GetFramingDistance() / FMath::Max(1.f, Settings.CameraDistance);
}

FVector ARTSMenuBattleCamera::KeepAboveGround(const FVector& Location) const
{
	using namespace RTSMenuBattleCameraPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return Location;
	}
	FHitResult GroundHit;
	const FVector TraceStart = Location + FVector::UpVector * GroundTraceHalfHeightCm;
	const FVector TraceEnd = Location - FVector::UpVector * GroundTraceHalfHeightCm;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MenuBattleCameraGround), false, this);
	if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		return Location;
	}
	const float MinZ = GroundHit.ImpactPoint.Z + MinGroundClearanceCm;
	return Location.Z >= MinZ ? Location : FVector(Location.X, Location.Y, MinZ);
}
