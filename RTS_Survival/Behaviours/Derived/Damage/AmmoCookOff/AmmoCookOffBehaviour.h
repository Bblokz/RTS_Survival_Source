#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Behaviours/Derived/Damage/AmmoCookOff/AmmoCookOffAttachmentRules.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "AmmoCookOffBehaviour.generated.h"

class ACPPTurretsMaster;
class ATankMaster;
class UAudioComponent;
class UMeshComponent;
class URTSFireSubsystem;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * @brief Add this permanent behaviour to a tank to burn its health and show a pooled ammunition fire.
 * Configure damage, attachment, visuals, and a looping cook-off sound in a behaviour Blueprint subclass.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UAmmoCookOffBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	UAmmoCookOffBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnTick(float DeltaTime) override;

private:
	void StartCookOffEffect();
	void StopCookOffEffect();
	void StartCookOffSound();
	void StopCookOffSound();
	ACPPTurretsMaster* GetFirstMountedTurret() const;

	/**
	 * @brief Resolve the component and transform both the Niagara effect and spatial sound must follow.
	 * @param OutAttachActor Actor owning the resolved attachment component.
	 * @param OutAttachComponent Hull mesh for offset mode or first mounted turret mesh for socket mode.
	 * @param OutSocketName Turret socket for socket mode, otherwise NAME_None.
	 * @param OutRelativeOffset Component-relative offset for the effect and sound.
	 * @return True when the configured attachment target is usable.
	 */
	bool TryGetAttachment(AActor*& OutAttachActor,
	                      USceneComponent*& OutAttachComponent,
	                      FName& OutSocketName,
	                      FVector& OutRelativeOffset) const;
	bool GetIsValidTankMaster() const;
	bool GetIsValidFireSubsystem() const;
	bool GetIsValidCookOffSound() const;
	bool GetIsValidCookOffAudioComponent() const;

	static constexpr float DefaultDamagePerSecond = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Damage",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	float M_DamagePerSecond = DefaultDamagePerSecond;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Effect", meta=(AllowPrivateAccess="true"))
	ERTSFireType M_FireType = ERTSFireType::AmmoCookOffInf;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Effect", meta=(AllowPrivateAccess="true"))
	FVector M_FireWorldScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Effect", meta=(AllowPrivateAccess="true"))
	FRTSFireEffectParams M_FireEffectParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Effect", meta=(AllowPrivateAccess="true"))
	FAmmoCookOffAttachmentRules M_AttachmentRules;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Sound", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> M_CookOffSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Sound", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundConcurrency> M_SoundConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Ammo Cook Off|Sound", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundAttenuation> M_SoundAttenuation = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TWeakObjectPtr<URTSFireSubsystem> M_FireSubsystem;

	UPROPERTY()
	TWeakObjectPtr<UAudioComponent> M_CookOffAudioComponent;

	FDamageEvent M_DamageEvent;
	int32 M_FireHandle = INDEX_NONE;
};
