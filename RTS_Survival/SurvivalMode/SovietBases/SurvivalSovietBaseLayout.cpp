// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseLayout.h"

namespace SovietBaseLayoutConstants
{
	constexpr double OccupancyCellSize = 1500.0;
	// Everything but gun pits stays inside this fraction of the build area radius (the flat part).
	constexpr double BuildAreaBlockFraction = 0.97;

	constexpr int32 ProductionPlacementAttempts = 8;
	constexpr double ProductionRadiusStep = 450.0;
	constexpr double ProductionAngleJitter = 0.3;
	constexpr double ProductionRingStartJitter = 0.5;
	constexpr double ProductionSpacingExtra = 250.0;

	constexpr int32 SupportPlacementAttempts = 12;
	constexpr double SupportRadiusMinFactor = 0.85;
	constexpr double SupportRadiusMaxFactor = 1.3;
	constexpr double SupportSpacingExtra = 200.0;

	constexpr int32 GunPitPlacementAttempts = 6;
	constexpr double GunPitJitter = 500.0;
	constexpr double GunPitSpacingExtra = 150.0;
	constexpr int32 GunPitRingCandidates = 13;
	constexpr double GunPitRingStepRadians = 0.33;
	constexpr double GunPitBlockMargin = 1.05;

	constexpr double UnitFootprintHalfExtent = 260.0;
	constexpr int32 UnitPlacementAttempts = 12;
	constexpr double UnitRadiusJitterFraction = 0.25;
	constexpr double HoldPositionRingFraction = 0.45;
	constexpr double HoldPositionArcRadians = 1.4;
	constexpr double PatrolSpawnRingFraction = 0.22;
	constexpr double ReserveSpawnRingFraction = 0.18;

	constexpr double LaunchPointRadiusFraction = 0.92;
	constexpr double LaunchPointFootprintHalfExtent = 600.0;
	constexpr double LaunchPointAngleStepRadians = 0.35;
	constexpr int32 LaunchPointAngleCandidates = 7;
}

namespace
{
	FVector2D SovietLayout_DirFromYaw(const double YawRadians)
	{
		return FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
	}

	double SovietLayout_YawTowards(const FVector2D& From, const FVector2D& To, const double Fallback)
	{
		const FVector2D Delta = To - From;
		if (Delta.IsNearlyZero())
		{
			return Fallback;
		}
		return FMath::Atan2(Delta.Y, Delta.X);
	}

	/** @brief One perimeter line in world XY: centre, outward facing and length along its right axis. */
	struct FSovietPerimeterLine
	{
		FVector2D Center = FVector2D::ZeroVector;
		double FacingYaw = 0.0;
		double Length = 0.0;
	};

	/**
	 * @brief Mutable state of one layout run. Member order matters: the builder keeps references to the random
	 * stream, occupancy grid and result declared before it.
	 */
	struct FSovietLayoutContext
	{
		explicit FSovietLayoutContext(const FSurvivalSovietLayoutInput& InInput)
			: Input(InInput)
			  , Preset(*InInput.Preset)
			  , Random(InInput.Seed)
			  , Occupancy(SovietBaseLayoutConstants::OccupancyCellSize)
			  , Builder(Random, Occupancy, Result)
		{
		}

		const FSurvivalSovietLayoutInput& Input;
		const FSurvivalSovietBasePreset& Preset;
		FRandomStream Random;
		FEnemyOccupancyGrid Occupancy;
		FEnemyBaseSetupResult Result;
		FEnemyBaseSetupBuilder Builder;

		TArray<ESurvivalSovietStructureRole> ClusterRoles;
		TArray<int32> DecoratedClusters;
		TArray<FVector2D> ProductionPositions;
		double ThreatYaw = 0.0;

		int32 BeginCluster(const ESurvivalSovietStructureRole Role)
		{
			const int32 Cluster = Builder.BeginCluster();
			if (ClusterRoles.Num() <= Cluster)
			{
				ClusterRoles.SetNum(Cluster + 1);
			}
			ClusterRoles[Cluster] = Role;
			return Cluster;
		}

		TArray<FEnemyResolvedEntry> ResolveList(const TArray<FEnemyBlueprintEntry>& Entries) const
		{
			TArray<FEnemyResolvedEntry> Resolved;
			for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
			{
				FEnemyResolvedEntry ResolvedEntry;
				if (not Input.ResolveEntry(Entries[EntryIndex], ResolvedEntry))
				{
					continue;
				}
				ResolvedEntry.SettingsIndex = EntryIndex;
				Resolved.Add(ResolvedEntry);
			}
			return Resolved;
		}

		void BlockOutsideRadius(const double Radius)
		{
			const FVector2D Center = Input.Center;
			const double RadiusSquared = FMath::Square(Radius);
			Builder.SetBlockedPredicate([Center, RadiusSquared](const FVector2D& Position)
			{
				return FVector2D::DistSquared(Center, Position) > RadiusSquared;
			});
		}
	};

	// ------------------------------------------------------------------
	// Production compound
	// ------------------------------------------------------------------

	TArray<const FSurvivalSovietStructureSlot*> SovietLayout_ExpandSlots(
		const TArray<FSurvivalSovietStructureSlot>& Slots,
		const int32 Strength,
		const int32 MaxProductionTier,
		const float Multiplier)
	{
		TArray<const FSurvivalSovietStructureSlot*> Copies;
		for (const FSurvivalSovietStructureSlot& Slot : Slots)
		{
			if (Slot.ProductionTier > MaxProductionTier)
			{
				continue;
			}
			const int32 Count = FSurvivalSovietBaseLayout::GetSlotCount(Slot, Strength, Multiplier);
			for (int32 CopyIndex = 0; CopyIndex < Count; ++CopyIndex)
			{
				Copies.Add(&Slot);
			}
		}
		return Copies;
	}

	void SovietLayout_PlaceProduction(FSovietLayoutContext& Context)
	{
		using namespace SovietBaseLayoutConstants;
		const FSurvivalSovietBasePreset& Preset = Context.Preset;
		const TArray<const FSurvivalSovietStructureSlot*> Copies = SovietLayout_ExpandSlots(
			Preset.ProductionBuildings, Context.Input.Strength, Context.Input.Scaling.MaxProductionTier, 1.f);
		if (Copies.IsEmpty())
		{
			return;
		}

		const TArray<FEnemyResolvedEntry> Sandbags = Context.ResolveList(Preset.Perimeter.Sandbags.SandbagUnits);
		const bool bWrapInSandbags = Preset.bSandbagProductionBuildings and Context.Input.Strength >= 1
			and Preset.Perimeter.Sandbags.bEnabled and not Sandbags.IsEmpty();

		const int32 RingCount = FMath::Max(1, Copies.Num() - 1);
		// The ring starts behind the main building as seen from the player.
		const double RingStartYaw = Context.ThreatYaw + PI
		+ Context.Random.FRandRange(-ProductionRingStartJitter, ProductionRingStartJitter);

		for (int32 CopyIndex = 0; CopyIndex < Copies.Num(); ++CopyIndex)
		{
			FEnemyBlueprintEntry Entry = Copies[CopyIndex]->Building;
			TArray<FEnemyResolvedEntry> Single = Context.ResolveList({Entry});
			if (Single.IsEmpty())
			{
				continue;
			}

			const int32 Cluster = Context.BeginCluster(ESurvivalSovietStructureRole::Production);
			const double BaseRadius = CopyIndex == 0 ? 0.0 : Preset.CoreCompoundRadius;
			const double BaseYaw = RingStartYaw + (CopyIndex - 1) * (2.0 * PI / RingCount);
			for (int32 Attempt = 0; Attempt < ProductionPlacementAttempts; ++Attempt)
			{
				const double Radius = BaseRadius + Attempt * ProductionRadiusStep;
				const double Yaw = BaseYaw + Context.Random.FRandRange(-ProductionAngleJitter, ProductionAngleJitter) * Attempt;
				FEnemyDefenseFrame Frame;
				Frame.Origin = Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * Radius;
				Frame.FacingYaw = Context.ThreatYaw;

				FEnemyFootprint Footprint;
				if (not Context.Builder.PlaceSingleBuilding(Single, Frame, ProductionSpacingExtra, Cluster, Footprint))
				{
					continue;
				}
				Context.ProductionPositions.Add(Frame.Origin);
				if (bWrapInSandbags)
				{
					Context.Builder.PlaceSandbagWall(Sandbags, Preset.Perimeter.Sandbags, Footprint, Cluster);
				}
				Context.DecoratedClusters.Add(Cluster);
				break;
			}
		}
	}

	double SovietLayout_ComputeRequiredClusterDistance(const TArray<FVector2D>& Positions)
	{
		if (Positions.Num() < 2)
		{
			return 0.0;
		}
		double Required = 0.0;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			double Nearest = TNumericLimits<double>::Max();
			for (int32 Other = 0; Other < Positions.Num(); ++Other)
			{
				if (Other != Index)
				{
					Nearest = FMath::Min(Nearest, FVector2D::Distance(Positions[Index], Positions[Other]));
				}
			}
			Required = FMath::Max(Required, Nearest);
		}
		return Required;
	}

	// ------------------------------------------------------------------
	// Support ring
	// ------------------------------------------------------------------

	void SovietLayout_PlaceSupport(FSovietLayoutContext& Context)
	{
		using namespace SovietBaseLayoutConstants;
		const FSurvivalSovietBasePreset& Preset = Context.Preset;
		const TArray<const FSurvivalSovietStructureSlot*> Copies = SovietLayout_ExpandSlots(
			Preset.SupportBuildings, Context.Input.Strength, TNumericLimits<int32>::Max(), 1.f);
		const double RingRadius = Context.Input.BuildAreaRadius * Preset.SupportRingFraction;

		for (const FSurvivalSovietStructureSlot* Slot : Copies)
		{
			TArray<FEnemyResolvedEntry> Single = Context.ResolveList({Slot->Building});
			if (Single.IsEmpty())
			{
				continue;
			}
			const int32 Cluster = Context.BeginCluster(ESurvivalSovietStructureRole::Support);
			for (int32 Attempt = 0; Attempt < SupportPlacementAttempts; ++Attempt)
			{
				const double Yaw = Context.Random.FRandRange(0.0, 2.0 * PI);
				const double Radius = RingRadius * Context.Random.FRandRange(SupportRadiusMinFactor, SupportRadiusMaxFactor);
				FEnemyDefenseFrame Frame;
				Frame.Origin = Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * Radius;
				Frame.FacingYaw = Context.ThreatYaw;

				FEnemyFootprint Footprint;
				if (Context.Builder.PlaceSingleBuilding(Single, Frame, SupportSpacingExtra, Cluster, Footprint))
				{
					Context.DecoratedClusters.Add(Cluster);
					break;
				}
			}
		}
	}

	// ------------------------------------------------------------------
	// Perimeter lines
	// ------------------------------------------------------------------

	TArray<FSovietPerimeterLine> SovietLayout_ComputePerimeterLines(
		const FSovietLayoutContext& Context,
		const EEnemyPerimeterLayout Layout)
	{
		const FSurvivalSovietPerimeterSettings& Perimeter = Context.Preset.Perimeter;
		const double HalfSize = Context.Input.BuildAreaRadius * Perimeter.HalfSizeFraction;
		const double SideLength = FMath::Max(0.0, 2.0 * HalfSize - Perimeter.LineMargin);
		TArray<FSovietPerimeterLine> Lines;

		switch (Layout)
		{
		case EEnemyPerimeterLayout::FrontLine:
			Lines.Add({Context.Input.Center + SovietLayout_DirFromYaw(Context.ThreatYaw) * HalfSize,
			           Context.ThreatYaw, SideLength});
			break;
		case EEnemyPerimeterLayout::Corners:
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const double Yaw = Context.ThreatYaw + PI * 0.25 + Corner * HALF_PI;
				Lines.Add({Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * (HalfSize * UE_SQRT_2),
				           Yaw, Perimeter.CornerLineLength});
			}
			break;
		case EEnemyPerimeterLayout::FullPerimeter:
		default:
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const double Yaw = Context.ThreatYaw + Side * HALF_PI;
				Lines.Add({Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * HalfSize, Yaw, SideLength});
			}
			break;
		}
		return Lines;
	}

	void SovietLayout_PlacePerimeter(FSovietLayoutContext& Context)
	{
		const FSurvivalSovietPerimeterSettings& Perimeter = Context.Preset.Perimeter;
		const TArray<FEnemyResolvedEntry> Bunkers = Context.ResolveList(Perimeter.Bunkers);
		if (Bunkers.IsEmpty())
		{
			return;
		}
		const TArray<FEnemyResolvedEntry> Sandbags = Context.ResolveList(Perimeter.Sandbags.SandbagUnits);
		const TArray<FEnemyResolvedEntry> Obstacles = Context.ResolveList(Perimeter.ObstacleBelt.Obstacles);
		const TArray<FEnemyResolvedEntry> Wire = Context.ResolveList(Perimeter.BarbedWire.WireSegments);

		double MaxHalfDepth = 0.0;
		double MaxHalfWidth = 0.0;
		for (const FEnemyResolvedEntry& Bunker : Bunkers)
		{
			const double Scale = Bunker.bOverrideScale ? Bunker.MaxScale : 1.0;
			MaxHalfDepth = FMath::Max(MaxHalfDepth, Bunker.FootprintHalfExtents.X * Scale);
			MaxHalfWidth = FMath::Max(MaxHalfWidth, Bunker.FootprintHalfExtents.Y * Scale);
		}

		const EEnemyPerimeterLayout Layout = Perimeter.LayoutPerStrength.IsEmpty()
			                                     ? EEnemyPerimeterLayout::FullPerimeter
			                                     : Perimeter.LayoutPerStrength[FMath::Clamp(
				                                     Context.Input.Strength, 0, Perimeter.LayoutPerStrength.Num() - 1)];
		const double Spacing = Perimeter.BunkerSpacing / FMath::Max(0.25f, Context.Input.Scaling.DefenseDensity);
		const bool bLayWire = Perimeter.BarbedWire.bEnabled and not Wire.IsEmpty()
			and Context.Input.Strength >= Perimeter.WireMinStrength;

		const TArray<FSovietPerimeterLine> Lines = SovietLayout_ComputePerimeterLines(Context, Layout);
		for (const FSovietPerimeterLine& Line : Lines)
		{
			const int32 Cluster = Context.BeginCluster(ESurvivalSovietStructureRole::PerimeterBunker);
			FEnemyDefenseFrame Frame;
			Frame.Origin = Line.Center;
			Frame.FacingYaw = Line.FacingYaw;
			Context.Builder.PlaceBunkerRow(Bunkers, Frame, Line.Length, Spacing, Perimeter.BunkerSpacingExtra, Cluster);

			const double HalfWidth = Line.Length * 0.5 + MaxHalfWidth;
			if (Perimeter.Sandbags.bEnabled and not Sandbags.IsEmpty())
			{
				FEnemyFootprint LineFootprint;
				LineFootprint.Center = Frame.Origin;
				LineFootprint.YawRadians = Frame.FacingYaw;
				LineFootprint.HalfExtents = FVector2D(MaxHalfDepth, HalfWidth);
				Context.Builder.PlaceSandbagWall(Sandbags, Perimeter.Sandbags, LineFootprint, Cluster);
			}
			if (Perimeter.ObstacleBelt.bEnabled and not Obstacles.IsEmpty())
			{
				Context.Builder.PlaceObstacleBelt(
					Obstacles, Perimeter.ObstacleBelt, Frame, MaxHalfDepth, HalfWidth, Cluster);
			}
			if (bLayWire)
			{
				Context.Builder.PlaceBarbedWire(Wire, Perimeter.BarbedWire, Frame, MaxHalfDepth, HalfWidth, Cluster);
			}
			Context.DecoratedClusters.Add(Cluster);
		}
	}

	// ------------------------------------------------------------------
	// Gun pits
	// ------------------------------------------------------------------

	TArray<FVector2D> SovietLayout_BuildGunPitCandidates(const FSovietLayoutContext& Context)
	{
		using namespace SovietBaseLayoutConstants;
		const FSurvivalSovietBasePreset& Preset = Context.Preset;
		const FVector2D Center = Context.Input.Center;
		const FVector2D ThreatDirection = SovietLayout_DirFromYaw(Context.ThreatYaw);
		const double MaxDistance = Context.Input.BuildAreaRadius * Preset.MaxDefensePointDistanceFraction;

		// Defense points first, the ones on the player's side first.
		TArray<FVector2D> DefensePoints;
		for (const FVector2D& Point : Context.Input.DefensePoints)
		{
			if (FVector2D::Distance(Point, Center) <= MaxDistance)
			{
				DefensePoints.Add(Point);
			}
		}
		DefensePoints.Sort([&Center, &ThreatDirection](const FVector2D& Left, const FVector2D& Right)
		{
			return FVector2D::DotProduct((Left - Center).GetSafeNormal(), ThreatDirection)
				> FVector2D::DotProduct((Right - Center).GetSafeNormal(), ThreatDirection);
		});

		TArray<FVector2D> Candidates = DefensePoints;
		// Ring fallback fanning out from the threat direction: 0, +s, -s, +2s, -2s ...
		const double RingRadius = Context.Input.BuildAreaRadius * Preset.GunPitRingFraction;
		for (int32 Step = 0; Step < GunPitRingCandidates; ++Step)
		{
			const int32 Magnitude = (Step + 1) / 2;
			const double Sign = (Step % 2 == 0) ? 1.0 : -1.0;
			const double Yaw = Context.ThreatYaw + Sign * Magnitude * GunPitRingStepRadians;
			Candidates.Add(Center + SovietLayout_DirFromYaw(Yaw) * RingRadius);
		}
		return Candidates;
	}

	void SovietLayout_PlaceGunPits(FSovietLayoutContext& Context)
	{
		using namespace SovietBaseLayoutConstants;
		const FSurvivalSovietBasePreset& Preset = Context.Preset;
		const TArray<const FSurvivalSovietStructureSlot*> Copies = SovietLayout_ExpandSlots(
			Preset.GunPits, Context.Input.Strength, TNumericLimits<int32>::Max(),
			Context.Input.Scaling.GunPitMultiplier);
		if (Copies.IsEmpty())
		{
			return;
		}

		Context.BlockOutsideRadius(
			Context.Input.BuildAreaRadius * Preset.MaxDefensePointDistanceFraction * GunPitBlockMargin);
		const TArray<FVector2D> Candidates = SovietLayout_BuildGunPitCandidates(Context);
		int32 NextCandidate = 0;

		for (const FSurvivalSovietStructureSlot* Slot : Copies)
		{
			TArray<FEnemyResolvedEntry> Single = Context.ResolveList({Slot->Building});
			if (Single.IsEmpty())
			{
				continue;
			}
			const int32 Cluster = Context.BeginCluster(ESurvivalSovietStructureRole::GunPit);
			bool bPlaced = false;
			while (not bPlaced and NextCandidate < Candidates.Num())
			{
				const FVector2D Candidate = Candidates[NextCandidate++];
				for (int32 Attempt = 0; Attempt < GunPitPlacementAttempts and not bPlaced; ++Attempt)
				{
					const FVector2D Jitter = Attempt == 0
						                         ? FVector2D::ZeroVector
						                         : FVector2D(Context.Random.FRandRange(-GunPitJitter, GunPitJitter),
						                                     Context.Random.FRandRange(-GunPitJitter, GunPitJitter));
					FEnemyDefenseFrame Frame;
					Frame.Origin = Candidate + Jitter;
					Frame.FacingYaw = SovietLayout_YawTowards(Frame.Origin, Context.Input.ThreatLocation,
					                                          Context.ThreatYaw);
					FEnemyFootprint Footprint;
					bPlaced = Context.Builder.PlaceSingleBuilding(Single, Frame, GunPitSpacingExtra, Cluster, Footprint);
				}
			}
		}

		Context.BlockOutsideRadius(Context.Input.BuildAreaRadius * BuildAreaBlockFraction);
	}

	// ------------------------------------------------------------------
	// Decoration
	// ------------------------------------------------------------------

	void SovietLayout_ScatterDecorators(FSovietLayoutContext& Context)
	{
		for (const FEnemyDecoratorProfile& Profile : Context.Preset.DecoratorProfiles)
		{
			const TArray<FEnemyResolvedEntry> Decorators = Context.ResolveList(Profile.Decorators);
			if (Decorators.IsEmpty())
			{
				continue;
			}
			for (const int32 Cluster : Context.DecoratedClusters)
			{
				Context.Builder.ScatterDecorators(Decorators, Profile, Cluster);
			}
		}
	}

	// ------------------------------------------------------------------
	// Garrison
	// ------------------------------------------------------------------

	FVector2D SovietLayout_FindUnitSpot(
		FSovietLayoutContext& Context,
		const ESurvivalSovietGarrisonBehaviour Behaviour)
	{
		using namespace SovietBaseLayoutConstants;
		const double BuildRadius = Context.Input.BuildAreaRadius;
		double RingFraction = ReserveSpawnRingFraction;
		bool bFaceThreat = false;
		switch (Behaviour)
		{
		case ESurvivalSovietGarrisonBehaviour::HoldPosition:
			RingFraction = HoldPositionRingFraction;
			bFaceThreat = true;
			break;
		case ESurvivalSovietGarrisonBehaviour::Patrol:
			RingFraction = PatrolSpawnRingFraction;
			break;
		case ESurvivalSovietGarrisonBehaviour::StrategicAIReserve:
		default:
			break;
		}

		const uint8 AvoidMask = EnemySolidCategoryMask() | EnemyOccupancyMask(EEnemyPlacementCategory::Decorator);
		FVector2D Spot = Context.Input.Center;
		for (int32 Attempt = 0; Attempt < UnitPlacementAttempts; ++Attempt)
		{
			const double Yaw = bFaceThreat
				                   ? Context.ThreatYaw + Context.Random.FRandRange(-HoldPositionArcRadians,
				                                                                   HoldPositionArcRadians)
				                   : Context.Random.FRandRange(0.0, 2.0 * PI);
			const double Radius = BuildRadius * RingFraction
				* Context.Random.FRandRange(1.0 - UnitRadiusJitterFraction, 1.0 + UnitRadiusJitterFraction);
			Spot = Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * Radius;

			FEnemyFootprint Footprint;
			Footprint.Center = Spot;
			Footprint.HalfExtents = FVector2D(UnitFootprintHalfExtent, UnitFootprintHalfExtent);
			Footprint.YawRadians = Context.ThreatYaw;
			if (Context.Occupancy.OverlapsAny(Footprint, AvoidMask))
			{
				continue;
			}
			Context.Occupancy.Add(Footprint, EEnemyPlacementCategory::Decorator);
			return Spot;
		}
		// Every spot was taken; units push each other apart, so the last candidate is still usable.
		return Spot;
	}

	void SovietLayout_PlanGarrison(FSovietLayoutContext& Context, FSurvivalSovietBasePlan& OutPlan)
	{
		const int32 Strength = Context.Input.Strength;
		for (const FSurvivalSovietGarrisonGroup& Group : Context.Preset.Garrison)
		{
			if (Group.UnitOptions.IsEmpty() or Strength < Group.MinStrength)
			{
				continue;
			}
			const float RawCount = Group.Count + Group.CountPerStrength * (Strength - Group.MinStrength);
			const int32 UnitCount = FMath::Max(0, FMath::RoundToInt(RawCount * Context.Input.Scaling.GarrisonMultiplier));
			if (UnitCount <= 0)
			{
				continue;
			}

			FSurvivalSovietPlannedGarrison& Planned = OutPlan.Garrison.AddDefaulted_GetRef();
			Planned.Behaviour = Group.Behaviour;
			Planned.FacingYawRadians = Context.ThreatYaw;
			Planned.IterationsAtPatrolPoint = Group.IterationsAtPatrolPoint;
			Planned.GuardSecondsPerIteration = Group.GuardSecondsPerIteration;
			Planned.GuardSphereRadius = Group.GuardSphereRadius;
			for (int32 UnitIndex = 0; UnitIndex < UnitCount; ++UnitIndex)
			{
				Planned.Units.Add(Group.UnitOptions[Context.Random.RandRange(0, Group.UnitOptions.Num() - 1)]);
				Planned.SpawnPositions.Add(SovietLayout_FindUnitSpot(Context, Group.Behaviour));
			}

			if (Group.Behaviour != ESurvivalSovietGarrisonBehaviour::Patrol)
			{
				continue;
			}
			const int32 PointCount = FMath::Max(3, Group.PatrolPointCount);
			const double Phase = Context.Random.FRandRange(0.0, 2.0 * PI);
			const double PatrolRadius = Context.Input.BuildAreaRadius * Group.PatrolRadiusFraction;
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const double Yaw = Phase + PointIndex * (2.0 * PI / PointCount);
				Planned.PatrolPoints.Add(Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * PatrolRadius);
			}
		}
	}

	FVector2D SovietLayout_FindLaunchPoint(const FSovietLayoutContext& Context)
	{
		using namespace SovietBaseLayoutConstants;
		const double Radius = Context.Input.BuildAreaRadius * LaunchPointRadiusFraction;
		FVector2D Fallback = Context.Input.Center + SovietLayout_DirFromYaw(Context.ThreatYaw) * Radius;
		for (int32 Step = 0; Step < LaunchPointAngleCandidates; ++Step)
		{
			const int32 Magnitude = (Step + 1) / 2;
			const double Sign = (Step % 2 == 0) ? 1.0 : -1.0;
			const double Yaw = Context.ThreatYaw + Sign * Magnitude * LaunchPointAngleStepRadians;
			FEnemyFootprint Footprint;
			Footprint.Center = Context.Input.Center + SovietLayout_DirFromYaw(Yaw) * Radius;
			Footprint.HalfExtents = FVector2D(LaunchPointFootprintHalfExtent, LaunchPointFootprintHalfExtent);
			if (not Context.Occupancy.OverlapsAny(Footprint, EnemySolidCategoryMask()))
			{
				return Footprint.Center;
			}
		}
		return Fallback;
	}

	void SovietLayout_ConvertResult(const FSovietLayoutContext& Context, FSurvivalSovietBasePlan& OutPlan)
	{
		OutPlan.Structures.Reserve(Context.Result.Actors.Num());
		for (const FEnemyPlacedActor& Placed : Context.Result.Actors)
		{
			if (Placed.AssetIndex == INDEX_NONE)
			{
				continue;
			}
			FSurvivalSovietPlannedStructure& Structure = OutPlan.Structures.AddDefaulted_GetRef();
			Structure.ClassIndex = Placed.AssetIndex;
			Structure.Position = Placed.Position;
			Structure.YawRadians = Placed.YawRadians;
			Structure.UniformScale = Placed.UniformScale;
			const bool bIsBuilding = Placed.Category == EEnemyPlacementCategory::Bunker;
			Structure.Role = bIsBuilding and Context.ClusterRoles.IsValidIndex(Placed.ClusterIndex)
				                 ? Context.ClusterRoles[Placed.ClusterIndex]
				                 : ESurvivalSovietStructureRole::Fortification;
		}
	}
}

int32 FSurvivalSovietBaseLayout::GetSlotCount(
	const FSurvivalSovietStructureSlot& Slot,
	const int32 Strength,
	const float Multiplier)
{
	if (Strength < Slot.MinStrength)
	{
		return 0;
	}
	const float RawCount = Slot.Count + Slot.CountPerStrength * (Strength - Slot.MinStrength);
	return FMath::Max(0, FMath::RoundToInt(RawCount * FMath::Max(0.f, Multiplier)));
}

FSurvivalSovietBasePlan FSurvivalSovietBaseLayout::BuildPlan(const FSurvivalSovietLayoutInput& Input)
{
	FSurvivalSovietBasePlan Plan;
	if (Input.Preset == nullptr or not Input.ResolveEntry)
	{
		return Plan;
	}

	FSovietLayoutContext Context(Input);
	Context.ThreatYaw = SovietLayout_YawTowards(Input.Center, Input.ThreatLocation, 0.0);
	Context.BlockOutsideRadius(Input.BuildAreaRadius * SovietBaseLayoutConstants::BuildAreaBlockFraction);

	// Order = priority for space: production core, perimeter lines, gun pits, support ring, props, units.
	SovietLayout_PlaceProduction(Context);
	SovietLayout_PlacePerimeter(Context);
	SovietLayout_PlaceGunPits(Context);
	SovietLayout_PlaceSupport(Context);
	SovietLayout_ScatterDecorators(Context);
	SovietLayout_PlanGarrison(Context, Plan);

	Plan.Center = Input.Center;
	Plan.ThreatYawRadians = Context.ThreatYaw;
	Plan.LaunchPoint = SovietLayout_FindLaunchPoint(Context);
	Plan.ProductionCount = Context.ProductionPositions.Num();
	Plan.RequiredCoreClusterDistance = SovietLayout_ComputeRequiredClusterDistance(Context.ProductionPositions);
	SovietLayout_ConvertResult(Context, Plan);
	return Plan;
}
