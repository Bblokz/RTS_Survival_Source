#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Behaviours/Derived/Damage/TankEngineFire/TankEngineFireAttachmentRules.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "TankEngineFireBehaviour.generated.h"

class ATankMaster;
class URTSFireSubsystem;

/**
 * @brief Add this behaviour to a tank to burn its health and show a pooled hull fire.
 * Configure damage, attachment, and visuals in a behaviour Blueprint subclass.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UTankEngineFireBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	UTankEngineFireBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnRefreshed(UBehaviour* RefreshingBehaviour) override;

private:
	void SampleDuration();
	void StartFireEffect();
	void StopFireEffect();
	bool GetIsValidTankMaster() const;
	bool GetIsValidFireSubsystem() const;

	static constexpr float DefaultDamagePerSecond = 5.0f;
	static constexpr float DefaultMinimumDurationSeconds = 6.0f;
	static constexpr float DefaultMaximumDurationSeconds = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Damage", meta=(AllowPrivateAccess="true", ClampMin="0"))
	float M_DamagePerSecond = DefaultDamagePerSecond;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Duration", meta=(AllowPrivateAccess="true", ClampMin="0.01"))
	float M_MinDurationSeconds = DefaultMinimumDurationSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Duration", meta=(AllowPrivateAccess="true", ClampMin="0.01"))
	float M_MaxDurationSeconds = DefaultMaximumDurationSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Effect", meta=(AllowPrivateAccess="true"))
	ERTSFireType M_FireType = ERTSFireType::TankFire;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Effect", meta=(AllowPrivateAccess="true"))
	FVector M_FireWorldScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Effect", meta=(AllowPrivateAccess="true"))
	FRTSFireEffectParams M_FireEffectParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Tank Engine Fire|Effect", meta=(AllowPrivateAccess="true"))
	FTankEngineFireAttachmentRules M_AttachmentRules;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TWeakObjectPtr<URTSFireSubsystem> M_FireSubsystem;

	FDamageEvent M_DamageEvent;
	// Caps damage at the sampled lifetime even when the behaviour component ticks after expiry.
	float M_RemainingDamageSeconds = 0.0f;
	int32 M_FireHandle = INDEX_NONE;
};
