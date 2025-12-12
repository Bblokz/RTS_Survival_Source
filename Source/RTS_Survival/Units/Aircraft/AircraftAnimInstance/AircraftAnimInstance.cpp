// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftAnimInstance.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

void UAircraftAnimInstance::Init(AAircraftMaster* OwningAircraft)
{
	if (not OwningAircraft)
	{
		RTSFunctionLibrary::ReportError("AircraftAnimInstance::Init: OwningAircraft is null.");
		return;
	}
	AircraftSoundController.InitEngineSound(OwningAircraft, this);
}

void UAircraftAnimInstance::UpdateAnim(const FRotator TargetRotator, const float MovementSpeed)
{
	// Keep original relationship: M_PropSpeed = Max / Speed.
	// (Do not guard for zero here because GetLastAirSpeed relies on reciprocal yielding 0.)
	M_PropSpeed = PropellerAnimSettings.MaxMovementSpeed / MovementSpeed;

	// Local shorthands
	const float Pitch = TargetRotator.Pitch; // deg
	const float YawAbs = FMath::Abs(TargetRotator.Yaw); // used as "up/down movement considered" gate
	const float Roll = TargetRotator.Roll;
	const float RollAbs = FMath::Abs(Roll);

	const float YawEnter = StabilizerAnimSettings.ThresholdConsiderMovementUpDown + StabilizerHysteresisDegrees;
	const float YawExit  = FMath::Max(0.0f, StabilizerAnimSettings.ThresholdConsiderMovementUpDown - StabilizerHysteresisDegrees);

	const float RollEnter = StabilizerAnimSettings.ThresholdRollConsiderLeftRight + StabilizerHysteresisDegrees;
	const float RollExit  = FMath::Max(0.0f, StabilizerAnimSettings.ThresholdRollConsiderLeftRight - StabilizerHysteresisDegrees);

	const EStabilizerAnimationType Prev = StabilizerAnimSettings.StabilizerAnimationType;

	const auto IsYawGroup = [](EStabilizerAnimationType T)
	{
		return (T == EStabilizerAnimationType::Climb) || (T == EStabilizerAnimationType::Dive);
	};
	const auto IsRollGroup = [](EStabilizerAnimationType T)
	{
		return (T == EStabilizerAnimationType::Left) || (T == EStabilizerAnimationType::Right);
	};

	// --- Group selection with hysteresis (Yaw group has priority, same as before) ---
	// We "latch" into a group once entered and only leave when the angle falls below the "exit" threshold.
	const bool YawWantsEnter = (YawAbs >= YawEnter);
	const bool YawCanHold    = (YawAbs >= YawExit);

	const bool RollWantsEnter = (RollAbs >= RollEnter);
	const bool RollCanHold    = (RollAbs >= RollExit);

	// Helper lambdas to determine signed state with hysteresis around the 0-crossing.
	const auto ResolveUpDownWithHyst = [this, Pitch, Prev]() -> EStabilizerAnimationType
	{
		// If firmly positive/negative beyond hysteresis band, pick the sign.
		if (Pitch >  +StabilizerHysteresisDegrees) return EStabilizerAnimationType::Climb;
		if (Pitch <  -StabilizerHysteresisDegrees) return EStabilizerAnimationType::Dive;

		// Within the deadband: keep current if already in this group, else bias to sign >= 0.
		if (Prev == EStabilizerAnimationType::Climb || Prev == EStabilizerAnimationType::Dive)
		{
			return Prev;
		}
		return (Pitch >= 0.0f) ? EStabilizerAnimationType::Climb : EStabilizerAnimationType::Dive;
	};

	const auto ResolveLeftRightWithHyst = [this, Roll, Prev]() -> EStabilizerAnimationType
	{
		if (Roll >  +StabilizerHysteresisDegrees) return EStabilizerAnimationType::Left;
		if (Roll <  -StabilizerHysteresisDegrees) return EStabilizerAnimationType::Right;

		if (Prev == EStabilizerAnimationType::Left || Prev == EStabilizerAnimationType::Right)
		{
			return Prev;
		}
		return (Roll >= 0.0f) ? EStabilizerAnimationType::Left : EStabilizerAnimationType::Right;
	};

	// State machine with hysteresis
	EStabilizerAnimationType Next = EStabilizerAnimationType::Base;

	if (IsYawGroup(Prev))
	{
		// Currently latched in yaw (Climb/Dive)
		if (YawCanHold)
		{
			Next = ResolveUpDownWithHyst();
		}
		else
		{
			// Lost yaw; consider entering roll, otherwise base.
			if (RollWantsEnter)
			{
				Next = ResolveLeftRightWithHyst();
			}
			else
			{
				Next = EStabilizerAnimationType::Base;
			}
		}
	}
	else if (IsRollGroup(Prev))
	{
		// Currently latched in roll (Left/Right)
		if (YawWantsEnter)
		{
			// Yaw group preempts roll when clearly above its enter threshold (keeps original priority).
			Next = ResolveUpDownWithHyst();
		}
		else if (RollCanHold)
		{
			Next = ResolveLeftRightWithHyst();
		}
		else
		{
			// Lost roll; maybe enter yaw, else base.
			if (YawWantsEnter)
			{
				Next = ResolveUpDownWithHyst();
			}
			else
			{
				Next = EStabilizerAnimationType::Base;
			}
		}
	}
	else
	{
		// Was Base: allow entering either group using the stricter "enter" thresholds. Yaw keeps priority.
		if (YawWantsEnter)
		{
			Next = ResolveUpDownWithHyst();
		}
		else if (RollWantsEnter)
		{
			Next = ResolveLeftRightWithHyst();
		}
		else
		{
			Next = EStabilizerAnimationType::Base;
		}
	}

	StabilizerAnimSettings.StabilizerAnimationType = Next;
}

void UAircraftAnimInstance::UpdateLandedState(const EAircraftLandingState NewLandingState)
{
	WingRotorAnimSettings.LandedState = NewLandingState;
}

void UAircraftAnimInstance::SetMaxMovementSpeed(const float MaxMovementSpeed)
{
	PropellerAnimSettings.MaxMovementSpeed = MaxMovementSpeed;
}

void UAircraftAnimInstance::OnPrepareVto(const float VtoPrepTime)
{
	if (not WingRotorAnimSettings.PrepareVTOMontage)
	{
		RTSFunctionLibrary::ReportError(
			"AircraftAnimInstance::OnPrepareVto: PrepareVTOMontage is not set in WingRotorAnimSettings.");
		return;
	}
	Montage_Play(WingRotorAnimSettings.PrepareVTOMontage,
	             GetMontagePlayRateForTime(VtoPrepTime, WingRotorAnimSettings.PrepareVTOMontage));
	AircraftSoundController.PlayVto_Prep(this, VtoPrepTime);
}

void UAircraftAnimInstance::OnVtoStart(const float TotalVtoTime)
{
	if (not WingRotorAnimSettings.TakeOffMontage)
	{
		RTSFunctionLibrary::ReportError(
			"AircraftAnimInstance::OnVTOStart: TakeOffMontage is not set in WingRotorAnimSettings.");
		return;
	}
	// no PlayRate to set; montage is always at the same speed.
	Montage_Play(WingRotorAnimSettings.TakeOffMontage);
	AircraftSoundController.PlayVto_Start(this, TotalVtoTime);
}

void UAircraftAnimInstance::OnVtoComplete()
{
	// Stop any montages that might be playing.
	Montage_Stop(0.2f);
	AircraftSoundController.Play_Airborne(this);
}

void UAircraftAnimInstance::OnWaitForLanding()
{
	if (not WingRotorAnimSettings.HoverWaitForLandingMontage)
	{
		RTSFunctionLibrary::ReportError(
			"AircraftAnimInstance::OnWaitForLanding: HoverWaitForLandingMontage is not set in WingRotorAnimSettings.");
		return;
	}
	Montage_Play(WingRotorAnimSettings.HoverWaitForLandingMontage);
}

void UAircraftAnimInstance::OnStartLanding(const float TotalLandingTime)
{
	if (not WingRotorAnimSettings.TakeOffMontage)
	{
		RTSFunctionLibrary::ReportError(
			"AircraftAnimInstance::OnStartLanding: TakeOffMontage is not set in WingRotorAnimSettings.");
		return;
	}
	const float PlayRate = GetMontagePlayRateForTime(TotalLandingTime, WingRotorAnimSettings.TakeOffMontage);
	Montage_Play(WingRotorAnimSettings.LandingMontage, PlayRate);
	AircraftSoundController.PlayVto_Landing(this, TotalLandingTime);
}

void UAircraftAnimInstance::OnLandingComplete()
{
	AircraftSoundController.Play_Landed(this);
}

float UAircraftAnimInstance::GetPropSpeed()
{
	if (WingRotorAnimSettings.LandedState == EAircraftLandingState::Landed)
	{
		return 0.0f;
	}
	return M_PropSpeed;
}

float UAircraftAnimInstance::GetMontagePlayRateForTime(const float TotalMontageTimeAllowed,
                                                       const UAnimMontage* Montage) const
{
	if (not IsValid(Montage))
	{
		return 1.f;
	}

	// Avoid div-by-zero and silly values
	if (TotalMontageTimeAllowed <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const float MontageLength = Montage->GetPlayLength(); // seconds
	const float RateScale = FMath::Max(KINDA_SMALL_NUMBER, Montage->RateScale);

	// Ensure the montage finishes in VtoPrepTime seconds.
	const float PlayRate = (MontageLength / TotalMontageTimeAllowed) / RateScale;
	// todo remove.
	RTSFunctionLibrary::PrintString("MONTAGECALCULATED PLAYRATE: " + FString::SanitizeFloat(PlayRate), FColor::Red,
	                                5.f);
	return PlayRate;
}
