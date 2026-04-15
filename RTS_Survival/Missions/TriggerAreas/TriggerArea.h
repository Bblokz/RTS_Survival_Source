#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "TriggerArea.generated.h"

class UBoxComponent;
class USphereComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTriggerAreaOverlapSignature, AActor*, OverlappingActor,
                                             ATriggerArea*, TriggerArea);

/**
 * @brief Mission-trigger helper actor that forwards overlap events through one unified callback path.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ATriggerArea : public AActor
{
	GENERATED_BODY()

public:
	ATriggerArea();

	virtual void PostInitializeComponents() override;

	void ApplyScaleToCollision(const FVector& Scale);

	void SetupOverlapLogic(const ETriggerOverlapLogic TriggerOverlapLogic);

	UPROPERTY(BlueprintAssignable, Category = "Mission|Trigger")
	FOnTriggerAreaOverlapSignature OnTriggerAreaOverlap;

private:
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> M_TriggerCollisionComponent = nullptr;

	float M_BaseSphereRadius = 100.0f;
	FVector M_BaseBoxExtent = FVector(100.0f);

	bool BeginPlay_InitTriggerCollisionComponent();
	bool GetIsValidTriggerCollisionComponent() const;

	UFUNCTION()
	void OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent,
	                             AActor* OtherActor,
	                             UPrimitiveComponent* OtherComp,
	                             int32 OtherBodyIndex,
	                             bool bFromSweep,
	                             const FHitResult& SweepResult);
};
