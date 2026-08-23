#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "AutoScavengeTriggerComponent.generated.h"

class AScavengeableObject;
class ASquadController;

/**
 * @brief Add to scavengable actors that should automatically order nearby player-one scavenger squads.
 * The inherited Sphere Radius setting controls the activation distance in centimeters.
 */
UCLASS(ClassGroup=(Scavenging), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAutoScavengeTriggerComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UAutoScavengeTriggerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AScavengeableObject> M_ScavengeableOwner;

	bool GetIsValidScavengeableOwner() const;
	ASquadController* ResolveOverlappingSquadController(AActor* OverlappingActor) const;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	                           AActor* OtherActor,
	                           UPrimitiveComponent* OtherComponent,
	                           int32 OtherBodyIndex,
	                           bool bFromSweep,
	                           const FHitResult& SweepResult);
};
