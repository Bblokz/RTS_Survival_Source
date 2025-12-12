#pragma once

#include "CoreMinimal.h"

#include "FImpulseOnCrushed.generated.h"

/**
 * @brief Queues a physics impulse to be applied after a delay when crushed by a tank.
 * Owns its timer handle and prevents double queueing.
 */
USTRUCT(BlueprintType)
struct FImpulseOnCrushed
{
	GENERATED_BODY()

public:
	void Init(UPrimitiveComponent* TargetPrimitive,
	           float CrushImpulseScale,
	           float TimeTillImpulse);
	/** Called from C++ overlap handlers to enqueue an impulse based on overlap context. */
	void QueueImpulseFromOverlap(UPrimitiveComponent* OverlappedComponent,
	                             UPrimitiveComponent* OtherComp,
	                             bool bFromSweep,
	                             const FHitResult& SweepResult,
	                             UWorld* World);

private:
	// Target primitive to receive the impulse (designer-provided).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction", meta=(AllowPrivateAccess="true"))
	UPrimitiveComponent* M_TargetPrimitive = nullptr;

	// Scales the impulse strength.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction", meta=(AllowPrivateAccess="true"))
	float M_CrushImpulseScale = 1.f;

	// How long to wait (seconds) before applying the impulse.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction", meta=(AllowPrivateAccess="true"))
	float M_TimeTillImpulse = 0.f;

	// Prevents double queueing once a timer has been set.
	bool bM_IsImpulseSet = false;
	// Detects if any configuration is present.
	bool bM_IsInitialized = false;

	// Timer used to execute the delayed impulse.
	FTimerHandle M_ImpulseHandle;
	
	static FVector ComputeImpulseDirection(UPrimitiveComponent* Target,
	                                       UPrimitiveComponent* OtherComp,
	                                       bool bFromSweep,
	                                       const FHitResult& SweepResult);

	static float ComputeImpulseMagnitude(UPrimitiveComponent* Target,
	                                     UPrimitiveComponent* OtherComp,
	                                     float Scale);

	static FVector ComputeImpactPoint(UPrimitiveComponent* Target,
	                                  bool bFromSweep,
	                                  const FHitResult& SweepResult);

		/** Build & apply a localized, directed force field instead of AddImpulseAtLocation. */
    	static void ApplyImpulseField(UPrimitiveComponent* Target,
    	                              const FVector& Direction,
    	                              float Magnitude,
    	                              const FVector& AtLocation);
};
