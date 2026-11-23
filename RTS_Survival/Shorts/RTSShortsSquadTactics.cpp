// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "NavigationPath.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitPlannedPosition.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsSquadTactics, Log, All);

namespace RTSShortsSquadTacticsPrivate
{
	constexpr int32 TickEveryFrames = 6;
	// The game's resource outlines write stencils 1, 3 and 5 (FRTSOutlineHelpers); soldiers get their own.
	constexpr int32 SoldierStencil = 8;
	constexpr int32 UnusedStencil = 250;
	const TCHAR* const OutlinerMaterialPath = TEXT("/Game/RTS_Survival/Blueprints/OutLineSystem/Materials/M_PP_Outliner_Master.M_PP_Outliner_Master");
	// Cover points round a trunk: the cover finder merges published points closer than its 90 cm spacing.
	constexpr int32 TrunkCoverPointCount = 6;
	constexpr float TrunkCoverClearanceCm = 60.f;
	constexpr float MinimumTrunkCoverRadiusCm = 95.f;
	constexpr float DefaultTrunkRadiusCm = 15.f;
	// A trunk point shields a soldier from the unit when it faces at least this much away from it.
	constexpr float ShieldingDot = 0.25f;
	// Same sideways step and height the cover finder tests a standing point's firing lane from.
	constexpr float PeekOffsetCm = 84.f;
	constexpr float FiringHeightCm = 150.f;
	constexpr float TargetAimHeightCm = 100.f;
	constexpr float ArrivedCm = 350.f;
	// The move order needs a moment before the squad reports it as its active command.
	constexpr float MoveStartGraceSeconds = 0.5f;
	// Soldiers that find no trunk point stand beside the tree this far apart.
	constexpr float SideSlotSpacingCm = 130.f;
	constexpr float RecheckSeconds = 2.f;
	// Open flank spots tried when the first is off the infantry's navigation: this many, each this much nearer the squad.
	constexpr int32 FlankFallbackSteps = 4;
	constexpr float FlankFallbackStepDegrees = 30.f;
	// A squad still out of range by this fraction of its weapon's reach moves again at once.
	constexpr float OutOfRangeFraction = 0.98f;
	// Tree score: working round the unit's side counts most, staying on its own side next, walking far counts against.
	constexpr float SameSideScore = 0.35f;
	constexpr float TravelPenaltyPerCm = 1.f / 4000.f;

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}

	TArray<ASquadUnit*> GetLivingSoldiers(ASquadController& Squad)
	{
		TArray<ASquadUnit*> Soldiers;
		for (ASquadUnit* Soldier : Squad.GetSquadUnitsChecked())
		{
			if (IsValid(Soldier) && Soldier->IsUnitAlive())
			{
				Soldiers.Add(Soldier);
			}
		}
		return Soldiers;
	}

	FVector GetCentroid(const TArray<ASquadUnit*>& Soldiers)
	{
		FVector Sum = FVector::ZeroVector;
		for (const ASquadUnit* Soldier : Soldiers)
		{
			Sum += Soldier->GetActorLocation();
		}
		return Soldiers.Num() > 0 ? Sum / Soldiers.Num() : FVector::ZeroVector;
	}

	float GetWeaponRange(const TArray<ASquadUnit*>& Soldiers)
	{
		float Range = 0.f;
		for (const ASquadUnit* Soldier : Soldiers)
		{
			if (const AInfantryWeaponMaster* Weapon = Soldier->GetInfantryWeapon())
			{
				Range = FMath::Max(Range, Weapon->GetMaxWeaponRange());
			}
		}
		return Range;
	}

	/** +1 when Location is on the unit's right, -1 on its left. */
	float GetSide(const AActor& Unit, const FVector& Location)
	{
		const FVector Forward = Unit.GetActorForwardVector().GetSafeNormal2D();
		return FVector::CrossProduct(Forward, (Location - Unit.GetActorLocation()).GetSafeNormal2D()).Z >= 0.f ? 1.f : -1.f;
	}

	float GetDegreesOffNose(const AActor& Unit, const FVector& Location)
	{
		const FVector Forward = Unit.GetActorForwardVector().GetSafeNormal2D();
		const FVector ToLocation = (Location - Unit.GetActorLocation()).GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToLocation), -1.f, 1.f)));
	}

	/** The trunk's collision capsule: its centre on the ground and its radius. Orchard trees collide with nothing else. */
	void GetTrunk(const AActor& Tree, FVector& OutCentre, float& OutRadius)
	{
		OutCentre = Tree.GetActorLocation();
		OutRadius = 0.f;
		TArray<UStaticMeshComponent*> Meshes;
		Tree.GetComponents(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			const UBodySetup* Body = IsValid(Mesh) ? Mesh->GetBodySetup() : nullptr;
			if (Body == nullptr)
			{
				continue;
			}
			const FTransform MeshTransform = Mesh->GetComponentTransform();
			const float ScaleXY = FMath::Max(MeshTransform.GetScale3D().X, MeshTransform.GetScale3D().Y);
			for (const FKSphylElem& Capsule : Body->AggGeom.SphylElems)
			{
				if (Capsule.Radius * ScaleXY > OutRadius)
				{
					OutRadius = Capsule.Radius * ScaleXY;
					const FVector Centre = MeshTransform.TransformPosition(Capsule.Center);
					OutCentre = FVector(Centre.X, Centre.Y, Tree.GetActorLocation().Z);
				}
			}
		}
		if (OutRadius <= 0.f)
		{
			OutRadius = DefaultTrunkRadiusCm * Tree.GetActorScale3D().X;
		}
	}

	/** Where a soldier on this standing point fires from: stepped out to the side its peek goes, as the cover finder tests it. */
	FVector GetFiringLaneStart(const FRTSCoverPoint& Point)
	{
		const FVector Facing = -Point.CoverNormal.GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing).GetSafeNormal();
		const FVector Side = Point.CoverType == ERTSCoverType::StandingLeft ? -Right : Right;
		return Point.Location + Side * PeekOffsetCm + FVector::UpVector * FiringHeightCm;
	}
}

AActor* ARTSShortsDirector::GetSquadTacticsTarget() const
{
	AActor* Target = M_Battle.Evasion.bIsActive && not M_Battle.Evasion.Evaders.IsEmpty() ? M_Battle.Evasion.Evaders[0].Unit.Get()
		                 : (M_Units.IsValidIndex(M_CurrentSegment) ? M_Units[M_CurrentSegment].Get() : nullptr);
	return IsValid(Target) && Target->IsUnitAlive() ? Target : nullptr;
}

void ARTSShortsDirector::SpawnBattleCoverTrees()
{
	using namespace RTSShortsSquadTacticsPrivate;
	if (bM_CoverTreesSpawned)
	{
		return;
	}
	bM_CoverTreesSpawned = true;
	URTSCoverFinderWorldSubsystem* Cover = GetWorld()->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const FTransform StageTransform = M_Stage->GetActorTransform();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FRTSShortsCoverTree& TreeSpec : M_Job.BattleCoverTrees)
	{
		UClass* TreeClass = LoadClass<AActor>(nullptr, *TreeSpec.ClassPath);
		if (TreeClass == nullptr)
		{
			UE_LOG(LogRTSShortsSquadTactics, Warning, TEXT("RTS_SHORTS_COVER_TREE_CLASS_MISSING class=%s"), *TreeSpec.ClassPath);
			continue;
		}
		// GetBattleGroundPoint lifts spawns 80 cm clear of the ground: a tree stands on it.
		constexpr float SpawnLiftCm = 80.f;
		const FVector Ground = GetBattleGroundPoint(StageTransform.TransformPosition(TreeSpec.Offset)) - FVector(0.f, 0.f, SpawnLiftCm);
		const FTransform TreeTransform(FRotator(0.f, StageTransform.Rotator().Yaw + TreeSpec.Yaw, 0.f), Ground,
		                               FVector(TreeSpec.Scale));
		AActor* Tree = GetWorld()->SpawnActor<AActor>(TreeClass, TreeTransform, SpawnParameters);
		if (not IsValid(Tree))
		{
			continue;
		}
		FRTSShortsCoverTreeState& State = M_CoverTrees.AddDefaulted_GetRef();
		State.Tree = Tree;
		float TrunkRadius = 0.f;
		GetTrunk(*Tree, State.TrunkLocation, TrunkRadius);
		const float RingRadius = FMath::Max(TrunkRadius + TrunkCoverClearanceCm, MinimumTrunkCoverRadiusCm);
		TArray<FRTSCoverPoint> Points;
		for (int32 Index = 0; Index < TrunkCoverPointCount; ++Index)
		{
			const FVector Normal = FVector::ForwardVector.RotateAngleAxis(360.f * Index / TrunkCoverPointCount, FVector::UpVector);
			FVector Location = GetBattleGroundPoint(State.TrunkLocation + Normal * RingRadius) - FVector(0.f, 0.f, SpawnLiftCm);
			FNavLocation OnNavigation;
			if (Navigation != nullptr && not Navigation->ProjectPointToNavigation(Location, OnNavigation, FVector(60.f, 60.f, 250.f)))
			{
				continue;
			}
			FRTSCoverPoint& Point = Points.AddDefaulted_GetRef();
			Point.Location = Navigation != nullptr ? OnNavigation.Location : Location;
			Point.CoverNormal = Normal;
			// Neighbours peek opposite ways, so the side facing away from a threat has both.
			Point.CoverType = Index % 2 == 0 ? ERTSCoverType::StandingLeft : ERTSCoverType::StandingRight;
		}
		const int32 Authored = Points.Num();
		State.RegistrationId = Cover != nullptr ? Cover->RegisterAuthoredCoverProvider(Tree, MoveTemp(Points)) : 0;
		int32 Published = 0;
		if (Cover != nullptr && State.RegistrationId != 0)
		{
			for (const FRTSCoverPoint& Point : Cover->FindCoverPointsInRadius(State.TrunkLocation, RingRadius + 150.f))
			{
				Published += Point.ProviderRegistrationId == State.RegistrationId ? 1 : 0;
			}
		}
		UE_LOG(LogRTSShortsSquadTactics, Display, TEXT("RTS_SHORTS_COVER_TREE tree=%s at=%s trunk=%.0f authored=%d published=%d"),
		       *Tree->GetName(), *State.TrunkLocation.ToCompactString(), TrunkRadius, Authored, Published);
	}
}

void ARTSShortsDirector::DropFallenCoverTrees()
{
	URTSCoverFinderWorldSubsystem* Cover = GetWorld()->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	for (FRTSShortsCoverTreeState& State : M_CoverTrees)
	{
		// A medium or heavy tank topples an orchard tree; it lies there for a few seconds and then despawns.
		constexpr float SunkCm = 50.f;
		const bool bStanding = State.Tree.IsValid() && State.Tree->GetActorUpVector().Z > 0.95f
			&& FMath::Abs(State.Tree->GetActorLocation().Z - State.TrunkLocation.Z) < SunkCm;
		if (State.RegistrationId == 0 || bStanding)
		{
			continue;
		}
		if (Cover != nullptr)
		{
			Cover->UnregisterAuthoredCoverProvider(State.RegistrationId);
		}
		State.RegistrationId = 0;
		UE_LOG(LogRTSShortsSquadTactics, Display, TEXT("RTS_SHORTS_COVER_TREE_FALLEN at=%s"), *State.TrunkLocation.ToCompactString());
	}
}

void ARTSShortsDirector::TickSoldierOutlines(const FRTSShortsSquadTactics& Settings)
{
	using namespace RTSShortsSquadTacticsPrivate;
	if (not Settings.bOutlineSoldiers)
	{
		return;
	}
	if (not IsValid(M_SoldierOutlineVolume))
	{
		UMaterialInterface* Outliner = LoadObject<UMaterialInterface>(nullptr, OutlinerMaterialPath);
		if (Outliner == nullptr)
		{
			UE_LOG(LogRTSShortsSquadTactics, Warning, TEXT("RTS_SHORTS_SOLDIER_OUTLINE_MATERIAL_MISSING path=%s"), OutlinerMaterialPath);
			return;
		}
		UMaterialInstanceDynamic* Outline = UMaterialInstanceDynamic::Create(Outliner, this);
		// Only the soldiers' stencil: the other two slots point at a value nothing writes, so resource outlines stay off.
		Outline->SetScalarParameterValue(TEXT("Primary Outline Custom Stencil order"), SoldierStencil);
		Outline->SetScalarParameterValue(TEXT("Secondary Outline Custom Stencil order"), UnusedStencil);
		Outline->SetScalarParameterValue(TEXT("Third Outline Custom Stencil order"), UnusedStencil);
		Outline->SetScalarParameterValue(TEXT("Global Stencil Order"), UnusedStencil);
		Outline->SetVectorParameterValue(TEXT("Primary Outline Color"), Settings.OutlineColor);
		Outline->SetScalarParameterValue(TEXT("Outline Width"), Settings.OutlineWidth);
		Outline->SetScalarParameterValue(TEXT("Outline Opacity"), 1.f);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		M_SoldierOutlineVolume = GetWorld()->SpawnActor<APostProcessVolume>(SpawnParameters);
		if (not IsValid(M_SoldierOutlineVolume))
		{
			return;
		}
		M_SoldierOutlineVolume->bUnbound = true;
		M_SoldierOutlineVolume->Priority = 100.f;
		M_SoldierOutlineVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Outline));
		UE_LOG(LogRTSShortsSquadTactics, Display, TEXT("RTS_SHORTS_SOLDIER_OUTLINE_ON stencil=%d width=%.1f"), SoldierStencil, Settings.OutlineWidth);
	}
	for (const TWeakObjectPtr<AActor>& Enemy : M_Battle.EnemyUnits)
	{
		ASquadUnit* Soldier = Cast<ASquadUnit>(Enemy.Get());
		if (not IsValid(Soldier))
		{
			continue;
		}
		const bool bOutline = Soldier->IsUnitAlive();
		// The soldier and what he carries: his weapon is an attached actor of its own.
		TArray<AActor*> Parts;
		Soldier->GetAttachedActors(Parts, true, true);
		Parts.Add(Soldier);
		for (const AActor* Part : Parts)
		{
			TArray<UPrimitiveComponent*> Components;
			Part->GetComponents(Components);
			for (UPrimitiveComponent* Component : Components)
			{
				const bool bDrawn = Cast<UMeshComponent>(Component) != nullptr;
				if (not bDrawn || Component->bRenderCustomDepth == bOutline)
				{
					continue;
				}
				Component->SetRenderCustomDepth(bOutline);
				Component->SetCustomDepthStencilValue(bOutline ? SoldierStencil : 0);
			}
		}
	}
}

void ARTSShortsDirector::BeginSquadTactics(const FRTSShortsSegment& Segment)
{
	const FRTSShortsSquadTactics& Settings = Segment.BattleSquadTactics;
	if (not Settings.bEnabled)
	{
		return;
	}
	for (int32 Index = 0; Index < M_Battle.EnemySquads.Num(); ++Index)
	{
		FRTSShortsSquadTacticState& Tactic = M_Battle.SquadTactics.AddDefaulted_GetRef();
		Tactic.Squad = M_Battle.EnemySquads[Index];
		Tactic.OrderAtSeconds = Settings.OrderAtSeconds + Settings.OrderStaggerSeconds * Index;
	}
}

void ARTSShortsDirector::TickSquadTactics(const int32 Frame)
{
	using namespace RTSShortsSquadTacticsPrivate;
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	const FRTSShortsSquadTactics& Settings = Segment.BattleSquadTactics;
	if (not Settings.bEnabled || M_Battle.SquadTactics.IsEmpty() || Frame % TickEveryFrames != 0)
	{
		return;
	}
	DropFallenCoverTrees();
	TickSoldierOutlines(Settings);
	AActor* Target = GetSquadTacticsTarget();
	if (Target == nullptr)
	{
		return;
	}
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	if (const UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
	{
		const float Health = TargetHealth->GetHealthPercentage();
		if (M_Battle.SquadTargetHealth >= 0.f && Health < M_Battle.SquadTargetHealth - KINDA_SMALL_NUMBER)
		{
			M_Battle.SquadTargetDamagedSeconds = SegmentSeconds;
		}
		M_Battle.SquadTargetHealth = Health;
	}
	for (FRTSShortsSquadTacticState& Tactic : M_Battle.SquadTactics)
	{
		ASquadController* Squad = Cast<ASquadController>(Tactic.Squad.Get());
		if (not IsValid(Squad))
		{
			continue;
		}
		const TArray<ASquadUnit*> Soldiers = GetLivingSoldiers(*Squad);
		if (Soldiers.IsEmpty())
		{
			continue;
		}
		if (not Tactic.bOrdered)
		{
			if (SegmentSeconds >= Tactic.OrderAtSeconds)
			{
				OrderSquadTactic(Tactic, Settings, *Target, SegmentSeconds);
			}
			continue;
		}
		const FVector Centroid = GetCentroid(Soldiers);
		if (Tactic.bMoving)
		{
			// Done when the squad's own move is: its soldiers settle onto their trunk points as that order ends, and an
			// attack sent while they are still walking would take them off the walk and out of the cover.
			const bool bMoveDone = SegmentSeconds - Tactic.MovedAtSeconds >= MoveStartGraceSeconds
				&& Squad->GetActiveCommandID() != EAbilityID::IdMove;
			const bool bArrived = bMoveDone && FVector::Dist2D(Centroid, Tactic.Destination) < ArrivedCm;
			if (not bMoveDone && SegmentSeconds - Tactic.MovedAtSeconds < Settings.MoveTimeoutSeconds)
			{
				continue;
			}
			Tactic.bMoving = false;
			Tactic.ArrivedSeconds = SegmentSeconds;
			Tactic.NextCheckSeconds = SegmentSeconds + Settings.HoldSeconds;
			// Holding: it shoots what it can reach from here and does not walk out of its cover after it.
			Squad->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
			// Their own choice, not an order: soldiers keep their cover instead of stepping out to obey it.
			Squad->IssueAutonomousAttack(Target);
			M_Battle.OrderedTargets.Add(Tactic.Squad, Target);
			int32 InCover = 0;
			for (const ASquadUnit* Soldier : Soldiers)
			{
				InCover += Soldier->GetCoverRuntimeState().AssignedCoverPoint.PointId != 0 ? 1 : 0;
				if (not bMoveDone)
				{
					// A squad still on its move when the time ran out: what each soldier is doing.
					UE_LOG(LogRTSShortsSquadTactics, Display,
					       TEXT("RTS_SHORTS_SQUAD_MOVE_STUCK soldier=%s stressed=%d cover=%d speed=%.0f toDestination=%.0f command=%d"),
					       *Soldier->GetName(), Soldier->GetIsStressed() ? 1 : 0, static_cast<int32>(Soldier->GetCoverRuntimeState().State),
					       Soldier->GetVelocity().Size2D(), FVector::Dist2D(Soldier->GetActorLocation(), Tactic.Destination),
					       static_cast<int32>(Squad->GetActiveCommandID()));
				}
			}
			UE_LOG(LogRTSShortsSquadTactics, Display, TEXT("RTS_SHORTS_SQUAD_ARRIVED at=%.2f squad=%s arrived=%d tree=%d inCover=%d/%d distance=%.0f"),
			       SegmentSeconds, *Squad->GetName(), bArrived ? 1 : 0, Tactic.TreeIndex, InCover, Soldiers.Num(),
			       FVector::Dist2D(Centroid, Target->GetActorLocation()));
			continue;
		}
		if (SegmentSeconds < Tactic.NextCheckSeconds)
		{
			continue;
		}
		Tactic.NextCheckSeconds = SegmentSeconds + RecheckSeconds;
		const float Range = GetWeaponRange(Soldiers);
		const bool bOutOfRange = Range > 0.f && FVector::Dist2D(Centroid, Target->GetActorLocation()) > Range * OutOfRangeFraction;
		const bool bTreeLost = Tactic.TreeIndex != INDEX_NONE
			&& (not M_CoverTrees.IsValidIndex(Tactic.TreeIndex) || M_CoverTrees[Tactic.TreeIndex].RegistrationId == 0);
		bool bInCover = false;
		for (const ASquadUnit* Soldier : Soldiers)
		{
			bInCover |= Soldier->GetCoverRuntimeState().AssignedCoverPoint.PointId != 0;
		}
		const bool bFaced = not bInCover && GetDegreesOffNose(*Target, Centroid) < Settings.FacedDegrees;
		// Holding where nobody is hurting the unit: this spot does not engage it, so try another angle.
		const bool bNoEffect = SegmentSeconds - FMath::Max(M_Battle.SquadTargetDamagedSeconds, Tactic.ArrivedSeconds) > Settings.NoEffectSeconds;
		if (bOutOfRange || bTreeLost || bFaced || bNoEffect)
		{
			UE_LOG(LogRTSShortsSquadTactics, Display, TEXT("RTS_SHORTS_SQUAD_RETHINK at=%.2f squad=%s outOfRange=%d treeLost=%d faced=%d noEffect=%d"),
			       SegmentSeconds, *Squad->GetName(), bOutOfRange ? 1 : 0, bTreeLost ? 1 : 0, bFaced ? 1 : 0, bNoEffect ? 1 : 0);
			OrderSquadTactic(Tactic, Settings, *Target, SegmentSeconds, bNoEffect ? Tactic.TreeIndex : INDEX_NONE);
		}
	}
}

void ARTSShortsDirector::OrderSquadTactic(FRTSShortsSquadTacticState& Tactic, const FRTSShortsSquadTactics& Settings,
                                          AActor& Target, const float SegmentSeconds, const int32 AvoidTreeIndex)
{
	using namespace RTSShortsSquadTacticsPrivate;
	ASquadController* Squad = Cast<ASquadController>(Tactic.Squad.Get());
	if (not IsValid(Squad))
	{
		return;
	}
	const TArray<ASquadUnit*> Soldiers = GetLivingSoldiers(*Squad);
	const float Range = GetWeaponRange(Soldiers);
	int32 TreeIndex = INDEX_NONE;
	FVector Destination = FVector::ZeroVector;
	TArray<FSquadUnitPlannedDestination> Plan;
	const bool bTree = TryPlanSquadTreeCover(Tactic, Settings, Target, Soldiers, Range, AvoidTreeIndex, TreeIndex, Destination, Plan);
	if (not bTree)
	{
		Destination = FindSquadFlankSpot(Settings, Target, Soldiers, Range);
	}
	const int32 CoverSoldiers = Plan.FilterByPredicate([](const FSquadUnitPlannedDestination& Entry)
	{
		return Entry.Position.GetIsCover();
	}).Num();
	// The same vector for both: the squad matches a plan to its move by location, to the centimetre.
	if (bTree)
	{
		Squad->SetPlannedMoveDestinations(Destination, MoveTemp(Plan));
	}
	Squad->SetAggroStance(ERTSAggroBehaviour::Stance_Aggressive);
	const FRotator Facing = (Target.GetActorLocation() - Destination).GetSafeNormal2D().Rotation();
	const ECommandQueueError Result = Squad->MoveToLocation(Destination, true, Facing, false);
	Tactic.bOrdered = true;
	Tactic.bMoving = true;
	Tactic.MovedAtSeconds = SegmentSeconds;
	Tactic.TreeIndex = TreeIndex;
	Tactic.Destination = Destination;
	M_Battle.OrderedTargets.Remove(Tactic.Squad);
	UE_LOG(LogRTSShortsSquadTactics, Display,
	       TEXT("RTS_SHORTS_SQUAD_TACTIC at=%.2f squad=%s choice=%s tree=%d coverSoldiers=%d/%d range=%.0f result=%d to=%s targetDistance=%.0f"),
	       SegmentSeconds, *Squad->GetName(), bTree ? TEXT("tree") : TEXT("flank"), TreeIndex, CoverSoldiers, Soldiers.Num(), Range,
	       static_cast<int32>(Result), *Destination.ToCompactString(), FVector::Dist2D(Destination, Target.GetActorLocation()));
}

FVector ARTSShortsDirector::FindSquadFlankSpot(const FRTSShortsSquadTactics& Settings, const AActor& Target,
                                              const TArray<ASquadUnit*>& Soldiers, const float WeaponRange) const
{
	using namespace RTSShortsSquadTacticsPrivate;
	const FVector TargetLocation = Target.GetActorLocation();
	const FVector Centroid = GetCentroid(Soldiers);
	const FVector Forward = Target.GetActorForwardVector().GetSafeNormal2D();
	const float Side = GetSide(Target, Centroid);
	const float Radius = WeaponRange > 0.f ? FMath::Min(Settings.FlankRadiusCm, WeaponRange * Settings.RangeFraction) : Settings.FlankRadiusCm;
	// Round the side first, then spots ever closer to the squad's own bearing, and last straight at the unit: a unit that
	// backed off to the edge of the stage leaves flank spots off the infantry's navigation, and a squad sent there stands still.
	TArray<FVector> Candidates;
	for (int32 Step = 0; Step < FlankFallbackSteps; ++Step)
	{
		const float Bearing = Settings.FlankBearingDegrees - FlankFallbackStepDegrees * Step;
		Candidates.Add(TargetLocation + Forward.RotateAngleAxis(Side * Bearing, FVector::UpVector) * Radius);
	}
	Candidates.Add(TargetLocation + (Centroid - TargetLocation).GetSafeNormal2D() * Radius);
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	for (const FVector& Candidate : Candidates)
	{
		const FVector Ground = GetBattleGroundPoint(Candidate);
		const UNavigationPath* Path = Navigation != nullptr && not Soldiers.IsEmpty()
			? UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Soldiers[0]->GetActorLocation(), Ground, Soldiers[0])
			: nullptr;
		if (Path != nullptr && Path->IsValid() && not Path->IsPartial() && Path->PathPoints.Num() > 0)
		{
			return Path->PathPoints.Last();
		}
	}
	UE_LOG(LogRTSShortsSquadTactics, Display, TEXT("RTS_SHORTS_SQUAD_NO_FLANK_PATH squad_at=%s target_at=%s"),
	       *Centroid.ToCompactString(), *TargetLocation.ToCompactString());
	return GetBattleGroundPoint(Candidates.Last());
}

bool ARTSShortsDirector::TryPlanSquadTreeCover(const FRTSShortsSquadTacticState& Tactic, const FRTSShortsSquadTactics& Settings,
                                               const AActor& Target, const TArray<ASquadUnit*>& Soldiers, const float WeaponRange,
                                               const int32 AvoidTreeIndex, int32& OutTreeIndex, FVector& OutDestination,
                                               TArray<FSquadUnitPlannedDestination>& OutPlan) const
{
	using namespace RTSShortsSquadTacticsPrivate;
	const URTSCoverFinderWorldSubsystem* Cover = GetWorld()->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	if (Cover == nullptr || Soldiers.IsEmpty() || WeaponRange <= 0.f)
	{
		return false;
	}
	const FVector TargetLocation = Target.GetActorLocation();
	const FVector Forward = Target.GetActorForwardVector().GetSafeNormal2D();
	const FVector Centroid = GetCentroid(Soldiers);
	const float SquadSide = GetSide(Target, Centroid);
	const ECollisionChannel LaneChannel = Soldiers[0]->GetOwningPlayer() == 1 ? COLLISION_TRACE_ENEMY : COLLISION_TRACE_PLAYER;
	FCollisionQueryParams LaneParams(SCENE_QUERY_STAT(ShortsSquadCoverLane), true);
	for (const TWeakObjectPtr<AActor>& Enemy : M_Battle.EnemyUnits)
	{
		if (Enemy.IsValid())
		{
			LaneParams.AddIgnoredActor(Enemy.Get());
		}
	}
	float BestScore = -TNumericLimits<float>::Max();
	TArray<FRTSCoverPoint> BestPoints;
	for (int32 TreeIndex = 0; TreeIndex < M_CoverTrees.Num(); ++TreeIndex)
	{
		const FRTSShortsCoverTreeState& Tree = M_CoverTrees[TreeIndex];
		if (Tree.RegistrationId == 0 || not Tree.Tree.IsValid() || TreeIndex == AvoidTreeIndex)
		{
			continue;
		}
		// One squad to a tree: a trunk shelters a few soldiers, not two squads.
		const bool bClaimed = M_Battle.SquadTactics.ContainsByPredicate([&Tactic, TreeIndex](const FRTSShortsSquadTacticState& Other)
		{
			return Other.Squad != Tactic.Squad && Other.TreeIndex == TreeIndex && GetIsAlive(Other.Squad);
		});
		if (bClaimed)
		{
			continue;
		}
		const FVector AwayFromTarget = (Tree.TrunkLocation - TargetLocation).GetSafeNormal2D();
		const float Distance = FVector::Dist2D(Tree.TrunkLocation, TargetLocation);
		if (Distance < Settings.MinimumCoverDistanceCm || Distance > WeaponRange * Settings.RangeFraction)
		{
			continue;
		}
		// Trunk points on the far side from the unit that have a clear shot at it from where the soldier steps out.
		TArray<FRTSCoverPoint> Usable;
		FCollisionQueryParams TreeLaneParams = LaneParams;
		TreeLaneParams.AddIgnoredActor(Tree.Tree.Get());
		for (const FRTSCoverPoint& Point : Cover->FindCoverPointsInRadius(Tree.TrunkLocation, MinimumTrunkCoverRadiusCm + 250.f))
		{
			if (Point.ProviderRegistrationId != Tree.RegistrationId || FVector::DotProduct(Point.CoverNormal, AwayFromTarget) < ShieldingDot)
			{
				continue;
			}
			FHitResult Hit;
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, GetFiringLaneStart(Point),
			                                                          TargetLocation + FVector::UpVector * TargetAimHeightCm,
			                                                          LaneChannel, TreeLaneParams);
			if (not bBlocked || Hit.GetActor() == &Target || (IsValid(Hit.GetActor()) && Hit.GetActor()->IsOwnedBy(&Target)))
			{
				Usable.Add(Point);
			}
		}
		if (Usable.IsEmpty())
		{
			continue;
		}
		const float Bearing = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, AwayFromTarget), -1.f, 1.f)));
		const float TreeSide = GetSide(Target, Tree.TrunkLocation);
		const float Score = Bearing / 180.f + (TreeSide == SquadSide ? SameSideScore : -SameSideScore)
			- FVector::Dist2D(Centroid, Tree.TrunkLocation) * TravelPenaltyPerCm;
		if (Score > BestScore)
		{
			BestScore = Score;
			OutTreeIndex = TreeIndex;
			BestPoints = MoveTemp(Usable);
		}
	}
	if (OutTreeIndex == INDEX_NONE)
	{
		return false;
	}
	const FRTSShortsCoverTreeState& Tree = M_CoverTrees[OutTreeIndex];
	const FVector AwayFromTarget = (Tree.TrunkLocation - TargetLocation).GetSafeNormal2D();
	// Straightest behind the trunk first.
	BestPoints.Sort([&AwayFromTarget](const FRTSCoverPoint& A, const FRTSCoverPoint& B)
	{
		return FVector::DotProduct(A.CoverNormal, AwayFromTarget) > FVector::DotProduct(B.CoverNormal, AwayFromTarget);
	});
	OutDestination = BestPoints[0].Location;
	const FVector Across = FVector::CrossProduct(FVector::UpVector, AwayFromTarget).GetSafeNormal();
	for (int32 Index = 0; Index < Soldiers.Num(); ++Index)
	{
		FSquadUnitPlannedDestination& Entry = OutPlan.AddDefaulted_GetRef();
		Entry.SquadUnit = Soldiers[Index];
		if (BestPoints.IsValidIndex(Index))
		{
			Entry.Position.Location = BestPoints[Index].Location;
			Entry.Position.Facing = -BestPoints[Index].CoverNormal;
			Entry.Position.Type = ESquadPlannedPositionType::StandingCover;
			Entry.Position.CoverPoint = BestPoints[Index];
			continue;
		}
		// More soldiers than trunk points: beside the tree, facing the unit.
		const int32 Slot = Index - BestPoints.Num();
		const float Offset = (Slot / 2 + 1) * SideSlotSpacingCm * (Slot % 2 == 0 ? 1.f : -1.f);
		Entry.Position.Location = OutDestination + Across * Offset;
		Entry.Position.Facing = -AwayFromTarget;
		Entry.Position.Type = ESquadPlannedPositionType::RegularStanding;
	}
	return true;
}
