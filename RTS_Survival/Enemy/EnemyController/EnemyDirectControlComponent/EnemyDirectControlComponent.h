#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DirectControl_Retreat/EnemyRetreatCache.h"
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

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RegisterDirectControlUnits(const TArray<AActor*>& UnitActors);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void DeregisterDirectControlUnits(const TArray<AActor*>& UnitActors);

	void HandleAsyncRetreatGroupResult(const FResultAlliedTanksToRetreat& RetreatResult);

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable)
	TArray<TWeakObjectPtr<AActor>> GetRetreatGroupUnitsToExclude() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	// Registry of units currently owned by direct control AI for issuing strategic orders.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_RegisteredIdleDirectControlUnits;

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
