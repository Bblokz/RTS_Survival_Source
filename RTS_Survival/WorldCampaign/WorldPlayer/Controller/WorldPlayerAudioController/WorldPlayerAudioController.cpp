// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldPlayerAudioController.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UWorldPlayerAudioController::UWorldPlayerAudioController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldPlayerAudioController::PlayUISound(USoundBase* Sound) const
{
	if (not IsValid(Sound))
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Sound);
}
