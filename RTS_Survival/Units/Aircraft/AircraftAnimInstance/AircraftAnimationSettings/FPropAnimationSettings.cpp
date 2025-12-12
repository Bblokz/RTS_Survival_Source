#include "FPropAnimationSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

UAnimSequence* FStabilizerAnimationSettings::GetAnimation() const
{
	UAnimSequence* ChosenSequence = nullptr;
	switch (StabilizerAnimationType)
	{
	case EStabilizerAnimationType::Base:
		ChosenSequence = StabilizerBaseAnimation;
		break;
	case EStabilizerAnimationType::Left:
		ChosenSequence = StabilizerLeftAnimation;
		break;
	case EStabilizerAnimationType::Right:
		ChosenSequence = StabilizerRightAnimation;
		break;
	case EStabilizerAnimationType::Dive:
		ChosenSequence = StabilizerDiveAnimation;
		break;
	case EStabilizerAnimationType::Climb:
		ChosenSequence = StabilizerClimbAnimation;
		break;
	default:
		RTSFunctionLibrary::ReportError("Unable to translation stabilization animation enum into a UAnimSequence*."
			" Please check the enum value: " + FString::FromInt(static_cast<int32>(StabilizerAnimationType))
			+ " or the animation settings.");
		return nullptr;
	}
	if (not ChosenSequence)
	{
		RTSFunctionLibrary::ReportError("Chosen Stabilizer Animation is nullptr. Please check the animation settings.");
		return StabilizerBaseAnimation;
	}
	return ChosenSequence;
}

UAnimSequence* FWingRotorsAnimationSettings::GetAnimation() const
{
	UAnimSequence* ChosenSequence = nullptr;
	if(LandedState == EAircraftLandingState::Airborne)
	{
		ChosenSequence = FlyingSequence;	
	}
	else
	{
		ChosenSequence =IdleSequence;
	}
	if(not ChosenSequence)
	{
		RTSFunctionLibrary::ReportError("Chosen Wing Rotors Animation is nullptr. Please check the animation settings.");
		return IdleSequence;
	}
	return ChosenSequence;
}
