#pragma once

#include "CoreMinimal.h"

#include "RTSMusicTypes.generated.h"


UENUM(BlueprintType)
enum class ERTSMusicType: uint8
{
	None,
	MainTheme,
	GameStart,
	GameTrackAlternative,
	// Plays a specific action track.
	Combat,
	// Plays looping action tracks.
	CombatLoop,
	AttackWave,
	ClearZone,
	BriefingLoop,
	LoadingLoop,
	PrepareLoop,
	TensionLoop,
	Victory,
	Defeat
	
};


// Contains the music tracks for each type of music.
USTRUCT(BlueprintType)
struct FRTSMusicTypes
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSMusicType MusicType = ERTSMusicType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<USoundBase>> MusicTracks;
};
