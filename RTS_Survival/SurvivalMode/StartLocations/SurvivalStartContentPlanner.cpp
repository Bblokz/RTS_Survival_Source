// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalStartContentPlanner.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

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
{
}

FSurvivalStartContentPlan FSurvivalStartContentPlanner::BuildPlan()
{
	M_Plan.Reset();
	if (not M_Projector.HasLandscape())
	{
		return M_Plan;
	}
	PlanPits();
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
	TArray<ESurvivalStartContentCategory> FieldOrder;
	while (RadixiteLeft > 0 or MetalLeft > 0)
	{
		if (RadixiteLeft-- > 0)
		{
			FieldOrder.Add(ESurvivalStartContentCategory::Radixite);
		}
		if (MetalLeft-- > 0)
		{
			FieldOrder.Add(ESurvivalStartContentCategory::Metal);
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
	return true;
}

void FSurvivalStartContentPlanner::PopulateField(
	const FSurvivalResourceFieldSettings& FieldSettings,
	const ESurvivalStartContentCategory Category,
	const FSurvivalGroundHit& CenterHit)
{
	const FVector2D FieldCenter(CenterHit.Location);
	FSurvivalOccupancy FieldOccupancy;
	const int32 CentreIndex = SurvivalStartPlacement::PickWeightedActorIndex(FieldSettings.CentreActors, M_Random);
	if (FieldSettings.CentreActors.IsValidIndex(CentreIndex))
	{
		const FSurvivalWeightedActorClass& CentreEntry = FieldSettings.CentreActors[CentreIndex];
		AddPlannedActor(CentreEntry, CenterHit, GetRandomYaw(), Category);
		FieldOccupancy.Add(FieldCenter, CentreEntry.FootprintRadius);
	}

	const int32 MinActors = FMath::Min(FieldSettings.MinFieldActors, FieldSettings.MaxFieldActors);
	const int32 MaxActors = FMath::Max(FieldSettings.MinFieldActors, FieldSettings.MaxFieldActors);
	const int32 ActorCount = ScaleCount(M_Random.RandRange(MinActors, MaxActors),
	                                    M_Scale.ResourceFieldActorMultiplier, MinActors > 0 ? 1 : 0);
	for (int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		TryPlaceFieldActor(FieldSettings, Category, FieldCenter, FieldOccupancy);
	}
}

bool FSurvivalStartContentPlanner::TryPlaceFieldActor(
	const FSurvivalResourceFieldSettings& FieldSettings,
	const ESurvivalStartContentCategory Category,
	const FVector2D& FieldCenter,
	FSurvivalOccupancy& FieldOccupancy)
{
	const int32 EntryIndex = SurvivalStartPlacement::PickWeightedActorIndex(FieldSettings.FieldActors, M_Random);
	if (not FieldSettings.FieldActors.IsValidIndex(EntryIndex))
	{
		return false;
	}
	const FSurvivalWeightedActorClass& Entry = FieldSettings.FieldActors[EntryIndex];
	const float MaxOffset = FMath::Max(0.f, FieldSettings.FieldRadius - Entry.FootprintRadius);
	for (int32 Attempt = 0; Attempt < M_Settings.PlacementAttemptsPerItem; ++Attempt)
	{
		const FVector2D Position = SurvivalStartPlacement::RandomPointInAnnulus(
			FieldCenter, 0.f, MaxOffset, M_Random);
		FSurvivalGroundHit Ground;
		if (not FieldOccupancy.IsFree(Position, Entry.FootprintRadius, FieldSettings.ActorClearance)
			or not TryFindGround(Position, FieldSettings.MaxGroundSlopeDegrees, Ground))
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
				or not TryFindGround(Position, Group.MaxGroundSlopeDegrees, Ground))
			{
				continue;
			}
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
