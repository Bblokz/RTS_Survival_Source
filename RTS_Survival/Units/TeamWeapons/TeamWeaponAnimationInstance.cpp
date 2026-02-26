// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TeamWeaponAnimationInstance.h"

#include "TeamWeapon.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTeamWeaponAnimationInstance::InitTeamWeaponAnimInst(const float DeploymentTime)
{
	M_DeploymentTime = DeploymentTime;
}

void UTeamWeaponAnimationInstance::PlayLegsWheelsSlotMontage(const ETeamWeaponMontage MontageType,
                                                             const bool bWaitForMontage)
{
	if (not GetIsWheelsLegsMontage(MontageType))
	{
		const FString MontageAsString = UEnum::GetValueAsString(MontageType);
		RTSFunctionLibrary::ReportError(
			"the montage:  " + MontageAsString + "\n is NOT a wheels legs montage slot type of montage!"
			"\n but a request for playing such a montage was made!");
		return;
	}
	float PlayRate = 1.f;
	UAnimMontage* Montage = GetMontageAndPlayrate(MontageType, PlayRate);
	if (not Montage)
	{
		const FString MontageAsString = UEnum::GetValueAsString(MontageType);
		RTSFunctionLibrary::ReportError(
			"the montage:  " + MontageAsString + "\n is null for playing in the legs and wheels slot!"
			"\n check the anim instance defaults!");
		return;
	}
	if (bWaitForMontage)
	{
		PlayAndWaitForMontage(MontageType, Montage, PlayRate);
	}
	else
	{
		InstantlyFinishMontage(MontageType);
	}
}

void UTeamWeaponAnimationInstance::PlayFireAnimation()
{
	if(not GunMontages.FireMontage)
	{
		RTSFunctionLibrary::ReportError("No valid FireMontage for team weapon anim instance: " + GetName() +
			"\n check the anim instance defaults!");
		return;
	}
	Montage_Play( GunMontages.FireMontage, 1.f);
}

void UTeamWeaponAnimationInstance::PlayBurstFireAnimation()
{
	
	if(not GunMontages.BurstFireMontage)
	{
		RTSFunctionLibrary::ReportError("No valid FireMontage for team weapon anim instance: " + GetName() +
			"\n check the anim instance defaults!");
		return;
	}
	Montage_Play( GunMontages.BurstFireMontage, 1.f);
}

void UTeamWeaponAnimationInstance::StartMoveLoop(const bool bForward)
{
	if (bM_IsMoveLoopActive && bM_MoveLoopForward == bForward && M_MoveLoopMontage && Montage_IsPlaying(M_MoveLoopMontage))
	{
		return;
	}

	float MoveMontagePlayRate = 1.0f;
	const ETeamWeaponMontage MoveMontageType = bForward
		? ETeamWeaponMontage::MoveForwardMontage
		: ETeamWeaponMontage::MoveBackwardMontage;
	UAnimMontage* MoveMontage = GetMontageAndPlayrate(MoveMontageType, MoveMontagePlayRate);
	if (not MoveMontage)
	{
		return;
	}

	bM_IsMoveLoopActive = true;
	bM_MoveLoopForward = bForward;
	M_MoveLoopMontage = MoveMontage;

	M_MoveLoopMontageEndedDelegate.Unbind();
	M_MoveLoopMontageEndedDelegate.BindUObject(this, &UTeamWeaponAnimationInstance::OnMoveLoopMontageEnded);

	Montage_Play(MoveMontage, MoveMontagePlayRate);
	Montage_SetEndDelegate(M_MoveLoopMontageEndedDelegate, MoveMontage);
}

void UTeamWeaponAnimationInstance::StopMoveLoop()
{
	bM_IsMoveLoopActive = false;
	M_MoveLoopMontageEndedDelegate.Unbind();

	if (M_MoveLoopMontage && Montage_IsPlaying(M_MoveLoopMontage))
	{
		constexpr float MoveMontageBlendOutTime = 0.1f;
		Montage_Stop(MoveMontageBlendOutTime, M_MoveLoopMontage);
	}

	M_MoveLoopMontage = nullptr;
}

UAnimSequence* UTeamWeaponAnimationInstance::GetCurrentAOBaseSequence()
{
	if (not M_ActiveSequence)
	{
		RTSFunctionLibrary::ReportError("No valid active sequence for team weapon anim instance: " + GetName());
		return Sequences.DeployedSequence;
	}
	return M_ActiveSequence;
}


bool UTeamWeaponAnimationInstance::GetIsWheelsLegsMontage(const ETeamWeaponMontage NewMontage) const
{
	const bool bIsMovement = (NewMontage == ETeamWeaponMontage::MoveBackwardMontage || NewMontage ==
		ETeamWeaponMontage::MoveForwardMontage);
	const bool bIsDeployPack = (NewMontage == ETeamWeaponMontage::PackMontage || NewMontage ==
		ETeamWeaponMontage::DeployMontage);
	if (not bIsMovement && not bIsDeployPack)
	{
		return false;
	}
	return true;
}

void UTeamWeaponAnimationInstance::PlayAndWaitForMontage(const ETeamWeaponMontage MontageType,
                                                         UAnimMontage* ValidMontage, const float PlayRate)
{
	// Unbind any existing delegate bindings to avoid duplicate bindings
	M_MontageEndedDelegate.Unbind();
	switch (MontageType)
	{
	case ETeamWeaponMontage::DeployMontage:
		M_MontageEndedDelegate.BindUObject(this, &UTeamWeaponAnimationInstance::OnDeployMontageFinished);
		break;
	case ETeamWeaponMontage::PackMontage:
		M_MontageEndedDelegate.BindUObject(this, &UTeamWeaponAnimationInstance::OnPackMontageFinished);
		break;
	// Falls through.
	case ETeamWeaponMontage::MoveBackwardMontage:
	case ETeamWeaponMontage::MoveForwardMontage:
		break;
	default:
		return;
	}
	Montage_Play(ValidMontage, PlayRate);
	// Bind the OnMontageEnded delegate to call OnWeaponMontageFinished with the correct MontageType
	Montage_SetEndDelegate(M_MontageEndedDelegate, ValidMontage);
}

void UTeamWeaponAnimationInstance::InstantlyFinishMontage(const ETeamWeaponMontage MontageType)
{
	switch (MontageType)
	{
	case ETeamWeaponMontage::NoMontage:
		break;
	case ETeamWeaponMontage::DeployMontage:
		OnDeployMontageFinished(nullptr, false);
		return;
	case ETeamWeaponMontage::PackMontage:
		OnPackMontageFinished(nullptr, false);
		return;
	case ETeamWeaponMontage::MoveForwardMontage:
		break;
	case ETeamWeaponMontage::MoveBackwardMontage:
		break;
	case ETeamWeaponMontage::FireMontage:
		break;
	}
}

void UTeamWeaponAnimationInstance::OnPackMontageFinished(UAnimMontage* Montage, bool bInterrupted)
{
	SetBaseSequenceToPacked();
}

void UTeamWeaponAnimationInstance::OnDeployMontageFinished(UAnimMontage* Montage, bool bInterrupted)
{
	SetBaseSequenceToDeployed();
}

void UTeamWeaponAnimationInstance::SetBaseSequenceToPacked()
{
	if (not Sequences.PackedSequence)
	{
		RTSFunctionLibrary::ReportError("No valid PackedSequence for team weapon anim instance: " + GetName());
		return;
	}
	M_ActiveSequence = Sequences.PackedSequence;
}

void UTeamWeaponAnimationInstance::SetBaseSequenceToDeployed()
{
	if (not Sequences.DeployedSequence)
	{
		RTSFunctionLibrary::ReportError("No valid DeployedSequence for team weapon anim instance: " + GetName());
		return;
	}
	M_ActiveSequence = Sequences.DeployedSequence;
}

UAnimMontage* UTeamWeaponAnimationInstance::GetMontageAndPlayrate(const ETeamWeaponMontage MontageType,
                                                                  float& OutPlayrate)
{
	const float BaseMontageTime = GetMontageBaseTime(DeployAndMvtMontages.DeployToPacked);
	const float SafeDeploymentTime = FMath::Max(M_DeploymentTime, 0.01f);
	const float SafeBaseMontageTime = FMath::Max(BaseMontageTime, 0.01f);
	switch (MontageType)
	{
	case ETeamWeaponMontage::NoMontage:
		return nullptr;
	case ETeamWeaponMontage::DeployMontage:
		OutPlayrate = -SafeBaseMontageTime / SafeDeploymentTime;
		return DeployAndMvtMontages.DeployToPacked;
	case ETeamWeaponMontage::PackMontage:
		OutPlayrate = SafeBaseMontageTime / SafeDeploymentTime;
		return DeployAndMvtMontages.DeployToPacked;
	case ETeamWeaponMontage::MoveForwardMontage:
		OutPlayrate = TWAnimSettings.FwdWheelsMovePlayRate;
		return DeployAndMvtMontages.MoveFwdMontage;
	case ETeamWeaponMontage::MoveBackwardMontage:
		OutPlayrate = TWAnimSettings.BwdWheelsMovePlayRate;
		return DeployAndMvtMontages.MoveFwdMontage;
	case ETeamWeaponMontage::FireMontage:
		OutPlayrate = 1.f;
		return GunMontages.FireMontage;
	}
	return nullptr;
}

float UTeamWeaponAnimationInstance::GetMontageBaseTime(const UAnimMontage* Montage)
{
	if (not Montage)
	{
		return 1;
	}
	return Montage->GetPlayLength() * Montage->RateScale;
}

void UTeamWeaponAnimationInstance::OnMoveLoopMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (not bM_IsMoveLoopActive)
	{
		return;
	}

	if (bInterrupted)
	{
		return;
	}

	StartMoveLoop(bM_MoveLoopForward);
}
