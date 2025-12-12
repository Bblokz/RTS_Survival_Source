// Copyright (c) Bas Blokzijl. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Sound/SoundCue.h"
#include "FlameThrowerSettings.generated.h"

/**
 * Designer settings for flamethrower weapons.
 * Notes:
 * - FlameIterations is clamped to [1..10].
 * - ConeAngleMlt is clamped to [1..5].
 * - LoopingSound is assumed looping; we start it at fire begin and stop+destroy after the flame ends.
 */
USTRUCT(BlueprintType)
struct FFlameThrowerSettings
{
	GENERATED_BODY()

	/** Niagara system that renders the flame (must expose: Color, Duration, Range, ConeAngle). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VFX")
	UNiagaraSystem* FlameEffect = nullptr;

	/** Looping fire sound to start/stop manually while the flame is active. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Audio")
	USoundCue* LoopingSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Audio")
	USoundAttenuation* LaunchAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Audio")
	USoundConcurrency* LaunchConcurrency = nullptr;

	/** Color parameter that drives the flame tint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VFX")
	FLinearColor Color = FLinearColor(1.f, 0.5f, 0.f, 1.f);

	/** How many “ticks” the flame lasts (duration = Iterations * FlameIterationTime). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Gameplay", meta=(ClampMin="1", ClampMax="10", UIMin="1", UIMax="10"))
	int32 FlameIterations = 2;

	/** Cone multiplier (ConeAngle = FlameConeAngleUnit * ConeAngleMlt). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Gameplay", meta=(ClampMin="1", ClampMax="5", UIMin="1", UIMax="5"))
	int32 ConeAngleMlt = 2;
};
