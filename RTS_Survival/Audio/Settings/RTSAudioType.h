// RTSAudioType.h
#pragma once

#include "CoreMinimal.h"

#include "RTSAudioType.generated.h"

UENUM(BlueprintType)
enum class ERTSAudioType : uint8
{
	SFXAndWeapons UMETA(DisplayName="SFX & Weapons"),
	Music UMETA(DisplayName="Music"),
	Voicelines UMETA(DisplayName="Voicelines"),
	Announcer UMETA(DisplayName="Announcer"),
	TransmissionsAndCinematics UMETA(DisplayName="Transmissions & Cinematics"),
	UI UMETA(DisplayName="UI")
};
