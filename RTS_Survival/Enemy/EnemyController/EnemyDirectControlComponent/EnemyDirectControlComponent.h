#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyDirectControlComponent.generated.h"

class AEnemyController;
class AActor;
class ICommands;
class UEnemyFormationController;
struct FResultAlliedTanksToRetreat;
struct FDamagedTanksRetreatGroup;

USTRUCT()
struct FDirectControlRetreatCache
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDamagedTanksRetreatGroup> M_CachedRetreatGroups;

	UPROPERTY()
	int32 M_LastRequestID = INDEX_NONE;
};

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	// Registry of units currently owned by direct control AI for issuing strategic orders.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_RegisteredDirectControlUnits;

	FTimerHandle M_DirectControlTickTimerHandle;

	UPROPERTY()
	FDirectControlRetreatCache M_RetreatCache;

	bool EnsureEnemyControllerIsValid() const;
	bool GetIsValidDirectControlUnitActor(const AActor* UnitActor) const;

	void StartDirectControlTickTimer();
	void StopDirectControlTickTimer();
	void TickDirectControl();
	void RemoveInvalidRegisteredUnits();
	void DebugDrawRegisteredDirectControlUnits() const;
	void DebugReportRegisterDeregister(const FString& Message) const;

	ICommands* TryGetCommandsInterface(AActor* UnitActor) const;
	void HandleRetreatGroup(const FDamagedTanksRetreatGroup& RetreatGroup);
	void RegisterRetreatGroupUnits(const FDamagedTanksRetreatGroup& RetreatGroup);
	void RemoveRetreatGroupUnitsFromFormations(const FDamagedTanksRetreatGroup& RetreatGroup);
	void IssueRetreatOrdersToDamagedTanks(const FDamagedTanksRetreatGroup& RetreatGroup);
	void IssueHazmatSupportOrders(const FDamagedTanksRetreatGroup& RetreatGroup);
	AActor* GetFirstValidDamagedTank(const FDamagedTanksRetreatGroup& RetreatGroup) const;
	bool TryGetProjectedLocation(const FVector& OriginalLocation, FVector& OutProjectedLocation) const;
	bool EnsureFormationControllerIsValid(UEnemyFormationController*& OutFormationController) const;

	void Debug_RetreatUnitsRemovedFromFormations(const TArray<AActor*>& RemovedUnits) const;
};
