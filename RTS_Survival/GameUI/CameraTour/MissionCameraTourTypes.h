// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "MissionCameraTourTypes.generated.h"

class USoundBase;

/**
 * One shot of a mission camera tour: the camera eases from its Start pose to its End pose over Seconds while the
 * shot's caption shows. The camera is the normal RTS camera pawn, so a pose is a ground point to look at plus the
 * pawn's yaw and pitch and the spring arm length (zoom).
 */
USTRUCT(BlueprintType)
struct FMissionCameraTourShot
{
	GENERATED_BODY()

	// Ground point (with its height) in the middle of the screen when the shot starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FVector FocusStart = FVector::ZeroVector;

	// Ground point in the middle of the screen when the shot ends.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FVector FocusEnd = FVector::ZeroVector;

	// World yaw of the camera (the direction it looks in), at the start and the end of the shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float YawStart = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float YawEnd = 0.f;

	// Extra pitch on top of the camera's usual RTS tilt: negative looks down more steeply, positive flatter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "-40.0", ClampMax = "30.0"))
	float PitchStart = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "-40.0", ClampMax = "30.0"))
	float PitchEnd = 0.f;

	// Spring arm length (distance from the camera to the focus); 0 keeps the player's zoom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0"))
	float ArmLengthStart = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0"))
	float ArmLengthEnd = 0.f;

	// Real seconds the shot lasts, its blend-in included.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.5"))
	float Seconds = 5.f;

	// The first seconds of the shot ease the camera over from wherever the previous shot left it; 0 cuts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0"))
	float BlendInSeconds = 1.5f;

	// Caption shown bottom centre during the shot; empty shows none.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	FText Subtitle;
};

/** Presentation of a whole camera tour. */
USTRUCT(BlueprintType)
struct FMissionCameraTourSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	bool bShowLetterbox = true;

	// Height of each cinema bar as a fraction of the screen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation",
		meta = (EditCondition = "bShowLetterbox", ClampMin = "0.0", ClampMax = "0.2"))
	float LetterboxHeightFraction = 0.1f;

	// After the last shot the camera eases back to where the player had it, over these seconds; 0 cuts back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation", meta = (ClampMin = "0.0"))
	float ReturnSeconds = 1.75f;

	// Plays once when the tour takes the screen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	TObjectPtr<USoundBase> StartSound = nullptr;
};
