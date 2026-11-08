// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalAbility_SpawnSettings.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/MissionPlayerGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/Tests/AllGlobalAbilities/AllGlobalAbilitiesUnlock.h"
#include "GlobalAbilitiesManager.generated.h"

USTRUCT(BlueprintType)
struct FEnemyGlobalAbilityAISettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ThinkStepInterval = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeTillAllowHeavyCarpetBombing = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeTillAllowBarrages = 0.0f;
};

USTRUCT(BlueprintType)
struct FEnemyGlobalAbilityLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGlobalAbility AbilityType = EGlobalAbility::GA_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStartOnCooldown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CooldownTimeOverride = 0;
};


// One built expansion making a manager's abilities recharge faster (FBxpData::GlobalAbilityCooldownMlt). Weak: an
// expansion that is gone without unregistering simply stops counting.
struct FGlobalAbilityCooldownSource
{
	TWeakObjectPtr<const AActor> Source;
	float CooldownMlt = 1.f;
};

class UW_GA_Description;
class UW_GA_Item;
class UGameUnitManager;
struct FTrainingOption;
class UPlayerResourceManager;
class UPlayerTechManager;
class UGlobalAbility;
class UW_GlobalAbilityPanel;

/**
 * @brief Owns a controller's loaded global abilities and maintains their shared runtime state.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGlobalAbilitiesManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UGlobalAbilitiesManager();
	
	void InitGlobalAbilitiesManager(const int32 OwningPlayer,
	                                const TArray<EGlobalAbility>& GlobalAbilities,
	                                UW_GlobalAbilityPanel* W_GlobalAbilityPanel, TObjectPtr<UPlayerResourceManager> PlayerResourceMgr, TObjectPtr<
		                                UGameUnitManager> GameUnitMgr, TObjectPtr<UPlayerTechManager> PlayerTechMgr, const FVector& PlayerStartLocation, float
	                                AircraftHeight);
	
	void SetupSpawnSettings(const FVector& PlayerStart, float AircraftHeightStart);

	/**
	 * @brief Ability preview only (commander selection, UAbilityPreviewDirector): a player manager without panel,
	 * resources or requirements, whose aircraft come in around an explicit area instead of the fog manager's map
	 * (the hangar map has none). Mission managers never call this.
	 * @param PreviewCentre Ground location the previewed abilities target.
	 * @param AircraftApproachDirection Horizontal direction from the centre towards the edge the aircraft come from.
	 * @param PreviewExtent Distance from the centre to that edge, along which the aircraft spawn.
	 * @param AircraftHeight World Z the aircraft spawn and leave at.
	 * @param SpawnLineHalfLength How far to either side of the edge centre aircraft spawn; small keeps every run on the
	 * approach line, so the preview can place targets under it (missions use half the map extent).
	 */
	void SetupForAbilityPreview(const FVector& PreviewCentre, const FVector& AircraftApproachDirection,
	                            const float PreviewExtent, const float AircraftHeight, const float SpawnLineHalfLength);

	bool GetIsAbilityPreviewManager() const { return bM_IsAbilityPreviewManager; }

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	bool IsPlayerAbilityManager() const;
	bool IsEnemyAbilityManager() const;
	/**
	 * @brief The mission decides which tiers of the commander's kit the player may use and whether they cost
	 * resources. Call before InitGlobalAbilitiesManager: abilities above the tier are not loaded, so the panel never
	 * shows them. Enemy managers ignore these settings.
	 * @param MissionSettings The mission manager's player global ability settings.
	 */
	void SetPlayerMissionSettings(const FMissionPlayerGlobalAbilitySettings& MissionSettings);
	bool GetAreResourceCostsDisabled() const;
	bool QueryRequirementForAbility(const UGlobalAbility* Ability) const;
	bool QueryTierAllowedForAbility(const UGlobalAbility* Ability) const;
	bool QueryUnitRequirementForAbility(const UGlobalAbility* Ability) const;
	bool QueryPlayerUnitRequirementForAbility(const FTrainingOption& RequiredUnit) const;
	bool QueryEnemyUnitRequirementForAbility(const FTrainingOption& RequiredUnit) const;
	bool QueryTechnologyRequirementForAbility(const UGlobalAbility* Ability) const;
	bool QueryCostsForAbility(const UGlobalAbility* Ability) const;
	bool QueryCooldownForAbility(const UGlobalAbility* Ability) const;
	void TickGlobalAbilityCooldowns();
	void ApplyCooldownOverride(UGlobalAbility* Ability, const FEnemyGlobalAbilityLoadoutEntry& LoadoutEntry);
	UGlobalAbility* FindLoadedAbilityByType(EGlobalAbility AbilityType) const;
	TArray<UGlobalAbility*> GetAvailableEnemyGlobalAbilities() const;
	bool TryExecuteAbilityAtLocation(UGlobalAbility* Ability, const FVector& TargetLocation);
	
	void OnHoveredAbilityButton(UGlobalAbility* HoveredAbility, const bool bIsHover);
	void OnClickedAbilityButton(UGlobalAbility* ClickedAbility);
	void OnAbilityFinishedExecuting(UGlobalAbility* Ability);
	bool TryPayForAbility(UGlobalAbility* Ability) const;
	bool GetHasActiveAbility() const;
	FVector GetAircraftBombingSpawnLocation(const UObject* Requester, const FVector& TargetLocation) const;
	FVector GetAircraftBombingRetreatLocation(const UObject* Requester, const FVector& TargetLocation) const;

	/**
	 * @brief A straight flight has to start on its own line, so it cannot use the random edge spawn of the other
	 * aircraft: walks back along the line to the map edge.
	 * @param LineStart Ground location where the run starts.
	 * @param FlightDirection Horizontal direction of the run.
	 * @param MaxApproachDistance Longest approach; keeps a run from taking too long on large maps.
	 * @return Spawn location on the line at the aircraft height.
	 */
	FVector GetStraightFlightSpawnLocation(const FVector& LineStart, const FVector& FlightDirection,
	                                       float MaxApproachDistance) const;

	/**
	 * @brief Lets a built expansion (the Cipher room) make these abilities recharge faster while it stands. Each
	 * ability's CoolDownTime becomes the effective cooldown (button fill, tooltip) and running cooldowns are rescaled.
	 * @param Source The expansion granting the bonus; registering it again only updates its multiplier.
	 * @param CooldownMlt The fraction of their cooldown the abilities take to recharge (0..1).
	 */
	void RegisterCooldownMlt(const AActor* Source, const float CooldownMlt);
	void UnregisterCooldownMlt(const AActor* Source);
	// 1 without a bonus; otherwise the lowest registered multiplier (bonuses do not stack).
	float GetCooldownMlt() const;

#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// RTS_SurvivalShippingGlobalAbilityTests only: the player has every ability, more than the panel's 8 slots, so
	// the panel shows one page of them at a time. Wraps around; refreshes the shown items' availability.
	void ShowAbilityPanelPage(int32 PageIndex);
	int32 GetAbilityPanelPage() const;
	int32 GetAbilityPanelPageCount() const;
	const TArray<TObjectPtr<UGlobalAbility>>& GetLoadedAbilities() const { return M_GlobalAbilities; }
#endif

private:
	// Built expansions that make the abilities recharge faster; the lowest multiplier counts.
	TArray<FGlobalAbilityCooldownSource> M_CooldownSources;

	// The multiplier the cooldowns were last scaled with, so a change rescales them exactly once.
	float M_AppliedCooldownMlt = 1.f;

	// Each loaded ability's cooldown without any bonus; CoolDownTime itself holds the effective (scaled) cooldown.
	UPROPERTY()
	TMap<TObjectPtr<UGlobalAbility>, int32> M_BaseCooldownTimes;

	void OnCooldownMltChanged();
	void ApplyCooldownMltToAbility(UGlobalAbility* Ability, const float CooldownMlt);

	FGlobalAbility_SpawnSettings M_SpawnSetings;

	// Set by SetupForAbilityPreview: abilities of this manager skip player controller feedback silently.
	bool bM_IsAbilityPreviewManager = false;

	// Preview managers only: half length of the aircraft spawn line (missions use half the map extent).
	float M_PreviewSpawnLineHalfLength = 0.f;

	// Abilities above the tier cap are never loaded for the player: they are hidden from the panel, not greyed out.
	UPROPERTY()
	FMissionPlayerGlobalAbilitySettings M_PlayerMissionSettings;
	// Player manager only: the template's tier is above the mission's cap, so the ability is not loaded.
	bool GetIsAbilityTierHiddenForPlayer(const UGlobalAbility* AbilityTemplate) const;
	UPROPERTY()
	int32 M_OwningPlayer;
	
	UPROPERTY()
	TWeakObjectPtr<UW_GlobalAbilityPanel> M_GlobalAbilityPanel;
	[[nodiscard]] bool EnsureIsValidGlobalAbilityPanel()const;
	
	UPROPERTY()
	TWeakObjectPtr<UW_GA_Description> M_GA_Description;
	[[nodiscard]] bool EnsureIsValidGlobalAbilityDescription()const;
	
	UPROPERTY()
	TWeakObjectPtr<UPlayerTechManager> M_PlayerTechManager;
	[[nodiscard]] bool EnsureIsValidPlayerTechManager()const;
	
	UPROPERTY()
	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;
	[[nodiscard]] bool EnsureIsValidPlayerResourceManager()const;
	
	UPROPERTY()
	TWeakObjectPtr<UGameUnitManager> M_GameUnitManager;
	[[nodiscard]] bool EnsureIsValidGameUnitManager()const;
	
	void LoadGlobalAbilities(TArray<EGlobalAbility> AbilityEnums);
	void OnLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects);
	
	void InitAbilityPanel(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities);

	void CheckRequirements();
	void StartPlayerRequirementTimer();
	void ClearPlayerRequirementTimer();
	void StartCooldownTimer();
	void ClearCooldownTimer();
	void ReleaseLoadedAbilities();
	void UpdateAbilityAvailability(UGlobalAbility* Ability);
	void UpdateAbilityItemAvailability(const UGlobalAbility* Ability, const bool bIsEnabled, const bool bUseGreyTint) const;
	FText GetHoverDescriptionForAbility(const UGlobalAbility* Ability) const;
	FString GetUnitRequirementDisplayName(const FTrainingOption& UnitId) const;
	// Returns true if the player has this unit.
	bool CheckDoesPlayerHaveUnit(const FTrainingOption& UnitId)const;
	bool CheckDoesEnemyHaveUnit(const FTrainingOption& UnitId) const;
	bool CheckDoesPlayerHaveSquad(const FTrainingOption& UnitId) const;
	bool CheckDoesPlayerHaveTank(const FTrainingOption& UnitId) const;
	bool CheckDoesPlayerHaveNomadic(const FTrainingOption& UnitId) const;
	bool CheckDoesPlayerHaveBuildingExpansion(const FTrainingOption& UnitId) const;
	bool CheckDoesPlayerHaveAircraft(const FTrainingOption& UnitId) const;
	bool CheckDoesEnemyHaveSquad(const FTrainingOption& UnitId) const;
	bool CheckDoesEnemyHaveTank(const FTrainingOption& UnitId) const;
	bool CheckDoesEnemyHaveNomadic(const FTrainingOption& UnitId) const;
	bool CheckDoesEnemyHaveBuildingExpansion(const FTrainingOption& UnitId) const;
	bool CheckDoesEnemyHaveAircraft(const FTrainingOption& UnitId) const;
	
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_GlobalAbilities;

	FTimerHandle M_CheckRequirementsTimerHandle;
	FTimerHandle M_CooldownTimerHandle;

	// Half length of the line aircraft spawn and retreat along: half the map extent, or the preview's own value.
	float GetAircraftSpawnLineHalfLength() const;

	bool TryGenerateLocationOnAircraftEdgeLine(const FVector& AircraftEdgeCenterLocation,
													const FVector& FowManagerPosition, const FVector& PlayerStart,
													float SpawnLineHalfLength, float AircraftHeightStart,
													FVector& OutSpawnLocation) const;
	
};
