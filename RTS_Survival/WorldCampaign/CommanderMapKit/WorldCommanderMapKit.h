// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitSettings.h"
#include "WorldCommanderMapKit.generated.h"

class UWorldCampaignProgressManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldCommanderMapAbilityUsed, EWorldCommanderMapAbility, Ability,
                                             const FWorldCommanderMapAbilityTarget&, Target);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldCommanderMapAbilityCooldownChanged, int32, TurnsRemaining);

// Native requests for ability effects owned by other world-map systems; the world player controller binds each one
// to the system that applies it. Parameters are already resolved from the commander's kit.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWorldCommanderWeakenTargetRequested, const FGuid& /*AnchorKey*/,
                                       int32 /*StrengthPercent*/, int32 /*Turns*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWorldCommanderRevealRegionRequested, const FVector& /*Location*/,
                                       float /*Radius*/, int32 /*Turns*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldCommanderRemoveFortificationRequested, const FGuid& /*AnchorKey*/,
                                     int32 /*FortificationCount*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldCommanderDamageDivisionRequested, const FGuid& /*DivisionKey*/,
                                     int32 /*Damage*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldCommanderRevealTargetStrengthRequested, const FGuid& /*AnchorKey*/,
                                     int32 /*Turns*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWorldCommanderStallStrengthGrowthRequested, const FGuid& /*AnchorKey*/,
                                       float /*Radius*/, int32 /*Turns*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldCommanderForcedMarchRequested, int32 /*ExtraSteps*/,
                                     int32 /*Turns*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldCommanderProtectBuildingRequested, FName /*BuildNodeId*/,
                                     int32 /*Turns*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldCommanderReinforceNextMissionRequested, int32 /*ExtraSquads*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldCommanderStallFreezeRequested, int32 /*Points*/);
/** A timed effect ran out at the start of a player turn; the system that applied it undoes it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldCommanderTimedEffectExpired, const FWorldCommanderTimedEffect&);

/**
 * @brief Added to the world player controller next to UWorldCampaignProgressManager. Other map systems ask it for
 * the commander's passive (GetPassive...) to scale their numbers; the UI calls TryUseMapAbility. Effects that need
 * only the progress state are applied here; the rest are broadcast as native requests the controller binds.
 * @note TickCooldownAtStartOfPlayerTurn: the controller calls it at the start of every player turn.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldCommanderMapKit : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldCommanderMapKit();

	// --- Kit ---

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander")
	ERTSCommander GetCommander() const;

	/**
	 * @brief The campaign commander's passive and map ability, for the UI.
	 * @param OutKit Untouched when the commander has no kit.
	 * @return false when there is no commander or no kit for it.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Commander")
	bool GetCommanderKit(FWorldCommanderMapKitDefinition& OutKit) const;

	// --- Passive queries; each returns the neutral value when the commander has no such passive ---

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	FWorldMapResources GetPassiveScaledBuildingCost(EWorldCampaignBuilding Building,
	                                               const FWorldMapResources& BaseCost) const;

	/** Multiplier on airfield and battery range and on the watch post reveal radius; 1 when neutral. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	float GetPassiveBuildingRangeMultiplier(EWorldCampaignBuilding Building) const;

	/** Multiplier on the strength a support building removes from missions in range; 1 when neutral. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	float GetPassiveStrengthReductionMultiplier(EWorldCampaignBuilding Building) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	FWorldMapResources GetPassiveScaledBuildingIncome(EWorldCampaignBuilding Building,
	                                                 const FWorldMapResources& BaseIncome) const;

	/** Range within which a building that normally does not support missions also lowers enemy strength; 0 = none. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	float GetPassiveMissionSupportRange(EWorldCampaignBuilding Building) const;

	/**
	 * @brief Operation reward after the commander's passive.
	 * @param BaseReward Reward before the passive.
	 * @param Target What kind of operation was won.
	 * @param bIsStoryMission Whether it was a pinned story mission.
	 * @return BaseReward when the passive does not apply.
	 */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	FWorldMapResources GetPassiveScaledOperationReward(const FWorldMapResources& BaseReward,
	                                                  EWorldOperationTarget Target, bool bIsStoryMission) const;

	/** Multiplier on the strength an enemy item adds to targets near it; 1 when neutral. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	float GetPassiveEnemyItemStrengthMultiplier(EMapEnemyItem EnemyItem) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	int32 GetPassiveIgnoredFortificationCount() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	int32 GetPassiveExtraBuildNodesPerCountry() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Passive")
	bool GetPassiveBuildingsFinishInstantly() const;

	// --- Map ability ---

	/**
	 * @brief Uses the commander's map ability on a target, applies or requests its effect and starts the cooldown.
	 * @param Target Where the player aimed; only the field the ability needs is read.
	 * @param OutFailReason Player-facing reason when nothing happened.
	 * @return true when the ability was used.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Commander|Ability")
	bool TryUseMapAbility(const FWorldCommanderMapAbilityTarget& Target, FText& OutFailReason);

	/**
	 * @brief Same checks as TryUseMapAbility without using it, for the ability button.
	 * @param Target Where the player aims.
	 * @param OutFailReason Player-facing reason when it cannot be used.
	 * @return true when TryUseMapAbility would succeed now.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Commander|Ability")
	bool GetCanUseMapAbility(const FWorldCommanderMapAbilityTarget& Target, FText& OutFailReason) const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Ability")
	EWorldCommanderAbilityTargetKind GetMapAbilityTargetKind() const;

	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Ability")
	int32 GetMapAbilityCooldownTurnsRemaining() const;

	/** Effects still running from earlier ability uses; the systems that apply them read this after a load. */
	UFUNCTION(BlueprintPure, Category = "World Campaign|Commander|Ability")
	TArray<FWorldCommanderTimedEffect> GetActiveTimedEffects() const;

	/** Counts the cooldown and the timed effects down by one turn; broadcasts expired effects. */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Commander|Ability")
	void TickCooldownAtStartOfPlayerTurn();

	// --- Events ---

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Commander")
	FOnWorldCommanderMapAbilityUsed OnMapAbilityUsed;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Commander")
	FOnWorldCommanderMapAbilityCooldownChanged OnMapAbilityCooldownChanged;

	FOnWorldCommanderWeakenTargetRequested OnWeakenTargetRequested;
	FOnWorldCommanderRevealRegionRequested OnRevealRegionRequested;
	FOnWorldCommanderRemoveFortificationRequested OnRemoveFortificationRequested;
	FOnWorldCommanderDamageDivisionRequested OnDamageDivisionRequested;
	FOnWorldCommanderRevealTargetStrengthRequested OnRevealTargetStrengthRequested;
	FOnWorldCommanderStallStrengthGrowthRequested OnStallStrengthGrowthRequested;
	FOnWorldCommanderForcedMarchRequested OnForcedMarchRequested;
	FOnWorldCommanderProtectBuildingRequested OnProtectBuildingRequested;
	FOnWorldCommanderReinforceNextMissionRequested OnReinforceNextMissionRequested;
	FOnWorldCommanderStallFreezeRequested OnStallFreezeRequested;
	FOnWorldCommanderTimedEffectExpired OnTimedEffectExpired;

protected:
	virtual void BeginPlay() override;

private:
	// Sibling component on the controller that owns the campaign progress state; the commander and cooldown are read
	// through it on every call so a restored save is always seen.
	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_ProgressManager;

	bool GetIsValidProgressManager() const;

	/** The commander's kit, or nullptr without a commander, kit or progress manager. */
	const FWorldCommanderMapKitDefinition* FindCommanderKit() const;

	/** The commander's passive, or an empty (neutral) one. */
	FWorldCommanderPassiveDefinition GetCommanderPassive() const;

	void ApplyMapAbilityEffect(const FWorldCommanderMapAbilityDefinition& Ability,
	                           const FWorldCommanderMapAbilityTarget& Target);
	void ApplyConvertRadixiteToBlueprints(const FWorldCommanderMapAbilityDefinition& Ability);
	void ApplyGrantResources(const FWorldCommanderMapAbilityDefinition& Ability);
	void RequestRevealRegion(const FWorldCommanderMapAbilityDefinition& Ability,
	                         const FWorldCommanderMapAbilityTarget& Target) const;
};
