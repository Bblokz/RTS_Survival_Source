// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftFlightModel.h"

#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

namespace AircraftFlightModelConstants
{
	// Below this speed the velocity says nothing about the heading; the actor's yaw is used instead (cm/s).
	constexpr float MinSpeedForFlightDirection = 50.f;
	// Speeds used for the floor and the altitude hold so a nearly stopped aircraft still gets sensible limits (cm/s).
	constexpr float MinSpeedForPitchLimits = 100.f;
}

void FAircraftFlightModel::Reset()
{
	M_YawRate = 0.f;
	M_PitchRate = 0.f;
}

float FAircraftFlightModel::GetTurnRadius(const float Speed, const FAircraftFlightModelSettings& Settings)
{
	const float MaxTurnRateRadians = FMath::DegreesToRadians(FMath::Max(Settings.MaxTurnRate, 1.f));
	return FMath::Max(Speed, 0.f) / MaxTurnRateRadians;
}

float FAircraftFlightModel::GetWantedRate(const float AngleError, const float Gain, const float RateAcceleration,
                                          const float MaxRate)
{
	const float AbsError = FMath::Abs(AngleError);
	const float ProportionalRate = AbsError * Gain;
	// The fastest rate from which braking at RateAcceleration still stops on the wanted angle.
	const float BrakingRate = FMath::Sqrt(2.f * RateAcceleration * AbsError);
	return FMath::Sign(AngleError) * FMath::Min3(ProportionalRate, BrakingRate, MaxRate);
}

float FAircraftFlightModel::GetLowestPitchAboveFloor(const FAircraftFlightInput& Input, const float Speed,
                                                     const FAircraftFlightModelSettings& Settings)
{
	using namespace AircraftFlightModelConstants;
	if (not Input.FloorZ.IsSet())
	{
		return -Settings.MaxFlightPitch;
	}
	// Descend at most as fast as reaches the floor in FloorReactionSeconds, so the dive flattens out before it.
	const float HeightAboveFloor = FMath::Max(0.f, Input.Location.Z - Input.FloorZ.GetValue());
	const float MaxDescentSpeed = HeightAboveFloor / FMath::Max(Settings.FloorReactionSeconds, KINDA_SMALL_NUMBER);
	const float SinLowestPitch = FMath::Clamp(MaxDescentSpeed / FMath::Max(Speed, MinSpeedForPitchLimits), 0.f, 1.f);
	return FMath::Max(-Settings.MaxFlightPitch, -FMath::RadiansToDegrees(FMath::Asin(SinLowestPitch)));
}

float FAircraftFlightModel::GetAltitudeHoldPitch(const FAircraftFlightInput& Input, const float Speed,
                                                 const FAircraftFlightModelSettings& Settings)
{
	using namespace AircraftFlightModelConstants;
	const float HeightError = Input.AimPoint.Z - Input.Location.Z;
	const float AbsHeightError = FMath::Abs(HeightError);
	const float FlightSpeed = FMath::Max(Speed, MinSpeedForPitchLimits);
	const float MaxClimbSpeed = FlightSpeed * FMath::Sin(FMath::DegreesToRadians(Settings.MaxAltitudeHoldPitch));
	// Proportional when close, never faster than easing out at the hold acceleration allows: no overshoot.
	const float WantedVerticalSpeed = FMath::Sign(HeightError) * FMath::Min3(
		AbsHeightError * Settings.AltitudeHoldGain,
		FMath::Sqrt(2.f * Settings.AltitudeHoldAcceleration * AbsHeightError),
		MaxClimbSpeed);
	return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(WantedVerticalSpeed / FlightSpeed, -1.f, 1.f)));
}

float FAircraftFlightModel::GetNewSpeed(const FAircraftFlightInput& Input, const float CurrentSpeed,
                                        const FAircraftFlightModelSettings& Settings) const
{
	// A hard turn bleeds a little speed, the more the closer it is to the fastest turn.
	const float TurnShare = FMath::Clamp(FMath::Abs(M_YawRate) / FMath::Max(Settings.MaxTurnRate, 1.f), 0.f, 1.f);
	const float TurnSpeedLoss = FMath::Clamp(Settings.TurnSpeedLoss, 0.f, 1.f) * TurnShare * TurnShare;
	const float TargetSpeed = FMath::Max(0.f, Input.WantedSpeed * (1.f - TurnSpeedLoss));
	if (CurrentSpeed < TargetSpeed)
	{
		return FMath::Min(TargetSpeed, CurrentSpeed + FMath::Max(Input.Acceleration, 0.f) * Input.DeltaTime);
	}
	const float Deceleration = Input.Deceleration.IsSet()
		                           ? FMath::Max(Input.Deceleration.GetValue(), Settings.SpeedDecreaseRate)
		                           : Settings.SpeedDecreaseRate;
	return FMath::Max(TargetSpeed, CurrentSpeed - FMath::Max(Deceleration, 0.f) * Input.DeltaTime);
}

float FAircraftFlightModel::GetNewRoll(const FAircraftFlightInput& Input, const float Speed,
                                       const FAircraftFlightModelSettings& Settings) const
{
	// The bank is the share of the hardest turn's sideways acceleration (speed x turn rate at cruise speed): wings
	// level when flying straight or nearly still, the full bank only in the hardest turn at cruise speed.
	const float ReferenceSpeed = FMath::Max(Input.ReferenceSpeed, 1.f);
	const float LateralShare = (Speed * M_YawRate) / (ReferenceSpeed * FMath::Max(Settings.MaxTurnRate, 1.f));
	const float WantedRoll = FMath::Clamp(LateralShare, -1.f, 1.f) * Settings.MaxBankAngle;
	const float CurrentRoll = FRotator::NormalizeAxis(Input.CurrentRotation.Roll);
	// Exponential easing that stays stable over long frames, then a roll rate cap so the bank never whips over.
	const float EaseShare = 1.f - FMath::Exp(-Settings.BankInterpSpeed * Input.DeltaTime);
	const float MaxRollStep = Settings.MaxRollRate * Input.DeltaTime;
	const float RollStep = FMath::Clamp((WantedRoll - CurrentRoll) * EaseShare, -MaxRollStep, MaxRollStep);
	return CurrentRoll + RollStep;
}

FAircraftFlightOutput FAircraftFlightModel::Step(const FAircraftFlightInput& Input,
                                                 const FAircraftFlightModelSettings& Settings)
{
	using namespace AircraftFlightModelConstants;
	FAircraftFlightOutput Output;
	const float DeltaTime = FMath::Max(Input.DeltaTime, 0.f);
	const float Speed = Input.Velocity.Size();

	// The flight direction now: where the aircraft actually moves, not where its nose points.
	FRotator FlightRotation = Input.CurrentRotation;
	FlightRotation.Pitch = 0.f;
	if (Speed >= MinSpeedForFlightDirection)
	{
		FlightRotation = Input.Velocity.Rotation();
	}
	FlightRotation.Roll = 0.f;

	const FVector ToAim = Input.AimPoint - Input.Location;
	const FRotator AimRotation = ToAim.IsNearlyZero() ? FlightRotation : ToAim.Rotation();

	// Heading: the turn rate moves toward the rate that reaches the aim, never faster than its acceleration allows.
	const float YawError = FMath::FindDeltaAngleDegrees(FlightRotation.Yaw, AimRotation.Yaw);
	const float WantedYawRate = GetWantedRate(YawError, Settings.TurnResponsiveness, Settings.TurnRateAcceleration,
	                                          Settings.MaxTurnRate);
	M_YawRate = FMath::FInterpConstantTo(M_YawRate, WantedYawRate, DeltaTime, Settings.TurnRateAcceleration);
	const float NewYaw = FRotator::NormalizeAxis(FlightRotation.Yaw + M_YawRate * DeltaTime);

	// Climb angle: toward the aim (dives) or the eased altitude hold (cruise), within the limits and the floor.
	const float LowestPitch = GetLowestPitchAboveFloor(Input, Speed, Settings);
	const float AimPitch = Input.bHoldAltitude
		                       ? GetAltitudeHoldPitch(Input, Speed, Settings)
		                       : FRotator::NormalizeAxis(AimRotation.Pitch);
	const float WantedPitch = FMath::Clamp(AimPitch, LowestPitch, Settings.MaxFlightPitch);
	const float PitchError = WantedPitch - FlightRotation.Pitch;
	const float WantedPitchRate = GetWantedRate(PitchError, Settings.TurnResponsiveness,
	                                            Settings.PitchRateAcceleration, Settings.MaxPitchRate);
	M_PitchRate = FMath::FInterpConstantTo(M_PitchRate, WantedPitchRate, DeltaTime, Settings.PitchRateAcceleration);
	float NewPitch = FlightRotation.Pitch + M_PitchRate * DeltaTime;
	if (NewPitch < LowestPitch)
	{
		// The lowest pitch rises gradually as the floor comes closer, so following it flattens the dive smoothly; a
		// descending rate is dropped so the pull-out does not lag behind the floor.
		NewPitch = LowestPitch;
		M_PitchRate = FMath::Max(M_PitchRate, 0.f);
	}
	NewPitch = FMath::Clamp(NewPitch, -Settings.MaxFlightPitch, Settings.MaxFlightPitch);

	const float NewSpeed = GetNewSpeed(Input, Speed, Settings);
	Output.Velocity = FRotator(NewPitch, NewYaw, 0.f).Vector() * NewSpeed;
	Output.Rotation = FRotator(NewPitch + Input.ClimbPitch, NewYaw, GetNewRoll(Input, NewSpeed, Settings));
	return Output;
}
