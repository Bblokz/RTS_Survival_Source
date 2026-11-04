// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "GliderDropTestSubsystem.generated.h"

class ACameraActor;
class ATankMaster;
class UGlobalAbility;

UENUM()
enum class EGliderDropTestPhase : uint8
{
	WaitingForStart,
	Running,
	Finished
};

/**
 * @brief -GliderDropValidate: runs the E-100 glider drop ability on the anchor (where the camera looks, or
 * -GliderDropAnchorX= and -GliderDropAnchorY=; run heading -GliderDropYaw=deg), follows the glider with a camera, screenshots every phase
 * into Saved/GliderDropTest and checks the tank drives out. Logs RTS_GLIDER_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UGliderDropTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EGliderDropTestPhase M_Phase = EGliderDropTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_RunStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_RunDirection = FVector::ForwardVector;
	EGliderDeliveryPhase M_LastSeenPhase = EGliderDeliveryPhase::None;
	EGliderDeliveryPhase M_FurthestPhase = EGliderDeliveryPhase::None;
	float M_PhaseSeenSeconds = 0.f;
	bool bM_TookMidPhaseShot = false;
	bool bM_DeliveredBroadcast = false;
	FVector M_TankStartLocation = FVector::ZeroVector;
	float M_TankDeliveredSeconds = 0.f;

	// Nothing else holds the running ability; its load callbacks outlive the start of the run.
	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TWeakObjectPtr<AGliderTankDelivery> M_Delivery;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_Tank;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartRun();
	void TickRun();
	void TrackDeliveryPhase();
	void OnTankDelivered(AGliderTankDelivery* Delivery, ATankMaster* Tank);
	void LogSkidDustParameters(const AGliderTankDelivery& Delivery) const;
	void FinishRun();
	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
};
