// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CargoParachuteDescentComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** How a load that is not a soldier comes down: the canopies above it and how it moves. */
USTRUCT(BlueprintType)
struct FCargoParachuteDescentSettings
{
	GENERATED_BODY()

	// Canopy mesh authored like the soldier parachute: lines meet at the bottom, canopy above. Empty uses the
	// soldier parachute of the ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> CanopyMesh = nullptr;

	// Scale of each canopy relative to the soldier parachute.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float CanopyScale = 2.5f;

	// 0: no canopy (the load carries its own, e.g. a jumper proxy), 1: one canopy, 3: a cluster for heavy loads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "3"))
	int32 CanopyCount = 1;

	// How far the canopies of a cluster lean away from each other, in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float ClusterTiltDegrees = 24.f;

	// Height of the canopy mesh point where its lines meet (soldier parachute space, unscaled).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float CanopyLineMeetHeight = 81.f;

	// Gap between the top of the load and the point where the lines meet.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float LineGapAboveLoad = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "50.0"))
	float DescentSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float SwayAmplitudeDegrees = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float SwayPeriodSeconds = 4.5f;

	// Pendulum pivot above the load origin; 0 puts it at the canopy (derived from the load height and canopy size).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SwayPivotHeight = 0.f;

	// The sway fades out over this last part of the descent so the load touches down upright.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SwaySettleSeconds = 2.f;
};

/** @param bLanded False when the load was destroyed or removed before touching down. */
DECLARE_DELEGATE_TwoParams(FOnCargoDescentFinished, AActor* /*Load*/, bool /*bLanded*/);

/**
 * @brief Added at runtime to any actor that comes down under a parachute that is not a soldier: a team weapon, a
 * dropped building, or a jumper proxy. Attaches 0, 1 or 3 scaled canopies to the owner's root, flies the owner on a
 * straight line to its landing transform with a pendulum sway, keeps the owner's turrets silent while in the air,
 * then removes the canopies, puts the owner exactly on the landing transform and destroys itself.
 * The soldier's own descent stays in UParachuteDescentComponent.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UCargoParachuteDescentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCargoParachuteDescentComponent();

	/**
	 * @brief Starts the descent; call right after registering the component.
	 * @param Settings Canopies and motion.
	 * @param FallbackCanopyMesh Used when Settings has no canopy mesh.
	 * @param StartLocation Owner location where the load leaves the aircraft.
	 * @param LandingTransform Owner transform on the ground.
	 */
	void StartDescent(
		const FCargoParachuteDescentSettings& Settings,
		UStaticMesh* FallbackCanopyMesh,
		const FVector& StartLocation,
		const FTransform& LandingTransform);

	/**
	 * @brief The landing follows this component (location and yaw) every frame; used by jumper proxies whose real
	 * soldier still moves on the ground while the proxy is in the air. The descent ends unlanded if it goes away.
	 */
	void SetLandingTrackComponent(USceneComponent* TrackComponent);

	// Descent progress 0..1; 1 when not descending.
	float GetDescentAlpha() const;
	bool GetIsDescending() const { return M_State.bM_IsDescending; }
	int32 GetCanopyCount() const { return M_CanopyComponents.Num(); }

	FOnCargoDescentFinished OnDescentFinished;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FCargoDescentState
	{
		FVector StartLocation = FVector::ZeroVector;
		FTransform LandingTransform = FTransform::Identity;
		FVector SwayAxis = FVector::ForwardVector;
		float SwayPhaseRadians = 0.0f;
		float SwayPivotHeight = 0.0f;
		float ElapsedSeconds = 0.0f;
		float DurationSeconds = 1.0f;
		bool bM_IsDescending = false;
		bool bM_TracksLanding = false;
	};

	FCargoDescentState M_State;

	UPROPERTY()
	FCargoParachuteDescentSettings M_Settings;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_CanopyComponents;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_LandingTrackComponent;

	/** @return Height of the top of the owner's visible geometry above its origin. */
	float GetLoadTopHeight() const;
	void AttachCanopies(UStaticMesh* CanopyMesh, float LoadTopHeight);
	void AttachCanopy(UStaticMesh* CanopyMesh, const FVector& AttachPoint, const FVector& LeanDirection);
	void UpdateTrackedLanding();
	void TickDescent(float DeltaTime);
	FTransform GetSwayedTransform(const FVector& PathLocation) const;
	void SilenceOwnerTurrets() const;
	void FinishDescent(bool bLanded, bool bDestroySelf = true);
	void RemoveCanopies();
};
