// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

// Segment.ImpactShow ("every impact of the X"): one beat per segment. The segment's unit fires a few main-gun shells at a
// target picked so its weapon plays one surface's impact effect: sand ground, a soldier (flesh), a concrete wall (stone),
// a vehicle it penetrates (metal) or armour it bounces off. Every impact is recorded with the surface and the Niagara
// system the projectile resolved, so the post step labels each beat from what really played, and a beat that showed
// something else is reported instead of mislabelled. The camera follows Settings.Shots, anchored on the beat's events.

#include "RTSShortsDirector.h"

#include "AssetCompilingManager.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Projectile/Tests/ProjectileMapTestHooks.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsImpactShow, Log, All);

namespace RTSShortsImpactShowPrivate
{
	TArray<UWeaponState*> GetUnitWeapons(const AActor* Unit)
	{
		if (const ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			return FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
		}
		return {};
	}

	FString SurfaceName(const uint8 Surface)
	{
		switch (static_cast<ERTSSurfaceType>(Surface))
		{
		case ERTSSurfaceType::Sand:
			return TEXT("Sand");
		case ERTSSurfaceType::Metal:
			return TEXT("Metal");
		case ERTSSurfaceType::Flesh:
			return TEXT("Flesh");
		case ERTSSurfaceType::Air:
			return TEXT("Air");
		case ERTSSurfaceType::Stone:
			return TEXT("Stone");
		}
		return TEXT("Unknown");
	}

	bool TryParseSurface(const FString& Name, ERTSSurfaceType& OutSurface)
	{
		for (const ERTSSurfaceType Surface : {ERTSSurfaceType::Sand, ERTSSurfaceType::Metal, ERTSSurfaceType::Flesh,
		                                      ERTSSurfaceType::Air, ERTSSurfaceType::Stone})
		{
			if (SurfaceName(static_cast<uint8>(Surface)).Equals(Name, ESearchCase::IgnoreCase))
			{
				OutSurface = Surface;
				return true;
			}
		}
		return false;
	}

#if RTS_WITH_MAP_TESTS
	FString VerdictName(const EMapTestArmorHitVerdict Verdict)
	{
		switch (Verdict)
		{
		case EMapTestArmorHitVerdict::Bounce:
			return TEXT("Bounce");
		case EMapTestArmorHitVerdict::Penetration:
			return TEXT("Penetration");
		case EMapTestArmorHitVerdict::OverPenetration:
			return TEXT("OverPenetration");
		case EMapTestArmorHitVerdict::ArmorDestroyed:
			return TEXT("ArmorDestroyed");
		case EMapTestArmorHitVerdict::FlankAmmoDetonation:
			return TEXT("FlankAmmoDetonation");
		}
		return TEXT("Unknown");
	}
#endif

	/**
	 * Corpses are instances on a plain actor the corpse subsystem owns; hiding that actor clears them from the later
	 * beats without touching its bookkeeping. @return How many holders were hidden.
	 */
	int32 HideCorpses(UWorld* World)
	{
		int32 Hidden = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor->GetClass() == AActor::StaticClass() && Actor->FindComponentByClass<UInstancedStaticMeshComponent>() != nullptr
				&& not Actor->IsHidden())
			{
				Actor->SetActorHiddenInGame(true);
				++Hidden;
			}
		}
		return Hidden;
	}

	float SmoothStep(const float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
		return Clamped * Clamped * (3.f - 2.f * Clamped);
	}
}

AActor* ARTSShortsDirector::SpawnImpactShowTarget(const FRTSShortsSegment& Segment, const FVector& TargetWorld)
{
	const FRTSShortsImpactShow& Settings = Segment.ImpactShow;
	if (Settings.TargetKind == TEXT("Ground") || Settings.TargetClassPath.IsEmpty())
	{
		return nullptr;
	}
	UClass* TargetClass = LoadClass<AActor>(nullptr, *Settings.TargetClassPath);
	if (not IsValid(TargetClass))
	{
		UE_LOG(LogRTSShortsImpactShow, Warning, TEXT("RTS_SHORTS_IMPACT_TARGET_MISSING class=%s"), *Settings.TargetClassPath);
		return nullptr;
	}
	// Same reason as SpawnWallHolesTarget: moving an actor whose mesh is still compiling asserted in the nav octree.
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FRotator Rotation(0.f, M_Stage->GetActorRotation().Yaw + Settings.TargetYaw, 0.f);
	AActor* Target = nullptr;
	if (Settings.TargetKind == TEXT("Actor"))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Target = GetWorld()->SpawnActor<AActor>(TargetClass, TargetWorld, Rotation, Params);
		if (IsValid(Target) && Settings.bCentreTargetOnBounds)
		{
			FVector Origin;
			FVector Extent;
			Target->GetActorBounds(true, Origin, Extent);
			Target->SetActorLocation(Target->GetActorLocation() + FVector(TargetWorld.X - Origin.X, TargetWorld.Y - Origin.Y, 0.f));
		}
		if (IsValid(Target) && Settings.bTargetInvulnerable)
		{
			// A destructible fence collapsed under the 88 and its first debris frames were untextured (round 1).
			Target->SetCanBeDamaged(false);
		}
	}
	else
	{
#if RTS_WITH_MAP_TESTS
		Target = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), *Settings.TargetClassPath,
		                                                 FTransform(Rotation, TargetWorld + FVector(0.f, 0.f, 50.f)),
		                                                 static_cast<uint8>(Segment.TargetOwningPlayer));
#endif
		if (IsValid(Target))
		{
			if (UHealthComponent* Health = Target->FindComponentByClass<UHealthComponent>();
				Health != nullptr && not FMath::IsNearlyEqual(Settings.TargetHealthScale, 1.f))
			{
				Health->SetMaxHealth(Health->GetMaxHealth() * Settings.TargetHealthScale);
			}
			if (Settings.bTargetInvulnerable)
			{
				Target->SetCanBeDamaged(false);
			}
			ForceAnimationTicking(Target);
			if (ASquadController* Squad = Cast<ASquadController>(Target))
			{
				// The squad stays where it stood; it walked out of the gun's aim while the shot was held (final renders).
				Squad->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
			}
		}
	}
	UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_TARGET kind=%s class=%s ok=%d at=%s"), *Settings.TargetKind,
	       *Settings.TargetClassPath, IsValid(Target) ? 1 : 0, IsValid(Target) ? *Target->GetActorLocation().ToCompactString() : TEXT("-"));
	return Target;
}

void ARTSShortsDirector::MakeImpactShowGroundLikeGame(const FString& PhysMaterialPath)
{
	UPhysicalMaterial* PhysMaterial = LoadObject<UPhysicalMaterial>(nullptr, *PhysMaterialPath);
	if (not IsValid(PhysMaterial))
	{
		UE_LOG(LogRTSShortsImpactShow, Warning, TEXT("RTS_SHORTS_IMPACT_GROUND_PM_MISSING path=%s"), *PhysMaterialPath);
		return;
	}
	const FVector Point = M_ImpactShow.TargetPoint;
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		ALandscapeProxy* Landscape = *It;
		FVector Origin;
		FVector Extent;
		Landscape->GetActorBounds(false, Origin, Extent);
		if (Landscape->IsHidden() || FMath::Abs(Point.X - Origin.X) > Extent.X || FMath::Abs(Point.Y - Origin.Y) > Extent.Y
			|| FMath::Abs(Point.Z - Origin.Z) > Extent.Z + 100.f)
		{
			continue;
		}
#if WITH_EDITOR
		// The Shorts desert stage's landscape ignored the projectile channels and has no physical material, so shells
		// flew through it (take 3); a game map's landscape blocks them and resolves as sand.
		Landscape->DefaultPhysMaterial = PhysMaterial;
		Landscape->BodyInstance.SetResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
		Landscape->BodyInstance.SetResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
		Landscape->RecreateCollisionComponents();
#endif
		UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_GROUND landscape=%s pm=%s"), *Landscape->GetActorNameOrLabel(),
		       *PhysMaterial->GetName());
	}
}

FVector ARTSShortsDirector::ImpactShowFramePoint(const FVector& Offset) const
{
	return M_ImpactShow.TargetPoint + FRotator(0.f, M_ImpactShow.StageYaw, 0.f).RotateVector(Offset);
}

void ARTSShortsDirector::BeginImpactShow(const int32 SegmentIndex, AActor* MainUnit)
{
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	if (not Segment.ImpactShow.bEnabled)
	{
		return;
	}
	if (Segment.ImpactShow.TargetKind != TEXT("Ground") && not M_SegmentTarget.IsValid())
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("impact-show target did not spawn (check ImpactShow.TargetClassPath)"));
		return;
	}
	M_ImpactShow = FRTSShortsImpactShowState();
	M_ImpactShow.bIsActive = true;
	M_ImpactShow.Unit = MainUnit;
	M_Manifest.Segments[SegmentIndex].ImpactAttempts = 1;
	M_ImpactShow.BeginFrame = GetTimelineFrame();
	M_ImpactShow.TargetPoint = GetTargetWorldLocation(Segment, MainUnit);
	// The beat's frame runs along the line of fire, so shots read the same wherever the target stands (off to the side,
	// or behind a shooter turned round to keep the plant in the background).
	M_ImpactShow.StageYaw = IsValid(MainUnit) && FVector::Dist2D(MainUnit->GetActorLocation(), M_ImpactShow.TargetPoint) > 100.f
		? (M_ImpactShow.TargetPoint - MainUnit->GetActorLocation()).GetSafeNormal2D().Rotation().Yaw
		: M_Stage->GetActorRotation().Yaw;
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[SegmentIndex];
	ManifestSegment.ImpactBeat = Segment.ImpactShow.Beat;
	if (not Segment.ImpactShow.GroundPhysMaterialPath.IsEmpty())
	{
		MakeImpactShowGroundLikeGame(Segment.ImpactShow.GroundPhysMaterialPath);
		// The world-static trace found something 2 m above the sand (round 2: the shell overflew by 34 m): the aim
		// point is the ground the shells now hit.
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ImpactShowGround), false, MainUnit);
		const FVector Point = M_ImpactShow.TargetPoint;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.f, 0.f, 3000.f), Point - FVector(0.f, 0.f, 3000.f),
		                                         COLLISION_TRACE_ENEMY, Params))
		{
			UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_GROUND_Z from=%.0f to=%.0f actor=%s"), Point.Z,
			       Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetActor()));
			M_ImpactShow.TargetPoint.Z = Hit.ImpactPoint.Z;
		}
	}
#if RTS_WITH_MAP_TESTS
	M_ImpactShow.LaunchHandle = FProjectileMapTestHooks::OnShellLaunched().AddLambda(
		[this](const FMapTestShellLaunchReport& Report)
		{
			if (not M_ImpactShow.bIsActive || M_ImpactShow.bEnded || M_ImpactShow.FireFrame < 0
				|| Report.WeaponOwner.Get() == nullptr || Report.WeaponOwner.Get() != M_ImpactShow.MainGunOwner.Get()
				|| static_cast<uint32>(Report.WeaponName) != M_ImpactShow.MainGunName
				|| not M_Manifest.Segments.IsValidIndex(M_CurrentSegment))
			{
				return;
			}
			FRTSShortsManifestSegment& Manifest = M_Manifest.Segments[M_CurrentSegment];
			const int32 ShellIndex = Manifest.ImpactShellsFired++;
			M_ImpactShow.ShellByLaunchId.Add(Report.ShellLaunchId, ShellIndex);
			const int32 Frame = GetTimelineFrame();
			M_ImpactShow.LastLaunchFrame = Frame;
			if (M_ImpactShow.FirstLaunchFrame < 0)
			{
				M_ImpactShow.FirstLaunchFrame = Frame;
				Manifest.ImpactLaunchFrame = Frame;
			}
			UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_LAUNCH segment=%d shell=%d frame=%d launch=%u"),
			       M_CurrentSegment, ShellIndex, Frame, Report.ShellLaunchId);
		});
	M_ImpactShow.ImpactHandle = FProjectileMapTestHooks::OnImpact().AddUObject(this, &ARTSShortsDirector::OnImpactShowImpact);
	M_ImpactShow.ArmorHandle = FProjectileMapTestHooks::OnArmorHit().AddLambda(
		[this](const FMapTestArmorHitReport& Report)
		{
			if (M_ImpactShow.bIsActive && M_ImpactShow.ShellByLaunchId.Contains(Report.ShellLaunchId)
				&& not M_ImpactShow.VerdictByLaunchId.Contains(Report.ShellLaunchId))
			{
				M_ImpactShow.VerdictByLaunchId.Add(Report.ShellLaunchId, RTSShortsImpactShowPrivate::VerdictName(Report.Verdict));
				UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_ARMOR launch=%u verdict=%s raw=%.0f effective=%.0f pen=%.0f angle=%.0f"),
				       Report.ShellLaunchId, *RTSShortsImpactShowPrivate::VerdictName(Report.Verdict), Report.RawArmor,
				       Report.EffectiveArmor, Report.PenAtImpact, Report.ImpactAngleDegrees);
			}
		});
#endif
	UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_BEGIN segment=%d beat=%s kind=%s target=%s point=%s"),
	       SegmentIndex, *Segment.ImpactShow.Beat, *Segment.ImpactShow.TargetKind, *GetNameSafe(M_SegmentTarget.Get()),
	       *M_ImpactShow.TargetPoint.ToCompactString());
	TickImpactShow(GetTimelineFrame());
}

void ARTSShortsDirector::SetImpactShowWeaponBlocked(UWeaponState* Weapon, const bool bBlocked)
{
	if (not IsValid(Weapon))
	{
		return;
	}
	Weapon->SetModuleFireRestriction(this, bBlocked);
	M_ImpactShow.RestrictedWeapons.AddUnique(Weapon);
}

void ARTSShortsDirector::OrderImpactShowFire(const int32 SegmentIndex)
{
	const FRTSShortsImpactShow& Settings = M_Job.Segments[SegmentIndex].ImpactShow;
	AActor* Unit = M_ImpactShow.Unit.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr)
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("unit has no commands interface"));
		return;
	}
	const int32 Frame = GetTimelineFrame();
	M_ImpactShow.FireFrame = Frame;
	M_Manifest.Segments[SegmentIndex].FireOrderFrame = Frame;
	if (Settings.ReleaseGunAtSeconds < 0.f || M_ImpactShow.Attempt > 1)
	{
		M_ImpactShow.bGunReleased = true;
	}
	SetImpactShowWeaponBlocked(M_ImpactShow.MainGun.Get(), not M_ImpactShow.bGunReleased);
	AActor* Target = M_SegmentTarget.Get();
	if (ASquadController* Squad = Cast<ASquadController>(Target))
	{
		// A squad takes no hits itself: the soldier nearest the shooter is the one the gun aims at, so he falls in front
		// of the others (round 1: the casualty fell behind standing soldiers).
		ASquadUnit* Nearest = nullptr;
		const FVector From = IsValid(Unit) ? Unit->GetActorLocation() : M_ImpactShow.TargetPoint;
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			if (IsValid(Soldier) && (Nearest == nullptr || FVector::DistSquared(Soldier->GetActorLocation(), From)
				< FVector::DistSquared(Nearest->GetActorLocation(), From)))
			{
				Nearest = Soldier;
			}
		}
		Target = Nearest;
	}
	ECommandQueueError Result;
	FVector& AimPoint = M_ImpactShow.AimPoint;
	AimPoint = ImpactShowFramePoint(Settings.AimOffset);
	if (Settings.TargetKind == TEXT("Unit") && IsValid(Target))
	{
		M_ImpactShow.AttackTarget = Target;
		// The gun picks its own spot on a unit; AimOffset is where the shell is expected to land, for the camera.
		AimPoint = Target->GetActorLocation() + FRotator(0.f, M_ImpactShow.StageYaw, 0.f).RotateVector(Settings.AimOffset);
		Result = Settings.bFireAtAimPoint ? Commands->AttackGround(AimPoint, true) : Commands->AttackActor(Target, true);
	}
	else
	{
		Result = Commands->AttackGround(AimPoint, true);
	}
	UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_FIRE segment=%d frame=%d result=%d target=%s aim=%s gun=%s"),
	       SegmentIndex, Frame, static_cast<int32>(Result), *GetNameSafe(Target), *AimPoint.ToCompactString(),
	       *GetNameSafe(M_ImpactShow.MainGun.Get()));
}

void ARTSShortsDirector::OnImpactShowImpact(const FMapTestImpactReport& Report)
{
#if RTS_WITH_MAP_TESTS
	using namespace RTSShortsImpactShowPrivate;
	const int32* ShellIndex = M_ImpactShow.bIsActive ? M_ImpactShow.ShellByLaunchId.Find(Report.ShellLaunchId) : nullptr;
	if (ShellIndex == nullptr || not M_Manifest.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const int32 Frame = GetTimelineFrame();
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[M_CurrentSegment];
	FRTSShortsImpactRecord& Record = ManifestSegment.Impacts.AddDefaulted_GetRef();
	Record.Frame = Frame;
	Record.ShellIndex = *ShellIndex;
	Record.Surface = Report.bBounce ? TEXT("Bounce") : SurfaceName(Report.Surface);
	Record.EffectName = Report.EffectName;
	Record.HitActor = GetNameSafe(Report.HitActor.Get());
	if (const FString* Verdict = M_ImpactShow.VerdictByLaunchId.Find(Report.ShellLaunchId))
	{
		Record.Verdict = *Verdict;
	}
	const FVector2D Screen = ProjectToFrame(Report.Location);
	Record.ScreenX = Screen.X;
	Record.ScreenY = Screen.Y;
	const FString& Beat = M_Job.Segments[M_CurrentSegment].ImpactShow.Beat;
	const bool bFirstOfShell = not M_ImpactShow.ShellsWithImpact.Contains(*ShellIndex);
	if (bFirstOfShell)
	{
		M_ImpactShow.ShellsWithImpact.Add(*ShellIndex);
		M_ImpactShow.LastShellImpactFrame = Frame;
		if (M_ImpactShow.FirstImpactFrame < 0)
		{
			M_ImpactShow.FirstImpactFrame = Frame;
			M_ImpactShow.FirstImpactLocation = Report.Location;
			M_ImpactShow.FramedImpactFrame = Frame;
			M_ImpactShow.FramedImpactLocation = Report.Location;
			ManifestSegment.ImpactFirstFrame = Frame;
		}
		// Only a shell's first impact counts: a ricochet's second landing would show the ground's sand, not the beat.
		if (Record.Surface.Equals(Beat, ESearchCase::IgnoreCase) && not ManifestSegment.bImpactBeatShown)
		{
			ManifestSegment.bImpactBeatShown = true;
			ManifestSegment.ImpactShownShell = *ShellIndex;
			// A burst's first round can miss (an airburst 36 m behind the squad, Pz II F): the close shot moves on to
			// the round that showed the beat.
			if (M_ImpactShow.FramedImpactFrame != Frame)
			{
				M_ImpactShow.FramedImpactFrame = Frame;
				M_ImpactShow.FramedImpactLocation = Report.Location;
			}
			ManifestSegment.ImpactMissCm = FVector::Dist(Report.Location, M_ImpactShow.AimPoint);
		}
	}
	UE_LOG(LogRTSShortsImpactShow, Display,
	       TEXT("RTS_SHORTS_IMPACT_HIT segment=%d beat=%s shell=%d first=%d frame=%d surface=%s effect=%s actor=%s verdict=%s at=%s screen=%.3f,%.3f"),
	       M_CurrentSegment, *Beat, *ShellIndex, bFirstOfShell ? 1 : 0, Frame, *Record.Surface, *Record.EffectName, *Record.HitActor,
	       *Record.Verdict, *Report.Location.ToCompactString(), Record.ScreenX, Record.ScreenY);
	WriteManifest();
#endif
}

void ARTSShortsDirector::SetImpactShowTimeDilation(const float Dilation) const
{
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), FMath::Clamp(Dilation, 0.05f, 1.f));
}

void ARTSShortsDirector::ApplyImpactShowShot(const int32 Index, const int32 Frame)
{
	const FRTSShortsImpactShow& Settings = M_Job.Segments[M_CurrentSegment].ImpactShow;
	if (not Settings.Shots.IsValidIndex(Index) || not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsWallHolesShot& Shot = Settings.Shots[Index];
	const float Alpha = Shot.MoveSeconds > 0.f
		? RTSShortsImpactShowPrivate::SmoothStep(static_cast<float>(Frame - M_ImpactShow.ActiveShotFrame) / (Shot.MoveSeconds * M_Job.Fps))
		: 0.f;
	const FVector Offset = Shot.MoveSeconds > 0.f ? FMath::Lerp(Shot.Offset, Shot.EndOffset, Alpha) : Shot.Offset;
	const FVector LookOffset = Shot.MoveSeconds > 0.f ? FMath::Lerp(Shot.LookOffset, Shot.EndLookOffset, Alpha) : Shot.LookOffset;
	const float Focal = Shot.MoveSeconds > 0.f && Shot.EndFocalLength > 0.f
		? FMath::Lerp(Shot.FocalLength, Shot.EndFocalLength, Alpha)
		: Shot.FocalLength;
	// "Impact" shots are framed on where the shell really hit (the close shots framed the target's origin, round 1).
	const FRotator Axes(0.f, M_ImpactShow.StageYaw, 0.f);
	FVector Origin = ImpactShowFramePoint(FVector::ZeroVector);
	if (Shot.Frame == TEXT("Impact") && M_ImpactShow.FramedImpactFrame >= 0)
	{
		Origin = M_ImpactShow.FramedImpactLocation;
	}
	else if (Shot.Frame == TEXT("AimThenImpact"))
	{
		// A cut before the hit (round 3: show the target waiting) frames the expected hit; once the shell lands the
		// frame glides onto the real impact point in 0.2 s.
		Origin = M_ImpactShow.AimPoint;
		if (M_ImpactShow.FramedImpactFrame >= 0)
		{
			// From wherever the frame was (the aim, or a missed round's impact) onto the framed impact.
			if (M_ImpactShow.SettleFromFrame != M_ImpactShow.FramedImpactFrame)
			{
				M_ImpactShow.SettleFromFrame = M_ImpactShow.FramedImpactFrame;
				M_ImpactShow.SettleFrom = M_ImpactShow.FramedImpactFrame == M_ImpactShow.FirstImpactFrame
					? M_ImpactShow.AimPoint
					: M_ImpactShow.LastFrameOrigin;
			}
			const float Settle = RTSShortsImpactShowPrivate::SmoothStep(
				static_cast<float>(Frame - M_ImpactShow.FramedImpactFrame) / FMath::Max(1.f, 0.2f * M_Job.Fps));
			Origin = FMath::Lerp(M_ImpactShow.SettleFrom, M_ImpactShow.FramedImpactLocation, Settle);
		}
	}
	M_ImpactShow.LastFrameOrigin = Origin;
	FVector CameraLocation = Origin + Axes.RotateVector(Offset);
	const FVector LookPoint = Origin + Axes.RotateVector(LookOffset);
	// Never below the ground: the sand beat's hero camera sat under the sloping desert landscape (round 3).
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(ImpactShowCameraGround), false);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, CameraLocation + FVector(0.f, 0.f, 5000.f), CameraLocation - FVector(0.f, 0.f, 5000.f),
	                                         COLLISION_TRACE_LANDSCAPE, GroundParams))
	{
		constexpr float MinGroundClearanceCm = 100.f;
		CameraLocation.Z = FMath::Max(CameraLocation.Z, GroundHit.ImpactPoint.Z + MinGroundClearanceCm);
	}
	FRotator Rotation = (LookPoint - CameraLocation).Rotation();
	Rotation.Roll = 0.f;
	M_Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
	if (UCineCameraComponent* Cine = M_Camera->GetCineCameraComponent())
	{
		Cine->SetCurrentFocalLength(Focal);
	}
	if (Frame == M_ImpactShow.ActiveShotFrame)
	{
		ApplyCameraCut(CameraLocation);
		UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_SHOT segment=%d shot=%d frame=%d camera=%s focal=%.1f"),
		       M_CurrentSegment, Index, Frame, *CameraLocation.ToCompactString(), Focal);
	}
}

void ARTSShortsDirector::TickImpactShow(const int32 Frame)
{
	using namespace RTSShortsImpactShowPrivate;
	if (not M_ImpactShow.bIsActive || M_ImpactShow.bEnded || not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const FRTSShortsImpactShow& Settings = M_Job.Segments[M_CurrentSegment].ImpactShow;
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[M_CurrentSegment];
	const float Fps = static_cast<float>(M_Job.Fps);
	AActor* Unit = M_ImpactShow.Unit.Get();

	// Only the main gun fires, and only between the fire order and its last shell (turrets may come up a tick late).
	if (IsValid(Unit))
	{
		UWeaponState* MainGun = M_ImpactShow.MainGun.Get();
		const TArray<UWeaponState*> Weapons = GetUnitWeapons(Unit);
		if (not IsValid(MainGun))
		{
			for (UWeaponState* Weapon : Weapons)
			{
				if (IsValid(Weapon) && (MainGun == nullptr || Weapon->GetRawWeaponData().WeaponCalibre > MainGun->GetRawWeaponData().WeaponCalibre))
				{
					MainGun = Weapon;
				}
			}
			if (IsValid(MainGun))
			{
				M_ImpactShow.MainGun = MainGun;
				M_ImpactShow.MainGunOwner = MainGun->GetWeaponOwnerObject();
				M_ImpactShow.MainGunName = static_cast<uint32>(MainGun->GetRawWeaponData().WeaponName);
				SetImpactShowWeaponBlocked(MainGun, not M_ImpactShow.bGunReleased);
				FString WeaponName = UEnum::GetValueAsString(MainGun->GetRawWeaponData().WeaponName);
				FString Left;
				FString Right;
				ManifestSegment.ImpactWeaponName = WeaponName.Split(TEXT("::"), &Left, &Right) ? Right : WeaponName;
				// The weapon's own effect for the beat: what the viewer should see if the surface resolves as planned.
				if (const FWeaponVFX* Vfx = MainGun->GetWeaponVfx())
				{
					ERTSSurfaceType Surface;
					if (Settings.Beat.Equals(TEXT("Bounce"), ESearchCase::IgnoreCase))
					{
						ManifestSegment.ImpactExpectedEffect = GetNameSafe(Vfx->BounceEffect);
					}
					else if (TryParseSurface(Settings.Beat, Surface))
					{
						const FRTSSurfaceImpactData* Data = Vfx->SurfaceImpactEffects.Find(Surface);
						ManifestSegment.ImpactExpectedEffect = Data != nullptr ? GetNameSafe(Data->ImpactEffect) : FString();
					}
				}
				UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_GUN segment=%d weapon=%s calibre=%.0f shell=%s expected=%s"),
				       M_CurrentSegment, *ManifestSegment.ImpactWeaponName, MainGun->GetRawWeaponData().WeaponCalibre,
				       *UEnum::GetValueAsString(MainGun->GetRawWeaponData().ShellType), *ManifestSegment.ImpactExpectedEffect);
			}
		}
		for (UWeaponState* Weapon : Weapons)
		{
			if (IsValid(Weapon) && Weapon != MainGun && not M_ImpactShow.RestrictedWeapons.Contains(Weapon))
			{
				SetImpactShowWeaponBlocked(Weapon, true);
			}
		}
		if (not M_ImpactShow.bGunBlocked
			&& ManifestSegment.ImpactShellsFired - M_ImpactShow.AttemptShellBase >= FMath::Max(1, Settings.Shells))
		{
			M_ImpactShow.bGunBlocked = true;
			SetImpactShowWeaponBlocked(MainGun, true);
			if (ICommands* Commands = Cast<ICommands>(Unit))
			{
				Commands->StopCommand();
			}
			UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_GUN_DONE segment=%d frame=%d shells=%d"), M_CurrentSegment, Frame,
			       ManifestSegment.ImpactShellsFired);
		}
	}

	// A soldier killed in the flesh beat can settle into a corpse after that beat ended (its ragdoll takes a while), so
	// later beats keep hiding corpse holders.
	if (not Settings.Beat.Equals(TEXT("Flesh"), ESearchCase::IgnoreCase) && Frame % 10 == 0)
	{
		HideCorpses(GetWorld());
	}

	// Squad soldiers are separate actors and spawn after the controller: their animation must tick offscreen too.
	if (ASquadController* Squad = Cast<ASquadController>(M_SegmentTarget.Get()))
	{
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			if (IsValid(Soldier) && not M_ImpactShow.AnimForced.Contains(Soldier))
			{
				M_ImpactShow.AnimForced.Add(Soldier);
				ForceAnimationTicking(Soldier);
			}
		}
	}

	// The gun fires at a known time: released ReleaseGunAtSeconds into the beat, after the turret had time to aim.
	if (M_ImpactShow.FireFrame >= 0 && not M_ImpactShow.bGunReleased && not M_ImpactShow.bGunBlocked
		&& Frame - M_ImpactShow.BeginFrame >= FMath::RoundToInt(Settings.ReleaseGunAtSeconds * Fps))
	{
		M_ImpactShow.bGunReleased = true;
		UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_GUN_RELEASED segment=%d frame=%d"), M_CurrentSegment, Frame);
		// A fresh order on release: a soldier picked 9 s earlier had moved, and three 88 shells flew past the squad
		// (final renders, 2026-11-21). The order also frees the gun now that bGunReleased is set.
		OrderImpactShowFire(M_CurrentSegment);
	}

	// Slow motion from the first impact; the output keeps its frame rate, so the impact plays out over more frames.
	if (Settings.ImpactTimeDilation < 1.f && Settings.SlowMoSeconds > 0.f && M_ImpactShow.FirstImpactFrame >= 0 && not M_ImpactShow.bSlowMoDone)
	{
		if (M_ImpactShow.SlowMoFrame < 0)
		{
			M_ImpactShow.SlowMoFrame = Frame;
		}
		const float SinceSlow = static_cast<float>(Frame - M_ImpactShow.SlowMoFrame) / Fps;
		const float EaseOut = FMath::Max(0.01f, Settings.SlowMoEaseOutSeconds);
		const float Alpha = SinceSlow <= Settings.SlowMoSeconds ? 0.f : SmoothStep((SinceSlow - Settings.SlowMoSeconds) / EaseOut);
		SetImpactShowTimeDilation(FMath::Lerp(Settings.ImpactTimeDilation, 1.f, Alpha));
		if (Alpha >= 1.f)
		{
			M_ImpactShow.bSlowMoDone = true;
		}
	}

	// Camera: the latest shot (in list order, from NextShot on) whose anchor event happened and whose time came; earlier
	// pending shots are dropped. A retry fires as soon as the gun reloads, before the "Begin" re-cut meant for the held
	// shot, and that pending re-cut kept the close shot from ever starting (Pz IV G flesh, final render).
	int32 DueShot = INDEX_NONE;
	for (int32 Index = M_ImpactShow.NextShot; Settings.Shots.IsValidIndex(Index); ++Index)
	{
		const FRTSShortsWallHolesShot& Shot = Settings.Shots[Index];
		const int32 AnchorFrame = Shot.Anchor == TEXT("Fire") ? M_ImpactShow.FirstLaunchFrame
			: Shot.Anchor == TEXT("Impact") ? M_ImpactShow.FirstImpactFrame
			: M_ImpactShow.BeginFrame;
		if (AnchorFrame >= 0 && Frame >= AnchorFrame + FMath::RoundToInt(Shot.AtSeconds * Fps))
		{
			DueShot = Index;
		}
	}
	if (DueShot != INDEX_NONE)
	{
		M_ImpactShow.ActiveShot = DueShot;
		M_ImpactShow.ActiveShotFrame = Frame;
		ManifestSegment.CameraCutFrames.Add(Frame);
		M_ImpactShow.NextShot = DueShot + 1;
	}
	if (M_ImpactShow.ActiveShot >= 0)
	{
		ApplyImpactShowShot(M_ImpactShow.ActiveShot, Frame);
	}

	// Bursts at a squad: the soldier the gun aimed at fell mid-burst, so the gun goes quiet; order it onto the nearest
	// soldier still standing (dead soldiers leave the squad's list).
	const int32 FiredThisAttempt = ManifestSegment.ImpactShellsFired - M_ImpactShow.AttemptShellBase;
	const bool bBurst = Settings.Shells > 1;
	if (bBurst && FiredThisAttempt > 0 && not M_ImpactShow.bGunBlocked && M_ImpactShow.bGunReleased
		&& Cast<ASquadController>(M_SegmentTarget.Get()) != nullptr
		&& Frame >= FMath::Max(M_ImpactShow.LastLaunchFrame, M_ImpactShow.LastRetargetFrame) + FMath::RoundToInt(Settings.BurstRetargetSeconds * Fps))
	{
		M_ImpactShow.LastRetargetFrame = Frame;
		UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_RETARGET segment=%d frame=%d fired=%d"), M_CurrentSegment, Frame,
		       FiredThisAttempt);
		// Keep the attempt's fire frame: the timeout still runs from the first order.
		const int32 FireFrame = M_ImpactShow.FireFrame;
		const int32 FireOrderFrame = ManifestSegment.FireOrderFrame;
		OrderImpactShowFire(M_CurrentSegment);
		M_ImpactShow.FireFrame = FireFrame;
		ManifestSegment.FireOrderFrame = FireOrderFrame;
	}

	// End: once every shell has landed and held, or on the timeout. A burst that stopped early (its soldier and the
	// squad gone, the magazine dry) counts as landed once every launched shell hit and the gun stayed quiet.
	const int32 Shells = M_ImpactShow.AttemptShellBase + FMath::Max(1, Settings.Shells);
	bool bAllLanded = M_ImpactShow.ShellsWithImpact.Num() >= Shells && M_ImpactShow.LastShellImpactFrame >= 0;
	if (not bAllLanded && bBurst && FiredThisAttempt > 0 && M_ImpactShow.LastShellImpactFrame >= 0
		&& M_ImpactShow.ShellsWithImpact.Num() >= ManifestSegment.ImpactShellsFired
		&& Frame >= M_ImpactShow.LastLaunchFrame + FMath::RoundToInt(Settings.BurstIdleSeconds * Fps))
	{
		bAllLanded = true;
		if (not M_ImpactShow.bGunBlocked)
		{
			M_ImpactShow.bGunBlocked = true;
			SetImpactShowWeaponBlocked(M_ImpactShow.MainGun.Get(), true);
			if (ICommands* Commands = Cast<ICommands>(Unit))
			{
				Commands->StopCommand();
			}
			UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_BURST_SHORT segment=%d frame=%d fired=%d of %d"), M_CurrentSegment,
			       Frame, FiredThisAttempt, Settings.Shells);
		}
	}
	if (bAllLanded && not ManifestSegment.bImpactBeatShown && M_ImpactShow.Attempt < FMath::Max(1, Settings.MaxAttempts)
		&& Frame >= M_ImpactShow.LastShellImpactFrame + FMath::RoundToInt(Settings.RetryDelaySeconds * Fps))
	{
		// The shell showed another surface (the T-34's glacis bounced it at 85 degrees, take 2): fire again with the
		// camera starting over; the post cuts from this attempt's launch.
		++M_ImpactShow.Attempt;
		ManifestSegment.ImpactAttempts = M_ImpactShow.Attempt;
		M_ImpactShow.AttemptShellBase = ManifestSegment.ImpactShellsFired;
		M_ImpactShow.bGunBlocked = false;
		M_ImpactShow.NextShot = 0;
		M_ImpactShow.ActiveShot = -1;
		M_ImpactShow.BeginFrame = Frame;
		M_ImpactShow.FirstLaunchFrame = -1;
		M_ImpactShow.FirstImpactFrame = -1;
		M_ImpactShow.FramedImpactFrame = -1;
		M_ImpactShow.SettleFromFrame = -1;
		ManifestSegment.ImpactLaunchFrame = -1;
		ManifestSegment.ImpactFirstFrame = -1;
		UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_RETRY segment=%d frame=%d attempt=%d"), M_CurrentSegment, Frame,
		       M_ImpactShow.Attempt);
		OrderImpactShowFire(M_CurrentSegment);
		return;
	}
	const bool bHeld = bAllLanded && Frame >= M_ImpactShow.LastShellImpactFrame + FMath::RoundToInt(Settings.HoldAfterImpactSeconds * Fps);
	const bool bTimedOut = M_ImpactShow.FireFrame >= 0 && Frame >= M_ImpactShow.FireFrame + FMath::RoundToInt(Settings.TimeoutSeconds * Fps);
	if (not bHeld && not bTimedOut)
	{
		return;
	}
	M_ImpactShow.bEnded = true;
	ManifestSegment.ImpactEndFrame = Frame;
	SetImpactShowTimeDilation(1.f);
	UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_END segment=%d frame=%d beat=%s shown=%d shells=%d impacts=%d timedOut=%d"),
	       M_CurrentSegment, Frame, *Settings.Beat, ManifestSegment.bImpactBeatShown ? 1 : 0, ManifestSegment.ImpactShellsFired,
	       ManifestSegment.Impacts.Num(), bTimedOut && not bHeld ? 1 : 0);
	ShortenRunningSegment(Frame + 1);
	WriteManifest();
}

void ARTSShortsDirector::EndImpactShow()
{
	if (not M_ImpactShow.bIsActive)
	{
		return;
	}
#if RTS_WITH_MAP_TESTS
	FProjectileMapTestHooks::OnShellLaunched().Remove(M_ImpactShow.LaunchHandle);
	FProjectileMapTestHooks::OnImpact().Remove(M_ImpactShow.ImpactHandle);
	FProjectileMapTestHooks::OnArmorHit().Remove(M_ImpactShow.ArmorHandle);
#endif
	for (const TWeakObjectPtr<UWeaponState>& Weapon : M_ImpactShow.RestrictedWeapons)
	{
		if (UWeaponState* State = Weapon.Get())
		{
			State->SetModuleFireRestriction(this, false);
		}
	}
	SetImpactShowTimeDilation(1.f);
	// The flesh beat's corpses lay in the stone beat's shot (rounds 2 and 3).
	const int32 HiddenCorpseHolders = RTSShortsImpactShowPrivate::HideCorpses(GetWorld());
	UE_LOG(LogRTSShortsImpactShow, Display, TEXT("RTS_SHORTS_IMPACT_DONE segment=%d hiddenCorpseHolders=%d"), M_CurrentSegment,
	       HiddenCorpseHolders);
	M_ImpactShow = FRTSShortsImpactShowState();
}
