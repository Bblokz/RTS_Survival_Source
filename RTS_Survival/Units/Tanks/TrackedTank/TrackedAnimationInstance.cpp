// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackedAnimationInstance.h"

#include "NiagaraComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UTrackedAnimationInstance::UTrackedAnimationInstance()
	: LeftTrackMaterial(nullptr),
	  RightTrackMaterial(nullptr),
	  M_SpeedChangeToStationaryTurn(0),
	  M_PlayRateSlowForward(0),
	  M_PlayRateFastForward(0),
	  M_PlayRateSlowBackward(0),
	  M_PlayRateFastBackward(0)
{
}

void UTrackedAnimationInstance::SetMovementParameters(
	const float NewSpeed,
	const float NewRotationYaw,
	const bool bIsMovingForward)
{
	// Super call calculates playrate and sets variables.
	Super::SetMovementParameters(NewSpeed, NewRotationYaw, bIsMovingForward);
	if (IsValid(LeftTrackMaterial) && IsValid(RightTrackMaterial))
	{
		RotationYaw = NewRotationYaw;
		Speed = NewSpeed;
		bM_IsMovingForward = bIsMovingForward;

		if (Speed <= AnimationSpeedDivider)
		{
			PlayRate = 1;
		}
		else
		{
			PlayRate = Speed / AnimationSpeedDivider;
		}

		float LeftTrackSpeed, RightTrackSpeed;
		CurrentTrackAnimation = GetTrackAnimationFromFlag(LeftTrackSpeed, RightTrackSpeed);

		// Apply the calculated play rates to the track materials
		LeftTrackMaterial->SetScalarParameterValue(FName("PlayRate"), LeftTrackSpeed);
		RightTrackMaterial->SetScalarParameterValue(FName("PlayRate"), RightTrackSpeed);
	}
	else
	{
		const FString MeshName = GetSkelMeshComponent() ? GetSkelMeshComponent()->GetName() : "Unknown";
		RTSFunctionLibrary::ReportError("Track materials are null!"
			"\n On vehicle mesh: " + MeshName
			+ "\n In function: UTrackedAnimationInstance::SetMovementParameters");
	}
	if constexpr (DeveloperSettings::Debugging::GVehicle_Track_Animation_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Playerate: " + FString::SanitizeFloat(PlayRate), FColor::Red);
	}
}

void UTrackedAnimationInstance::SetChassisAnimToIdle()
{
	if (IsValid(LeftTrackMaterial) && IsValid(RightTrackMaterial))
	{
		LeftTrackMaterial->SetScalarParameterValue(FName("PlayRate"), 0);
		RightTrackMaterial->SetScalarParameterValue(FName("PlayRate"), 0);
		CurrentTrackAnimation = ETrackAnimation::TA_Stationary;
		Speed = 0;
		PlayRate = 0;
	}
}

void UTrackedAnimationInstance::SetChassisAnimToStationaryRotation(const bool bRotateToRight)
{
	if (not IsValid(LeftTrackMaterial) || not IsValid(RightTrackMaterial))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(TryGetPawnOwner(), "TrackMaterial",
		                                             "UTrackedAnimationInstance::SetTracksToStationaryRotation");
		return;
	}
	float LeftTrackSpeed, RightTrackSpeed;
	ETrackAnimation ChosenAnimation;
	if (bRotateToRight)
	{
		ChosenAnimation = ETrackAnimation::TA_StationaryTurnRight;
		LeftTrackSpeed = M_PlayRateSlowForward;
		RightTrackSpeed = M_PlayRateSlowBackward;
	}
	else
	{
		ChosenAnimation = ETrackAnimation::TA_TurnLeft;
		LeftTrackSpeed = M_PlayRateSlowForward;
		RightTrackSpeed = M_PlayRateFastForward;
	}
	LeftTrackMaterial->SetScalarParameterValue(FName("PlayRate"), LeftTrackSpeed);
	RightTrackMaterial->SetScalarParameterValue(FName("PlayRate"), RightTrackSpeed);
	CurrentTrackAnimation = ChosenAnimation;
}


void UTrackedAnimationInstance::InitTrackedAnimationInstance(
	const float NewAnimationSpeedDivider,
	UMaterialInstance* TrackBaseMaterial,
	UNiagaraComponent* TrackParticleComponent,
	UNiagaraComponent* ExhaustParticleComponent,
	const int LeftTrackMaterialIndex,
	const int RightTrackMaterialIndex,
	const float PlayRateSlowForward,
	const float PlayRateFastForward,
	const float PlayRateSlowBackward,
	const float PlayRateFastBackward,
	const float MaxExhaustPlayrate,
	const float MinExhaustPlayrate,
	const float SpeedChangeToStationaryTurn)
{
	if (USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
	{
		LeftTrackMaterial = UMaterialInstanceDynamic::Create(TrackBaseMaterial, Mesh);
		RightTrackMaterial = UMaterialInstanceDynamic::Create(TrackBaseMaterial, Mesh);
		Mesh->SetMaterial(LeftTrackMaterialIndex, LeftTrackMaterial);
		Mesh->SetMaterial(RightTrackMaterialIndex, RightTrackMaterial);
	}

	AnimationSpeedDivider = NewAnimationSpeedDivider;
	M_SpeedChangeToStationaryTurn = SpeedChangeToStationaryTurn;
	// Note that playrates can be negative.
	if (PlayRateSlowForward == 0 || PlayRateFastForward == 0 || PlayRateSlowBackward == 0 || PlayRateFastBackward == 0)
	{
		RTSFunctionLibrary::ReportError("Play rates are invalid!"
			"\n On vehicle: " + GetSkelMeshComponent()->GetOwner()->GetName()
			+ "\n In function: UTrackedAnimationInstance::InitTrackedAnimationInstance");
	}
	else
	{
		M_PlayRateSlowForward = PlayRateSlowForward;
		M_PlayRateFastForward = PlayRateFastForward;
		M_PlayRateSlowBackward = PlayRateSlowBackward;
		M_PlayRateFastBackward = PlayRateFastBackward;
	}
	SetupParticleVFX(TrackParticleComponent, ExhaustParticleComponent, MaxExhaustPlayrate, MinExhaustPlayrate);

}

void UTrackedAnimationInstance::NativeBeginPlay()
{
	// Parent; Chassis to idle and start the VFX update timer.
	Super::NativeBeginPlay();

}



ETrackAnimation UTrackedAnimationInstance::GetTrackAnimationFromFlag(
	float& OutLeftTrackSpeed,
	float& OutRightTrackSpeed) const
{
	uint8 AnimationFlag = 0;
	ETrackAnimation ChosenAnimation = ETrackAnimation::TA_Stationary;

	// Define bitwise masks for conditions
	constexpr uint8 MovingForward = 1 << 0; // 00000001
	constexpr uint8 MovingBackward = 1 << 1; // 00000010
	constexpr uint8 TurningRight = 1 << 2; // 00000100
	constexpr uint8 TurningLeft = 1 << 3; // 00001000
	constexpr uint8 StationaryTurn = 1 << 4; // 00010000
	constexpr uint8 SlowStraight = 1 << 5; // 00100000


	// Set flags based on current movement
	AnimationFlag |= bM_IsMovingForward ? MovingForward : MovingBackward;
	if (FMath::Abs(RotationYaw) > DeveloperSettings::GamePlay::AngleIgnoreLeftRightTrackAnim)
	{
		AnimationFlag |= (RotationYaw > 0) ? TurningRight : TurningLeft;
		if (Speed <= M_SpeedChangeToStationaryTurn)
		{
			AnimationFlag |= StationaryTurn;
		}
	}
	else if (Speed <= M_SpeedChangeToStationaryTurn)
	{
		AnimationFlag |= SlowStraight;
	}
	FString Debug;
	// Determine current track animation based on flags
	switch (AnimationFlag)
	{
	case MovingForward:
		ChosenAnimation = ETrackAnimation::TA_Forward;
		OutLeftTrackSpeed = M_PlayRateFastForward;
		OutRightTrackSpeed = M_PlayRateFastForward;
		Debug = "Forward!";
		break;
	case MovingBackward:
		ChosenAnimation = ETrackAnimation::TA_Backward;
		OutLeftTrackSpeed = M_PlayRateFastBackward;
		OutRightTrackSpeed = M_PlayRateFastBackward;
		Debug = "Backward!";
		break;
	case MovingForward | SlowStraight:
		ChosenAnimation = ETrackAnimation::TA_Forward;
		OutLeftTrackSpeed = M_PlayRateSlowForward;
		OutRightTrackSpeed = M_PlayRateSlowForward;
		Debug = "Slow Forward!";
		break;
	case MovingBackward | SlowStraight:
		ChosenAnimation = ETrackAnimation::TA_Backward;
		OutLeftTrackSpeed = M_PlayRateSlowBackward;
		OutRightTrackSpeed = M_PlayRateSlowBackward;
		Debug = "Slow Backward!";
		break;
	case MovingForward | TurningRight:
		ChosenAnimation = ETrackAnimation::TA_TurnRight;
		OutLeftTrackSpeed = M_PlayRateFastForward;
		OutRightTrackSpeed = M_PlayRateSlowForward;
		Debug = "Turn Right!";
		break;
	case MovingForward | TurningLeft:
		ChosenAnimation = ETrackAnimation::TA_TurnLeft;
		OutLeftTrackSpeed = M_PlayRateSlowForward;
		OutRightTrackSpeed = M_PlayRateFastForward;
		Debug = "Turn Left!";
		break;
	case MovingBackward | TurningRight:
		ChosenAnimation = ETrackAnimation::TA_TurnRight;
		OutLeftTrackSpeed = M_PlayRateSlowBackward;
		OutRightTrackSpeed = M_PlayRateFastBackward;
		Debug = "Turn Right Backward!";
		break;
	case MovingBackward | TurningLeft:
		ChosenAnimation = ETrackAnimation::TA_TurnLeft;
		OutLeftTrackSpeed = M_PlayRateFastBackward;
		OutRightTrackSpeed = M_PlayRateFastBackward;
		Debug = "Turn Left Backward!";
		break;
	case MovingForward | TurningRight | StationaryTurn:
	case MovingBackward | TurningRight | StationaryTurn:
		ChosenAnimation = ETrackAnimation::TA_StationaryTurnRight;
		OutLeftTrackSpeed = M_PlayRateSlowForward;
		OutRightTrackSpeed = M_PlayRateSlowBackward;
		Debug = "Stationary Turn Right!";
		break;
	case MovingForward | TurningLeft | StationaryTurn:
	case MovingBackward | TurningLeft | StationaryTurn:
		ChosenAnimation = ETrackAnimation::TA_StationaryTurnLeft;
		OutLeftTrackSpeed = M_PlayRateSlowBackward;
		OutRightTrackSpeed = M_PlayRateSlowForward;
		Debug = "Stationary Turn Left!";
		break;
	default:
		Debug = "WARNING: default state!";
		ChosenAnimation = ETrackAnimation::TA_Stationary;
		OutLeftTrackSpeed = M_PlayRateFastForward;
		OutRightTrackSpeed = M_PlayRateFastForward;
		break;
	}
	if constexpr (DeveloperSettings::Debugging::GVehicle_Track_Animation_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Debug, FColor::Purple);
	}
	return ChosenAnimation;
}
