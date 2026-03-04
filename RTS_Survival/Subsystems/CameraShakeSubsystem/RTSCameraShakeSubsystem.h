#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizationDistance.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSCameraShakeSubsystem.generated.h"

class APlayerController;

UENUM()
enum class ERTSCameraShakeEventType : uint8
{
	WeaponFire,
	Explosion
};

USTRUCT(BlueprintType)
struct FRTSCameraShakeRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FVector M_WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	int32 M_CalibreMm = 0;

	UPROPERTY(BlueprintReadWrite)
	ERTSCameraShakeEventType M_EventType = ERTSCameraShakeEventType::WeaponFire;

	UPROPERTY(BlueprintReadWrite)
	bool bM_HasOptimizationDistanceHint = false;

	UPROPERTY(BlueprintReadWrite)
	ERTSOptimizationDistance M_OptimizationDistanceHint = ERTSOptimizationDistance::None;
};

/**
 * @brief World subsystem that aggregates projectile weapon fire and explosion shake requests into stable camera pulses.
 * Systems submit lightweight requests; this subsystem applies visibility/distance/rate-limit rules and plays the
 * configured camera shake classes on the local player camera.
 */
UCLASS()
class RTS_SURVIVAL_API URTSCameraShakeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * @brief Queue a projectile weapon fire shake request for this frame/window.
	 * @param Request Request payload containing location, calibre, and optional optimization hint.
	 */
	void RequestWeaponFireShake(const FRTSCameraShakeRequest& Request);

	/**
	 * @brief Queue an explosion shake request for this frame/window.
	 * @param Request Request payload containing location, calibre, and optional optimization hint.
	 */
	void RequestExplosionShake(const FRTSCameraShakeRequest& Request);

private:
	UPROPERTY()
	TArray<FRTSCameraShakeRequest> M_PendingRequests;

	UPROPERTY()
	TArray<float> M_RequestTimesSeconds;

	float M_AggregationWindowStartTimeSeconds = -1.0f;
	float M_LastHeavyShakeTimeSeconds = -1000.0f;

	void QueueRequest(const FRTSCameraShakeRequest& Request, const bool bAllowEventType);
	void Tick_PruneRequestRateWindow(const float CurrentTimeSeconds);
	void Tick_FlushAggregationWindow(const float CurrentTimeSeconds);
	void ResetAggregationWindow();

	float BuildAggregatedIntensity(const FVector& CameraLocation,
		const FVector& CameraForward,
		const float CosHalfFov) const;
	void TryPlayAggregatedShake(const float CurrentTimeSeconds,
		APlayerController* PlayerController,
		const float AggregatedIntensity);

	bool GetIsRequestPassingGlobalThresholds(const FRTSCameraShakeRequest& Request) const;
	bool GetIsRequestWithinDistanceRules(const FRTSCameraShakeRequest& Request,
	                                    const FVector& CameraLocation,
	                                    float& OutDistanceMultiplier,
	                                    float& OutNormalizedCalibre) const;
	bool GetIsRequestPassingVisibilityRules(const FRTSCameraShakeRequest& Request,
	                                       const FVector& CameraLocation,
	                                       const FVector& CameraForward,
	                                       const float CosHalfFov) const;

	float GetNormalizedCalibre(const int32 CalibreMm) const;
	float GetEventSpecificWeight(const ERTSCameraShakeEventType EventType) const;
	float GetEventSpecificDistanceForCalibre(const float NormalizedCalibre) const;

	APlayerController* GetPrimaryPlayerController() const;
};
