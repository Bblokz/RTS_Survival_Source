// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainsSunbreakDrift.generated.h"

class UMaterialParameterCollection;

/** @brief Preserves an authored shaft anchor while its cloud opening travels across the landscape. */
USTRUCT()
struct FTrainsSunbreakRayBinding
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> RayActor;

	UPROPERTY()
	FVector AnchorLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float GroundHeight = 0.f;

	UPROPERTY(Transient)
	bool bHasGround = false;
};

/** @brief Configures the Trains cloud travel area and how shafts follow the terrain. */
USTRUCT(BlueprintType)
struct FTrainsSunbreakDriftSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cloud Drift")
	FVector2D MinimumBounds = FVector2D(-42000.f, -51000.f);

	UPROPERTY(EditAnywhere, Category = "Cloud Drift")
	FVector2D MaximumBounds = FVector2D(16000.f, 38000.f);

	UPROPERTY(EditAnywhere, Category = "Cloud Drift")
	FVector2D WindDirection = FVector2D(1.f, 0.28f);

	UPROPERTY(EditAnywhere, Category = "Cloud Drift", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SpeedPerDriftScale = 100.f;

	UPROPERTY(EditAnywhere, Category = "Cloud Drift", meta = (ClampMin = "0.01", Units = "s"))
	float GroundSampleInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Cloud Drift", meta = (ClampMin = "1.0", Units = "cm"))
	float BoundaryFadeDistance = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Cloud Drift", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundSinkDistance = 100.f;
};

/**
 * @brief Place one in Ger_Trains to advance its shared cloud offset and carry the fake shafts with it.
 * Editor realtime preview and gameplay share the same movement and continuous speed changes.
 */
UCLASS()
class RTS_SURVIVAL_API ATrainsSunbreakDrift : public AActor
{
	GENERATED_BODY()

public:
	ATrainsSunbreakDrift();
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	/**
	 * @brief Records authored anchors so preview and gameplay restart from the same cloud field.
	 * @param SunbreakControls Collection shared by the Trains sun and ray materials.
	 * @param RayActors Shafts to move with the cloud openings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trains Sunbreaks")
	void ConfigureSunbreakDrift(UMaterialParameterCollection* SunbreakControls, const TArray<AActor*>& RayActors);

	UFUNCTION(BlueprintCallable, Category = "Trains Sunbreaks")
	void AdvanceSunbreakDrift(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Trains Sunbreaks")
	void ResetSunbreakDrift();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditInstanceOnly, Category = "Trains Sunbreaks")
	TWeakObjectPtr<UMaterialParameterCollection> M_SunbreakControls;

	UPROPERTY(EditAnywhere, Category = "Trains Sunbreaks")
	FTrainsSunbreakDriftSettings M_Settings;

	// Save authored anchors, rather than baking the current preview displacement into the layout.
	UPROPERTY()
	TArray<FTrainsSunbreakRayBinding> M_RayBindings;

	UPROPERTY(Transient)
	FVector2D M_CloudOffset = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float M_GroundSampleElapsedSeconds = 0.f;

	bool GetIsValidSunbreakControls() const;
	bool GetIsValidRayActor(const FTrainsSunbreakRayBinding& RayBinding) const;
	FVector2D GetCloudVelocity() const;
	void PublishCloudOffset() const;
	void UpdateRayBindings(float DeltaSeconds, bool bSampleGround);

	/**
	 * @brief Keeps a shaft in its moving opening while easing changes in ground elevation.
	 * @param RayBinding Authored anchor and cached terrain state for the shaft.
	 * @param DeltaSeconds Elapsed time; zero snaps directly to the sampled height.
	 * @param bSampleGround Whether to refresh the landscape collision sample.
	 */
	void UpdateRayBinding(FTrainsSunbreakRayBinding& RayBinding, float DeltaSeconds, bool bSampleGround);

	/**
	 * @brief Prevents shafts from floating above valleys or appearing beyond the landscape.
	 * @param RayBinding Receives terrain height and hit validity.
	 * @param RayLocation Horizontal position to sample.
	 */
	void SampleRayGround(FTrainsSunbreakRayBinding& RayBinding, const FVector& RayLocation) const;

	/**
	 * @brief Conceals map-edge recycling without overriding the artist's ray fade control.
	 * @param RayBinding Identifies the shaft and whether terrain exists below it.
	 * @param RayLocation Position used to fade near the travel area's edges.
	 */
	void ApplyRayFade(const FTrainsSunbreakRayBinding& RayBinding, const FVector& RayLocation) const;
	float GetBoundaryFade(const FVector& RayLocation) const;
	static float WrapCoordinate(double Coordinate, double Minimum, double Maximum);
};
