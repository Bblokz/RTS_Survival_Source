// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOIPlanner.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOISettings.h"

namespace SurvivalPOIComposerConstants
{
	constexpr int32 ConvoyCargoPiles = 2;
	constexpr int32 PlaceAttempts = 10;
	constexpr float SolidClearance = 60.f;
	// Decals may overlap a little; craters a little less.
	constexpr float DecalOverlapFactor = 0.45f;
	constexpr float CraterOverlapFactor = 0.7f;
	constexpr float InsideSiteTolerance = 1.05f;
	constexpr float FenceBreachChance = 0.08f;
	// Largest footprint radius of a standing object, for picking the route segments a composition must test.
	constexpr float MaxRouteFootprint = 1500.f;
	// Extra distance a standing object keeps beyond the route's free width and a gate foot's clearance: a Blueprint's box
	// reaches a little past its footprint radius (the post check measures the box).
	constexpr float LayoutKeepMargin = 100.f;
}

/**
 * @brief Lays out one site in its local frame (+X = the site's story direction: the attack, the door, the flight line,
 * the convoy's way) and writes world-space objects. Every object keeps its footprint free of the others, sits on
 * ground its asset accepts and stays inside the site radius.
 */
class FSurvivalPOIComposer
{
public:
	FSurvivalPOIComposer(const FSurvivalPOIPlanner& Planner, const FSurvivalPOISite& Site, TArray<FSurvivalPOIPlacedObject>& OutObjects)
		: M_Planner(Planner)
		, M_Settings(*Planner.M_Settings)
		, M_Site(Site)
		, M_Out(OutObjects)
		, M_Random(Site.Seed)
	{
		const float YawRadians = FMath::DegreesToRadians(Site.YawDegrees);
		M_Forward = FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
		M_Right = FVector2D(-M_Forward.Y, M_Forward.X);
		// Odd sub layouts are mirrored across the story axis.
		M_Mirror = Site.SubLayout % 2 == 1 ? -1.f : 1.f;
		GatherNearRouteSegments();
	}

	void SetAnchorOnly(const bool bInAnchorOnly) { bM_AnchorOnly = bInAnchorOnly; }

	void SetKeyObjectCount(FSurvivalPOIKeyObjectCount* InKeyObjects) { M_KeyObjects = InKeyObjects; }

	void Compose()
	{
		switch (M_Site.Type)
		{
		case ESurvivalPOIType::SmallBattlefield:
			ComposeBattlefield();
			break;
		case ESurvivalPOIType::RuinedHangar:
			ComposeHangar();
			break;
		case ESurvivalPOIType::CrashSite:
			ComposeCrashSite();
			break;
		case ESurvivalPOIType::DeadConvoy:
			ComposeConvoy();
			break;
		case ESurvivalPOIType::TankGraveyard:
			ComposeGraveyard();
			break;
		case ESurvivalPOIType::PumpStation:
			ComposePumpStation();
			break;
		case ESurvivalPOIType::RelayStation:
			ComposeRelayStation();
			break;
		default:
			break;
		}
	}

private:
	const FSurvivalPOIPlanner& M_Planner;
	const USurvivalPointsOfInterestSettings& M_Settings;
	const FSurvivalPOISite& M_Site;
	TArray<FSurvivalPOIPlacedObject>& M_Out;
	FRandomStream M_Random;
	// A planning trial: only the objects that can block an anchor are placed.
	bool bM_AnchorOnly = false;
	// Counts scavengeables and capture targets tried and placed (planning tests), when set.
	FSurvivalPOIKeyObjectCount* M_KeyObjects = nullptr;
	// Route segments that pass near the site: standing objects keep the route's free width plus their footprint off them.
	TArray<TPair<FVector2D, FVector2D>> M_NearRouteSegments;
	// Gate feet near the site: standing objects keep the gate foot clearance plus their footprint from them.
	TArray<FVector2D> M_NearGateFeet;
	FVector2D M_Forward = FVector2D(1.0, 0.0);
	FVector2D M_Right = FVector2D(0.0, 1.0);
	float M_Mirror = 1.f;
	// Footprint of the object placed last (the plane decides how far the salvage lies).
	float M_LastFootprint = 0.f;
	FSurvivalOccupancy M_Solid;
	FSurvivalOccupancy M_Flat;

	FVector2D ToWorld(const FVector2D& Local) const
	{
		return ToWorldAtScale(Local, M_Settings.Placement.CompositionScale);
	}

	FVector2D ToWorldAtScale(const FVector2D& Local, const float Scale) const
	{
		const FVector2D Scaled = Local * Scale;
		return FVector2D(M_Site.Center) + M_Forward * Scaled.X + M_Right * (Scaled.Y * M_Mirror);
	}

	void GatherNearRouteSegments()
	{
		using namespace SurvivalPOIComposerConstants;
		const FSurvivalPOILayoutExclusions& Layout = M_Settings.Placement.Layout;
		const FVector2D Center(M_Site.Center);
		const double Reach = M_Site.Radius * InsideSiteTolerance + Layout.RouteHalfWidth + MaxRouteFootprint;
		for (const FSurvivalPOILayoutLine& Route : Layout.Routes)
		{
			for (int32 Index = 1; Index < Route.Points.Num(); ++Index)
			{
				const FVector2D Closest = FMath::ClosestPointOnSegment2D(Center, Route.Points[Index - 1], Route.Points[Index]);
				if (FVector2D::Distance(Closest, Center) <= Reach)
				{
					M_NearRouteSegments.Emplace(Route.Points[Index - 1], Route.Points[Index]);
				}
			}
		}
		for (const FSurvivalPOILayoutLine& Ramp : Layout.Ramps)
		{
			if (FVector2D::Distance(Ramp.FootPoint, Center) <= M_Site.Radius * InsideSiteTolerance + Layout.GateFootClearance + MaxRouteFootprint)
			{
				M_NearGateFeet.Add(Ramp.FootPoint);
			}
		}
	}

	/** @return Whether a standing object of this radius at World keeps the free width of every route and clear of every gate foot. */
	bool GetIsClearOfRoutes(const FVector2D& World, const float Radius) const
	{
		using namespace SurvivalPOIComposerConstants;
		const FSurvivalPOILayoutExclusions& Layout = M_Settings.Placement.Layout;
		const float Keep = Layout.RouteHalfWidth + Radius + LayoutKeepMargin;
		for (const TPair<FVector2D, FVector2D>& Segment : M_NearRouteSegments)
		{
			if (FVector2D::Distance(FMath::ClosestPointOnSegment2D(World, Segment.Key, Segment.Value), World) < Keep)
			{
				return false;
			}
		}
		for (const FVector2D& Foot : M_NearGateFeet)
		{
			if (FVector2D::Distance(Foot, World) < Layout.GateFootClearance + Radius + LayoutKeepMargin)
			{
				return false;
			}
		}
		return true;
	}

	/** @return +1 or -1: the local Y side on which the nearest route passes inside the site, or 0 when none does. */
	float GetRouteSideLocal() const
	{
		FVector2D RoutePoint = FVector2D::ZeroVector;
		FVector2D Tangent = FVector2D::ZeroVector;
		double Distance = 0.0;
		if (not M_Planner.GetNearestRoutePoint(FVector2D(M_Site.Center), RoutePoint, Tangent, Distance) or Distance > M_Site.Radius)
		{
			return 0.f;
		}
		const double Side = FVector2D::DotProduct(RoutePoint - FVector2D(M_Site.Center), M_Right) * M_Mirror;
		return Side >= 0.0 ? 1.f : -1.f;
	}

	float ToWorldYaw(const float LocalYaw) const
	{
		return M_Site.YawDegrees + LocalYaw * M_Mirror;
	}

	float Range(const float Min, const float Max)
	{
		return M_Random.FRandRange(Min, Max);
	}

	/** @return AssetKey of a weighted pick from the pool, or INDEX_NONE. */
	int32 PickAsset(const FName PoolName)
	{
		const int32 PoolIndex = M_Settings.FindPoolIndex(PoolName);
		if (PoolIndex == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		const TArray<FSurvivalPOIAsset>& Assets = M_Settings.Pools[PoolIndex].Assets;
		float Total = 0.f;
		for (const FSurvivalPOIAsset& Asset : Assets)
		{
			Total += Asset.GetIsSet() ? FMath::Max(0.f, Asset.Weight) : 0.f;
		}
		if (Total <= 0.f)
		{
			return INDEX_NONE;
		}
		float Roll = M_Random.FRandRange(0.f, Total);
		for (int32 Index = 0; Index < Assets.Num(); ++Index)
		{
			if (not Assets[Index].GetIsSet() or Assets[Index].Weight <= 0.f)
			{
				continue;
			}
			Roll -= Assets[Index].Weight;
			if (Roll <= 0.f)
			{
				return PoolIndex * FSurvivalPOIPlacedObject::AssetKeyStride + Index;
			}
		}
		return PoolIndex * FSurvivalPOIPlacedObject::AssetKeyStride + Assets.Num() - 1;
	}

	bool GetIsSpotFree(const FVector2D& World, const FSurvivalPOIAsset& Asset, const ESurvivalPOIRole Role, const float WorldYaw) const
	{
		using namespace SurvivalPOIComposerConstants;
		const FVector2D Local = World - FVector2D(M_Site.Center);
		if (Local.Size() + Asset.FootprintRadius * 0.5f > M_Site.Radius * InsideSiteTolerance)
		{
			return false;
		}
		if (Role == ESurvivalPOIRole::Decal)
		{
			return M_Flat.IsFree(World, Asset.FootprintRadius * DecalOverlapFactor);
		}
		if (Role == ESurvivalPOIRole::Crater)
		{
			return M_Flat.IsFree(World, Asset.FootprintRadius * CraterOverlapFactor)
				and M_Solid.IsFree(World, Asset.FootprintRadius * DecalOverlapFactor);
		}
		if (Asset.HalfExtent.X > 0.f and Asset.HalfExtent.Y > 0.f)
		{
			// Long objects test the same row of circles they will occupy.
			const float CircleRadius = FMath::Min(Asset.HalfExtent.X, Asset.HalfExtent.Y);
			for (const FVector2D& Centre : GetRowCircles(World, WorldYaw, Asset.HalfExtent))
			{
				if (not M_Solid.IsFree(Centre, CircleRadius, SolidClearance) or not GetIsClearOfRoutes(Centre, CircleRadius))
				{
					return false;
				}
			}
			return true;
		}
		return M_Solid.IsFree(World, Asset.FootprintRadius, SolidClearance) and GetIsClearOfRoutes(World, Asset.FootprintRadius);
	}

	bool GetIsGroundFit(const FVector2D& World, const FSurvivalPOIAsset& Asset, float& OutHeight) const
	{
		FSurvivalGroundHit Hit;
		if (not M_Planner.M_Projector.TryProject(World, Hit))
		{
			return false;
		}
		OutHeight = Hit.Location.Z;
		return FSurvivalGroundProjector::GetSlopeDegrees(Hit.Normal) <= Asset.MaxSlopeDegrees;
	}

	/**
	 * @brief Places one object of the pool near a local spot.
	 * @param Jitter How far (cm) later attempts may move away from the spot.
	 * @return The placed object's world XY, or unset when nothing fit.
	 */
	TOptional<FVector2D> Place(const FName PoolName, const ESurvivalPOIRole Role, const FVector2D& Local, const float LocalYaw,
	                           const float Jitter, const float SinkCm = 0.f, const float ScaleOverride = 0.f)
	{
		using namespace SurvivalPOIComposerConstants;
		const int32 AssetKey = PickAsset(PoolName);
		const FSurvivalPOIAsset* Asset = M_Settings.GetAssetByKey(AssetKey);
		// A planning trial places only the hero: it is each hero type's first object, so the trial matches the real composition.
		const bool bSkippedInTrial = Role != ESurvivalPOIRole::Hero;
		if (Asset == nullptr or (bM_AnchorOnly and bSkippedInTrial))
		{
			return {};
		}
		// Unit-sized objects keep their size while the layout shrinks: a hero, capture target or scavengeable that finds no
		// spot at the shrunk layout tries its original spacing before it is given up; filler is simply dropped.
		const bool bKeyObject = Role == ESurvivalPOIRole::Hero or Role == ESurvivalPOIRole::Capture or Role == ESurvivalPOIRole::Scavenge;
		const bool bCounted = M_KeyObjects != nullptr and (Role == ESurvivalPOIRole::Capture or Role == ESurvivalPOIRole::Scavenge);
		if (bCounted)
		{
			++M_KeyObjects->Attempted;
			M_KeyObjects->CaptureAttempted += Role == ESurvivalPOIRole::Capture ? 1 : 0;
		}
		const TOptional<FVector2D> Placed = TryPlaceAsset(AssetKey, *Asset, Role, Local, LocalYaw, Jitter, SinkCm, ScaleOverride, bKeyObject,
		                                                  bCounted);
		if (Placed.IsSet() or Role != ESurvivalPOIRole::Hero)
		{
			return Placed;
		}
		// A main object that does not fit tries the other models of its pool, smallest first (the Avenger where the DC-3's
		// wings would reach a route or the rim).
		const int32 PoolIndex = M_Settings.FindPoolIndex(PoolName);
		const TArray<FSurvivalPOIAsset>& Assets = M_Settings.Pools[PoolIndex].Assets;
		TArray<int32> Others;
		for (int32 Index = 0; Index < Assets.Num(); ++Index)
		{
			const int32 OtherKey = PoolIndex * FSurvivalPOIPlacedObject::AssetKeyStride + Index;
			if (OtherKey != AssetKey and Assets[Index].GetIsSet())
			{
				Others.Add(Index);
			}
		}
		Others.Sort([&Assets](const int32 A, const int32 B) { return Assets[A].FootprintRadius < Assets[B].FootprintRadius; });
		for (const int32 Index : Others)
		{
			const TOptional<FVector2D> Fallback = TryPlaceAsset(PoolIndex * FSurvivalPOIPlacedObject::AssetKeyStride + Index, Assets[Index], Role,
			                                                    Local, LocalYaw, Jitter, SinkCm, ScaleOverride, bKeyObject, bCounted);
			if (Fallback.IsSet())
			{
				return Fallback;
			}
		}
		return {};
	}

	/** @brief Tries one asset at the spot and around it; adds it to the output and the occupancy when it fits. */
	TOptional<FVector2D> TryPlaceAsset(const int32 AssetKey, const FSurvivalPOIAsset& AssetRef, const ESurvivalPOIRole Role,
	                                   const FVector2D& Local, const float LocalYaw, const float Jitter, const float SinkCm,
	                                   const float ScaleOverride, const bool bKeyObject, const bool bCounted)
	{
		using namespace SurvivalPOIComposerConstants;
		const FSurvivalPOIAsset* Asset = &AssetRef;
		const int32 TotalAttempts = bKeyObject and M_Settings.Placement.CompositionScale < 1.f ? 2 * PlaceAttempts : PlaceAttempts;
		for (int32 TryIndex = 0; TryIndex < TotalAttempts; ++TryIndex)
		{
			const int32 Attempt = TryIndex % PlaceAttempts;
			const float Scale = TryIndex < PlaceAttempts ? M_Settings.Placement.CompositionScale : 1.f;
			const float Spread = Jitter * Attempt / static_cast<float>(PlaceAttempts - 1);
			const FVector2D TryLocal = Local + FVector2D(Range(-Spread, Spread), Range(-Spread, Spread));
			const FVector2D World = ToWorldAtScale(TryLocal, Scale);
			float Height = 0.f;
			const float WorldYaw = FRotator::NormalizeAxis(ToWorldYaw(LocalYaw) + Asset->YawOffsetDegrees);
			if (not GetIsSpotFree(World, *Asset, Role, WorldYaw) or not GetIsGroundFit(World, *Asset, Height))
			{
				continue;
			}
			FSurvivalPOIPlacedObject& Object = M_Out.AddDefaulted_GetRef();
			Object.SiteIndex = M_Site.SiteIndex;
			Object.AssetKey = AssetKey;
			Object.Role = Role;
			Object.Location = FVector(World, Height);
			Object.YawDegrees = FRotator::NormalizeAxis(ToWorldYaw(LocalYaw) + Asset->YawOffsetDegrees);
			Object.Scale = ScaleOverride > 0.f ? ScaleOverride : Range(Asset->ScaleRange.X, Asset->ScaleRange.Y);
			Object.SinkCm = SinkCm;
			M_LastFootprint = Asset->FootprintRadius;
			if (bCounted)
			{
				++M_KeyObjects->Placed;
				M_KeyObjects->CapturePlaced += Role == ESurvivalPOIRole::Capture ? 1 : 0;
			}
			const bool bIsFlat = Role == ESurvivalPOIRole::Decal or Role == ESurvivalPOIRole::Crater;
			if (not bIsFlat and Asset->HalfExtent.X > 0.f and Asset->HalfExtent.Y > 0.f)
			{
				AddRowOccupancy(World, Object.YawDegrees, Asset->HalfExtent);
				return World;
			}
			(bIsFlat ? M_Flat : M_Solid).Add(World, Asset->FootprintRadius * (bIsFlat ? DecalOverlapFactor : 1.f));
			return World;
		}
		return {};
	}

	/** @return The row of circles along a long object's X axis that stands for its rectangle. */
	static TArray<FVector2D> GetRowCircles(const FVector2D& World, const float YawDegrees, const FVector2D& HalfExtent)
	{
		const float Radius = FMath::Min(HalfExtent.X, HalfExtent.Y);
		const float Length = FMath::Max(HalfExtent.X, HalfExtent.Y) - Radius;
		const float Yaw = FMath::DegreesToRadians(YawDegrees + (HalfExtent.X >= HalfExtent.Y ? 0.f : 90.f));
		const FVector2D Axis(FMath::Cos(Yaw), FMath::Sin(Yaw));
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / (0.5f * Radius)));
		TArray<FVector2D> Centres;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			Centres.Add(World + Axis * FMath::Lerp(-Length, Length, Step / static_cast<float>(Steps)));
		}
		return Centres;
	}

	/** @brief Keeps a long object's rectangle free as a row of circles along its X axis. */
	void AddRowOccupancy(const FVector2D& World, const float YawDegrees, const FVector2D& HalfExtent)
	{
		const float Radius = FMath::Min(HalfExtent.X, HalfExtent.Y);
		for (const FVector2D& Centre : GetRowCircles(World, YawDegrees, HalfExtent))
		{
			M_Solid.Add(Centre, Radius);
		}
	}

	FVector2D ToLocal(const FVector2D& World) const
	{
		const FVector2D Offset = World - FVector2D(M_Site.Center);
		const double Scale = FMath::Max(UE_KINDA_SMALL_NUMBER, static_cast<double>(M_Settings.Placement.CompositionScale));
		return FVector2D(FVector2D::DotProduct(Offset, M_Forward), FVector2D::DotProduct(Offset, M_Right) * M_Mirror) / Scale;
	}

	void ScatterCraters(const int32 Count, const FVector2D& LocalMin, const FVector2D& LocalMax, const float SmallShare)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FName Pool = M_Random.FRand() < SmallShare ? SurvivalPOIPools::SmallCrater : SurvivalPOIPools::Crater;
			const FVector2D Local(Range(LocalMin.X, LocalMax.X), Range(LocalMin.Y, LocalMax.Y));
			Place(Pool, ESurvivalPOIRole::Crater, Local, Range(0.f, 360.f), 400.f);
		}
	}

	// --- Small battlefield: Soviet armour attacked along +X into a German line of gun pits and sandbags. ---

	void ComposeBattlefield()
	{
		const float LineX = Range(1200.f, 1600.f);
		const float Bend = M_Site.SubLayout == 1 ? Range(350.f, 700.f) : (M_Site.SubLayout == 2 ? -Range(300.f, 550.f) : 0.f);
		auto LineAt = [LineX, Bend](const float Y) { return LineX + Bend * FMath::Square(Y / 2200.f); };

		// The defenders' line: pits face the attack (-X), sandbags close the gaps, ammo lies behind the guns.
		const int32 PitCount = M_Random.RandRange(2, 3);
		const float PitSpacing = Range(1350.f, 1700.f);
		for (int32 Index = 0; Index < PitCount; ++Index)
		{
			const float Y = (Index - 0.5f * (PitCount - 1)) * PitSpacing + Range(-150.f, 150.f);
			const TOptional<FVector2D> Pit = Place(SurvivalPOIPools::GunPit, ESurvivalPOIRole::Capture, FVector2D(LineAt(Y), Y),
			                                       180.f + Range(-12.f, 12.f), 250.f);
			if (not Pit.IsSet())
			{
				continue;
			}
			const FVector2D PitLocal = ToLocal(Pit.GetValue());
			for (int32 Box = 0; Box < M_Random.RandRange(1, 2); ++Box)
			{
				Place(SurvivalPOIPools::AmmoDebris, ESurvivalPOIRole::Debris,
				      PitLocal + FVector2D(Range(420.f, 700.f), Range(-350.f, 350.f)), Range(0.f, 360.f), 150.f);
			}
		}
		const int32 SandbagCount = M_Random.RandRange(3, 5);
		for (int32 Index = 0; Index < SandbagCount; ++Index)
		{
			const float Y = Range(-2400.f, 2400.f);
			const float Tangent = FMath::RadiansToDegrees(FMath::Atan(2.f * Bend * Y / FMath::Square(2200.f)));
			Place(SurvivalPOIPools::Sandbags, ESurvivalPOIRole::Cover, FVector2D(LineAt(Y) - 150.f, Y), 180.f - Tangent, 200.f);
		}
		// The defenders' own losses behind the line, still facing the attack.
		const int32 DefenderWrecks = M_Site.SubLayout == 0 ? 2 : 1;
		for (int32 Index = 0; Index < DefenderWrecks; ++Index)
		{
			const float Yaw = M_Random.FRand() < 0.3f ? 180.f + Range(40.f, 75.f) : 180.f + Range(-20.f, 20.f);
			Place(SurvivalPOIPools::ScavengeTankWreck, ESurvivalPOIRole::Scavenge,
			      FVector2D(LineX + Range(700.f, 1300.f), Range(-1800.f, 1800.f)), Yaw, 400.f);
		}
		ComposeBattlefieldAttackers(LineX);
		// Shells fell on the line and along the axis of the attack.
		ScatterCraters(M_Random.RandRange(6, 9), FVector2D(LineX - 500.f, -2600.f), FVector2D(LineX + 600.f, 2600.f), 0.5f);
		ScatterCraters(M_Random.RandRange(3, 5), FVector2D(-2600.f, -1300.f), FVector2D(LineX - 600.f, 1300.f), 0.4f);
	}

	void ComposeBattlefieldAttackers(const float LineX)
	{
		// More wrecks far out, fewer near the line: the attack thinned out as it came in.
		const int32 AttackerCount = M_Random.RandRange(2, 4);
		for (int32 Index = 0; Index < AttackerCount; ++Index)
		{
			const float Progress = FMath::Pow(M_Random.FRand(), 1.6f);
			const float X = -2700.f + (LineX - 1100.f + 2700.f) * Progress;
			const float Y = Range(-2100.f, 2100.f) * (1.f - 0.4f * Progress);
			const float Yaw = M_Random.FRand() < 0.25f ? Range(50.f, 95.f) * (M_Random.FRand() < 0.5f ? -1.f : 1.f) : Range(-25.f, 25.f);
			// The first hulls still hold salvage; the rest are burnt-out cover.
			const bool bIsScavenge = Index < (M_Site.SubLayout == 2 ? 2 : 1);
			const TOptional<FVector2D> Wreck = Place(bIsScavenge ? SurvivalPOIPools::SovietScavengeWreck : SurvivalPOIPools::SovietWreck,
			                                         bIsScavenge ? ESurvivalPOIRole::Scavenge : ESurvivalPOIRole::Cover,
			                                         FVector2D(X, Y), Yaw, 450.f);
			if (Wreck.IsSet() and M_Random.FRand() < 0.6f)
			{
				const FVector2D Beside = ToLocal(Wreck.GetValue()) + FVector2D(Range(-200.f, 200.f), Range(-200.f, 200.f));
				Place(SurvivalPOIPools::Scorch, ESurvivalPOIRole::Decal, Beside, Range(0.f, 360.f), 150.f);
			}
		}
		Place(SurvivalPOIPools::VehicleParts, ESurvivalPOIRole::Scavenge, FVector2D(Range(-1800.f, -300.f), Range(-1600.f, 1600.f)),
		      Range(0.f, 360.f), 500.f);
	}

	// --- Ruined hangar: abandoned vehicles on the apron, nose out, stores along the walls. ---

	void ComposeHangar()
	{
		const float HangarX = -Range(350.f, 550.f);
		// Sunk 30 cm so the floor slab sits in the sand instead of standing on it as a lip.
		Place(SurvivalPOIPools::Hangar, ESurvivalPOIRole::Hero, FVector2D(HangarX, 0.f), Range(-3.f, 3.f), 0.f, 30.f);
		const int32 VehicleCount = M_Site.SubLayout == 2 ? 1 : 2;
		for (int32 Index = 0; Index < VehicleCount; ++Index)
		{
			const float Y = VehicleCount == 1 ? Range(-200.f, 200.f) : (Index == 0 ? -1.f : 1.f) * Range(420.f, 560.f);
			// In the door, nose out, clear of the hangar's nav obstacle so infantry and the vehicle can leave.
			const TOptional<FVector2D> Vehicle = Place(SurvivalPOIPools::CaptureVehicle, ESurvivalPOIRole::Capture,
			                                           FVector2D(HangarX + Range(1450.f, 1600.f), Y), Range(-8.f, 8.f), 200.f);
			if (Vehicle.IsSet())
			{
				// Their tracks run from the door out onto the sand.
				const FVector2D VehicleLocal = ToLocal(Vehicle.GetValue());
				Place(SurvivalPOIPools::ApronTracks, ESurvivalPOIRole::Decal, VehicleLocal + FVector2D(Range(500.f, 800.f), 0.f),
				      Range(0.f, 360.f), 60.f);
			}
		}
		const int32 StoreCount = M_Random.RandRange(3, 5);
		for (int32 Index = 0; Index < StoreCount; ++Index)
		{
			const float Side = Index % 2 == 0 ? 1.f : -1.f;
			const float Yaw = M_Random.FRand() < 0.5f ? 0.f : 90.f;
			Place(SurvivalPOIPools::HangarStores, ESurvivalPOIRole::Cover,
			      FVector2D(HangarX + Range(-800.f, 700.f), Side * Range(1150.f, 1400.f)), Yaw + Range(-6.f, 6.f), 250.f);
		}
		const float ScavengeSide = M_Random.FRand() < 0.5f ? 1.f : -1.f;
		Place(SurvivalPOIPools::VehicleParts, ESurvivalPOIRole::Scavenge,
		      FVector2D(Range(-200.f, 700.f), ScavengeSide * Range(1650.f, 1950.f)), Range(0.f, 360.f), 300.f);
		Place(SurvivalPOIPools::CarWreck, ESurvivalPOIRole::Scavenge,
		      FVector2D(Range(900.f, 1500.f), -ScavengeSide * Range(1400.f, 1900.f)), Range(0.f, 360.f), 300.f);
		ScatterCraters(M_Random.RandRange(2, 3), FVector2D(-1900.f, -1900.f), FVector2D(900.f, 1900.f), 0.7f);
		ScatterCraters(1, FVector2D(2000.f, -1600.f), FVector2D(2500.f, 1600.f), 0.f);
	}

	// --- Crash site: the plane at the end of a gouged scar along its flight line, debris thinning out. ---

	void ComposeCrashSite()
	{
		const float PlaneX = Range(1300.f, 1800.f);
		Place(SurvivalPOIPools::PlaneWreck, ESurvivalPOIRole::Hero, FVector2D(PlaneX, 0.f), Range(-12.f, 12.f), 200.f, 40.f);
		const float PlaneFootprint = M_LastFootprint;
		const float ImpactX = -Range(2600.f, 3000.f);
		Place(SurvivalPOIPools::Crater, ESurvivalPOIRole::Crater, FVector2D(ImpactX, 0.f), Range(0.f, 360.f), 150.f);
		// The gouge: a chain of small craters from the first impact to under the fuselage.
		for (float X = ImpactX + 400.f; X < PlaneX - 200.f; X += Range(380.f, 520.f))
		{
			Place(SurvivalPOIPools::CrashScar, ESurvivalPOIRole::Decal, FVector2D(X, Range(-90.f, 90.f)), Range(0.f, 360.f), 60.f);
		}
		ScatterCraters(M_Random.RandRange(2, 4), FVector2D(ImpactX, -500.f), FVector2D(PlaneX - 1500.f, 500.f), 0.8f);
		// Pieces it shed on first impact lie back along the scar, to one side (one aircraft per site: no second airframe).
		for (int32 Index = 0; Index < 2; ++Index)
		{
			Place(SurvivalPOIPools::PlaneDebris, ESurvivalPOIRole::Debris,
			      FVector2D(ImpactX + Range(500.f, 1500.f), (M_Random.FRand() < 0.5f ? 1.f : -1.f) * Range(400.f, 900.f)),
			      Range(0.f, 360.f), 300.f, 10.f);
		}
		for (int32 Index = 0; Index < M_Random.RandRange(3, 5); ++Index)
		{
			const float Angle = FMath::DegreesToRadians(Range(-110.f, 110.f));
			const FVector2D Local = FVector2D(PlaneX, 0.f) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Range(1700.f, 2600.f);
			Place(SurvivalPOIPools::PlaneDebris, ESurvivalPOIRole::Debris, Local, Range(0.f, 360.f), 350.f, 15.f);
		}
		// Salvage piles thrown clear of the fuselage, around it and back along the scar.
		const float PlaneReach = PlaneFootprint + 450.f;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(180.f + (Index - 2) * 55.f + Range(-15.f, 15.f));
			FVector2D Local = FVector2D(PlaneX, 0.f) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (PlaneReach + Range(150.f, 900.f));
			// Off the scar itself.
			Local.Y += Local.Y >= 0.f ? 400.f : -400.f;
			Place(SurvivalPOIPools::CrashScavenge, ESurvivalPOIRole::Scavenge, Local, Range(0.f, 360.f), 500.f);
		}
		for (int32 Index = 0; Index < M_Random.RandRange(5, 7); ++Index)
		{
			const float Angle = Range(0.f, UE_TWO_PI);
			const FVector2D Local = FVector2D(PlaneX, 0.f) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Range(2600.f, 3600.f);
			Place(SurvivalPOIPools::AmmoDebris, ESurvivalPOIRole::Debris, Local, Range(0.f, 360.f), 300.f);
		}
	}

	// --- Dead convoy: a column nose to tail, lead and tail knocked out, some pushed off the track, craters on the ambush side. ---

	void ComposeConvoy()
	{
		const int32 VehicleCount = M_Site.SubLayout == 2 ? 6 : 5;
		float AmbushSide = M_Random.FRand() < 0.5f ? 1.f : -1.f;
		// Along a route the column was hit from the road: craters pock the verge, pushed wrecks and cargo lie off the road.
		const float RouteSide = GetRouteSideLocal();
		if (RouteSide != 0.f)
		{
			AmbushSide = RouteSide;
		}
		float X = -3500.f;
		TArray<int32> Pushed = {M_Random.RandRange(1, VehicleCount - 2)};
		if (M_Random.FRand() < 0.5f)
		{
			Pushed.AddUnique(M_Random.RandRange(1, VehicleCount - 2));
		}
		for (int32 Index = 0; Index < VehicleCount; ++Index)
		{
			const bool bIsEscort = Index == 0 or Index == VehicleCount - 1;
			const bool bIsPushed = Pushed.Contains(Index);
			const float Y = bIsPushed ? -AmbushSide * Range(550.f, 900.f) : Range(-80.f, 80.f);
			const float Yaw = bIsPushed ? -AmbushSide * Range(20.f, 38.f) : Range(-6.f, 6.f);
			const FName MiddlePool = M_Random.FRand() < 0.7f ? SurvivalPOIPools::ConvoyTruck : SurvivalPOIPools::ConvoyVehicle;
			Place(bIsEscort ? SurvivalPOIPools::ConvoyEscort : MiddlePool,
			      bIsEscort ? ESurvivalPOIRole::Cover : ESurvivalPOIRole::Scavenge, FVector2D(X, Y), Yaw, 120.f);
			// A gap where a hit split the column.
			X += Range(1100.f, 1350.f) + (M_Random.FRand() < 0.25f ? Range(300.f, 700.f) : 0.f);
		}
		for (float TrackX = -4300.f; TrackX <= 4300.f; TrackX += Range(1500.f, 1800.f))
		{
			Place(SurvivalPOIPools::ConvoyTracks, ESurvivalPOIRole::Decal, FVector2D(TrackX, Range(-40.f, 40.f)), Range(-3.f, 3.f), 60.f);
		}
		ScatterCraters(M_Random.RandRange(5, 7), FVector2D(-3600.f, AmbushSide > 0.f ? 450.f : -2100.f),
		               FVector2D(3600.f, AmbushSide > 0.f ? 2100.f : -450.f), 0.4f);
		Place(SurvivalPOIPools::VehicleParts, ESurvivalPOIRole::Scavenge, FVector2D(Range(-2000.f, 2000.f), -AmbushSide * Range(1300.f, 1800.f)),
		      Range(0.f, 360.f), 400.f);
		// Cargo thrown from the trucks on the side away from the ambush: what makes the column worth the drive.
		for (int32 Pile = 0; Pile < SurvivalPOIComposerConstants::ConvoyCargoPiles; ++Pile)
		{
			Place(SurvivalPOIPools::CrashScavenge, ESurvivalPOIRole::Scavenge,
			      FVector2D(Range(-2800.f, 2800.f), -AmbushSide * Range(500.f, 1100.f)), Range(0.f, 360.f), 400.f);
		}
	}

	// --- Tank graveyard: an armoured attack that died here, hulls scattered, turrets thrown clear. ---

	void ComposeGraveyard()
	{
		const int32 WreckCount = M_Random.RandRange(6, 8);
		const int32 ScavengeCount = M_Random.RandRange(2, 3);
		TArray<FVector2D> SovietHulls;
		for (int32 Index = 0; Index < WreckCount; ++Index)
		{
			const bool bIsScavenge = Index < ScavengeCount;
			const float Angle = Range(0.f, UE_TWO_PI);
			const FVector2D Local = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::Sqrt(M_Random.FRand()) * 3100.f;
			// One attack axis (+X), as the battle went; a hull now and then slewed round when it was hit.
			const float Yaw = M_Random.FRand() < 0.12f ? Range(60.f, 120.f) * (M_Random.FRand() < 0.5f ? -1.f : 1.f) : Range(-30.f, 30.f);
			const FName ScavengePool = Index % 2 == 0 ? SurvivalPOIPools::SovietScavengeWreck : SurvivalPOIPools::ScavengeTankWreck;
			const TOptional<FVector2D> Wreck = Place(bIsScavenge ? ScavengePool : SurvivalPOIPools::SovietWreck,
			                                         bIsScavenge ? ESurvivalPOIRole::Scavenge : ESurvivalPOIRole::Cover, Local, Yaw, 600.f);
			if (Wreck.IsSet() and not bIsScavenge)
			{
				SovietHulls.Add(ToLocal(Wreck.GetValue()));
			}
		}
		for (int32 Index = 0; Index < FMath::Min(2, SovietHulls.Num()); ++Index)
		{
			const FVector2D Thrown = SovietHulls[Index] + FVector2D(Range(500.f, 900.f), Range(-400.f, 400.f));
			Place(SurvivalPOIPools::ThrownTurret, ESurvivalPOIRole::Debris, Thrown, Range(0.f, 360.f), 200.f);
		}
		Place(SurvivalPOIPools::VehicleParts, ESurvivalPOIRole::Scavenge, FVector2D(Range(-1500.f, 1500.f), Range(-1500.f, 1500.f)),
		      Range(0.f, 360.f), 900.f);
		ScatterCraters(M_Random.RandRange(8, 12), FVector2D(-3500.f, -3500.f), FVector2D(3500.f, 3500.f), 0.5f);
	}

	// --- Pump station: the water tower over the dry well, the pump engine beside it, a pipe run and the fuel drums. ---

	void ComposePumpStation()
	{
		Place(SurvivalPOIPools::PumpTower, ESurvivalPOIRole::Hero, FVector2D(Range(-200.f, 200.f), Range(-200.f, 200.f)),
		      Range(0.f, 360.f), 150.f);
		Place(SurvivalPOIPools::PumpMachinery, ESurvivalPOIRole::Cover, FVector2D(Range(700.f, 900.f), Range(-250.f, 250.f)),
		      Range(-10.f, 10.f), 200.f);
		// The tanks stand in a row beside the tower, one yard, all square to the pipe run.
		const int32 MachineCount = M_Random.RandRange(2, 3);
		const float YardSide = M_Random.FRand() < 0.5f ? 1.f : -1.f;
		for (int32 Index = 0; Index < MachineCount; ++Index)
		{
			Place(SurvivalPOIPools::PumpMachinery, ESurvivalPOIRole::Cover,
			      FVector2D(-900.f + Index * Range(650.f, 800.f), YardSide * Range(1050.f, 1200.f)), 0.f, 120.f);
		}
		// The pipe run leaves the pump along +X toward the next well.
		const int32 PipeCount = M_Random.RandRange(2, 3);
		for (int32 Index = 0; Index < PipeCount; ++Index)
		{
			Place(SurvivalPOIPools::PumpPipes, ESurvivalPOIRole::Cover, FVector2D(1500.f + Index * 800.f, Range(-120.f, 120.f)),
			      Range(-6.f, 6.f), 150.f);
		}
		Place(SurvivalPOIPools::PumpBarrels, ESurvivalPOIRole::Cover, FVector2D(Range(-1800.f, -1200.f), Range(-600.f, 600.f)),
		      Range(0.f, 360.f), 300.f);
		Place(SurvivalPOIPools::VehicleParts, ESurvivalPOIRole::Scavenge, FVector2D(Range(-600.f, 600.f), Range(1500.f, 2100.f)),
		      Range(0.f, 360.f), 400.f);
		Place(SurvivalPOIPools::CarWreck, ESurvivalPOIRole::Scavenge, FVector2D(Range(-1500.f, -600.f), -Range(1500.f, 2100.f)),
		      Range(0.f, 360.f), 400.f);
		ScatterCraters(M_Random.RandRange(2, 4), FVector2D(-2200.f, -2200.f), FVector2D(2200.f, 2200.f), 0.6f);
	}

	// --- Relay station: a wire-fenced yard round a steel mast, a pole line out, one mast fallen outside the fence. ---

	void ComposeRelayStation()
	{
		const FVector2D Half(Range(1300.f, 1500.f), Range(1100.f, 1250.f));
		Place(SurvivalPOIPools::RelayMast, ESurvivalPOIRole::Hero, FVector2D::ZeroVector, Range(-5.f, 5.f), 100.f);
		ComposeFenceSide(FVector2D(-Half.X, -Half.Y), FVector2D(Half.X, -Half.Y), false);
		ComposeFenceSide(FVector2D(-Half.X, Half.Y), FVector2D(Half.X, Half.Y), false);
		ComposeFenceSide(FVector2D(-Half.X, -Half.Y), FVector2D(-Half.X, Half.Y), false);
		// The gate: the +X side has a gap in the middle.
		ComposeFenceSide(FVector2D(Half.X, -Half.Y), FVector2D(Half.X, Half.Y), true);
		// The fallen mast lies outside the back or a side fence (sub layout), where it came down.
		const FVector2D MastSpot = M_Site.SubLayout == 0
			                           ? FVector2D(-Half.X - Range(900.f, 1100.f), Range(-300.f, 300.f))
			                           : FVector2D(Range(-400.f, 400.f), (M_Site.SubLayout == 1 ? 1.f : -1.f) * (Half.Y + Range(900.f, 1100.f)));
		const float MastYaw = (M_Site.SubLayout == 0 ? 90.f : 0.f) + Range(-15.f, 15.f);
		Place(SurvivalPOIPools::RelayFallenMast, ESurvivalPOIRole::Scavenge, MastSpot, MastYaw, 200.f);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Place(SurvivalPOIPools::RelayPole, ESurvivalPOIRole::Cover,
			      FVector2D(Half.X + 700.f + Index * 900.f, Range(-60.f, 60.f) + Index * 250.f), Range(-5.f, 5.f), 100.f);
		}
		Place(SurvivalPOIPools::VehicleParts, ESurvivalPOIRole::Scavenge, FVector2D(Range(-800.f, 800.f), Half.Y + Range(700.f, 1100.f)),
		      Range(0.f, 360.f), 300.f);
		ScatterCraters(M_Random.RandRange(1, 3), FVector2D(-2500.f, -2500.f), FVector2D(2500.f, 2500.f), 0.7f);
	}

	/** @brief Wire pieces end to end along one side; with bLeaveGate the middle piece stays out for the gate. */
	void ComposeFenceSide(const FVector2D& From, const FVector2D& To, const bool bLeaveGate)
	{
		// BP_Military_Dest_BarbedWireLong is 5.3 m long along its Y axis.
		constexpr float PieceLength = 530.f;
		const FVector2D Side = To - From;
		const int32 Pieces = FMath::Max(1, FMath::RoundToInt(Side.Size() / PieceLength));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Side.Y, Side.X)) - 90.f;
		const int32 Gap = bLeaveGate ? Pieces / 2 : INDEX_NONE;
		for (int32 Piece = 0; Piece < Pieces; ++Piece)
		{
			// The gate gap, and now and then a piece flattened by a vehicle.
			if (Piece == Gap or M_Random.FRand() < SurvivalPOIComposerConstants::FenceBreachChance)
			{
				continue;
			}
			Place(SurvivalPOIPools::RelayFence, ESurvivalPOIRole::Cover, From + Side * ((Piece + 0.5f) / Pieces), Yaw, 0.f);
		}
	}
};

void FSurvivalPOIPlanner::ComposeSite(const FSurvivalPOISite& Site, TArray<FSurvivalPOIPlacedObject>& OutObjects, const bool bAnchorOnly,
                                      FSurvivalPOIKeyObjectCount* OutKeyObjects) const
{
	FSurvivalPOIComposer Composer(*this, Site, OutObjects);
	Composer.SetAnchorOnly(bAnchorOnly);
	Composer.SetKeyObjectCount(OutKeyObjects);
	Composer.Compose();
}
