// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Player/Abilities.h"
#include "EnemyUnitAbilityAIComponent.generated.h"

class AEnemyController;
class AMissionManager;
class ICommands;
class UGrenadeComponent;
class UEnemyUnitAbilityAISettings;
struct FUnitAbilityEntry;
enum class EBehaviourAbilityType : uint8;
enum class EEnemyGrenadeUsage : uint8;

#if !UE_BUILD_SHIPPING
// Test hook: an enemy unit used an ability; the location is the aim point for grenades and zero for behaviours.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnEnemyUnitAbilityUsedForTest, AActor* /*UnitActor*/, EAbilityID /*Ability*/,
                                       const FVector& /*AimLocation*/);
#endif

// Why a squad with a ready grenade did not throw it; counted for tuning and the map test.
enum class EEnemyGrenadeRejection : uint8
{
	NotIdleOrAttacking,
	NotInCombat,
	NoTarget,
	TooFewSoldiers,
	WrongTargetKind,
	TargetNearlyDead,
	TooFewInBlast,
	Claimed,
	OutOfReach,
	OrderRefused,
	Count
};

// How often the enemy unit ability AI used abilities this match, and why grenades were held back.
USTRUCT()
struct FEnemyUnitAbilityAIStats
{
	GENERATED_BODY()

	int32 BehavioursUsed = 0;
	int32 GrenadesThrown = 0;
	int32 GrenadeRejections[static_cast<int32>(EEnemyGrenadeRejection::Count)] = {};
};

// One enemy squad or tank the ability AI looks after.
USTRUCT()
struct FEnemyAbilityAIUnit
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> UnitActor;

	bool bM_IsSquad = false;

	// World seconds of the last ability this unit used; spaces abilities out per unit.
	float M_LastAbilityUseSeconds = -1.f;
	bool bM_HasUsedAbility = false;
};

// Area a grenade was just thrown at; other squads leave it alone until the claim expires.
USTRUCT()
struct FEnemyGrenadeTargetClaim
{
	GENERATED_BODY()

	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	float ExpireSeconds = 0.f;
};

// The target categories abilities care about.
enum class EEnemyAbilityTargetKind : uint8
{
	None,
	Infantry,
	Armor,
	Structure
};

// Everything one evaluation of a unit needs, gathered once per evaluation.
struct FEnemyAbilityUnitContext
{
	ICommands* Commands = nullptr;
	AActor* UnitActor = nullptr;
	bool bIsSquad = false;
	EAbilityID ActiveCommand = EAbilityID::IdIdle;
	bool bIsInCombat = false;
	bool bIsMoving = false;
	// Idle or attacking and not walking to a formation waypoint: grenades and aim abilities may interrupt.
	bool bMayUseQueuedAbilities = false;
	bool bIsOnHoldPosition = false;
	bool bIsInFormation = false;
	// The attack command's target, otherwise what the weapons are shooting at.
	TWeakObjectPtr<AActor> CombatTarget;
	bool bHasWeaponTarget = false;
};

/**
 * @brief Lets enemy squads and tanks use their behaviour and grenade abilities on their own; lives on the enemy
 * controller with its own timer, so it also runs on maps where the strategic AI does not.
 * Tuned in Project Settings > Game > RTS Enemy Unit Ability AI and gated by the mission difficulty.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UEnemyUnitAbilityAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyUnitAbilityAIComponent();

	const FEnemyUnitAbilityAIStats& GetStats() const { return M_Stats; }
	int32 GetRosterCount() const { return M_Roster.Num(); }

#if !UE_BUILD_SHIPPING
	FOnEnemyUnitAbilityUsedForTest OnEnemyUnitAbilityUsedForTest;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;
	bool GetIsValidEnemyController() const;

	UPROPERTY()
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	UPROPERTY()
	TArray<FEnemyAbilityAIUnit> M_Roster;

	// Next roster index to evaluate; the roster is walked round robin a few units per tick.
	int32 M_NextRosterIndex = 0;
	float M_NextRosterRefreshSeconds = 0.f;

	UPROPERTY()
	TArray<FEnemyGrenadeTargetClaim> M_GrenadeClaims;

	// Units in a direct-control retreat group, refreshed every tick.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_DirectControlRetreatUnits;

	// Player soldier locations, gathered at most once per tick and only when a grenade is considered.
	TArray<FVector> M_PlayerSoldierLocations;
	bool bM_HasGatheredPlayerSoldiersThisTick = false;

	FTimerHandle M_EvaluationTimerHandle;
	FEnemyUnitAbilityAIStats M_Stats;

	void BeginPlay_StartEvaluationTimer();
	void EvaluationTick();
	bool TryGetMissionDifficulty(ERTSGameDifficulty& OutDifficulty);
	static bool GetIsDifficultyAllowed(const UEnemyUnitAbilityAISettings& Settings, const bool bIsSquad,
	                                   const ERTSGameDifficulty Difficulty);
	void PrepareTick(const float NowSeconds);

	void RefreshRoster();
	void AddEnemyUnitToRoster(AActor* UnitActor, const bool bIsSquad, TArray<FEnemyAbilityAIUnit>& InOutNewRoster) const;
	static bool GetHasUsableAbilityOnCard(ICommands* Commands);

	/**
	 * @brief Lets one unit use at most one ability, so abilities are spread out over the fight.
	 * @param Unit Roster entry of the unit.
	 * @param Settings Tuning values.
	 * @param Difficulty Mission difficulty, checked against the squad or tank threshold.
	 * @param NowSeconds World time of this tick.
	 */
	void EvaluateUnit(FEnemyAbilityAIUnit& Unit, const UEnemyUnitAbilityAISettings& Settings,
	                  const ERTSGameDifficulty Difficulty, const float NowSeconds);

	bool TryBuildUnitContext(const FEnemyAbilityAIUnit& Unit, FEnemyAbilityUnitContext& OutContext) const;
	bool GetIsUnitRetreating(const AActor* UnitActor, const EAbilityID ActiveCommand) const;
	static AActor* FindWeaponTarget(AActor* UnitActor, const bool bIsSquad);
	static bool GetIsOnHoldPosition(AActor* UnitActor, const bool bIsSquad);

	bool TryUseBehaviourAbility(const FEnemyAbilityUnitContext& Context, const TArray<FUnitAbilityEntry>& Card,
	                            const UEnemyUnitAbilityAISettings& Settings, const bool bRepairOnly);
	bool GetShouldUseBehaviour(const FEnemyAbilityUnitContext& Context, const EBehaviourAbilityType BehaviourAbility,
	                           const UEnemyUnitAbilityAISettings& Settings) const;
	bool GetIsTargetInCloseRange(const FEnemyAbilityUnitContext& Context,
	                             const UEnemyUnitAbilityAISettings& Settings) const;

	/**
	 * @brief Throws the first ready grenade that has a target worth it; the running command resumes afterwards.
	 * @param Context The evaluated squad.
	 * @param Card The squad's command card.
	 * @param Settings Tuning values.
	 * @param NowSeconds World time, used for target claims.
	 * @return True when a grenade was ordered.
	 */
	bool TryThrowGrenade(const FEnemyAbilityUnitContext& Context, const TArray<FUnitAbilityEntry>& Card,
	                     const UEnemyUnitAbilityAISettings& Settings, const float NowSeconds);
	bool GetIsSquadReadyToThrow(const FEnemyAbilityUnitContext& Context, const UEnemyUnitAbilityAISettings& Settings,
	                            EEnemyGrenadeRejection& OutRejection) const;
	void RegisterGrenadeThrow(const FEnemyAbilityUnitContext& Context, const UGrenadeComponent& GrenadeComponent,
	                          const FVector& AimLocation, const UEnemyUnitAbilityAISettings& Settings,
	                          const float NowSeconds);

	/**
	 * @brief Picks where a grenade lands, or nothing when the target is not worth the throw.
	 * @param Context The evaluated squad.
	 * @param GrenadeComponent Component of the grenade subtype, for its ranges.
	 * @param GrenadeUsage Targets this grenade is meant for.
	 * @param Settings Tuning values.
	 * @param OutAimLocation The landing spot.
	 * @param OutRejection Why there is no landing spot, when the function returns false.
	 * @return True when the target is worth a grenade and in reach.
	 */
	bool TryFindGrenadeAimLocation(const FEnemyAbilityUnitContext& Context, const UGrenadeComponent& GrenadeComponent,
	                               const EEnemyGrenadeUsage GrenadeUsage, const UEnemyUnitAbilityAISettings& Settings,
	                               FVector& OutAimLocation, EEnemyGrenadeRejection& OutRejection);
	bool TryFindInfantryAimLocation(const FVector& TargetLocation, const float AoeRadius,
	                                const UEnemyUnitAbilityAISettings& Settings, FVector& OutAimLocation);
	int32 CountPlayerSoldiersNear(const FVector& Location, const float Radius);
	void GatherPlayerSoldierLocations();
	bool GetIsGrenadeTargetClaimed(const FVector& AimLocation) const;
	void RemoveExpiredGrenadeClaims(const float NowSeconds);

	static UGrenadeComponent* FindGrenadeComponentOfType(AActor* SquadActor, const int32 GrenadeAbilityRaw);
	static EEnemyAbilityTargetKind GetTargetKind(const AActor* TargetActor);
	static float GetActorHealthRatio(AActor* Actor, const bool bIsSquad);
	static bool GetIsTargetHealthWorthAbility(AActor* TargetActor, const UEnemyUnitAbilityAISettings& Settings);
	static float GetMaxWeaponRange(AActor* UnitActor, const bool bIsSquad);
	static int32 GetAliveSoldierCount(AActor* SquadActor);

	void OnBehaviourUsed(FEnemyAbilityAIUnit& Unit, const FEnemyAbilityUnitContext& Context, const float NowSeconds);
	void DebugAbilityUse(const FEnemyAbilityUnitContext& Context, const EAbilityID Ability,
	                     const FString& Detail) const;
};
