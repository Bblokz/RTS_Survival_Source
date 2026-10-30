// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Procedural/CustomNodes/EnemyBaseSetup/PCGEnemyBaseSetupTypes.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "SurvivalSovietBaseTypes.generated.h"

/**
 * @brief What a spawned Soviet base structure is for; decides bookkeeping after spawning.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietStructureRole : uint8
{
	// Barracks / factory / research centre: unlocks enemy tech, anchors the strategic AI base cluster and the
	// base counts as destroyed once all of them are dead.
	Production,
	// Storage, towers and other buildings inside the base.
	Support,
	// Bunker of a perimeter line.
	PerimeterBunker,
	// Gun pit on a defense point (hill top, ramp head) or the gun pit ring.
	GunPit,
	// Sandbags, hedgehogs, barbed wire and decorator props.
	Fortification
};

/**
 * @brief How a garrison group behaves once it has spawned.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietGarrisonBehaviour : uint8
{
	// Stays where it spawned (facing the player's start location) and engages what comes in range.
	HoldPosition,
	// Patrols random points inside the base with attack-move support logic (enemy formation controller).
	Patrol,
	// Registered as idle direct-control unit on the strategic AI blackboard; the AI may pull it into its own
	// actions (counter attacks, flanks, retreats).
	StrategicAIReserve
};

/**
 * @brief Where the per-match generation seed comes from.
 */
UENUM(BlueprintType)
enum class ESurvivalSovietSeedMode : uint8
{
	// A new seed every match: bases differ every run.
	RandomPerMatch,
	// Always FixedSeed: identical bases every run (testing, daily challenge).
	Fixed
};

/**
 * @brief One building the base places (production, support or gun pit), gated by base strength.
 * @note Building is the shared EnemyBaseSetup entry: class, weight, optional footprint override, scale range.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietStructureSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure")
	FEnemyBlueprintEntry Building;

	// The slot is skipped on bases weaker than this (0 = weakest).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	int32 MinStrength = 0;

	// Production tier this building unlocks for the enemy AI (0 = barracks, 1 = light tank factory,
	// 2 = tier 2 research centre); -1 = not a production building. Difficulty caps the highest tier spawned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "-1"))
	int32 ProductionTier = -1;

	// Copies placed on a strength 0 base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0"))
	int32 Count = 1;

	// Extra copies per strength level above MinStrength (rounded).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Structure", meta = (ClampMin = "0.0"))
	float CountPerStrength = 0.f;
};

/**
 * @brief The fortified lines around the base core: bunker rows on a square rotated to face the player's start
 * location, each with a sandbag parapet, an obstacle belt and (from WireMinStrength) barbed wire.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietPerimeterSettings
{
	GENERATED_BODY()

	// Layout per base strength (index = strength, the last entry is used for higher strengths).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	TArray<EEnemyPerimeterLayout> LayoutPerStrength = {
		EEnemyPerimeterLayout::Corners, EEnemyPerimeterLayout::FrontLine, EEnemyPerimeterLayout::FullPerimeter
	};

	// Half size of the perimeter square as a fraction of the start location's build area radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter", meta = (ClampMin = "0.2", ClampMax = "0.7"))
	float HalfSizeFraction = 0.55f;

	// Amount (cm) each full side line is shortened (leaves corner gaps to drive through).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter", meta = (ClampMin = "0"))
	float LineMargin = 1800.f;

	// Corners layout: length (cm) of each corner strongpoint line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter", meta = (ClampMin = "0"))
	float CornerLineLength = 2600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	TArray<FEnemyBlueprintEntry> Bunkers;

	// Distance (cm) between bunker centres at defense density 1; divided by the difficulty defense density.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter", meta = (ClampMin = "300"))
	float BunkerSpacing = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter", meta = (ClampMin = "0"))
	float BunkerSpacingExtra = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	FEnemyObstacleBeltSettings ObstacleBelt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	FEnemyBarbedWireSettings BarbedWire;

	// Barbed wire is only laid on bases at least this strong.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter", meta = (ClampMin = "0"))
	int32 WireMinStrength = 1;

	// Sandbag parapet in front of every line (and around production buildings when enabled on the preset).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	FEnemySandbagSettings Sandbags;
};

/**
 * @brief A group of enemy units spawned inside the base after its structures are placed.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietGarrisonGroup
{
	GENERATED_BODY()

	// One option is picked (seeded) per unit; enemy-owned Soviet training options (tanks or squads).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison")
	TArray<FTrainingOption> UnitOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison", meta = (ClampMin = "0"))
	int32 Count = 2;

	// Extra units per strength level above MinStrength (rounded), before the difficulty garrison multiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison", meta = (ClampMin = "0.0"))
	float CountPerStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison", meta = (ClampMin = "0"))
	int32 MinStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison")
	ESurvivalSovietGarrisonBehaviour Behaviour = ESurvivalSovietGarrisonBehaviour::HoldPosition;

	// Patrol: radius of the patrol ring as a fraction of the build area radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison|Patrol",
		meta = (ClampMin = "0.1", ClampMax = "0.9", EditCondition = "Behaviour == ESurvivalSovietGarrisonBehaviour::Patrol"))
	float PatrolRadiusFraction = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison|Patrol",
		meta = (ClampMin = "3", EditCondition = "Behaviour == ESurvivalSovietGarrisonBehaviour::Patrol"))
	int32 PatrolPointCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison|Patrol",
		meta = (ClampMin = "1", EditCondition = "Behaviour == ESurvivalSovietGarrisonBehaviour::Patrol"))
	int32 IterationsAtPatrolPoint = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison|Patrol",
		meta = (ClampMin = "0.0", EditCondition = "Behaviour == ESurvivalSovietGarrisonBehaviour::Patrol"))
	float GuardSecondsPerIteration = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison|Patrol",
		meta = (ClampMin = "0.0", EditCondition = "Behaviour == ESurvivalSovietGarrisonBehaviour::Patrol"))
	float GuardSphereRadius = 600.f;
};

/**
 * @brief Resources the player receives when a base is destroyed (all its production buildings dead).
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietBaseReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	ERTSResourceType Resource = ERTSResourceType::Resource_Radixite;

	// Amount on a strength 0 base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0"))
	int32 Amount = 300;

	// Fraction of Amount added per strength level (0.5 = +50% per level).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "0.0"))
	float BonusFractionPerStrength = 0.5f;
};

/**
 * @brief One kind of Soviet base. A preset is picked (seeded, weighted) per base among those allowed for the
 * base's strength; the strength then decides which slots, lines and garrison groups appear.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietBasePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FName PresetName = NAME_None;

	// Shown in the "base destroyed" notification.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	int32 MinStrength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	int32 MaxStrength = 2;

	// Production buildings are placed on a ring of this radius (cm) around the deploy centre, the first one in
	// the middle. Keep it below half the strategic AI core cluster distance so they form one AI base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "500"))
	float CoreCompoundRadius = 1700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TArray<FSurvivalSovietStructureSlot> ProductionBuildings;

	// Wraps every production building in the perimeter sandbag wall from strength 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	bool bSandbagProductionBuildings = true;

	// Support buildings sit on a ring at this fraction of the build area radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support", meta = (ClampMin = "0.1", ClampMax = "0.8"))
	float SupportRingFraction = 0.33f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support")
	TArray<FSurvivalSovietStructureSlot> SupportBuildings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perimeter")
	FSurvivalSovietPerimeterSettings Perimeter;

	// Gun pits use the start location's defense points first (closest to the player side first), then a ring.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun Pits")
	TArray<FSurvivalSovietStructureSlot> GunPits;

	// Fallback ring for gun pits as a fraction of the build area radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun Pits", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float GunPitRingFraction = 0.8f;

	// Defense points farther than this fraction of the build area radius from the centre are ignored.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun Pits", meta = (ClampMin = "0.5"))
	float MaxDefensePointDistanceFraction = 1.6f;

	// Ammo / fuel props scattered around production, support and line clusters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decoration")
	TArray<FEnemyDecoratorProfile> DecoratorProfiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Garrison")
	TArray<FSurvivalSovietGarrisonGroup> Garrison;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward")
	TArray<FSurvivalSovietBaseReward> DestroyRewards;
};

/**
 * @brief How one difficulty scales every base.
 */
USTRUCT(BlueprintType)
struct FSurvivalSovietBaseDifficultyScaling
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;

	// Added to the distance-rank strength of every base (clamped to 0..MaxStrength).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	int32 StrengthBonus = 0;

	// Highest production tier spawned (0 = barracks only); caps the tech level the enemy AI can reach.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0"))
	int32 MaxProductionTier = 1;

	// Bunker density on the lines: spacing is divided by this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.25"))
	float DefenseDensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float GunPitMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float GarrisonMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", meta = (ClampMin = "0.0"))
	float RewardMultiplier = 1.f;
};
