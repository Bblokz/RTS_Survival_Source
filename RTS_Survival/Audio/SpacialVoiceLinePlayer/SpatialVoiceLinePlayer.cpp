// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SpatialVoiceLinePlayer.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

using namespace DeveloperSettings::GamePlay::Audio;

USpatialVoiceLinePlayer::USpatialVoiceLinePlayer()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USpatialVoiceLinePlayer::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_SetupAttenuationOverride();
	BeginPlay_ReportErrorIfnoAttenuation();
	if (BeginPlay_CheckEnabledForSpatialAudio())
	{
		BeginPlay_SetupControllerReference();
	}
}

bool USpatialVoiceLinePlayer::BeginPlay_CheckEnabledForSpatialAudio()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	const URTSComponent* RTSComp = Owner->FindComponentByClass<URTSComponent>();
	if (!RTSComp)
	{
		return false;
	}

	// Only enable if owned by local player (player index 1).
	bIsEnabledForSpatialAudio = (RTSComp->GetOwningPlayer() == 1);
	return bIsEnabledForSpatialAudio;
}

void USpatialVoiceLinePlayer::OverrideAttenuation(UAudioComponent* AudioComp) const
{
	if (!IsValid(AudioComp))
	{
		return;
	}

	if (OverrideAttenuationSettings)
	{
		AudioComp->AttenuationSettings = OverrideAttenuationSettings;
	}

}

void USpatialVoiceLinePlayer::BeginPlay_SetupControllerReference()
{
	M_PlayerController = FRTS_Statics::GetRTSController(this);
}

bool USpatialVoiceLinePlayer::EnsureControllerIsValid() const
{
	if (!M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("No valid controller found for spatial voice line player; ")
			TEXT("component likely on a non-owned unit."));
		return false;
	}
	return true;
}

void USpatialVoiceLinePlayer::PlaySpatialVoiceLine(
	ERTSVoiceLine VoiceLineType,
	const FVector& Location, const bool bIgnorePlayerCooldown)
{
	if (not bIsEnabledForSpatialAudio || bIsOnSpatialCooldown || not EnsureControllerIsValid())
	{
		return;
	}
	
	// Probability check.
	if (not ShouldPlaySpatialVoiceLine(VoiceLineType))
	{
		Debug_SpatialNotAllowed(VoiceLineType, Location);
		return;
	}
	Debug_SpatialAllowed(VoiceLineType, Location);

	// if(GetOwner())
	// {
	// 	ASquadUnit* OwningSquadUnit = Cast<ASquadUnit>(GetOwner());
	// 	if(OwningSquadUnit)
	// 	{
	// 		if(OwningSquadUnit->GetRTSComponent())
	// 		{
	// 			if(OwningSquadUnit->GetRTSComponent()->GetOwningPlayer() != 1)
	// 			{
	// 				RTSFunctionLibrary::PrintString("enemy vl!");
	// 			}
	// 		}
	// 	}
	// }

	// 1) Start spam cooldown immediately
	float FluxPct = 0.f;
	const float Base = GetBaseIntervalBetweenSpatialVoiceLines(FluxPct);
	const float RandPct = FMath::FRandRange(-FluxPct, FluxPct);
	const float Multi = 1.f + RandPct * 0.01f;
	const float CooldownTime = Base * Multi;

	bIsOnSpatialCooldown = true;
	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_SpatialCooldown,
		this,
		&USpatialVoiceLinePlayer::ResetSpatialCooldown,
		CooldownTime,
		false
	);

	UAudioComponent* SpatialAudio = ExecuteSpatialVoiceLine(
		VoiceLineType,
		Location,
		bIgnorePlayerCooldown
	);

	HandleNewSpatialAudio(SpatialAudio);
}

void USpatialVoiceLinePlayer::ForcePlaySpatialVoiceLine(
	const ERTSVoiceLine VoiceLineType,
	const FVector& Location)
{
	if (not bIsEnabledForSpatialAudio || not EnsureControllerIsValid())
	{
		return;
	}

	// Disregard player cooldown per vlType.
	constexpr bool bIgnorePlayerCooldown = true;
	Debug_ForceSpatialAudio(VoiceLineType, Location);

	UAudioComponent* SpatialAudio = ExecuteSpatialVoiceLine(
		VoiceLineType,
		Location,
		bIgnorePlayerCooldown
	);

	HandleNewSpatialAudio(SpatialAudio);
}


void USpatialVoiceLinePlayer::HandleNewSpatialAudio(UAudioComponent* SpatialAudio)
{
	// Only save the weak reference to the spatial audio if it could actually be played.
	if (!SpatialAudio)
	{
		return;
	}

	M_LastSpacialAudio = SpatialAudio;

	// Apply per-unit overrides (attenuation / concurrency) on the pooled component.
	OverrideAttenuation(SpatialAudio);

		SpatialAudio->Play(0.f);
}

void USpatialVoiceLinePlayer::PlayVoiceLineOverRadio(const ERTSVoiceLine VoiceLineType, const bool bForcePlay,
                                                     const bool bQueueIfNotPlayed) const
{
	if (not EnsureControllerIsValid() || not bIsEnabledForSpatialAudio)
	{
		return;
	}
	M_PlayerController->PlayVoiceLine(
		GetOwner(),
		VoiceLineType,
		bForcePlay,
		bQueueIfNotPlayed
	);
}

void USpatialVoiceLinePlayer::ResetSpatialCooldown()
{
	bIsOnSpatialCooldown = false;
}

UAudioComponent* USpatialVoiceLinePlayer::ExecuteSpatialVoiceLine(
	const ERTSVoiceLine VoiceLineType,
	const FVector& Location,
	const bool bIgnorePlayerCooldown)
{
	if (not EnsureControllerIsValid())
	{
		return nullptr;
	}

	return M_PlayerController->PlaySpatialVoiceLine(
		GetOwner(),
		VoiceLineType,
		Location,
		bIgnorePlayerCooldown
	);
}

bool USpatialVoiceLinePlayer::ShouldPlaySpatialVoiceLine(ERTSVoiceLine VoiceLineType) const
{
	const auto& Probs = DeveloperSettings::GamePlay::Audio::SpatialVoiceLinePlayProbability;
	if (const float* Chance = Probs.Find(VoiceLineType))
	{
		return FMath::FRand() <= *Chance;
	}
	if (const UEnum* EnumPtr = StaticEnum<ERTSVoiceLine>())
	{
		const FString EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(VoiceLineType));
		RTSFunctionLibrary::ReportError("Did not find probability for spatial audio in the mapping. Type: " + EnumName);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Did not find probability for spatial audio, and could not retrieve enum name.");
	}

	return true;
}

float USpatialVoiceLinePlayer::GetBaseIntervalBetweenSpatialVoiceLines(float& OutFluxPercentage) const
{
	OutFluxPercentage = FluxPercentageIntervalBetweenSpatialVoiceLines;
	return BaseIntervalBetweenSpatialVoiceLines;
	
}

void USpatialVoiceLinePlayer::Debug_SpatialNotAllowed(const ERTSVoiceLine& VoiceLine, const FVector& Location) const
{
	if constexpr (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
	{
		FString EnumName = "Enum Ptr NULL";
		if (const UEnum* EnumPtr = StaticEnum<ERTSVoiceLine>())
		{
			EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(VoiceLine));
		}
		if (const UWorld* World = GetWorld())
		{
			DrawDebugString(World, Location + FVector(0.f, 0.f, 300.f),
			                FString::Printf(TEXT("Spatial audio blocked by probability %s"), *EnumName),
			                nullptr,
			                FColor::Red,
			                5.f,
			                true,
			                1.5f
			);
		}
	}
}

void USpatialVoiceLinePlayer::Debug_SpatialAllowed(const ERTSVoiceLine& VoiceLine, const FVector& Location) const
{
	if constexpr (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
	{
		FString EnumName = "Enum Ptr NULL";
		if (const UEnum* EnumPtr = StaticEnum<ERTSVoiceLine>())
		{
			EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(VoiceLine));
		}
		if (const UWorld* World = GetWorld())
		{
			DrawDebugString(World, Location + FVector(0.f, 0.f, 400.f),
			                FString::Printf(TEXT("Play Spatial Audio %s"), *EnumName),
			                nullptr,
			                FColor::Blue,
			                5.f,
			                true,
			                1.5f
			);
		}
	}
}

void USpatialVoiceLinePlayer::Debug_ForceSpatialAudio(const ERTSVoiceLine& VoiceLine, const FVector& Location) const
{
	
	if constexpr (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
	{
		FString EnumName = "Enum Ptr NULL";
		if (const UEnum* EnumPtr = StaticEnum<ERTSVoiceLine>())
		{
			EnumName = EnumPtr->GetNameStringByValue(static_cast<int64>(VoiceLine));
		}
		if (const UWorld* World = GetWorld())
		{
			DrawDebugString(World, Location + FVector(0.f, 0.f, 400.f),
			                FString::Printf(TEXT("FORCE Spatial Audio %s"), *EnumName),
			                nullptr,
			                FColor::Purple,
			                5.f,
			                true,
			                1.5f
			);
		}
	}
}

void USpatialVoiceLinePlayer::BeginPlay_SetupAttenuationOverride()
{
	
	if(not IsValid(GetOwner()))
	{
		return;
	}
	const URTSComponent* RTSComp = GetOwner()->FindComponentByClass<URTSComponent>();
	if(not IsValid(RTSComp))
	{
		return;
	}
	if(RTSComp->GetOwningPlayer() > 0 )
	{
		OverrideAttenuationSettings = PlayerSoundUnitAttenuationSettings;
	}
	else
	{
		OverrideAttenuationSettings = EnemySoundUnitAttenuationSettings;
	}
}

void USpatialVoiceLinePlayer::BeginPlay_ReportErrorIfnoAttenuation() const
{
	if(not OverrideAttenuationSettings)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("Unknown Owner");
				bool bValid = false;
		FString OwnerType = "Unknown Type";
		if(GetOwner())
		{
			ASquadUnit* OwningSquadUnit = Cast<ASquadUnit>(GetOwner());
			if(OwningSquadUnit && OwningSquadUnit->GetRTSComponent())
			{
				OwnerType = OwningSquadUnit->GetRTSComponent()->GetDisplayName(bValid);	
			}
			else
			{
				ATankMaster* OwningTank = Cast<ATankMaster>(GetOwner());
				if(OwningTank && OwningTank->GetRTSComponent())
				{
					OwnerType = OwningTank->GetRTSComponent()->GetDisplayName(bValid);
				}
			}
			
		}
		if(not bValid)
		{
			OwnerType = "RTSCOMPONENT INVALID NAME";
		}
		RTSFunctionLibrary::ReportError(
			TEXT("No spatial attenuation settings set on SpatialVoiceLinePlayer component on ")
			+ OwnerName + "\n Type: " + OwnerType);
	}
}
