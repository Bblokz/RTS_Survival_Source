// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionEnemyArmy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthContribution.h"
#include "EnemyItemSiteTypes.generated.h"

class AEnemyConvoyRoute;
class UStochasticDecisionTree;
struct FWorldMissionContext;

/**
 * @brief One thing a signature slot can spawn: a building, a strip section, a parked aircraft.
 * Used by AEnemyItemSignatureSlot and UEnemyItemSlotContentSet.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FEnemyItemSlotContent
{
	GENERATED_BODY()

	// Spawned at the slot as the slot's enemy player. Empty = this entry spawns nothing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Content")
	TSoftClassPtr<AActor> ActorClass;

	// Counts toward the operation's victory: the item is destroyed when every signature target is.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Content")
	bool bIsSignatureTarget = true;

	bool GetHasActorClass() const { return not ActorClass.IsNull(); }
};

/**
 * @brief Reusable "what goes on this kind of spot" table, so maps do not repeat the same per-item classes on every
 * slot (for example "large building spot": hangar for Airfield, tank hall for Factory, barracks for Barracks).
 * Assign it to AEnemyItemSignatureSlot::ContentSet; the slot's own entries win over it.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UEnemyItemSlotContentSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Content")
	TMap<EMapEnemyItem, FEnemyItemSlotContent> ContentByItem;

	// Spawned for item types without an entry above. Leave its class empty to spawn nothing then.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Content")
	FEnemyItemSlotContent FillerContent;

	UEnemyItemSlotContentSet();
};

/**
 * @brief How the enemy AI plays one enemy item type on a handmade map. Every part is optional; what is left empty
 * keeps the map's own enemy controller settings.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FEnemyItemSiteProfile
{
	GENERATED_BODY()

	// Copied into the enemy controller. Without any tree the AI trains nothing, so the map or this must give one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI")
	TObjectPtr<UStochasticDecisionTree> DecisionTree = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI")
	bool bOverrideTraining = false;

	// Which units the AI trains at its buildings and how fast; applied when bOverrideTraining is set.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy AI", meta = (EditCondition = "bOverrideTraining"))
	FEnemyMissionTrainingOverride Training;
};

/**
 * @brief One profile per enemy item type, shared by every handmade enemy item map through AEnemyItemSiteDirector,
 * and one per division type for the handmade field maps.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UEnemyItemSiteProfiles : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profiles")
	TMap<EMapEnemyItem, FEnemyItemSiteProfile> ProfileByItem;

	// Used on field maps, where a division or convoy is attacked instead of an item.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profiles")
	TMap<EWorldFieldDivisions, FEnemyItemSiteProfile> ProfileByDivision;
};

/** What a division army does once it stands at the enemy start of a field map. */
UENUM(BlueprintType)
enum class EEnemyDivisionArmyOrders : uint8
{
	// The units stay where they spawned and fight what comes in range.
	HoldStart UMETA(DisplayName = "Hold At Start"),
	// The units join the enemy AI's direct control; its decision tree attacks or defends with them.
	EnemyAIControl UMETA(DisplayName = "Enemy AI Control")
};

/**
 * @brief How AEnemyItemSiteDirector spawns the attacked division on a field map. The units themselves come from the
 * world map (FWorldMissionEnemyArmy); this only says how they stand and what they do.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FEnemyDivisionArmySettings
{
	GENERATED_BODY()

	FEnemyDivisionArmySettings();

	// Per division type; types without an entry use DefaultOrders.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Division Army")
	TMap<EWorldFieldDivisions, EEnemyDivisionArmyOrders> OrdersByDivision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Division Army")
	EEnemyDivisionArmyOrders DefaultOrders = EEnemyDivisionArmyOrders::EnemyAIControl;

	// Supply convoys drive this placed route with the world map's truck count; win when every truck is destroyed or
	// captured, lose when one escapes. Turn the route's Start On Begin Play off. Unset: the trucks park at the start.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Division Army")
	TObjectPtr<AEnemyConvoyRoute> ConvoyRoute = nullptr;

	// Spawned once per truck of a supply convoy when no ConvoyRoute is set (the world map only counts its trucks).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Division Army")
	FEnemyPlacementUnit ConvoyTruck;

	// Scales the ring spacing the units spawn on around the director; tanks need more room than squads.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Division Army", meta = (ClampMin = "0.5"))
	float UnitSpacingMultiplier = 2.f;

	EEnemyDivisionArmyOrders GetOrdersForDivision(EWorldFieldDivisions DivisionType) const;
};

/**
 * @brief What an enemy item site is built for: read from the world mission context, or from the director's test
 * values when the map was opened without the world map.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FEnemyItemSiteContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	EMapEnemyItem ItemType = EMapEnemyItem::None;

	// A division or convoy is attacked (a field map): its Army spawns instead of the item's base content.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	bool bIsDivisionOperation = false;

	// The attacked division's units; only used when bIsDivisionOperation is set.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site", meta = (EditCondition = "bIsDivisionOperation"))
	FWorldMissionEnemyArmy Army;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	int32 OperationSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	TArray<FWorldStrengthContribution> StrengthContributions;

	// True when the world map launched the operation, false for the director's test values.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Site")
	bool bFromMissionContext = false;

	int32 GetTotalStrengthPercentage() const;
	bool GetHasFortification(EWorldFortificationStrength Fortification) const;
};

/**
 * @brief When an AEnemyItemStrengthSlot switches on. All set conditions must hold.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FEnemyItemStrengthSlotCondition
{
	GENERATED_BODY()

	// The world map object must carry this fortification (bunkers, minefields, AA). None = no requirement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	EWorldFortificationStrength RequiredFortification = EWorldFortificationStrength::None;

	// The operation's summed strength percentage must reach this. 0 = no requirement.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (ClampMin = "0"))
	int32 MinimumTotalStrengthPercentage = 0;

	// Only for these item types; empty = every item type.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TArray<EMapEnemyItem> OnlyForItems;
};

/** Where the "destroy the signature targets" objective of an enemy item site stands. */
UENUM(BlueprintType)
enum class EEnemyItemSiteObjectiveState : uint8
{
	// The site director has not built the site yet.
	WaitingForSite,
	// The site is built but spawned no victory target; the objective cannot complete.
	NoTargets,
	InProgress,
	AllTargetsDestroyed,
	// A convoy truck escaped (exit or branch destination): the operation is lost.
	EnemyEscaped
};

/** What a convoy route battle knows about its one convoy; the director fills it, the rules decide the objective. */
USTRUCT()
struct RTS_SURVIVAL_API FEnemyConvoyBattleState
{
	GENERATED_BODY()

	// The route started the convoy (its units may still be spawning).
	bool bHasConvoyStarted = false;
	// Every unit spawned and the column formed; before that zero trucks is no win.
	bool bHasConvoySpawned = false;
	bool bHasConvoyFinished = false;
	// The route ended without starting a convoy (bad drive path, broken definition); reported by the route.
	bool bHasRouteFailed = false;
	EConvoyOutcome Outcome = EConvoyOutcome::None;
	// Trucks that spawned, from the finished convoy's outcome (trucks that never spawned are left out).
	int32 SpawnedTruckCount = 0;
	int32 EscapedTruckCount = 0;
	int32 BranchDeliveredTruckCount = 0;
};

/**
 * @brief Pure rules of enemy item sites, kept apart from the actors so they can be tested without a level.
 */
namespace EnemyItemSiteRules
{
	/**
	 * @brief Picks what a signature slot spawns for an item type. Order: the slot's own entry, the content set's
	 * entry, the slot's filler, the content set's filler. An entry with no class counts as missing.
	 * @param ItemType Item the site is built for.
	 * @param SlotContentByItem The slot's own per-item entries.
	 * @param SlotFiller The slot's own filler.
	 * @param ContentSet Optional shared table.
	 * @param OutContent The content to spawn.
	 * @return false when nothing should spawn on this slot for this item.
	 */
	RTS_SURVIVAL_API bool TryResolveSlotContent(EMapEnemyItem ItemType,
	                                            const TMap<EMapEnemyItem, FEnemyItemSlotContent>& SlotContentByItem,
	                                            const FEnemyItemSlotContent& SlotFiller,
	                                            const UEnemyItemSlotContentSet* ContentSet,
	                                            FEnemyItemSlotContent& OutContent);

	RTS_SURVIVAL_API bool GetShouldActivateStrengthSlot(const FEnemyItemStrengthSlotCondition& Condition,
	                                                    const FEnemyItemSiteContext& SiteContext);

	/**
	 * @brief Builds the site context from the world mission context when the world map launched an enemy item or
	 * division operation; otherwise (PIE startup settings, a direct level load) returns the director's test values.
	 * @param MissionContext Context the game instance holds.
	 * @param TestContext Values to use when the map was opened without the world map.
	 * @return The context the site is built for.
	 */
	RTS_SURVIVAL_API FEnemyItemSiteContext BuildSiteContext(const FWorldMissionContext& MissionContext,
	                                                        const FEnemyItemSiteContext& TestContext);

	/**
	 * @brief Decides the objective state, so zero living targets only counts as a win once targets were spawned.
	 * @param bIsSiteBuilt Whether the director has built the site.
	 * @param SpawnedTargetCount Victory targets the site spawned.
	 * @param LivingTargetCount Victory targets that are still alive.
	 * @return The objective state.
	 */
	RTS_SURVIVAL_API EEnemyItemSiteObjectiveState GetObjectiveState(bool bIsSiteBuilt, int32 SpawnedTargetCount,
	                                                                int32 LivingTargetCount);

	/**
	 * @brief The owner's convoy rule: any escaped or branch-delivered truck loses; the win needs the finished convoy
	 * to have lost every spawned truck to the player. Decided from truck fates, as escaped trucks are removed too.
	 * @param Battle The convoy's state.
	 * @return EnemyEscaped, AllTargetsDestroyed, NoTargets (route failed, aborted or no truck spawned), or waiting.
	 */
	RTS_SURVIVAL_API EEnemyItemSiteObjectiveState GetConvoyBattleObjectiveState(const FEnemyConvoyBattleState& Battle);

	/**
	 * @brief Turns a division's roster into the units a field map spawns: vehicles, then squads, then one truck per
	 * convoy truck. Entries left on None are skipped, so a bad roster entry cannot spawn a default unit.
	 * @param Army The division's units from the world map.
	 * @param ConvoyTruck What one convoy truck spawns as.
	 * @param OutUnits Receives the units to spawn, in spawn order.
	 * @return Roster entries that were skipped (None subtypes, or trucks while ConvoyTruck is not set).
	 */
	RTS_SURVIVAL_API int32 BuildArmySpawnList(const FWorldMissionEnemyArmy& Army,
	                                          const FEnemyPlacementUnit& ConvoyTruck,
	                                          TArray<FEnemyPlacementUnit>& OutUnits);
}
