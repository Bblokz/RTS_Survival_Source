#include "WheeledAnimationInstance.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UWheeledAnimationInstance::SetMovementParameters(const float NewSpeed, const float NewRotationYaw,
                                                      const bool bIsMovingForward)
{
	// 1) Call the base class to calculate PlayRate, Speed, RotationYaw, etc.
	Super::SetMovementParameters(NewSpeed, NewRotationYaw, bIsMovingForward);

	// 2) Define local bit flags for different conditions
	constexpr uint8 MOVING_FORWARD = 1 << 0; // 00000001
	constexpr uint8 MOVING_BACKWARD = 1 << 1; // 00000010
	constexpr uint8 TURNING_RIGHT = 1 << 2; // 00000100
	constexpr uint8 TURNING_LEFT = 1 << 3; // 00001000
	constexpr uint8 STATIONARY_TURN = 1 << 4; // 00010000

	// 3) Combine flags into a single AnimationFlag
	uint8 AnimationFlag = 0;

	// Forward vs. Backward
	if (bIsMovingForward)
	{
		AnimationFlag |= MOVING_FORWARD;
	}
	else
	{
		AnimationFlag |= MOVING_BACKWARD;
	}

	// Check if we’re turning left or right
	if (RotationYaw >= M_ConsiderRotationYawTurningThreshold)
	{
		AnimationFlag |= TURNING_RIGHT;
	}
	else if (RotationYaw <= -M_ConsiderRotationYawTurningThreshold)
	{
		AnimationFlag |= TURNING_LEFT;
	}

	// Check if speed is small enough to be considered “stationary turn”
	// (or “slow turn” if you prefer). You can tweak M_SpeedChangeToStationaryTurn in InitWheeledAnimationInstance.
	if (Speed <= M_SpeedChangeToStationaryTurn)
	{
		AnimationFlag |= STATIONARY_TURN;
	}
	FString Debug = "No Debug Set";
	// 4) Determine final animation based on the combination of flags
	switch (AnimationFlag)
	{
	// ---------------------------------------------------
	// Straight Movement
	// ---------------------------------------------------
	case MOVING_FORWARD:
		CurrentWheelAnimation = EWheelAnimation::WA_Forward;
		Debug = "Forward";
		break;

	case MOVING_BACKWARD:
		CurrentWheelAnimation = EWheelAnimation::WA_Backward;
		Debug = "Backward";
		break;

	case STATIONARY_TURN:
		// If Speed ~ 0 but no turn bits, we can treat it as Stationary
		Debug = "Stationary";
		CurrentWheelAnimation = EWheelAnimation::WA_Stationary;
		break;

	case MOVING_FORWARD | TURNING_LEFT:
		Debug = "TurnLeft";
		CurrentWheelAnimation = EWheelAnimation::WA_TurnLeft;
		break;

	case MOVING_FORWARD | TURNING_RIGHT:
		Debug = "TurnRight";
		CurrentWheelAnimation = EWheelAnimation::WA_TurnRight;
		break;

	// Forward + turning + stationary turn
	case MOVING_FORWARD | TURNING_LEFT | STATIONARY_TURN:
		Debug = "StationaryTurnLeft";
		CurrentWheelAnimation = EWheelAnimation::WA_StationaryTurnLeft;
		break;

	case MOVING_FORWARD | TURNING_RIGHT | STATIONARY_TURN:
		Debug = "StationaryTurnRight";
		CurrentWheelAnimation = EWheelAnimation::WA_StationaryTurnRight;
		break;

	case MOVING_BACKWARD | TURNING_LEFT:
		Debug = "TurnRightBackwards";
		CurrentWheelAnimation = EWheelAnimation::WA_TurnRightBackwards;
		break;

	case MOVING_BACKWARD | TURNING_RIGHT:
		Debug = "TurnLeftBackwards";
		CurrentWheelAnimation = EWheelAnimation::WA_TurnLeftBackwards;
		break;

	// Backward + turning + stationary turn
	case MOVING_BACKWARD | TURNING_LEFT | STATIONARY_TURN:
		Debug = "StationaryTurnLeft";
		CurrentWheelAnimation = EWheelAnimation::WA_StationaryTurnLeft;
		break;

	case MOVING_BACKWARD | TURNING_RIGHT | STATIONARY_TURN:
		Debug = "StationaryTurnRight";
		CurrentWheelAnimation = EWheelAnimation::WA_StationaryTurnRight;
		break;


	default:
		// If none of the cases matched exactly, treat as stationary
		Debug = "DEFAULT : Stationary";
		CurrentWheelAnimation = EWheelAnimation::WA_Stationary;
		break;
	}
	if (DeveloperSettings::Debugging::GVehicle_Wheel_Animation_Compile_DebugSymbols)
	{
		// Get owner location.
		const FVector DebugLocation = GetOwningComponent()->GetComponentLocation() + FVector(0, 0, 400);
		DrawDebugString(GetWorld(), DebugLocation, Debug, nullptr, FColor::Red, 0.1f, false);
		const FString PlayRateDebug = FString::SanitizeFloat(PlayRate);
		DrawDebugString(GetWorld(), DebugLocation + FVector(0, 0, 100), PlayRateDebug, nullptr, FColor::Red, 0.1f,
		                false);
	}
}

void UWheeledAnimationInstance::SetChassisAnimToStationaryRotation(const bool bRotateToRight)
{
	CurrentWheelAnimation = bRotateToRight
		                        ? EWheelAnimation::WA_StationaryTurnRight
		                        : EWheelAnimation::WA_StationaryTurnLeft;
}

void UWheeledAnimationInstance::SetChassisAnimToIdle()
{
	CurrentWheelAnimation = EWheelAnimation::WA_Stationary;
}

void UWheeledAnimationInstance::NativeBeginPlay()
{
	// Parent; Chassis to idle and start the VFX update timer.
	Super::NativeBeginPlay();
}


void UWheeledAnimationInstance::InitWheeledAnimationInstance(
	UNiagaraComponent* ChassisSmokeComponent,
	UNiagaraComponent* ExhaustParticleComponent,
	const float MaxExhaustPlayrate,
	const float MinExhaustPlayrate,
	float NewAnimationSpeedDivider,
	float SpeedChangeToStationaryTurn,
	float ConsiderRotationYawTurningThreshold)
{
	if (NewAnimationSpeedDivider <= 0)
	{
		RTSFunctionLibrary::ReportError("AnimationSpeedDivider must be greater than 0 in " + GetName());
		NewAnimationSpeedDivider = 200;
	}
	if (SpeedChangeToStationaryTurn <= 0)
	{
		RTSFunctionLibrary::ReportError("SpeedChangeToStationaryTurn must be greater than 0 in " + GetName());
		SpeedChangeToStationaryTurn = 30;
	}
	if (ConsiderRotationYawTurningThreshold <= 0)
	{
		RTSFunctionLibrary::ReportError("ConsiderRotationYawTurningThreshold must be greater than 0 in " + GetName());
		ConsiderRotationYawTurningThreshold = 15;
	}

	AnimationSpeedDivider = NewAnimationSpeedDivider;
	M_SpeedChangeToStationaryTurn = SpeedChangeToStationaryTurn;
	M_ConsiderRotationYawTurningThreshold = ConsiderRotationYawTurningThreshold;
	SetupParticleVFX(ChassisSmokeComponent, ExhaustParticleComponent, MaxExhaustPlayrate, MinExhaustPlayrate);
}
