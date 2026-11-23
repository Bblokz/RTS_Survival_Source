// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

// Size stacks and rate-of-fire stacks read their numbers from the render log instead of guessing them:
// - RTS_SHORTS_HULL: the placed unit's own meshes (hull, tracks) and, with the turrets and guns attached to it, the
//   with-gun length, both measured along the unit's forward axis in the unit's local space (actor bounds include
//   the selection decal and read 7.68 m for every armoured car).
// - RTS_SHORTS_MAINGUN: the unit's largest-calibre weapons (every barrel of a multi-gun mount counts) with their
//   magazine, cooldown and reload; RTS_SHORTS_ROUND for every main-gun round (a magazine drop) and RTS_SHORTS_RELOAD
//   when a reload starts or ends. The post step groups rounds into shots (a burst is one shot).

#include "RTSShortsDirector.h"

#include "Components/MeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsMainGun, Log, All);

namespace RTSShortsMainGunPrivate
{
	/** Gun parts never count toward the hull: casemate guns and embedded turrets are components of the unit itself. */
	bool GetIsGunPart(const UMeshComponent* Mesh)
	{
		const FString Name = Mesh->GetName();
		return Name.Contains(TEXT("Turret")) || Name.Contains(TEXT("Gun")) || Name.Contains(TEXT("Barrel"))
			|| Name.Contains(TEXT("Mantlet"));
	}

	/** Visible mesh components of Actor, in Unit's local space, one RTS_SHORTS_HULL_PART line each. Parts longer than
	 * MaxPartCm (beams, tracers, debris planes) are left out. */
	bool AddMeshBounds(const int32 SegmentIndex, const AActor* Actor, const FTransform& UnitTransform, FBox& InOutBox,
	                   const bool bHullOnly)
	{
		constexpr double MaxPartCm = 2000.0;
		bool bAdded = false;
		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		for (const UMeshComponent* Mesh : Meshes)
		{
			if (not IsValid(Mesh) || not Mesh->IsVisible() || Mesh->bHiddenInGame)
			{
				continue;
			}
			const FTransform ToUnit = Mesh->GetComponentTransform().GetRelativeTransform(UnitTransform);
			// Skinned meshes report physics-asset bounds (a King Tiger turret read 2.4 m, a T-34 AA turret 14 m): use the
			// mesh asset's own bounds (reference pose) placed by the component's transform.
			FBox Box = Mesh->CalcBounds(ToUnit).GetBox();
			if (const USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Mesh))
			{
				if (const USkinnedAsset* Asset = Skinned->GetSkinnedAsset())
				{
					Box = Asset->GetBounds().GetBox().TransformBy(ToUnit);
				}
			}
			const bool bGunPart = GetIsGunPart(Mesh);
			const bool bTooLong = Box.GetSize().X > MaxPartCm;
			UE_LOG(LogRTSShortsMainGun, Display, TEXT("RTS_SHORTS_HULL_PART segment=%d hullPass=%d actor=%s comp=%s class=%s minX=%.0f maxX=%.0f gun=%d skipped=%d"),
			       SegmentIndex, bHullOnly ? 1 : 0, *Actor->GetName(), *Mesh->GetName(), *Mesh->GetClass()->GetName(),
			       Box.Min.X, Box.Max.X, bGunPart ? 1 : 0, bTooLong ? 1 : 0);
			if (Box.GetSize().IsNearlyZero() || bTooLong || (bHullOnly && bGunPart))
			{
				continue;
			}
			InOutBox += Box;
			bAdded = true;
		}
		return bAdded;
	}

	/** One projectile (or trace) per fire socket for every magazine round on the multi-barrel weapon classes. */
	int32 GetProjectilesPerRound(const UWeaponState* Weapon)
	{
		if (const UWeaponStateMultiProjectile* Multi = Cast<UWeaponStateMultiProjectile>(Weapon))
		{
			return FMath::Max(1, Multi->GetProjectileCount());
		}
		if (const UWeaponStateMultiTrace* MultiTrace = Cast<UWeaponStateMultiTrace>(Weapon))
		{
			return FMath::Max(1, MultiTrace->GetTraceCount());
		}
		return 1;
	}

	float CalibreOf(const UWeaponState* Weapon)
	{
		return IsValid(Weapon) ? Weapon->GetRawWeaponData().WeaponCalibre : 0.f;
	}
}

void ARTSShortsDirector::LogUnitHullSize(const int32 SegmentIndex, const AActor* Unit) const
{
	using namespace RTSShortsMainGunPrivate;
	if (not IsValid(Unit))
	{
		return;
	}
	const FTransform UnitTransform(Unit->GetActorRotation(), Unit->GetActorLocation());
	FBox Hull(ForceInit);
	AddMeshBounds(SegmentIndex, Unit, UnitTransform, Hull, true);
	FBox WithGun(ForceInit);
	AddMeshBounds(SegmentIndex, Unit, UnitTransform, WithGun, false);
	// Turrets: attached actors (child actor components) and the turrets the tank registered.
	TArray<AActor*> Attached;
	Unit->GetAttachedActors(Attached, true, true);
	for (ACPPTurretsMaster* Turret : GetUnitTurrets(Unit))
	{
		Attached.AddUnique(Turret);
	}
	for (const AActor* Child : Attached)
	{
		if (IsValid(Child) && not Child->IsHidden())
		{
			AddMeshBounds(SegmentIndex, Child, UnitTransform, WithGun, false);
		}
	}
	if (not Hull.IsValid)
	{
		UE_LOG(LogRTSShortsMainGun, Warning, TEXT("RTS_SHORTS_HULL segment=%d unit=%s no meshes"), SegmentIndex, *Unit->GetName());
		return;
	}
	const FVector Size = Hull.GetSize();
	UE_LOG(LogRTSShortsMainGun, Display,
	       TEXT("RTS_SHORTS_HULL segment=%d unit=%s len=%.0f wid=%.0f hgt=%.0f minX=%.0f maxX=%.0f minZ=%.0f gunLen=%.0f gunMinX=%.0f gunMaxX=%.0f gunMaxZ=%.0f"),
	       SegmentIndex, *Unit->GetName(), Size.X, Size.Y, Size.Z, Hull.Min.X, Hull.Max.X, Hull.Min.Z,
	       WithGun.GetSize().X, WithGun.Min.X, WithGun.Max.X, WithGun.Max.Z);
}

void ARTSShortsDirector::BeginMainGunLog(const int32 SegmentIndex, AActor* Unit)
{
	using namespace RTSShortsMainGunPrivate;
	EndMainGunLog();
	LogUnitHullSize(SegmentIndex, Unit);
	const ATankMaster* Tank = Cast<ATankMaster>(Unit);
	if (not IsValid(Tank))
	{
		return;
	}
	const TArray<UWeaponState*> Weapons = FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
	float Largest = 0.f;
	for (const UWeaponState* Weapon : Weapons)
	{
		Largest = FMath::Max(Largest, CalibreOf(Weapon));
	}
	if (Largest <= 0.f)
	{
		return;
	}
	M_MainGunLog.Segment = SegmentIndex;
	const bool bMainGunOnly = M_Job.Segments.IsValidIndex(SegmentIndex) && M_Job.Segments[SegmentIndex].bMainGunOnly;
	for (UWeaponState* Weapon : Weapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		if (FMath::IsNearlyEqual(CalibreOf(Weapon), Largest))
		{
			M_MainGunLog.Weapons.Add(Weapon);
			M_MainGunLog.LastMag.Add(Weapon->GetCurrentMagCapacity());
			const int32 PerRound = GetProjectilesPerRound(Weapon);
			M_MainGunLog.ProjectilesPerRound.Add(PerRound);
			M_MainGunLog.bWasReloading.Add(Weapon->GetIsReloading());
			const FWeaponData& Data = Weapon->GetRawWeaponData();
			UE_LOG(LogRTSShortsMainGun, Display,
			       TEXT("RTS_SHORTS_MAINGUN segment=%d weapon=%d calibre=%.0f mag=%d cooldown=%.3f reload=%.2f flux=%d projectiles=%d class=%s"),
			       SegmentIndex, M_MainGunLog.Weapons.Num() - 1, Data.WeaponCalibre, Data.MagCapacity, Data.BaseCooldown,
			       Data.ReloadSpeed, Data.CooldownFlux, PerRound, *Weapon->GetClass()->GetName());
		}
		else if (bMainGunOnly)
		{
			// MG sparks and noise would blur the one number the viewer counts.
			Weapon->SetModuleFireRestriction(this, true);
			M_MainGunLog.Blocked.Add(Weapon);
		}
	}
	UE_LOG(LogRTSShortsMainGun, Display, TEXT("RTS_SHORTS_MAINGUN_SET segment=%d weapons=%d blocked=%d"), SegmentIndex,
	       M_MainGunLog.Weapons.Num(), M_MainGunLog.Blocked.Num());
}

void ARTSShortsDirector::TickMainGunLog(const int32 Frame)
{
	if (M_MainGunLog.Segment < 0 || M_MainGunLog.Segment != M_CurrentSegment || not M_SegmentStartFrames.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const int32 Local = Frame - M_SegmentStartFrames[M_CurrentSegment];
	for (int32 Index = 0; Index < M_MainGunLog.Weapons.Num(); ++Index)
	{
		const UWeaponState* Weapon = M_MainGunLog.Weapons[Index].Get();
		if (not IsValid(Weapon))
		{
			continue;
		}
		const int32 Mag = Weapon->GetCurrentMagCapacity();
		const bool bReloading = Weapon->GetIsReloading();
		// A reload that ended since the last tick refilled the magazine; a gun that fired in that same tick (a
		// one-round Tiger) never showed the full magazine, and that round went uncounted (RateOfFire01, 2026-11-12).
		const bool bReloadEnded = M_MainGunLog.bWasReloading[Index] && not bReloading;
		const int32 Before = bReloadEnded
			                     ? FMath::Max(M_MainGunLog.LastMag[Index], Weapon->GetRawWeaponData().MagCapacity)
			                     : M_MainGunLog.LastMag[Index];
		const int32 Dropped = Before - Mag;
		// A burst weapon with fewer rounds left than a burst empties its magazine and starts reloading in the same
		// tick without firing (UWeaponState::OnCooldownShutDown): those rounds were never fired.
		const bool bDiscarded = Dropped > 0 && Mag == 0 && bReloading && not M_MainGunLog.bWasReloading[Index];
		if (Dropped > 0 && not bDiscarded)
		{
			M_MainGunLog.Rounds += Dropped;
			UE_LOG(LogRTSShortsMainGun, Display, TEXT("RTS_SHORTS_ROUND segment=%d frame=%d local=%d weapon=%d rounds=%d mag=%d total=%d projectiles=%d"),
			       M_CurrentSegment, Frame, Local, Index, Dropped, Mag, M_MainGunLog.Rounds,
			       Dropped * M_MainGunLog.ProjectilesPerRound[Index]);
		}
		else if (bDiscarded)
		{
			UE_LOG(LogRTSShortsMainGun, Display, TEXT("RTS_SHORTS_ROUND_DISCARDED segment=%d frame=%d weapon=%d rounds=%d"),
			       M_CurrentSegment, Frame, Index, Dropped);
		}
		if (bReloading != M_MainGunLog.bWasReloading[Index])
		{
			UE_LOG(LogRTSShortsMainGun, Display, TEXT("RTS_SHORTS_RELOAD segment=%d frame=%d local=%d weapon=%d state=%d"),
			       M_CurrentSegment, Frame, Local, Index, bReloading ? 1 : 0);
		}
		M_MainGunLog.LastMag[Index] = Mag;
		M_MainGunLog.bWasReloading[Index] = bReloading;
	}
}

void ARTSShortsDirector::EndMainGunLog()
{
	for (const TWeakObjectPtr<UWeaponState>& Weapon : M_MainGunLog.Blocked)
	{
		if (Weapon.IsValid())
		{
			Weapon->SetModuleFireRestriction(this, false);
		}
	}
	if (M_MainGunLog.Segment >= 0)
	{
		UE_LOG(LogRTSShortsMainGun, Display, TEXT("RTS_SHORTS_MAINGUN_END segment=%d rounds=%d"), M_MainGunLog.Segment, M_MainGunLog.Rounds);
	}
	M_MainGunLog = FMainGunLogState();
}
