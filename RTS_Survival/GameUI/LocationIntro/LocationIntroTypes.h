// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "LocationIntroTypes.generated.h"

class USoundBase;

/** World axis the camera drifts along during a location intro. */
UENUM(BlueprintType)
enum class ELocationIntroDriftAxis : uint8
{
	WorldX,
	WorldY
};

/**
 * @brief Slow camera turn of a location intro: the camera pawn turns by these offsets (an orbit around the point the
 * camera looks at) and, with bReturnToStartRotation, turns back during the last ReturnSeconds of the intro.
 */
USTRUCT(BlueprintType)
struct FLocationIntroPanSettings
{
	GENERATED_BODY()

	// Positive turns the view to the right (orbits the look-at point counter-clockwise seen from above).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pan", meta = (ClampMin = "-90", ClampMax = "90"))
	float YawDegrees = 10.f;

	// Extra tilt added to the camera pawn; positive tilts up. Keep it small, the RTS camera has a fixed base pitch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pan", meta = (ClampMin = "-30", ClampMax = "30"))
	float PitchDegrees = 0.f;

	// False leaves the camera turned when the intro ends.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pan")
	bool bReturnToStartRotation = true;

	// Part of the intro (at its end) used to turn back; the turn out uses the time before it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pan",
		meta = (ClampMin = "0.1", EditCondition = "bReturnToStartRotation"))
	float ReturnSeconds = 1.6f;
};

/**
 * @brief Look and timing of a location intro; the defaults are the tuned look, so most calls leave it unconnected.
 * All times are real seconds from the start of the intro.
 */
USTRUCT(BlueprintType)
struct FLocationIntroPresentation
{
	GENERATED_BODY()

	// Black cinema bars that slide in at the top and bottom of the screen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Letterbox")
	bool bShowLetterbox = true;

	// Height of each bar as a fraction of the screen height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Letterbox",
		meta = (ClampMin = "0.02", ClampMax = "0.2", EditCondition = "bShowLetterbox"))
	float LetterboxHeightFraction = 0.085f;

	// Start from a black screen that fades to the game; use at the very start of a map, not mid-mission.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fade")
	bool bFadeFromBlack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fade",
		meta = (ClampMin = "0.1", EditCondition = "bFadeFromBlack"))
	float FadeFromBlackSeconds = 1.8f;

	// When the location name starts to appear.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0"))
	float LocationStartSeconds = 0.5f;

	// When the date starts to type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0"))
	float DateStartSeconds = 1.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "1"))
	float TypewriterLettersPerSecond = 17.f;

	// The texts fade and the bars slide away during the last part of the intro.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.1"))
	float FadeOutSeconds = 1.1f;

	// Played once when the intro starts (a sting or low drone).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> IntroSound = nullptr;

	// Played for every typed letter of the date and time (a short typewriter click).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sounds")
	TObjectPtr<USoundBase> TypewriterSound = nullptr;
};

/** Texts of a location intro. */
USTRUCT(BlueprintType)
struct FLocationIntroTexts
{
	GENERATED_BODY()

	// Large, top centre (e.g. "Berlin"); shown in capitals.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texts")
	FText Location;

	// Optional smaller line under the location (e.g. "Reichstag district, Germany").
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texts")
	FText LocationSubtitle;

	// Typed out bottom left (e.g. "21 April 1945").
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texts")
	FText Date;

	// Optional line typed under the date (e.g. "05:40 hrs").
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texts")
	FText TimeOfDay;
};
