// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseLayout.h"

#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseMasterPlan.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

namespace SovietBaseLayoutConstants
{
	constexpr double OccupancyCellSize = 1500.0;
	// Master plans are drawn on a build area of this radius and scaled to the real one.
	constexpr double PlanRadius = 8000.0;

	constexpr double BuildingClearance = 120.0;
	constexpr double GatePostInset = 700.0;
	constexpr double GateOnRunTolerance = 900.0;
	constexpr double GateLeafTurnRadians = HALF_PI;
	constexpr double RightAngleDot = 0.1;
	constexpr double RunJoinTolerance = 5.0;
	// A span shorter than this share of a straight piece stays open.
	constexpr double ShortestSpanShare = 0.5;
	constexpr double BunkerWallClearance = 320.0;
	// Between a wall-attached bunker's back and the wall's outer face.
	constexpr double BunkerFlushGap = 12.0;
	constexpr double BunkerRowStep = 800.0;
	constexpr int32 BunkerRowTries = 4;
	constexpr double BunkerRingFirstStandOff = 1300.0;
	constexpr int32 BunkerRingTries = 5;
	constexpr int32 BunkerRingAngles = 28;
	constexpr double SecondRowStandOff = 2500.0;
	constexpr double PerimeterMarchStep = 150.0;
	constexpr double PerimeterMarchLimit = 12000.0;
	constexpr double GatePostSideOffset = 750.0;
	constexpr double GateBlockSideOffset = 760.0;
	constexpr double ThreatenedRunDot = 0.2;
	constexpr double RunBunkerSlideStep = 500.0;
	constexpr int32 RunBunkerSlideSteps = 3;
	constexpr int32 MaxPathPasses = 40;
	constexpr double SizeFillFullBudget = 800.0;
	constexpr double PlotScanStep = 320.0;
	constexpr double InteriorScanStep = 480.0;
	constexpr int32 TopUpCopiesFactor = 2;
	constexpr double GunScanRingStep = 1250.0;
	constexpr double GunScanSpotSpacing = 1400.0;
	constexpr double GunScanMinRadius = 1800.0;
	constexpr double ForwardPostStandOff = 1900.0;
	constexpr double ForwardPostSlide = 700.0;
	constexpr int32 ForwardPostTries = 6;
	constexpr double OutpostSandbagOffset = 900.0;
	constexpr double OutpostSandbagHalfRun = 900.0;
	constexpr double SecondRowArcRadians = 2.6;
	constexpr double SecondRowSlideRadians = 0.14;
	constexpr int32 SecondRowSlideSteps = 8;
	constexpr double SecondBeltStandOff = 1250.0;
	constexpr double SecondBeltHalfRunShare = 0.3;
	constexpr double WireStandOff = 750.0;
	constexpr double WireRowSpacing = 330.0;
	constexpr double BeltStandOff = 500.0;
	constexpr double BeltLengthShare = 0.36;
	constexpr double ParapetPostInset = 330.0;
	constexpr double ParapetPostSpacing = 1500.0;

	constexpr double RevetmentPitch = 1050.0;
	constexpr double RevetmentVehicleOffset = 230.0;
	constexpr double VehicleHalfLength = 420.0;
	constexpr double VehicleHalfWidth = 240.0;
	constexpr double ParkSlotSpareShare = 1.0;

	constexpr double UnitFootprintHalfExtent = 240.0;
	constexpr int32 FreeSpotAttempts = 16;
	constexpr double FreeSpotRadius = 1500.0;
	constexpr double StreetPostOffset = 620.0;
	constexpr int32 PatrolPointCount = 6;

	constexpr double RidgeSearchStep = 450.0;
	constexpr int32 RidgeSearchRings = 3;
	constexpr int32 RidgeSearchAngles = 8;
	constexpr double RidgeGateClearance = 1500.0;

	constexpr double CratePitch = 560.0;
	constexpr double CrateChance = 0.72;
	constexpr double MunitionsPitch = 900.0;
	constexpr double MunitionsChance = 0.8;
	constexpr double BarrelPitch = 720.0;
	constexpr double BarrelChance = 0.8;
	constexpr double WorkshopPitch = 1500.0;
	constexpr double WorkshopChance = 0.55;
	constexpr double PropClearance = 130.0;
	constexpr double HutGap = 420.0;
	constexpr double BermMargin = 260.0;
	constexpr double FlagInset = 350.0;
	constexpr int32 FlagForceGap = 9;

	// Storage yards.
	constexpr double YardFenceMargin = 180.0;
	constexpr double YardInnerMargin = 220.0;
	constexpr double YardShallowInnerMargin = 140.0;
	constexpr double YardEntranceHalfWidth = 380.0;
	constexpr double YardAisle = 320.0;
	constexpr double YardStackGap = 110.0;
	constexpr double YardStackClearance = 30.0;
	constexpr double YardFrontClear = 260.0;
	constexpr double YardWallClearance = 450.0;
	// Other buildings keep this far off a yard's fence line.
	constexpr double YardKeepOut = 150.0;
	constexpr double YardStoreRowStep = 400.0;
	constexpr double YardDeepRatio = 1.15;
	constexpr double YardStoreMinHalfDepth = 1300.0;
	// Below this half depth a yard drops its front clearance: one row of stacks along the street side is better than none.
	constexpr double YardShallowHalfDepth = 1000.0;
	constexpr double YardStreetClearance = 60.0;
	constexpr double YardPlotClearance = 150.0;
	// A store row plus two rows of stacks and their aisles, and four stacks either side of the middle path.
	constexpr double YardTargetHalfDepth = 1700.0;
	constexpr double YardTargetHalfLength = 2100.0;
	constexpr double YardStreetSampleStep = 200.0;
	// Stores beyond this stand elsewhere: a yard is for stacks first.
	constexpr int32 YardMaxStores = 3;
	constexpr double YardStoreSlide = 320.0;
	constexpr double YardNetStep = 300.0;
	constexpr double YardMinExtent = 700.0;
	constexpr double YardShrinkStep = 100.0;
	constexpr double YardPathHalfWidth = 230.0;
	constexpr double YardPathWidth = 480.0;
	constexpr double YardPathOutside = 650.0;
	// A yard aims at this many stacks; below it the fill tries smaller stack types, the other ammunition list and turned cells.
	constexpr int32 YardTargetStacks = 8;
	// Pull a side in at most this often (YardShrinkStep each) to free its fence line.
	constexpr int32 YardTightenSteps = 14;
	// Fence to fence between a fuel yard and an ammunition yard; with the inner margins no fuel stack is within 2000 of ammunition.
	constexpr double YardFuelAmmoFenceGap = 1700.0;
	// Fence to fence between two yards of one kind.
	constexpr double YardSameKindFenceGap = 300.0;
	// A store of one kind stands this far outside the fence of a yard of the other kind (stacks sit 220+ inside it).
	constexpr double YardStoreFireBreak = 1850.0;
	// Lanterns.
	// From a painted road edge; the lantern stands on the verge.
	constexpr double LanternRoadClearance = 20.0;
	constexpr double LanternVergeShare = 1.05;
	// Within 8 m of the gate middle: just inside the wall, beside the road.
	constexpr double LanternGateInset = 350.0;
	constexpr double LanternBesideRoad = 30.0;
	constexpr double LanternMainWidth = 1050.0;
	constexpr double LanternStep = 350.0;
	constexpr double LanternYardOut = 260.0;
	// Just outside the fence face, where a lane's painted verge stops short of the fence.
	constexpr double LanternYardHug = 110.0;
	constexpr double LanternYardStep = 160.0;
	constexpr double LanternJunctionStep = 120.0;
	constexpr double JunctionLanternTolerance = 80.0;

	constexpr double PadMarginShare = 0.45;
	constexpr double PadMinMargin = 70.0;

	constexpr double LaunchPointStandOff = 2200.0;
	constexpr int32 AxisSearchSteps = 18;
	constexpr double AxisJitterRadians = 0.3;
	constexpr double ApproachWeight = 2.0;
	constexpr double OpenApproachThreshold = 0.75;
}

namespace
{
	FVector2D SovietLayout_DirFromYaw(const double YawRadians)
	{
		return FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
	}

	double SovietLayout_Yaw(const FVector2D& Direction)
	{
		return FMath::Atan2(Direction.Y, Direction.X);
	}

	/** @return The smallest link length that still chains every position into one cluster. */
	double SovietLayout_ComputeChainDistance(const TArray<FVector2D>& Positions)
	{
		if (Positions.Num() < 2)
		{
			return 0.0;
		}
		TArray<bool> bLinked;
		TArray<double> Nearest;
		bLinked.Init(false, Positions.Num());
		Nearest.Init(TNumericLimits<double>::Max(), Positions.Num());
		Nearest[0] = 0.0;
		double Required = 0.0;
		for (int32 Round = 0; Round < Positions.Num(); ++Round)
		{
			int32 Pick = INDEX_NONE;
			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				if (not bLinked[Index] and (Pick == INDEX_NONE or Nearest[Index] < Nearest[Pick]))
				{
					Pick = Index;
				}
			}
			bLinked[Pick] = true;
			Required = FMath::Max(Required, Nearest[Pick]);
			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				Nearest[Index] = FMath::Min(Nearest[Index], FVector2D::Distance(Positions[Pick], Positions[Index]));
			}
		}
		return Required;
	}

	/** @brief A place a unit can stand at, with the way it faces. */
	struct FSovietPost
	{
		FVector2D Position = FVector2D::ZeroVector;
		double Yaw = 0.0;
	};

	/** @brief A building waiting for its plot. */
	struct FSovietBuildingItem
	{
		ESurvivalSovietZone Zone = ESurvivalSovietZone::Depot;
		ESurvivalSovietStructureRole Role = ESurvivalSovietStructureRole::Support;
		FEnemyResolvedEntry Entry;
		// Threat points paid; handed back when the item finds no spot.
		int32 ThreatPoints = 0;
		FString ClassName;
		bool bPlaced = false;
	};

	/** @brief How one end of a wall span finishes. */
	struct FSovietWallSpanEnd
	{
		// A rounded end piece, not a flat joint against a gate.
		bool bCap = true;
		// Cm the span reaches past the run's end point, or stops short of it when negative.
		double Extend = 0.0;
	};

	/** @brief One place along a span; the piece standing there is stretched to its length. */
	struct FSovietWallSlot
	{
		double Start = 0.0;
		double Length = 0.0;
		bool bFree = true;
	};

	/** @brief A storage yard rectangle: Front points to its street, Along runs beside it. */
	struct FSovietYardFrame
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Front = FVector2D(1.0, 0.0);
		FVector2D Along = FVector2D(0.0, 1.0);
		double HalfLength = 0.0;
		double HalfDepth = 0.0;
	};

	/** @brief A storage yard decided with the master plan: its ground is kept free of every building but its own stores. */
	struct FSovietYardPlan
	{
		FSovietYardFrame Frame;
		ESurvivalSovietYardKind Kind = ESurvivalSovietYardKind::Ammo;
		ESovietPlotDressing Dressing = ESovietPlotDressing::Crates;
		int32 PlotIndex = INDEX_NONE;
		// Set once FitYard has grown it; later yards keep their distance from this frame.
		bool bFitted = false;
		// Ammunition stores and fuel tanks of the preset that stand in this yard's back row.
		TArray<FSovietBuildingItem> Stores;
	};

	/** @brief Which stacks fill a yard and the cell they share, chosen so the yard holds as many stacks as it can. */
	struct FSovietYardStackChoice
	{
		TArray<FEnemyResolvedEntry> Stacks;
		double Width = 0.0;
		double Depth = 0.0;
		// Added to the row yaw: a quarter turn puts a stack's long side across the row.
		double YawOffset = 0.0;
		int32 Estimate = 0;
		int32 ListIndex = 0;
	};

	/** @brief One unit bought from the threat path. */
	struct FSovietUnitPurchase
	{
		const FSurvivalSovietThreatStep* Step = nullptr;
		FTrainingOption Unit;
		int32 ThreatPoints = 0;
	};

	template <typename TValue>
	TValue SovietLayout_ByStrength(const TArray<TValue>& Table, const int32 Strength, const TValue Fallback)
	{
		return Table.IsEmpty() ? Fallback : Table[FMath::Clamp(Strength, 0, Table.Num() - 1)];
	}

	/**
	 * @brief Executes a master plan. Member order matters: the builder keeps references to the random stream,
	 * occupancy grid and result declared before it.
	 */
	class FSovietLayoutContext
	{
	public:
		explicit FSovietLayoutContext(const FSurvivalSovietLayoutInput& InInput)
			: Input(InInput)
			  , Preset(*InInput.Preset)
			  , Kit(*InInput.Kit)
			  , Random(InInput.Seed)
			  , Occupancy(SovietBaseLayoutConstants::OccupancyCellSize)
			  , Builder(Random, Occupancy, Result)
			  , Unit(InInput.BuildAreaRadius / SovietBaseLayoutConstants::PlanRadius)
		{
		}

		/** @brief Runs one stage; what it placed is appended to the plan. */
		void RunStage(ESurvivalSovietPlanStage Stage, FSurvivalSovietBasePlan& OutPlan);

	private:
		const FSurvivalSovietLayoutInput& Input;
		const FSurvivalSovietBasePreset& Preset;
		const FSurvivalSovietSiteKit& Kit;
		FRandomStream Random;
		FEnemyOccupancyGrid Occupancy;
		FEnemyBaseSetupResult Result;
		FEnemyBaseSetupBuilder Builder;

		double Unit = 1.0;
		double ThreatYaw = 0.0;
		double AxisYaw = 0.0;
		FVector2D ThreatDirection = FVector2D(1.0, 0.0);
		FVector2D AxisForward = FVector2D(1.0, 0.0);
		FVector2D AxisLeft = FVector2D(0.0, 1.0);
		FSovietMasterPlan Master;
		// Placed actors already handed to the plan.
		int32 ConvertedActors = 0;

		TArray<ESurvivalSovietStructureRole> ClusterRoles;
		TArray<FVector2D> ProductionPositions;
		TArray<FSovietPost> VehiclePosts;
		TArray<FSovietPost> InfantryPosts;
		TArray<FSovietPost> CorePosts;
		TArray<FVector2D> StreetNodes;

		// What the threat budget bought; structures hand their points back when they find no spot.
		TArray<FSovietBuildingItem> EmplacedItems;
		TArray<FSovietBuildingItem> LineBunkers;
		TArray<FSovietUnitPurchase> UnitPurchases;
		TMap<const FSurvivalSovietThreatStep*, int32> StepCopies;
		TArray<FVector2D> GunScanSpots;
		TMap<int32, int32> ClusterThreatPoints;
		TMap<int32, ESurvivalSovietZone> ClusterZones;
		// Dressing that is a building and gets a pad.
		TSet<int32> DressedBuildingClusters;
		TMap<int32, FVector2D> ActorStretch;
		FSurvivalSovietWallReport WallReport;
		TArray<FSurvivalSovietPlannedWallPiece> WallPieces;
		TArray<FSurvivalSovietPlannedGate> BuiltGates;
		int32 WallSpanCount = 0;
		// Pieces laid since the last flag, over spans and runs: flags stay rare and never stand side by side, corners included.
		// Starts at zero so the first pieces of the loop, which meet its last ones, carry no flag.
		int32 PiecesSinceFlag = 0;
		TArray<FSurvivalSovietPlannedYard> Yards;
		TArray<FSovietYardPlan> YardPlans;
		// Wall slots left open by a street, a building or bad ground: pictured in the preview.
		TArray<FSurvivalSovietPlannedGate> OpenSlots;
		TSet<int32> YardStoreClusters;
		// The yard whose fence is being laid: wall pieces then count for it, not for the base wall.
		int32 ActiveYard = INDEX_NONE;
		TArray<FSurvivalSovietPlannedLantern> Lanterns;
		TArray<FString> DroppedSlots;
		int32 ThreatBudget = 0;
		int32 ThreatSpent = 0;
		int32 PointsOutsideMainLine = 0;
		int32 EmplacementSpotsUsed = 0;
		int32 ReservePointsLeft = 0;
		int32 ReservePointsSpent = 0;

		// ---- Frame ----
		FVector2D ToWorld(const FVector2D& Local) const;
		FVector2D DirectionToWorld(const FVector2D& Local) const;
		FVector2D ToLocal(const FVector2D& World) const;
		void ChooseAxis();
		void BuildMasterPlan();

		// ---- Shared helpers ----
		int32 BeginCluster(ESurvivalSovietStructureRole Role);
		TArray<FEnemyResolvedEntry> ResolveList(const TArray<FEnemyBlueprintEntry>& Entries) const;
		bool GetIsThreatened(const FVector2D& Outward) const;

		// ---- Streets and gates ----
		void PlaceStreets(FSurvivalSovietBasePlan& OutPlan);
		void PlaceGates();
		void PlaceGate(const FSovietGate& Gate);
		void PlaceGateOpening(const FSurvivalSovietWallFamily* Family, const FVector2D& Center, const FVector2D& Outward, int32 Cluster);
		bool FindRunUnderGate(const FSovietGate& Gate, FVector2D& OutCenter, FVector2D& OutOutward) const;

		// ---- Walls ----
		const FSurvivalSovietWallFamily* GetWallFamily() const;
		double GetGateHalfWidth(const FSurvivalSovietWallFamily& Family) const;
		bool GetIsClosedJoin(int32 RunIndex, bool bAtRunEnd, FVector2D& OutOtherDirection) const;
		void PlaceWalls();
		void PlaceWallRun(const FSurvivalSovietWallFamily& Family, int32 RunIndex, int32 Cluster);
		FSovietWallSpanEnd MakeRunEnd(const FSurvivalSovietWallFamily& Family, int32 RunIndex, bool bAtRunEnd) const;
		void PlaceWallSpan(const FSurvivalSovietWallFamily& Family, const FVector2D& Start, const FVector2D& End,
		                   const FVector2D& Outward, const FSovietWallSpanEnd& StartEnd, const FSovietWallSpanEnd& EndEnd, int32 Cluster);
		TArray<FSovietWallSlot> MakeWallSlots(const FSurvivalSovietWallFamily& Family, double Length, bool bStartCap, bool bEndCap) const;
		const FSurvivalSovietWallPiece& ChooseWallPiece(const FSurvivalSovietWallFamily& Family, bool bStartCap, bool bEndCap);
		void PlaceWallPiece(const FSurvivalSovietWallPiece& Piece, const FVector2D& JointStart, const FVector2D& Along,
		                    const FVector2D& Outward, double SlotLength, double Thickness, int32 Cluster);
		void PlaceWallCorners(const FSurvivalSovietWallFamily& Family, int32 Cluster);
		void PlaceOpenSlotEnds(const TArray<FSovietWallSlot>& Slots, const FVector2D& Start, const FVector2D& Walk, const FVector2D& Outward,
		                       int32 Cluster);
		bool GetIsInsidePerimeter(const FVector2D& World) const;
		double GetPerimeterDistance(const FVector2D& Direction) const;

		// ---- Buildings ----
		TArray<FSovietBuildingItem> GatherBuildings() const;
		void GatherSlots(const TArray<FSurvivalSovietStructureSlot>& Slots, ESurvivalSovietStructureRole Role,
		                 int32 MaxTier, TArray<FSovietBuildingItem>& OutItems) const;
		void PlaceBuildings();
		void PlaceZone(ESurvivalSovietZone Zone, TArray<FSovietBuildingItem>& Items);
		void FillPlot(const FSovietPlot& Plot, TArray<FSovietBuildingItem>& Items);
		bool PlaceAnywhereInPlot(const FSovietPlot& Plot, FSovietBuildingItem& Item);
		bool PlaceAnywhereInside(FSovietBuildingItem& Item);
		void NoteDropped(const FSovietBuildingItem& Item, const TCHAR* Reason);
		bool PlaceBuildingItem(FSovietBuildingItem& Item, const FVector2D& Center, double Yaw);
		double GetBuildingYaw(const FEnemyResolvedEntry& Entry, const FVector2D& Front, const FVector2D& Along,
		                      ESovietPlotFacing Facing) const;

		// ---- Threat budget ----
		void BuySignatures();
		void BuyThreatPath(bool bGarrisonOnly);
		bool GetIsStepAllowed(const FSurvivalSovietThreatStep& Step) const;
		bool GetCanBuyStep(const FSurvivalSovietThreatStep& Step, int32 Pass, bool bGarrisonOnly) const;
		bool TryBuyStep(const FSurvivalSovietThreatStep& Step, bool bTopUp);
		bool TryBuyStructure(const FSurvivalSovietThreatStep& Step);
		bool TryBuyUnit(const FSurvivalSovietThreatStep& Step, bool bTopUp);

		// ---- Vehicle park ----
		int32 CountParkedVehicles() const;
		void PlaceParkRows();
		bool PlaceRevetment(const FSovietParkRow& Row, const FVector2D& Center);

		// ---- Guns and towers ----
		void PlaceEmplacements();
		void PlaceRidgeGuns();
		void BuildGunScanSpots();
		bool PlaceOnScanSpot(FSovietBuildingItem& Item);
		TArray<FVector2D> GatherRidgeSpots() const;
		bool PlaceOnRidgeSpot(FSovietBuildingItem& Item, const FVector2D& Spot);
		void SortEmplacements();

		// ---- Perimeter ----
		void PlaceFence();
		void PlaceFenceRun(const FSovietFenceRun& Run);
		void PlaceLineBunkers();
		void SplitLineBunkers(TArray<FSovietBuildingItem*>& OutForward, TArray<FSovietBuildingItem*>& OutInner,
		                      TArray<FSovietBuildingItem*>& OutMain);
		void PlaceForwardPosts(TArray<FSovietBuildingItem*>& Bunkers);
		bool PlaceForwardPost(FSovietBuildingItem& Bunker, double LocalYaw);
		void PlaceSecondRow(TArray<FSovietBuildingItem*>& Bunkers);
		bool PlaceBunkerOutside(FSovietBuildingItem& Bunker, const FVector2D& Center, double Yaw);
		bool PlaceBunkerOnRing(FSovietBuildingItem& Bunker);
		void PlaceSecondBelts();
		void PlaceRunBunkers(const FSovietFenceRun& Run, TArray<FSovietBuildingItem*>& Bunkers);
		bool GetIsClearOfGates(const FVector2D& Spot, double HalfWidth) const;
		void GetRunGeometry(const FSovietFenceRun& Run, FVector2D& OutStart, FVector2D& OutEnd, FVector2D& OutOutward) const;
		void PlaceRunWire(const FVector2D& Start, const FVector2D& End, const FVector2D& Outward, bool bThreatened,
		                  int32 Cluster);
		void PlaceRunBelt(const FVector2D& Middle, const FVector2D& Outward, double Length, int32 Cluster);
		void PlaceBeltOnly();
		void AddParapetPosts(const FVector2D& Start, const FVector2D& End, const FVector2D& Outward);

		// ---- Dressing ----
		void DressPlots(FSurvivalSovietBasePlan& OutPlan);
		void DressPlot(const FSovietPlot& Plot);

		// ---- Storage yards ----
		static bool GetIsYardDressing(ESovietPlotDressing Dressing, ESurvivalSovietYardKind& OutKind);
		static FVector2D GetEdgeOutwardFrom(const FVector2D& Start, const FVector2D& End, const FVector2D& Center);
		const FSurvivalSovietWallFamily* FindWallFamily(ESurvivalSovietWallFamily FamilyKind) const;
		void PlanYards();
		bool GetIsInForeignYard(const FEnemyFootprint& Footprint) const;
		/** @return The yard kind a building belongs to when it is an ammunition store, supplies or a fuel tank. */
		static TOptional<ESurvivalSovietYardKind> GetStoreKind(const FSovietBuildingItem& Item);
		/** @return Whether a store being placed would stand within the fire break of a yard of the other kind. */
		bool GetIsInStoreFireBreak(const FEnemyFootprint& Footprint) const;
		// Set while PlaceBuildingItem places a store, so the footprint predicate can keep the fire break.
		TOptional<ESurvivalSovietYardKind> PlacingStoreKind;
		void TakeYardStores(TArray<FSovietBuildingItem>& Items);
		/** @return How far towards the street the stores reach, as an offset from the yard centre. */
		double PlaceYardStores(int32 PlanIndex);
		void NoteBuildingsInsideYards();
		void BuildYard(int32 PlanIndex, FSurvivalSovietBasePlan& OutPlan);
		void FenceYard(const FSurvivalSovietWallFamily& Family, const FSovietYardFrame& Frame, int32 Cluster);
		/** @brief Pulls a yard off the wall, the streets and other plots, then grows it into free ground towards its target size. */
		void FitYard(FSovietYardFrame& Frame, int32 OwnPlot) const;
		bool GetIsYardSideClear(const FSovietYardFrame& Frame, const double (&Extents)[4], int32 Side, int32 OwnPlot) const;
		/** @return Whether the point lies within the fence gap of a yard that was already grown (fire break for the other kind). */
		bool GetIsNearFittedYard(const FVector2D& Point, int32 OwnPlot, ESurvivalSovietYardKind OwnKind) const;
		bool GetIsLineOnStreet(const FVector2D& From, const FVector2D& To) const;
		void PlaceYardSide(const FSurvivalSovietWallFamily& Family, const FVector2D& Start, const FVector2D& End, const FVector2D& Outward,
		                   const FSovietWallSpanEnd& Ends, int32 Cluster);
		void PlaceYardSpan(const FSurvivalSovietWallFamily& Family, const FVector2D& Start, const FVector2D& End, const FVector2D& Outward,
		                   const FSovietWallSpanEnd& StartEnd, const FSovietWallSpanEnd& EndEnd, int32 Cluster);
		void PlaceYardCorner(const FSurvivalSovietWallFamily& Family, const FVector2D& Corner, const FVector2D& Back, const FVector2D& Next,
		                     int32 Cluster);
		/** @brief Stacks in straight rows either side of a middle path; OutRowDepths gets each filled row offset towards the street. */
		/**
		 * @brief Stacks in straight rows; OutRowDepths gets each filled row's offset along Frame.Front.
		 * @param bMiddleAisle Keep an aisle on the frame's middle line (the entrance path of a deep yard runs there).
		 */
		void FillYardRows(const FSovietYardFrame& Frame, const FSovietYardStackChoice& Choice, int32 Cluster, double FirstDepth,
		                  bool bMiddleAisle, TArray<double>& OutRowDepths);
		/** @return Row offsets along Frame.Front for cells of this depth (the same rules the fill uses). */
		static TArray<double> GetYardRowDepths(const FSovietYardFrame& Frame, double StackDepth, double FirstDepth, bool bMiddleAisle);
		static int32 GetYardStacksPerRow(const FSovietYardFrame& Frame, double StackWidth);
		/**
		 * @brief Picks the stack list, the stack types and the cell turn that put the most stacks in the yard (up to the target),
		 * preferring the plot's own list, then more stack types for variety.
		 * @param Lists The plot's own stack list first, then alternatives of the same kind.
		 */
		static FSovietYardStackChoice ChooseYardStacks(const FSovietYardFrame& Frame, const TArray<TArray<FEnemyResolvedEntry>>& Lists,
		                                               double FirstDepth, bool bMiddleAisle);
		/** @return How often a side was pulled in until every fence slot of the yard is free (the entrance excepted). */
		int32 TightenYardForFence(const FSurvivalSovietWallFamily& Family, FSovietYardFrame& Frame) const;
		int32 FindBlockedYardSide(const FSurvivalSovietWallFamily& Family, const FSovietYardFrame& Frame) const;
		int32 CountBlockedFenceSlots(const FSurvivalSovietWallFamily& Family, const FVector2D& Start, const FVector2D& End) const;
		FString DescribeFenceBlock(const FEnemyFootprint& Footprint, uint8 Mask) const;
		void CoverYardWithNets(const FSovietYardFrame& Frame, const TArray<double>& RowDepths);
		/** @return The net placed at this offset (largest first, along the rows or turned a quarter), or null when none fits. */
		const FEnemyResolvedEntry* TryPlaceYardNet(const FSovietYardFrame& Frame, const TArray<FEnemyResolvedEntry>& Nets,
		                                           const TArray<double, TInlineAllocator<2>>& Depths, double Offset, uint8 Mask,
		                                           int32 Cluster, FEnemyFootprint& OutFootprint);
		void AddYardPaths(const FSovietYardFrame& Frame, const FSovietYardFrame& RowFrame, bool bDeep, const TArray<double>& RowDepths,
		                  FSurvivalSovietBasePlan& OutPlan);

		// ---- Lanterns ----
		double GetPaintHalfWidth(ESurvivalSovietRoadKind Kind, double Width) const;
		bool GetIsOnPaintedRoad(const TArray<FSurvivalSovietPlannedRoad>& Roads, const FVector2D& Point, double Margin) const;
		bool PlaceLantern(const TArray<FSurvivalSovietPlannedRoad>& Roads, const FVector2D& Position, const TCHAR* Lights, const FVector2D& Target);
		void PlaceLanternNear(const TArray<FSurvivalSovietPlannedRoad>& Roads, const TArray<FVector2D>& Candidates, const TCHAR* Lights,
		                      const FVector2D& Target);
		void PlaceLanterns(const FSurvivalSovietBasePlan& Plan);
		void PlaceJunctionLantern(const TArray<FSurvivalSovietPlannedRoad>& Roads, int32 RoadIndex, const FVector2D& End, const FVector2D& Into);
		void FillPlotGrid(const FSovietPlot& Plot, const TArray<FEnemyBlueprintEntry>& Entries, double Pitch,
		                  double Chance);
		void FillPlotRows(const FSovietPlot& Plot, const TArray<FEnemyBlueprintEntry>& Entries);
		void PlaceBerm(const FSovietPlot& Plot);
		void PlaceParadeFlags(const FSovietPlot& Plot);

		// ---- Garrison ----
		void BuildPosts();
		void PlanGarrison(FSurvivalSovietBasePlan& OutPlan);
		void PlanGarrisonStep(const FSurvivalSovietThreatStep& Step, FSurvivalSovietBasePlan& OutPlan);
		FSovietPost TakePost(ESurvivalSovietStepKind Kind, bool bVehicle);
		bool TryTakeFrom(TArray<FSovietPost>& Posts, FSovietPost& OutPost);
		bool GetIsSpotFree(const FVector2D& Position) const;
		FSovietPost FindFreeSpot();
		void ReserveUnitSpot(const FVector2D& Position, double Yaw);

		// ---- Result ----
		FVector2D FindLaunchPoint() const;
		void ConvertNewActors(FSurvivalSovietBasePlan& OutPlan);
		void BeginLayout();
		void FinishPlan(FSurvivalSovietBasePlan& OutPlan);
		void PlacePads(FSurvivalSovietBasePlan& OutPlan);
		void ValidateBunkerRule(FSurvivalSovietBasePlan& OutPlan) const;
	};

	// ------------------------------------------------------------------
	// Frame
	// ------------------------------------------------------------------

	FVector2D FSovietLayoutContext::ToWorld(const FVector2D& Local) const
	{
		return Input.Center + (AxisForward * Local.X + AxisLeft * Local.Y) * Unit;
	}

	FVector2D FSovietLayoutContext::DirectionToWorld(const FVector2D& Local) const
	{
		return AxisForward * Local.X + AxisLeft * Local.Y;
	}

	FVector2D FSovietLayoutContext::ToLocal(const FVector2D& World) const
	{
		const FVector2D Delta = (World - Input.Center) / Unit;
		return FVector2D(FVector2D::DotProduct(Delta, AxisForward), FVector2D::DotProduct(Delta, AxisLeft));
	}

	void FSovietLayoutContext::ChooseAxis()
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D ToThreat = Input.ThreatLocation - Input.Center;
		ThreatYaw = ToThreat.IsNearlyZero() ? 0.0 : SovietLayout_Yaw(ToThreat);
		ThreatDirection = SovietLayout_DirFromYaw(ThreatYaw);

		double PreferredYaw = ThreatYaw;
		switch (GetSovietGatePreference(Preset.Archetype, Random))
		{
		case ESovietGatePreference::AwayFromThreat:
			PreferredYaw += PI;
			break;
		case ESovietGatePreference::FlankThreat:
			PreferredYaw += Random.FRand() < 0.5f ? HALF_PI : -HALF_PI;
			break;
		case ESovietGatePreference::TowardThreat:
		default:
			break;
		}
		PreferredYaw += Random.FRandRange(-AxisJitterRadians, AxisJitterRadians);

		// The gate goes on the open approach nearest to the wish; a base never opens onto a cliff.
		double BestYaw = PreferredYaw;
		double BestScore = TNumericLimits<double>::Lowest();
		for (int32 Step = 0; Step < AxisSearchSteps; ++Step)
		{
			const double Offset = (Step + 1) / 2 * (2.0 * PI / AxisSearchSteps) * (Step % 2 == 0 ? 1.0 : -1.0);
			const double Yaw = PreferredYaw + Offset;
			const double Openness = Input.Site != nullptr ? Input.Site->GetApproachOpenness(Yaw) : 1.0;
			const double Score = (Openness >= OpenApproachThreshold ? ApproachWeight : Openness)
				- FMath::Abs(Offset) / PI;
			if (Score > BestScore)
			{
				BestScore = Score;
				BestYaw = Yaw;
			}
		}
		AxisYaw = BestYaw;
		AxisForward = SovietLayout_DirFromYaw(AxisYaw);
		const double MirrorSign = Random.FRand() < 0.5f ? 1.0 : -1.0;
		AxisLeft = FVector2D(-AxisForward.Y, AxisForward.X) * MirrorSign;
	}

	void FSovietLayoutContext::BuildMasterPlan()
	{
		TArray<FVector2D> DefensePointsLocal;
		for (const FVector2D& Point : Input.DefensePoints)
		{
			DefensePointsLocal.Add(ToLocal(Point));
		}
		Master = BuildSovietMasterPlan(
			Preset.Archetype, Input.Strength,
			FMath::Clamp(Input.ThreatBudget / SovietBaseLayoutConstants::SizeFillFullBudget, 0.0, 1.0), Random, Input.Site,
			[this](const FVector2D& World) { return ToLocal(World); },
			[this](const FVector2D& Local) { return ToWorld(Local); },
			DefensePointsLocal);
	}

	// ------------------------------------------------------------------
	// Shared helpers
	// ------------------------------------------------------------------

	int32 FSovietLayoutContext::BeginCluster(const ESurvivalSovietStructureRole Role)
	{
		const int32 Cluster = Builder.BeginCluster();
		if (ClusterRoles.Num() <= Cluster)
		{
			ClusterRoles.SetNum(Cluster + 1);
		}
		ClusterRoles[Cluster] = Role;
		return Cluster;
	}

	TArray<FEnemyResolvedEntry> FSovietLayoutContext::ResolveList(const TArray<FEnemyBlueprintEntry>& Entries) const
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

	bool FSovietLayoutContext::GetIsThreatened(const FVector2D& Outward) const
	{
		return Master.bFortifyAllRuns
			or FVector2D::DotProduct(Outward, ThreatDirection) > SovietBaseLayoutConstants::ThreatenedRunDot;
	}

	// ------------------------------------------------------------------
	// Streets and gates
	// ------------------------------------------------------------------

	void FSovietLayoutContext::PlaceStreets(FSurvivalSovietBasePlan& OutPlan)
	{
		for (const FSovietStreet& Street : Master.Streets)
		{
			TArray<FVector2D> WorldPoints;
			for (const FVector2D& Point : Street.Points)
			{
				WorldPoints.Add(ToWorld(Point));
			}
			for (int32 Index = 0; Index + 1 < WorldPoints.Num(); ++Index)
			{
				// Buildings keep their clearance from the corridor, so it stops that much short of the street's edge.
				Builder.ReserveCorridor(WorldPoints[Index], WorldPoints[Index + 1],
				                        Street.Width * 0.5 - SovietBaseLayoutConstants::BuildingClearance);
				StreetNodes.Add((WorldPoints[Index] + WorldPoints[Index + 1]) * 0.5);
			}
			if (Street.Kind == ESurvivalSovietRoadKind::None or WorldPoints.Num() < 2)
			{
				continue;
			}
			FSurvivalSovietPlannedRoad& Road = OutPlan.Roads.AddDefaulted_GetRef();
			Road.Kind = Street.Kind;
			Road.Points = WorldPoints;
			Road.Width = Street.Width;
		}
	}

	void FSovietLayoutContext::PlaceGates()
	{
		for (const FSovietGate& Gate : Master.Gates)
		{
			PlaceGate(Gate);
		}
	}

	bool FSovietLayoutContext::FindRunUnderGate(const FSovietGate& Gate, FVector2D& OutCenter, FVector2D& OutOutward) const
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Center = ToWorld(Gate.Position);
		double BestDistance = GateOnRunTolerance;
		for (const FSovietFenceRun& Run : Master.Fence)
		{
			FVector2D Start;
			FVector2D End;
			FVector2D Outward;
			GetRunGeometry(Run, Start, End, Outward);
			const FVector2D Along = (End - Start).GetSafeNormal();
			const double Reach = FVector2D::DotProduct(Center - Start, Along);
			const double Across = FMath::Abs(FVector2D::DotProduct(Center - Start, Outward));
			if (Reach > 0.0 and Reach < FVector2D::Distance(Start, End) and Across < BestDistance)
			{
				BestDistance = Across;
				OutCenter = Start + Along * Reach;
				OutOutward = Outward;
			}
		}
		return BestDistance < GateOnRunTolerance;
	}

	void FSovietLayoutContext::PlaceGate(const FSovietGate& Gate)
	{
		using namespace SovietBaseLayoutConstants;
		// The wall decides where the gate stands: a plan's gate point may lie a little off its run.
		FVector2D Center = ToWorld(Gate.Position);
		FVector2D Outward = DirectionToWorld(Gate.Outward);
		const bool bOnRun = FindRunUnderGate(Gate, Center, Outward);
		const FVector2D Side(-Outward.Y, Outward.X);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		const FSurvivalSovietWallFamily* Family = bOnRun ? GetWallFamily() : nullptr;
		if (Family != nullptr and Family->Gate.GetIsSet())
		{
			// The arch spans the street on purpose, so it ignores the reserved corridor.
			const double Length = Family->Gate.Length;
			WallSpanCount += 1;
			PlaceWallPiece(Family->Gate, Center - Side * (Length * 0.5), Side, Outward, Length, Family->Thickness, Cluster);
			if (not WallPieces.IsEmpty())
			{
				WallPieces.Last().bGate = true;
				WallPieces.Last().Kind = ESurvivalSovietWallPieceKind::Gate;
			}
		}
		else
		{
			PlaceGateOpening(Family, Center, Outward, Cluster);
		}
		FSurvivalSovietPlannedGate& Built = BuiltGates.AddDefaulted_GetRef();
		Built.Center = Center;
		Built.Outward = Outward;
		Built.Width = Family != nullptr ? GetGateHalfWidth(*Family) * 2.0 : GateBlockSideOffset * 2.0;
		WallReport.Gates += 1;
		for (const double Sign : {-1.0, 1.0})
		{
			FSovietPost Post;
			Post.Position = Center - Outward * GatePostInset + Side * (Sign * GatePostSideOffset);
			Post.Yaw = SovietLayout_Yaw(Outward);
			InfantryPosts.Add(Post);
		}
	}

	void FSovietLayoutContext::PlaceGateOpening(const FSurvivalSovietWallFamily* Family, const FVector2D& Center,
	                                            const FVector2D& Outward, const int32 Cluster)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Side(-Outward.Y, Outward.X);
		const double HalfOpening = Family != nullptr ? GetGateHalfWidth(*Family) : GateBlockSideOffset;
		const double Yaw = SovietLayout_Yaw(Outward);
		const TArray<FEnemyResolvedEntry> Blocks = ResolveList(Kit.GateBlocks);
		const TArray<FEnemyResolvedEntry> Leaf = Family != nullptr and Family->GateLeaf.GetIsSet()
			                                         ? ResolveList({Family->GateLeaf.Entry})
			                                         : TArray<FEnemyResolvedEntry>();
		for (const double Sign : {-1.0, 1.0})
		{
			const FVector2D Jamb = Center + Side * (Sign * HalfOpening);
			if (not Leaf.IsEmpty())
			{
				// A swing gate stood open, pointing out of the base.
				FEnemyFootprint Footprint;
				Footprint.Center = Jamb + Outward * (Family->GateLeaf.Length * 0.5);
				Footprint.HalfExtents = FVector2D(Family->GateLeaf.Length * 0.5, Family->Thickness * 0.5);
				Footprint.YawRadians = Yaw;
				Builder.PlaceAtPivot(EEnemyPlacementCategory::Hedgehog, Leaf[0], Jamb, Yaw - GateLeafTurnRadians, Footprint, Cluster);
				continue;
			}
			FEnemyFootprint Footprint;
			Builder.PlaceCentered(EEnemyPlacementCategory::Hedgehog, Blocks, INDEX_NONE, Jamb, Yaw, 0.0,
			                      EnemySolidCategoryMask(), Cluster, Footprint);
		}
	}

	// ------------------------------------------------------------------
	// Walls: one family per base, every span fitted exactly between its ends
	// ------------------------------------------------------------------

	const FSurvivalSovietWallFamily* FSovietLayoutContext::GetWallFamily() const
	{
		return Kit.WallFamilies.FindByPredicate([this](const FSurvivalSovietWallFamily& Family)
		{
			return Family.Family == Master.WallFamily;
		});
	}

	double FSovietLayoutContext::GetGateHalfWidth(const FSurvivalSovietWallFamily& Family) const
	{
		return (Family.Gate.GetIsSet() ? Family.Gate.Length : Family.GateOpening) * 0.5;
	}

	bool FSovietLayoutContext::GetIsClosedJoin(const int32 RunIndex, const bool bAtRunEnd, FVector2D& OutOtherDirection) const
	{
		using namespace SovietBaseLayoutConstants;
		const int32 Count = Master.Fence.Num();
		const int32 OtherIndex = (RunIndex + (bAtRunEnd ? 1 : Count - 1)) % Count;
		const FSovietFenceRun& Run = Master.Fence[RunIndex];
		const FSovietFenceRun& Other = Master.Fence[OtherIndex];
		const FVector2D Point = bAtRunEnd ? Run.End : Run.Start;
		const FVector2D OtherPoint = bAtRunEnd ? Other.Start : Other.End;
		if (OtherIndex == RunIndex or FVector2D::Distance(Point, OtherPoint) > RunJoinTolerance)
		{
			return false;
		}
		// Unit vector from the shared point along the other run.
		OutOtherDirection = DirectionToWorld((bAtRunEnd ? Other.End - Other.Start : Other.Start - Other.End).GetSafeNormal());
		return true;
	}

	void FSovietLayoutContext::PlaceWalls()
	{
		const FSurvivalSovietWallFamily* Family = GetWallFamily();
		WallReport.Family = Master.WallFamily;
		if (Family == nullptr or not Family->Straight.GetIsSet())
		{
			return;
		}
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		PlaceWallCorners(*Family, Cluster);
		for (int32 RunIndex = 0; RunIndex < Master.Fence.Num(); ++RunIndex)
		{
			PlaceWallRun(*Family, RunIndex, Cluster);
		}
	}

	FSovietWallSpanEnd FSovietLayoutContext::MakeRunEnd(const FSurvivalSovietWallFamily& Family, const int32 RunIndex,
	                                                    const bool bAtRunEnd) const
	{
		using namespace SovietBaseLayoutConstants;
		FSovietWallSpanEnd SpanEnd;
		FVector2D OtherDirection;
		if (not GetIsClosedJoin(RunIndex, bAtRunEnd, OtherDirection))
		{
			return SpanEnd;
		}
		FVector2D Start;
		FVector2D End;
		FVector2D Outward;
		GetRunGeometry(Master.Fence[RunIndex], Start, End, Outward);
		const bool bRightAngle = FMath::Abs(FVector2D::DotProduct((End - Start).GetSafeNormal(), OtherDirection)) < RightAngleDot;
		if (Family.Corner.GetIsSet() and bRightAngle)
		{
			SpanEnd.bCap = false;
			SpanEnd.Extend = -Family.Corner.Length;
			return SpanEnd;
		}
		// Both walls run on to the corner point's far side, so the outer corner has no notch.
		SpanEnd.Extend = Family.Thickness * 0.5;
		return SpanEnd;
	}

	void FSovietLayoutContext::PlaceWallRun(const FSurvivalSovietWallFamily& Family, const int32 RunIndex, const int32 Cluster)
	{
		FVector2D Start;
		FVector2D End;
		FVector2D Outward;
		GetRunGeometry(Master.Fence[RunIndex], Start, End, Outward);
		FSovietWallSpanEnd StartEnd = MakeRunEnd(Family, RunIndex, false);
		FSovietWallSpanEnd EndEnd = MakeRunEnd(Family, RunIndex, true);
		// The walk keeps the outside on the right hand: the pieces have a fixed left and right end.
		const FVector2D Walk(-Outward.Y, Outward.X);
		if (FVector2D::DotProduct(End - Start, Walk) < 0.0)
		{
			Swap(Start, End);
			Swap(StartEnd, EndEnd);
		}
		const double Length = FVector2D::Distance(Start, End);
		TArray<double> GateMiddles;
		for (const FSovietGate& Gate : Master.Gates)
		{
			FVector2D Center;
			FVector2D GateOutward;
			if (FindRunUnderGate(Gate, Center, GateOutward) and FVector2D::DotProduct(GateOutward, Outward) > 0.99)
			{
				const double Reach = FVector2D::DotProduct(Center - Start, Walk);
				const bool bOnThisLine = FMath::Abs(FVector2D::DotProduct(Center - Start, Outward)) < 1.0;
				if (bOnThisLine and Reach > 0.0 and Reach < Length)
				{
					GateMiddles.Add(Reach);
				}
			}
		}
		GateMiddles.Sort();
		const double HalfGate = GetGateHalfWidth(Family);
		double From = -StartEnd.Extend;
		FSovietWallSpanEnd FromEnd = StartEnd;
		for (const double Middle : GateMiddles)
		{
			FSovietWallSpanEnd GateJoint;
			GateJoint.bCap = false;
			PlaceWallSpan(Family, Start + Walk * From, Start + Walk * (Middle - HalfGate), Outward, FromEnd, GateJoint, Cluster);
			From = Middle + HalfGate;
			FromEnd = GateJoint;
		}
		PlaceWallSpan(Family, Start + Walk * From, Start + Walk * (Length + EndEnd.Extend), Outward, FromEnd, EndEnd, Cluster);
	}

	TArray<FSovietWallSlot> FSovietLayoutContext::MakeWallSlots(const FSurvivalSovietWallFamily& Family, const double Length,
	                                                            const bool bStartCap, const bool bEndCap) const
	{
		using namespace SovietBaseLayoutConstants;
		TArray<FSovietWallSlot> Slots;
		const double StraightLength = Family.Straight.Length;
		if (Length < StraightLength * ShortestSpanShare)
		{
			return Slots;
		}
		const double FirstLength = bStartCap and Family.StartCap.GetIsSet() ? Family.StartCap.Length : StraightLength;
		const double LastLength = bEndCap and Family.EndCap.GetIsSet() ? Family.EndCap.Length : StraightLength;
		const int32 Count = FMath::Max(1, FMath::RoundToInt32(Length / StraightLength));
		const double Nominal = Count == 1 ? StraightLength : FirstLength + LastLength + (Count - 2) * StraightLength;
		const double Fit = Length / Nominal;
		double Cursor = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FSovietWallSlot& Slot = Slots.AddDefaulted_GetRef();
			const double Own = Count == 1 ? StraightLength : Index == 0 ? FirstLength : Index == Count - 1 ? LastLength : StraightLength;
			Slot.Start = Cursor;
			Slot.Length = Own * Fit;
			Cursor += Slot.Length;
		}
		return Slots;
	}

	const FSurvivalSovietWallPiece& FSovietLayoutContext::ChooseWallPiece(
		const FSurvivalSovietWallFamily& Family, const bool bStartCap, const bool bEndCap)
	{
		if (ActiveYard != INDEX_NONE)
		{
			return Family.Straight;
		}
		const FSurvivalSovietWallPiece& Plain = bStartCap and Family.StartCap.GetIsSet()
			                                        ? Family.StartCap
			                                        : bEndCap and Family.EndCap.GetIsSet()
			                                        ? Family.EndCap
			                                        : Family.Straight;
		const FSurvivalSovietWallPiece& Flag = bStartCap and Family.StartCap.GetIsSet()
			                                       ? Family.FlagStartCap
			                                       : bEndCap and Family.EndCap.GetIsSet()
			                                       ? Family.FlagEndCap
			                                       : Family.FlagStraight;
		// Always rolled, so a flag does not shift the rest of the seed's wall.
		const bool bRolledFlag = Random.FRand() < Family.FlagRate;
		// Only straights carry a flag: on the rounded end pieces they bunch up at the corners.
		// The roll keeps flags irregular; after FlagForceGap pieces without one the next straight carries a flag, so a concrete
		// loop holds its 6% floor (round 1: CF had 1 flag in 36 to 38 pieces on two plans).
		const bool bDue = bRolledFlag or PiecesSinceFlag >= SovietBaseLayoutConstants::FlagForceGap;
		const bool bFlag = bDue and Flag.GetIsSet() and not bStartCap and not bEndCap and PiecesSinceFlag >= Family.FlagMinGap;
		PiecesSinceFlag = bFlag ? 0 : PiecesSinceFlag + 1;
		WallReport.FlagPieces += bFlag ? 1 : 0;
		return bFlag ? Flag : Plain;
	}

	void FSovietLayoutContext::PlaceWallSpan(
		const FSurvivalSovietWallFamily& Family,
		const FVector2D& Start,
		const FVector2D& End,
		const FVector2D& Outward,
		const FSovietWallSpanEnd& StartEnd,
		const FSovietWallSpanEnd& EndEnd,
		const int32 Cluster)
	{
		const FVector2D Walk = (End - Start).GetSafeNormal();
		TArray<FSovietWallSlot> Slots = MakeWallSlots(Family, FVector2D::Distance(Start, End), StartEnd.bCap, EndEnd.bCap);
		const uint8 Mask = EnemySolidCategoryMask() & ~EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog);
		for (FSovietWallSlot& Slot : Slots)
		{
			FEnemyFootprint Footprint;
			Footprint.Center = Start + Walk * (Slot.Start + Slot.Length * 0.5);
			Footprint.HalfExtents = FVector2D(Slot.Length * 0.5, Family.Thickness * 0.5);
			Footprint.YawRadians = SovietLayout_Yaw(Walk);
			Slot.bFree = Builder.GetIsFootprintFree(Footprint, Mask);
			int32& Blocked = ActiveYard != INDEX_NONE ? Yards[ActiveYard].FenceGaps : WallReport.BlockedSlots;
			Blocked += Slot.bFree ? 0 : 1;
			if (not Slot.bFree and ActiveYard != INDEX_NONE)
			{
				Yards[ActiveYard].FenceGapCauses.FindOrAdd(DescribeFenceBlock(Footprint, Mask))++;
				Yards[ActiveYard].FenceGapPositions.Add(Footprint.Center);
			}
			if (not Slot.bFree and ActiveYard == INDEX_NONE)
			{
				FSurvivalSovietPlannedGate& Open = OpenSlots.AddDefaulted_GetRef();
				Open.Center = Footprint.Center;
				Open.Outward = Outward;
				Open.Width = Slot.Length;
			}
		}
		WallSpanCount += 1;
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			if (not Slots[Index].bFree)
			{
				continue;
			}
			// A gap in the wall gets the same finished end as a corner.
			const bool bFirst = Index == 0 ? StartEnd.bCap : not Slots[Index - 1].bFree;
			const bool bLast = Index == Slots.Num() - 1 ? EndEnd.bCap : not Slots[Index + 1].bFree;
			const bool bAlone = bFirst and bLast;
			// A single piece between two open slots reads as debris, not wall.
			const bool bOrphan = bAlone and Slots.Num() > 1 and not Family.StartCap.GetIsSet();
			if (bOrphan)
			{
				Slots[Index].bFree = false;
				continue;
			}
			const int32 FlagsBefore = WallReport.FlagPieces;
			const FSurvivalSovietWallPiece& Piece = ChooseWallPiece(Family, bFirst and not bAlone, bLast and not bAlone);
			PlaceWallPiece(Piece, Start + Walk * Slots[Index].Start, Walk, Outward, Slots[Index].Length, Family.Thickness, Cluster);
			if (ActiveYard == INDEX_NONE and not WallPieces.IsEmpty())
			{
				WallPieces.Last().bFlag = WallReport.FlagPieces > FlagsBefore;
				const bool bEndPiece = (bFirst or bLast) and not bAlone and (Family.StartCap.GetIsSet() or Family.EndCap.GetIsSet());
				WallPieces.Last().Kind = bEndPiece ? ESurvivalSovietWallPieceKind::End : ESurvivalSovietWallPieceKind::Straight;
			}
		}
		if (ActiveYard == INDEX_NONE and not Family.StartCap.GetIsSet())
		{
			PlaceOpenSlotEnds(Slots, Start, Walk, Outward, Cluster);
		}
	}

	void FSovietLayoutContext::PlaceOpenSlotEnds(const TArray<FSovietWallSlot>& Slots, const FVector2D& Start, const FVector2D& Walk,
	                                             const FVector2D& Outward, const int32 Cluster)
	{
		// A thin fence that stops at a street or bad ground ends in a concrete block, not in a cut panel.
		const TArray<FEnemyResolvedEntry> Blocks = ResolveList(Kit.GateBlocks);
		if (Blocks.IsEmpty())
		{
			return;
		}
		const uint8 Mask = EnemyOccupancyMask(EEnemyPlacementCategory::Bunker) | EnemyOccupancyMask(EEnemyPlacementCategory::Sandbag)
			| EnemyOccupancyMask(EEnemyPlacementCategory::BarbedWire);
		const double BlockHalf = Blocks[0].FootprintHalfExtents.GetMax();
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			if (Slots[Index].bFree)
			{
				continue;
			}
			const bool bFreeBefore = Index > 0 and Slots[Index - 1].bFree;
			const bool bFreeAfter = Index + 1 < Slots.Num() and Slots[Index + 1].bFree;
			FEnemyFootprint Footprint;
			if (bFreeBefore)
			{
				const FVector2D Joint = Start + Walk * Slots[Index].Start;
				Builder.PlaceCentered(EEnemyPlacementCategory::Hedgehog, Blocks, INDEX_NONE, Joint - Walk * BlockHalf, SovietLayout_Yaw(Outward),
				                      0.0, Mask, Cluster, Footprint);
			}
			if (bFreeAfter)
			{
				const FVector2D Joint = Start + Walk * (Slots[Index].Start + Slots[Index].Length);
				Builder.PlaceCentered(EEnemyPlacementCategory::Hedgehog, Blocks, INDEX_NONE, Joint + Walk * BlockHalf, SovietLayout_Yaw(Outward),
				                      0.0, Mask, Cluster, Footprint);
			}
		}
	}

	void FSovietLayoutContext::PlaceWallPiece(
		const FSurvivalSovietWallPiece& Piece,
		const FVector2D& JointStart,
		const FVector2D& Along,
		const FVector2D& Outward,
		const double SlotLength,
		const double Thickness,
		const int32 Cluster)
	{
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList({Piece.Entry});
		if (Resolved.IsEmpty())
		{
			return;
		}
		const double Stretch = SlotLength / Piece.Length;
		const EEnemySandbagFrontAxis FrontAxis = Resolved[0].SandbagFrontAxis;
		const double Yaw = SovietLayout_Yaw(Outward) - EnemyFrontAxisYawRadians(FrontAxis);
		const bool bFrontOnX = FrontAxis == EEnemySandbagFrontAxis::PositiveX or FrontAxis == EEnemySandbagFrontAxis::NegativeX;
		FEnemyFootprint Footprint;
		Footprint.Center = JointStart + Along * (SlotLength * 0.5);
		Footprint.HalfExtents = FVector2D(SlotLength * 0.5, Thickness * 0.5);
		Footprint.YawRadians = SovietLayout_Yaw(Along);
		const FVector2D Pivot = JointStart + Along * (Piece.PivotAlong * Stretch) + Outward * Piece.PivotAcross;
		Builder.PlaceAtPivot(EEnemyPlacementCategory::Hedgehog, Resolved[0], Pivot, Yaw, Footprint, Cluster);
		ActorStretch.Add(Result.Actors.Num() - 1, bFrontOnX ? FVector2D(1.0, Stretch) : FVector2D(Stretch, 1.0));
		if (ActiveYard != INDEX_NONE)
		{
			Yards[ActiveYard].FencePieces += 1;
			return;
		}
		FSurvivalSovietPlannedWallPiece& Record = WallPieces.AddDefaulted_GetRef();
		Record.ClassName = Piece.Entry.BlueprintClass.GetAssetName();
		Record.Span = WallSpanCount;
		Record.JointStart = JointStart;
		Record.JointEnd = JointStart + Along * SlotLength;
		Record.Stretch = Stretch;
		Record.Pivot = Pivot;
		Record.YawRadians = Yaw;
		Record.NominalLength = Piece.Length;
		WallReport.Pieces += 1;
		WallReport.MaxStretch = FMath::Max(WallReport.MaxStretch, FMath::Max(Stretch, 1.0 / Stretch));
	}

	void FSovietLayoutContext::PlaceWallCorners(const FSurvivalSovietWallFamily& Family, const int32 Cluster)
	{
		using namespace SovietBaseLayoutConstants;
		if (not Family.Corner.GetIsSet())
		{
			return;
		}
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList({Family.Corner.Entry});
		for (int32 RunIndex = 0; RunIndex < Master.Fence.Num() and not Resolved.IsEmpty(); ++RunIndex)
		{
			FVector2D Next;
			if (not GetIsClosedJoin(RunIndex, true, Next))
			{
				continue;
			}
			const FSovietFenceRun& Run = Master.Fence[RunIndex];
			const FVector2D Back = DirectionToWorld((Run.Start - Run.End).GetSafeNormal());
			if (FMath::Abs(FVector2D::DotProduct(Back, Next)) > RightAngleDot)
			{
				continue;
			}
			// Arms along local +X and -Y: the first arm is the one the other lies clockwise of.
			const FVector2D FirstArm = FVector2D::CrossProduct(Back, Next) < 0.0 ? Back : Next;
			const FVector2D SecondArm = FVector2D::CrossProduct(Back, Next) < 0.0 ? Next : Back;
			const FVector2D Point = ToWorld(Run.End);
			FEnemyFootprint Footprint;
			Footprint.Center = Point + (FirstArm + SecondArm) * (Family.Corner.Length * 0.5);
			Footprint.HalfExtents = FVector2D(Family.Corner.Length * 0.5, Family.Corner.Length * 0.5);
			Footprint.YawRadians = SovietLayout_Yaw(FirstArm);
			if (not Builder.GetIsFootprintFree(Footprint, EnemySolidCategoryMask()))
			{
				WallReport.BlockedSlots += 1;
				continue;
			}
			Builder.PlaceAtPivot(EEnemyPlacementCategory::Hedgehog, Resolved[0], Point, Footprint.YawRadians, Footprint, Cluster);
			WallReport.Corners += 1;
			FSurvivalSovietPlannedWallPiece& Record = WallPieces.AddDefaulted_GetRef();
			Record.ClassName = Family.Corner.Entry.BlueprintClass.GetAssetName();
			Record.Kind = ESurvivalSovietWallPieceKind::Corner;
			Record.Span = INDEX_NONE;
			Record.Pivot = Point;
			Record.YawRadians = Footprint.YawRadians;
			Record.NominalLength = Family.Corner.Length;
			Record.JointStart = Point + FirstArm * Family.Corner.Length;
			Record.JointEnd = Point + SecondArm * Family.Corner.Length;
		}
	}

	bool FSovietLayoutContext::GetIsInsidePerimeter(const FVector2D& World) const
	{
		const FVector2D Local = ToLocal(World);
		const TArray<FVector2D>& Line = Master.PerimeterLine;
		if (Line.Num() < 3)
		{
			return Local.Size() < Master.OuterRadius;
		}
		bool bInside = false;
		for (int32 Index = 0, Previous = Line.Num() - 1; Index < Line.Num(); Previous = Index++)
		{
			const FVector2D& A = Line[Index];
			const FVector2D& B = Line[Previous];
			const bool bCrosses = (A.Y > Local.Y) != (B.Y > Local.Y)
				and Local.X < (B.X - A.X) * (Local.Y - A.Y) / (B.Y - A.Y) + A.X;
			bInside = bCrosses ? not bInside : bInside;
		}
		return bInside;
	}

	double FSovietLayoutContext::GetPerimeterDistance(const FVector2D& Direction) const
	{
		using namespace SovietBaseLayoutConstants;
		double Distance = 0.0;
		while (Distance < PerimeterMarchLimit and GetIsInsidePerimeter(Input.Center + Direction * Distance))
		{
			Distance += PerimeterMarchStep;
		}
		return Distance;
	}

	// ------------------------------------------------------------------
	// Buildings
	// ------------------------------------------------------------------

	void FSovietLayoutContext::GatherSlots(
		const TArray<FSurvivalSovietStructureSlot>& Slots,
		const ESurvivalSovietStructureRole Role,
		const int32 MaxTier,
		TArray<FSovietBuildingItem>& OutItems) const
	{
		for (const FSurvivalSovietStructureSlot& Slot : Slots)
		{
			if (Slot.ProductionTier > MaxTier)
			{
				continue;
			}
			const int32 Count = FSurvivalSovietBaseLayout::GetSlotCount(Slot, Input.Strength, Input.Scaling.Difficulty);
			const TArray<FEnemyResolvedEntry> Resolved = ResolveList({Slot.Building});
			if (Resolved.IsEmpty())
			{
				continue;
			}
			for (int32 Copy = 0; Copy < Count; ++Copy)
			{
				FSovietBuildingItem& Item = OutItems.AddDefaulted_GetRef();
				Item.Zone = Slot.Zone;
				Item.Role = Role;
				Item.Entry = Resolved[0];
				Item.ClassName = Slot.Building.BlueprintClass.GetAssetName();
			}
		}
	}

	TArray<FSovietBuildingItem> FSovietLayoutContext::GatherBuildings() const
	{
		TArray<FSovietBuildingItem> Items;
		GatherSlots(Preset.ProductionBuildings, ESurvivalSovietStructureRole::Production,
		            Input.Scaling.MaxProductionTier, Items);
		GatherSlots(Preset.SupportBuildings, ESurvivalSovietStructureRole::Support, TNumericLimits<int32>::Max(), Items);
		return Items;
	}

	void FSovietLayoutContext::PlaceBuildings()
	{
		TArray<FSovietBuildingItem> Items = GatherBuildings();
		TakeYardStores(Items);
		const ESurvivalSovietZone Order[] = {
			ESurvivalSovietZone::Command, ESurvivalSovietZone::Workshop, ESurvivalSovietZone::Barracks,
			ESurvivalSovietZone::Fuel, ESurvivalSovietZone::Depot
		};
		// Production first, across all zones: a store must never take the ground a declared factory needs.
		for (const ESurvivalSovietStructureRole Role : {
			     ESurvivalSovietStructureRole::Production, ESurvivalSovietStructureRole::Support
		     })
		{
			TArray<FSovietBuildingItem> RoleItems = Items.FilterByPredicate([Role](const FSovietBuildingItem& Item)
			{
				return Item.Role == Role;
			});
			for (const ESurvivalSovietZone Zone : Order)
			{
				PlaceZone(Zone, RoleItems);
			}
		}
	}

	void FSovietLayoutContext::NoteDropped(const FSovietBuildingItem& Item, const TCHAR* Reason)
	{
		DroppedSlots.Add(FString::Printf(TEXT("%s (%s)"), *Item.ClassName, Reason));
	}

	void FSovietLayoutContext::PlaceZone(const ESurvivalSovietZone Zone, TArray<FSovietBuildingItem>& Items)
	{
		TArray<FSovietBuildingItem> ZoneItems = Items.FilterByPredicate([Zone](const FSovietBuildingItem& Item)
		{
			return Item.Zone == Zone;
		});
		if (ZoneItems.IsEmpty())
		{
			return;
		}
		// Big buildings first: they decide the rows.
		ZoneItems.StableSort([](const FSovietBuildingItem& Left, const FSovietBuildingItem& Right)
		{
			return Left.Entry.FootprintHalfExtents.SizeSquared() > Right.Entry.FootprintHalfExtents.SizeSquared();
		});
		for (const FSovietPlot& Plot : Master.Plots)
		{
			if (Plot.Zone == Zone)
			{
				FillPlot(Plot, ZoneItems);
			}
		}
		// Whatever did not fit moves to any plot of a non-fuel zone, then to any free ground inside the base.
		for (const FSovietPlot& Plot : Master.Plots)
		{
			if (Plot.Zone != Zone and Plot.Zone != ESurvivalSovietZone::Fuel)
			{
				FillPlot(Plot, ZoneItems);
			}
		}
		for (FSovietBuildingItem& Item : ZoneItems)
		{
			if (not Item.bPlaced and not PlaceAnywhereInside(Item))
			{
				NoteDropped(Item, TEXT("no free ground"));
			}
		}
	}

	double FSovietLayoutContext::GetBuildingYaw(
		const FEnemyResolvedEntry& Entry,
		const FVector2D& Front,
		const FVector2D& Along,
		const ESovietPlotFacing Facing) const
	{
		const bool bLongOnLocalX = Entry.FootprintHalfExtents.X >= Entry.FootprintHalfExtents.Y;
		if (Facing == ESovietPlotFacing::LongSideToStreet)
		{
			return bLongOnLocalX ? SovietLayout_Yaw(Along) : SovietLayout_Yaw(Front);
		}
		// Rus huts and barracks have their door on the local -X end.
		return bLongOnLocalX ? SovietLayout_Yaw(-Front) : SovietLayout_Yaw(Along);
	}

	bool FSovietLayoutContext::PlaceBuildingItem(FSovietBuildingItem& Item, const FVector2D& Center, const double Yaw)
	{
		using namespace SovietBaseLayoutConstants;
		const int32 Cluster = BeginCluster(Item.Role);
		ClusterThreatPoints.Add(Cluster, Item.ThreatPoints);
		ClusterZones.Add(Cluster, Item.Zone);
		FEnemyFootprint Footprint;
		const TArray<FEnemyResolvedEntry> Single = {Item.Entry};
		// A store keeps the fire break from the yards of the other kind wherever it stands (the predicate reads this).
		PlacingStoreKind = GetStoreKind(Item);
		const bool bPlaced = Builder.PlaceCentered(EEnemyPlacementCategory::Bunker, Single, 0, Center, Yaw, BuildingClearance,
		                                           EnemySolidCategoryMask(), Cluster, Footprint);
		PlacingStoreKind.Reset();
		if (not bPlaced)
		{
			return false;
		}
		Item.bPlaced = true;
		if (Item.Role == ESurvivalSovietStructureRole::Production)
		{
			ProductionPositions.Add(Footprint.Center);
		}
		return true;
	}

	void FSovietLayoutContext::FillPlot(const FSovietPlot& Plot, TArray<FSovietBuildingItem>& Items)
	{
		const FVector2D Center = ToWorld(Plot.Center);
		const FVector2D Front = DirectionToWorld(Plot.Front).GetSafeNormal();
		const FVector2D Along(-Front.Y, Front.X);
		const double HalfLength = Plot.HalfLength * Unit;
		const double HalfDepth = Plot.HalfDepth * Unit;

		double RowFront = HalfDepth;
		TArray<int32> Waiting;
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (not Items[Index].bPlaced)
			{
				Waiting.Add(Index);
			}
		}
		while (not Waiting.IsEmpty() and RowFront > -HalfDepth)
		{
			// One row: everything that still fits side by side, centred on the plot.
			TArray<int32> Row;
			TArray<double> Yaws;
			TArray<double> Widths;
			double UsedLength = 0.0;
			double RowDepth = 0.0;
			for (int32 WaitIndex = 0; WaitIndex < Waiting.Num();)
			{
				const FEnemyResolvedEntry& Entry = Items[Waiting[WaitIndex]].Entry;
				const double Yaw = GetBuildingYaw(Entry, Front, Along, Plot.Facing);
				FEnemyFootprint Rotated;
				Rotated.HalfExtents = Entry.FootprintHalfExtents;
				Rotated.YawRadians = Yaw;
				const double Width = 2.0 * Rotated.SupportRadius(Along);
				const double Depth = 2.0 * Rotated.SupportRadius(Front);
				const double NewLength = UsedLength + (Row.IsEmpty() ? 0.0 : Plot.Gap) + Width;
				if (NewLength > 2.0 * HalfLength or RowFront - Depth < -HalfDepth)
				{
					++WaitIndex;
					continue;
				}
				UsedLength = NewLength;
				RowDepth = FMath::Max(RowDepth, Depth);
				Row.Add(Waiting[WaitIndex]);
				Yaws.Add(Yaw);
				Widths.Add(Width);
				Waiting.RemoveAt(WaitIndex);
			}
			if (Row.IsEmpty())
			{
				return;
			}
			double Cursor = -UsedLength * 0.5;
			for (int32 RowIndex = 0; RowIndex < Row.Num(); ++RowIndex)
			{
				const FVector2D ItemCenter = Center + Along * (Cursor + Widths[RowIndex] * 0.5)
					+ Front * (RowFront - RowDepth * 0.5);
				PlaceBuildingItem(Items[Row[RowIndex]], ItemCenter, Yaws[RowIndex]);
				Cursor += Widths[RowIndex] + Plot.Gap;
			}
			RowFront -= RowDepth + Plot.Gap;
		}
	}

	bool FSovietLayoutContext::PlaceAnywhereInPlot(const FSovietPlot& Plot, FSovietBuildingItem& Item)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Center = ToWorld(Plot.Center);
		const FVector2D Front = DirectionToWorld(Plot.Front).GetSafeNormal();
		const FVector2D Along(-Front.Y, Front.X);
		const double Yaw = GetBuildingYaw(Item.Entry, Front, Along, Plot.Facing);
		FEnemyFootprint Rotated;
		Rotated.HalfExtents = Item.Entry.FootprintHalfExtents;
		Rotated.YawRadians = Yaw;
		const double HalfWidth = Rotated.SupportRadius(Along);
		const double HalfDepth = Rotated.SupportRadius(Front);
		const double RoomAlong = Plot.HalfLength * Unit - HalfWidth;
		const double RoomDeep = Plot.HalfDepth * Unit - HalfDepth;
		if (RoomAlong < 0.0 or RoomDeep < 0.0)
		{
			return false;
		}
		const int32 Columns = FMath::FloorToInt32(RoomAlong / PlotScanStep);
		const int32 Rows = FMath::FloorToInt32(2.0 * RoomDeep / PlotScanStep);
		for (int32 Row = 0; Row <= Rows; ++Row)
		{
			const FVector2D RowCenter = Center + Front * (RoomDeep - Row * PlotScanStep);
			for (int32 Column = 0; Column <= 2 * Columns; ++Column)
			{
				// Centre first, then outward to both sides.
				const double Offset = ((Column + 1) / 2) * PlotScanStep * (Column % 2 == 0 ? 1.0 : -1.0);
				if (PlaceBuildingItem(Item, RowCenter + Along * Offset, Yaw))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool FSovietLayoutContext::PlaceAnywhereInside(FSovietBuildingItem& Item)
	{
		using namespace SovietBaseLayoutConstants;
		const double Radius = Master.InteriorRadius;
		const int32 Steps = FMath::FloorToInt32(Radius / InteriorScanStep);
		// Rings from the middle outward; the building lies along the base's own axis.
		for (int32 Ring = 0; Ring <= Steps; ++Ring)
		{
			const double RingRadius = Ring * InteriorScanStep;
			const int32 Angles = FMath::Max(1, FMath::CeilToInt32(2.0 * PI * RingRadius / InteriorScanStep));
			for (int32 Angle = 0; Angle < Angles; ++Angle)
			{
				const double Yaw = 2.0 * PI * Angle / Angles;
				const FVector2D Local = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)) * RingRadius;
				if (PlaceBuildingItem(Item, ToWorld(Local), SovietLayout_Yaw(AxisForward)))
				{
					return true;
				}
			}
		}
		return false;
	}

	// ------------------------------------------------------------------
	// Threat budget
	// ------------------------------------------------------------------

	bool FSovietLayoutContext::GetIsStepAllowed(const FSurvivalSovietThreatStep& Step) const
	{
		return Input.Strength >= Step.MinStrength and Input.Scaling.MaxProductionTier >= Step.RequiredProductionTier
			and static_cast<uint8>(Input.Scaling.Difficulty) >= static_cast<uint8>(Step.MinDifficulty);
	}

	bool FSovietLayoutContext::GetCanBuyStep(const FSurvivalSovietThreatStep& Step, const int32 Pass,
	                                         const bool bGarrisonOnly) const
	{
		using namespace SovietBaseLayoutConstants;
		const bool bGarrison = Step.Kind != ESurvivalSovietStepKind::Emplacement
			and Step.Kind != ESurvivalSovietStepKind::LineBunker;
		if ((bGarrisonOnly and not bGarrison) or Pass < Step.FirstPass or not GetIsStepAllowed(Step))
		{
			return false;
		}
		const int32 CopyLimit = bGarrisonOnly ? Step.MaxCopies * TopUpCopiesFactor : Step.MaxCopies;
		return StepCopies.FindRef(&Step) < CopyLimit;
	}

	void FSovietLayoutContext::BuySignatures()
	{
		for (const FSurvivalSovietThreatStep& Step : Preset.ThreatPath)
		{
			if (Step.bSignature and Step.MinDifficulty == Input.Scaling.Difficulty
				and Input.Strength >= Step.SignatureMinStrength
				and Input.Scaling.MaxProductionTier >= Step.RequiredProductionTier)
			{
				TryBuyStep(Step, false);
			}
		}
	}

	bool FSovietLayoutContext::TryBuyStructure(const FSurvivalSovietThreatStep& Step)
	{
		if (Step.ThreatPoints > ThreatBudget - ThreatSpent)
		{
			return false;
		}
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList({Step.Building});
		if (Resolved.IsEmpty())
		{
			return false;
		}
		const bool bBunker = Step.Kind == ESurvivalSovietStepKind::LineBunker;
		FSovietBuildingItem& Item = bBunker ? LineBunkers.AddDefaulted_GetRef() : EmplacedItems.AddDefaulted_GetRef();
		Item.Zone = Step.Zone;
		Item.Role = bBunker
			            ? ESurvivalSovietStructureRole::PerimeterBunker
			            : (Step.Zone == ESurvivalSovietZone::Watch
				               ? ESurvivalSovietStructureRole::Support
				               : ESurvivalSovietStructureRole::GunPit);
		Item.Entry = Resolved[0];
		Item.ThreatPoints = Step.ThreatPoints;
		Item.ClassName = Step.Building.BlueprintClass.GetAssetName();
		ThreatSpent += Step.ThreatPoints;
		return true;
	}

	bool FSovietLayoutContext::TryBuyUnit(const FSurvivalSovietThreatStep& Step, const bool bTopUp)
	{
		const bool bReserve = Step.Kind == ESurvivalSovietStepKind::Reserve;
		const int32 Ceiling = FMath::Min3(ThreatBudget - ThreatSpent, Input.Scaling.MaxUnitPoints,
		                                  bReserve ? ReservePointsLeft : TNumericLimits<int32>::Max());
		const int32 Floor = bTopUp ? Input.Scaling.MinTopUpUnitPoints : 0;
		TArray<FTrainingOption> Affordable;
		for (const FTrainingOption& Option : Step.UnitOptions)
		{
			const int32 Points = FSurvivalSovietBaseLayout::GetUnitThreatPoints(Option);
			if (Points <= Ceiling and Points >= Floor)
			{
				Affordable.Add(Option);
			}
		}
		if (Affordable.IsEmpty())
		{
			return false;
		}
		FSovietUnitPurchase& Purchase = UnitPurchases.AddDefaulted_GetRef();
		Purchase.Step = &Step;
		Purchase.Unit = Affordable[Random.RandRange(0, Affordable.Num() - 1)];
		Purchase.ThreatPoints = FSurvivalSovietBaseLayout::GetUnitThreatPoints(Purchase.Unit);
		ThreatSpent += Purchase.ThreatPoints;
		if (bReserve)
		{
			ReservePointsLeft -= Purchase.ThreatPoints;
			ReservePointsSpent += Purchase.ThreatPoints;
		}
		return true;
	}

	bool FSovietLayoutContext::TryBuyStep(const FSurvivalSovietThreatStep& Step, const bool bTopUp)
	{
		const bool bBought = Step.Kind == ESurvivalSovietStepKind::Emplacement
		                     or Step.Kind == ESurvivalSovietStepKind::LineBunker
			                     ? TryBuyStructure(Step)
			                     : TryBuyUnit(Step, bTopUp);
		if (bBought)
		{
			StepCopies.FindOrAdd(&Step)++;
		}
		return bBought;
	}

	void FSovietLayoutContext::BuyThreatPath(const bool bGarrisonOnly)
	{
		using namespace SovietBaseLayoutConstants;
		for (int32 Pass = 0; Pass < MaxPathPasses; ++Pass)
		{
			bool bBoughtAny = false;
			bool bStepsAhead = false;
			for (const FSurvivalSovietThreatStep& Step : Preset.ThreatPath)
			{
				bStepsAhead = bStepsAhead or Step.FirstPass > Pass;
				if (GetCanBuyStep(Step, Pass, bGarrisonOnly))
				{
					bBoughtAny = TryBuyStep(Step, bGarrisonOnly) or bBoughtAny;
				}
			}
			if (not bBoughtAny and not bStepsAhead)
			{
				return;
			}
		}
	}

	// ------------------------------------------------------------------
	// Vehicle park
	// ------------------------------------------------------------------

	int32 FSovietLayoutContext::CountParkedVehicles() const
	{
		int32 Vehicles = 0;
		for (const FSovietUnitPurchase& Purchase : UnitPurchases)
		{
			const bool bVehicle = Purchase.Unit.UnitType != EAllUnitType::UNType_Squad;
			Vehicles += bVehicle and Purchase.Step->Kind != ESurvivalSovietStepKind::PatrolGarrison ? 1 : 0;
		}
		return Vehicles;
	}

	bool FSovietLayoutContext::PlaceRevetment(const FSovietParkRow& Row, const FVector2D& Center)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Front = DirectionToWorld(Row.Front).GetSafeNormal();
		const double Yaw = SovietLayout_Yaw(Front);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		const TArray<FEnemyResolvedEntry> Revetments = ResolveList(Kit.Revetments);
		FEnemyFootprint Footprint;
		if (not Builder.PlaceCentered(EEnemyPlacementCategory::Sandbag, Revetments, INDEX_NONE, Center, Yaw, 0.0,
		                              EnemySolidCategoryMask(), Cluster, Footprint))
		{
			return false;
		}
		FSovietPost Post;
		Post.Position = Center + Front * RevetmentVehicleOffset;
		Post.Yaw = Yaw;
		VehiclePosts.Add(Post);
		// Keeps props out of the bay; the vehicle itself spawns later.
		FEnemyFootprint Bay;
		Bay.Center = Post.Position;
		Bay.HalfExtents = FVector2D(VehicleHalfLength, VehicleHalfWidth);
		Bay.YawRadians = Yaw;
		Occupancy.Add(Bay, EEnemyPlacementCategory::Decorator);
		return true;
	}

	void FSovietLayoutContext::PlaceParkRows()
	{
		using namespace SovietBaseLayoutConstants;
		// One bay per vehicle: empty sandbag horseshoes read as a mistake.
		int32 BaysLeft = FMath::CeilToInt32(CountParkedVehicles() * ParkSlotSpareShare);
		for (const FSovietParkRow& Row : Master.ParkRows)
		{
			const FVector2D Start = ToWorld(Row.Start);
			const FVector2D End = ToWorld(Row.End);
			const double Length = FVector2D::Distance(Start, End);
			const int32 Bays = FMath::FloorToInt32(Length / RevetmentPitch) + 1;
			const FVector2D Step = (End - Start).GetSafeNormal() * RevetmentPitch;
			for (int32 Bay = 0; Bay < Bays and BaysLeft > 0; ++Bay)
			{
				BaysLeft -= PlaceRevetment(Row, Start + Step * Bay) ? 1 : 0;
			}
		}
	}

	// ------------------------------------------------------------------
	// Guns and towers
	// ------------------------------------------------------------------

	void FSovietLayoutContext::SortEmplacements()
	{
		// Direct fire and watch: threatened side first. Artillery: farthest from the threat first.
		const FVector2D LocalThreat = ToLocal(Input.Center + ThreatDirection);
		Master.Emplacements.StableSort([LocalThreat](const FSovietEmplacement& Left, const FSovietEmplacement& Right)
		{
			const double LeftDot = FVector2D::DotProduct(Left.Position, LocalThreat);
			const double RightDot = FVector2D::DotProduct(Right.Position, LocalThreat);
			return Left.Zone == ESurvivalSovietZone::Artillery ? LeftDot < RightDot : LeftDot > RightDot;
		});
	}

	TArray<FVector2D> FSovietLayoutContext::GatherRidgeSpots() const
	{
		const double MaxDistance = Input.BuildAreaRadius * Preset.MaxDefensePointDistanceFraction;
		TArray<FVector2D> Spots;
		auto AddSpot = [this, &Spots, MaxDistance](const FVector2D& Spot)
		{
			if (FVector2D::Distance(Spot, Input.Center) <= MaxDistance)
			{
				Spots.Add(Spot);
			}
		};
		for (const FVector2D& Point : Input.DefensePoints)
		{
			AddSpot(Point);
		}
		if (Input.Site != nullptr)
		{
			for (const FVector2D& Overlook : Input.Site->GetOverlooks())
			{
				AddSpot(Overlook);
			}
		}
		const FVector2D Center = Input.Center;
		const FVector2D Threat = ThreatDirection;
		Spots.StableSort([Center, Threat](const FVector2D& Left, const FVector2D& Right)
		{
			return FVector2D::DotProduct((Left - Center).GetSafeNormal(), Threat)
				> FVector2D::DotProduct((Right - Center).GetSafeNormal(), Threat);
		});
		return Spots;
	}

	bool FSovietLayoutContext::PlaceOnRidgeSpot(FSovietBuildingItem& Item, const FVector2D& Spot)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Outward = (Spot - Input.Center).GetSafeNormal();
		// Guns on the player's side watch the player; the others cover their own flank of the base.
		const FVector2D Facing = FVector2D::DotProduct(Outward, ThreatDirection) > 0.0 ? ThreatDirection : Outward;
		for (int32 Ring = 0; Ring <= RidgeSearchRings; ++Ring)
		{
			const int32 Angles = Ring == 0 ? 1 : RidgeSearchAngles;
			for (int32 Angle = 0; Angle < Angles; ++Angle)
			{
				const FVector2D Offset = SovietLayout_DirFromYaw(2.0 * PI * Angle / Angles) * (Ring * RidgeSearchStep);
				if (PlaceBuildingItem(Item, Spot + Offset, SovietLayout_Yaw(Facing)))
				{
					return true;
				}
			}
		}
		return false;
	}

	void FSovietLayoutContext::PlaceRidgeGuns()
	{
		TArray<FSovietBuildingItem>& Items = EmplacedItems;
		const TArray<FVector2D> Spots = GatherRidgeSpots();
		int32 Direct = 0;
		for (const FSovietBuildingItem& Item : Items)
		{
			Direct += Item.Role == ESurvivalSovietStructureRole::GunPit
			          and Item.Zone != ESurvivalSovietZone::Artillery ? 1 : 0;
		}
		int32 GunsLeft = FMath::CeilToInt32(Direct * Master.RidgeGunShare);
		int32 NextSpot = 0;
		for (FSovietBuildingItem& Item : Items)
		{
			if (Item.Role != ESurvivalSovietStructureRole::GunPit or Item.Zone == ESurvivalSovietZone::Artillery)
			{
				continue;
			}
			while (GunsLeft > 0 and not Item.bPlaced and NextSpot < Spots.Num())
			{
				PlaceOnRidgeSpot(Item, Spots[NextSpot++]);
			}
			GunsLeft -= Item.bPlaced ? 1 : 0;
		}
	}

	void FSovietLayoutContext::BuildGunScanSpots()
	{
		using namespace SovietBaseLayoutConstants;
		// Spots inside the perimeter on rings; capacity grows with the size of the plan.
		for (double Radius = Master.InteriorRadius; Radius >= GunScanMinRadius; Radius -= GunScanRingStep)
		{
			const int32 Spots = FMath::FloorToInt32(2.0 * PI * Radius / GunScanSpotSpacing);
			const double Phase = Random.FRandRange(0.0, 2.0 * PI);
			for (int32 Spot = 0; Spot < Spots; ++Spot)
			{
				const double Yaw = Phase + 2.0 * PI * Spot / Spots;
				GunScanSpots.Add(FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)) * Radius);
			}
		}
	}

	bool FSovietLayoutContext::PlaceOnScanSpot(FSovietBuildingItem& Item)
	{
		const FVector2D LocalThreat = ToLocal(Input.Center + ThreatDirection);
		const bool bBehindTheLine = Item.Zone == ESurvivalSovietZone::Artillery;
		// Artillery rear first, everything else threatened side first.
		TArray<FVector2D> Spots = GunScanSpots;
		Spots.StableSort([LocalThreat, bBehindTheLine](const FVector2D& Left, const FVector2D& Right)
		{
			const double LeftDot = FVector2D::DotProduct(Left.GetSafeNormal(), LocalThreat);
			const double RightDot = FVector2D::DotProduct(Right.GetSafeNormal(), LocalThreat);
			return bBehindTheLine ? LeftDot < RightDot : LeftDot > RightDot;
		});
		for (const FVector2D& Spot : Spots)
		{
			const double Yaw = SovietLayout_Yaw(DirectionToWorld(Spot.GetSafeNormal()));
			if (PlaceBuildingItem(Item, ToWorld(Spot), Yaw))
			{
				return true;
			}
		}
		return false;
	}

	void FSovietLayoutContext::PlaceEmplacements()
	{
		PlaceRidgeGuns();
		SortEmplacements();
		for (FSovietBuildingItem& Item : EmplacedItems)
		{
			if (Item.bPlaced)
			{
				PointsOutsideMainLine += Item.ThreatPoints;
				++EmplacementSpotsUsed;
				continue;
			}
			for (const FSovietEmplacement& Emplacement : Master.Emplacements)
			{
				if (Emplacement.Zone != Item.Zone)
				{
					continue;
				}
				const double Yaw = SovietLayout_Yaw(DirectionToWorld(Emplacement.Facing));
				if (PlaceBuildingItem(Item, ToWorld(Emplacement.Position), Yaw))
				{
					break;
				}
			}
			if (not Item.bPlaced)
			{
				PlaceOnScanSpot(Item);
			}
			EmplacementSpotsUsed += Item.bPlaced ? 1 : 0;
			if (not Item.bPlaced)
			{
				ThreatSpent -= Item.ThreatPoints;
				NoteDropped(Item, TEXT("no spot; points returned"));
			}
		}
	}

	// ------------------------------------------------------------------
	// Perimeter
	// ------------------------------------------------------------------

	void FSovietLayoutContext::PlaceFence()
	{
		PlaceLineBunkers();
		for (const FSovietFenceRun& Run : Master.Fence)
		{
			PlaceFenceRun(Run);
		}
		PlaceSecondBelts();
	}

	void FSovietLayoutContext::SplitLineBunkers(TArray<FSovietBuildingItem*>& OutForward,
	                                            TArray<FSovietBuildingItem*>& OutInner,
	                                            TArray<FSovietBuildingItem*>& OutMain)
	{
		TArray<FSovietBuildingItem*> ByPoints;
		for (FSovietBuildingItem& Bunker : LineBunkers)
		{
			ByPoints.Add(&Bunker);
		}
		ByPoints.StableSort([](const FSovietBuildingItem& Left, const FSovietBuildingItem& Right)
		{
			return Left.ThreatPoints < Right.ThreatPoints;
		});
		// Light bunkers go forward as outposts, the heaviest form a second row beyond the wall, the rest hold the main line.
		const int32 Forward = FMath::Min(ByPoints.Num(), SovietLayout_ByStrength(Input.Scaling.ForwardPostsByStrength, Input.Strength, 0));
		const int32 Inner = FMath::Min(ByPoints.Num() - Forward,
		                               SovietLayout_ByStrength(Input.Scaling.SecondRowBunkersByStrength, Input.Strength, 0));
		for (int32 Index = 0; Index < ByPoints.Num(); ++Index)
		{
			if (Index < Forward)
			{
				OutForward.Add(ByPoints[Index]);
			}
			else if (Index >= ByPoints.Num() - Inner)
			{
				OutInner.Add(ByPoints[Index]);
			}
			else
			{
				OutMain.Add(ByPoints[Index]);
			}
		}
	}

	bool FSovietLayoutContext::PlaceForwardPost(FSovietBuildingItem& Bunker, const double WorldYaw)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Outward = SovietLayout_DirFromYaw(WorldYaw);
		const FVector2D Side(-Outward.Y, Outward.X);
		const double Distance = GetPerimeterDistance(Outward) + ForwardPostStandOff * Unit;
		for (int32 Try = 0; Try < ForwardPostTries; ++Try)
		{
			const double Slide = ((Try + 1) / 2) * ForwardPostSlide * (Try % 2 == 0 ? 1.0 : -1.0);
			const FVector2D Position = Input.Center + Outward * Distance + Side * Slide;
			if (not PlaceBunkerOutside(Bunker, Position, WorldYaw))
			{
				continue;
			}
			const FVector2D Front = Position + Outward * OutpostSandbagOffset;
			const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
			Builder.PlaceSegmentRun(EEnemyPlacementCategory::Sandbag, ResolveList(Kit.SandbagRuns),
			                        Front - Side * OutpostSandbagHalfRun, Front + Side * OutpostSandbagHalfRun, Outward,
			                        EnemySolidCategoryMask(), false, Cluster);
			FSovietPost Post;
			Post.Position = Position + Side * OutpostSandbagHalfRun;
			Post.Yaw = WorldYaw;
			InfantryPosts.Add(Post);
			return true;
		}
		return false;
	}

	void FSovietLayoutContext::PlaceForwardPosts(TArray<FSovietBuildingItem*>& Bunkers)
	{
		// Open approaches nearest the threat first; with none known, fan out around the threat.
		TArray<double> Yaws;
		if (Input.Site != nullptr)
		{
			Yaws = Input.Site->GetApproachYaws();
		}
		const double Threat = ThreatYaw;
		Yaws.StableSort([Threat](const double Left, const double Right)
		{
			return FMath::Abs(FMath::FindDeltaAngleRadians(Left, Threat)) < FMath::Abs(FMath::FindDeltaAngleRadians(Right, Threat));
		});
		for (int32 Index = 0; Index < Bunkers.Num(); ++Index)
		{
			Yaws.Add(ThreatYaw + (Index + 1) / 2 * 0.6 * (Index % 2 == 0 ? 1.0 : -1.0));
		}
		int32 NextYaw = 0;
		for (FSovietBuildingItem* Bunker : Bunkers)
		{
			while (not Bunker->bPlaced and Yaws.IsValidIndex(NextYaw))
			{
				PlaceForwardPost(*Bunker, Yaws[NextYaw++]);
			}
			PointsOutsideMainLine += Bunker->bPlaced ? Bunker->ThreatPoints : 0;
		}
	}

	bool FSovietLayoutContext::PlaceBunkerOutside(FSovietBuildingItem& Bunker, const FVector2D& Center, const double Yaw)
	{
		FEnemyFootprint Footprint;
		Footprint.Center = Center;
		Footprint.HalfExtents = Bunker.Entry.FootprintHalfExtents;
		Footprint.YawRadians = Yaw;
		FVector2D Corners[4];
		Footprint.GetCorners(Corners);
		for (const FVector2D& Corner : Corners)
		{
			if (GetIsInsidePerimeter(Corner))
			{
				return false;
			}
		}
		return not GetIsInsidePerimeter(Center) and PlaceBuildingItem(Bunker, Center, Yaw);
	}

	bool FSovietLayoutContext::PlaceBunkerOnRing(FSovietBuildingItem& Bunker)
	{
		using namespace SovietBaseLayoutConstants;
		for (int32 Ring = 0; Ring < BunkerRingTries; ++Ring)
		{
			const double StandOff = BunkerRingFirstStandOff + Ring * BunkerRowStep;
			for (int32 Step = 0; Step < BunkerRingAngles; ++Step)
			{
				// From the threatened side out to both flanks and round to the back.
				const double Turn = ((Step + 1) / 2) * (2.0 * PI / BunkerRingAngles) * (Step % 2 == 0 ? 1.0 : -1.0);
				const FVector2D Outward = SovietLayout_DirFromYaw(ThreatYaw + Turn);
				const FVector2D Center = Input.Center + Outward * (GetPerimeterDistance(Outward) + StandOff);
				if (PlaceBunkerOutside(Bunker, Center, ThreatYaw + Turn))
				{
					return true;
				}
			}
		}
		return false;
	}

	void FSovietLayoutContext::PlaceSecondRow(TArray<FSovietBuildingItem*>& Bunkers)
	{
		using namespace SovietBaseLayoutConstants;
		for (int32 Index = 0; Index < Bunkers.Num(); ++Index)
		{
			const double Alpha = Bunkers.Num() > 1 ? static_cast<double>(Index) / (Bunkers.Num() - 1) - 0.5 : 0.0;
			const double Yaw = ThreatYaw + Alpha * SecondRowArcRadians;
			for (int32 Step = 0; Step <= 2 * SecondRowSlideSteps and not Bunkers[Index]->bPlaced; ++Step)
			{
				const double Slide = ((Step + 1) / 2) * SecondRowSlideRadians * (Step % 2 == 0 ? 1.0 : -1.0);
				const FVector2D Outward = SovietLayout_DirFromYaw(Yaw + Slide);
				PlaceBunkerOutside(*Bunkers[Index], Input.Center + Outward * (GetPerimeterDistance(Outward) + SecondRowStandOff * Unit), Yaw + Slide);
			}
			PointsOutsideMainLine += Bunkers[Index]->bPlaced ? Bunkers[Index]->ThreatPoints : 0;
		}
	}

	void FSovietLayoutContext::PlaceSecondBelts()
	{
		using namespace SovietBaseLayoutConstants;
		if (not Input.Scaling.bSecondBelt or not Preset.ObstacleBelt.bEnabled)
		{
			return;
		}
		FEnemyObstacleBeltSettings Belt = Preset.ObstacleBelt;
		Belt.SideOverhang = 0.f;
		Belt.Formation = EEnemyObstacleFormation::StraightLine;
		const TArray<FEnemyResolvedEntry> Obstacles = ResolveList(Belt.Obstacles);
		for (const FSovietFenceRun& Run : Master.Fence)
		{
			FVector2D Start;
			FVector2D End;
			FVector2D Outward;
			GetRunGeometry(Run, Start, End, Outward);
			if (not GetIsThreatened(Outward))
			{
				continue;
			}
			FEnemyDefenseFrame Frame;
			Frame.Origin = (Start + End) * 0.5;
			Frame.FacingYaw = SovietLayout_Yaw(Outward);
			const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
			Builder.PlaceObstacleBelt(Obstacles, Belt, Frame, SecondBeltStandOff,
			                          FVector2D::Distance(Start, End) * SecondBeltHalfRunShare, Cluster);
		}
	}

	void FSovietLayoutContext::GetRunGeometry(const FSovietFenceRun& Run, FVector2D& OutStart, FVector2D& OutEnd,
	                                          FVector2D& OutOutward) const
	{
		OutStart = ToWorld(Run.Start);
		OutEnd = ToWorld(Run.End);
		const FVector2D Along = (OutEnd - OutStart).GetSafeNormal();
		OutOutward = FVector2D(-Along.Y, Along.X);
		if (FVector2D::DotProduct(OutOutward, (OutStart + OutEnd) * 0.5 - Input.Center) < 0.0)
		{
			OutOutward = -OutOutward;
		}
	}

	void FSovietLayoutContext::PlaceLineBunkers()
	{
		TArray<FSovietBuildingItem*> Forward;
		TArray<FSovietBuildingItem*> Inner;
		TArray<FSovietBuildingItem*> Main;
		SplitLineBunkers(Forward, Inner, Main);
		PlaceForwardPosts(Forward);
		PlaceSecondRow(Inner);

		// Share the main line over the threatened runs by length; a plan with no line there uses every run.
		TArray<const FSovietFenceRun*> Runs;
		for (const FSovietFenceRun& Run : Master.Fence)
		{
			FVector2D Start;
			FVector2D End;
			FVector2D Outward;
			GetRunGeometry(Run, Start, End, Outward);
			if (GetIsThreatened(Outward))
			{
				Runs.Add(&Run);
			}
		}
		for (int32 Index = 0; Runs.IsEmpty() and Index < Master.Fence.Num(); ++Index)
		{
			Runs.Add(&Master.Fence[Index]);
		}
		TArray<TArray<FSovietBuildingItem*>> Shares;
		Shares.SetNum(Runs.Num());
		for (FSovietBuildingItem* Bunker : Main)
		{
			int32 Best = 0;
			double BestRoom = -1.0;
			for (int32 RunIndex = 0; RunIndex < Runs.Num(); ++RunIndex)
			{
				const double Room = FVector2D::Distance(Runs[RunIndex]->Start, Runs[RunIndex]->End) / (Shares[RunIndex].Num() + 1);
				if (Room > BestRoom)
				{
					BestRoom = Room;
					Best = RunIndex;
				}
			}
			if (Shares.IsValidIndex(Best))
			{
				Shares[Best].Add(Bunker);
			}
		}
		for (int32 RunIndex = 0; RunIndex < Runs.Num(); ++RunIndex)
		{
			PlaceRunBunkers(*Runs[RunIndex], Shares[RunIndex]);
		}
		for (FSovietBuildingItem& Bunker : LineBunkers)
		{
			// A bunker the line has no room for still stands outside the wall, as near the threatened side as it can.
			if (not Bunker.bPlaced and not PlaceBunkerOnRing(Bunker))
			{
				ThreatSpent -= Bunker.ThreatPoints;
				NoteDropped(Bunker, TEXT("no spot; points returned"));
			}
		}
	}

	void FSovietLayoutContext::PlaceRunBunkers(const FSovietFenceRun& Run, TArray<FSovietBuildingItem*>& Bunkers)
	{
		using namespace SovietBaseLayoutConstants;
		FVector2D Start;
		FVector2D End;
		FVector2D Outward;
		GetRunGeometry(Run, Start, End, Outward);
		const FVector2D Along = (End - Start).GetSafeNormal();
		const double Length = FVector2D::Distance(Start, End);
		const FSurvivalSovietWallFamily* Family = GetWallFamily();
		const double HalfWall = Family != nullptr ? Family->Thickness * 0.5 : 0.0;
		for (int32 Index = 0; Index < Bunkers.Num(); ++Index)
		{
			const FVector2D Slot = Start + Along * ((Index + 0.5) * Length / Bunkers.Num());
			const FVector2D Half = Bunkers[Index]->Entry.FootprintHalfExtents;
			// First flush against the wall's outer face, its back to it; further out where the wire or the ground gets in the way.
			const double FlushStandOff = HalfWall + Half.X + BunkerFlushGap;
			for (int32 Row = 0; Row < BunkerRowTries and not Bunkers[Index]->bPlaced; ++Row)
			{
				const double StandOff = Row == 0 ? FlushStandOff : BunkerWallClearance + Half.X + (Row - 1) * BunkerRowStep;
				const FVector2D Line = Slot + Outward * StandOff;
				// Slide along the line when the even spot is a gate or street.
				for (int32 Step = 0; Step <= 2 * RunBunkerSlideSteps and not Bunkers[Index]->bPlaced; ++Step)
				{
					const double Slide = ((Step + 1) / 2) * RunBunkerSlideStep * (Step % 2 == 0 ? 1.0 : -1.0);
					const FVector2D Spot = Line + Along * Slide;
					if (not GetIsClearOfGates(Spot, Half.Y))
					{
						continue;
					}
					PlaceBunkerOutside(*Bunkers[Index], Spot, SovietLayout_Yaw(Outward));
				}
			}
		}
	}

	bool FSovietLayoutContext::GetIsClearOfGates(const FVector2D& Spot, const double HalfWidth) const
	{
		// A bunker beside a gate stands in its mouth: keep a whole gate width of wall free on both sides.
		for (const FSurvivalSovietPlannedGate& Gate : BuiltGates)
		{
			const FVector2D Side(-Gate.Outward.Y, Gate.Outward.X);
			const double AlongWall = FMath::Abs(FVector2D::DotProduct(Spot - Gate.Center, Side));
			const double AcrossWall = FMath::Abs(FVector2D::DotProduct(Spot - Gate.Center, Gate.Outward));
			if (AlongWall < Gate.Width * 1.5 + HalfWidth and AcrossWall < Gate.Width * 1.5)
			{
				return false;
			}
		}
		return true;
	}

	void FSovietLayoutContext::PlaceRunWire(
		const FVector2D& Start,
		const FVector2D& End,
		const FVector2D& Outward,
		const bool bThreatened,
		const int32 Cluster)
	{
		using namespace SovietBaseLayoutConstants;
		const int32 Rows = SovietLayout_ByStrength(Input.Scaling.WireRowsByStrength, Input.Strength, 0);
		const int32 RowsHere = bThreatened ? Rows : FMath::Min(Rows, 1);
		const TArray<FEnemyResolvedEntry> Wire = ResolveList(Kit.BarbedWire);
		for (int32 Row = 0; Row < RowsHere; ++Row)
		{
			const FVector2D Offset = Outward * (WireStandOff + Row * WireRowSpacing);
			Builder.PlaceSegmentRun(EEnemyPlacementCategory::BarbedWire, Wire, Start + Offset, End + Offset, Outward,
			                        EnemySolidCategoryMask(), false, Cluster);
		}
	}

	void FSovietLayoutContext::PlaceRunBelt(const FVector2D& Middle, const FVector2D& Outward, const double Length,
	                                        const int32 Cluster)
	{
		using namespace SovietBaseLayoutConstants;
		if (not Preset.ObstacleBelt.bEnabled or Input.Strength < Input.Scaling.BeltMinStrength)
		{
			return;
		}
		// Only the middle of a run: the ends stay open so a belt slows an assault without sealing the base.
		FEnemyObstacleBeltSettings Belt = Preset.ObstacleBelt;
		Belt.SideOverhang = 0.f;
		FEnemyDefenseFrame Frame;
		Frame.Origin = Middle;
		Frame.FacingYaw = SovietLayout_Yaw(Outward);
		Builder.PlaceObstacleBelt(ResolveList(Belt.Obstacles), Belt, Frame, BeltStandOff, Length * BeltLengthShare, Cluster);
	}

	void FSovietLayoutContext::AddParapetPosts(const FVector2D& Start, const FVector2D& End, const FVector2D& Outward)
	{
		using namespace SovietBaseLayoutConstants;
		const double Length = FVector2D::Distance(Start, End);
		const FVector2D Along = (End - Start).GetSafeNormal();
		const int32 Count = FMath::FloorToInt32(Length / ParapetPostSpacing);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FSovietPost Post;
			Post.Position = Start + Along * ((Index + 0.5) * Length / Count) - Outward * ParapetPostInset;
			Post.Yaw = SovietLayout_Yaw(Outward);
			InfantryPosts.Add(Post);
		}
	}

	void FSovietLayoutContext::PlaceFenceRun(const FSovietFenceRun& Run)
	{
		FVector2D Start;
		FVector2D End;
		FVector2D Outward;
		GetRunGeometry(Run, Start, End, Outward);
		const FVector2D Middle = (Start + End) * 0.5;
		const double Length = FVector2D::Distance(Start, End);
		const bool bThreatened = GetIsThreatened(Outward);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		PlaceRunWire(Start, End, Outward, bThreatened, Cluster);
		if (bThreatened)
		{
			PlaceRunBelt(Middle, Outward, Length, Cluster);
		}
		AddParapetPosts(Start, End, Outward);
	}

	// ------------------------------------------------------------------
	// Dressing
	// ------------------------------------------------------------------

	void FSovietLayoutContext::DressPlots(FSurvivalSovietBasePlan& OutPlan)
	{
		for (const FSovietPlot& Plot : Master.Plots)
		{
			ESurvivalSovietYardKind Kind;
			if (not GetIsYardDressing(Plot.Dressing, Kind))
			{
				DressPlot(Plot);
			}
		}
		for (int32 PlanIndex = 0; PlanIndex < YardPlans.Num(); ++PlanIndex)
		{
			BuildYard(PlanIndex, OutPlan);
		}
		PlaceLanterns(OutPlan);
	}

	FVector2D FSovietLayoutContext::GetEdgeOutwardFrom(const FVector2D& Start, const FVector2D& End, const FVector2D& Center)
	{
		const FVector2D Along = (End - Start).GetSafeNormal();
		const FVector2D Out(-Along.Y, Along.X);
		return FVector2D::DotProduct(Out, (Start + End) * 0.5 - Center) < 0.0 ? -Out : Out;
	}

	// ------------------------------------------------------------------
	// Storage yards: stacks in rows with aisles, fenced by one family, nets over ammunition, paths between the rows
	// ------------------------------------------------------------------

	bool FSovietLayoutContext::GetIsYardDressing(const ESovietPlotDressing Dressing, ESurvivalSovietYardKind& OutKind)
	{
		switch (Dressing)
		{
		case ESovietPlotDressing::Crates:
		case ESovietPlotDressing::Munitions:
			OutKind = ESurvivalSovietYardKind::Ammo;
			return true;
		case ESovietPlotDressing::Barrels:
			OutKind = ESurvivalSovietYardKind::Fuel;
			return true;
		default:
			return false;
		}
	}

	const FSurvivalSovietWallFamily* FSovietLayoutContext::FindWallFamily(const ESurvivalSovietWallFamily FamilyKind) const
	{
		return Kit.WallFamilies.FindByPredicate([FamilyKind](const FSurvivalSovietWallFamily& Family)
		{
			return Family.Family == FamilyKind;
		});
	}

	void FSovietLayoutContext::PlanYards()
	{
		using namespace SovietBaseLayoutConstants;
		for (int32 PlotIndex = 0; PlotIndex < Master.Plots.Num(); ++PlotIndex)
		{
			const FSovietPlot& Plot = Master.Plots[PlotIndex];
			ESurvivalSovietYardKind Kind;
			if (not GetIsYardDressing(Plot.Dressing, Kind))
			{
				continue;
			}
			FSovietYardPlan& Plan = YardPlans.AddDefaulted_GetRef();
			Plan.Kind = Kind;
			Plan.Dressing = Plot.Dressing;
			Plan.Frame.Center = ToWorld(Plot.Center);
			Plan.Frame.Front = DirectionToWorld(Plot.Front).GetSafeNormal();
			Plan.Frame.Along = FVector2D(-Plan.Frame.Front.Y, Plan.Frame.Front.X);
			Plan.Frame.HalfLength = Plot.HalfLength * Unit + YardFenceMargin;
			Plan.Frame.HalfDepth = Plot.HalfDepth * Unit + YardFenceMargin;
			Plan.PlotIndex = PlotIndex;
			FitYard(Plan.Frame, PlotIndex);
			YardPlans.Last().bFitted = true;
		}
	}

	bool FSovietLayoutContext::GetIsInForeignYard(const FEnemyFootprint& Footprint) const
	{
		using namespace SovietBaseLayoutConstants;
		FVector2D Corners[4];
		Footprint.GetCorners(Corners);
		for (int32 PlanIndex = 0; PlanIndex < YardPlans.Num(); ++PlanIndex)
		{
			if (PlanIndex == ActiveYard)
			{
				continue;
			}
			const FSovietYardFrame& Frame = YardPlans[PlanIndex].Frame;
			const auto GetIsInside = [&Frame](const FVector2D& Point)
			{
				const FVector2D Offset = Point - Frame.Center;
				return FMath::Abs(FVector2D::DotProduct(Offset, Frame.Along)) < Frame.HalfLength + YardKeepOut
					and FMath::Abs(FVector2D::DotProduct(Offset, Frame.Front)) < Frame.HalfDepth + YardKeepOut;
			};
			// A footprint touching the yard by a corner or its centre, or a yard centre under a large footprint.
			bool bTouches = GetIsInside(Footprint.Center);
			for (const FVector2D& Corner : Corners)
			{
				bTouches = bTouches or GetIsInside(Corner);
			}
			const FVector2D ToYard = Frame.Center - Footprint.Center;
			const FVector2D Axis = SovietLayout_DirFromYaw(Footprint.YawRadians);
			const FVector2D Side(-Axis.Y, Axis.X);
			bTouches = bTouches or (FMath::Abs(FVector2D::DotProduct(ToYard, Axis)) < Footprint.HalfExtents.X
				and FMath::Abs(FVector2D::DotProduct(ToYard, Side)) < Footprint.HalfExtents.Y);
			if (bTouches)
			{
				return true;
			}
		}
		return false;
	}

	TOptional<ESurvivalSovietYardKind> FSovietLayoutContext::GetStoreKind(const FSovietBuildingItem& Item)
	{
		const bool bStore = Item.Role == ESurvivalSovietStructureRole::Support
			and (Item.ClassName.Contains(TEXT("Storage")) or Item.ClassName.Contains(TEXT("Supplies")) or Item.ClassName.Contains(TEXT("Fuel")));
		if (not bStore)
		{
			return {};
		}
		const bool bFuel = Item.Zone == ESurvivalSovietZone::Fuel or Item.ClassName.Contains(TEXT("Fuel"));
		return bFuel ? ESurvivalSovietYardKind::Fuel : ESurvivalSovietYardKind::Ammo;
	}

	bool FSovietLayoutContext::GetIsInStoreFireBreak(const FEnemyFootprint& Footprint) const
	{
		using namespace SovietBaseLayoutConstants;
		if (not PlacingStoreKind.IsSet())
		{
			return false;
		}
		FVector2D Corners[4];
		Footprint.GetCorners(Corners);
		for (const FSovietYardPlan& Plan : YardPlans)
		{
			if (Plan.Kind == PlacingStoreKind.GetValue())
			{
				continue;
			}
			const FSovietYardFrame& Frame = Plan.Frame;
			for (const FVector2D& Point : {Corners[0], Corners[1], Corners[2], Corners[3], Footprint.Center})
			{
				const FVector2D Offset = Point - Frame.Center;
				if (FMath::Abs(FVector2D::DotProduct(Offset, Frame.Along)) < Frame.HalfLength + YardStoreFireBreak
					and FMath::Abs(FVector2D::DotProduct(Offset, Frame.Front)) < Frame.HalfDepth + YardStoreFireBreak)
				{
					return true;
				}
			}
		}
		return false;
	}

	void FSovietLayoutContext::TakeYardStores(TArray<FSovietBuildingItem>& Items)
	{
		// Ammunition stores and fuel tanks stand in their yard, not beside it; a loudspeaker or a hut is no store.
		TArray<int32> AmmoYards;
		TArray<int32> FuelYards;
		for (int32 PlanIndex = 0; PlanIndex < YardPlans.Num(); ++PlanIndex)
		{
			(YardPlans[PlanIndex].Kind == ESurvivalSovietYardKind::Ammo ? AmmoYards : FuelYards).Add(PlanIndex);
		}
		int32 NextAmmo = 0;
		int32 NextFuel = 0;
		for (int32 Index = Items.Num() - 1; Index >= 0; --Index)
		{
			const FSovietBuildingItem& Item = Items[Index];
			const bool bStore = Item.Role == ESurvivalSovietStructureRole::Support
				and (Item.ClassName.Contains(TEXT("Storage")) or Item.ClassName.Contains(TEXT("Supplies")) or Item.ClassName.Contains(TEXT("Fuel")));
			const bool bFuel = Item.Zone == ESurvivalSovietZone::Fuel;
			const TArray<int32>& Candidates = bFuel ? FuelYards : AmmoYards;
			if (not bStore or Candidates.IsEmpty())
			{
				continue;
			}
			int32& Next = bFuel ? NextFuel : NextAmmo;
			FSovietYardPlan& Target = YardPlans[Candidates[Next % Candidates.Num()]];
			// A shallow yard keeps its ground for the stacks; its stores stand beside it.
			const double Shorter = FMath::Min(Target.Frame.HalfDepth, Target.Frame.HalfLength);
			if (Target.Stores.Num() >= SovietBaseLayoutConstants::YardMaxStores or Shorter < SovietBaseLayoutConstants::YardStoreMinHalfDepth)
			{
				continue;
			}
			Next += 1;
			Target.Stores.Add(Item);
			Items.RemoveAt(Index);
		}
	}

	double FSovietLayoutContext::PlaceYardStores(const int32 PlanIndex)
	{
		using namespace SovietBaseLayoutConstants;
		FSovietYardPlan& Plan = YardPlans[PlanIndex];
		const FSovietYardFrame& Frame = Plan.Frame;
		const double InnerLength = Frame.HalfLength - YardInnerMargin;
		const double InnerDepth = Frame.HalfDepth - YardInnerMargin;
		double Reach = -InnerDepth;
		Plan.Stores.StableSort([](const FSovietBuildingItem& Left, const FSovietBuildingItem& Right)
		{
			return Left.Entry.FootprintHalfExtents.SizeSquared() > Right.Entry.FootprintHalfExtents.SizeSquared();
		});
		for (FSovietBuildingItem& Store : Plan.Stores)
		{
			const double Yaw = GetBuildingYaw(Store.Entry, Frame.Front, Frame.Along, ESovietPlotFacing::LongSideToStreet);
			FEnemyFootprint Rotated;
			Rotated.HalfExtents = Store.Entry.FootprintHalfExtents;
			Rotated.YawRadians = Yaw;
			const double HalfWidth = Rotated.SupportRadius(Frame.Along);
			const double HalfDepth = Rotated.SupportRadius(Frame.Front);
			// One back row only, from the middle out: the front of the yard is for the stacks. Stores that do not fit stand elsewhere.
			for (double Depth = -InnerDepth + HalfDepth; not Store.bPlaced and Depth <= -InnerDepth + HalfDepth; Depth += YardStoreRowStep)
			{
				for (int32 Step = 0; not Store.bPlaced and (Step / 2) * YardStoreSlide + HalfWidth <= InnerLength; ++Step)
				{
					const double Offset = ((Step + 1) / 2) * YardStoreSlide * (Step % 2 == 0 ? 1.0 : -1.0);
					if (PlaceBuildingItem(Store, Frame.Center + Frame.Front * Depth + Frame.Along * Offset, Yaw))
					{
						YardStoreClusters.Add(Result.Actors.Last().ClusterIndex);
						Reach = FMath::Max(Reach, Depth + HalfDepth);
					}
				}
			}
		}
		return Reach;
	}

	void FSovietLayoutContext::BuildYard(const int32 PlanIndex, FSurvivalSovietBasePlan& OutPlan)
	{
		using namespace SovietBaseLayoutConstants;
		const FSovietYardPlan& Plan = YardPlans[PlanIndex];
		const FSovietYardFrame& Frame = Plan.Frame;
		// Active before the fence test, so this yard's own ground does not block its fence.
		ActiveYard = PlanIndex;
		const FSurvivalSovietWallFamily* Family = FindWallFamily(Plan.Kind == ESurvivalSovietYardKind::Ammo ? Kit.AmmoYardFence : Kit.FuelYardFence);
		const bool bFenced = Family != nullptr and Family->Straight.GetIsSet();
		// Buildings, guns and runs stand by now: pull in any side whose fence line they would cut, so the fence closes.
		const int32 TightenSteps = bFenced ? TightenYardForFence(*Family, YardPlans[PlanIndex].Frame) : 0;
		FSurvivalSovietPlannedYard& Yard = Yards.AddDefaulted_GetRef();
		Yard.Kind = Plan.Kind;
		Yard.Center = Frame.Center;
		Yard.HalfExtents = FVector2D(Frame.HalfLength, Frame.HalfDepth);
		Yard.YawRadians = SovietLayout_Yaw(Frame.Along);
		Yard.Entrance = Frame.Center + Frame.Front * Frame.HalfDepth;
		Yard.FenceTightenSteps = TightenSteps;
		const int32 FenceCluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		if (bFenced)
		{
			Yard.FenceClass = Family->Straight.Entry.BlueprintClass.GetAssetName();
			FenceYard(*Family, Frame, FenceCluster);
		}
		const double StoreReach = PlaceYardStores(PlanIndex);
		for (const FSovietBuildingItem& Store : Plan.Stores)
		{
			Yard.StoreClasses.Add(Store.ClassName);
		}
		// The plot's own stacks first; an ammunition yard may also fill from the other ammunition list when its own cells are too big.
		TArray<TArray<FEnemyResolvedEntry>> StackLists;
		if (Plan.Kind == ESurvivalSovietYardKind::Ammo)
		{
			const bool bMunitions = Plan.Dressing == ESovietPlotDressing::Munitions;
			StackLists.Add(ResolveList(bMunitions ? Kit.Munitions : Kit.Crates));
			StackLists.Add(ResolveList(bMunitions ? Kit.Crates : Kit.Munitions));
		}
		else
		{
			StackLists.Add(ResolveList(Kit.Barrels));
		}
		TArray<double> RowDepths;
		const int32 StackCluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		// Rows run along the yard's longer side. A deep yard turns its rows to point at the street, with the entrance path
		// down a middle aisle; the stores in the back row then simply take the cells they stand on.
		const bool bDeep = Frame.HalfDepth > Frame.HalfLength * YardDeepRatio;
		FSovietYardFrame RowFrame = Frame;
		if (bDeep)
		{
			RowFrame.Along = Frame.Front;
			RowFrame.Front = -Frame.Along;
			RowFrame.HalfLength = Frame.HalfDepth;
			RowFrame.HalfDepth = Frame.HalfLength;
		}
		// Without stores the rows start at the yard's inner margin (FillYardRows clamps to it).
		const double FirstDepth = bDeep or StoreReach <= -Frame.HalfDepth + YardInnerMargin ? -TNumericLimits<double>::Max()
			                          : StoreReach + YardAisle;
		const FSovietYardStackChoice Choice = ChooseYardStacks(RowFrame, StackLists, FirstDepth, bDeep);
		Yard.StackList = Choice.ListIndex == 0 ? TEXT("own") : TEXT("other");
		Yard.StackTypes = Choice.Stacks.Num();
		Yard.StackEstimate = Choice.Estimate;
		FillYardRows(RowFrame, Choice, StackCluster, FirstDepth, bDeep, RowDepths);
		if (Plan.Kind == ESurvivalSovietYardKind::Ammo)
		{
			CoverYardWithNets(RowFrame, RowDepths);
		}
		AddYardPaths(Frame, RowFrame, bDeep, RowDepths, OutPlan);
		ActiveYard = INDEX_NONE;
		// Stores the yard had no room for stand anywhere else inside the base.
		for (FSovietBuildingItem& Store : YardPlans[PlanIndex].Stores)
		{
			if (not Store.bPlaced and not PlaceAnywhereInside(Store))
			{
				NoteDropped(Store, TEXT("no free ground"));
			}
		}
	}

	void FSovietLayoutContext::NoteBuildingsInsideYards()
	{
		for (int32 YardIndex = 0; YardIndex < Yards.Num() and YardIndex < YardPlans.Num(); ++YardIndex)
		{
			const FSovietYardFrame& Frame = YardPlans[YardIndex].Frame;
			for (const FEnemyPlacedActor& Placed : Result.Actors)
			{
				const FVector2D Offset = Placed.Footprint.Center - Frame.Center;
				const bool bInside = FMath::Abs(FVector2D::DotProduct(Offset, Frame.Along)) < Frame.HalfLength
					and FMath::Abs(FVector2D::DotProduct(Offset, Frame.Front)) < Frame.HalfDepth;
				if (Placed.Category == EEnemyPlacementCategory::Bunker and bInside and not YardStoreClusters.Contains(Placed.ClusterIndex))
				{
					Yards[YardIndex].BuildingsInside.Add(Placed.AssetIndex);
				}
			}
		}
	}

	bool FSovietLayoutContext::GetIsYardSideClear(const FSovietYardFrame& Frame, const double (&Extents)[4], const int32 Side,
	                                              const int32 OwnPlot) const
	{
		using namespace SovietBaseLayoutConstants;
		// Extents run from the plot centre towards the street, the back, the right and the left.
		const FVector2D Normals[4] = {Frame.Front, -Frame.Front, Frame.Along, -Frame.Along};
		const FVector2D Normal = Normals[Side];
		const FVector2D Across = Side < 2 ? Frame.Along : Frame.Front;
		const double Plus = Extents[Side < 2 ? 2 : 0];
		const double Minus = Extents[Side < 2 ? 3 : 1];
		const FVector2D Clearance = Frame.Center + Normal * (Extents[Side] + YardWallClearance);
		if (not GetIsInsidePerimeter(Clearance + Across * Plus) or not GetIsInsidePerimeter(Clearance - Across * Minus))
		{
			return false;
		}
		// The fence line must not run on a street or through another plot.
		const FVector2D Line = Frame.Center + Normal * Extents[Side];
		if (GetIsLineOnStreet(Line - Across * Minus, Line + Across * Plus))
		{
			return false;
		}
		ESurvivalSovietYardKind OwnKind = ESurvivalSovietYardKind::Ammo;
		const bool bOwnIsYard = Master.Plots.IsValidIndex(OwnPlot) and GetIsYardDressing(Master.Plots[OwnPlot].Dressing, OwnKind);
		const int32 Samples = FMath::Max(2, FMath::CeilToInt32((Plus + Minus) / YardStreetSampleStep) + 1);
		for (int32 Sample = 0; Sample < Samples; ++Sample)
		{
			const FVector2D Point = FMath::Lerp(Line - Across * Minus, Line + Across * Plus, static_cast<double>(Sample) / (Samples - 1));
			for (int32 PlotIndex = 0; PlotIndex < Master.Plots.Num(); ++PlotIndex)
			{
				const FSovietPlot& Other = Master.Plots[PlotIndex];
				// A yard of the other kind keeps the fire break from this one; its own fence margin counts too.
				ESurvivalSovietYardKind OtherKind;
				const bool bFireBreak = bOwnIsYard and GetIsYardDressing(Other.Dressing, OtherKind) and OtherKind != OwnKind;
				const double PlotClearance = bFireBreak ? YardFenceMargin + YardFuelAmmoFenceGap : YardPlotClearance;
				const FVector2D Offset = Point - ToWorld(Other.Center);
				const FVector2D Front = DirectionToWorld(Other.Front).GetSafeNormal();
				const FVector2D Along(-Front.Y, Front.X);
				const bool bInOther = FMath::Abs(FVector2D::DotProduct(Offset, Along)) < Other.HalfLength * Unit + PlotClearance
					and FMath::Abs(FVector2D::DotProduct(Offset, Front)) < Other.HalfDepth * Unit + PlotClearance;
				if (PlotIndex != OwnPlot and bInOther)
				{
					return false;
				}
			}
			if (bOwnIsYard and GetIsNearFittedYard(Point, OwnPlot, OwnKind))
			{
				return false;
			}
		}
		return true;
	}

	bool FSovietLayoutContext::GetIsNearFittedYard(const FVector2D& Point, const int32 OwnPlot, const ESurvivalSovietYardKind OwnKind) const
	{
		using namespace SovietBaseLayoutConstants;
		for (const FSovietYardPlan& Other : YardPlans)
		{
			if (not Other.bFitted or Other.PlotIndex == OwnPlot)
			{
				continue;
			}
			const double Gap = Other.Kind == OwnKind ? YardSameKindFenceGap : YardFuelAmmoFenceGap;
			const FVector2D Offset = Point - Other.Frame.Center;
			if (FMath::Abs(FVector2D::DotProduct(Offset, Other.Frame.Along)) < Other.Frame.HalfLength + Gap
				and FMath::Abs(FVector2D::DotProduct(Offset, Other.Frame.Front)) < Other.Frame.HalfDepth + Gap)
			{
				return true;
			}
		}
		return false;
	}

	void FSovietLayoutContext::FitYard(FSovietYardFrame& Frame, const int32 OwnPlot) const
	{
		using namespace SovietBaseLayoutConstants;
		double Extents[4] = {Frame.HalfDepth, Frame.HalfDepth, Frame.HalfLength, Frame.HalfLength};
		// Pull in every side that would meet the wall, a street or another plot...
		for (int32 Side = 0; Side < 4; ++Side)
		{
			while (Extents[Side] > YardMinExtent and not GetIsYardSideClear(Frame, Extents, Side, OwnPlot))
			{
				Extents[Side] -= YardShrinkStep;
			}
		}
		// ...then let the back and both ends grow into free ground until the yard holds a store row and two rows of stacks.
		const double Targets[4] = {0.0, YardTargetHalfDepth, YardTargetHalfLength, YardTargetHalfLength};
		for (const int32 Side : {1, 2, 3})
		{
			const int32 Opposite = Side == 1 ? 0 : (Side == 2 ? 3 : 2);
			while (Extents[Side] + Extents[Opposite] < 2.0 * Targets[Side])
			{
				Extents[Side] += YardShrinkStep;
				if (not GetIsYardSideClear(Frame, Extents, Side, OwnPlot))
				{
					Extents[Side] -= YardShrinkStep;
					break;
				}
			}
		}
		Frame.Center += Frame.Front * ((Extents[0] - Extents[1]) * 0.5) + Frame.Along * ((Extents[2] - Extents[3]) * 0.5);
		Frame.HalfDepth = (Extents[0] + Extents[1]) * 0.5;
		Frame.HalfLength = (Extents[2] + Extents[3]) * 0.5;
	}

	bool FSovietLayoutContext::GetIsLineOnStreet(const FVector2D& From, const FVector2D& To) const
	{
		using namespace SovietBaseLayoutConstants;
		const double Length = FVector2D::Distance(From, To);
		const int32 Samples = FMath::Max(2, FMath::CeilToInt32(Length / YardStreetSampleStep) + 1);
		for (int32 Sample = 0; Sample < Samples; ++Sample)
		{
			const FVector2D Point = FMath::Lerp(From, To, static_cast<double>(Sample) / (Samples - 1));
			for (const FSovietStreet& Street : Master.Streets)
			{
				for (int32 Index = 0; Street.Kind != ESurvivalSovietRoadKind::None and Index + 1 < Street.Points.Num(); ++Index)
				{
					const FVector2D A = ToWorld(Street.Points[Index]);
					const FVector2D B = ToWorld(Street.Points[Index + 1]);
					const FVector2D Along = (B - A).GetSafeNormal();
					const FVector2D Closest = A + Along * FMath::Clamp(FVector2D::DotProduct(Point - A, Along), 0.0, FVector2D::Distance(A, B));
					if (FVector2D::Distance(Point, Closest) < Street.Width * 0.5 + YardStreetClearance)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	void FSovietLayoutContext::FenceYard(const FSurvivalSovietWallFamily& Family, const FSovietYardFrame& Frame, const int32 Cluster)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D& C = Frame.Center;
		const FVector2D Corners[4] = {
			C + Frame.Front * Frame.HalfDepth - Frame.Along * Frame.HalfLength,
			C + Frame.Front * Frame.HalfDepth + Frame.Along * Frame.HalfLength,
			C - Frame.Front * Frame.HalfDepth + Frame.Along * Frame.HalfLength,
			C - Frame.Front * Frame.HalfDepth - Frame.Along * Frame.HalfLength
		};
		const bool bCornerPieces = Family.Corner.GetIsSet();
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const FVector2D Start = Corners[Side];
			const FVector2D End = Corners[(Side + 1) % 4];
			if (bCornerPieces)
			{
				PlaceYardCorner(Family, Start, (Corners[(Side + 3) % 4] - Start).GetSafeNormal(), (End - Start).GetSafeNormal(), Cluster);
			}
			FSovietWallSpanEnd Ends;
			Ends.bCap = false;
			Ends.Extend = bCornerPieces ? -Family.Corner.Length : Family.Thickness * 0.5;
			const FVector2D Outward = GetEdgeOutwardFrom(Start, End, C);
			if (Side != 0)
			{
				PlaceYardSide(Family, Start, End, Outward, Ends, Cluster);
				continue;
			}
			// The street side: two spans either side of the entrance, finished square at the opening.
			const FVector2D Walk = (End - Start).GetSafeNormal();
			const FVector2D Middle = (Start + End) * 0.5;
			FSovietWallSpanEnd Opening;
			Opening.bCap = false;
			PlaceYardSpan(Family, Start - Walk * Ends.Extend, Middle - Walk * YardEntranceHalfWidth, Outward, Ends, Opening, Cluster);
			PlaceYardSpan(Family, Middle + Walk * YardEntranceHalfWidth, End + Walk * Ends.Extend, Outward, Opening, Ends, Cluster);
		}
	}

	void FSovietLayoutContext::PlaceYardSide(const FSurvivalSovietWallFamily& Family, const FVector2D& Start, const FVector2D& End,
	                                         const FVector2D& Outward, const FSovietWallSpanEnd& Ends, const int32 Cluster)
	{
		const FVector2D Walk = (End - Start).GetSafeNormal();
		PlaceYardSpan(Family, Start - Walk * Ends.Extend, End + Walk * Ends.Extend, Outward, Ends, Ends, Cluster);
	}

	void FSovietLayoutContext::PlaceYardSpan(const FSurvivalSovietWallFamily& Family, const FVector2D& Start, const FVector2D& End,
	                                         const FVector2D& Outward, const FSovietWallSpanEnd& StartEnd,
	                                         const FSovietWallSpanEnd& EndEnd, const int32 Cluster)
	{
		// Same handedness as the base wall: the outside stays on the right hand of the walk.
		const FVector2D RightOfWalk(-Outward.Y, Outward.X);
		if (FVector2D::DotProduct(End - Start, RightOfWalk) < 0.0)
		{
			PlaceWallSpan(Family, End, Start, Outward, EndEnd, StartEnd, Cluster);
			return;
		}
		PlaceWallSpan(Family, Start, End, Outward, StartEnd, EndEnd, Cluster);
	}

	void FSovietLayoutContext::PlaceYardCorner(const FSurvivalSovietWallFamily& Family, const FVector2D& Corner, const FVector2D& Back,
	                                           const FVector2D& Next, const int32 Cluster)
	{
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList({Family.Corner.Entry});
		if (Resolved.IsEmpty())
		{
			return;
		}
		// Arms along local +X and -Y: the first arm is the one the other lies clockwise of (see PlaceWallCorners).
		const FVector2D FirstArm = FVector2D::CrossProduct(Back, Next) < 0.0 ? Back : Next;
		const FVector2D SecondArm = FVector2D::CrossProduct(Back, Next) < 0.0 ? Next : Back;
		FEnemyFootprint Footprint;
		Footprint.Center = Corner + (FirstArm + SecondArm) * (Family.Corner.Length * 0.5);
		Footprint.HalfExtents = FVector2D(Family.Corner.Length * 0.5, Family.Corner.Length * 0.5);
		Footprint.YawRadians = SovietLayout_Yaw(FirstArm);
		// Same mask as the spans: the yard's own fence pieces are hedgehogs and touch the corner square on the occupancy grid.
		const uint8 CornerMask = EnemySolidCategoryMask() & ~EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog);
		if (not Builder.GetIsFootprintFree(Footprint, CornerMask))
		{
			Yards[ActiveYard].FenceGaps += 1;
			Yards[ActiveYard].FenceGapCauses.FindOrAdd(TEXT("corner:") + DescribeFenceBlock(Footprint, CornerMask))++;
			Yards[ActiveYard].FenceGapPositions.Add(Footprint.Center);
			return;
		}
		Builder.PlaceAtPivot(EEnemyPlacementCategory::Hedgehog, Resolved[0], Corner, Footprint.YawRadians, Footprint, Cluster);
		Yards[ActiveYard].FencePieces += 1;
	}

	TArray<double> FSovietLayoutContext::GetYardRowDepths(const FSovietYardFrame& Frame, const double StackDepth, const double FirstDepth,
	                                                      const bool bMiddleAisle)
	{
		using namespace SovietBaseLayoutConstants;
		const double InnerDepth = Frame.HalfDepth - (Frame.HalfDepth < YardShallowHalfDepth ? YardShallowInnerMargin : YardInnerMargin);
		TArray<double> Depths;
		if (StackDepth <= 0.0)
		{
			return Depths;
		}
		if (bMiddleAisle)
		{
			for (double Depth = (YardAisle + StackDepth) * 0.5; Depth + StackDepth * 0.5 <= InnerDepth; Depth += StackDepth + YardAisle)
			{
				Depths.Add(-Depth);
				Depths.Add(Depth);
			}
			Depths.Sort();
			return Depths;
		}
		const double FrontClear = Frame.HalfDepth < YardShallowHalfDepth ? 0.0 : YardFrontClear;
		for (double Depth = FMath::Max(FirstDepth, -InnerDepth) + StackDepth * 0.5; Depth + StackDepth * 0.5 <= InnerDepth - FrontClear;
		     Depth += StackDepth + YardAisle)
		{
			Depths.Add(Depth);
		}
		return Depths;
	}

	int32 FSovietLayoutContext::GetYardStacksPerRow(const FSovietYardFrame& Frame, const double StackWidth)
	{
		using namespace SovietBaseLayoutConstants;
		const double InnerLength = Frame.HalfLength - YardInnerMargin;
		int32 Count = 0;
		for (double Offset = YardPathHalfWidth + StackWidth * 0.5; StackWidth > 0.0 and Offset + StackWidth * 0.5 <= InnerLength;
		     Offset += StackWidth + YardStackGap)
		{
			// One either side of the middle path.
			Count += 2;
		}
		return Count;
	}

	FSovietYardStackChoice FSovietLayoutContext::ChooseYardStacks(const FSovietYardFrame& Frame,
	                                                             const TArray<TArray<FEnemyResolvedEntry>>& Lists,
	                                                             const double FirstDepth, const bool bMiddleAisle)
	{
		using namespace SovietBaseLayoutConstants;
		struct FCell
		{
			int32 Index = 0;
			double Width = 0.0;
			double Depth = 0.0;
		};
		FSovietYardStackChoice Best;
		bool bHaveBest = false;
		// Ranks a choice: reach the target first, then the plot's own list, then variety, then the raw count.
		const auto IsBetter = [](const FSovietYardStackChoice& Candidate, const FSovietYardStackChoice& Current)
		{
			const int32 CandidateReach = FMath::Min(Candidate.Estimate, YardTargetStacks);
			const int32 CurrentReach = FMath::Min(Current.Estimate, YardTargetStacks);
			if (CandidateReach != CurrentReach)
			{
				return CandidateReach > CurrentReach;
			}
			if (Candidate.ListIndex != Current.ListIndex)
			{
				return Candidate.ListIndex < Current.ListIndex;
			}
			if (Candidate.Stacks.Num() != Current.Stacks.Num())
			{
				return Candidate.Stacks.Num() > Current.Stacks.Num();
			}
			return Candidate.Estimate > Current.Estimate;
		};
		for (int32 ListIndex = 0; ListIndex < Lists.Num(); ++ListIndex)
		{
			const TArray<FEnemyResolvedEntry>& List = Lists[ListIndex];
			for (const double YawOffset : {0.0, UE_DOUBLE_HALF_PI})
			{
				TArray<FCell> Cells;
				for (int32 Index = 0; Index < List.Num(); ++Index)
				{
					FEnemyFootprint Rotated;
					Rotated.HalfExtents = List[Index].FootprintHalfExtents;
					Rotated.YawRadians = SovietLayout_Yaw(Frame.Along) + YawOffset;
					Cells.Add({Index, 2.0 * Rotated.SupportRadius(Frame.Along), 2.0 * Rotated.SupportRadius(Frame.Front)});
				}
				// Smallest cells first: keeping the K smallest types gives the tightest shared cell for K types.
				Cells.Sort([](const FCell& Left, const FCell& Right)
				{
					return Left.Width * Left.Depth < Right.Width * Right.Depth;
				});
				for (int32 Kept = Cells.Num(); Kept >= 1; --Kept)
				{
					FSovietYardStackChoice Candidate;
					Candidate.YawOffset = YawOffset;
					Candidate.ListIndex = ListIndex;
					for (int32 CellIndex = 0; CellIndex < Kept; ++CellIndex)
					{
						Candidate.Stacks.Add(List[Cells[CellIndex].Index]);
						Candidate.Width = FMath::Max(Candidate.Width, Cells[CellIndex].Width);
						Candidate.Depth = FMath::Max(Candidate.Depth, Cells[CellIndex].Depth);
					}
					Candidate.Estimate = GetYardRowDepths(Frame, Candidate.Depth, FirstDepth, bMiddleAisle).Num()
						* GetYardStacksPerRow(Frame, Candidate.Width);
					if (not bHaveBest or IsBetter(Candidate, Best))
					{
						Best = MoveTemp(Candidate);
						bHaveBest = true;
					}
				}
			}
		}
		return Best;
	}

	void FSovietLayoutContext::FillYardRows(const FSovietYardFrame& Frame, const FSovietYardStackChoice& Choice, const int32 Cluster,
	                                        const double FirstDepth, const bool bMiddleAisle, TArray<double>& OutRowDepths)
	{
		using namespace SovietBaseLayoutConstants;
		const TArray<FEnemyResolvedEntry>& Stacks = Choice.Stacks;
		if (Stacks.IsEmpty())
		{
			return;
		}
		// One cell size for every stack, so rows and aisles stay straight and even.
		const double StackWidth = Choice.Width;
		const double StackDepth = Choice.Depth;
		const double StackYaw = SovietLayout_Yaw(Frame.Along) + Choice.YawOffset;
		FSurvivalSovietPlannedYard& Yard = Yards[ActiveYard];
		const double InnerLength = Frame.HalfLength - YardInnerMargin;
		Yard.AisleWidth = YardAisle;
		const uint8 Mask = EnemySolidCategoryMask() | EnemyOccupancyMask(EEnemyPlacementCategory::Decorator);
		const TArray<double> Depths = GetYardRowDepths(Frame, StackDepth, FirstDepth, bMiddleAisle);
		for (const double Depth : Depths)
		{
			int32 Placed = 0;
			for (double Offset = YardPathHalfWidth + StackWidth * 0.5; Offset + StackWidth * 0.5 <= InnerLength; Offset += StackWidth + YardStackGap)
			{
				for (const double Sign : {-1.0, 1.0})
				{
					const FVector2D Position = Frame.Center + Frame.Front * Depth + Frame.Along * (Sign * Offset);
					FEnemyFootprint Footprint;
					const int32 ActorsBefore = Result.Actors.Num();
					if (not Builder.PlaceCentered(EEnemyPlacementCategory::Decorator, Stacks, INDEX_NONE, Position, StackYaw,
					                              YardStackClearance, Mask, Cluster, Footprint))
					{
						continue;
					}
					Placed += 1;
					Yard.ItemPositions.Add(Footprint.Center);
					Yard.StackArea += StackWidth * StackDepth;
					// Keyed by class index; the manifest turns it into the class name.
					const int32 AssetIndex = Result.Actors.IsValidIndex(ActorsBefore) ? Result.Actors[ActorsBefore].AssetIndex : INDEX_NONE;
					Yard.ItemCounts.FindOrAdd(FString::FromInt(AssetIndex))++;
				}
			}
			if (Placed > 0)
			{
				OutRowDepths.Add(Depth);
				Yard.Rows += 1;
			}
		}
	}

	void FSovietLayoutContext::CoverYardWithNets(const FSovietYardFrame& Frame, const TArray<double>& RowDepths)
	{
		using namespace SovietBaseLayoutConstants;
		TArray<FEnemyResolvedEntry> Nets = ResolveList(Kit.YardNets);
		if (Nets.IsEmpty() or RowDepths.IsEmpty())
		{
			return;
		}
		// Largest first: a big net over two rows where it fits, smaller nets where a store or the fence is in the way.
		Nets.Sort([](const FEnemyResolvedEntry& Left, const FEnemyResolvedEntry& Right)
		{
			return Left.FootprintHalfExtents.X * Left.FootprintHalfExtents.Y > Right.FootprintHalfExtents.X * Right.FootprintHalfExtents.Y;
		});
		FSurvivalSovietPlannedYard& Yard = Yards[ActiveYard];
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		// Over the stacks, never over a store, a street or the fence.
		const uint8 Mask = EnemyOccupancyMask(EEnemyPlacementCategory::Bunker) | EnemyOccupancyMask(EEnemyPlacementCategory::Reserved)
			| EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog);
		const double Reach = Frame.HalfLength - YardInnerMargin;
		for (int32 Row = 0; Row < RowDepths.Num(); Row += 2)
		{
			const double RowDepth = Row + 1 < RowDepths.Num() ? (RowDepths[Row] + RowDepths[Row + 1]) * 0.5 : RowDepths[Row];
			// A lone row in a shallow yard: a net may also hang centred in the yard, where it has the most room to the fence.
			TArray<double, TInlineAllocator<2>> Depths = {RowDepth};
			if (Row + 1 >= RowDepths.Num() and not FMath::IsNearlyZero(RowDepth))
			{
				Depths.Add(0.0);
			}
			for (double Offset = -Reach; Offset <= Reach; Offset += YardNetStep)
			{
				FEnemyFootprint Footprint;
				const FEnemyResolvedEntry* Placed = TryPlaceYardNet(Frame, Nets, Depths, Offset, Mask, Cluster, Footprint);
				if (Placed == nullptr)
				{
					continue;
				}
				Yard.NetCounts.FindOrAdd(FString::FromInt(Placed->OutputAssetIndex))++;
				Yard.NetArea += 4.0 * Footprint.HalfExtents.X * Footprint.HalfExtents.Y;
				Offset += 2.0 * Footprint.SupportRadius(Frame.Along) - YardNetStep;
			}
		}
	}

	const FEnemyResolvedEntry* FSovietLayoutContext::TryPlaceYardNet(const FSovietYardFrame& Frame, const TArray<FEnemyResolvedEntry>& Nets,
	                                                               const TArray<double, TInlineAllocator<2>>& Depths, const double Offset,
	                                                               const uint8 Mask, const int32 Cluster, FEnemyFootprint& OutFootprint)
	{
		for (const FEnemyResolvedEntry& Net : Nets)
		{
			for (const double Depth : Depths)
			{
				for (const double Turn : {0.0, UE_DOUBLE_HALF_PI})
				{
					const FVector2D Spot = Frame.Center + Frame.Front * Depth + Frame.Along * Offset;
					if (Builder.PlaceCentered(EEnemyPlacementCategory::Decorator, {Net}, 0, Spot, SovietLayout_Yaw(Frame.Along) + Turn, 0.0,
					                          Mask, Cluster, OutFootprint))
					{
						return &Net;
					}
				}
			}
		}
		return nullptr;
	}

	FString FSovietLayoutContext::DescribeFenceBlock(const FEnemyFootprint& Footprint, const uint8 Mask) const
	{
		static const TCHAR* const CategoryNames[] = {
			TEXT("building"), TEXT("sandbag"), TEXT("hedgehog"), TEXT("wire"), TEXT("decorator"), TEXT("street")
		};
		TArray<FString> Causes;
		if (not Builder.GetIsFootprintFree(Footprint, 0))
		{
			// The builder's predicates fail before any category is tested, so name them and stop.
			return GetIsInForeignYard(Footprint) ? TEXT("other_yard") : TEXT("terrain");
		}
		for (int32 Category = 0; Category < UE_ARRAY_COUNT(CategoryNames); ++Category)
		{
			const uint8 Bit = EnemyOccupancyMask(static_cast<EEnemyPlacementCategory>(Category));
			if ((Mask & Bit) != 0 and not Builder.GetIsFootprintFree(Footprint, Bit))
			{
				Causes.AddUnique(CategoryNames[Category]);
			}
		}
		return Causes.IsEmpty() ? TEXT("unknown") : FString::Join(Causes, TEXT("+"));
	}

	int32 FSovietLayoutContext::CountBlockedFenceSlots(const FSurvivalSovietWallFamily& Family, const FVector2D& Start,
	                                                   const FVector2D& End) const
	{
		const FVector2D Walk = (End - Start).GetSafeNormal();
		const uint8 Mask = EnemySolidCategoryMask() & ~EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog);
		int32 Blocked = 0;
		for (const FSovietWallSlot& Slot : MakeWallSlots(Family, FVector2D::Distance(Start, End), false, false))
		{
			FEnemyFootprint Footprint;
			Footprint.Center = Start + Walk * (Slot.Start + Slot.Length * 0.5);
			Footprint.HalfExtents = FVector2D(Slot.Length * 0.5, Family.Thickness * 0.5);
			Footprint.YawRadians = SovietLayout_Yaw(Walk);
			Blocked += Builder.GetIsFootprintFree(Footprint, Mask) ? 0 : 1;
		}
		return Blocked;
	}

	int32 FSovietLayoutContext::FindBlockedYardSide(const FSurvivalSovietWallFamily& Family, const FSovietYardFrame& Frame) const
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D& C = Frame.Center;
		const FVector2D FrontLeft = C + Frame.Front * Frame.HalfDepth - Frame.Along * Frame.HalfLength;
		const FVector2D FrontRight = C + Frame.Front * Frame.HalfDepth + Frame.Along * Frame.HalfLength;
		const FVector2D BackRight = C - Frame.Front * Frame.HalfDepth + Frame.Along * Frame.HalfLength;
		const FVector2D BackLeft = C - Frame.Front * Frame.HalfDepth - Frame.Along * Frame.HalfLength;
		const FVector2D Entrance = C + Frame.Front * Frame.HalfDepth;
		// Sides as in FitYard: 0 the street side (two spans beside the entrance), 1 the back, 2 the +Along end, 3 the -Along end.
		if (CountBlockedFenceSlots(Family, FrontLeft, Entrance - Frame.Along * YardEntranceHalfWidth) > 0
			or CountBlockedFenceSlots(Family, Entrance + Frame.Along * YardEntranceHalfWidth, FrontRight) > 0)
		{
			return 0;
		}
		// A blocked corner piece: pull in the side that runs along the street-facing axis first, as FitYard does.
		if (Family.Corner.GetIsSet())
		{
			const double Arm = Family.Corner.Length;
			const auto IsCornerBlocked = [this, Arm](const FVector2D& Corner, const FVector2D& ArmA, const FVector2D& ArmB)
			{
				FEnemyFootprint Footprint;
				Footprint.Center = Corner + (ArmA + ArmB) * (Arm * 0.5);
				Footprint.HalfExtents = FVector2D(Arm * 0.5, Arm * 0.5);
				Footprint.YawRadians = SovietLayout_Yaw(ArmA);
				return not Builder.GetIsFootprintFree(Footprint, EnemySolidCategoryMask() & ~EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog));
			};
			if (IsCornerBlocked(FrontLeft, Frame.Along, -Frame.Front) or IsCornerBlocked(FrontRight, -Frame.Along, -Frame.Front))
			{
				return 0;
			}
			if (IsCornerBlocked(BackRight, -Frame.Along, Frame.Front) or IsCornerBlocked(BackLeft, Frame.Along, Frame.Front))
			{
				return 1;
			}
		}
		if (CountBlockedFenceSlots(Family, BackRight, BackLeft) > 0)
		{
			return 1;
		}
		if (CountBlockedFenceSlots(Family, FrontRight, BackRight) > 0)
		{
			return 2;
		}
		if (CountBlockedFenceSlots(Family, BackLeft, FrontLeft) > 0)
		{
			return 3;
		}
		return INDEX_NONE;
	}

	int32 FSovietLayoutContext::TightenYardForFence(const FSurvivalSovietWallFamily& Family, FSovietYardFrame& Frame) const
	{
		using namespace SovietBaseLayoutConstants;
		const FSovietYardFrame Original = Frame;
		for (int32 Step = 0; Step < YardTightenSteps; ++Step)
		{
			const int32 Side = FindBlockedYardSide(Family, Frame);
			if (Side == INDEX_NONE)
			{
				return Step;
			}
			const bool bDepthSide = Side < 2;
			double& Half = bDepthSide ? Frame.HalfDepth : Frame.HalfLength;
			if (2.0 * Half - YardShrinkStep < 2.0 * YardMinExtent)
			{
				break;
			}
			// Pull only that side in: the opposite side stays where it is.
			const FVector2D Normals[4] = {Frame.Front, -Frame.Front, Frame.Along, -Frame.Along};
			Half -= YardShrinkStep * 0.5;
			Frame.Center -= Normals[Side] * (YardShrinkStep * 0.5);
		}
		// Still blocked after the allowed steps: keep the planned yard and let the gaps show (and be reported).
		if (FindBlockedYardSide(Family, Frame) != INDEX_NONE)
		{
			Frame = Original;
			return -1;
		}
		return YardTightenSteps;
	}

	void FSovietLayoutContext::AddYardPaths(const FSovietYardFrame& Frame, const FSovietYardFrame& RowFrame, const bool bDeep,
	                                        const TArray<double>& RowDepths, FSurvivalSovietBasePlan& OutPlan)
	{
		using namespace SovietBaseLayoutConstants;
		if (RowDepths.IsEmpty())
		{
			return;
		}
		const auto AddPath = [&OutPlan](const FVector2D& From, const FVector2D& To)
		{
			FSurvivalSovietPlannedRoad& Path = OutPlan.Roads.AddDefaulted_GetRef();
			Path.Kind = ESurvivalSovietRoadKind::Track;
			Path.Width = YardPathWidth;
			Path.Points = {From, To};
		};
		// Worn earth from outside the entrance down the middle, and along every aisle between the rows.
		const double Back = bDeep ? -Frame.HalfDepth + YardInnerMargin : RowDepths[0];
		AddPath(Frame.Center + Frame.Front * (Frame.HalfDepth + YardPathOutside), Frame.Center + Frame.Front * Back);
		const double Reach = RowFrame.HalfLength - YardInnerMargin;
		for (int32 Row = 0; Row + 1 < RowDepths.Num(); ++Row)
		{
			const double Mid = (RowDepths[Row] + RowDepths[Row + 1]) * 0.5;
			if (bDeep and FMath::Abs(Mid) < YardAisle)
			{
				// The entrance path already runs down the middle aisle.
				continue;
			}
			const FVector2D Middle = RowFrame.Center + RowFrame.Front * Mid;
			AddPath(Middle - RowFrame.Along * Reach, Middle + RowFrame.Along * Reach);
		}
	}

	// ------------------------------------------------------------------
	// Lanterns: beside every gate, street junction and yard entrance, never on a street
	// ------------------------------------------------------------------

	double FSovietLayoutContext::GetPaintHalfWidth(const ESurvivalSovietRoadKind Kind, const double Width) const
	{
		switch (Kind)
		{
		case ESurvivalSovietRoadKind::Main:
			return Width * 0.5 * Kit.MainRoadPaintShare;
		case ESurvivalSovietRoadKind::Track:
			return Width * 0.5 * Kit.TrackRoadPaintShare;
		case ESurvivalSovietRoadKind::Strip:
			return Width * 0.5;
		case ESurvivalSovietRoadKind::Lane:
		case ESurvivalSovietRoadKind::None:
		default:
			return Width * 0.5 * Kit.LaneRoadPaintShare;
		}
	}

	bool FSovietLayoutContext::GetIsOnPaintedRoad(const TArray<FSurvivalSovietPlannedRoad>& Roads, const FVector2D& Point,
	                                              const double Margin) const
	{
		for (const FSurvivalSovietPlannedRoad& Road : Roads)
		{
			const double Reach = GetPaintHalfWidth(Road.Kind, Road.Width) + Margin;
			for (int32 Index = 0; Index + 1 < Road.Points.Num(); ++Index)
			{
				const FVector2D& A = Road.Points[Index];
				const FVector2D Along = (Road.Points[Index + 1] - A).GetSafeNormal();
				const double Clamped = FMath::Clamp(FVector2D::DotProduct(Point - A, Along), 0.0, FVector2D::Distance(A, Road.Points[Index + 1]));
				if (FVector2D::Distance(Point, A + Along * Clamped) < Reach)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool FSovietLayoutContext::PlaceLantern(const TArray<FSurvivalSovietPlannedRoad>& Roads, const FVector2D& Position, const TCHAR* Lights,
	                                        const FVector2D& Target)
	{
		using namespace SovietBaseLayoutConstants;
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList({Kit.Lantern});
		if (Resolved.IsEmpty() or GetIsOnPaintedRoad(Roads, Position, LanternRoadClearance))
		{
			return false;
		}
		FEnemyFootprint Footprint;
		Footprint.Center = Position;
		Footprint.HalfExtents = Resolved[0].FootprintHalfExtents;
		if (not Builder.GetIsFootprintFree(Footprint, EnemySolidCategoryMask() | EnemyOccupancyMask(EEnemyPlacementCategory::Decorator)))
		{
			return false;
		}
		const FVector2D Toward = (Target - Position).GetSafeNormal();
		Builder.PlaceAtPivot(EEnemyPlacementCategory::Decorator, Resolved[0], Position, SovietLayout_Yaw(Toward), Footprint,
		                     BeginCluster(ESurvivalSovietStructureRole::Fortification));
		FSurvivalSovietPlannedLantern& Lantern = Lanterns.AddDefaulted_GetRef();
		Lantern.Position = Position;
		Lantern.Lights = Lights;
		Lantern.Target = Target;
		return true;
	}

	void FSovietLayoutContext::PlaceLanternNear(const TArray<FSurvivalSovietPlannedRoad>& Roads, const TArray<FVector2D>& Candidates,
	                                            const TCHAR* Lights, const FVector2D& Target)
	{
		for (const FVector2D& Candidate : Candidates)
		{
			if (PlaceLantern(Roads, Candidate, Lights, Target))
			{
				return;
			}
		}
	}

	void FSovietLayoutContext::PlaceLanterns(const FSurvivalSovietBasePlan& Plan)
	{
		using namespace SovietBaseLayoutConstants;
		const TArray<FSurvivalSovietPlannedRoad>& Roads = Plan.Roads;
		for (const FSurvivalSovietPlannedGate& Gate : BuiltGates)
		{
			// Inside the gate, beside the road: first on the left of the way in, then on the right.
			const FVector2D Side(-Gate.Outward.Y, Gate.Outward.X);
			const FVector2D Inside = Gate.Center - Gate.Outward * LanternGateInset;
			const double Beside = GetPaintHalfWidth(ESurvivalSovietRoadKind::Main, LanternMainWidth) + LanternRoadClearance + LanternBesideRoad;
			PlaceLanternNear(Roads, {Inside + Side * Beside, Inside - Side * Beside, Inside + Side * (Beside + LanternStep),
			                         Inside - Side * (Beside + LanternStep)}, TEXT("gate"), Gate.Center);
		}
		for (int32 RoadIndex = 0; RoadIndex < Roads.Num(); ++RoadIndex)
		{
			const FSurvivalSovietPlannedRoad& Road = Roads[RoadIndex];
			if (Road.Kind == ESurvivalSovietRoadKind::Track or Road.Points.Num() < 2)
			{
				continue;
			}
			for (const bool bAtEnd : {false, true})
			{
				const FVector2D End = bAtEnd ? Road.Points.Last() : Road.Points[0];
				const FVector2D Into = (End - (bAtEnd ? Road.Points[Road.Points.Num() - 2] : Road.Points[1])).GetSafeNormal();
				PlaceJunctionLantern(Roads, RoadIndex, End, Into);
			}
		}
		for (int32 YardIndex = 0; YardIndex < Yards.Num(); ++YardIndex)
		{
			const FSurvivalSovietPlannedYard& Yard = Yards[YardIndex];
			// The yard's own ground is open to its lantern.
			ActiveYard = YardIndex;
			const FVector2D Along = SovietLayout_DirFromYaw(Yard.YawRadians);
			const FVector2D Front(Along.Y, -Along.X);
			const FVector2D Out = FVector2D::DotProduct(Front, Yard.Entrance - Yard.Center) < 0.0 ? -Front : Front;
			const double Beside = YardEntranceHalfWidth + LanternRoadClearance + LanternBesideRoad;
			// Outside beside the entrance, else just inside the fence where the street verge is in the way.
			// Outside beside the entrance, then hugging the fence (a lane verge often reaches the fence line), then just inside.
			TArray<FVector2D> Spots;
			for (const double OutDistance : {LanternYardOut, LanternYardHug, -LanternYardOut})
			{
				for (const double AlongDistance : {Beside, Beside + LanternYardStep, Beside + 2.0 * LanternYardStep})
				{
					Spots.Add(Yard.Entrance + Out * OutDistance + Along * AlongDistance);
					Spots.Add(Yard.Entrance + Out * OutDistance - Along * AlongDistance);
				}
			}
			PlaceLanternNear(Roads, Spots, TEXT("yard"), Yard.Entrance);
			ActiveYard = INDEX_NONE;
		}
	}

	void FSovietLayoutContext::PlaceJunctionLantern(const TArray<FSurvivalSovietPlannedRoad>& Roads, const int32 RoadIndex,
	                                                const FVector2D& End, const FVector2D& Into)
	{
		using namespace SovietBaseLayoutConstants;
		const FSurvivalSovietPlannedRoad& Road = Roads[RoadIndex];
		for (int32 OtherIndex = 0; OtherIndex < Roads.Num(); ++OtherIndex)
		{
			const FSurvivalSovietPlannedRoad& Other = Roads[OtherIndex];
			if (OtherIndex == RoadIndex or Other.Kind == ESurvivalSovietRoadKind::Track)
			{
				continue;
			}
			const TArray<FSurvivalSovietPlannedRoad> Single = {Other};
			if (not GetIsOnPaintedRoad(Single, End, JunctionLanternTolerance))
			{
				continue;
			}
			// In the corner of the mouth: back along this road past the other's verge, then aside past this road's verge.
			const double OwnHalf = GetPaintHalfWidth(Road.Kind, Road.Width);
			const double OtherHalf = GetPaintHalfWidth(Other.Kind, Other.Width);
			const FVector2D Aside(-Into.Y, Into.X);
			// On the verge of the corner, within 8 m of the junction: just past each painted edge.
			const double BackBy = OtherHalf * LanternVergeShare + LanternBesideRoad;
			const double Out = OwnHalf * LanternVergeShare + LanternBesideRoad;
			const FVector2D Near = End - Into * BackBy;
			const FVector2D Far = End + Into * BackBy;
			// Corner spots first, then small steps back along this road and out from it, so a blocked corner still keeps
			// its lantern within 8 m of the junction.
			TArray<FVector2D> Spots = {Near + Aside * Out, Near - Aside * Out, Far + Aside * Out, Far - Aside * Out};
			for (const double Step : {LanternJunctionStep, 2.0 * LanternJunctionStep, LanternStep})
			{
				for (const FVector2D& Base : {Near - Into * Step, Near})
				{
					const double Aside2 = Base == Near ? Out + Step : Out;
					Spots.Add(Base + Aside * Aside2);
					Spots.Add(Base - Aside * Aside2);
				}
			}
			PlaceLanternNear(Roads, Spots, TEXT("junction"), End);
			return;
		}
	}



	void FSovietLayoutContext::DressPlot(const FSovietPlot& Plot)
	{
		using namespace SovietBaseLayoutConstants;
		if (Plot.bBerm)
		{
			PlaceBerm(Plot);
		}
		switch (Plot.Dressing)
		{
		case ESovietPlotDressing::Huts:
			FillPlotRows(Plot, Kit.Huts);
			break;
		case ESovietPlotDressing::Hangars:
			FillPlotRows(Plot, Kit.Hangars);
			break;
		case ESovietPlotDressing::Crates:
			FillPlotGrid(Plot, Kit.Crates, CratePitch, CrateChance);
			break;
		case ESovietPlotDressing::Munitions:
			FillPlotGrid(Plot, Kit.Munitions, MunitionsPitch, MunitionsChance);
			break;
		case ESovietPlotDressing::Barrels:
			FillPlotGrid(Plot, Kit.Barrels, BarrelPitch, BarrelChance);
			break;
		case ESovietPlotDressing::Workshop:
			FillPlotGrid(Plot, Kit.Materials, WorkshopPitch, WorkshopChance);
			FillPlotGrid(Plot, Kit.Wrecks, WorkshopPitch, WorkshopChance * 0.5);
			break;
		case ESovietPlotDressing::Parade:
			PlaceParadeFlags(Plot);
			break;
		case ESovietPlotDressing::None:
		default:
			break;
		}
	}

	void FSovietLayoutContext::FillPlotGrid(
		const FSovietPlot& Plot,
		const TArray<FEnemyBlueprintEntry>& Entries,
		const double Pitch,
		const double Chance)
	{
		using namespace SovietBaseLayoutConstants;
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList(Entries);
		if (Resolved.IsEmpty())
		{
			return;
		}
		const FVector2D Center = ToWorld(Plot.Center);
		const FVector2D Front = DirectionToWorld(Plot.Front).GetSafeNormal();
		const FVector2D Along(-Front.Y, Front.X);
		const int32 Columns = FMath::FloorToInt32(2.0 * Plot.HalfLength * Unit / Pitch);
		const int32 Rows = FMath::FloorToInt32(2.0 * Plot.HalfDepth * Unit / Pitch);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		const uint8 Mask = EnemySolidCategoryMask() | EnemyOccupancyMask(EEnemyPlacementCategory::Decorator);
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				if (Random.FRand() > Chance)
				{
					continue;
				}
				const FVector2D Position = Center + Along * ((Column - (Columns - 1) * 0.5) * Pitch)
					+ Front * ((Row - (Rows - 1) * 0.5) * Pitch);
				FEnemyFootprint Footprint;
				Builder.PlaceCentered(EEnemyPlacementCategory::Decorator, Resolved, INDEX_NONE, Position,
				                      SovietLayout_Yaw(Along), PropClearance, Mask, Cluster, Footprint);
			}
		}
	}

	void FSovietLayoutContext::FillPlotRows(const FSovietPlot& Plot, const TArray<FEnemyBlueprintEntry>& Entries)
	{
		using namespace SovietBaseLayoutConstants;
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList(Entries);
		if (Resolved.IsEmpty())
		{
			return;
		}
		const FVector2D Center = ToWorld(Plot.Center);
		const FVector2D Front = DirectionToWorld(Plot.Front).GetSafeNormal();
		const FVector2D Along(-Front.Y, Front.X);
		const FEnemyResolvedEntry& Entry = Resolved[0];
		const double Yaw = GetBuildingYaw(Entry, Front, Along, Plot.Facing);
		FEnemyFootprint Rotated;
		Rotated.HalfExtents = Entry.FootprintHalfExtents;
		Rotated.YawRadians = Yaw;
		const double Width = 2.0 * Rotated.SupportRadius(Along) + HutGap;
		const double Depth = 2.0 * Rotated.SupportRadius(Front) + HutGap;
		const int32 Columns = FMath::FloorToInt32((2.0 * Plot.HalfLength * Unit + HutGap) / Width);
		const int32 Rows = FMath::FloorToInt32((2.0 * Plot.HalfDepth * Unit + HutGap) / Depth);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		DressedBuildingClusters.Add(Cluster);
		ClusterZones.Add(Cluster, Plot.Zone);
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const FVector2D Position = Center + Along * ((Column - (Columns - 1) * 0.5) * Width)
					+ Front * (Plot.HalfDepth * Unit - (Row + 0.5) * Depth);
				FEnemyFootprint Footprint;
				Builder.PlaceCentered(EEnemyPlacementCategory::Decorator, Resolved, 0, Position, Yaw, BuildingClearance,
				                      EnemySolidCategoryMask() | EnemyOccupancyMask(EEnemyPlacementCategory::Decorator),
				                      Cluster, Footprint);
			}
		}
	}

	void FSovietLayoutContext::PlaceBerm(const FSovietPlot& Plot)
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Center = ToWorld(Plot.Center);
		const FVector2D Front = DirectionToWorld(Plot.Front).GetSafeNormal();
		const FVector2D Along(-Front.Y, Front.X);
		const double Long = Plot.HalfLength * Unit + BermMargin;
		const double Deep = Plot.HalfDepth * Unit + BermMargin;
		const FVector2D FrontLeft = Center + Front * Deep - Along * Long;
		const FVector2D FrontRight = Center + Front * Deep + Along * Long;
		const FVector2D RearLeft = Center - Front * Deep - Along * Long;
		const FVector2D RearRight = Center - Front * Deep + Along * Long;
		const TArray<FEnemyResolvedEntry> Sandbags = ResolveList(Kit.SandbagRuns);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		const uint8 Mask = EnemySolidCategoryMask();
		Builder.PlaceSegmentRun(EEnemyPlacementCategory::Sandbag, Sandbags, RearLeft, RearRight, -Front, Mask, false, Cluster);
		Builder.PlaceSegmentRun(EEnemyPlacementCategory::Sandbag, Sandbags, FrontLeft, RearLeft, -Along, Mask, false, Cluster);
		Builder.PlaceSegmentRun(EEnemyPlacementCategory::Sandbag, Sandbags, FrontRight, RearRight, Along, Mask, false, Cluster);
	}

	void FSovietLayoutContext::PlaceParadeFlags(const FSovietPlot& Plot)
	{
		using namespace SovietBaseLayoutConstants;
		const TArray<FEnemyResolvedEntry> Flags = ResolveList(Kit.Flags);
		const FVector2D Center = ToWorld(Plot.Center);
		const FVector2D Front = DirectionToWorld(Plot.Front).GetSafeNormal();
		const FVector2D Along(-Front.Y, Front.X);
		const int32 Cluster = BeginCluster(ESurvivalSovietStructureRole::Fortification);
		for (const double Sign : {-1.0, 1.0})
		{
			const FVector2D Position = Center + Front * (Plot.HalfDepth * Unit - FlagInset)
				+ Along * (Sign * (Plot.HalfLength * Unit - FlagInset));
			FEnemyFootprint Footprint;
			Builder.PlaceCentered(EEnemyPlacementCategory::Decorator, Flags, INDEX_NONE, Position,
			                      SovietLayout_Yaw(Front), 0.0, EnemySolidCategoryMask(), Cluster, Footprint);
		}
	}

	// ------------------------------------------------------------------
	// Garrison
	// ------------------------------------------------------------------

	void FSovietLayoutContext::BuildPosts()
	{
		using namespace SovietBaseLayoutConstants;
		const FVector2D Center = Input.Center;
		const FVector2D Threat = ThreatDirection;
		// Threatened side first, so a small garrison stands where it is needed.
		InfantryPosts.StableSort([Center, Threat](const FSovietPost& Left, const FSovietPost& Right)
		{
			return FVector2D::DotProduct(Left.Position - Center, Threat)
				> FVector2D::DotProduct(Right.Position - Center, Threat);
		});
		for (const FVector2D& Node : StreetNodes)
		{
			// Beside the street, not on it.
			const FVector2D ToNode = Node - Center;
			const FVector2D Side(-ToNode.Y, ToNode.X);
			FSovietPost Post;
			Post.Position = Node + Side.GetSafeNormal() * StreetPostOffset;
			Post.Yaw = ThreatYaw;
			CorePosts.Add(Post);
		}
		VehiclePosts.StableSort([Center, Threat](const FSovietPost& Left, const FSovietPost& Right)
		{
			return FVector2D::DotProduct(Left.Position - Center, Threat)
				> FVector2D::DotProduct(Right.Position - Center, Threat);
		});
	}

	bool FSovietLayoutContext::GetIsSpotFree(const FVector2D& Position) const
	{
		using namespace SovietBaseLayoutConstants;
		FEnemyFootprint Footprint;
		Footprint.Center = Position;
		Footprint.HalfExtents = FVector2D(UnitFootprintHalfExtent, UnitFootprintHalfExtent);
		const uint8 SolidsWithoutStreets = EnemySolidCategoryMask() & ~EnemyOccupancyMask(EEnemyPlacementCategory::Reserved);
		return not Occupancy.OverlapsAny(Footprint, SolidsWithoutStreets)
			and (Input.Site == nullptr or Input.Site->GetIsFootprintBuildable(Footprint));
	}

	void FSovietLayoutContext::ReserveUnitSpot(const FVector2D& Position, const double Yaw)
	{
		using namespace SovietBaseLayoutConstants;
		FEnemyFootprint Footprint;
		Footprint.Center = Position;
		Footprint.HalfExtents = FVector2D(UnitFootprintHalfExtent, UnitFootprintHalfExtent);
		Footprint.YawRadians = Yaw;
		Occupancy.Add(Footprint, EEnemyPlacementCategory::Decorator);
	}

	bool FSovietLayoutContext::TryTakeFrom(TArray<FSovietPost>& Posts, FSovietPost& OutPost)
	{
		while (not Posts.IsEmpty())
		{
			const FSovietPost Post = Posts[0];
			Posts.RemoveAt(0);
			if (GetIsSpotFree(Post.Position))
			{
				OutPost = Post;
				return true;
			}
		}
		return false;
	}

	FSovietPost FSovietLayoutContext::FindFreeSpot()
	{
		using namespace SovietBaseLayoutConstants;
		FSovietPost Post;
		Post.Yaw = ThreatYaw;
		Post.Position = Input.Center;
		for (int32 Attempt = 0; Attempt < FreeSpotAttempts and not StreetNodes.IsEmpty(); ++Attempt)
		{
			const FVector2D Node = StreetNodes[Random.RandRange(0, StreetNodes.Num() - 1)];
			const FVector2D Spot = Node + SovietLayout_DirFromYaw(Random.FRandRange(0.0, 2.0 * PI)) * FreeSpotRadius;
			if (GetIsSpotFree(Spot))
			{
				Post.Position = Spot;
				return Post;
			}
		}
		return Post;
	}

	FSovietPost FSovietLayoutContext::TakePost(const ESurvivalSovietStepKind Kind, const bool bVehicle)
	{
		FSovietPost Post;
		const bool bHolds = Kind == ESurvivalSovietStepKind::HoldGarrison;
		if (bVehicle and bHolds and TryTakeFrom(VehiclePosts, Post))
		{
			return Post;
		}
		if (not bVehicle and bHolds and TryTakeFrom(InfantryPosts, Post))
		{
			return Post;
		}
		if (bVehicle and Kind == ESurvivalSovietStepKind::Reserve and TryTakeFrom(VehiclePosts, Post))
		{
			return Post;
		}
		if (TryTakeFrom(CorePosts, Post))
		{
			return Post;
		}
		return FindFreeSpot();
	}

	void FSovietLayoutContext::PlanGarrisonStep(const FSurvivalSovietThreatStep& Step, FSurvivalSovietBasePlan& OutPlan)
	{
		using namespace SovietBaseLayoutConstants;
		FSurvivalSovietPlannedGarrison Planned;
		Planned.Kind = Step.Kind;
		for (const FSovietUnitPurchase& Purchase : UnitPurchases)
		{
			if (Purchase.Step != &Step)
			{
				continue;
			}
			const bool bVehicle = Purchase.Unit.UnitType != EAllUnitType::UNType_Squad;
			const FSovietPost Post = TakePost(Step.Kind, bVehicle);
			ReserveUnitSpot(Post.Position, Post.Yaw);
			Planned.Units.Add(Purchase.Unit);
			Planned.SpawnPositions.Add(Post.Position);
			Planned.SpawnYaws.Add(Post.Yaw);
		}
		if (Planned.Units.IsEmpty())
		{
			return;
		}
		if (Step.Kind == ESurvivalSovietStepKind::PatrolGarrison and not StreetNodes.IsEmpty())
		{
			// Patrols walk the base's streets.
			const int32 PointCount = FMath::Clamp(PatrolPointCount, 3, StreetNodes.Num());
			const int32 Stride = FMath::Max(1, StreetNodes.Num() / PointCount);
			const int32 Phase = Random.RandRange(0, StreetNodes.Num() - 1);
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				Planned.PatrolPoints.Add(StreetNodes[(Phase + PointIndex * Stride) % StreetNodes.Num()]);
			}
		}
		OutPlan.Garrison.Add(MoveTemp(Planned));
	}

	void FSovietLayoutContext::PlanGarrison(FSurvivalSovietBasePlan& OutPlan)
	{
		BuildPosts();
		for (const FSurvivalSovietThreatStep& Step : Preset.ThreatPath)
		{
			PlanGarrisonStep(Step, OutPlan);
		}
	}

	// ------------------------------------------------------------------
	// Result
	// ------------------------------------------------------------------

	FVector2D FSovietLayoutContext::FindLaunchPoint() const
	{
		using namespace SovietBaseLayoutConstants;
		if (Master.Gates.IsEmpty())
		{
			return Input.Center + ThreatDirection * Input.BuildAreaRadius;
		}
		// The gate facing the player if there are several: attacks leave by the road towards him.
		const FSovietGate* Best = &Master.Gates[0];
		for (const FSovietGate& Gate : Master.Gates)
		{
			if (FVector2D::DotProduct(DirectionToWorld(Gate.Outward), ThreatDirection)
				> FVector2D::DotProduct(DirectionToWorld(Best->Outward), ThreatDirection))
			{
				Best = &Gate;
			}
		}
		return ToWorld(Best->Position) + DirectionToWorld(Best->Outward) * LaunchPointStandOff;
	}

	void FSovietLayoutContext::ConvertNewActors(FSurvivalSovietBasePlan& OutPlan)
	{
		for (; ConvertedActors < Result.Actors.Num(); ++ConvertedActors)
		{
			const FEnemyPlacedActor& Placed = Result.Actors[ConvertedActors];
			if (Placed.AssetIndex == INDEX_NONE)
			{
				continue;
			}
			FSurvivalSovietPlannedStructure& Structure = OutPlan.Structures.AddDefaulted_GetRef();
			Structure.ClassIndex = Placed.AssetIndex;
			Structure.Position = Placed.Position;
			Structure.YawRadians = Placed.YawRadians;
			Structure.UniformScale = Placed.UniformScale;
			Structure.StretchXY = ActorStretch.Contains(ConvertedActors) ? ActorStretch[ConvertedActors] : FVector2D(1.0, 1.0);
			const bool bIsBuilding = Placed.Category == EEnemyPlacementCategory::Bunker;
			Structure.Role = bIsBuilding and ClusterRoles.IsValidIndex(Placed.ClusterIndex)
				                 ? ClusterRoles[Placed.ClusterIndex]
				                 : ESurvivalSovietStructureRole::Fortification;
			Structure.ThreatPoints = bIsBuilding ? ClusterThreatPoints.FindRef(Placed.ClusterIndex) : 0;
		}
	}

	void FSovietLayoutContext::PlacePads(FSurvivalSovietBasePlan& OutPlan)
	{
		TArray<FEnemyBlueprintEntry> PadEntry;
		PadEntry.AddDefaulted();
		PadEntry[0].BlueprintClass = Kit.PadClass;
		PadEntry[0].bUseFootprintOverride = true;
		const TArray<FEnemyResolvedEntry> Resolved = ResolveList(PadEntry);
		for (const FEnemyPlacedActor& Placed : Result.Actors)
		{
			const bool bBuilding = Placed.Category == EEnemyPlacementCategory::Bunker or DressedBuildingClusters.Contains(Placed.ClusterIndex);
			if (not bBuilding)
			{
				continue;
			}
			OutPlan.BuildingsTotal += 1;
			const ESurvivalSovietStructureRole Role = ClusterRoles.IsValidIndex(Placed.ClusterIndex)
				                                          ? ClusterRoles[Placed.ClusterIndex]
				                                          : ESurvivalSovietStructureRole::Fortification;
			const bool bDefence = Role == ESurvivalSovietStructureRole::GunPit or Role == ESurvivalSovietStructureRole::PerimeterBunker;
			const ESurvivalSovietZone* Zone = ClusterZones.Find(Placed.ClusterIndex);
			const ESurvivalSovietPadKind* ZoneKind = Zone != nullptr ? Master.PadKinds.Find(*Zone) : nullptr;
			const ESurvivalSovietPadKind Kind = bDefence ? ESurvivalSovietPadKind::PackedEarth : ZoneKind != nullptr ? *ZoneKind : Master.DefaultPad;
			const FVector2D Half = Placed.Footprint.HalfExtents;
			if (Resolved.IsEmpty() or Kind == ESurvivalSovietPadKind::None or FMath::Max(Half.X, Half.Y) < Kit.PadMinHalfExtent)
			{
				continue;
			}
			FSurvivalSovietPlannedStructure& Pad = OutPlan.Structures.AddDefaulted_GetRef();
			Pad.ClassIndex = Resolved[0].OutputAssetIndex;
			Pad.Role = ESurvivalSovietStructureRole::Foundation;
			Pad.Position = Placed.Footprint.Center;
			Pad.YawRadians = Placed.Footprint.YawRadians;
			// Small items get a narrow margin: a fuel tank on a yard twice its size reads as a stamp.
			const double Margin = FMath::Clamp(FMath::Min(Half.X, Half.Y) * SovietBaseLayoutConstants::PadMarginShare,
			                                   SovietBaseLayoutConstants::PadMinMargin, static_cast<double>(Kit.PadMargin));
			Pad.StretchXY = Half + FVector2D(Margin, Margin);
			Pad.PadKind = Kind;
			OutPlan.BuildingsOnPad += 1;
			FSurvivalSovietPlannedPad& Record = OutPlan.Pads.AddDefaulted_GetRef();
			Record.BuildingClassIndex = Placed.AssetIndex;
			Record.Center = Pad.Position;
			Record.PadHalfExtents = Pad.StretchXY;
			Record.FootprintHalfExtents = Half;
			Record.YawRadians = Pad.YawRadians;
			Record.Kind = Kind;
		}
	}

	void FSovietLayoutContext::ValidateBunkerRule(FSurvivalSovietBasePlan& OutPlan) const
	{
		for (const FVector2D& Point : Master.PerimeterLine)
		{
			OutPlan.PerimeterLine.Add(ToWorld(Point));
		}
		for (const FEnemyPlacedActor& Placed : Result.Actors)
		{
			const bool bBunker = Placed.Category == EEnemyPlacementCategory::Bunker and ClusterRoles.IsValidIndex(Placed.ClusterIndex)
				and ClusterRoles[Placed.ClusterIndex] == ESurvivalSovietStructureRole::PerimeterBunker;
			if (not bBunker)
			{
				continue;
			}
			FVector2D Corners[4];
			Placed.Footprint.GetCorners(Corners);
			for (const FVector2D& Corner : Corners)
			{
				if (GetIsInsidePerimeter(Corner))
				{
					OutPlan.RuleViolations.Add(FString::Printf(TEXT("bunker inside the perimeter at (%.0f, %.0f)"), Placed.Position.X, Placed.Position.Y));
					break;
				}
			}
		}
		for (const FString& Violation : OutPlan.RuleViolations)
		{
			UE_LOG(LogTemp, Error, TEXT("SOVIET_RULE %s"), *Violation);
		}
	}

	void FSovietLayoutContext::BeginLayout()
	{
		// Bad ground, and the ground of a storage yard other than the one being filled, take no footprint.
		const FSurvivalSovietSiteMap* Site = Input.Site;
		Builder.SetBlockedFootprintPredicate([this, Site](const FEnemyFootprint& Footprint)
		{
			return (Site != nullptr and not Site->GetIsFootprintBuildable(Footprint)) or GetIsInForeignYard(Footprint)
				or GetIsInStoreFireBreak(Footprint);
		});
		ThreatBudget = Input.ThreatBudget;
		ReservePointsLeft = Input.ReserveBudget;
		ChooseAxis();
		BuildMasterPlan();
		PlanYards();
		BuildGunScanSpots();
		BuySignatures();
		BuyThreatPath(false);
	}

	void FSovietLayoutContext::FinishPlan(FSurvivalSovietBasePlan& OutPlan)
	{
		// Structures that found no spot gave their points back: the tier's own units get them.
		BuyThreatPath(true);
		PlanGarrison(OutPlan);
		OutPlan.Center = Input.Center;
		OutPlan.ThreatYawRadians = ThreatYaw;
		OutPlan.AxisYawRadians = AxisYaw;
		OutPlan.LaunchPoint = FindLaunchPoint();
		OutPlan.ProductionCount = ProductionPositions.Num();
		OutPlan.ThreatPoints = ThreatSpent;
		OutPlan.ReservePoints = ReservePointsSpent;
		OutPlan.ThreatBudget = ThreatBudget;
		OutPlan.UnspentPoints = ThreatBudget - ThreatSpent;
		OutPlan.DroppedSlots = DroppedSlots;
		OutPlan.EmplacementSpotsUsed = EmplacementSpotsUsed;
		OutPlan.EmplacementSpotsAvailable = Master.Emplacements.Num() + GunScanSpots.Num();
		OutPlan.PointsOutsideMainLine = PointsOutsideMainLine;
		OutPlan.RequiredCoreClusterDistance = SovietLayout_ComputeChainDistance(ProductionPositions);
		OutPlan.Wall = WallReport;
		OutPlan.WallPieces = WallPieces;
		OutPlan.Gates = BuiltGates;
		NoteBuildingsInsideYards();
		OutPlan.Yards = Yards;
		OutPlan.OpenWallSlots = OpenSlots;
		OutPlan.Lanterns = Lanterns;
		PlacePads(OutPlan);
		ValidateBunkerRule(OutPlan);
	}

	void FSovietLayoutContext::RunStage(const ESurvivalSovietPlanStage Stage, FSurvivalSovietBasePlan& OutPlan)
	{
		// Order of the stages = priority for space. Streets first: they are what keeps the base drivable.
		switch (Stage)
		{
		case ESurvivalSovietPlanStage::MasterPlan:
			BeginLayout();
			break;
		case ESurvivalSovietPlanStage::Streets:
			PlaceStreets(OutPlan);
			PlaceGates();
			// Before the buildings: a wall is the base's outline, and what cannot stand inside it does not push it around.
			PlaceWalls();
			break;
		case ESurvivalSovietPlanStage::Buildings:
			PlaceBuildings();
			break;
		case ESurvivalSovietPlanStage::VehiclePark:
			PlaceParkRows();
			break;
		case ESurvivalSovietPlanStage::Guns:
			PlaceEmplacements();
			break;
		case ESurvivalSovietPlanStage::Perimeter:
			PlaceFence();
			break;
		case ESurvivalSovietPlanStage::Dressing:
			DressPlots(OutPlan);
			break;
		case ESurvivalSovietPlanStage::Garrison:
		default:
			FinishPlan(OutPlan);
			break;
		}
		ConvertNewActors(OutPlan);
	}
}

/** @brief Keeps the input alive next to the context that refers to it. */
struct FSurvivalSovietBaseLayoutRun::FImpl
{
	explicit FImpl(const FSurvivalSovietLayoutInput& InInput)
		: Input(InInput)
		  , Context(Input)
	{
	}

	FSurvivalSovietLayoutInput Input;
	FSovietLayoutContext Context;
};

FSurvivalSovietBaseLayoutRun::FSurvivalSovietBaseLayoutRun(const FSurvivalSovietLayoutInput& Input)
	: M_Impl(MakeUnique<FImpl>(Input))
{
}

FSurvivalSovietBaseLayoutRun::~FSurvivalSovietBaseLayoutRun() = default;

void FSurvivalSovietBaseLayoutRun::RunStage(const ESurvivalSovietPlanStage Stage, FSurvivalSovietBasePlan& InOutPlan)
{
	M_Impl->Context.RunStage(Stage, InOutPlan);
}

int32 FSurvivalSovietBaseLayout::GetSlotCount(
	const FSurvivalSovietStructureSlot& Slot,
	const int32 Strength,
	const ERTSGameDifficulty Difficulty)
{
	const int32 DifficultySteps = static_cast<int32>(Difficulty) - static_cast<int32>(Slot.MinDifficulty);
	if (Strength < Slot.MinStrength or DifficultySteps < 0)
	{
		return 0;
	}
	const float RawCount = Slot.Count + Slot.CountPerStrength * (Strength - Slot.MinStrength)
		+ Slot.CountPerDifficulty * DifficultySteps;
	return FMath::Max(0, FMath::RoundToInt(RawCount));
}

int32 FSurvivalSovietBaseLayout::GetUnitThreatPoints(const FTrainingOption& Option)
{
	if (Option.UnitType == EAllUnitType::UNType_Squad)
	{
		return EnemyTrainingHelpers::GetSquadTrainingPointCost(static_cast<ESquadSubtype>(Option.SubtypeValue));
	}
	return EnemyTrainingHelpers::GetTankTrainingPointsCost(static_cast<ETankSubtype>(Option.SubtypeValue));
}

FSurvivalSovietBasePlan FSurvivalSovietBaseLayout::BuildPlan(const FSurvivalSovietLayoutInput& Input)
{
	FSurvivalSovietBasePlan Plan;
	if (Input.Preset == nullptr or Input.Kit == nullptr or not Input.ResolveEntry)
	{
		return Plan;
	}
	FSurvivalSovietBaseLayoutRun Run(Input);
	const ESurvivalSovietPlanStage Stages[] = {
		ESurvivalSovietPlanStage::MasterPlan, ESurvivalSovietPlanStage::Streets, ESurvivalSovietPlanStage::Buildings,
		ESurvivalSovietPlanStage::VehiclePark, ESurvivalSovietPlanStage::Guns, ESurvivalSovietPlanStage::Perimeter,
		ESurvivalSovietPlanStage::Dressing, ESurvivalSovietPlanStage::Garrison
	};
	for (const ESurvivalSovietPlanStage Stage : Stages)
	{
		Run.RunStage(Stage, Plan);
	}
	return Plan;
}
