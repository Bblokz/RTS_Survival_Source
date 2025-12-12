#pragma once

#include "CoreMinimal.h"

#include "ConstructionPreviewSounds.generated.h"

UENUM()
enum class EConstructionPreviewSoundType
{
	None,
	StartPreview,
	Placement,
	Socket,
	RotateClockWise,
	RotateCounterClockWise
};

USTRUCT(BlueprintType)
struct FConstructionPreviewSounds
{
	GENERATED_BODY()

	void PlayConstructionSound(const UObject* WorldContextObject, EConstructionPreviewSoundType SoundType) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundBase* StartPreviewSound = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundBase* PlacementSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundBase* SocketSound= nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundBase* RotateClockWiseSound= nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundBase* RotateCounterClockWiseSound= nullptr;
};
