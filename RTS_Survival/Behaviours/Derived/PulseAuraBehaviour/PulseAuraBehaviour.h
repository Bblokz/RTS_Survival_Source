// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "PulseAuraBehaviour.generated.h"

class UBehaviourComp;
class URTSComponent;
class URTSRadiusPoolSubsystem;

USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FPulseAuraBehaviourSettings
{
	GENERATED_BODY()

	// Radius used for overlap checks and the radius visualization attached to the owner.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura")
	float Radius = 750.f;

	// Radius type used by the radius pool while this behaviour is active.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura")
	ERTSRadiusType RadiusType = ERTSRadiusType::FullCircle_PulseRepair;

	// Offset used when attaching the pooled radius actor to the behaviour owner.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura")
	FVector RadiusOffset = FVector::ZeroVector;

	// Interval in seconds between each async pulse overlap check.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura", meta=(ClampMin="1"))
	int32 PulseIntervalSeconds = 1;

	// Behaviour classes applied to valid overlapped units for each pulse.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura")
	TArray<TSubclassOf<UBehaviour>> BehavioursToApply;

	// Defines which overlap group we query in the async overlap library.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura")
	ETriggerOverlapLogic OverlapRule = ETriggerOverlapLogic::OverlapPlayer;
};

/**
 * @brief Timed aura behaviour that periodically pulses and applies child behaviours to units inside a radius.
 * The aura displays its active radius through the pooled radius subsystem while it is active.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UPulseAuraBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	UPulseAuraBehaviour();

	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnTick(float DeltaTime) override;

protected:
	// Allows derived pulse aura behaviours to provide custom filtering rules per overlapped unit.
	virtual bool GetShouldApplyToOverlappedActor(const AActor* OverlappedActor) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pulse Aura")
	FPulseAuraBehaviourSettings M_PulseAuraSettings;

private:
	void RefreshPulseTimer(float DeltaTime);
	void ExecutePulse();
	void ApplyBehavioursToTargetActor(AActor& TargetActor) const;
	ETriggerOverlapLogic GetResolvedOverlapRule() const;
	void ShowAuraRadius();
	void HideAuraRadius();
	bool GetIsValidOwningActor() const;
	bool GetIsValidOwningRTSComponent() const;
	bool GetIsValidRadiusSubsystem() const;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_OwningActor;

	UPROPERTY()
	TWeakObjectPtr<URTSComponent> M_OwningRTSComponent;

	UPROPERTY()
	TWeakObjectPtr<URTSRadiusPoolSubsystem> M_RadiusSubsystem;

	float M_TimeSinceLastPulseSeconds = 0.f;
	int32 M_AuraRadiusId = INDEX_NONE;
	uint8 M_OwningPlayer = 0;
};

