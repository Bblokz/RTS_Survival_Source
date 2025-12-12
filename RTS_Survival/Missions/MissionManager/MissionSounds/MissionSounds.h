#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundCue.h"

#include "MissionSounds.generated.h"

USTRUCT(Blueprintable)
struct FMissionSoundSettings
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* MissionCompleteSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* MissionLoadedSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* MissionFailedSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundConcurrency* MissionSoundConcurrency = nullptr;
};

UENUM()
enum class EMissionSoundType : uint8
{
	None,
	MissionLoaded,
	MissionCompleted,
	MissionFailed
};
