// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/TurretAnimInstance/TurretAnimInstance.h"
#include "TeamWeaponState/TeamWeaponState.h"
#include "TeamWeaponAnimationInstance.generated.h"

// The base sequences use to feed into the AO that uses the pitch and yaw from the turret anim instance base.
USTRUCT(BlueprintType)
struct FTeamWeaponSequences
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* PackedSequence = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* DeployedSequence = nullptr;
};

// We assume the designer places only montages in here that are of the same slot and that this slot does not interfere with the weapon.
USTRUCT(BlueprintType)
struct FTeamWeaponLegsAndWheelsSlot
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* DeployToPacked = nullptr;
	// Can be left null for weapons that do not play an animation while moving like mortars.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* MoveFwdMontage = nullptr;
};


USTRUCT(BlueprintType)
struct FTeamWeaponGunSlot
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* FireMontage = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* BurstFireMontage = nullptr;
};

// The playrates to use for the wheel animations (if used)
USTRUCT(BlueprintType)
struct FTeamWeaponAnimSettings
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FwdWheelsMovePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BwdWheelsMovePlayRate = -1.0f;
};

class ATeamWeapon;
enum class ETeamWeaponState : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTeamWeaponAnimationInstance : public UTurretAnimInstance
{
	GENERATED_BODY()

public:
	void InitTeamWeaponAnimInst(const float DeploymentTime);
	void PlayLegsWheelsSlotMontage(const ETeamWeaponMontage MontageType,
	                               bool bWaitForMontage = true);
	void PlayFireAnimation();
	void PlayBurstFireAnimation();

protected:
	/**
	 * Threadsafe to allow multiple threads to access the function.
	 * @return The current aim offset sequence that is the base pose, based on the current aim offset type.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, meta = (BlueprintThreadSafe))
	inline UAnimSequence* GetCurrentAOBaseSequence();
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings")
	FTeamWeaponAnimSettings TWAnimSettings;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animations|Sequences")
	FTeamWeaponSequences Sequences;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animations|Montages")
	FTeamWeaponGunSlot GunMontages;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animations|Montages")
	FTeamWeaponLegsAndWheelsSlot DeployAndMvtMontages;

private:
	UPROPERTY()
	TObjectPtr<UAnimSequence> M_ActiveSequence;

	bool GetIsWheelsLegsMontage(const ETeamWeaponMontage NewMontage) const;


	void PlayAndWaitForMontage(const ETeamWeaponMontage MontageType, UAnimMontage* ValidMontage,
		const float PlayRate);
	void InstantlyFinishMontage(const ETeamWeaponMontage MontageType);
	FOnMontageEnded M_MontageEndedDelegate;
	void OnPackMontageFinished(UAnimMontage* Montage, bool bInterrupted);
	void OnDeployMontageFinished(UAnimMontage* Montage, bool bInterrupted);
	void SetBaseSequenceToPacked();
	void SetBaseSequenceToDeployed();
	UAnimMontage* GetMontageAndPlayrate(const ETeamWeaponMontage MontageType, float& OutPlayrate);
	float GetMontageBaseTime(const UAnimMontage* Montage);

	// Set at init; defines how long packing and deploying take.
	float M_DeploymentTime;
};
