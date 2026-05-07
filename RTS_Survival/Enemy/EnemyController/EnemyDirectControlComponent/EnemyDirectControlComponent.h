#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DirectControl_Retreat/EnemyRetreatCache.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitsResult/FBlackboardIdleUnitsResult.h"
#include "RTS_Survival/Enemy/StrategicAI/IdleUnitSelectionPolicy.h"
#include "EnemyDirectControlComponent.generated.h"

class UEnemyStrategicAIComponent;
class AEnemyController;
class AActor;
class ICommands;
class UEnemyFormationController;
struct FResultAlliedTanksToRetreat;
struct FDamagedTanksRetreatGroup;


/**
 * @brief Tracks units under direct enemy AI control and offers safe registration APIs for blueprints.
 * @note RegisterDirectControlUnit: call in blueprint when a unit should be controlled by direct control logic.
 * @note DeregisterDirectControlUnit: call in blueprint to return a unit to normal controller ownership.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyDirectControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyDirectControlComponent();

	void InitDirectControlComponent(AEnemyController* EnemyController);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	bool RegisterDirectControlUnit(AActor* UnitActor);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	bool DeregisterDirectControlUnit(AActor* UnitActor);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ClearDirectControlUnits();

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable)
	TArray<AActor*> GetRegisteredDirectControlUnits() const;

	/** @brief Randomly drains up to Max units from idle entries and removes each pick immediately to avoid re-selection.
	 *  @param MaxUnitsToPick Upper limit for how many idle entries may be consumed from the blackboard.
	 *  @return Typed pick result built from the entries that were removed.
	 */
	FBlackboardIdleUnitsResult PickRandomMaxIdleBlackboardUnits(const int32 MaxUnitsToPick) const;

	/** @brief Chooses a random target count in [Min, Max] (bounded by availability) and only mutates when Min is reachable.
	 *  @param MinUnitsToPick Minimum required picks; if unavailable, the blackboard remains unchanged.
	 *  @param MaxUnitsToPick Upper bound for random pick count before availability clamping.
	 *  @return Typed pick result for removed entries, or empty when the minimum cannot be satisfied.
	 */
	FBlackboardIdleUnitsResult PickRandomMinMaxIdleBlackboardUnits(
		const int32 MinUnitsToPick,
		const int32 MaxUnitsToPick) const;

	/** @brief Applies subaction-built selection rules while keeping blackboard mutation inside direct control.
	 *  @param SelectionPolicy Fallback min/max plus any requirement-driven unit buckets to consume.
	 *  @return Typed pick result, or empty if required buckets/minimum cannot be satisfied.
	 */
	FBlackboardIdleUnitsResult PickIdleBlackboardUnitsByPolicy(
		const FIdleUnitSelectionPolicy& SelectionPolicy) const;

	/** @brief Resolves a min-max random target, consumes squads first, then tanks if squad supply is exhausted.
	 *  @param MinUnitsToPick Minimum required picks; no removal happens if this threshold cannot be met.
	 *  @param MaxUnitsToPick Upper bound for the random pick target before availability clamping.
	 *  @return Typed pick result reflecting entries removed under squad-first priority.
	 */
	FBlackboardIdleUnitsResult PreferSquad_PickRandomMinMax(
		const int32 MinUnitsToPick,
		const int32 MaxUnitsToPick) const;

	/** @brief Resolves a min-max random target, consumes tanks first, then squads if tank supply is exhausted.
	 *  @param MinUnitsToPick Minimum required picks; no removal happens if this threshold cannot be met.
	 *  @param MaxUnitsToPick Upper bound for the random pick target before availability clamping.
	 *  @return Typed pick result reflecting entries removed under tank-first priority.
	 */
	FBlackboardIdleUnitsResult PreferTank_PickRandomMinMax(
		const int32 MinUnitsToPick,
		const int32 MaxUnitsToPick) const;

	/** @brief Runs regular min-max picking while excluding hazmat squads, then relaxes that rule only to recover Min.
	 *  @param MinUnitsToPick Minimum required picks; mutation is skipped when total availability cannot satisfy it.
	 *  @param MaxUnitsToPick Upper bound for random target picks before availability clamping.
	 *  @return Typed pick result favoring non-hazmat entries and using hazmats only as minimum-recovery fallback.
	 */
	FBlackboardIdleUnitsResult PreferNoHazmats_PickRandomMinMax(
		const int32 MinUnitsToPick,
		const int32 MaxUnitsToPick) const;

	/** @brief Applies min-max gating against hazmat-only availability and consumes only hazmat engineer squad entries.
	 *  @param MinUnitsToPick Minimum required hazmat picks; failure leaves the blackboard unchanged.
	 *  @param MaxUnitsToPick Upper bound for random hazmat pick target after hazmat availability clamp.
	 *  @return Typed pick result for removed hazmat engineer entries only.
	 */
	FBlackboardIdleUnitsResult IdleHazmatEngineers_PickRandomMinMax(
		const int32 MinUnitsToPick,
		const int32 MaxUnitsToPick) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RegisterDirectControlUnits(const TArray<AActor*>& UnitActors);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void DeregisterDirectControlUnits(const TArray<AActor*>& UnitActors);

	void HandleAsyncRetreatGroupResult(const FResultAlliedTanksToRetreat& RetreatResult);

	TArray<TWeakObjectPtr<AActor>> GetRetreatGroupUnitsToExclude() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	FTimerHandle M_DirectControlTickTimerHandle;

	UPROPERTY()
	FDirectControlRetreatCache M_RetreatCache;

	bool EnsureEnemyControllerIsValid() const;
	UEnemyStrategicAIComponent* GetValidStrategicAIComponent() const;
	bool GetIsValidDirectControlUnitActor(const AActor* UnitActor) const;

	void StartDirectControlTickTimer();
	void StopDirectControlTickTimer();
	void TickDirectControl();
	void RemoveInvalidRegisteredUnits();
	void DebugDrawRegisteredDirectControlUnits() const;
	void DebugReportRegisterDeregister(const FString& Message) const;
	void AppendRetreatGroupUnitsToExclude(
		const FDamagedTanksRetreatGroup& RetreatGroup,
		TArray<TWeakObjectPtr<AActor>>& OutUnitsToExclude) const;

	ICommands* TryGetCommandsInterface(AActor* UnitActor) const;
	void OnRetreatGroupFound(FDamagedTanksRetreatGroup& RetreatGroup);
	void RemoveRetreatGroupUnitsFromFormations(const FDamagedTanksRetreatGroup& RetreatGroup);
	void IgnoreHazmatsInFieldConstruction(FDamagedTanksRetreatGroup& RetreatGroup) const;
	bool IsRetreatGroupDamagedTanksOnly(const FDamagedTanksRetreatGroup& RetreatGroup) const;
	void MarkRetreatGroupTanksOnlyRetreating(FDamagedTanksRetreatGroup& RetreatGroup) const;
	void IssueRetreatOrdersToDamagedTanks(const FDamagedTanksRetreatGroup& RetreatGroup);
	void IssueHazmatSupportOrders(const FDamagedTanksRetreatGroup& RetreatGroup);
	void MarkRetreatGroupHazmatsRepairing(FDamagedTanksRetreatGroup& RetreatGroup) const;
	AActor* GetFirstValidDamagedTank(const FDamagedTanksRetreatGroup& RetreatGroup) const;
	bool TryGetProjectedLocation(const FVector& OriginalLocation, FVector& OutProjectedLocation) const;
	bool EnsureFormationControllerIsValid(UEnemyFormationController*& OutFormationController) const;

	void Debug_RetreatUnitsRemovedFromFormations(const TArray<AActor*>& RemovedUnits) const;
	void Debug_IgnoredFieldconstructionHazmats(const TArray<AActor*>& IgnoredHazmatUnits) const;
};
