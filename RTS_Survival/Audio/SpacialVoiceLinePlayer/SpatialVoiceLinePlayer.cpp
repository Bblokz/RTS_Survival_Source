// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SpatialVoiceLinePlayer.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

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
	if (not BeginPlay_CheckEnabledForSpatialAudio())
	{
		return;
	}

	BeginPlay_SetupControllerReference();
}

void USpatialVoiceLinePlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_SpatialCooldown);
		World->GetTimerManager().ClearTimer(TimerHandle_SpatialStagger);
	}

	M_LastSpacialAudio.Reset();
	M_PlayerController.Reset();
	Super::EndPlay(EndPlayReason);
}

bool USpatialVoiceLinePlayer::BeginPlay_CheckEnabledForSpatialAudio()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return false;
	}
	const URTSComponent* RTSComp = Owner->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComp))
	{
		return false;
	}

	// Only enable if owned by local player (player index 1).
	bIsEnabledForSpatialAudio = (RTSComp->GetOwningPlayer() == 1);
	return bIsEnabledForSpatialAudio;
}

void USpatialVoiceLinePlayer::OverrideAttenuation(UAudioComponent* AudioComp) const
{
	if (not IsValid(AudioComp))
	{
		return;
	}

	if (IsValid(OverrideAttenuationSettings))
	{
		AudioComp->AttenuationSettings = OverrideAttenuationSettings;
	}

}

void USpatialVoiceLinePlayer::BeginPlay_SetupControllerReference()
{
	M_PlayerController = FRTS_Statics::GetRTSController(this);
}

bool USpatialVoiceLinePlayer::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("GetIsValidPlayerController"),
		this);
	return false;
}

bool USpatialVoiceLinePlayer::GetIsValidOwnerForAudioRequest(const FString& FunctionName) const
{
	if (IsValid(GetOwner()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("Owner"),
		FunctionName,
		this);
	return false;
}

UWorld* USpatialVoiceLinePlayer::GetValidWorldForTimer(const FString& FunctionName) const
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		return World;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("World"),
		FunctionName,
		this);
	return nullptr;
}

void USpatialVoiceLinePlayer::PlaySpatialVoiceLine(
	ERTSVoiceLine VoiceLineType,
	const FVector& Location, const bool bIgnorePlayerCooldown)
{
	if (not bIsEnabledForSpatialAudio || bIsOnSpatialCooldown || not GetIsValidPlayerController())
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

	// 1) Start spam cooldown immediately
	UWorld* World = GetValidWorldForTimer(TEXT("PlaySpatialVoiceLine"));
	if (not IsValid(World))
	{
		return;
	}

	float FluxPercentage = 0.f;
	const float BaseInterval = GetBaseIntervalBetweenSpatialVoiceLines(FluxPercentage);
	const float RandomPercentage = FMath::FRandRange(-FluxPercentage, FluxPercentage);
	const float CooldownMultiplier = 1.f + RandomPercentage * 0.01f;
	const float CooldownTime = FMath::Max(0.01f, BaseInterval * CooldownMultiplier);

	bIsOnSpatialCooldown = true;
	World->GetTimerManager().SetTimer(
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
	if (not bIsEnabledForSpatialAudio || not GetIsValidPlayerController())
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
	if (not IsValid(SpatialAudio))
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
	if (not GetIsValidPlayerController() || not bIsEnabledForSpatialAudio || not GetIsValidOwnerForAudioRequest(TEXT("PlayVoiceLineOverRadio")))
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
	if (not GetIsValidPlayerController() || not GetIsValidOwnerForAudioRequest(TEXT("ExecuteSpatialVoiceLine")))
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
	
	if (not IsValid(GetOwner()))
	{
		return;
	}
	const URTSComponent* RTSComp = GetOwner()->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComp))
	{
		return;
	}
	if (RTSComp->GetOwningPlayer() > 0)
	{
		OverrideAttenuationSettings = PlayerSoundUnitAttenuationSettings;
	}
	else
	{
		OverrideAttenuationSettings = EnemySoundUnitAttenuationSettings;
	}
}

FString USpatialVoiceLinePlayer::GetOwnerTypeNameForAttenuationError(
	AActor* Owner,
	bool& bOutValidOwnerType) const
{
	bOutValidOwnerType = false;

	ASquadUnit* OwningSquadUnit = Cast<ASquadUnit>(Owner);
	if (IsValid(OwningSquadUnit) && IsValid(OwningSquadUnit->GetRTSComponent()))
	{
		return OwningSquadUnit->GetRTSComponent()->GetDisplayName(bOutValidOwnerType);
	}

	ATankMaster* OwningTank = Cast<ATankMaster>(Owner);
	if (IsValid(OwningTank) && IsValid(OwningTank->GetRTSComponent()))
	{
		return OwningTank->GetRTSComponent()->GetDisplayName(bOutValidOwnerType);
	}

	return TEXT("Unknown Type");
}

void USpatialVoiceLinePlayer::BeginPlay_ReportErrorIfnoAttenuation() const
{
	if (IsValid(OverrideAttenuationSettings))
	{
		return;
	}

	AActor* Owner = GetOwner();
	const FString OwnerName = IsValid(Owner) ? Owner->GetName() : TEXT("Unknown Owner");
	bool bValidOwnerType = false;
	FString OwnerType = TEXT("Unknown Type");
	if (IsValid(Owner))
	{
		OwnerType = GetOwnerTypeNameForAttenuationError(Owner, bValidOwnerType);
	}

	if (not bValidOwnerType)
	{
		OwnerType = TEXT("RTSCOMPONENT INVALID NAME");
	}

	RTSFunctionLibrary::ReportError(
		TEXT("No spatial attenuation settings set on SpatialVoiceLinePlayer component on ")
		+ OwnerName + TEXT("\n Type: ") + OwnerType);
}
