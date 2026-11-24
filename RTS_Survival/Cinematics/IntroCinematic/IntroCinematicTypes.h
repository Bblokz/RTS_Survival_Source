// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "IntroCinematicTypes.generated.h"

/** What an intro cinematic cue puts on screen (or does to the world) while the master sequence is inside its range. */
UENUM(BlueprintType)
enum class EIntroCinematicCueType : uint8
{
	// The narrator portrait, top middle.
	Portrait,
	// Date and place card, lower left: Line1 large, Line2 small.
	Card,
	// Rubber stamp at Position, turned by AngleDegrees.
	Stamp,
	// Centred archival notice on black; burns away over the last Param seconds.
	Notice,
	// Small typed label at Position.
	Label,
	// Not drawn: hides every streamed-level actor whose class name contains Line1, for the whole film.
	HideActors,
	// Not drawn: runs the scalar material parameter Line2 from 0 to 1 over the cue, on every static mesh actor
	// whose mesh name contains Line1 (a baked deploy animation playing inside a shot).
	MaterialRamp
};

/**
 * @brief Row of DT_IntroCues: one on-screen element of the 1976 intro cinematic, placed on the master sequence clock.
 * Filled by Source/Tools/IntroCinematic from the shot manifest; UIntroCinematicSubsystem feeds the rows to the HUD.
 */
USTRUCT(BlueprintType)
struct FIntroCinematicCueRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	EIntroCinematicCueType Type = EIntroCinematicCueType::Card;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	float StartSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	float EndSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FString Line1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FString Line2;

	// Screen position as a fraction of the screen (0,0 = top left); stamps and labels only.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FVector2D Position = FVector2D(0.5, 0.5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	float AngleDegrees = 0.0f;

	// Notice: seconds the burn takes. Other types: unused.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	float Param = 0.0f;
};
