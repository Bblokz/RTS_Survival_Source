#pragma once
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

#include "CoreMinimal.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "FPropAnimationSettings.generated.h"

enum class EAircraftLandingState : uint8;

USTRUCT(Blueprintable, BlueprintType)
struct FPropAnimationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAimOffsetBlendSpace1D* PropellerBlendSpace = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* PropellerBaseAnimation = nullptr;

	UPROPERTY()
	float MaxMovementSpeed = 0.0f;
};

UENUM(BlueprintType)
enum class EStabilizerAnimationType : uint8
{
	Base,
	Left,
	Right,
	Dive,
	Climb
};

USTRUCT(Blueprintable, BlueprintType)
struct FStabilizerAnimationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EStabilizerAnimationType StabilizerAnimationType = EStabilizerAnimationType::Base;

	UAnimSequence* GetAnimation() const;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ThresholdConsiderMovementUpDown = 2.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ThresholdRollConsiderLeftRight = 0.5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* StabilizerBaseAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* StabilizerLeftAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* StabilizerRightAnimation = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* StabilizerDiveAnimation = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* StabilizerClimbAnimation = nullptr;
};

USTRUCT(Blueprintable, BlueprintType)
struct FWingRotorsAnimationSettings
{
	GENERATED_BODY()

	// Note that we only use the airborne or idle form this enum; at vertical take off and landing we overwrite the
	// anim sequence with a montage.
	EAircraftLandingState LandedState = EAircraftLandingState::None;

	// Are we airborne? otherwise play (on the ground) idle sequence.
	UAnimSequence* GetAnimation() const;

	// Montage played while the aircraft is on the ground; preparing to take off.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* PrepareVTOMontage = nullptr;

	// Played after the PrepareVTOMontage; while ticking the aircraft to go up.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* TakeOffMontage = nullptr;
	
	// Played after the PrepareVTOMontage; while ticking the aircraft to go up.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* LandingMontage= nullptr;

	// Played while the aircraft is hovering airborne; waiting for the doors to open.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* HoverWaitForLandingMontage = nullptr;
	
	// The idle sequence of the wing rotors; when the aircraft is not airborne.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* IdleSequence = nullptr;

	// The flying sequence of the wing rotors; when the aircraft is airborne.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimSequence* FlyingSequence = nullptr;
};
