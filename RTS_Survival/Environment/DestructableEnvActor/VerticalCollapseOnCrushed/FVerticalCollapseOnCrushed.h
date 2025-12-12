#pragma once

#include "CoreMinimal.h"
#include "FVerticalCollapseOnCrushed.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

/**
 * @brief Queues a non-physics vertical collapse by interpolating world rotation
 *        around an axis derived from the overlap (tips away from the tank).
 *        Eases in non-linearly and stops at 90°.
 *
 * Owns its timer and prevents double queueing.
 */
USTRUCT(BlueprintType)
struct FVerticalCollapseOnCrushed
{
	GENERATED_BODY()

public:
	/** @param TargetPrimitive Primitive to rotate (must be Movable). */
	void Init(UPrimitiveComponent* TargetPrimitive,
	          float CollapseSpeedScale,
	          USoundBase* CollapseSound,
	          USoundAttenuation* Attenuation,
	          USoundConcurrency* Concurrency);

	/** @brief Enqueue the collapse based on the overlap context. */
	void QueueCollapseFromOverlap(UPrimitiveComponent* OverlappedComponent,
	                              UPrimitiveComponent* OtherComp,
	                              bool bFromSweep,
	                              const FHitResult& SweepResult,
	                              UWorld* World);

	/** Collapse in a random ground-plane direction (ignores Z), easing to 90°. */
	void QueueCollapseRandom(UWorld* World);

private:
	// Target primitive to rotate (designer-provided).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction", meta=(AllowPrivateAccess="true"))
	UPrimitiveComponent* M_TargetPrimitive = nullptr;

	// Higher => shorter duration (faster).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction", meta=(AllowPrivateAccess="true"))
	float M_CollapseSpeedScale = 1.f;

	// Prevents double queueing.
	bool bM_CollapseQueued = false;

	// Detects if Init has been called.
	bool bM_IsInitialized = false;

	// Timer used to tick the interpolation.
	FTimerHandle M_CollapseTickHandle;

	/** Transient animation state (kept internal). */
	struct FCollapseAnim
	{
		FVector M_AxisWS = FVector::ZeroVector; // normalized axis
		FQuat M_InitialWorldRotation = FQuat::Identity;
		double M_StartSeconds = 0.0;
		float M_Duration = 0.f; // seconds
		float M_TargetAngleDeg = 90.f; // stop at 90°
	};

	FCollapseAnim M_Anim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction|Audio", meta=(AllowPrivateAccess="true"))
    USoundBase* M_CollapseSound = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction|Audio", meta=(AllowPrivateAccess="true"))
    USoundAttenuation* M_Attenuation = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CrushDestruction|Audio", meta=(AllowPrivateAccess="true"))
    USoundConcurrency* M_Concurrency = nullptr;

	// ===== Helpers =====

	
void PlayCollapseSound(UWorld* World, const FVector& AtLocation) const;
	
	static FVector ComputeFallDirection(UPrimitiveComponent* Target,
	                                    UPrimitiveComponent* OtherComp,
	                                    bool bFromSweep,
	                                    const FHitResult& SweepResult);

	// Up x FallDir
	static FVector ComputeAxisFromFallDir(const FVector& FallDir);

	// Starts a repeating timer that updates world rotation until 90° is reached.
	static void StartCollapseTimer(UWorld* World,
	                               TWeakObjectPtr<UPrimitiveComponent> WeakTarget,
	                               const FVector& AxisWS,
	                               const FQuat& InitialWorldRotation,
	                               float DurationSeconds,
	                               float TargetAngleDeg,
	                               FTimerHandle& OutHandle);
};
