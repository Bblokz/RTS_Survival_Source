// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ParachuteDescentComponent.generated.h"

class ASquadUnit;
class UAnimMontage;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FParatrooperDescentSettings
{
	GENERATED_BODY()

	// Attached to the soldier's skeletal mesh with an identity transform: authored in that mesh's space.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> ParachuteMesh = nullptr;

	// Single-section FullBody montage on the infantry skeleton; looped while descending.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAnimMontage> ParachuteMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "50.0"))
	float DescentSpeed = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float SwayAmplitudeDegrees = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float SwayPeriodSeconds = 3.6f;

	// Height of the sway pivot (the canopy) above the soldier's actor origin: the soldier swings below it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SwayPivotHeight = 480.0f;

	// The sway fades out over this last part of the descent so the soldier touches down upright.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float SwaySettleSeconds = 1.5f;
};

/** @param bLanded False when the descent ended because the soldier died or was removed mid-air. */
DECLARE_DELEGATE_TwoParams(FOnParatrooperDescentFinished, ASquadUnit* /*SquadUnit*/, bool /*bLanded*/);

/**
 * @brief Added at runtime to one squad unit by the paratroopers global ability: shows the soldier with a parachute,
 * flies him down to a landing point with a pendulum sway, then removes the parachute, restores the unit and
 * destroys itself. Weapon fire, selection, collision and walking are suspended while in the air.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UParachuteDescentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UParachuteDescentComponent();

	/**
	 * @brief Starts the descent; the owner must be a squad unit. Call right after registering the component.
	 * @param Settings Parachute mesh, montage and motion tuning.
	 * @param StartLocation Actor location where the soldier leaves the aircraft.
	 * @param LandingLocation Actor location on the ground (capsule centre) where he lands.
	 * @param FacingYaw Yaw the soldier faces during the whole descent.
	 */
	void StartDescent(
		const FParatrooperDescentSettings& Settings,
		const FVector& StartLocation,
		const FVector& LandingLocation,
		float FacingYaw);

	FOnParatrooperDescentFinished OnDescentFinished;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FParachuteDescentState
	{
		FVector StartLocation = FVector::ZeroVector;
		FVector LandingLocation = FVector::ZeroVector;
		FVector SwayAxis = FVector::ForwardVector;
		float SwayPhaseRadians = 0.0f;
		float FacingYaw = 0.0f;
		float ElapsedSeconds = 0.0f;
		float DurationSeconds = 1.0f;
		bool bM_IsDescending = false;
	};

	FParachuteDescentState M_DescentState;

	UPROPERTY()
	FParatrooperDescentSettings M_Settings;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ParachuteMeshComponent;

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_SquadUnit;

	[[nodiscard]] bool GetIsValidSquadUnit() const;

	void StartDescent_SuspendUnit();
	void StartDescent_AttachParachute();
	void TickDescent(float DeltaTime);
	FTransform GetSwayedTransform(const FVector& PathLocation) const;
	/**
	 * @brief Removes the parachute, restores a landed soldier and notifies the ability.
	 * @param bLanded False when the soldier died or is being removed mid-air.
	 * @param bDestroySelf False from EndPlay: destroying the component there re-enters EndPlay.
	 */
	void FinishDescent(bool bLanded, bool bDestroySelf = true);
	void FinishDescent_RestoreUnit();
	void RemoveParachute();
};
