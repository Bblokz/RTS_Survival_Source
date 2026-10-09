// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "SquadUnitAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SquadAnimationEnums/SquadAnimationEnums.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSquadUnitCoverAnimation, Log, All);

namespace SquadUnitCoverAnimLogStatics
{
	constexpr uint32 FirstCoverPoseReportBit = 16;
	constexpr uint32 FirstUnplayableMontageReportBit = 8;
	constexpr uint32 AdditiveAimBaseReportBit = 1u << 30;
	constexpr uint32 AimAssetSetupReportBit = 1u << 29;

	/**
	 * Missing cover assets are a Blueprint setup gap shared by every unit of that animation class,
	 * so each gap is reported once per class instead of once per unit and transition.
	 */
	bool TryMarkMissingCoverAssetReported(const UObject& AnimInstance, const uint32 ReportBit)
	{
		static TMap<FName, uint32> ReportedBitsPerAnimClass;
		uint32& ReportedBits = ReportedBitsPerAnimClass.FindOrAdd(AnimInstance.GetClass()->GetFName());
		if ((ReportedBits & ReportBit) != 0)
		{
			return false;
		}
		ReportedBits |= ReportBit;
		return true;
	}

	FString DescribeSequenceAdditiveSetup(const UAnimSequence* Sequence)
	{
		if (not IsValid(Sequence))
		{
			return TEXT("none");
		}
		return FString::Printf(
			TEXT("%s[additive=%s base_pose=%s base_seq=%s frame=%d length=%.2f root_motion=%d]"),
			*Sequence->GetName(),
			*UEnum::GetValueAsString(Sequence->GetAdditiveAnimType()),
			*UEnum::GetValueAsString(Sequence->RefPoseType.GetValue()),
			*GetNameSafe(Sequence->RefPoseSeq),
			Sequence->RefFrameIndex,
			Sequence->GetPlayLength(),
			Sequence->HasRootMotion() ? 1 : 0);
	}

	// Lists what an aim offset and its base pose are built from, to compare a working pair with a broken one.
	void LogAimAssetSetup(const TCHAR* Role, const UAimOffsetBlendSpace* AimOffset, const UAnimSequence* BaseSequence)
	{
		UE_LOG(
			LogRTSSquadUnitCoverAnimation,
			Verbose,
			TEXT("RTS_COVER_AIM_ASSETS role=%s base=%s aim_offset=%s valid_additive=%d samples=%d"),
			Role,
			*DescribeSequenceAdditiveSetup(BaseSequence),
			*GetNameSafe(AimOffset),
			IsValid(AimOffset) && AimOffset->IsValidAdditive() ? 1 : 0,
			IsValid(AimOffset) ? AimOffset->GetBlendSamples().Num() : 0);
		if (not IsValid(AimOffset))
		{
			return;
		}
		for (const FBlendSample& Sample : AimOffset->GetBlendSamples())
		{
			UE_LOG(
				LogRTSSquadUnitCoverAnimation,
				Verbose,
				TEXT("RTS_COVER_AIM_ASSETS role=%s sample=%s at=%s"),
				Role,
				*DescribeSequenceAdditiveSetup(Cast<UAnimSequence>(Sample.Animation)),
				*Sample.SampleValue.ToCompactString());
		}
	}

	/**
	 * @brief Measures how far a clip carries the root away from where it starts.
	 * A cover idle is held for as long as the unit hides, so a travelling clip makes the unit walk in a loop.
	 * @return Largest distance of the root from its first frame, in centimetres.
	 */
	float GetLargestRootTravel(const UAnimSequence& Sequence)
	{
		constexpr int32 TravelSampleCount = 8;
		const FSkeletonPoseBoneIndex RootBoneIndex(0);
		FTransform RootAtStart;
		Sequence.GetBoneTransform(RootAtStart, RootBoneIndex, 0.0, true);
		float LargestTravel = 0.0f;
		for (int32 SampleIndex = 1; SampleIndex <= TravelSampleCount; ++SampleIndex)
		{
			FTransform RootAtSample;
			const double SampleTime = Sequence.GetPlayLength() * SampleIndex / TravelSampleCount;
			Sequence.GetBoneTransform(RootAtSample, RootBoneIndex, SampleTime, true);
			LargestTravel = FMath::Max(
				LargestTravel,
				static_cast<float>(FVector::Dist(RootAtSample.GetTranslation(), RootAtStart.GetTranslation())));
		}
		return LargestTravel;
	}
}

namespace SquadUnitTeamWeaponCrewAnimStatics
{
	// Blend out used when a crew montage is stopped because the operator leaves the deployed weapon.
	constexpr float CrewMontageBlendOutTime = 0.1f;
	// Guards the division for the reload synced play rate.
	constexpr float MinReloadTimeSeconds = 0.01f;
	// A crewing operator never walks; above this speed the crew montage is dropped no matter who moved the unit.
	// Well above the controller's settled speed so a unit that is still braking after arming is never dropped.
	constexpr float CrewAnimationMaxSpeedCmPerSec = 60.0f;

	void PrintCrewAnimDebug(const AActor* Operator, const FString& Message)
	{
		if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
		{
			if (not IsValid(Operator))
			{
				return;
			}

			RTSFunctionLibrary::PrintString(
				Operator->GetActorLocation(),
				Operator,
				Message,
				FColor::Cyan);
		}
	}
}

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
	return GetRandomStandingPistolFireMontage();
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
	if (PistolSingleFireMontages.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No standing pistol fire montages are configured in FWeaponMontages.");
		return nullptr;
	}
	const int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, PistolSingleFireMontages.Num() - 1);
	return PistolSingleFireMontages[RandomIndex];
}

UAnimMontage* FWeaponMontages::GetRandomCrouchPistolFireMontage() const
{
	if (PistolCrouchSingleFireMontages.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No crouch pistol fire montages are configured in FWeaponMontages.");
		return nullptr;
	}
	const int32 RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0, PistolCrouchSingleFireMontages.Num() - 1);
	return PistolCrouchSingleFireMontages[RandomIndex];
}

UAnimMontage* FWeaponMontages::GetRandomPronePistolFireMontage() const
{
	if (PistolProneSingleFireMontages.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No prone pistol fire montages are configured in FWeaponMontages.");
		return nullptr;
	}
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

UAnimMontage* FSquadUnitDeathMontages::GetRandomDeathMontage(const bool bUseCrouchedDeathMontage) const
{
	const TArray<TObjectPtr<UAnimMontage>>& Montages =
		bUseCrouchedDeathMontage ? CrouchedDeathMontages : StandingDeathMontages;

	int32 ValidMontageCount = 0;
	for (const TObjectPtr<UAnimMontage>& Montage : Montages)
	{
		if (not IsValid(Montage))
		{
			continue;
		}

		++ValidMontageCount;
	}

	if (ValidMontageCount == 0)
	{
		return nullptr;
	}

	int32 SelectedValidMontageIndex = FMath::RandRange(0, ValidMontageCount - 1);
	for (const TObjectPtr<UAnimMontage>& Montage : Montages)
	{
		if (not IsValid(Montage))
		{
			continue;
		}

		if (SelectedValidMontageIndex == 0)
		{
			return Montage.Get();
		}

		--SelectedValidMontageIndex;
	}

	return nullptr;
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
	case ESquadAimPositionMontage::Misc_Grenade:
		return GrenadePullAndThrow;
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

FSquadUnitStandingCoverAnimationSet::FSquadUnitStandingCoverAnimationSet()
{
	EnterStartOffset.TowardCover = SquadUnitCoverAnimDefaults::StandingEnterStartDepth;
}

FSquadUnitCoverAnimationSets::FSquadUnitCoverAnimationSets()
{
	StandingLeft.ExposedOffset.TowardCover = SquadUnitCoverAnimDefaults::StandingLeftExposedDepth;
	StandingLeft.ExposedOffset.Right = SquadUnitCoverAnimDefaults::StandingLeftExposedRight;
	StandingRight.ExposedOffset.TowardCover = SquadUnitCoverAnimDefaults::StandingRightExposedDepth;
	StandingRight.ExposedOffset.Right = SquadUnitCoverAnimDefaults::StandingRightExposedRight;
	// A soldier in a trench stands up where he crouched: no travel into the pose, none out of it.
	Trench.EnterStartOffset = FSquadUnitCoverLocalOffset();
	Trench.ExposedOffset = FSquadUnitCoverLocalOffset();
}

void FSquadUnitCoverAnimRuntime::Reset()
{
	M_IdlePose = ESquadIdleAnimationPose::Regular;
	M_Action = ESquadCoverAnimAction::None;
	M_ActiveMontageAction = ESquadCoverAnimAction::None;
	M_NextAction = ESquadCoverAnimAction::None;
	M_ActiveMontage = nullptr;
	M_ActiveMontageSeconds = 0.0f;
	M_PendingCoverReloadEndWorldSeconds = -1.0f;
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
	if (GetIsCoverAnimationActive() && not GetIsCoverFireAllowed())
	{
		return;
	}

	StartMontage(SelectFireMontage(true), true);
}

void USquadUnitAnimInstance::PlayBurstAnim()
{
	if (GetIsCoverAnimationActive() && not GetIsCoverFireAllowed())
	{
		return;
	}

	StartMontage(SelectFireMontage(false), true);
}

UAnimMontage* USquadUnitAnimInstance::SelectFireMontage(const bool bIsSingleFire)
{
	if (const FSquadUnitCoverAimAssets* CoverAimAssets = GetActiveCoverAimAssets())
	{
		UAnimMontage* CoverFireMontage = bIsSingleFire
			? CoverAimAssets->SingleFireMontage
			: CoverAimAssets->BurstFireMontage;
		if (IsValid(CoverFireMontage))
		{
			return CoverFireMontage;
		}
	}
	return WeaponMontages.GetFireMontage(AimPositionMontages.AimPosition, AimOffsets.M_AimOffsetType, bIsSingleFire);
}

const FSquadUnitCoverAimAssets* USquadUnitAnimInstance::GetActiveCoverAimAssets() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::CrouchCover)
	{
		return &CoverAnimations.Crouch.AimAssets;
	}
	const bool bIsStandingPeek = GetIsPeekPose(M_CoverAnimRuntime.M_IdlePose);
	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = bIsStandingPeek
		? GetStandingCoverAnimationSet()
		: nullptr;
	return StandingAnimationSet == nullptr ? nullptr : &StandingAnimationSet->PeekAimAssets;
}

bool USquadUnitAnimInstance::GetIsInCoverReloadPosition() const
{
	return IsValid(CoverAnimations.CoverReload) && GetIsCoverAnimationActive() &&
		M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::Exiting;
}

void USquadUnitAnimInstance::PlayCoverReloadMontage(const float RemainingReloadSeconds)
{
	M_CoverAnimRuntime.M_PendingCoverReloadEndWorldSeconds = -1.0f;
	StartMontage(CoverAnimations.CoverReload, true, RemainingReloadSeconds);
}

void USquadUnitAnimInstance::TryPlayPendingCoverReload()
{
	// A reload with less than this left would only flash the first frames of the clip.
	constexpr float MinimumShownReloadSeconds = 0.4f;
	const UWorld* World = GetWorld();
	const float PendingEndSeconds = M_CoverAnimRuntime.M_PendingCoverReloadEndWorldSeconds;
	M_CoverAnimRuntime.M_PendingCoverReloadEndWorldSeconds = -1.0f;
	if (PendingEndSeconds < 0.0f || not IsValid(World) || not GetIsInCoverReloadPosition())
	{
		return;
	}
	const float RemainingReloadSeconds = PendingEndSeconds - World->GetTimeSeconds();
	if (RemainingReloadSeconds >= MinimumShownReloadSeconds)
	{
		PlayCoverReloadMontage(RemainingReloadSeconds);
	}
}

void USquadUnitAnimInstance::StopCoverReloadMontage()
{
	constexpr float CoverReloadBlendOutSeconds = 0.15f;
	M_CoverAnimRuntime.M_PendingCoverReloadEndWorldSeconds = -1.0f;
	// A soldier that leaves its cover must not slide away in a crouched full-body clip.
	if (IsValid(CoverAnimations.CoverReload) && Montage_IsPlaying(CoverAnimations.CoverReload))
	{
		Montage_Stop(CoverReloadBlendOutSeconds, CoverAnimations.CoverReload);
	}
}

void USquadUnitAnimInstance::PlayReloadAnim(const float ReloadTime)
{
	if (GetIsInCoverReloadPosition())
	{
		// Stepping back behind the cover comes first; the reload clip follows for whatever time is left.
		const UWorld* World = GetWorld();
		if (GetIsCoverTransitionMontageActive() && IsValid(World))
		{
			M_CoverAnimRuntime.M_PendingCoverReloadEndWorldSeconds = World->GetTimeSeconds() + ReloadTime;
			return;
		}
		PlayCoverReloadMontage(ReloadTime);
		return;
	}
	UAnimMontage* SelectedMontage = WeaponMontages.GetReloadMontage(AimOffsets.M_AimOffsetType);
	StartMontage(SelectedMontage, true, ReloadTime);
}

void USquadUnitAnimInstance::PlaySwitchWeaponMontage(const ESquadWeaponAimOffset NewWeaponAimOffset)
{
	UAnimMontage* SelectedMontage = WeaponMontages.GetSwitchWeaponMontage(NewWeaponAimOffset);
	StartMontage(SelectedMontage, true);
}

void USquadUnitAnimInstance::PlayGrenadeThrowMontage(const float MontageTime)
{
	CancelCoverAnimation();
	UAnimMontage* SelectedMontage = AimPositionMontages.GetMiscFullBodyMontage(ESquadAimPositionMontage::Misc_Grenade);
	StartMontage(SelectedMontage, false, MontageTime);
}

void USquadUnitAnimInstance::PlayWeldingMontage()
{
	CancelCoverAnimation();
	UAnimMontage* SelectedMontage = AimPositionMontages.GetMiscFullBodyMontage(ESquadAimPositionMontage::Misc_Welding);
	StartMontage(SelectedMontage, false);
}

void USquadUnitAnimInstance::StopAllMontages()
{
	// Montage_Stop without a montage stops every montage; keep the full-body runtime owners in sync.
	ClearTeamWeaponCrewAnimationRuntime(false);
	ClearCoverAnimationRuntime();
	Montage_Stop(0.1f);
}

void USquadUnitAnimInstance::SetWeaponAimOffset(const ESquadWeaponAimOffset AimOffsetType)
{
	// A unit in cover keeps its cover pose: only the regular aim offset behind it changes, and that one is set
	// up for standing again when the unit leaves. A squad mate's weapon is handed over on its death, which
	// used to throw the receiving unit out of its cover.
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

bool USquadUnitAnimInstance::EnterCover(const ESquadIdleAnimationPose CoverPose)
{
	if (MovementState != ESquadMovementAnimState::Idle || GetIsTeamWeaponCrewAnimationActive())
	{
		return false;
	}

	CancelCoverAnimation();

	const bool bIsSupportedCoverPose = CoverPose == ESquadIdleAnimationPose::CrouchCover ||
		CoverPose == ESquadIdleAnimationPose::StandingCoverLeft ||
		CoverPose == ESquadIdleAnimationPose::StandingCoverRight ||
		CoverPose == ESquadIdleAnimationPose::TrenchCover;
	if (not bIsSupportedCoverPose)
	{
		RTSFunctionLibrary::ReportError(
			"USquadUnitAnimInstance::EnterCover received an unsupported idle animation pose.");
		return false;
	}
	UAnimMontage* EnterMontage = GetCoverEnterMontage(CoverPose);
	// A trench is entered in its protected pose, which is a crouch.
	const bool bEntersCrouched = CoverPose == ESquadIdleAnimationPose::CrouchCover || GetIsTrenchPose(CoverPose);
	AimPositionMontages.AimPosition = bEntersCrouched ? ESquadAimPosition::Crouch : ESquadAimPosition::Standing;

	M_CoverAnimRuntime.M_IdlePose = CoverPose;
	SetIdleAnimationPose(CoverPose);
	const uint32 PoseReportBit = 1u <<
		(SquadUnitCoverAnimLogStatics::FirstCoverPoseReportBit + static_cast<uint32>(CoverPose));
	if (SquadUnitCoverAnimLogStatics::TryMarkMissingCoverAssetReported(*this, PoseReportBit))
	{
		LogMissingCoverAnimationAssets(CoverPose);
		LogCoverMontageRootMotion(CoverPose);
	}
	// An in-place clip shows its root offset from frame one; blending in would slide the mesh away and back.
	const bool bStartOnFirstFrame = not GetDoesCoverEnterMontageMoveCapsule(CoverPose);
	if (PlayCoverMontage(EnterMontage, ESquadCoverAnimAction::Entering, bStartOnFirstFrame))
	{
		return true;
	}

	SetCoverAnimAction(ESquadCoverAnimAction::Protected);
	return false;
}

bool USquadUnitAnimInstance::StartStandingCoverPeek()
{
	if (M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::Protected)
	{
		return false;
	}

	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
	if (StandingAnimationSet == nullptr)
	{
		return false;
	}

	M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::None;
	if (PlayCoverMontage(
		StandingAnimationSet->ExposeFromCoverMontage,
		ESquadCoverAnimAction::Exposing))
	{
		return true;
	}
	EnterExposedCoverPose();
	return false;
}

bool USquadUnitAnimInstance::ReturnToStandingCover()
{
	if (M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::Exposed)
	{
		return false;
	}

	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
	if (StandingAnimationSet == nullptr)
	{
		return false;
	}

	M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::None;
	if (PlayCoverMontage(
		StandingAnimationSet->ReturnToCoverMontage,
		ESquadCoverAnimAction::Returning))
	{
		return true;
	}
	EnterProtectedCoverPose();
	return false;
}

bool USquadUnitAnimInstance::ExitCover()
{
	if (M_CoverAnimRuntime.M_Action == ESquadCoverAnimAction::Exposed)
	{
		const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
		if (StandingAnimationSet == nullptr)
		{
			return false;
		}

		M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::Exiting;
		if (PlayCoverMontage(
			StandingAnimationSet->ReturnToCoverMontage,
			ESquadCoverAnimAction::Returning))
		{
			return true;
		}

		M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::None;
		AnimNotify_Cover_BackInCover();
		if (not PlayExitCoverMontage())
		{
			ClearCoverAnimationRuntime();
		}
		return false;
	}

	if (M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::Protected)
	{
		return false;
	}

	return PlayExitCoverMontage();
}

void USquadUnitAnimInstance::CancelCoverAnimation()
{
	if (not GetIsCoverAnimationActive())
	{
		return;
	}

	UAnimMontage* ActiveCoverMontage = M_CoverAnimRuntime.M_ActiveMontage;
	M_CoverMontageEndedDelegate.Unbind();
	ClearCoverAnimationRuntime();

	if (IsValid(ActiveCoverMontage) && Montage_IsPlaying(ActiveCoverMontage))
	{
		Montage_Stop(0.1f, ActiveCoverMontage);
	}
}

bool USquadUnitAnimInstance::GetIsCoverAnimationActive() const
{
	return M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::None;
}

bool USquadUnitAnimInstance::GetIsCoverFireAllowed() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::CrouchCover)
	{
		return M_CoverAnimRuntime.M_Action == ESquadCoverAnimAction::Protected;
	}

	return M_CoverAnimRuntime.M_Action == ESquadCoverAnimAction::Exposed;
}

UAnimMontage* USquadUnitAnimInstance::GetCoverEnterMontage(const ESquadIdleAnimationPose CoverPose) const
{
	if (CoverPose == ESquadIdleAnimationPose::CrouchCover)
	{
		return CoverAnimations.Crouch.EnterCoverMontage;
	}
	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = FindStandingCoverAnimationSet(CoverPose);
	return StandingAnimationSet == nullptr ? nullptr : StandingAnimationSet->EnterCoverMontage;
}

FSquadUnitCoverLocalOffset USquadUnitAnimInstance::GetCoverEnterStartOffset(
	const ESquadIdleAnimationPose CoverPose) const
{
	// Without an enter montage there is no root travel to compensate, so the unit stops on the point itself.
	if (not IsValid(GetCoverEnterMontage(CoverPose)))
	{
		return FSquadUnitCoverLocalOffset();
	}
	if (CoverPose == ESquadIdleAnimationPose::CrouchCover)
	{
		return CoverAnimations.Crouch.EnterStartOffset;
	}
	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = FindStandingCoverAnimationSet(CoverPose);
	return StandingAnimationSet == nullptr ? FSquadUnitCoverLocalOffset() : StandingAnimationSet->EnterStartOffset;
}

bool USquadUnitAnimInstance::GetDoesCoverEnterMontageMoveCapsule(const ESquadIdleAnimationPose CoverPose) const
{
	const UAnimMontage* EnterMontage = GetCoverEnterMontage(CoverPose);
	return IsValid(EnterMontage) && EnterMontage->HasRootMotion();
}

bool USquadUnitAnimInstance::TryGetStandingCoverExposedOffset(
	const ESquadIdleAnimationPose CoverPose,
	FSquadUnitCoverLocalOffset& OutExposedOffset) const
{
	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = FindStandingCoverAnimationSet(CoverPose);
	if (StandingAnimationSet == nullptr || not IsValid(StandingAnimationSet->ExposeFromCoverMontage))
	{
		return false;
	}
	OutExposedOffset = StandingAnimationSet->ExposedOffset;
	return true;
}

FString USquadUnitAnimInstance::GetPoseDebugString() const
{
	const UAnimMontage* ActiveMontage = GetCurrentActiveMontage();
	return FString::Printf(
		TEXT("aiming=%d aim_position=%s movement=%s idle_pose=%s graph_pose=%s cover_action=%s montage=%s montage_pos=%.2f cover_ao=%s cover_ao_base=%s cover_idle=%s regular_ao=%s regular_ao_base=%s angle=%.1f"),
		bAimToTarget ? 1 : 0,
		*UEnum::GetValueAsString(AimPositionMontages.AimPosition),
		*UEnum::GetValueAsString(MovementState),
		*UEnum::GetValueAsString(IdleAnimationPose),
		*UEnum::GetValueAsString(CoverGraphPose),
		*UEnum::GetValueAsString(M_CoverAnimRuntime.M_Action),
		*GetNameSafe(ActiveMontage),
		IsValid(ActiveMontage) ? Montage_GetPosition(ActiveMontage) : -1.0f,
		*GetNameSafe(GetCurrentCoverAimOffset()),
		*GetNameSafe(GetCurrentCoverAimBaseSequence()),
		*GetNameSafe(GetCurrentCoverIdlePose()),
		*GetNameSafe(AimOffsets.M_ActiveAimOffset),
		*GetNameSafe(AimOffsets.M_ActiveAimOffsetSequence),
		AimOffsetAngle);
}

bool USquadUnitAnimInstance::GetIsCoverTransitionMontageActive() const
{
	return M_CoverAnimRuntime.M_ActiveMontageAction != ESquadCoverAnimAction::None;
}

void USquadUnitAnimInstance::ForceCompleteCoverTransition()
{
	if (not GetIsCoverTransitionMontageActive())
	{
		return;
	}
	const ESquadCoverAnimAction CompletedAction = M_CoverAnimRuntime.M_ActiveMontageAction;
	const ESquadCoverAnimAction NextAction = M_CoverAnimRuntime.M_NextAction;
	UAnimMontage* StalledMontage = M_CoverAnimRuntime.M_ActiveMontage;
	// Unbound first so stopping the montage is not treated as an interruption that leaves cover.
	M_CoverMontageEndedDelegate.Unbind();
	M_CoverAnimRuntime.M_ActiveMontage = nullptr;
	M_CoverAnimRuntime.M_ActiveMontageAction = ESquadCoverAnimAction::None;
	M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::None;
	M_CoverAnimRuntime.M_ActiveMontageSeconds = 0.0f;
	if (IsValid(StalledMontage) && Montage_IsPlaying(StalledMontage))
	{
		constexpr float StalledMontageBlendOutSeconds = 0.1f;
		Montage_Stop(StalledMontageBlendOutSeconds, StalledMontage);
	}
	CompleteCoverTransition(CompletedAction, NextAction);
}

UAnimSequence* USquadUnitAnimInstance::ResolveCoverIdlePose() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::CrouchCover)
	{
		return CoverAnimations.Crouch.ProtectedIdlePose;
	}

	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
	if (StandingAnimationSet == nullptr)
	{
		return nullptr;
	}
	// A trench without its own crouch clip uses the one of crouch cover.
	const bool bUseCrouchFallback = GetIsTrenchPose(M_CoverAnimRuntime.M_IdlePose) &&
		not IsValid(StandingAnimationSet->ProtectedIdlePose);
	return bUseCrouchFallback ? CoverAnimations.Crouch.ProtectedIdlePose : StandingAnimationSet->ProtectedIdlePose;
}

UAimOffsetBlendSpace* USquadUnitAnimInstance::ResolveCoverAimOffset() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::CrouchCover)
	{
		return CoverAnimations.Crouch.AimAssets.AimOffset;
	}

	if (not GetIsPeekPose(M_CoverAnimRuntime.M_IdlePose))
	{
		return nullptr;
	}

	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
	return StandingAnimationSet == nullptr ? nullptr : StandingAnimationSet->PeekAimAssets.AimOffset;
}

UAnimSequence* USquadUnitAnimInstance::ResolveCoverAimBaseSequence() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::CrouchCover)
	{
		return GetUsableCoverAimBaseSequence(CoverAnimations.Crouch.AimAssets.BaseSequence);
	}

	if (not GetIsPeekPose(M_CoverAnimRuntime.M_IdlePose))
	{
		return nullptr;
	}

	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
	return StandingAnimationSet == nullptr
		? nullptr
		: GetUsableCoverAimBaseSequence(StandingAnimationSet->PeekAimAssets.BaseSequence);
}

UAnimSequence* USquadUnitAnimInstance::GetUsableCoverAimBaseSequence(UAnimSequence* ConfiguredBaseSequence) const
{
	if (not IsValid(ConfiguredBaseSequence) || not ConfiguredBaseSequence->IsValidAdditive())
	{
		return ConfiguredBaseSequence;
	}
	// The pose the additive clip was authored against is what the designer meant to put here.
	UAnimSequence* AdditiveBasePose = ConfiguredBaseSequence->RefPoseSeq;
	const bool bHasUsableBasePose = IsValid(AdditiveBasePose) && not AdditiveBasePose->IsValidAdditive();
	if (SquadUnitCoverAnimLogStatics::TryMarkMissingCoverAssetReported(
		*this,
		SquadUnitCoverAnimLogStatics::AdditiveAimBaseReportBit))
	{
		RTSFunctionLibrary::ReportError(
			"Cover aim BaseSequence " + ConfiguredBaseSequence->GetName() + " on " + GetClass()->GetName() +
			" is an additive aim offset sample, not a pose. Played as a pose it collapses the mesh. Assign " +
			(bHasUsableBasePose ? AdditiveBasePose->GetName() : FString("a non-additive pose")) +
			" in CoverAnimations instead; " +
			(bHasUsableBasePose ? "it is used automatically for now." : "the unit keeps its cover idle pose for now."));
	}
	return bHasUsableBasePose ? AdditiveBasePose : nullptr;
}


void USquadUnitAnimInstance::UnitDies()
{
	// Death montages own the full-body slot, so clear any montage and crew bookkeeping before selection.
	StopAllMontages();

	USkeletalMeshComponent* SkeletalMeshComponent = GetSkelMeshComponent();
	if (not IsValid(SkeletalMeshComponent))
	{
		RTSFunctionLibrary::ReportError("Could not find the skel mesh component in USquadUnitAnimInstance::UnitDies");
		return;
	}

	AActor* Owner = SkeletalMeshComponent->GetOwner();
	if (not IsValid(Owner))
	{
		RTSFunctionLibrary::ReportError("Could not find the owner in USquadUnitAnimInstance::UnitDies");
		return;
	}

	USelectionComponent* SelectionComponent = Owner->FindComponentByClass<USelectionComponent>();
	if (not IsValid(SelectionComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Could not find the selection component in USquadUnitAnimInstance::UnitDies");
		return;
	}

	SelectionComponent->OnUnitSelected.RemoveAll(this);
	SelectionComponent->OnUnitDeselected.RemoveAll(this);
}

bool USquadUnitAnimInstance::PlayDeathMontage(
	const bool bUseCrouchedDeathMontage,
	const FOnSquadUnitDeathMontageFinished& CompletionDelegate,
	float& OutExpectedDuration)
{
	OutExpectedDuration = 0.0f;
	UAnimMontage* DeathMontage = DeathMontages.GetRandomDeathMontage(bUseCrouchedDeathMontage);
	if (not IsValid(DeathMontage))
	{
		const FString StanceName = bUseCrouchedDeathMontage ? TEXT("crouched") : TEXT("standing");
		RTSFunctionLibrary::ReportError(
			"No valid " + StanceName + " death montage is configured on " + GetName());
		return false;
	}

	const float DeathMontageLength = DeathMontage->GetPlayLength();
	if (not FMath::IsFinite(DeathMontageLength) || DeathMontageLength <= KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid death montage duration for " + DeathMontage->GetName() + " on " + GetName());
		return false;
	}

	const float PlayedDuration = Montage_Play(
		DeathMontage,
		1.0f,
		EMontagePlayReturnType::Duration);
	if (not FMath::IsFinite(PlayedDuration) || PlayedDuration <= KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(
			"Failed to play death montage " + DeathMontage->GetName() + " on " + GetName());
		return false;
	}

	M_DeathMontageCompletionDelegate = CompletionDelegate;
	M_DeathMontageEndedDelegate.Unbind();
	M_DeathMontageEndedDelegate.BindUObject(this, &USquadUnitAnimInstance::OnDeathMontageEnded);
	Montage_SetEndDelegate(M_DeathMontageEndedDelegate, DeathMontage);
	OutExpectedDuration = PlayedDuration;
	return true;
}

bool USquadUnitAnimInstance::GetShouldUseCrouchedDeathMontage(
	const bool bIsTeamWeaponOperator,
	const ESquadSubtype TeamWeaponSquadSubtype) const
{
	if (bIsTeamWeaponOperator)
	{
		return DeathMontages.TeamWeaponSubtypesUsingCrouchedDeathMontages.Contains(TeamWeaponSquadSubtype);
	}

	return AimPositionMontages.AimPosition != ESquadAimPosition::Standing;
}

void USquadUnitAnimInstance::OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	static_cast<void>(Montage);
	static_cast<void>(bInterrupted);

	FOnSquadUnitDeathMontageFinished CompletionDelegate = M_DeathMontageCompletionDelegate;
	M_DeathMontageCompletionDelegate.Unbind();
	M_DeathMontageEndedDelegate.Unbind();
	CompletionDelegate.ExecuteIfBound();
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
	// Montage_Play stops every other montage in the group, which would cut a cover transition short and leave
	// the unit half way into its pose. The full-body cover clip hides a reload or shot anyway.
	if (bIsWeaponMontage && GetIsCoverTransitionMontageActive())
	{
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
	// Cover transitions may contain root motion. The cover owner explicitly exits or cancels before locomotion resumes.
	if (GetIsCoverAnimationActive())
	{
		MovementState = ESquadMovementAnimState::Idle;
		return;
	}

	// Backstop for movement that bypasses the team weapon state machine (retreat, cargo, evasion): a moving
	// operator must never keep looping a full body crew montage. The controller re-arms once it is settled again.
	if (GetIsTeamWeaponCrewAnimationActive() &&
		MovementSpeed > SquadUnitTeamWeaponCrewAnimStatics::CrewAnimationMaxSpeedCmPerSec)
	{
		if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
		{
			SquadUnitTeamWeaponCrewAnimStatics::PrintCrewAnimDebug(
				GetOwningActor(),
				"Crew anim stopped: operator started moving.");
		}
		ClearTeamWeaponCrewAnimationRuntime(true);
	}

	// Aim position transitions share the FullBody slot with the crew montages and must not interrupt them.
	const bool bAllowAimPositionTransitions = not GetIsTeamWeaponCrewAnimationActive();
	if (FMath::IsNearlyZero(MovementSpeed))
	{
		// In this case we just started aiming while idle and can choose a random aim position.
		if (bAllowAimPositionTransitions && bAimToTarget && MovementState != ESquadMovementAnimState::Idle &&
			AimPositionMontages.AimPosition == ESquadAimPosition::Standing)
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
	if (bAllowAimPositionTransitions && MovementState != ESquadMovementAnimState::Walking)
	{
		OnStartWalking();
	}
	MovementState = ESquadMovementAnimState::Walking;
}

void USquadUnitAnimInstance::OnStartAimingWhileIdle()
{
	if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
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
	if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("OnStartWalking", FColor::Purple);
	}
	UAnimMontage* TransitionMontage = nullptr;
	switch (AimPositionMontages.AimPosition)
	{
	case ESquadAimPosition::Standing:
		if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Standing; NO CHANGE", FColor::Purple);
		}
		break;
	case ESquadAimPosition::Prone:
	case ESquadAimPosition::Crouch:
		if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
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

void USquadUnitAnimInstance::AnimNotify_Cover_AimReady()
{
	if (M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::Exposing)
	{
		return;
	}
	EnterExposedCoverPose();
}

void USquadUnitAnimInstance::AnimNotify_Cover_BackInCover()
{
	if (M_CoverAnimRuntime.M_Action != ESquadCoverAnimAction::Returning)
	{
		return;
	}
	EnterProtectedCoverPose();
}

void USquadUnitAnimInstance::EnterExposedCoverPose()
{
	const ESquadIdleAnimationPose PeekPose = GetStandingPeekPose();
	if (PeekPose == ESquadIdleAnimationPose::Regular)
	{
		return;
	}
	M_CoverAnimRuntime.M_IdlePose = PeekPose;
	if (GetIsTrenchPose(PeekPose))
	{
		// Stood up: weapon montages must be the standing ones from here on.
		AimPositionMontages.AimPosition = ESquadAimPosition::Standing;
	}
	SetIdleAnimationPose(PeekPose);
	SetCoverAnimAction(ESquadCoverAnimAction::Exposed);
}

void USquadUnitAnimInstance::EnterProtectedCoverPose()
{
	const ESquadIdleAnimationPose ProtectedPose = GetProtectedStandingCoverPose();
	if (ProtectedPose == ESquadIdleAnimationPose::Regular)
	{
		return;
	}
	M_CoverAnimRuntime.M_IdlePose = ProtectedPose;
	if (GetIsTrenchPose(ProtectedPose))
	{
		AimPositionMontages.AimPosition = ESquadAimPosition::Crouch;
	}
	SetIdleAnimationPose(ProtectedPose);
	SetCoverAnimAction(ESquadCoverAnimAction::Protected);
}

void USquadUnitAnimInstance::SetIdleAnimationPose(const ESquadIdleAnimationPose NewIdlePose)
{
	IdleAnimationPose = NewIdlePose;
	RefreshCoverGraphPose();
}

void USquadUnitAnimInstance::RefreshCoverGraphPose()
{
	// Only ever replaced by another valid asset; see FSquadUnitCoverGraphAssets for why they are never cleared.
	if (UAnimSequence* CoverIdlePose = ResolveCoverIdlePose())
	{
		M_CoverGraphAssets.IdlePose = CoverIdlePose;
	}
	if (UAimOffsetBlendSpace* CoverAimOffset = ResolveCoverAimOffset())
	{
		M_CoverGraphAssets.AimOffset = CoverAimOffset;
	}
	if (UAnimSequence* CoverAimBaseSequence = ResolveCoverAimBaseSequence())
	{
		M_CoverGraphAssets.AimBaseSequence = CoverAimBaseSequence;
	}

	switch (IdleAnimationPose)
	{
	case ESquadIdleAnimationPose::StandingCoverLeft:
	case ESquadIdleAnimationPose::StandingCoverRight:
	case ESquadIdleAnimationPose::TrenchCover:
		CoverGraphPose = ESquadCoverGraphPose::CoverIdle;
		break;
	case ESquadIdleAnimationPose::TrenchPeek:
		// Without a trench aim offset the soldier simply stands up into the regular aim.
		CoverGraphPose = IsValid(CoverAnimations.Trench.PeekAimAssets.AimOffset)
			? ESquadCoverGraphPose::CoverAim
			: ESquadCoverGraphPose::NotInCover;
		break;
	case ESquadIdleAnimationPose::CrouchCover:
		// Crouch cover needs no expose transition, so it ducks whenever the weapon has nothing to aim at.
		// Without usable aim assets it stays in the idle crouch instead of showing a broken aim pose.
		CoverGraphPose = bAimToTarget && IsValid(ResolveCoverAimOffset()) && IsValid(ResolveCoverAimBaseSequence())
			? ESquadCoverGraphPose::CoverAim
			: ESquadCoverGraphPose::CoverIdle;
		break;
	case ESquadIdleAnimationPose::StandingPeekLeft:
	case ESquadIdleAnimationPose::StandingPeekRight:
		CoverGraphPose = ESquadCoverGraphPose::CoverAim;
		break;
	case ESquadIdleAnimationPose::Regular:
	default:
		CoverGraphPose = ESquadCoverGraphPose::NotInCover;
		break;
	}
}

void USquadUnitAnimInstance::SetCoverAnimAction(const ESquadCoverAnimAction NewAction)
{
	if (M_CoverAnimRuntime.M_Action == NewAction)
	{
		return;
	}
	M_CoverAnimRuntime.M_Action = NewAction;
	OnCoverAnimActionChanged.ExecuteIfBound(NewAction);
}

bool USquadUnitAnimInstance::PlayCoverMontage(
	UAnimMontage* Montage,
	const ESquadCoverAnimAction MontageAction,
	const bool bStartOnFirstFrame)
{
	if (not IsValid(Montage))
	{
		const uint32 MontageActionBit = 1u << static_cast<uint32>(MontageAction);
		if (not SquadUnitCoverAnimLogStatics::TryMarkMissingCoverAssetReported(*this, MontageActionBit))
		{
			return false;
		}
		UE_LOG(
			LogRTSSquadUnitCoverAnimation,
			Warning,
			TEXT("Cover action %s has no montage configured on %s; gameplay state will continue immediately."),
			*UEnum::GetValueAsString(MontageAction),
			*GetClass()->GetName());
		return false;
	}

	M_CoverMontageEndedDelegate.Unbind();
	constexpr float CoverMontagePlayRate = 1.0f;
	const float PlayedDuration = bStartOnFirstFrame
		? Montage_PlayWithBlendIn(
			Montage,
			FAlphaBlendArgs(0.0f),
			CoverMontagePlayRate,
			EMontagePlayReturnType::Duration)
		: Montage_Play(Montage, CoverMontagePlayRate, EMontagePlayReturnType::Duration);
	if (not FMath::IsFinite(PlayedDuration) || PlayedDuration <= KINDA_SMALL_NUMBER)
	{
		// A montage the engine refuses (for example two slot tracks with the same name) fails on every use;
		// one report per animation class is enough, and the caller continues as if no montage were configured.
		const uint32 UnplayableMontageBit = 1u <<
			(SquadUnitCoverAnimLogStatics::FirstUnplayableMontageReportBit + static_cast<uint32>(MontageAction));
		if (SquadUnitCoverAnimLogStatics::TryMarkMissingCoverAssetReported(*this, UnplayableMontageBit))
		{
			RTSFunctionLibrary::ReportError(
				"Failed to play cover montage " + Montage->GetName() + " on " + GetClass()->GetName() +
				". Check the output log for the engine's LogAnimMontage warning about this asset.");
		}
		return false;
	}

	M_CoverAnimRuntime.M_ActiveMontageAction = MontageAction;
	M_CoverAnimRuntime.M_ActiveMontage = Montage;
	M_CoverAnimRuntime.M_ActiveMontageSeconds = PlayedDuration;
	M_CoverMontageEndedDelegate.BindUObject(this, &USquadUnitAnimInstance::OnCoverMontageEnded);
	Montage_SetEndDelegate(M_CoverMontageEndedDelegate, Montage);
	SetCoverAnimAction(MontageAction);
	return true;
}

bool USquadUnitAnimInstance::PlayExitCoverMontage()
{
	UAnimMontage* ExitMontage = nullptr;
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::CrouchCover)
	{
		ExitMontage = CoverAnimations.Crouch.ExitCoverMontage;
	}
	else
	{
		const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
		if (StandingAnimationSet == nullptr)
		{
			return false;
		}
		ExitMontage = StandingAnimationSet->ExitCoverMontage;
	}

	M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::None;
	if (PlayCoverMontage(ExitMontage, ESquadCoverAnimAction::Exiting))
	{
		return true;
	}
	ClearCoverAnimationRuntime();
	return false;
}

void USquadUnitAnimInstance::OnCoverMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != M_CoverAnimRuntime.M_ActiveMontage)
	{
		return;
	}

	const ESquadCoverAnimAction CompletedAction = M_CoverAnimRuntime.M_ActiveMontageAction;
	const ESquadCoverAnimAction NextAction = M_CoverAnimRuntime.M_NextAction;
	M_CoverMontageEndedDelegate.Unbind();
	M_CoverAnimRuntime.M_ActiveMontage = nullptr;
	M_CoverAnimRuntime.M_ActiveMontageAction = ESquadCoverAnimAction::None;
	M_CoverAnimRuntime.M_NextAction = ESquadCoverAnimAction::None;
	M_CoverAnimRuntime.M_ActiveMontageSeconds = 0.0f;

	// An interruption still ends the transition; the pose it was heading for is reached without the rest of the
	// clip, and the owning unit lines the capsule up. Leaving cover goes through CancelCoverAnimation, which
	// unbinds this delegate first.
	CompleteCoverTransition(CompletedAction, NextAction);
}

void USquadUnitAnimInstance::CompleteCoverTransition(
	const ESquadCoverAnimAction CompletedAction,
	const ESquadCoverAnimAction NextAction)
{
	if (CompletedAction == ESquadCoverAnimAction::Entering)
	{
		SetCoverAnimAction(ESquadCoverAnimAction::Protected);
		TryPlayPendingCoverReload();
		return;
	}

	// The Cover_AimReady and Cover_BackInCover notifies only move these switches earlier inside the clip;
	// a clip without them simply reaches the same pose when it ends.
	if (CompletedAction == ESquadCoverAnimAction::Exposing)
	{
		if (M_CoverAnimRuntime.M_Action == ESquadCoverAnimAction::Exposing)
		{
			EnterExposedCoverPose();
		}
		return;
	}

	if (CompletedAction == ESquadCoverAnimAction::Returning)
	{
		AnimNotify_Cover_BackInCover();
		if (NextAction == ESquadCoverAnimAction::Exiting && not PlayExitCoverMontage())
		{
			ClearCoverAnimationRuntime();
			return;
		}
		if (NextAction != ESquadCoverAnimAction::Exiting)
		{
			// Back behind the cover: now the reload that asked for the duck can be shown.
			TryPlayPendingCoverReload();
		}
		return;
	}

	if (CompletedAction == ESquadCoverAnimAction::Exiting)
	{
		ClearCoverAnimationRuntime();
	}
}

void USquadUnitAnimInstance::ClearCoverAnimationRuntime()
{
	const bool bHadCoverPose = M_CoverAnimRuntime.M_IdlePose != ESquadIdleAnimationPose::Regular ||
		IdleAnimationPose != ESquadIdleAnimationPose::Regular;
	StopCoverReloadMontage();
	M_CoverMontageEndedDelegate.Unbind();
	M_CoverAnimRuntime.Reset();
	SetIdleAnimationPose(ESquadIdleAnimationPose::Regular);

	if (not bHadCoverPose)
	{
		return;
	}

	AimPositionMontages.AimPosition = ESquadAimPosition::Standing;
	AimOffsets.UpdateAOForNewAimPosition(ESquadAimPosition::Standing);
}

void USquadUnitAnimInstance::LogMissingCoverAnimationAssets(
	const ESquadIdleAnimationPose CoverPose) const
{
	const bool bIsCrouchCover = CoverPose == ESquadIdleAnimationPose::CrouchCover;
	const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
	const UAnimSequence* ProtectedIdlePose = bIsCrouchCover
		? CoverAnimations.Crouch.ProtectedIdlePose
		: StandingAnimationSet == nullptr ? nullptr : StandingAnimationSet->ProtectedIdlePose;
	const FSquadUnitCoverAimAssets* AimAssets = bIsCrouchCover
		? &CoverAnimations.Crouch.AimAssets
		: StandingAnimationSet == nullptr ? nullptr : &StandingAnimationSet->PeekAimAssets;

	if (not IsValid(ProtectedIdlePose) && not IsValid(ResolveCoverIdlePose()))
	{
		UE_LOG(
			LogRTSSquadUnitCoverAnimation,
			Warning,
			TEXT("Cover pose %s has no protected idle animation configured on %s."),
			*UEnum::GetValueAsString(CoverPose),
			*GetClass()->GetName());
	}
	// A unit behind cover loops this clip for as long as it hides.
	constexpr float MaximumIdlePoseRootTravel = 10.0f;
	const float IdlePoseRootTravel = IsValid(ProtectedIdlePose)
		? SquadUnitCoverAnimLogStatics::GetLargestRootTravel(*ProtectedIdlePose)
		: 0.0f;
	if (IdlePoseRootTravel > MaximumIdlePoseRootTravel)
	{
		RTSFunctionLibrary::ReportError(
			"Cover ProtectedIdlePose " + ProtectedIdlePose->GetName() + " for " +
			UEnum::GetValueAsString(CoverPose) + " on " + GetClass()->GetName() + " moves its root by " +
			FString::SanitizeFloat(FMath::RoundToFloat(IdlePoseRootTravel)) +
			" cm. It is a transition clip, not a pose: the unit will step in and out of its cover in a loop. "
			"Assign a clip that stays in place.");
	}
	// The trench set's assets are optional: it falls back to the crouch clip and the regular standing aim.
	if (GetIsTrenchPose(CoverPose))
	{
		return;
	}
	if (AimAssets == nullptr || not IsValid(AimAssets->AimOffset))
	{
		UE_LOG(
			LogRTSSquadUnitCoverAnimation,
			Warning,
			TEXT("Cover pose %s has no aim offset configured on %s."),
			*UEnum::GetValueAsString(CoverPose),
			*GetClass()->GetName());
	}
	if (AimAssets == nullptr || not IsValid(AimAssets->BaseSequence))
	{
		UE_LOG(
			LogRTSSquadUnitCoverAnimation,
			Warning,
			TEXT("Cover pose %s has no aim base sequence configured on %s."),
			*UEnum::GetValueAsString(CoverPose),
			*GetClass()->GetName());
	}
}

void USquadUnitAnimInstance::LogCoverAimAssetSetup() const
{
	using SquadUnitCoverAnimLogStatics::LogAimAssetSetup;
	LogAimAssetSetup(
		TEXT("cover_crouch"),
		CoverAnimations.Crouch.AimAssets.AimOffset,
		CoverAnimations.Crouch.AimAssets.BaseSequence);
	LogAimAssetSetup(
		TEXT("cover_standing_left"),
		CoverAnimations.StandingLeft.PeekAimAssets.AimOffset,
		CoverAnimations.StandingLeft.PeekAimAssets.BaseSequence);
	LogAimAssetSetup(
		TEXT("cover_standing_right"),
		CoverAnimations.StandingRight.PeekAimAssets.AimOffset,
		CoverAnimations.StandingRight.PeekAimAssets.BaseSequence);
	LogAimAssetSetup(TEXT("regular_rifle"), AimOffsets.RifleAimOffset, AimOffsets.RifleAimOffsetSequence);
	LogAimAssetSetup(
		TEXT("regular_rifle_crouch"),
		AimOffsets.RifleCrouchAimOffset,
		AimOffsets.RifleCrouchAimOffsetSequence);
	LogAimAssetSetup(TEXT("regular_hip_crouch"), AimOffsets.HipCrouchAimOffset, AimOffsets.HipCrouchAimOffsetSequence);
	UE_LOG(
		LogRTSSquadUnitCoverAnimation,
		Verbose,
		TEXT("RTS_COVER_AIM_ASSETS role=cover_idle crouch=%s standing_left=%s standing_right=%s"),
		*SquadUnitCoverAnimLogStatics::DescribeSequenceAdditiveSetup(CoverAnimations.Crouch.ProtectedIdlePose),
		*SquadUnitCoverAnimLogStatics::DescribeSequenceAdditiveSetup(CoverAnimations.StandingLeft.ProtectedIdlePose),
		*SquadUnitCoverAnimLogStatics::DescribeSequenceAdditiveSetup(CoverAnimations.StandingRight.ProtectedIdlePose));
}

void USquadUnitAnimInstance::LogCoverMontageRootMotion(const ESquadIdleAnimationPose CoverPose) const
{
	if constexpr (DeveloperSettings::Debugging::GCoverFinder_Compile_DebugSymbols)
	{
		const bool bLogAimAssetSetup = SquadUnitCoverAnimLogStatics::TryMarkMissingCoverAssetReported(
			*this,
			SquadUnitCoverAnimLogStatics::AimAssetSetupReportBit);
		if (bLogAimAssetSetup)
		{
			LogCoverAimAssetSetup();
		}
		if (CoverPose == ESquadIdleAnimationPose::CrouchCover)
		{
			LogCoverMontageRootMotion(CoverAnimations.Crouch.EnterCoverMontage, TEXT("Crouch enter"));
			LogCoverMontageRootMotion(CoverAnimations.Crouch.ExitCoverMontage, TEXT("Crouch exit"));
			return;
		}
		const FSquadUnitStandingCoverAnimationSet* StandingAnimationSet = GetStandingCoverAnimationSet();
		if (StandingAnimationSet == nullptr)
		{
			return;
		}
		LogCoverMontageRootMotion(StandingAnimationSet->EnterCoverMontage, TEXT("Standing enter"));
		LogCoverMontageRootMotion(StandingAnimationSet->ExposeFromCoverMontage, TEXT("Standing expose"));
		LogCoverMontageRootMotion(StandingAnimationSet->ReturnToCoverMontage, TEXT("Standing return"));
		LogCoverMontageRootMotion(StandingAnimationSet->ExitCoverMontage, TEXT("Standing exit"));
	}
}

void USquadUnitAnimInstance::LogCoverMontageRootMotion(
	const UAnimMontage* Montage,
	const TCHAR* MontageRole) const
{
	const USkeletalMeshComponent* SkeletalMeshComponent = GetSkelMeshComponent();
	if (not IsValid(Montage) || not IsValid(SkeletalMeshComponent))
	{
		return;
	}
	// Root motion is authored in mesh space; the capsule moves by it after the mesh's relative rotation is applied.
	const FTransform MeshSpaceRootMotion = Montage->ExtractRootMotionFromTrackRange(0.0f, Montage->GetPlayLength());
	const FQuat MeshToActorRotation = SkeletalMeshComponent->GetRelativeRotation().Quaternion();
	const FVector ActorSpaceTranslation = MeshToActorRotation.RotateVector(MeshSpaceRootMotion.GetTranslation());
	UE_LOG(
		LogRTSSquadUnitCoverAnimation,
		Display,
		TEXT("RTS_COVER_ROOT_MOTION pose=%s role=%s montage=%s seconds=%.2f has_root_motion=%d forward_cm=%.1f right_cm=%.1f up_cm=%.1f yaw_deg=%.1f root_motion_mode=%s"),
		*UEnum::GetValueAsString(M_CoverAnimRuntime.M_IdlePose),
		MontageRole,
		*Montage->GetName(),
		Montage->GetPlayLength(),
		Montage->HasRootMotion() ? 1 : 0,
		ActorSpaceTranslation.X,
		ActorSpaceTranslation.Y,
		ActorSpaceTranslation.Z,
		MeshSpaceRootMotion.GetRotation().Rotator().Yaw,
		*UEnum::GetValueAsString(RootMotionMode.GetValue()));

	// Clips with root motion switched off still move the mesh by their root track; that travel is what an
	// EnterStartOffset has to compensate, so it is reported per sequence.
	const FSkeletonPoseBoneIndex RootBoneIndex(0);
	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
		{
			const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
			if (not IsValid(Sequence))
			{
				continue;
			}
			const bool bPlaysInReverse = Segment.AnimPlayRate < 0.0f;
			FTransform RootAtMontageStart;
			FTransform RootAtMontageEnd;
			Sequence->GetBoneTransform(
				RootAtMontageStart,
				RootBoneIndex,
				bPlaysInReverse ? Segment.AnimEndTime : Segment.AnimStartTime,
				true);
			Sequence->GetBoneTransform(
				RootAtMontageEnd,
				RootBoneIndex,
				bPlaysInReverse ? Segment.AnimStartTime : Segment.AnimEndTime,
				true);
			const FVector RootTrackTravel = MeshToActorRotation.RotateVector(
				RootAtMontageEnd.GetTranslation() - RootAtMontageStart.GetTranslation());
			UE_LOG(
				LogRTSSquadUnitCoverAnimation,
				Display,
				TEXT("RTS_COVER_ROOT_TRACK montage=%s slot=%s sequence=%s reversed=%d sequence_root_motion_enabled=%d root_track_forward_cm=%.1f root_track_right_cm=%.1f"),
				*Montage->GetName(),
				*SlotTrack.SlotName.ToString(),
				*Sequence->GetName(),
				bPlaysInReverse ? 1 : 0,
				Sequence->bEnableRootMotion ? 1 : 0,
				RootTrackTravel.X,
				RootTrackTravel.Y);
		}
	}
}

const FSquadUnitStandingCoverAnimationSet* USquadUnitAnimInstance::FindStandingCoverAnimationSet(
	const ESquadIdleAnimationPose CoverPose) const
{
	if (CoverPose == ESquadIdleAnimationPose::StandingCoverLeft ||
		CoverPose == ESquadIdleAnimationPose::StandingPeekLeft)
	{
		return &CoverAnimations.StandingLeft;
	}
	if (CoverPose == ESquadIdleAnimationPose::StandingCoverRight ||
		CoverPose == ESquadIdleAnimationPose::StandingPeekRight)
	{
		return &CoverAnimations.StandingRight;
	}
	return GetIsTrenchPose(CoverPose) ? &CoverAnimations.Trench : nullptr;
}

bool USquadUnitAnimInstance::GetIsPeekPose(const ESquadIdleAnimationPose CoverPose)
{
	return CoverPose == ESquadIdleAnimationPose::StandingPeekLeft ||
		CoverPose == ESquadIdleAnimationPose::StandingPeekRight ||
		CoverPose == ESquadIdleAnimationPose::TrenchPeek;
}

bool USquadUnitAnimInstance::GetIsTrenchPose(const ESquadIdleAnimationPose CoverPose)
{
	return CoverPose == ESquadIdleAnimationPose::TrenchCover || CoverPose == ESquadIdleAnimationPose::TrenchPeek;
}

const FSquadUnitStandingCoverAnimationSet* USquadUnitAnimInstance::GetStandingCoverAnimationSet() const
{
	return FindStandingCoverAnimationSet(M_CoverAnimRuntime.M_IdlePose);
}

ESquadIdleAnimationPose USquadUnitAnimInstance::GetProtectedStandingCoverPose() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingCoverLeft ||
		M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingPeekLeft)
	{
		return ESquadIdleAnimationPose::StandingCoverLeft;
	}

	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingCoverRight ||
		M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingPeekRight)
	{
		return ESquadIdleAnimationPose::StandingCoverRight;
	}

	return GetIsTrenchPose(M_CoverAnimRuntime.M_IdlePose)
		? ESquadIdleAnimationPose::TrenchCover
		: ESquadIdleAnimationPose::Regular;
}

ESquadIdleAnimationPose USquadUnitAnimInstance::GetStandingPeekPose() const
{
	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingCoverLeft ||
		M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingPeekLeft)
	{
		return ESquadIdleAnimationPose::StandingPeekLeft;
	}

	if (M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingCoverRight ||
		M_CoverAnimRuntime.M_IdlePose == ESquadIdleAnimationPose::StandingPeekRight)
	{
		return ESquadIdleAnimationPose::StandingPeekRight;
	}

	return GetIsTrenchPose(M_CoverAnimRuntime.M_IdlePose)
		? ESquadIdleAnimationPose::TrenchPeek
		: ESquadIdleAnimationPose::Regular;
}

// ----- Team Weapon Crew Animations -----

void FSquadUnitTeamWeaponCrewAnimRuntime::Reset()
{
	M_ActiveEntry = FTeamWeaponCrewMontageEntry();
	M_CrewRole = ECrewPositionType::None;
	M_TeamWeaponSquadSubtype = ESquadSubtype::Squad_None;
	bM_IsActive = false;
	bM_IsLoopPlaying = false;
	bM_IsReactPlaying = false;
	M_ActiveLoopMontage = nullptr;
}

void USquadUnitAnimInstance::StartTeamWeaponCrewAnimation(const ECrewPositionType CrewRole,
                                                          const ESquadSubtype TeamWeaponSquadSubtype)
{
	CancelCoverAnimation();

	if (M_TeamWeaponCrewAnimRuntime.GetIsSameAssignment(CrewRole, TeamWeaponSquadSubtype))
	{
		return;
	}
	if (M_TeamWeaponCrewAnimRuntime.bM_IsActive)
	{
		StopTeamWeaponCrewAnimation();
	}

	const FTeamWeaponCrewMontageEntry* ResolvedEntry = TeamWeaponCrewMontages.ResolveEntry(
		CrewRole, TeamWeaponSquadSubtype);
	if (ResolvedEntry == nullptr)
	{
		// ECrewPositionType::None: nothing to animate; stay inactive so the controller does not retry.
		return;
	}

	// Armed even when the entry has no montage: a missing montage is a valid designer choice (no error).
	M_TeamWeaponCrewAnimRuntime.M_ActiveEntry = *ResolvedEntry;
	M_TeamWeaponCrewAnimRuntime.M_CrewRole = CrewRole;
	M_TeamWeaponCrewAnimRuntime.M_TeamWeaponSquadSubtype = TeamWeaponSquadSubtype;
	M_TeamWeaponCrewAnimRuntime.bM_IsActive = true;

	// The authored full body pose replaces any crouch; reset so no stray crouch-to-stand transition plays later.
	AimPositionMontages.AimPosition = ESquadAimPosition::Standing;
	AimOffsets.UpdateAOForNewAimPosition(ESquadAimPosition::Standing);

	if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
	{
		SquadUnitTeamWeaponCrewAnimStatics::PrintCrewAnimDebug(
			GetOwningActor(),
			"Crew anim armed: " + UEnum::GetValueAsString(CrewRole) + " subtype: " +
			UEnum::GetValueAsString(TeamWeaponSquadSubtype) + " react: " +
			(ResolvedEntry->bReactToWeaponFire ? FString("true") : FString("false")));
	}

	if (not ResolvedEntry->bReactToWeaponFire)
	{
		if (ResolvedEntry->GetHasMontage())
		{
			PlayTeamWeaponCrewLoopMontage(ResolvedEntry->Montage, ResolvedEntry->PlayRate);
		}
		return;
	}

	if (ResolvedEntry->GetHasIdleLoopMontage())
	{
		PlayTeamWeaponCrewLoopMontage(ResolvedEntry->IdleLoopMontage, ResolvedEntry->IdleLoopPlayRate);
	}
}

void USquadUnitAnimInstance::StopTeamWeaponCrewAnimation()
{
	if (not M_TeamWeaponCrewAnimRuntime.bM_IsActive)
	{
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
	{
		SquadUnitTeamWeaponCrewAnimStatics::PrintCrewAnimDebug(
			GetOwningActor(),
			"Crew anim stopped: " + UEnum::GetValueAsString(M_TeamWeaponCrewAnimRuntime.M_CrewRole));
	}
	ClearTeamWeaponCrewAnimationRuntime(true);
}

void USquadUnitAnimInstance::OnTeamWeaponReloadStarted(const float ReloadTime)
{
	if (not M_TeamWeaponCrewAnimRuntime.bM_IsActive)
	{
		return;
	}
	const FTeamWeaponCrewMontageEntry& ActiveEntry = M_TeamWeaponCrewAnimRuntime.M_ActiveEntry;
	if (not ActiveEntry.bReactToWeaponFire || not ActiveEntry.GetHasMontage())
	{
		return;
	}

	const float ReactPlayRate = GetTeamWeaponCrewReactPlayRate(ActiveEntry.Montage, ReloadTime);
	if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
	{
		SquadUnitTeamWeaponCrewAnimStatics::PrintCrewAnimDebug(
			GetOwningActor(),
			"Crew react montage: " + ActiveEntry.Montage->GetName() +
			" reload: " + FString::SanitizeFloat(ReloadTime) +
			" length: " + FString::SanitizeFloat(ActiveEntry.Montage->GetPlayLength()) +
			" rate: " + FString::SanitizeFloat(ReactPlayRate));
	}

	// Set before Montage_Play: playing a montage in the same slot ends the idle loop with bInterrupted = true.
	M_TeamWeaponCrewAnimRuntime.bM_IsReactPlaying = true;
	M_TeamWeaponCrewAnimRuntime.bM_IsLoopPlaying = false;
	Montage_Play(ActiveEntry.Montage, ReactPlayRate);

	M_TeamWeaponCrewReactMontageEndedDelegate.Unbind();
	M_TeamWeaponCrewReactMontageEndedDelegate.BindUObject(
		this, &USquadUnitAnimInstance::OnTeamWeaponCrewReactMontageEnded);
	Montage_SetEndDelegate(M_TeamWeaponCrewReactMontageEndedDelegate, ActiveEntry.Montage);
}

void USquadUnitAnimInstance::PlayTeamWeaponCrewLoopMontage(UAnimMontage* LoopMontage, const float PlayRate)
{
	if (not IsValid(LoopMontage))
	{
		return;
	}
	M_TeamWeaponCrewAnimRuntime.M_ActiveLoopMontage = LoopMontage;
	M_TeamWeaponCrewAnimRuntime.bM_IsLoopPlaying = true;
	Montage_Play(LoopMontage, PlayRate);

	// Chain every section to the next (last back to first) on this montage instance so the asset loops by itself
	// with no gap and no asset edits. The end delegate below only fires when something interrupts the loop.
	const int32 SectionCount = LoopMontage->GetNumSections();
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const int32 NextSectionIndex = (SectionIndex + 1) % SectionCount;
		Montage_SetNextSection(
			LoopMontage->GetSectionName(SectionIndex),
			LoopMontage->GetSectionName(NextSectionIndex),
			LoopMontage);
	}

	M_TeamWeaponCrewLoopMontageEndedDelegate.Unbind();
	M_TeamWeaponCrewLoopMontageEndedDelegate.BindUObject(
		this, &USquadUnitAnimInstance::OnTeamWeaponCrewLoopMontageEnded);
	Montage_SetEndDelegate(M_TeamWeaponCrewLoopMontageEndedDelegate, LoopMontage);
}

void USquadUnitAnimInstance::OnTeamWeaponCrewLoopMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (not M_TeamWeaponCrewAnimRuntime.bM_IsActive || not M_TeamWeaponCrewAnimRuntime.bM_IsLoopPlaying)
	{
		return;
	}
	if (bInterrupted)
	{
		if (M_TeamWeaponCrewAnimRuntime.bM_IsReactPlaying)
		{
			// The reaction montage took over on purpose; it restarts the idle loop when it ends.
			return;
		}
		// Something else took the FullBody slot; drop the arming so the controller can re-arm when appropriate.
		if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
		{
			SquadUnitTeamWeaponCrewAnimStatics::PrintCrewAnimDebug(
				GetOwningActor(),
				"Crew loop interrupted externally; disarming.");
		}
		ClearTeamWeaponCrewAnimationRuntime(false);
		return;
	}

	// Natural end only happens when section chaining could not keep the montage alive; re-play as a fallback.
	const FTeamWeaponCrewMontageEntry& ActiveEntry = M_TeamWeaponCrewAnimRuntime.M_ActiveEntry;
	const bool bIsIdleLoop = ActiveEntry.bReactToWeaponFire;
	const float LoopPlayRate = bIsIdleLoop ? ActiveEntry.IdleLoopPlayRate : ActiveEntry.PlayRate;
	PlayTeamWeaponCrewLoopMontage(M_TeamWeaponCrewAnimRuntime.M_ActiveLoopMontage, LoopPlayRate);
}

void USquadUnitAnimInstance::OnTeamWeaponCrewReactMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (not M_TeamWeaponCrewAnimRuntime.bM_IsActive)
	{
		return;
	}
	M_TeamWeaponCrewAnimRuntime.bM_IsReactPlaying = false;
	if (bInterrupted)
	{
		// Either a newer reaction started (it manages its own end) or an external montage took the slot.
		return;
	}

	const FTeamWeaponCrewMontageEntry& ActiveEntry = M_TeamWeaponCrewAnimRuntime.M_ActiveEntry;
	if (not ActiveEntry.GetHasIdleLoopMontage())
	{
		return;
	}
	PlayTeamWeaponCrewLoopMontage(ActiveEntry.IdleLoopMontage, ActiveEntry.IdleLoopPlayRate);
}

void USquadUnitAnimInstance::ClearTeamWeaponCrewAnimationRuntime(const bool bStopPlayingMontages)
{
	M_TeamWeaponCrewLoopMontageEndedDelegate.Unbind();
	M_TeamWeaponCrewReactMontageEndedDelegate.Unbind();

	if (bStopPlayingMontages)
	{
		UAnimMontage* LoopMontage = M_TeamWeaponCrewAnimRuntime.M_ActiveLoopMontage;
		if (IsValid(LoopMontage) && Montage_IsPlaying(LoopMontage))
		{
			Montage_Stop(SquadUnitTeamWeaponCrewAnimStatics::CrewMontageBlendOutTime, LoopMontage);
		}
		UAnimMontage* ReactMontage = M_TeamWeaponCrewAnimRuntime.M_ActiveEntry.Montage;
		if (M_TeamWeaponCrewAnimRuntime.bM_IsReactPlaying && IsValid(ReactMontage) && Montage_IsPlaying(ReactMontage))
		{
			Montage_Stop(SquadUnitTeamWeaponCrewAnimStatics::CrewMontageBlendOutTime, ReactMontage);
		}
	}

	M_TeamWeaponCrewAnimRuntime.Reset();
}

float USquadUnitAnimInstance::GetTeamWeaponCrewReactPlayRate(const UAnimMontage* ReactMontage,
                                                              const float ReloadTime) const
{
	if (not IsValid(ReactMontage))
	{
		return 1.0f;
	}
	// Montage_Play's rate is multiplied by the asset RateScale while advancing; compensate so the sync stays exact.
	const float SafeReloadTime = FMath::Max(ReloadTime, SquadUnitTeamWeaponCrewAnimStatics::MinReloadTimeSeconds);
	const float SafeRateScale = FMath::Max(ReactMontage->RateScale, KINDA_SMALL_NUMBER);
	const float SyncedPlayRate = ReactMontage->GetPlayLength() / (SafeReloadTime * SafeRateScale);
	return SyncedPlayRate * M_TeamWeaponCrewAnimRuntime.M_ActiveEntry.PlayRate;
}
