#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "AUT_ResourceSystem_Manager.generated.h"

// Forward declarations
class UResourceDropOff;

/**
 * A test manager for verifying resource drop-offs:
 *  - Spawns a list of drop-off actor classes
 *  - Periodically runs tests (adding/removing resources, destroying an actor, etc.)
 */
UCLASS()
class RTS_SURVIVAL_API AUT_ResourceSystem_Manager : public AActor
{
	GENERATED_BODY()

public:
	AUT_ResourceSystem_Manager();

	/** Called by Blueprint to provide the classes we want to test. */
	UFUNCTION(BlueprintCallable, Category="ResourceSystemTest")
	void InitializeDropOffClasses(const TArray<TSubclassOf<AActor>>& InClasses);

	/** Called by Blueprint to set where we spawn these drop-off actors. */
	UFUNCTION(BlueprintCallable, Category="ResourceSystemTest")
	void InitializeSpawnLocations(const TArray<FVector>& InLocations);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditAnywhere, Category="ResourceSystemTest")
	float DelayBetweenSteps = 2.0f;

	/** The classes (set via Blueprint) to spawn/test. */
	UPROPERTY()
	TArray<TSubclassOf<AActor>> DropOffActorClasses;

	/** Where we’ll spawn each actor. If fewer spawn locations than classes, re-use the last location. */
	UPROPERTY()
	TArray<FVector> SpawnLocations;

	/** Actual spawned actors. */
	UPROPERTY()
	TArray<AActor*> SpawnedActors;

	/** Resource drop-off components from the spawned actors. */
	UPROPERTY()
	TArray<UResourceDropOff*> DropOffComponents;

	/** Index into our test steps. */
	int32 TestStepIndex = 0;

	/** Timer handle so each step runs with a gap. */
	FTimerHandle TestStepTimerHandle;

	void SpawnDropOffActors();
	void FindDropOffComp(AActor* Actor);
	void ScheduleNextStep();
	void PerformTestStep();
	void PerformTestStep0();
	void PerformTestStep1();
	void PerformTestStep2();
	void PerformTestStep3();
	void PerformTestStep4();

	void LogDropOffState(const FString& Context);
};
