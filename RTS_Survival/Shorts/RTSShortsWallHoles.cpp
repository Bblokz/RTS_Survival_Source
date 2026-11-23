// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

// Segment.WallHoles ("what do four X do to a wall?"): the segment's unit and three copies of it fire their main guns at
// a fresh building that carries UWallHoleDamageComponent. Each attacker steps through aim points spread over the face it
// sees, so the shell holes spread instead of widening one hole. The volley stops after FireSeconds, or once the target
// is down to StopAtHealthFraction (the finale may kill it), the camera cuts to the result and the round ends.
// The camera is driven by Settings.Shots, poses in the target's stage frame anchored on the round's events.

#include "RTSShortsDirector.h"

#include "AssetCompilingManager.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Projectile/Tests/ProjectileMapTestHooks.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsWallHoles, Log, All);

namespace RTSShortsWallHolesPrivate
{
	// An attacker that left its mark by more than this (it drove toward its aim point) is put back.
	constexpr float MaxDriftCm = 50.f;
	// After the cease-fire the target takes no more damage (shells already in flight): the result must stay standing.
	constexpr float ProtectedDamageReduction = 1.0e9f;

	TArray<UWeaponState*> GetUnitWeapons(const AActor* Unit)
	{
		if (const ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			return FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
		}
		return {};
	}

	FFloatProperty* FindFloatProperty(const AActor* Actor, const TCHAR* Name)
	{
		return IsValid(Actor) ? FindFProperty<FFloatProperty>(Actor->GetClass(), Name) : nullptr;
	}

	/** Health of an environment actor (protected UPROPERTY) or of the actor's health component; negative when unknown. */
	float ReadTargetHealth(const AActor* Target)
	{
		if (not IsValid(Target))
		{
			return 0.f;
		}
		if (const UHealthComponent* HealthComponent = Target->FindComponentByClass<UHealthComponent>())
		{
			return HealthComponent->GetCurrentHealth();
		}
		if (const FFloatProperty* Health = FindFloatProperty(Target, TEXT("Health")))
		{
			return Health->GetPropertyValue_InContainer(Target);
		}
		return -1.f;
	}

	float SmoothStep(const float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
		return Clamped * Clamped * (3.f - 2.f * Clamped);
	}
}

AActor* ARTSShortsDirector::SpawnWallHolesTarget(const FRTSShortsSegment& Segment, const FVector& TargetWorld)
{
	UClass* TargetClass = LoadClass<AActor>(nullptr, *Segment.TargetClassPath);
	if (not IsValid(TargetClass))
	{
		UE_LOG(LogRTSShortsWallHoles, Warning, TEXT("RTS_SHORTS_WALL_TARGET_MISSING class=%s"), *Segment.TargetClassPath);
		return nullptr;
	}
	// The class load just pulled in its meshes, which compile asynchronously in editor builds: moving the new actor while
	// its mesh finished compiling re-entered the nav octree and asserted (MTAccessDetector, first probe 2026-11-10).
	FAssetCompilingManager::Get().FinishAllCompilation();
	const float StageYaw = M_Stage->GetActorRotation().Yaw;
	const FRotator Rotation(0.f, StageYaw + Segment.WallHoles.TargetYaw, 0.f);
	// The target point stays the frame origin (aim points, camera); the first target may stand off it (two trees).
	const FVector TargetWorldShifted = TargetWorld + FRotator(0.f, StageYaw, 0.f).RotateVector(Segment.WallHoles.PrimaryOffset);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Target = GetWorld()->SpawnActor<AActor>(TargetClass, TargetWorldShifted, Rotation, Params);
	if (not IsValid(Target))
	{
		return nullptr;
	}
	if (Segment.WallHoles.bCentreTargetOnBounds)
	{
		// Only colliding components: an editor sprite at the root would drag the centre to the actor origin.
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(true, Origin, Extent);
		const FVector Shift(TargetWorldShifted.X - Origin.X, TargetWorldShifted.Y - Origin.Y, 0.f);
		Target->SetActorLocation(Target->GetActorLocation() + Shift);
		UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_TARGET_CENTRED shift=%s extent=%s"),
		       *Shift.ToCompactString(), *Extent.ToCompactString());
	}
	return Target;
}

void ARTSShortsDirector::SpawnStageProps()
{
	if (bM_StagePropsSpawned || M_Job.StageProps.IsEmpty() || not IsValid(M_Stage))
	{
		return;
	}
	bM_StagePropsSpawned = true;
	TArray<UStaticMesh*> Meshes;
	for (const FRTSShortsStageProp& Prop : M_Job.StageProps)
	{
		Meshes.Add(LoadObject<UStaticMesh>(nullptr, *Prop.MeshPath));
	}
	// Loading compiles the meshes asynchronously in editor builds; spawn only once they are done (see SpawnWallHolesTarget).
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FTransform StageTransform = M_Stage->GetActorTransform();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ShortsStageProps), false);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < M_Job.StageProps.Num(); ++Index)
	{
		const FRTSShortsStageProp& Prop = M_Job.StageProps[Index];
		if (Meshes[Index] == nullptr)
		{
			UE_LOG(LogRTSShortsWallHoles, Warning, TEXT("RTS_SHORTS_STAGE_PROP_MISSING mesh=%s"), *Prop.MeshPath);
			continue;
		}
		FVector Location = StageTransform.TransformPosition(Prop.Offset);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 5000.f), Location - FVector(0.f, 0.f, 5000.f),
		                                         ECC_WorldStatic, TraceParams))
		{
			Location.Z = Hit.ImpactPoint.Z;
		}
		Location.Z -= Prop.SinkCm;
		const FTransform Transform(FRotator(0.f, StageTransform.Rotator().Yaw + Prop.Yaw, 0.f), Location, Prop.Scale);
		AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, Params);
		if (not IsValid(Actor))
		{
			continue;
		}
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		// Movable: a runtime static actor has no built lighting and renders with preview lightmaps.
		Component->SetMobility(EComponentMobility::Movable);
		// Set dressing never touches the baked navmesh, and with no collision it cannot stop a shell.
		Component->SetCanEverAffectNavigation(false);
		Component->SetCollisionEnabled(Prop.bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Component->SetStaticMesh(Meshes[Index]);
		TraceParams.AddIgnoredActor(Actor);
		++Spawned;
	}
	UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_STAGE_PROPS spawned=%d of %d"), Spawned, M_Job.StageProps.Num());
}

FVector ARTSShortsDirector::WallHolesFramePoint(const FVector& Offset) const
{
	return M_WallHoles.TargetPoint + FRotator(0.f, M_WallHoles.StageYaw, 0.f).RotateVector(Offset);
}

void ARTSShortsDirector::BeginWallHoles(const int32 SegmentIndex, AActor* MainUnit)
{
	const FRTSShortsSegment& Segment = M_Job.Segments[SegmentIndex];
	if (not Segment.WallHoles.bEnabled)
	{
		return;
	}
	if (not M_SegmentTarget.IsValid())
	{
		MarkSegmentSkipped(SegmentIndex, TEXT("wall-holes target did not spawn (check TargetClassPath)"));
		return;
	}
	M_WallHoles = FRTSShortsWallHolesState();
	M_WallHoles.bIsActive = true;
	M_WallHoles.BeginFrame = GetTimelineFrame();
	M_WallHoles.StageYaw = M_Stage->GetActorRotation().Yaw;
	M_WallHoles.TargetPoint = GetTargetWorldLocation(Segment, MainUnit);
	M_WallHoles.Targets.Add(M_SegmentTarget);
	if (Segment.WallHoles.ExtraTargetOffsets.Num() > 0)
	{
		UClass* TargetClass = M_SegmentTarget->GetClass();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 Index = 0; Index < Segment.WallHoles.ExtraTargetOffsets.Num(); ++Index)
		{
			const float Yaw = Segment.WallHoles.ExtraTargetYaws.IsValidIndex(Index) ? Segment.WallHoles.ExtraTargetYaws[Index]
				: Segment.WallHoles.TargetYaw;
			const FVector Location = WallHolesFramePoint(Segment.WallHoles.ExtraTargetOffsets[Index]);
			AActor* Extra = GetWorld()->SpawnActor<AActor>(TargetClass, Location, FRotator(0.f, M_WallHoles.StageYaw + Yaw, 0.f), Params);
			if (IsValid(Extra))
			{
				M_WallHoles.Targets.Add(Extra);
				M_WallHoles.ExtraTargets.Add(Extra);
			}
		}
	}
	for (const TWeakObjectPtr<AActor>& Target : M_WallHoles.Targets)
	{
		TArray<UGeometryCollectionComponent*> Collections;
		if (Target.IsValid())
		{
			Target->GetComponents<UGeometryCollectionComponent>(Collections);
		}
		for (UGeometryCollectionComponent* Collection : Collections)
		{
			UE_LOG(LogRTSShortsWallHoles, Verbose, TEXT("RTS_SHORTS_WALL_TARGET_GC target=%s gc=%s rest=%s visible=%d"),
			       *GetNameSafe(Target.Get()), *Collection->GetName(), *GetNameSafe(Collection->GetRestCollection()),
			       Collection->IsVisible() ? 1 : 0);
		}
		const float Health = RTSShortsWallHolesPrivate::ReadTargetHealth(Target.Get());
		M_WallHoles.TargetStartHealth.Add(Health);
		M_WallHoles.TargetLastFraction.Add(1.f);
		M_WallHoles.TargetLargestDrop.Add(0.f);
		M_WallHoles.StartHealth += FMath::Max(0.f, Health);
	}
	M_WallHoles.Attackers.Add(MainUnit);
	M_WallHoles.PlacedLocations.Add(MainUnit->GetActorLocation());
	M_WallHoles.PlacedRotations.Add(MainUnit->GetActorRotation());
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FRotator UnitRotation(0.f, StageTransform.Rotator().Yaw + Segment.UnitYawOffset, 0.f);
#if RTS_WITH_MAP_TESTS
	for (int32 Index = 0; Index < Segment.BattleAllyClassPaths.Num(); ++Index)
	{
		// Line abreast beside the segment's unit: right, left, further right, ... unless offsets are given.
		FVector Offset = Segment.BattleAllyOffsets.IsValidIndex(Index) ? Segment.BattleAllyOffsets[Index] : FVector::ZeroVector;
		if (not Segment.BattleAllyOffsets.IsValidIndex(Index))
		{
			const float Side = Index % 2 == 0 ? 1.f : -1.f;
			Offset.Y = Side * static_cast<float>(Index / 2 + 1) * Segment.BattleSpacing;
		}
		const FVector Location = GetBattleGroundPoint(StageTransform.TransformPosition(Offset));
		AActor* Ally = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), *Segment.BattleAllyClassPaths[Index],
		                                                       FTransform(UnitRotation, Location),
		                                                       static_cast<uint8>(M_Job.UnitOwningPlayer));
		if (not IsValid(Ally))
		{
			UE_LOG(LogRTSShortsWallHoles, Warning, TEXT("RTS_SHORTS_WALL_ALLY_FAILED segment=%d class=%s"), SegmentIndex,
			       *Segment.BattleAllyClassPaths[Index]);
			continue;
		}
		ForceAnimationTicking(Ally);
		M_WallHoles.Attackers.Add(Ally);
		M_WallHoles.SpawnedAttackers.Add(Ally);
		M_WallHoles.PlacedLocations.Add(Location);
		M_WallHoles.PlacedRotations.Add(UnitRotation);
	}
	M_WallHoles.LaunchHandle = FProjectileMapTestHooks::OnShellLaunched().AddLambda(
		[this](const FMapTestShellLaunchReport& Report)
		{
			if (M_WallHoles.bIsActive && M_WallHoles.FireFrame >= 0 && M_WallHoles.CeaseFrame < 0
				&& M_WallHoles.MainGunOwners.Contains(Report.WeaponOwner) && M_Manifest.Segments.IsValidIndex(M_CurrentSegment))
			{
				++M_Manifest.Segments[M_CurrentSegment].WallShells;
			}
		});
#endif
	const int32 Attackers = M_WallHoles.Attackers.Num();
	const int32 Points = FMath::Max(1, Segment.WallHoles.AimPoints.Num());
	const int32 Groups = FMath::Clamp(Segment.WallHoles.AimGroups, 1, FMath::Max(1, FMath::Min(Attackers, Points)));
	// Rank the attackers by their side of the line (stage Y), left to right.
	TArray<int32> Order;
	for (int32 Index = 0; Index < Attackers; ++Index)
	{
		Order.Add(Index);
	}
	const FTransform StageFrame = M_Stage->GetActorTransform();
	Order.Sort([this, &StageFrame](const int32 A, const int32 B)
	{
		return StageFrame.InverseTransformPosition(M_WallHoles.PlacedLocations[A]).Y
			< StageFrame.InverseTransformPosition(M_WallHoles.PlacedLocations[B]).Y;
	});
	M_WallHoles.AimIndex.SetNumZeroed(Attackers);
	M_WallHoles.AimBlockStart.SetNumZeroed(Attackers);
	M_WallHoles.AimBlockEnd.SetNumZeroed(Attackers);
	for (int32 Rank = 0; Rank < Attackers; ++Rank)
	{
		const int32 Index = Order[Rank];
		const int32 Group = Rank * Groups / Attackers;
		const int32 BlockStart = Group * Points / Groups;
		const int32 BlockEnd = FMath::Max(BlockStart + 1, (Group + 1) * Points / Groups);
		// Members of a group start spread over its block; each then steps on by one inside it.
		const int32 GroupFirstRank = (Group * Attackers + Groups - 1) / Groups;
		const int32 GroupSize = FMath::Max(1, ((Group + 1) * Attackers + Groups - 1) / Groups - GroupFirstRank);
		M_WallHoles.AimBlockStart[Index] = BlockStart;
		M_WallHoles.AimBlockEnd[Index] = BlockEnd;
		M_WallHoles.AimIndex[Index] = (Rank - GroupFirstRank) * (BlockEnd - BlockStart) / GroupSize;
	}
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[SegmentIndex];
	ManifestSegment.WallAttackers = Attackers;
	ManifestSegment.WallHealthStart = M_WallHoles.StartHealth;
	ManifestSegment.WallHealthSampleFrames = FMath::Max(1, FMath::RoundToInt(M_Job.Fps / 10.f));
	UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_BEGIN segment=%d target=%s targets=%d health=%.0f attackers=%d point=%s"),
	       SegmentIndex, *GetNameSafe(M_SegmentTarget.Get()), M_WallHoles.Targets.Num(), M_WallHoles.StartHealth, Attackers,
	       *M_WallHoles.TargetPoint.ToCompactString());
	TickWallHoles(GetTimelineFrame());
}

UWeaponState* ARTSShortsDirector::BlockWallHolesSecondaries(AActor* Unit)
{
	UWeaponState* MainGun = nullptr;
	const TArray<UWeaponState*> Weapons = RTSShortsWallHolesPrivate::GetUnitWeapons(Unit);
	for (UWeaponState* Weapon : Weapons)
	{
		if (IsValid(Weapon) && (MainGun == nullptr || Weapon->GetRawWeaponData().WeaponCalibre > MainGun->GetRawWeaponData().WeaponCalibre))
		{
			MainGun = Weapon;
		}
	}
	for (UWeaponState* Weapon : Weapons)
	{
		if (IsValid(Weapon) && Weapon != MainGun && not M_WallHoles.RestrictedWeapons.Contains(Weapon))
		{
			SetWallHolesWeaponBlocked(Weapon, true);
		}
	}
	return MainGun;
}

void ARTSShortsDirector::SetWallHolesWeaponBlocked(UWeaponState* Weapon, const bool bBlocked)
{
	if (not IsValid(Weapon))
	{
		return;
	}
	Weapon->SetModuleFireRestriction(this, bBlocked);
	M_WallHoles.RestrictedWeapons.AddUnique(Weapon);
}

float ARTSShortsDirector::GetWallHolesTargetFraction(const int32 TargetIndex) const
{
	AActor* Target = M_WallHoles.Targets.IsValidIndex(TargetIndex) ? M_WallHoles.Targets[TargetIndex].Get() : nullptr;
	if (not IsValid(Target) || not Target->IsUnitAlive())
	{
		return 0.f;
	}
	const float Health = RTSShortsWallHolesPrivate::ReadTargetHealth(Target);
	const float Start = M_WallHoles.TargetStartHealth.IsValidIndex(TargetIndex) ? M_WallHoles.TargetStartHealth[TargetIndex] : 0.f;
	if (Health < 0.f || Start <= 0.f)
	{
		return 1.f;
	}
	return FMath::Clamp(Health / Start, 0.f, 1.f);
}

float ARTSShortsDirector::GetWallHolesHealthFraction() const
{
	if (M_WallHoles.Targets.IsEmpty())
	{
		return 0.f;
	}
	float Sum = 0.f;
	for (int32 Index = 0; Index < M_WallHoles.Targets.Num(); ++Index)
	{
		Sum += GetWallHolesTargetFraction(Index);
	}
	return Sum / M_WallHoles.Targets.Num();
}

void ARTSShortsDirector::RecordWallHolesDamage()
{
	if (not M_Manifest.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[M_CurrentSegment];
	ManifestSegment.WallHealthEnd = GetWallHolesHealthFraction();
	ManifestSegment.WallHoleCount = 0;
	ManifestSegment.WallHiddenCells = 0;
	for (const TWeakObjectPtr<AActor>& TargetPtr : M_WallHoles.Targets)
	{
		const AActor* Target = TargetPtr.Get();
		if (const UWallHoleDamageComponent* Holes = IsValid(Target) ? Target->FindComponentByClass<UWallHoleDamageComponent>() : nullptr)
		{
			ManifestSegment.WallHoleCount += Holes->GetHoleCount();
			ManifestSegment.WallHiddenCells += Holes->GetHiddenCellCount();
		}
		if (not IsValid(Target))
		{
			continue;
		}
		// Which mesh shows the target and whether it wears the masked hole material (trees: holes were logged but none
		// showed on camera, 2026-11-11).
		TArray<UPrimitiveComponent*> Primitives;
		Target->GetComponents<UPrimitiveComponent>(Primitives);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			const UMeshComponent* Mesh = Cast<UMeshComponent>(Primitive);
			UE_LOG(LogRTSShortsWallHoles, Verbose, TEXT("RTS_SHORTS_WALL_TARGET_PART target=%s part=%s class=%s visible=%d hiddenInGame=%d mat0=%s"),
			       *Target->GetName(), *Primitive->GetName(), *Primitive->GetClass()->GetName(), Primitive->IsVisible() ? 1 : 0,
			       Primitive->bHiddenInGame ? 1 : 0, Mesh && Mesh->GetNumMaterials() > 0 ? *GetNameSafe(Mesh->GetMaterial(0)) : TEXT("-"));
			if (const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Primitive))
			{
				// Holes as the shader sees them: custom primitive data 11.. = (mesh X, Y, Z, radius) x 6.
				const TArray<float>& Data = StaticMesh->GetCustomPrimitiveData().Data;
				FString Holes;
				for (int32 Index = 11; Index + 3 < Data.Num(); Index += 4)
				{
					if (Data[Index + 3] > 0.f)
					{
						Holes += FString::Printf(TEXT(" (%.0f,%.0f,%.0f r%.0f)"), Data[Index], Data[Index + 1], Data[Index + 2], Data[Index + 3]);
					}
				}
				const FBox Bounds = StaticMesh->GetStaticMesh() ? StaticMesh->GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
				FString WorldHoles;
				for (int32 Index = 11; Index + 3 < Data.Num(); Index += 4)
				{
					if (Data[Index + 3] > 0.f)
					{
						// World position and where it lands on the current frame (0..1), to see which side was holed.
						const FVector World = StaticMesh->GetComponentTransform().TransformPosition(FVector(Data[Index], Data[Index + 1], Data[Index + 2]));
						const FVector2D Screen = ProjectToFrame(World);
						WorldHoles += FString::Printf(TEXT(" (%s screen %.2f,%.2f)"), *World.ToCompactString(), Screen.X, Screen.Y);
					}
				}
				UE_LOG(LogRTSShortsWallHoles, Verbose, TEXT("RTS_SHORTS_WALL_TARGET_HOLES target=%s mesh=%s bounds=%s cpd=%d holes:%s world:%s"),
				       *Target->GetName(), *GetNameSafe(StaticMesh->GetStaticMesh()), *Bounds.ToString(), Data.Num(), *Holes, *WorldHoles);
			}
		}
	}
}

void ARTSShortsDirector::OrderWallHolesFire(const int32 SegmentIndex)
{
	const int32 Frame = GetTimelineFrame();
	M_WallHoles.FireFrame = Frame;
	M_WallHoles.LastRetargetFrame = Frame;
	M_Manifest.Segments[SegmentIndex].FireOrderFrame = Frame;
	M_Manifest.Segments[SegmentIndex].WallFireFrame = Frame;
	AimWallHolesAttackers(M_Job.Segments[SegmentIndex].WallHoles);
	UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_FIRE segment=%d frame=%d"), SegmentIndex, Frame);
}

void ARTSShortsDirector::AimWallHolesAttackers(const FRTSShortsWallHoles& Settings)
{
	for (int32 Index = 0; Index < M_WallHoles.Attackers.Num(); ++Index)
	{
		AActor* Attacker = M_WallHoles.Attackers[Index].Get();
		ICommands* Commands = Cast<ICommands>(Attacker);
		if (Commands == nullptr || not Attacker->IsUnitAlive())
		{
			continue;
		}
		const int32 BlockStart = M_WallHoles.AimBlockStart.IsValidIndex(Index) ? M_WallHoles.AimBlockStart[Index] : 0;
		const int32 BlockSize = M_WallHoles.AimBlockEnd.IsValidIndex(Index)
			? FMath::Max(1, M_WallHoles.AimBlockEnd[Index] - BlockStart)
			: FMath::Max(1, Settings.AimPoints.Num());
		const FVector Local = Settings.AimPoints.Num() > 0
			? Settings.AimPoints[FMath::Clamp(BlockStart + M_WallHoles.AimIndex[Index] % BlockSize, 0, Settings.AimPoints.Num() - 1)]
			: FVector(0.f, 0.f, 250.f);
		const FVector World = WallHolesFramePoint(Local);
		const ECommandQueueError Result = Commands->AttackGround(World, true);
		UE_LOG(LogRTSShortsWallHoles, Verbose, TEXT("RTS_SHORTS_WALL_AIM attacker=%d point=%d result=%d at=%s"), Index,
		       M_WallHoles.AimIndex[Index], static_cast<int32>(Result), *World.ToCompactString());
	}
}

void ARTSShortsDirector::CeaseWallHolesFire(const int32 Frame, const FString& Reason)
{
	const FRTSShortsWallHoles& Settings = M_Job.Segments[M_CurrentSegment].WallHoles;
	M_WallHoles.CeaseFrame = Frame;
	for (const TWeakObjectPtr<AActor>& AttackerPtr : M_WallHoles.Attackers)
	{
		AActor* Attacker = AttackerPtr.Get();
		if (ICommands* Commands = Cast<ICommands>(Attacker))
		{
			Commands->StopCommand();
		}
		// Blocked too: a stopped tank may still pick a target of its own.
		for (UWeaponState* Weapon : RTSShortsWallHolesPrivate::GetUnitWeapons(Attacker))
		{
			SetWallHolesWeaponBlocked(Weapon, true);
		}
	}
	for (const TWeakObjectPtr<AActor>& TargetPtr : M_WallHoles.Targets)
	{
		AActor* Target = TargetPtr.Get();
		if (Settings.bAllowKill || not IsValid(Target))
		{
			continue;
		}
		// Shells still in flight must not change the result being shown, nor kill the target under it.
		if (FFloatProperty* Reduction = RTSShortsWallHolesPrivate::FindFloatProperty(Target, TEXT("DamageReduction")))
		{
			Reduction->SetPropertyValue_InContainer(Target, RTSShortsWallHolesPrivate::ProtectedDamageReduction);
		}
		Target->SetCanBeDamaged(false);
	}
	RecordWallHolesDamage();
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[M_CurrentSegment];
	ManifestSegment.WallCeaseFrame = Frame;
	ManifestSegment.WallCeaseReason = Reason;
	ManifestSegment.StopOrderFrame = Frame;
	bM_StopOrdered = true;
	UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_CEASE segment=%d frame=%d reason=%s health=%.3f holes=%d cells=%d shells=%d"),
	       M_CurrentSegment, Frame, *Reason, ManifestSegment.WallHealthEnd, ManifestSegment.WallHoleCount,
	       ManifestSegment.WallHiddenCells, ManifestSegment.WallShells);
	WriteManifest();
}

void ARTSShortsDirector::ApplyWallHolesShot(const int32 Index, const int32 Frame)
{
	const FRTSShortsWallHoles& Settings = M_Job.Segments[M_CurrentSegment].WallHoles;
	if (not Settings.Shots.IsValidIndex(Index) || not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsWallHolesShot& Shot = Settings.Shots[Index];
	const float Alpha = Shot.MoveSeconds > 0.f
		? RTSShortsWallHolesPrivate::SmoothStep(static_cast<float>(Frame - M_WallHoles.ActiveShotFrame) / (Shot.MoveSeconds * M_Job.Fps))
		: 0.f;
	const FVector Offset = Shot.MoveSeconds > 0.f ? FMath::Lerp(Shot.Offset, Shot.EndOffset, Alpha) : Shot.Offset;
	const FVector LookOffset = Shot.MoveSeconds > 0.f ? FMath::Lerp(Shot.LookOffset, Shot.EndLookOffset, Alpha) : Shot.LookOffset;
	const float Focal = Shot.MoveSeconds > 0.f && Shot.EndFocalLength > 0.f
		? FMath::Lerp(Shot.FocalLength, Shot.EndFocalLength, Alpha)
		: Shot.FocalLength;
	const FVector CameraLocation = WallHolesFramePoint(Offset);
	FRotator Rotation = (WallHolesFramePoint(LookOffset) - CameraLocation).Rotation();
	Rotation.Roll = 0.f;
	M_Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
	if (UCineCameraComponent* Cine = M_Camera->GetCineCameraComponent())
	{
		Cine->SetCurrentFocalLength(Focal);
	}
	if (Frame == M_WallHoles.ActiveShotFrame)
	{
		ApplyCameraCut(CameraLocation);
		UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_SHOT segment=%d shot=%d frame=%d camera=%s focal=%.0f"),
		       M_CurrentSegment, Index, Frame, *CameraLocation.ToCompactString(), Focal);
	}
}

void ARTSShortsDirector::TickWallHoles(const int32 Frame)
{
	using namespace RTSShortsWallHolesPrivate;
	if (not M_WallHoles.bIsActive || M_WallHoles.bEnded || not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const FRTSShortsWallHoles& Settings = M_Job.Segments[M_CurrentSegment].WallHoles;
	FRTSShortsManifestSegment& ManifestSegment = M_Manifest.Segments[M_CurrentSegment];
	for (int32 Index = 0; Index < M_WallHoles.Attackers.Num(); ++Index)
	{
		AActor* Attacker = M_WallHoles.Attackers[Index].Get();
		if (not IsValid(Attacker))
		{
			continue;
		}
		if (Settings.bMainGunOnly || M_WallHoles.CeaseFrame >= 0)
		{
			// The turrets may come up a few ticks after the spawn; repeated until every weapon is known.
			if (UWeaponState* MainGun = BlockWallHolesSecondaries(Attacker))
			{
				M_WallHoles.MainGunOwners.Add(MainGun->GetWeaponOwnerObject());
				ManifestSegment.WallAttackerCalibreMm = FMath::Max(ManifestSegment.WallAttackerCalibreMm,
				                                                   FMath::RoundToInt(MainGun->GetRawWeaponData().WeaponCalibre));
			}
		}
		if (Index > 0)
		{
			// The segment's own unit is held by the director's drift lock; the copies are held here.
			SuppressUnitDecals(Attacker, M_CurrentSegment);
			if (FVector::Dist2D(Attacker->GetActorLocation(), M_WallHoles.PlacedLocations[Index]) > MaxDriftCm)
			{
				Attacker->SetActorLocationAndRotation(M_WallHoles.PlacedLocations[Index], M_WallHoles.PlacedRotations[Index],
				                                      false, nullptr, ETeleportType::TeleportPhysics);
				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Attacker->GetRootComponent()))
				{
					Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				}
			}
		}
	}
	const float Fps = static_cast<float>(M_Job.Fps);
	const float Health = GetWallHolesHealthFraction();
	if (Health < M_WallHoles.LastHealthFraction)
	{
		M_WallHoles.LastDamageFrame = Frame;
	}
	M_WallHoles.LastHealthFraction = Health;
	// The Tiger II's third 105 mm shell took the wall from 33 % (above the 30 % stop) to dead: stop once the next shell
	// like the biggest one so far would kill a target. Every target is spared on its own (two trees: the first one to
	// get low stops the volley).
	constexpr float NextShellMargin = 1.15f;
	bool bNextShellKills = false;
	bool bAnyTargetLow = false;
	bool bAllTargetsDown = M_WallHoles.Targets.Num() > 0;
	for (int32 Index = 0; Index < M_WallHoles.Targets.Num(); ++Index)
	{
		const float Fraction = GetWallHolesTargetFraction(Index);
		if (Fraction > 0.f)
		{
			bAllTargetsDown = false;
			M_WallHoles.TargetLargestDrop[Index] = FMath::Max(M_WallHoles.TargetLargestDrop[Index], M_WallHoles.TargetLastFraction[Index] - Fraction);
			bNextShellKills |= M_WallHoles.TargetLargestDrop[Index] > 0.f && Fraction <= M_WallHoles.TargetLargestDrop[Index] * NextShellMargin;
			bAnyTargetLow |= Fraction <= Settings.StopAtHealthFraction;
		}
		M_WallHoles.TargetLastFraction[Index] = Fraction;
	}
	if (M_WallHoles.FireFrame >= 0)
	{
		const int32 SinceFire = Frame - M_WallHoles.FireFrame;
		if (SinceFire >= 0 && SinceFire % ManifestSegment.WallHealthSampleFrames == 0)
		{
			ManifestSegment.WallHealthSamples.Add(Health);
		}
		if (M_WallHoles.DeathFrame < 0 && bAllTargetsDown)
		{
			M_WallHoles.DeathFrame = Frame;
			ManifestSegment.WallDeathFrame = Frame;
			UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_DEATH segment=%d frame=%d"), M_CurrentSegment, Frame);
		}
		if (M_WallHoles.CeaseFrame < 0)
		{
			if (M_WallHoles.DeathFrame >= 0)
			{
				CeaseWallHolesFire(Frame, TEXT("killed"));
			}
			else if (not Settings.bAllowKill && (bAnyTargetLow || bNextShellKills))
			{
				CeaseWallHolesFire(Frame, TEXT("health"));
			}
			else if (SinceFire >= FMath::RoundToInt(Settings.FireSeconds * Fps))
			{
				CeaseWallHolesFire(Frame, TEXT("time"));
			}
			else if (Settings.CeaseAfterIdleSeconds > 0.f && M_WallHoles.LastDamageFrame >= 0
				&& SinceFire >= FMath::RoundToInt(Settings.MinFireSeconds * Fps)
				&& Frame - M_WallHoles.LastDamageFrame >= FMath::RoundToInt(Settings.CeaseAfterIdleSeconds * Fps))
			{
				CeaseWallHolesFire(Frame, TEXT("idle"));
			}
			else if (Settings.RetargetSeconds > 0.f && Frame - M_WallHoles.LastRetargetFrame >= FMath::RoundToInt(Settings.RetargetSeconds * Fps))
			{
				M_WallHoles.LastRetargetFrame = Frame;
				for (int32& Aim : M_WallHoles.AimIndex)
				{
					++Aim;
				}
				AimWallHolesAttackers(Settings);
			}
		}
	}
	// Camera: start every shot whose anchor event happened and whose time came, in list order.
	while (Settings.Shots.IsValidIndex(M_WallHoles.NextShot))
	{
		const FRTSShortsWallHolesShot& Shot = Settings.Shots[M_WallHoles.NextShot];
		const int32 AnchorFrame = Shot.Anchor == TEXT("Fire") ? M_WallHoles.FireFrame
			: Shot.Anchor == TEXT("Cease") ? M_WallHoles.CeaseFrame
			: M_WallHoles.BeginFrame;
		const int32 DueFrame = AnchorFrame + FMath::RoundToInt(Shot.AtSeconds * Fps);
		if (Shot.Anchor != TEXT("Cease") && M_WallHoles.CeaseFrame >= 0 && (AnchorFrame < 0 || DueFrame > M_WallHoles.CeaseFrame))
		{
			// A short round (the target was down to its stop health after one volley): the fire-phase shots that did not
			// come yet are dropped, so the result shot is not held back behind them.
			++M_WallHoles.NextShot;
			continue;
		}
		if (AnchorFrame < 0 || Frame < DueFrame)
		{
			break;
		}
		M_WallHoles.ActiveShot = M_WallHoles.NextShot;
		M_WallHoles.ActiveShotFrame = Frame;
		ManifestSegment.WallShotFrames.Add(Frame);
		++M_WallHoles.NextShot;
	}
	if (M_WallHoles.ActiveShot >= 0)
	{
		ApplyWallHolesShot(M_WallHoles.ActiveShot, Frame);
	}
	if (M_WallHoles.CeaseFrame < 0)
	{
		return;
	}
	int32 EndFrame = M_WallHoles.CeaseFrame + FMath::RoundToInt(Settings.ResultHoldSeconds * Fps);
	if (M_WallHoles.DeathFrame >= 0)
	{
		EndFrame = FMath::Max(EndFrame, M_WallHoles.DeathFrame + FMath::RoundToInt(Settings.DeathHoldSeconds * Fps));
	}
	if (Frame < EndFrame)
	{
		return;
	}
	M_WallHoles.bEnded = true;
	ManifestSegment.WallHealthEnd = Health;
	UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_ROUND_END segment=%d frame=%d health=%.3f holes=%d cells=%d shells=%d"),
	       M_CurrentSegment, Frame, Health, ManifestSegment.WallHoleCount, ManifestSegment.WallHiddenCells, ManifestSegment.WallShells);
	ShortenRunningSegment(Frame + 1);
	WriteManifest();
}

void ARTSShortsDirector::EndWallHoles()
{
	if (not M_WallHoles.bIsActive)
	{
		return;
	}
#if RTS_WITH_MAP_TESTS
	FProjectileMapTestHooks::OnShellLaunched().Remove(M_WallHoles.LaunchHandle);
#endif
	for (const TWeakObjectPtr<UWeaponState>& Weapon : M_WallHoles.RestrictedWeapons)
	{
		if (UWeaponState* State = Weapon.Get())
		{
			State->SetModuleFireRestriction(this, false);
		}
	}
	for (const TWeakObjectPtr<AActor>& Extra : M_WallHoles.ExtraTargets)
	{
		// M_SegmentTarget goes with EndSegment; the extra targets go here.
		if (AActor* Target = Extra.Get(); IsValid(Target))
		{
			Target->Destroy();
		}
	}
	for (const TWeakObjectPtr<AActor>& Spawned : M_WallHoles.SpawnedAttackers)
	{
		if (AActor* Attacker = Spawned.Get(); IsValid(Attacker))
		{
			Attacker->Destroy();
		}
	}
	UE_LOG(LogRTSShortsWallHoles, Display, TEXT("RTS_SHORTS_WALL_END segment=%d"), M_CurrentSegment);
	M_WallHoles = FRTSShortsWallHolesState();
}
