#include "ConstructionPreviewSounds.h"

#include "Kismet/GameplayStatics.h"


void FConstructionPreviewSounds::PlayConstructionSound(
	const UObject* WorldContextObject,
	const EConstructionPreviewSoundType SoundType) const
{
	if (!WorldContextObject)
	{
		return;
	}

	USoundBase* SoundToPlay = nullptr;

	switch (SoundType)
	{
	case EConstructionPreviewSoundType::StartPreview:
		SoundToPlay = StartPreviewSound;
		break;
	case EConstructionPreviewSoundType::Placement:
		SoundToPlay = PlacementSound;
		break;
	case EConstructionPreviewSoundType::Socket:
		SoundToPlay = SocketSound;
		break;
	case EConstructionPreviewSoundType::RotateClockWise:
		SoundToPlay = RotateClockWiseSound;
		break;
	case EConstructionPreviewSoundType::RotateCounterClockWise:
		SoundToPlay = RotateCounterClockWiseSound;
		break;
	default:
		return; // No sound to play
	}

	if (SoundToPlay)
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, SoundToPlay);
	}	
}
