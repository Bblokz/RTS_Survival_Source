// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalStartContentPlanner.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavFilters/NavigationQueryFilter.h"

namespace SurvivalStartContentPlannerConstants
{
	// Field placement sweeps around its slot in steps of at most this angle.
	constexpr float MaxFieldSearchStepRadians = 12.f * PI / 180.f;
	// After this many attempts the field search moves outward by half a field radius.
	constexpr int32 FieldAttemptsPerSweep = 16;
	constexpr float FieldOutwardStepFraction = 0.5f;
	// Ground checks on the field disc: ring samples at this fraction of the radius.
	constexpr int32 FieldGroundRingSamples = 8;
	constexpr float FieldGroundRingFraction = 0.75f;
	constexpr float CraterMaxSlopeDegrees = 45.f;
	constexpr int32 FoliageAttemptsPerInstance = 6;
	constexpr float FoliageFootprintRadius = 60.f;
	constexpr float FullCircleDegrees = 360.f;
	// Fields may not sit higher than this above the deploy centre: up a rim flank harvesters would have to climb.
	constexpr float MaxFieldRiseAboveDeploy = 150.f;
	// A wreck this far below the deploy centre lies off the bench; at most this share of a start's wrecks may.
	constexpr float WreckOffBenchDrop = 150.f;
	constexpr float MaxWreckOffBenchShare = 2.f / 7.f;
	// The last quarter of a wreck's attempts accepts any ground, so a start whose bench ends early still gets its wrecks.
	constexpr float WreckAnyGroundAttemptShare = 0.75f;
	// Every field actor and wreck needs gentle ground over its footprint plus room for a harvester to reach it: ground
	// samples on this ring (footprint + margin) must keep the field's slope limit, and the spot must be on the navmesh
	// with a full path from the deploy centre (heavy tank agent; skipped when the world has no navigation data).
	constexpr int32 ActorFootprintSamples = 8;
	constexpr float ActorAccessMargin = 250.f;
	constexpr float ReachAgentRadius = 200.f;
	constexpr float ReachAgentHeight = 300.f;
	constexpr float ReachProjectExtentXY = 60.f;  // the spot itself must be on the navmesh, not just near it
	constexpr float ReachProjectExtentZ = 400.f;
	constexpr float ReachArriveTolerance = 300.f;
	// Specials and close nodes: hangar door search directions and attempts per item.
	constexpr int32 HangarDirections = 24;
	constexpr int32 HangarRadialSteps = 6;
	constexpr int32 CloseNodeAttempts = 64;
	constexpr float HangarApronGap = 500.f;
	constexpr float HangarStoreSideGap = 250.f;
	constexpr float DetailDecalYawDegrees = 360.f;
	// Scrap yard: the centre must be clear over this share of the yard radius; nodes may stand closer than other nodes.
	constexpr float YardFreeCoreFraction = 0.5f;
	constexpr float YardNodeClearanceFraction = 0.4f;
	// Props stand on the yard's far side: within this arc round the outward direction, at this share of the radius.
	constexpr float YardPropHalfArcRadians = 1.2f;
	constexpr float YardPropMinRadiusFraction = 0.8f;
	constexpr float YardPropMaxRadiusFraction = 1.35f;
	constexpr float YardPropSlopeFactor = 1.6f;
}

int32 FSurvivalStartContentPlan::GetPlannedFieldCount(const ESurvivalStartContentCategory Category) const
{
	int32 Count = 0;
	for (const FSurvivalPlannedField& Field : Fields)
	{
		Count += Field.Category == Category ? 1 : 0;
	}
	return Count;
}

int32 FSurvivalStartContentPlan::GetPlannedActorCount(const ESurvivalStartContentCategory Category) const
{
	int32 Count = 0;
	for (const FSurvivalPlannedActor& Actor : Actors)
	{
		Count += Actor.Category == Category ? 1 : 0;
	}
	return Count;
}

void FSurvivalStartContentPlan::CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	for (const FSurvivalPlannedActor& Actor : Actors)
	{
		OutPaths.AddUnique(Actor.ActorClass.ToSoftObjectPath());
	}
	for (const FSurvivalPlannedMeshInstance& Instance : MeshInstances)
	{
		OutPaths.AddUnique(Instance.Mesh.ToSoftObjectPath());
	}
}

void FSurvivalStartContentPlan::Reset()
{
	Actors.Reset();
	MeshInstances.Reset();
	Fields.Reset();
	FoliageCullDistance = 0.f;
	RequestedRadixiteFields = 0;
	RequestedMetalFields = 0;
	RequestedWrecks = 0;
	RequestedPits = 0;
}

FSurvivalStartContentPlanner::FSurvivalStartContentPlanner(
	UWorld& World,
	const USurvivalStartContentSettings& Settings,
	const FSurvivalStartContentPlanInput& Input)
	: M_Settings(Settings)
	, M_Input(Input)
	, M_Projector(World, Settings.GroundTraceUp, Settings.GroundTraceDown)
	, M_Random(Input.Seed)
	, M_Scale(Settings.GetDifficultyScale(Input.Difficulty))
	, M_World(&World)
{
}

FSurvivalStartContentPlan FSurvivalStartContentPlanner::BuildPlan()
{
	M_Plan.Reset();
	M_YardMetalNodes = 0;
	if (not M_Projector.HasLandscape())
	{
		return M_Plan;
	}
	InitReachability();
	PlanSpecial();
	PlanCloseResources();
	PlanPits();
	// Ground detail lies inside the build area (decals draw on the ground only; stones have no collision), so it is planned
	// before the build area is reserved; it still avoids the specials, close nodes and pits planned above.
	PlanGroundDetail();
	ReserveProtectedAreas();
	PlanResourceFields();
	PlanWrecks();
	PlanCraters();
	PlanFoliage();
	return M_Plan;
}

void FSurvivalStartContentPlanner::PlanPits()
{
	const FSurvivalPitSettings& PitSettings = M_Settings.Pits;
	M_Plan.RequestedPits = ScaleCount(PitSettings.PitCount, M_Scale.PitCountMultiplier, 0);
	FSurvivalOccupancy PitSpacing;
	const float SpacingRadius = PitSettings.MinPitSpacing * 0.5f;
	int32 PlannedPits = 0;
	for (const FSurvivalDefensePoint& DefensePoint : M_Input.DefensePoints)
	{
		if (PlannedPits >= M_Plan.RequestedPits)
		{
			return;
		}
		const FVector2D Position(DefensePoint.Location);
		FSurvivalGroundHit Ground;
		if (not PitSpacing.IsFree(Position, SpacingRadius)
			or not TryFindGround(Position, PitSettings.MaxGroundSlopeDegrees, Ground))
		{
			continue;
		}
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(PitSettings.Pits, M_Random);
		if (not PitSettings.Pits.IsValidIndex(EntryIndex))
		{
			return;
		}
		const FSurvivalWeightedActorClass& Entry = PitSettings.Pits[EntryIndex];
		const FVector2D Outward = Position - GetDeployCenter2D();
		const bool bCanFaceOutward = PitSettings.bFaceAwayFromDeployCenter and not Entry.bRandomYaw
			and not Outward.IsNearlyZero();
		const float Yaw = bCanFaceOutward
			                  ? static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Outward.Y, Outward.X)))
			                  : GetRandomYaw();
		AddPlannedActor(Entry, Ground, Yaw, ESurvivalStartContentCategory::Pit);
		PitSpacing.Add(Position, SpacingRadius);
		M_Occupancy.Add(Position, Entry.FootprintRadius);
		++PlannedPits;
	}
}

void FSurvivalStartContentPlanner::ReserveProtectedAreas()
{
	M_Occupancy.Add(GetDeployCenter2D(), M_Input.BuildAreaRadius);
	for (const FSurvivalDefensePoint& DefensePoint : M_Input.DefensePoints)
	{
		M_Occupancy.Add(FVector2D(DefensePoint.Location), M_Settings.DefensePointReserveRadius);
	}
	for (const FSurvivalReservedCircle& OtherStart : M_Input.OtherStartAreas)
	{
		M_Occupancy.Add(OtherStart.Center, OtherStart.Radius);
	}
}

void FSurvivalStartContentPlanner::PlanResourceFields()
{
	int32 RadixiteLeft = ScaleCount(M_Settings.RadixiteFields.FieldCount, M_Scale.ResourceFieldCountMultiplier, 1);
	int32 MetalLeft = ScaleCount(M_Settings.MetalFields.FieldCount, M_Scale.ResourceFieldCountMultiplier, 1);
	M_Plan.RequestedRadixiteFields = RadixiteLeft;
	M_Plan.RequestedMetalFields = MetalLeft;

	// Interleave the types so the harvesters of both resources have a field on every side of the base.
	// Metal goes first in each pair: it needs the larger disc on gentler ground, so it must not get the leftovers.
	TArray<ESurvivalStartContentCategory> FieldOrder;
	while (RadixiteLeft > 0 or MetalLeft > 0)
	{
		if (MetalLeft-- > 0)
		{
			FieldOrder.Add(ESurvivalStartContentCategory::Metal);
		}
		if (RadixiteLeft-- > 0)
		{
			FieldOrder.Add(ESurvivalStartContentCategory::Radixite);
		}
	}
	if (FieldOrder.IsEmpty())
	{
		return;
	}
	const float SlotAngle = 2.f * PI / FieldOrder.Num();
	const float StartAngle = M_Random.FRandRange(0.f, 2.f * PI);
	for (int32 FieldIndex = 0; FieldIndex < FieldOrder.Num(); ++FieldIndex)
	{
		const ESurvivalStartContentCategory Category = FieldOrder[FieldIndex];
		const FSurvivalResourceFieldSettings& FieldSettings = Category == ESurvivalStartContentCategory::Radixite
			                                                      ? M_Settings.RadixiteFields
			                                                      : M_Settings.MetalFields;
		TryPlanField(FieldSettings, Category, StartAngle + FieldIndex * SlotAngle, SlotAngle);
	}
}

bool FSurvivalStartContentPlanner::TryPlanField(
	const FSurvivalResourceFieldSettings& FieldSettings,
	const ESurvivalStartContentCategory Category,
	const float PreferredAngle,
	const float SlotAngle)
{
	using namespace SurvivalStartContentPlannerConstants;
	const float Radius = FieldSettings.FieldRadius;
	const float BaseDistance = M_Input.BuildAreaRadius + FieldSettings.GapFromBuildEdge + Radius;
	const float AngleStep = FMath::Min(SlotAngle * 0.25f, MaxFieldSearchStepRadians);
	for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
	{
		// Alternate around the slot: 0, +1, -1, +2, -2 ... steps.
		const int32 StepCount = (Attempt + 1) / 2;
		const float Angle = PreferredAngle + (Attempt % 2 == 1 ? StepCount : -StepCount) * AngleStep;
		const int32 Sweep = Attempt / FieldAttemptsPerSweep;
		const float Distance = BaseDistance + M_Random.FRandRange(0.f, FieldSettings.MaxExtraOutwardOffset)
			+ Sweep * Radius * FieldOutwardStepFraction;
		const FVector2D Center = GetDeployCenter2D() + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
		if (not M_Occupancy.IsFree(Center, Radius, M_Settings.FieldClearance))
		{
			continue;
		}
		FSurvivalGroundHit CenterHit;
		if (not GetIsFieldGroundValid(Center, FieldSettings, CenterHit))
		{
			continue;
		}
		FSurvivalPlannedField& Field = M_Plan.Fields.AddDefaulted_GetRef();
		Field.Center = CenterHit.Location;
		Field.Radius = Radius;
		Field.Category = Category;
		M_Occupancy.Add(Center, Radius);
		PopulateField(FieldSettings, Category, CenterHit);
		return true;
	}
	return false;
}

bool FSurvivalStartContentPlanner::GetIsFieldGroundValid(
	const FVector2D& Center,
	const FSurvivalResourceFieldSettings& FieldSettings,
	FSurvivalGroundHit& OutCenterHit) const
{
	using namespace SurvivalStartContentPlannerConstants;
	if (not TryFindGround(Center, FieldSettings.MaxGroundSlopeDegrees, OutCenterHit))
	{
		return false;
	}
	if (OutCenterHit.Location.Z - M_Input.DeployCenter.Z > MaxFieldRiseAboveDeploy)
	{
		return false;
	}
	const float RingRadius = FieldSettings.FieldRadius * FieldGroundRingFraction;
	// A field straddling a cliff edge would leave crystals unreachable for harvesters.
	const float MaxHeightDifference =
		FMath::Tan(FMath::DegreesToRadians(FieldSettings.MaxGroundSlopeDegrees)) * RingRadius;
	for (int32 SampleIndex = 0; SampleIndex < FieldGroundRingSamples; ++SampleIndex)
	{
		const float Angle = 2.f * PI * SampleIndex / FieldGroundRingSamples;
		FSurvivalGroundHit RingHit;
		if (not TryFindGround(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius,
		                      FieldSettings.MaxGroundSlopeDegrees, RingHit))
		{
			return false;
		}
		if (FMath::Abs(RingHit.Location.Z - OutCenterHit.Location.Z) > MaxHeightDifference)
		{
			return false;
		}
	}
	return GetIsReachable(OutCenterHit.Location);
}

void FSurvivalStartContentPlanner::PopulateField(
	const FSurvivalResourceFieldSettings& FieldSettings,
	const ESurvivalStartContentCategory Category,
	const FSurvivalGroundHit& CenterHit)
{
	const FVector2D FieldCenter(CenterHit.Location);
	FSurvivalOccupancy FieldOccupancy;
	float CentreFootprint = 0.f;
	const int32 CentreIndex = SurvivalStartPlacement::PickWeightedActorIndex(FieldSettings.CentreActors, M_Random);
	if (FieldSettings.CentreActors.IsValidIndex(CentreIndex))
	{
		const FSurvivalWeightedActorClass& CentreEntry = FieldSettings.CentreActors[CentreIndex];
		AddPlannedActor(CentreEntry, CenterHit, GetRandomYaw(), Category);
		FieldOccupancy.Add(FieldCenter, CentreEntry.FootprintRadius);
		CentreFootprint = CentreEntry.FootprintRadius;
	}

	const int32 MinActors = FMath::Min(FieldSettings.MinFieldActors, FieldSettings.MaxFieldActors);
	const int32 MaxActors = FMath::Max(FieldSettings.MinFieldActors, FieldSettings.MaxFieldActors);
	const int32 ActorCount = ScaleCount(M_Random.RandRange(MinActors, MaxActors),
	                                    M_Scale.ResourceFieldActorMultiplier, MinActors > 0 ? 1 : 0);
	for (int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		TryPlaceFieldActor(FieldSettings, Category, FieldCenter, CentreFootprint, FieldOccupancy);
	}
}

bool FSurvivalStartContentPlanner::TryPlaceFieldActor(
	const FSurvivalResourceFieldSettings& FieldSettings,
	const ESurvivalStartContentCategory Category,
	const FVector2D& FieldCenter,
	const float CentreFootprint,
	FSurvivalOccupancy& FieldOccupancy)
{
	// Only entries that fit between the centre actor and the field edge can ever be placed.
	TArray<FSurvivalWeightedActorClass> FittingEntries;
	for (const FSurvivalWeightedActorClass& Candidate : FieldSettings.FieldActors)
	{
		const float Room = FieldSettings.FieldRadius - Candidate.FootprintRadius;
		if (Room >= CentreFootprint + Candidate.FootprintRadius + FieldSettings.ActorClearance)
		{
			FittingEntries.Add(Candidate);
		}
	}
	const TArray<FSurvivalWeightedActorClass>& Entries = FittingEntries.IsEmpty() ? FieldSettings.FieldActors : FittingEntries;
	const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Entries, M_Random);
	if (not Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}
	const FSurvivalWeightedActorClass& Entry = Entries[EntryIndex];
	const float MaxOffset = FMath::Max(0.f, FieldSettings.FieldRadius - Entry.FootprintRadius);
	// Sample only the ring that is not covered by the centre actor.
	const float MinOffset = FMath::Min(MaxOffset, CentreFootprint > 0.f
		                                              ? CentreFootprint + Entry.FootprintRadius + FieldSettings.ActorClearance
		                                              : 0.f);
	for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
	{
		const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
			FieldCenter, MinOffset, MaxOffset, M_Random);
		FSurvivalGroundHit Ground;
		if (not FieldOccupancy.IsFree(Position, Entry.FootprintRadius, FieldSettings.ActorClearance)
			or not TryFindGround(Position, FieldSettings.MaxGroundSlopeDegrees, Ground)
			or not GetIsActorSpotValid(Position, Ground, Entry.FootprintRadius, FieldSettings.MaxGroundSlopeDegrees))
		{
			continue;
		}
		AddPlannedActor(Entry, Ground, GetRandomYaw(), Category);
		FieldOccupancy.Add(Position, Entry.FootprintRadius);
		return true;
	}
	return false;
}

void FSurvivalStartContentPlanner::PlanWrecks()
{
	using namespace SurvivalStartContentPlannerConstants;
	int32 TotalWrecks = 0;
	for (const FSurvivalWreckGroup& Group : M_Settings.WreckGroups)
	{
		TotalWrecks += ScaleCount(Group.Count, M_Scale.WreckCountMultiplier, 0);
	}
	// Salvage should be comparable between starts: most wrecks stay on the bench, a few lie below it.
	M_MaxWrecksOffBench = FMath::Max(1, FMath::RoundToInt(TotalWrecks * MaxWreckOffBenchShare));
	M_WrecksOffBench = 0;
	for (const FSurvivalWreckGroup& Group : M_Settings.WreckGroups)
	{
		PlanWreckGroup(Group);
	}
}

void FSurvivalStartContentPlanner::PlanWreckGroup(const FSurvivalWreckGroup& Group)
{
	const int32 WreckCount = ScaleCount(Group.Count, M_Scale.WreckCountMultiplier, 0);
	M_Plan.RequestedWrecks += WreckCount;
	const float InnerRadius = M_Input.BuildAreaRadius + Group.MinDistanceFromBuildEdge;
	const float OuterRadius = M_Input.BuildAreaRadius + Group.MaxDistanceFromBuildEdge;
	for (int32 WreckIndex = 0; WreckIndex < WreckCount; ++WreckIndex)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Group.Wrecks, M_Random);
		if (not Group.Wrecks.IsValidIndex(EntryIndex))
		{
			return;
		}
		const FSurvivalWeightedActorClass& Entry = Group.Wrecks[EntryIndex];
		// Half the spacing on both wrecks keeps every pair at least MinSpacing apart.
		const float SpacingRadius = FMath::Max(Entry.FootprintRadius, Group.MinSpacing * 0.5f);
		for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
				GetDeployCenter2D(), InnerRadius, OuterRadius, M_Random);
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, SpacingRadius)
				or not TryFindGround(Position, Group.MaxGroundSlopeDegrees, Ground)
				or not GetIsActorSpotValid(Position, Ground, Entry.FootprintRadius, Group.MaxGroundSlopeDegrees))
			{
				continue;
			}
			const bool bOffBench = Ground.Location.Z < M_Input.DeployCenter.Z - SurvivalStartContentPlannerConstants::WreckOffBenchDrop;
			const bool bAnyGround = Attempt >= M_Settings.PlacementAttemptsPerItem
				* SurvivalStartContentPlannerConstants::WreckAnyGroundAttemptShare;
			if (bOffBench and M_WrecksOffBench >= M_MaxWrecksOffBench and not bAnyGround)
			{
				continue;
			}
			M_WrecksOffBench += bOffBench ? 1 : 0;
			AddPlannedActor(Entry, Ground, GetRandomYaw(), ESurvivalStartContentCategory::Wreck);
			M_Occupancy.Add(Position, SpacingRadius);
			break;
		}
	}
}

void FSurvivalStartContentPlanner::PlanCraters()
{
	const FSurvivalClutterSettings& Clutter = M_Settings.Clutter;
	const float InnerRadius = M_Input.BuildAreaRadius + Clutter.MinDistanceFromBuildEdge;
	const float OuterRadius = M_Input.BuildAreaRadius + Clutter.MaxDistanceFromBuildEdge;
	for (int32 CraterIndex = 0; CraterIndex < Clutter.CraterCount; ++CraterIndex)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Clutter.CraterActors, M_Random);
		if (not Clutter.CraterActors.IsValidIndex(EntryIndex))
		{
			return;
		}
		const FSurvivalWeightedActorClass& Entry = Clutter.CraterActors[EntryIndex];
		for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
				GetDeployCenter2D(), InnerRadius, OuterRadius, M_Random);
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, Entry.FootprintRadius)
				or not TryFindGround(Position, SurvivalStartContentPlannerConstants::CraterMaxSlopeDegrees, Ground))
			{
				continue;
			}
			AddPlannedActor(Entry, Ground, GetRandomYaw(), ESurvivalStartContentCategory::Crater);
			M_Occupancy.Add(Position, Entry.FootprintRadius);
			break;
		}
	}
}

void FSurvivalStartContentPlanner::PlanFoliage()
{
	using namespace SurvivalStartContentPlannerConstants;
	const FSurvivalClutterSettings& Clutter = M_Settings.Clutter;
	const float InnerRadius = M_Input.BuildAreaRadius + Clutter.MinDistanceFromBuildEdge;
	const float OuterRadius = M_Input.BuildAreaRadius + Clutter.MaxDistanceFromBuildEdge;
	M_Plan.FoliageCullDistance = Clutter.FoliageCullDistance;
	for (int32 InstanceIndex = 0; InstanceIndex < Clutter.FoliageInstanceCount; ++InstanceIndex)
	{
		const int32 MeshIndex = SurvivalStartPlacement::PickWeightedMeshIndex(Clutter.FoliageMeshes, M_Random);
		if (not Clutter.FoliageMeshes.IsValidIndex(MeshIndex))
		{
			return;
		}
		const FSurvivalWeightedMesh& Entry = Clutter.FoliageMeshes[MeshIndex];
		for (int32 Attempt = 0; Attempt < FoliageAttemptsPerInstance; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
				GetDeployCenter2D(), InnerRadius, OuterRadius, M_Random);
			FSurvivalGroundHit Ground;
			// Foliage does not reserve space; it only avoids everything else.
			if (not M_Occupancy.IsFree(Position, FoliageFootprintRadius)
				or not TryFindGround(Position, Clutter.FoliageMaxGroundSlopeDegrees, Ground))
			{
				continue;
			}
			const float Scale = M_Random.FRandRange(FMath::Min(Entry.MinScale, Entry.MaxScale),
			                                        FMath::Max(Entry.MinScale, Entry.MaxScale));
			FSurvivalPlannedMeshInstance& Instance = M_Plan.MeshInstances.AddDefaulted_GetRef();
			Instance.Mesh = Entry.Mesh;
			Instance.Transform = FTransform(
				SurvivalStartPlacement::MakeGroundRotation(Ground.Normal, GetRandomYaw(), true),
				Ground.Location, FVector(Scale));
			break;
		}
	}
}

bool FSurvivalStartContentPlanner::GetIsOutsideLandingRing(const FVector2D& Position, const float Radius) const
{
	return FVector2D::Distance(Position, GetDeployCenter2D()) - Radius >= M_Settings.CloseResources.LandingClearRadius;
}

void FSurvivalStartContentPlanner::PlanSpecial()
{
	const FSurvivalStartSpecialSettings* Special = M_Settings.FindSpecial(M_Input.Special);
	if (Special == nullptr)
	{
		return;
	}
	if (not Special->Hangar.ActorClass.IsNull())
	{
		M_Plan.RequestedSpecialPieces += 1 + Special->CaptureVehicleCount;
		TryPlanHangar(*Special);
	}
	if (Special->WreckCount > 0)
	{
		PlanSpecialWrecks(*Special);
	}
	if (Special->ExtraMetalNodes > 0 and not Special->YardMetalActors.IsEmpty())
	{
		TryPlanScrapYard(*Special);
	}
}

void FSurvivalStartContentPlanner::TryPlanScrapYard(const FSurvivalStartSpecialSettings& Special)
{
	using namespace SurvivalStartContentPlannerConstants;
	const FVector2D Deploy = GetDeployCenter2D();
	const float StartAngle = M_Random.FRandRange(0.f, 2.f * PI);
	for (int32 Step = 0; Step < HangarRadialSteps; ++Step)
	{
		const float Distance = FMath::Lerp(Special.MinDistance, Special.MaxDistance,
		                                   static_cast<float>(Step) / FMath::Max(1, HangarRadialSteps - 1));
		for (int32 Direction = 0; Direction < HangarDirections; ++Direction)
		{
			const float Angle = StartAngle + 2.f * PI * Direction / HangarDirections;
			const FVector2D Outward(FMath::Cos(Angle), FMath::Sin(Angle));
			const FVector2D YardCenter = Deploy + Outward * Distance;
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(YardCenter, Special.YardRadius * YardFreeCoreFraction)
				or not GetIsOutsideLandingRing(YardCenter, Special.YardRadius * YardFreeCoreFraction)
				or not TryFindGround(YardCenter, Special.MaxGroundSlopeDegrees, Ground))
			{
				continue;
			}
			M_YardMetalNodes = PlanYardNodes(Special, YardCenter);
			M_Plan.PlannedCloseMetal += M_YardMetalNodes;
			PlanYardDressing(Special, YardCenter, Outward);
			return;
		}
	}
}

int32 FSurvivalStartContentPlanner::PlanYardNodes(const FSurvivalStartSpecialSettings& Special, const FVector2D& YardCenter)
{
	using namespace SurvivalStartContentPlannerConstants;
	const FSurvivalCloseResourceSettings& Close = M_Settings.CloseResources;
	int32 Planned = 0;
	for (int32 Node = 0; Node < Special.ExtraMetalNodes; ++Node)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Special.YardMetalActors, M_Random);
		if (not Special.YardMetalActors.IsValidIndex(EntryIndex))
		{
			break;
		}
		const FSurvivalWeightedActorClass& Entry = Special.YardMetalActors[EntryIndex];
		for (int32 Attempt = 0; Attempt < CloseNodeAttempts; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(YardCenter, 0.f, Special.YardRadius, M_Random);
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, Entry.FootprintRadius, Close.NodeClearance * YardNodeClearanceFraction)
				or not GetIsOutsideLandingRing(Position, Entry.FootprintRadius)
				or not TryFindGround(Position, Close.MaxGroundSlopeDegrees, Ground)
				or not GetIsActorSpotValid(Position, Ground, Entry.FootprintRadius, Close.MaxGroundSlopeDegrees))
			{
				continue;
			}
			AddPlannedActor(Entry, Ground, GetRandomYaw(), ESurvivalStartContentCategory::Metal);
			M_Occupancy.Add(Position, Entry.FootprintRadius);
			++Planned;
			break;
		}
	}
	return Planned;
}

void FSurvivalStartContentPlanner::PlanYardDressing(const FSurvivalStartSpecialSettings& Special, const FVector2D& YardCenter,
                                                    const FVector2D& Outward)
{
	using namespace SurvivalStartContentPlannerConstants;
	const float OutwardAngle = FMath::Atan2(Outward.Y, Outward.X);
	for (int32 Prop = 0; Prop < Special.YardPropCount; ++Prop)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Special.YardProps, M_Random);
		if (not Special.YardProps.IsValidIndex(EntryIndex))
		{
			break;
		}
		const FSurvivalWeightedActorClass& Entry = Special.YardProps[EntryIndex];
		for (int32 Attempt = 0; Attempt < CloseNodeAttempts; ++Attempt)
		{
			const float Angle = OutwardAngle + M_Random.FRandRange(-YardPropHalfArcRadians, YardPropHalfArcRadians);
			const float Distance = Special.YardRadius * M_Random.FRandRange(YardPropMinRadiusFraction, YardPropMaxRadiusFraction);
			const FVector2D Position = YardCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, Entry.FootprintRadius)
				or not GetIsOutsideLandingRing(Position, Entry.FootprintRadius)
				or not TryFindGround(Position, Special.MaxGroundSlopeDegrees * YardPropSlopeFactor, Ground))
			{
				continue;
			}
			AddPlannedActor(Entry, Ground, GetRandomYaw(), ESurvivalStartContentCategory::Detail);
			M_Occupancy.Add(Position, Entry.FootprintRadius);
			break;
		}
	}
	// Tracks and stains on the yard floor; decals draw on the ground only, so they need no room of their own.
	for (int32 Decal = 0; Decal < Special.YardDecalCount; ++Decal)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Special.YardDecals, M_Random);
		if (not Special.YardDecals.IsValidIndex(EntryIndex))
		{
			break;
		}
		const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(YardCenter, 0.f, Special.YardRadius, M_Random);
		FSurvivalGroundHit Ground;
		if (TryFindGround(Position, CraterMaxSlopeDegrees, Ground))
		{
			AddPlannedActor(Special.YardDecals[EntryIndex], Ground, GetRandomYaw(), ESurvivalStartContentCategory::Detail);
		}
	}
}

bool FSurvivalStartContentPlanner::TryPlanHangar(const FSurvivalStartSpecialSettings& Special)
{
	using namespace SurvivalStartContentPlannerConstants;
	const FVector2D Deploy = GetDeployCenter2D();
	const float StartAngle = M_Random.FRandRange(0.f, 2.f * PI);
	for (int32 Step = 0; Step < HangarRadialSteps; ++Step)
	{
		const float Distance = FMath::Lerp(Special.MinDistance, Special.MaxDistance, static_cast<float>(Step) / FMath::Max(1, HangarRadialSteps - 1));
		for (int32 Direction = 0; Direction < HangarDirections; ++Direction)
		{
			const float Angle = StartAngle + 2.f * PI * Direction / HangarDirections;
			const FVector2D Outward(FMath::Cos(Angle), FMath::Sin(Angle));
			const FVector2D HangarCenter = Deploy + Outward * Distance;
			const FVector2D Door = -Outward;  // the door (+X) faces the deploy centre
			const FVector2D Side(-Door.Y, Door.X);
			const float HangarYaw = FMath::RadiansToDegrees(FMath::Atan2(Door.Y, Door.X));
			FSurvivalGroundHit HangarGround;
			if (not M_Occupancy.IsFree(HangarCenter, Special.Hangar.FootprintRadius)
				or not GetIsOutsideLandingRing(HangarCenter, Special.Hangar.FootprintRadius)
				or not TryFindGround(HangarCenter, Special.MaxGroundSlopeDegrees, HangarGround)
				or not GetIsActorSpotValid(HangarCenter, HangarGround, Special.Hangar.FootprintRadius, Special.MaxGroundSlopeDegrees))
			{
				continue;
			}
			// Vehicles in a row on the apron, nose to the deploy centre; all must fit.
			TArray<FVector2D> VehicleSpots;
			TArray<FSurvivalGroundHit> VehicleGrounds;
			const FVector2D Apron = HangarCenter + Door * (Special.HangarHalfExtent.X + HangarApronGap + Special.CaptureVehicle.FootprintRadius);
			bool bApronFits = true;
			for (int32 Vehicle = 0; Vehicle < Special.CaptureVehicleCount and bApronFits; ++Vehicle)
			{
				const float Offset = (Vehicle - 0.5f * (Special.CaptureVehicleCount - 1)) * Special.CaptureVehicleSpacing;
				const FVector2D Spot = Apron + Side * Offset;
				FSurvivalGroundHit Ground;
				bApronFits = M_Occupancy.IsFree(Spot, Special.CaptureVehicle.FootprintRadius)
					and GetIsOutsideLandingRing(Spot, Special.CaptureVehicle.FootprintRadius)
					and TryFindGround(Spot, Special.MaxGroundSlopeDegrees, Ground)
					and GetIsActorSpotValid(Spot, Ground, Special.CaptureVehicle.FootprintRadius, Special.MaxGroundSlopeDegrees);
				VehicleSpots.Add(Spot);
				VehicleGrounds.Add(Ground);
			}
			if (not bApronFits)
			{
				continue;
			}
			AddPlannedActor(Special.Hangar, HangarGround, HangarYaw, ESurvivalStartContentCategory::Special);
			M_Occupancy.Add(HangarCenter, Special.Hangar.FootprintRadius);
			for (int32 Vehicle = 0; Vehicle < VehicleSpots.Num(); ++Vehicle)
			{
				AddPlannedActor(Special.CaptureVehicle, VehicleGrounds[Vehicle], HangarYaw, ESurvivalStartContentCategory::Special);
				M_Occupancy.Add(VehicleSpots[Vehicle], Special.CaptureVehicle.FootprintRadius);
			}
			M_Plan.PlannedSpecialPieces += 1 + VehicleSpots.Num();
			// Stores on both sides of the hangar mouth (decoration; skipped where they do not fit).
			for (int32 Store = 0; Store < Special.HangarStoreCount; ++Store)
			{
				const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Special.HangarStores, M_Random);
				if (not Special.HangarStores.IsValidIndex(EntryIndex))
				{
					break;
				}
				const FSurvivalWeightedActorClass& Entry = Special.HangarStores[EntryIndex];
				const float SideSign = Store % 2 == 0 ? 1.f : -1.f;
				// Beside the hangar's reserved circle, not just its walls, or the occupancy check always refuses the store.
				const float SideDistance = FMath::Max(Special.HangarHalfExtent.Y, Special.Hangar.FootprintRadius);
				const FVector2D Spot = HangarCenter + Side * SideSign * (SideDistance + HangarStoreSideGap + Entry.FootprintRadius)
					+ Door * (Special.HangarHalfExtent.X * 0.5f);
				FSurvivalGroundHit Ground;
				if (M_Occupancy.IsFree(Spot, Entry.FootprintRadius) and TryFindGround(Spot, Special.MaxGroundSlopeDegrees, Ground))
				{
					AddPlannedActor(Entry, Ground, HangarYaw, ESurvivalStartContentCategory::Special);
					M_Occupancy.Add(Spot, Entry.FootprintRadius);
				}
			}
			return true;
		}
	}
	return false;
}

void FSurvivalStartContentPlanner::PlanSpecialWrecks(const FSurvivalStartSpecialSettings& Special)
{
	M_Plan.RequestedSpecialPieces += Special.WreckCount;
	for (int32 WreckIndex = 0; WreckIndex < Special.WreckCount; ++WreckIndex)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Special.Wrecks, M_Random);
		if (not Special.Wrecks.IsValidIndex(EntryIndex))
		{
			return;
		}
		const FSurvivalWeightedActorClass& Entry = Special.Wrecks[EntryIndex];
		const float SpacingRadius = FMath::Max(Entry.FootprintRadius, Special.WreckSpacing * 0.5f);
		for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
				GetDeployCenter2D(), Special.MinDistance, Special.MaxDistance, M_Random);
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, SpacingRadius) or not GetIsOutsideLandingRing(Position, Entry.FootprintRadius)
				or not TryFindGround(Position, Special.MaxGroundSlopeDegrees, Ground)
				or not GetIsActorSpotValid(Position, Ground, Entry.FootprintRadius, Special.MaxGroundSlopeDegrees))
			{
				continue;
			}
			AddPlannedActor(Entry, Ground, GetRandomYaw(), ESurvivalStartContentCategory::Wreck);
			M_Occupancy.Add(Position, SpacingRadius);
			++M_Plan.PlannedSpecialPieces;
			break;
		}
	}
}

void FSurvivalStartContentPlanner::PlanCloseResources()
{
	int32 Radixite = 0;
	int32 Metal = 0;
	M_Settings.GetCloseNodeCounts(M_Input.Special, Radixite, Metal);
	M_Plan.RequestedCloseRadixite = Radixite;
	M_Plan.RequestedCloseMetal = Metal;
	// The scrap yard already holds some of the metal special's extra nodes.
	Metal = FMath::Max(0, Metal - M_YardMetalNodes);
	// Alternate the two kinds round the drop-off so both are close to every side of the HQ.
	TArray<ESurvivalStartContentCategory> Order;
	while (Radixite > 0 or Metal > 0)
	{
		if (Radixite-- > 0)
		{
			Order.Add(ESurvivalStartContentCategory::Radixite);
		}
		if (Metal-- > 0)
		{
			Order.Add(ESurvivalStartContentCategory::Metal);
		}
	}
	const float StartAngle = M_Random.FRandRange(0.f, 2.f * PI);
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		const bool bRadixite = Order[Index] == ESurvivalStartContentCategory::Radixite;
		const float Angle = StartAngle + 2.f * PI * Index / FMath::Max(1, Order.Num());
		if (TryPlanCloseNode(bRadixite ? M_Settings.CloseResources.RadixiteActors : M_Settings.CloseResources.MetalActors,
		                     Order[Index], Angle))
		{
			(bRadixite ? M_Plan.PlannedCloseRadixite : M_Plan.PlannedCloseMetal)++;
		}
	}
}

bool FSurvivalStartContentPlanner::TryPlanCloseNode(const TArray<FSurvivalWeightedActorClass>& Actors,
                                                    const ESurvivalStartContentCategory Category,
                                                    const float PreferredAngle)
{
	using namespace SurvivalStartContentPlannerConstants;
	const FSurvivalCloseResourceSettings& Close = M_Settings.CloseResources;
	const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Actors, M_Random);
	if (not Actors.IsValidIndex(EntryIndex))
	{
		return false;
	}
	const FSurvivalWeightedActorClass& Entry = Actors[EntryIndex];
	for (int32 Attempt = 0; Attempt < CloseNodeAttempts; ++Attempt)
	{
		// Around the preferred angle first, widening with every attempt.
		const float Spread = PI * FMath::Min(1.f, Attempt / static_cast<float>(CloseNodeAttempts / 2));
		const float Angle = PreferredAngle + M_Random.FRandRange(-Spread, Spread) * 0.5f;
		const float Distance = M_Random.FRandRange(Close.MinDistance, Close.MaxDistance);
		const FVector2D Position = GetDeployCenter2D() + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
		FSurvivalGroundHit Ground;
		if (not M_Occupancy.IsFree(Position, Entry.FootprintRadius, Close.NodeClearance)
			or not GetIsOutsideLandingRing(Position, Entry.FootprintRadius)
			or not TryFindGround(Position, Close.MaxGroundSlopeDegrees, Ground)
			or not GetIsActorSpotValid(Position, Ground, Entry.FootprintRadius, Close.MaxGroundSlopeDegrees))
		{
			continue;
		}
		AddPlannedActor(Entry, Ground, GetRandomYaw(), Category);
		M_Occupancy.Add(Position, Entry.FootprintRadius);
		return true;
	}
	return false;
}

void FSurvivalStartContentPlanner::PlanGroundDetail()
{
	using namespace SurvivalStartContentPlannerConstants;
	const FSurvivalGroundDetailSettings& Detail = M_Settings.GroundDetail;
	// Decals may lie under the drop lanes (they draw on the ground only); they avoid content and keep apart. Their own
	// spacing is kept apart from M_Occupancy so they never block the resource fields and wrecks planned after them.
	FSurvivalOccupancy DecalSpacing;
	for (int32 DecalIndex = 0; DecalIndex < Detail.DecalCount; ++DecalIndex)
	{
		const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(Detail.DecalActors, M_Random);
		if (not Detail.DecalActors.IsValidIndex(EntryIndex))
		{
			break;
		}
		const FSurvivalWeightedActorClass& Entry = Detail.DecalActors[EntryIndex];
		for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
				GetDeployCenter2D(), Detail.MinDistance, Detail.MaxDistance, M_Random);
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, Entry.FootprintRadius)
				or not DecalSpacing.IsFree(Position, Entry.FootprintRadius)
				or not TryFindGround(Position, CraterMaxSlopeDegrees, Ground))
			{
				continue;
			}
			AddPlannedActor(Entry, Ground, M_Random.FRandRange(0.f, DetailDecalYawDegrees), ESurvivalStartContentCategory::Detail);
			DecalSpacing.Add(Position, Entry.FootprintRadius * 0.5f);
			break;
		}
	}
	// Scattered stones (instanced, no collision) beyond the close resource ring.
	for (int32 StoneIndex = 0; StoneIndex < Detail.StoneCount; ++StoneIndex)
	{
		const int32 MeshIndex = SurvivalStartPlacement::PickWeightedMeshIndex(Detail.StoneMeshes, M_Random);
		if (not Detail.StoneMeshes.IsValidIndex(MeshIndex))
		{
			break;
		}
		const FSurvivalWeightedMesh& Entry = Detail.StoneMeshes[MeshIndex];
		for (int32 Attempt = 0; Attempt < FoliageAttemptsPerInstance; ++Attempt)
		{
			const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
				GetDeployCenter2D(), FMath::Max(Detail.StoneMinDistance, Detail.MinDistance), Detail.MaxDistance, M_Random);
			FSurvivalGroundHit Ground;
			if (not M_Occupancy.IsFree(Position, FoliageFootprintRadius)
				or not TryFindGround(Position, Detail.StoneMaxGroundSlopeDegrees, Ground))
			{
				continue;
			}
			const float Scale = M_Random.FRandRange(FMath::Min(Entry.MinScale, Entry.MaxScale), FMath::Max(Entry.MinScale, Entry.MaxScale));
			FSurvivalPlannedMeshInstance& Instance = M_Plan.MeshInstances.AddDefaulted_GetRef();
			Instance.Mesh = Entry.Mesh;
			Instance.Transform = FTransform(SurvivalStartPlacement::MakeGroundRotation(Ground.Normal, GetRandomYaw(), true),
			                                Ground.Location, FVector(Scale));
			break;
		}
	}
}

void FSurvivalStartContentPlanner::InitReachability()
{
	using namespace SurvivalStartContentPlannerConstants;
	M_ReachNavData = nullptr;
	UWorld* World = M_World.Get();
	UNavigationSystemV1* NavigationSystem = World != nullptr ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (NavigationSystem == nullptr)
	{
		return;
	}
	FNavAgentProperties Agent;
	Agent.AgentRadius = ReachAgentRadius;
	Agent.AgentHeight = ReachAgentHeight;
	const ANavigationData* NavData = NavigationSystem->GetNavDataForProps(Agent);
	FNavLocation Deploy;
	const FVector Extent(ReachProjectExtentXY, ReachProjectExtentXY, ReachProjectExtentZ);
	if (NavData == nullptr or not NavigationSystem->ProjectPointToNavigation(M_Input.DeployCenter, Deploy, Extent, NavData))
	{
		return;
	}
	M_ReachNavData = NavData;
	M_ReachDeploy = Deploy.Location;
}

bool FSurvivalStartContentPlanner::GetIsReachable(const FVector& Location) const
{
	using namespace SurvivalStartContentPlannerConstants;
	const ANavigationData* NavData = M_ReachNavData.Get();
	UWorld* World = M_World.Get();
	UNavigationSystemV1* NavigationSystem = World != nullptr ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (NavData == nullptr or NavigationSystem == nullptr)
	{
		// No navigation data (editor preview before a build): only the ground tests apply.
		return true;
	}
	FNavLocation Projected;
	const FVector Extent(ReachProjectExtentXY, ReachProjectExtentXY, ReachProjectExtentZ);
	if (not NavigationSystem->ProjectPointToNavigation(Location, Projected, Extent, NavData)
		or FVector::Dist2D(Projected.Location, Location) > ReachProjectExtentXY)
	{
		return false;
	}
	const FPathFindingQuery Query(nullptr, *NavData, M_ReachDeploy, Projected.Location,
	                              UNavigationQueryFilter::GetQueryFilter(*NavData, nullptr, nullptr));
	const FPathFindingResult Path = NavigationSystem->FindPathSync(Query);
	return Path.IsSuccessful() and Path.Path.IsValid() and not Path.IsPartial()
		and FVector::Dist2D(Path.Path->GetPathPoints().Last().Location, Projected.Location) <= ReachArriveTolerance;
}

bool FSurvivalStartContentPlanner::GetIsActorSpotValid(
	const FVector2D& Position,
	const FSurvivalGroundHit& Ground,
	const float FootprintRadius,
	const float MaxSlopeDegrees) const
{
	using namespace SurvivalStartContentPlannerConstants;
	// Gentle ground over the whole footprint plus an access ring: nothing straddles a cliff edge or sits on a ledge.
	const float RingRadius = FootprintRadius + ActorAccessMargin;
	const float MaxHeightDifference = FMath::Tan(FMath::DegreesToRadians(MaxSlopeDegrees)) * RingRadius;
	for (int32 SampleIndex = 0; SampleIndex < ActorFootprintSamples; ++SampleIndex)
	{
		const float Angle = 2.f * PI * SampleIndex / ActorFootprintSamples;
		FSurvivalGroundHit RingHit;
		if (not TryFindGround(Position + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius, MaxSlopeDegrees, RingHit)
			or FMath::Abs(RingHit.Location.Z - Ground.Location.Z) > MaxHeightDifference)
		{
			return false;
		}
	}
	return GetIsReachable(Ground.Location);
}

bool FSurvivalStartContentPlanner::TryFindGround(
	const FVector2D& Position,
	const float MaxSlopeDegrees,
	FSurvivalGroundHit& OutHit) const
{
	if (not M_Projector.TryProject(Position, OutHit))
	{
		return false;
	}
	return FSurvivalGroundProjector::GetSlopeDegrees(OutHit.Normal) <= MaxSlopeDegrees;
}

void FSurvivalStartContentPlanner::AddPlannedActor(
	const FSurvivalWeightedActorClass& Entry,
	const FSurvivalGroundHit& Ground,
	const float YawDegrees,
	const ESurvivalStartContentCategory Category)
{
	const FVector OffsetDirection = Entry.bAlignToGround ? Ground.Normal : FVector::UpVector;
	FSurvivalPlannedActor& Planned = M_Plan.Actors.AddDefaulted_GetRef();
	Planned.ActorClass = Entry.ActorClass;
	Planned.Category = Category;
	Planned.FootprintRadius = Entry.FootprintRadius;
	Planned.Transform = FTransform(
		SurvivalStartPlacement::MakeGroundRotation(Ground.Normal, YawDegrees + Entry.YawOffsetDegrees,
		                                           Entry.bAlignToGround),
		Ground.Location + OffsetDirection * Entry.ZOffset);
}

int32 FSurvivalStartContentPlanner::ScaleCount(const int32 Base, const float Multiplier,
                                               const int32 MinWhenBasePositive)
{
	if (Base <= 0)
	{
		return 0;
	}
	return FMath::Max(MinWhenBasePositive, FMath::RoundToInt(Base * Multiplier));
}

float FSurvivalStartContentPlanner::GetRandomYaw()
{
	return M_Random.FRandRange(0.f, SurvivalStartContentPlannerConstants::FullCircleDegrees);
}
