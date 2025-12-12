// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "SquadUnitAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SquadAnimationEnums/SquadAnimationEnums.h"

FAimOffsetTypes::FAimOffsetTypes()
	: M_ActiveAimOffset(nullptr)
	  , M_ActiveAimOffsetSequence(nullptr)
	  , RifleAimOffset(nullptr)
	  , RifleCrouchAimOffset(nullptr)
	  , PistolAimOffset(nullptr)
	  , PistolCrouchAimOffset(nullptr)
	  , HipAimOffset(nullptr)
	  , HipCrouchAimOffset(nullptr)
	  , M_AimOffsetType(ESquadWeaponAimOffset::Rifle)
	  , RifleAimOffsetSequence(nullptr)
	  , RifleCrouchAimOffsetSequence(nullptr)
	  , PistolAimOffsetSequence(nullptr)
	  , PistolCrouchAimOffsetSequence(nullptr)
	  , HipAimOffsetSequence(nullptr)
	  , HipCrouchAimOffsetSequence(nullptr)


{
}


void FAimOffsetTypes::UpdateAOForNewWeapon(const ESquadWeaponAimOffset NewAimOffsetType)
{
	M_AimOffsetType = NewAimOffsetType;
	switch (NewAimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
		M_ActiveAimOffset = RifleAimOffset;
		M_ActiveAimOffsetSequence = RifleAimOffsetSequence;
		break;
	case ESquadWeaponAimOffset::Pistol:
		M_ActiveAimOffset = PistolAimOffset;
		M_ActiveAimOffsetSequence = PistolAimOffsetSequence;
		break;
	case ESquadWeaponAimOffset::Hip:
		M_ActiveAimOffset = HipAimOffset;
		M_ActiveAimOffsetSequence = HipAimOffsetSequence;
		break;
	default:
		RTSFunctionLibrary::ReportError(
			"could not find the aim offset type in FAimOffsetTypes::UpdateAOForNewWeapon"
			"for the provided enum type."
			"\n Falling back to Rifle aim offset!");
		M_AimOffsetType = ESquadWeaponAimOffset::Rifle;
		M_ActiveAimOffset = RifleAimOffset;
		M_ActiveAimOffsetSequence = RifleAimOffsetSequence;
	}
}

void FAimOffsetTypes::UpdateAOForNewAimPosition(const ESquadAimPosition NewAimPosition)
{
	switch (M_AimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
		SetRifle_AOandSequenceForNewAim(NewAimPosition);
		break;
	case ESquadWeaponAimOffset::Pistol:
		SetPistol_AOandSequenceForNewAim(NewAimPosition);
		break;
	case ESquadWeaponAimOffset::Hip:
		SetHip_AOandSequenceForNewAim(NewAimPosition);
		break;
	}
}

void FAimOffsetTypes::SetRifle_AOandSequenceForNewAim(const ESquadAimPosition NewAimPosition)
{
	switch (NewAimPosition)
	{
	case ESquadAimPosition::Standing:
		M_ActiveAimOffset = RifleAimOffset;
		M_ActiveAimOffsetSequence = RifleAimOffsetSequence;
		break;
	case ESquadAimPosition::Crouch:
		M_ActiveAimOffset = RifleCrouchAimOffset;
		M_ActiveAimOffsetSequence = RifleCrouchAimOffsetSequence;
		break;
	case ESquadAimPosition::Prone:
		break;
	}
}

void FAimOffsetTypes::SetHip_AOandSequenceForNewAim(const ESquadAimPosition NewAimPosition)
{
	switch (NewAimPosition)
	{
	case ESquadAimPosition::Standing:
		M_ActiveAimOffset = HipAimOffset;
		M_ActiveAimOffsetSequence = HipAimOffsetSequence;
		break;
	case ESquadAimPosition::Crouch:
		M_ActiveAimOffset = HipCrouchAimOffset;
		M_ActiveAimOffsetSequence = HipCrouchAimOffsetSequence;
		break;
	case ESquadAimPosition::Prone:
		break;
	}
}

void FAimOffsetTypes::SetPistol_AOandSequenceForNewAim(const ESquadAimPosition NewAimPosition)
{
	switch (NewAimPosition)
	{
	case ESquadAimPosition::Standing:
		M_ActiveAimOffset = PistolAimOffset;
		M_ActiveAimOffsetSequence = PistolAimOffsetSequence;
		break;
	case ESquadAimPosition::Crouch:
		M_ActiveAimOffset = PistolCrouchAimOffset;
		M_ActiveAimOffsetSequence = PistolCrouchAimOffsetSequence;
		break;
	case ESquadAimPosition::Prone:
		break;
	}
}

FWeaponMontages::FWeaponMontages()
	: MontageReloadRifleStanding(nullptr)
	  , MontageReloadRifleHip(nullptr)
	  , MontageReloadPistolStanding(nullptr)
	  , RifleSingleFireMontage(nullptr)
	  , RifleBurstFireMontage(nullptr)
	  , RifleCrouchSingleFireMontage(nullptr)
	  , RifleCrouchBurstFireMontage(nullptr)
	  , RifleProneSingleFireMontage(nullptr)
	  , RifleProneBurstFireMontage(nullptr)
	  , HipFireSingleMontage(nullptr)
	  , HipFireBurstMontage(nullptr)
	  , HipFireCrouchSingleMontage(nullptr)
	  , HipFireCrouchBurstMontage(nullptr)
	  , HipFireProneSingleMontage(nullptr)
	  , HipFireProneBurstMontage(nullptr)
	  , SwitchToPistolMontage(nullptr)
	  , SwitchToRifleMontage(nullptr)
	  , ActiveWeaponMontage(ESquadWeaponMontage::NoActiveWeaponMontage)
{
	// Initialize arrays if needed
	PistolSingleFireMontages = {};
	PistolCrouchSingleFireMontages = {};
	PistolProneSingleFireMontages = {};
}


UAnimMontage* FWeaponMontages::GetFireMontage(
	const ESquadAimPosition FirePosition,
	const ESquadWeaponAimOffset AimOffsetType, const bool bIsSingleFire)
{
	switch (AimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
		ActiveWeaponMontage = bIsSingleFire
			                      ? ESquadWeaponMontage::FireRifleSingle
			                      : ESquadWeaponMontage::FireRifleBurst;
	// Return the fire montage in either standing, crouch or prone position.
		return GetRifleFireMontage(FirePosition, bIsSingleFire);
	case ESquadWeaponAimOffset::Pistol:
		ActiveWeaponMontage = ESquadWeaponMontage::FirePistolSingle;
		return GetPistolFireMontage(FirePosition);
	case ESquadWeaponAimOffset::Hip:
		ActiveWeaponMontage = bIsSingleFire
			                      ? ESquadWeaponMontage::FireRifleSingle
			                      : ESquadWeaponMontage::FireRifleBurst;
		return GetHipFireMontage(FirePosition, bIsSingleFire);
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FWeaponMontages::GetFireMontageType"
		"for the provided enum type."
		"\n Falling back to FireRifleSingle!");
	ActiveWeaponMontage = bIsSingleFire ? ESquadWeaponMontage::FireRifleSingle : ESquadWeaponMontage::FireRifleBurst;
	return bIsSingleFire ? RifleSingleFireMontage : RifleBurstFireMontage;
}

UAnimMontage* FWeaponMontages::GetRifleFireMontage(const ESquadAimPosition FirePosition, const bool bIsSingleFire) const
{
	switch (FirePosition)
	{
	case ESquadAimPosition::Standing:
		return bIsSingleFire ? RifleSingleFireMontage : RifleBurstFireMontage;
	case ESquadAimPosition::Crouch:
		return bIsSingleFire ? RifleCrouchSingleFireMontage : RifleCrouchBurstFireMontage;
	case ESquadAimPosition::Prone:
		return bIsSingleFire ? RifleProneSingleFireMontage : RifleProneBurstFireMontage;
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FWeaponMontages::GetRifleFireMontage"
		"for the provided enum type."
		"\n Falling back to FireRifleSingle!");
	return RifleSingleFireMontage;
}

UAnimMontage* FWeaponMontages::GetPistolFireMontage(const ESquadAimPosition FirePosition) const
{
	switch (FirePosition)
	{
	case ESquadAimPosition::Standing:
		return GetRandomStandingPistolFireMontage();
	case ESquadAimPosition::Crouch:
		return GetRandomCrouchPistolFireMontage();
	case ESquadAimPosition::Prone:
		return GetRandomPronePistolFireMontage();
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FWeaponMontages::GetPistolFireMontage"
		"for the provided enum type."
		"\n Falling back to FirePistolSingle!");
	return PistolSingleFireMontages[0];
}

UAnimMontage* FWeaponMontages::GetHipFireMontage(const ESquadAimPosition FirePosition, const bool bIsSingleFire) const
{
	switch (FirePosition)
	{
	case ESquadAimPosition::Standing:
		return bIsSingleFire ? HipFireSingleMontage : HipFireBurstMontage;
	case ESquadAimPosition::Crouch:
		return bIsSingleFire ? HipFireCrouchSingleMontage : HipFireCrouchBurstMontage;
	case ESquadAimPosition::Prone:
		return bIsSingleFire ? HipFireProneSingleMontage : HipFireProneBurstMontage;
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FWeaponMontages::GetHipFireMontage"
		"for the provided enum type."
		"\n Falling back to FireHipSingle!");
	return HipFireSingleMontage;
}

UAnimMontage* FWeaponMontages::GetReloadMontage(
	const ESquadWeaponAimOffset AimOffsetType)
{
	switch (AimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
		ActiveWeaponMontage = ESquadWeaponMontage::ReloadRifle;
		return MontageReloadRifleStanding;
	case ESquadWeaponAimOffset::Pistol:
		ActiveWeaponMontage = ESquadWeaponMontage::ReloadPistol;
		return MontageReloadPistolStanding;
	case ESquadWeaponAimOffset::Hip:
		ActiveWeaponMontage = ESquadWeaponMontage::ReloadHip;
		return MontageReloadRifleHip;
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FWeaponMontages::GetReloadMontageType"
		"for the provided enum type."
		"\n Falling back to ReloadRifle!");
	ActiveWeaponMontage = ESquadWeaponMontage::ReloadRifle;
	return MontageReloadRifleStanding;
}

UAnimMontage* FWeaponMontages::GetSwitchWeaponMontage(const ESquadWeaponAimOffset AimOffsetOfNewWeapon)
{
	switch (AimOffsetOfNewWeapon)
	{
	case ESquadWeaponAimOffset::Rifle:
	case ESquadWeaponAimOffset::Hip:
		ActiveWeaponMontage = ESquadWeaponMontage::SwitchToRifle;
		return SwitchToRifleMontage;
	case ESquadWeaponAimOffset::Pistol:
		ActiveWeaponMontage = ESquadWeaponMontage::SwitchToPistol;
		return SwitchToPistolMontage;
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FWeaponMontages::GetSwitchWeaponMontage"
		"for the provided enum type."
		"\n Falling back to SwitchToRifleMontage!");
	return SwitchToRifleMontage;
}


UAnimMontage* FWeaponMontages::GetRandomStandingPistolFireMontage() const
{
	const int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, PistolSingleFireMontages.Num() - 1);
	return PistolSingleFireMontages[RandomIndex];
}

UAnimMontage* FWeaponMontages::GetRandomCrouchPistolFireMontage() const
{
	const int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, PistolCrouchSingleFireMontages.Num() - 1);
	return PistolCrouchSingleFireMontages[RandomIndex];
}

UAnimMontage* FWeaponMontages::GetRandomPronePistolFireMontage() const
{
	const int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, PistolProneSingleFireMontages.Num() - 1);
	return PistolProneSingleFireMontages[RandomIndex];
}

FAimPositionMontages::FAimPositionMontages()
	: ActiveAimPositionMontage(ESquadAimPositionMontage::NoActiveAimPositionMontage)
	  , AimPosition(ESquadAimPosition::Standing)
	  , StandingToCrouch(nullptr)
	  , HipToCrouch(nullptr)
	  , CrouchToHip(nullptr)
	  , CrouchToStanding(nullptr)
	  , CrouchToPistol(nullptr)
	  , PistolToCrouch(nullptr)
	  , Welding(nullptr)
{
}


UAnimMontage* FAimPositionMontages::GetToCrouchAimPositionMontage(
	const ESquadWeaponAimOffset AimOffsetType)
{
	switch (AimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
		if (AimPosition == ESquadAimPosition::Standing)
		{
			AimPosition = ESquadAimPosition::Crouch;
			ActiveAimPositionMontage = ESquadAimPositionMontage::StandingToCrouch;
			return StandingToCrouch;
		}
		break;
	case ESquadWeaponAimOffset::Pistol:
		if (AimPosition == ESquadAimPosition::Standing)
		{
			AimPosition = ESquadAimPosition::Crouch;
			ActiveAimPositionMontage = ESquadAimPositionMontage::PistolToCrouch;
			return PistolToCrouch;
		}
		break;
	case ESquadWeaponAimOffset::Hip:
		if (AimPosition == ESquadAimPosition::Standing)
		{
			AimPosition = ESquadAimPosition::Crouch;
			ActiveAimPositionMontage = ESquadAimPositionMontage::HipToCrouch;
			return HipToCrouch;
		}
		break;
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FAimPositionMontages::GetToCrouchAimPositionMontage"
		"for the provided enum type."
		"\n Falling back to StandingToCrouch!");
	AimPosition = ESquadAimPosition::Crouch;
	ActiveAimPositionMontage = ESquadAimPositionMontage::StandingToCrouch;
	return StandingToCrouch;
}

UAnimMontage* FAimPositionMontages::GetToStandingAimPositionMontage(const ESquadWeaponAimOffset AimOffsetType)
{
	switch (AimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
		if (AimPosition == ESquadAimPosition::Crouch)
		{
			AimPosition = ESquadAimPosition::Standing;
			ActiveAimPositionMontage = ESquadAimPositionMontage::CrouchToStanding;
			return CrouchToStanding;
		}
	// todo prone
		break;
	case ESquadWeaponAimOffset::Pistol:
		if (AimPosition == ESquadAimPosition::Crouch)
		{
			AimPosition = ESquadAimPosition::Standing;
			ActiveAimPositionMontage = ESquadAimPositionMontage::CrouchToPistol;
			return CrouchToPistol;
		}
		break;
	case ESquadWeaponAimOffset::Hip:
		if (AimPosition == ESquadAimPosition::Crouch)
		{
			AimPosition = ESquadAimPosition::Standing;
			ActiveAimPositionMontage = ESquadAimPositionMontage::CrouchToHip;
			return CrouchToHip;
		}
	// todo prone
		break;
	}
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FAimPositionMontages::GetToStandingAimPositionMontage"
		"for the provided enum type."
		"\n Falling back to CrouchToStanding!");
	AimPosition = ESquadAimPosition::Standing;
	ActiveAimPositionMontage = ESquadAimPositionMontage::CrouchToStanding;
	return CrouchToStanding;
}

UAnimMontage* FAimPositionMontages::GetMiscFullBodyMontage(const ESquadAimPositionMontage MontageType)
{
	switch (MontageType)
	{
	case ESquadAimPositionMontage::Misc_Welding:
		return Welding;
	default:
		break;
	}
	FString MontageName = UEnum::GetValueAsString(MontageType);
	RTSFunctionLibrary::ReportError(
		"could not find the aim offset type in FAimPositionMontages::GetMiscFullBodyMontage"
		"for the provided enum type. MontageType: " + MontageName +
		"\n Falling back to Welding!");
	return Welding;
}

USquadUnitAnimInstance::USquadUnitAnimInstance(): bBeAlert(false), MovementState(), WeaponMontages(),
                                                  bAimToTarget(false), AimPositionMontages(),
                                                  Speed(0), AimOffsetAngle(0),
                                                  WalkingPlayRate(0), RunningPlayRate(0)

{
}

void USquadUnitAnimInstance::UpdateAnimState(const FVector& VectorSpeed)
{
	Speed = VectorSpeed.Size();

	// Calculate the walking play rate: 0.0 at speed 0, up to 1.5 at speed 600 or higher
	WalkingPlayRate = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 600.0f), FVector2D(0.0f, 1.5f), Speed);

	// Calculate the running play rate: 1.0 at speed 600, up to 2.0 at speed 1200 or higher
	RunningPlayRate = FMath::GetMappedRangeValueClamped(FVector2D(600.0f, 1200.0f), FVector2D(1.0f, 2.0f), Speed);

	SetMovementStateWithSpeed(Speed);
}

void USquadUnitAnimInstance::BindSelectionFunctions(USelectionComponent* SelectionComponent)
{
	if (IsValid(SelectionComponent))
	{
		SelectionComponent->OnUnitSelected.AddUObject(this, &USquadUnitAnimInstance::OnUnitSelected);
		SelectionComponent->OnUnitDeselected.AddUObject(this, &USquadUnitAnimInstance::OnUnitDeselected);
	}
}

void USquadUnitAnimInstance::PlaySingleFireAnim()
{
	UAnimMontage* SelectedMontage = WeaponMontages.GetFireMontage(AimPositionMontages.AimPosition,
	                                                              AimOffsets.M_AimOffsetType, true);
	StartMontage(SelectedMontage, true);
}

void USquadUnitAnimInstance::PlayBurstAnim()
{
	UAnimMontage* SelectedMontage = WeaponMontages.GetFireMontage(AimPositionMontages.AimPosition,
	                                                              AimOffsets.M_AimOffsetType, false);
	StartMontage(SelectedMontage, true);
}

void USquadUnitAnimInstance::PlayReloadAnim(const float ReloadTime)
{
	UAnimMontage* SelectedMontage = WeaponMontages.GetReloadMontage(AimOffsets.M_AimOffsetType);
	StartMontage(SelectedMontage, true, ReloadTime);
}

void USquadUnitAnimInstance::PlaySwitchWeaponMontage(const ESquadWeaponAimOffset NewWeaponAimOffset)
{
	UAnimMontage* SelectedMontage = WeaponMontages.GetSwitchWeaponMontage(NewWeaponAimOffset);
	StartMontage(SelectedMontage, true);
}

void USquadUnitAnimInstance::PlayWeldingMontage()
{
	UAnimMontage* SelectedMontage = AimPositionMontages.GetMiscFullBodyMontage(ESquadAimPositionMontage::Misc_Welding);
	StartMontage(SelectedMontage, false);
}

void USquadUnitAnimInstance::StopAllMontages()
{
	Montage_Stop(0.1f);
}

void USquadUnitAnimInstance::SetWeaponAimOffset(const ESquadWeaponAimOffset AimOffsetType)
{
	switch (AimOffsetType)
	{
	case ESquadWeaponAimOffset::Rifle:
	case ESquadWeaponAimOffset::Pistol:
	case ESquadWeaponAimOffset::Hip:
		AimOffsets.UpdateAOForNewWeapon(AimOffsetType);
		AimOffsets.UpdateAOForNewAimPosition(ESquadAimPosition::Standing);
		break;
	default:
		RTSFunctionLibrary::ReportError(
			"could not find the aim offset type in USquadUnitAnimInstance::SetWeaponAimOffset"
			"for the provided enum type."
			"\n Falling back to Rifle aim offset!");
		AimOffsets.UpdateAOForNewWeapon(ESquadWeaponAimOffset::Rifle);
		AimOffsets.UpdateAOForNewAimPosition(ESquadAimPosition::Standing);
	}
}


void USquadUnitAnimInstance::UnitDies() const
{
	if (IsValid(GetSkelMeshComponent()))
	{
		AActor* Owner = GetSkelMeshComponent()->GetOwner();
		if (IsValid(Owner))
		{
			USelectionComponent* SelectionComponent = Owner->FindComponentByClass<USelectionComponent>();
			if (IsValid(SelectionComponent))
			{
				SelectionComponent->OnUnitSelected.RemoveAll(this);
				SelectionComponent->OnUnitDeselected.RemoveAll(this);
				return;
			}
		}
		RTSFunctionLibrary::ReportError(
			"Could not find the selection component in USquadUnitAnimInstance::UnitDies");
		return;
	}
	RTSFunctionLibrary::ReportError("Could not find the skel mesh component in USquadUnitAnimInstance::UnitDies");
}

void USquadUnitAnimInstance::StartMontage(
	UAnimMontage* SelectedMontage,
	const bool bIsWeaponMontage,
	const float PlayTime)
{
	if (!SelectedMontage)
	{
		RTSFunctionLibrary::ReportError("Selected Montage is null in USquadUnitAnimInstance::StartMontage");
		return;
	}

	float PlayRate = 1.0f;
	if (PlayTime > 0.0f)
	{
		const float MontageLength = SelectedMontage->GetPlayLength();
		PlayRate = MontageLength / PlayTime;
	}
	Montage_Play(SelectedMontage, PlayRate);

	// Unbind any existing delegate bindings to avoid duplicate bindings
	M_MontageEndedDelegate.Unbind();

	if (bIsWeaponMontage)
	{
		M_MontageEndedDelegate.BindUObject(this, &USquadUnitAnimInstance::OnWeaponMontageFinished);
	}
	else
	{
		M_MontageEndedDelegate.BindUObject(this, &USquadUnitAnimInstance::OnAimPositionMontageFinished);
	}


	// Bind the OnMontageEnded delegate to call OnWeaponMontageFinished with the correct MontageType
	Montage_SetEndDelegate(M_MontageEndedDelegate, SelectedMontage);
}

void USquadUnitAnimInstance::OnWeaponMontageFinished(UAnimMontage* Montage, bool bInterrupted)
{
	WeaponMontages.ActiveWeaponMontage = ESquadWeaponMontage::NoActiveWeaponMontage;
}

void USquadUnitAnimInstance::OnAimPositionMontageFinished(UAnimMontage* Montage, bool bInterrupted)
{
	AimPositionMontages.ActiveAimPositionMontage = ESquadAimPositionMontage::NoActiveAimPositionMontage;
}


void USquadUnitAnimInstance::SetMovementStateWithSpeed(const float& MovementSpeed)
{
	if (FMath::IsNearlyZero(MovementSpeed))
	{
		// In this case we just started aiming while idle and can choose a random aim position.
		if (bAimToTarget && MovementState != ESquadMovementAnimState::Idle && AimPositionMontages.AimPosition ==
			ESquadAimPosition::Standing)
		{
			OnStartAimingWhileIdle();
		}
		MovementState = ESquadMovementAnimState::Idle;
		return;
	}
	if (MovementSpeed >= 600)
	{
		MovementState = ESquadMovementAnimState::Running;
		return;
	}
	if (MovementState != ESquadMovementAnimState::Walking)
	{
		OnStartWalking();
	}
	MovementState = ESquadMovementAnimState::Walking;
}

void USquadUnitAnimInstance::OnStartAimingWhileIdle()
{
	if (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("OnStartAimingWhileIdle", FColor::Purple);
	}
	// Get random enum value.
	ESquadAimPosition NewAimPosition = FMath::RandBool() ? ESquadAimPosition::Crouch : ESquadAimPosition::Standing;
	if (NewAimPosition != ESquadAimPosition::Standing)
	{
		AimOffsets.UpdateAOForNewAimPosition(NewAimPosition);
		// Play the aim position montage also saves the aim state.
		// todo only supports crouch now.
		UAnimMontage* SelectedMontage = AimPositionMontages.GetToCrouchAimPositionMontage(AimOffsets.M_AimOffsetType);
		StartMontage(SelectedMontage, false);
	}
}

void USquadUnitAnimInstance::OnStartWalking()
{
	if (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("OnStartWalking", FColor::Purple);
	}
	UAnimMontage* TransitionMontage = nullptr;
	switch (AimPositionMontages.AimPosition)
	{
	case ESquadAimPosition::Standing:
		if (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Standing; NO CHANGE", FColor::Purple);
		}
		break;
	case ESquadAimPosition::Prone:
	case ESquadAimPosition::Crouch:
		if (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Crouching; Back to Standing", FColor::Purple);
		}

		AimOffsets.UpdateAOForNewAimPosition(ESquadAimPosition::Standing);
		TransitionMontage = AimPositionMontages.GetToStandingAimPositionMontage(AimOffsets.M_AimOffsetType);
		break;
	}
	if (TransitionMontage)
	{
		StartMontage(TransitionMontage, false);
	}
}

void USquadUnitAnimInstance::OnUnitSelected()
{
	bBeAlert = true;
}

void USquadUnitAnimInstance::OnUnitDeselected()
{
	bBeAlert = false;
}
