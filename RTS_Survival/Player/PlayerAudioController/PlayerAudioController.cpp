// Copyright (C) Bas Blokzijl - All rights reserved.
#include "PlayerAudioController.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Audio/Pooling/RTSPooledAudio.h"
#include "RTS_Survival/Audio/Settings/RTSSpatialAudioSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPlayerAudioController::UPlayerAudioController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPlayerAudioController::BeginPlay()
{
	Super::BeginPlay();

	// Spawn an audio component on our owner
	if (AActor* Owner = GetOwner())
	{
		M_VoiceLineAudioComponent = NewObject<UAudioComponent>(Owner);
		M_VoiceLineAudioComponent->SetupAttachment(Owner->GetRootComponent());
		M_VoiceLineAudioComponent->RegisterComponent();
		M_VoiceLineAudioComponent->OnAudioFinished.AddDynamic(
			this, &UPlayerAudioController::HandleAudioFinished
		);
		M_ResourceTickAudioComponent = NewObject<UAudioComponent>(Owner);
		M_ResourceTickAudioComponent->SetupAttachment(Owner->GetRootComponent());
		M_ResourceTickAudioComponent->RegisterComponent();
		M_ResourceTickAudioComponent->bAutoActivate = false; // don’t start until spending begins
		if (IsValid(ResourceTickSettings.M_ResourceTickSound))
		{
			M_ResourceTickAudioComponent->SetSound(ResourceTickSettings.M_ResourceTickSound);
		}
		else
		{
			NoValidTickSound_SetTimer();
		}
	}
	BeginPlay_InitSpatialAudioPool();
}

void UPlayerAudioController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndPlay_ShutdownSpatialAudioPool();

	Super::EndPlay(EndPlayReason);
}


void UPlayerAudioController::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Tick_UpdateSpatialAudioPool(DeltaTime);
}


void UPlayerAudioController::PlayVoiceLine(const AActor* PrimarySelectedUnit,
                                           ERTSVoiceLine VoiceLineType,
                                           bool bForcePlay,
                                           const bool bQueueIfNotPlayed)
{
	if (VoiceLineType == ERTSVoiceLine::None)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Attempted to play None as voice line in PlayVoiceLine"));
		return;
	}
	if (bM_SuppressRegularVoiceLines)
	{
		return;
	}

	ERTSVoiceLineUnitType UnitType;
	if (not GetValidVoiceLineTypeForUnit(PrimarySelectedUnit, UnitType))
	{
		return;
	}

	AdjustVoiceLineForCombatSituation(PrimarySelectedUnit, VoiceLineType, UnitType, bForcePlay);

	USoundBase* VoiceLine = GetVoiceLineFromTypes(UnitType, VoiceLineType);
	if (not IsValid(VoiceLine))
	{
		return;
	}

	PlayAudio(VoiceLine, VoiceLineType, bForcePlay, bQueueIfNotPlayed);
}

void UPlayerAudioController::PlayAnnouncerVoiceLine(const EAnnouncerVoiceLineType Type,
                                                    const bool bQueueIfNotPlayed,
                                                    const bool InterruptRegularVoiceLines)
{
	const bool bIsCustomLine = Type == EAnnouncerVoiceLineType::Custom;
	if (bIsCustomLine)
	{
		RTSFunctionLibrary::ReportError(
			"PlayerAudioController is asked to play a custom line using the wrong function: "
			"PlayAnnouncerVoiceLine. Use PlayCustomAnnouncerVoiceLine instead.");
		return;
	}
	if (bM_SuppressRegularVoiceLines)
	{
		return;
	}
	if (not GetIsValidResourceAudioComponent())
	{
		RTSFunctionLibrary::ReportError(
			"PlayerAudioController cannot play announcer voice lines without a valid resource audio component.");
		return;
	}
	
	    USoundBase* VoiceLine = GetAnnouncerVoiceLineFromType(Type);

    UWorld* World = GetWorld();
    if (not IsValid(World) || not IsValid(VoiceLine))
    {
        return;
    }
	const float Now = GetWorld()->GetTimeSeconds();
	// if same type as last and on cooldown; do not spam the player.
	if (IsSameAnnouncerVoiceLineAsPrevious(Type) && IsAnnouncerVoiceLineOnCooldown(Type, Now))
	{
		DebugAudioController("Announcer Voiceline of same type is on cooldown.");
		return;
	}

	// If a current announcer voice line is playing then do not play nor queue.
	if (M_CurrentVoiceLineState.IsCurrentVoiceLineAnnouncerVoiceLine())
	{
		if (bQueueIfNotPlayed)
		{
			QueueAnnouncerLineIfNoAnnouncerLineIsQueued(Type, VoiceLine);
		}
		return;
	}
	if (M_CurrentVoiceLineState.IsCurrentVoiceLineRTSVoiceLine())
	{
		if (InterruptRegularVoiceLines)
		{
			// Interrupt regular voice line.
			M_VoiceLineAudioComponent->Stop();
			// Make sure the rts voice line enum is erased from state.
			M_CurrentVoiceLineState.Reset();
		}
		else
		{
			if (bQueueIfNotPlayed)
			{
				QueueAnnouncerLineIfNoAnnouncerLineIsQueued(Type, VoiceLine);
			}
			return;
		}
	}
	    // Global per-type cooldown for non-custom announcer lines.
        if (IsAnnouncerVoiceLineOnCooldown(Type, Now))
        {
            if (bQueueIfNotPlayed)
            {
                QueueAnnouncerLineIfNoAnnouncerLineIsQueued(Type, VoiceLine);
            }
            return;
        }
	// Record that this is now the “current” line
	M_CurrentVoiceLineState.SetCurrentVoiceLineAsAnnouncerVoiceLine(Type);

	// Calculate its full block‐out time
	const float NextAllowedTime = GetAnnouncerVoiceLineEndTimeAfterCooldown(Type, VoiceLine, Now);
    M_AnnouncerPlayCooldownEndTimes.Add(Type, NextAllowedTime);

	// Fire it off
	M_VoiceLineAudioComponent->SetSound(VoiceLine);
	M_VoiceLineAudioComponent->Play();
}


void UPlayerAudioController::PlayCustomAnnouncerVoiceLine(
    USoundBase* CustomVoiceLineSound,
    const bool bQueueIfNotPlayed)
{
    if (not GetIsValidVoiceLineAudioComp() || not IsValid(CustomVoiceLineSound))
    {
        return;
    }

    // If a custom announcer voice line is playing then do not play; optionally queue one extra custom line.
    if (M_CurrentVoiceLineState.IsCurrentCustomAnnouncerVoiceLine())
    {
        if (not bQueueIfNotPlayed)
        {
            return;
        }

        if (M_QueuedAnnouncerVoiceLineEntry.IsCustomVoiceLine())
        {
            RTSFunctionLibrary::ReportError(
                "Custom announcer voice line will be lost! There is already a custom line queued "
                "and so the extra line that comes up now and is attempted to be queued will be lost!");
            return;
        }

        QueueAnnouncerLineIfNoAnnouncerLineIsQueued(EAnnouncerVoiceLineType::Custom, CustomVoiceLineSound);
        return;
    }

    // Interrupt any regular announcer or RTS voice lines.
    if (M_VoiceLineAudioComponent->IsPlaying())
    {
        M_VoiceLineAudioComponent->Stop();
    }
    M_CurrentVoiceLineState.Reset();

    M_VoiceLineAudioComponent->SetSound(CustomVoiceLineSound);
    M_VoiceLineAudioComponent->Play();

    // Record that this is now the “current” line.
    M_CurrentVoiceLineState.SetCurrentVoiceLineAsAnnouncerVoiceLine(EAnnouncerVoiceLineType::Custom);
}

void UPlayerAudioController::SetSuppressRegularVoiceLines(const bool bSuppress)
{
	if (bM_SuppressRegularVoiceLines == bSuppress)
	{
		return;
	}

	bM_SuppressRegularVoiceLines = bSuppress;

	if (not bM_SuppressRegularVoiceLines)
	{
		return;
	}

	M_QueuedVoiceLineEntry.Reset();
	if (not M_QueuedAnnouncerVoiceLineEntry.IsCustomVoiceLine())
	{
		M_QueuedAnnouncerVoiceLineEntry.Reset();
	}

	if (M_CurrentVoiceLineState.IsCurrentCustomAnnouncerVoiceLine())
	{
		return;
	}

	if (not GetIsValidVoiceLineAudioComp())
	{
		return;
	}

	if (M_VoiceLineAudioComponent->IsPlaying())
	{
		M_VoiceLineAudioComponent->Stop();
	}
	M_CurrentVoiceLineState.Reset();
}

UAudioComponent* UPlayerAudioController::PlaySpatialVoiceLine(
	const AActor* PrimarySelectedUnit,
	ERTSVoiceLine VoiceLineType,
	const FVector& Location, const bool bIgnorePlayerCooldown)
{
	if (VoiceLineType == ERTSVoiceLine::None)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Attempted to play None as spatial voice line"));
		return nullptr;
	}
	if (bM_SuppressRegularVoiceLines)
	{
		return nullptr;
	}

	ERTSVoiceLineUnitType UnitType;
	if (not GetValidVoiceLineTypeForUnit(PrimarySelectedUnit, UnitType))
	{
		if constexpr (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
		{
			const FString UnitTypeString = UEnum::GetValueAsString(UnitType);
			const FString VoiceLineTypeString = UEnum::GetValueAsString(VoiceLineType);
			DrawDebugString(GetWorld(), Location + FVector(0.f, 0.f, 500.f),
				FText::FromString("Cannot play audio: " + VoiceLineTypeString +
				                  " for unit type: " + UnitTypeString).ToString() + "\n missing voice line type data.",
				                  nullptr, FColor::Red, 15.f, false, 2);
			
		}
		return nullptr;
	}

	// We do not care about force play when playing spatial voice lines.
	bool bForcePlayDummy = false;
	AdjustVoiceLineForCombatSituation(PrimarySelectedUnit, VoiceLineType, UnitType, bForcePlayDummy);

	const float Now = GetWorld()->GetTimeSeconds();
	if (not bIgnorePlayerCooldown)
	{
		const float NextAllowed = M_SpatialCooldownEndTimes.FindRef(VoiceLineType);
		if (Now < NextAllowed)
		{
			DebugAudioController("Spatial audio on Player-Cooldown, type:" + UEnum::GetValueAsString(VoiceLineType));
			return nullptr; // still cooling down for this line type
		}
	}

	// Determine the cooldown for this line type:
	// 1) Check per-type overrides in DeveloperSettings::GamePlay::Audio::SpatialVoiceLineCooldowns.
	const float* const CustomCooldownPtr =
		DeveloperSettings::GamePlay::Audio::SpatialVoiceLineCooldowns.Find(VoiceLineType);

	// 2) Fall back to CoolDownBetweenSpatialAudioOfSameType if no entry is found.
	const float SpatialCooldown = CustomCooldownPtr != nullptr
		                              ? *CustomCooldownPtr
		                              : DeveloperSettings::GamePlay::Audio::CoolDownBetweenSpatialAudioOfSameType;

	M_SpatialCooldownEndTimes.Add(
		VoiceLineType,
		Now + SpatialCooldown
	);


	USoundBase* Line = GetVoiceLineFromTypes(UnitType, VoiceLineType);
	if (not IsValid(Line))
	{
		return nullptr;
	}
	return SpawnSpatialVoiceLineOneShot(Line, Location, PrimarySelectedUnit, bIgnorePlayerCooldown);
}


USoundBase* UPlayerAudioController::GetNextVoiceLine(
	const ERTSVoiceLineUnitType UnitType,
	const ERTSVoiceLine VoiceLineType)
{
	// 1) Ensure we have data for this unit type
	if (!M_UnitVoiceLineData.Contains(UnitType))
	{
		OnCannotFindUnitType(UnitType, VoiceLineType);
		return nullptr;
	}

	// 2) Grab the real FVoiceLineData in the map
	FUnitVoiceLinesData& UnitData = M_UnitVoiceLineData[UnitType];
	FVoiceLineData* VoiceLinesPtr = nullptr;
	if (!UnitData.GetVoiceLinesForType(VoiceLineType, VoiceLinesPtr))
	{
		return nullptr;
	}

	// 3) Calling GetVoiceLine() advances the internal index and returns the asset
	return VoiceLinesPtr->GetVoiceLine();
}

void UPlayerAudioController::InitPlayerAudioPauseTimes(const TMap<ERTSVoiceLine, float> ExtraPauseTimes,
                                                       USoundConcurrency* VoiceLineSpatialConcurrency,
                                                       USoundAttenuation*
                                                       VoiceLineSpatialAttenuation,
                                                       const TMap<EAnnouncerVoiceLineType, float> PauseTimesAnnouncer)
{
	M_AudioPauseTimes = ExtraPauseTimes;
	M_VoiceLineAttenuationSettings = VoiceLineSpatialAttenuation;
	M_VoiceLineConcurrencySettings = VoiceLineSpatialConcurrency;
   // Announcer: PauseTimesAnnouncer holds the designer "extra pause" per type.
    M_AnnouncerExtraPauseTimes = PauseTimesAnnouncer;
    M_AnnouncerPlayCooldownEndTimes.Reset();
}

void UPlayerAudioController::InitUnitVoiceLine(const FRTSVoiceLineSettings NewUnitVoiceLines)
{
	if (M_UnitVoiceLineData.Contains(NewUnitVoiceLines.VoiceLineType))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Duplicate InitUnitVoiceLine for type: ") +
			UEnum::GetValueAsString(NewUnitVoiceLines.VoiceLineType));
		return;
	}

	FUnitVoiceLinesData Data;
	Data.UnitVoiceLineType = NewUnitVoiceLines.VoiceLineType;
	Data.VoiceLines = NewUnitVoiceLines.VoiceLines;
	M_UnitVoiceLineData.Add(NewUnitVoiceLines.VoiceLineType, MoveTemp(Data));
}

void UPlayerAudioController::InitAnnouncerVoiceLines(const FAnnouncerVoiceLineData AnnouncerVoiceLines)
{
	M_AnnouncerVoiceLineData = AnnouncerVoiceLines;
}

void UPlayerAudioController::InitResourceTickSound(USoundBase* ResourceTickSound, const FName& ResourceTickName,
                                                   const float ResourceTickMaxSpeed,
                                                   const float InitResourceTickSpeed)
{
	if (not IsValid(ResourceTickSound))
	{
		RTSFunctionLibrary::ReportError("no valid tick sound provided for player audio component "
			"\n On function InitResourceTickSound");
		return;
	}
	ResourceTickSettings.M_ResourceTickInitSpeed = InitResourceTickSpeed;
	ResourceTickSettings.M_ResourceTickSound = ResourceTickSound;
	ResourceTickSettings.M_ResourceTickParamName = ResourceTickName;
	ResourceTickSettings.M_ResourceTickMaxSpeed = ResourceTickMaxSpeed;
}

void UPlayerAudioController::PlayResourceTickSound(const bool bPlay, const float IntensityRequested)
{
	if (not GetIsValidResourceAudioComponent())
	{
		return;
	}

	if (bPlay)
	{
		RTSFunctionLibrary::PrintString("Intensity: " + FString::SanitizeFloat(IntensityRequested), FColor::Purple, 15);
		const float Intensity = FMath::Clamp(IntensityRequested, ResourceTickSettings.M_ResourceTickInitSpeed,
		                                     ResourceTickSettings.M_ResourceTickMaxSpeed);
		M_ResourceTickAudioComponent->SetFloatParameter(
			ResourceTickSettings.M_ResourceTickParamName,
			Intensity);
		if (M_ResourceTickAudioComponent->IsPlaying())
		{
			return;
		}
		M_ResourceTickAudioComponent->Play();
	}
	else if (M_ResourceTickAudioComponent->IsPlaying())
	{
		M_ResourceTickAudioComponent->Stop();
		// Reset tick audio for next time.
		ResourceTickSettings.M_ResourceTickSpeed = ResourceTickSettings.M_ResourceTickInitSpeed;
	}
}


void UPlayerAudioController::PlayAudio(USoundBase* VoiceLine,
                                       const ERTSVoiceLine VoiceLineType,
                                       const bool bForcePlay, bool bQueueIfNotPlayed)
{
	if (not GetIsValidVoiceLineAudioComp() || !IsValid(VoiceLine) || !GetWorld())
	{
		return;
	}
	const bool bIsPlayingAnnouncer = M_CurrentVoiceLineState.IsCurrentVoiceLineAnnouncerVoiceLine();
	if (bIsPlayingAnnouncer)
	{
		// Do not play nor queue.
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// If same type as last and on cooldown; regardless of force play do not spam the player!
	if (IsSameRTSVoiceLineAsPrevious(VoiceLineType) && IsRTSVoiceLineOnCooldown(Now))
	{
		DebugAudioController("Voiceline of same type is on cooldown.");
		return;
	}
	// If not forced, bail out if still playing anything
	if (!bForcePlay)
	{
		if (IsRTSVoiceLineOnCooldown(Now))
		{
			// Check if we want to queue the line to play after the current one finishes.
			if (bQueueIfNotPlayed)
			{
				// Only queues if no announcer line is queued.
				QueueVoiceLine(VoiceLine, VoiceLineType, bForcePlay);
			}
			return;
		}
	}
	else
	{
		// Force‐play: interrupt whatever's playing now
		if (M_VoiceLineAudioComponent->IsPlaying())
		{
			M_VoiceLineAudioComponent->Stop();
			// clean up previous state
			M_CurrentVoiceLineState.Reset();
		}
	}

	// Record that this is now the “current” line
	M_CurrentVoiceLineState.SetCurrentVoiceLineAsRTSVoiceLine(VoiceLineType);

	// Calculate its full block‐out time
	const float Duration = GetRTSVoiceLineEndTimeAfterCooldown(VoiceLineType, VoiceLine, Now);
	M_VoiceLineCooldownEndTime = Duration;

	// Fire it off
	M_VoiceLineAudioComponent->SetSound(VoiceLine);
	M_VoiceLineAudioComponent->Play();
}

void UPlayerAudioController::HandleAudioFinished()
{
	M_CurrentVoiceLineState.Reset();
	if(M_QueuedAnnouncerVoiceLineEntry.IsValid())
	{
		if(M_QueuedAnnouncerVoiceLineEntry.IsCustomVoiceLine())
		{
			PlayCustomAnnouncerVoiceLine(M_QueuedAnnouncerVoiceLineEntry.VoiceLine, false);
		}
		else
		{
			PlayAnnouncerVoiceLine(
				M_QueuedAnnouncerVoiceLineEntry.VoiceLineType, false, true);
		}
		M_QueuedAnnouncerVoiceLineEntry.Reset();
		return;
	}
	if (not M_QueuedVoiceLineEntry.IsValid())
	{
		return;
	}
	PlayAudio(
		M_QueuedVoiceLineEntry.VoiceLine,
		M_QueuedVoiceLineEntry.VoiceLineType,
		M_QueuedVoiceLineEntry.bForcePlay,
		/*bQueueIfNotPlayed=*/ false
	);
	M_QueuedVoiceLineEntry.Reset();
}

void UPlayerAudioController::QueueVoiceLine(
	USoundBase* VoiceLine,
	const ERTSVoiceLine VoiceLineType,
	const bool bForcePlay
)
{
	if (M_QueuedAnnouncerVoiceLineEntry.IsValid())
	{
		DebugAudioController("Did not queue regular voice line as announcer is queued!");
		return;
	}
	M_QueuedVoiceLineEntry.VoiceLine = VoiceLine;
	M_QueuedVoiceLineEntry.VoiceLineType = VoiceLineType;
	M_QueuedVoiceLineEntry.bForcePlay = bForcePlay;
	DebugAudioController(TEXT("Queued voice line for later playback"));
}

bool UPlayerAudioController::GetIsValidVoiceLineAudioComp() const
{
	if (IsValid(M_VoiceLineAudioComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid voice line audio component on player audio controller!"
		"\n See: UPlayerAudioController::GetIsValidVoiceLineAudioComp() for more details.");
	return false;
}

bool UPlayerAudioController::GetIsValidResourceAudioComponent() const
{
	if (IsValid(M_ResourceTickAudioComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid resource tick audio component on player audio controller!"
		"\n See: UPlayerAudioController::GetIsValidResourceAudioComponent() for more details.");
	return false;
}

float UPlayerAudioController::GetRTSVoiceLineEndTimeAfterCooldown(const ERTSVoiceLine VoiceLineType,
                                                                  const USoundBase* ValidVoiceLine,
                                                                  const float Now) const
{
	const float Duration = ValidVoiceLine->GetDuration();
	const float ExtraPause = M_AudioPauseTimes.FindRef(VoiceLineType);
	return Now + Duration + ExtraPause;
}

float UPlayerAudioController::GetAnnouncerVoiceLineEndTimeAfterCooldown(
    const EAnnouncerVoiceLineType VoiceLineType,
    const USoundBase* ValidVoiceLine,
    const float Now) const
{
    const float Duration = ValidVoiceLine != nullptr ? ValidVoiceLine->GetDuration() : 0.0f;
    const float ExtraPause = M_AnnouncerExtraPauseTimes.FindRef(VoiceLineType);
    return Now + Duration + ExtraPause;
}


bool UPlayerAudioController::GetValidVoiceLineTypeForUnit(
	const AActor* PrimarySelectedUnit,
	ERTSVoiceLineUnitType& OutVoiceLineUnitType)
{
	if (!IsValid(PrimarySelectedUnit))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Invalid primary unit provided for GetValidVoiceLine"));
		return false;
	}

	const auto* RTSComp = PrimarySelectedUnit
		->FindComponentByClass<URTSComponent>();
	if (!IsValid(RTSComp))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Invalid RTSComponent provided for GetValidVoiceLine"));
		return false;
	}

	OutVoiceLineUnitType = RTSComp->GetUnitVoiceLineType();
	if (OutVoiceLineUnitType == ERTSVoiceLineUnitType::None)
	{
		RTSFunctionLibrary::PrintString("The unit: " + PrimarySelectedUnit->GetName() +
			" has no voice line type set. " +
			"\n Forgot to set the VoiceLineUnitType in the RTSComponent?");
		return false;
	}
	return OutVoiceLineUnitType != ERTSVoiceLineUnitType::None;
}

USoundBase* UPlayerAudioController::GetVoiceLineFromTypes(
	const ERTSVoiceLineUnitType UnitType,
	const ERTSVoiceLine VoiceLineType)
{
	if (!M_UnitVoiceLineData.Contains(UnitType))
	{
		OnCannotFindUnitType(UnitType, VoiceLineType);
		return nullptr;
	}

	FUnitVoiceLinesData& UnitData = M_UnitVoiceLineData[UnitType];
	FVoiceLineData* VoiceLinesPtr = nullptr;
	if (!UnitData.GetVoiceLinesForType(VoiceLineType, VoiceLinesPtr))
	{
		return nullptr;
	}

	return VoiceLinesPtr->GetVoiceLine();
}

USoundBase* UPlayerAudioController::GetAnnouncerVoiceLineFromType(const EAnnouncerVoiceLineType VoiceLineType)
{
	if (VoiceLineType == EAnnouncerVoiceLineType::Custom)
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to find announcer voice line in hasmap for CUSTOM type this is unexpected"
			"as custom voice lines should be provided manually!");
		return nullptr;
	}
	FVoiceLineData* VoiceLinesPtr = nullptr;
	if (not M_AnnouncerVoiceLineData.GetVoiceLinesForType(VoiceLineType, VoiceLinesPtr))
	{
		return nullptr;
	}
	return VoiceLinesPtr->GetVoiceLine();
}

void UPlayerAudioController::QueueAnnouncerLineIfNoAnnouncerLineIsQueued(const EAnnouncerVoiceLineType VoiceLineType,
                                                                         USoundBase* VoiceLine)
{
	if (M_QueuedAnnouncerVoiceLineEntry.IsValid())
	{
		DebugAudioController("Did not queue announcer voice line as one (announcer line) is already queued!");
		return;
	}
	M_QueuedAnnouncerVoiceLineEntry.VoiceLineType = VoiceLineType;
	M_QueuedAnnouncerVoiceLineEntry.VoiceLine = VoiceLine;
}


void UPlayerAudioController::OnCannotFindUnitType(
	ERTSVoiceLineUnitType UnitType,
	ERTSVoiceLine VoiceLineType)
{
	const FString UnitTypeString = UEnum::GetValueAsString(UnitType);
	const FString VoiceLineTypeString = UEnum::GetValueAsString(VoiceLineType);

	RTSFunctionLibrary::ReportError(
		FString::Printf(
			TEXT("Cannot find data for unit type '%s' while requesting voice line '%s'"),
			*UnitTypeString, *VoiceLineTypeString));
}

bool UPlayerAudioController::IsRTSVoiceLineOnCooldown(const float Now)
{
	if (not GetIsValidVoiceLineAudioComp())
	{
		return false;
	}
		if (Now < M_VoiceLineCooldownEndTime)
		{
			return true;
		}
	return false;
}

bool UPlayerAudioController::IsSameRTSVoiceLineAsPrevious(const ERTSVoiceLine VoiceLineType) const
{
	return M_CurrentVoiceLineState.GetCurrentRTSVoiceLine() == VoiceLineType;
}

bool UPlayerAudioController::IsAnnouncerVoiceLineOnCooldown(
    const EAnnouncerVoiceLineType VoiceLineType,
    const float Now)
{
    if (not GetIsValidVoiceLineAudioComp())
    {
        return false;
    }

    const float* const CooldownEnd = M_AnnouncerPlayCooldownEndTimes.Find(VoiceLineType);
    if (CooldownEnd == nullptr)
    {
        return false;
    }

    return Now < *CooldownEnd;
}

bool UPlayerAudioController::IsSameAnnouncerVoiceLineAsPrevious(const EAnnouncerVoiceLineType VoiceLineType) const
{
	return M_CurrentVoiceLineState.GetCurrentAnnouncerVoiceLine() == VoiceLineType;
}

void UPlayerAudioController::DebugAudioController(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Blue);
	}
}

void UPlayerAudioController::AdjustVoiceLineForCombatSituation(const AActor* PrimarySelectedUnit,
                                                               ERTSVoiceLine& OutVoiceLineType,
                                                               const ERTSVoiceLineUnitType OutVoiceLineUnitType,
                                                               bool& OutOverrideForcePlay)
{
	const URTSComponent* RTSComp = PrimarySelectedUnit->FindComponentByClass<URTSComponent>();
	const UHealthComponent* HealthComponent = PrimarySelectedUnit->FindComponentByClass<UHealthComponent>();
	if (not IsValid(RTSComp) || not IsValid(HealthComponent))
	{
		return;
	}
	const bool bIsInCombat = RTSComp->GetIsUnitInCombat();
	const bool bIsDamaged = HealthComponent->GetHealthPercentage() <= 0.8f;
	// For selection voice line use repair request if damaged and not in combat. If in combat use Stressed otherwise
	// use the regular selection voice line.
	if (OutVoiceLineType == ERTSVoiceLine::Select)
	{
		DetermineSelectionVoiceLine(
			OutVoiceLineUnitType,
			bIsInCombat, bIsDamaged, OutVoiceLineType, OutOverrideForcePlay
		);
		return;
	}
	// For the attack voice line either use regular, excited or stressed depending on damage and combat.
	if (OutVoiceLineType == ERTSVoiceLine::Attack)
	{
		DetermineAttackVoiceLine(bIsInCombat, bIsDamaged, OutVoiceLineType);
		return;
	}
	// If the unit is in combat get the stressed voice line.
	if (bIsInCombat)
	{
		OutVoiceLineType = FRTS_VoiceLineHelpers::GetStressedVoiceLineVersion(OutVoiceLineType);
	}
}

void UPlayerAudioController::DetermineSelectionVoiceLine(
	const ERTSVoiceLineUnitType UnitType,
	const bool bIsInCombat,
	const bool bIsDamaged,
	ERTSVoiceLine& OutVoiceLineType,
	bool& OutOverrideForcePlay)
{
	const float Now = GetWorld()->GetTimeSeconds();
	float& WindowStart = M_SelectionWindowStartTimes.FindOrAdd(UnitType);
	int32& Count = M_SelectionCounts.FindOrAdd(UnitType);

	if (not bIsInCombat && bIsDamaged)
	{
		OutVoiceLineType = ERTSVoiceLine::SelectNeedRepairs;
		DebugAudioController(TEXT("Selection: NeedRepairs"));
		return;
	}

	if (bIsInCombat)
	{
		OutVoiceLineType = ERTSVoiceLine::SelectStressed;
		DebugAudioController(TEXT("Selection: Stressed"));
		return;
	}

	// Quick‐select window logic
	if (Now - WindowStart > SelectionAnnoyWindow)
	{
		WindowStart = Now;
		Count = 1;
	}
	else
	{
		++Count;
		if (Count >= SelectionAnnoyThreshold)
		{
			OutVoiceLineType = ERTSVoiceLine::SelectAnnoyed;
			DebugAudioController(TEXT("Selection: Annoyed!"));
			// Force play annoyed voice line!
			OutOverrideForcePlay = true;
			// reset for next cycle
			Count = 0;
			WindowStart = Now;
			return;
		}
	}

	OutVoiceLineType = ERTSVoiceLine::Select;
	DebugAudioController(FString::Printf(TEXT("Selection count %d"), Count));
}

void UPlayerAudioController::DetermineAttackVoiceLine(const bool bIsInCombat, const bool bIsDamaged,
                                                      ERTSVoiceLine& OutVoiceLineType) const
{
	if (bIsInCombat && bIsDamaged)
	{
		DebugAudioController("Unit attack voice line Stressed");
		OutVoiceLineType = ERTSVoiceLine::AttackStressed;
		return;
	}
	if (bIsInCombat)
	{
		DebugAudioController("Unit attack voice line Excited");
		OutVoiceLineType = ERTSVoiceLine::AttackExcited;
	}
}

UAudioComponent* UPlayerAudioController::SpawnSpatialVoiceLineOneShot(
	USoundBase* VoiceLine,
	const FVector& Location,
	const AActor* Owner,
	const bool bIsForceRequest)
{
	if (not IsValid(VoiceLine))
	{
		return nullptr;
	}

	UAudioComponent* AudioComp = AcquirePooledSpatialAudioComponent(bIsForceRequest);
	if (AudioComp == nullptr)
	{
		// For non-forced requests, this just means the pool is exhausted.
		DebugAudioController("Audio Spatial Pool Exhausted!");
		return nullptr;
	}

	AudioComp->SetSound(VoiceLine);

	if (IsValid(Owner))
	{
		AudioComp->SetWorldLocation(Location);
		AudioComp->SetWorldRotation(Owner->GetActorRotation());
	}
	else
	{
		AudioComp->SetWorldLocation(Location);
	}

	return AudioComp;
}

void UPlayerAudioController::OnValidResourceSound() const
{
	if (GetIsValidResourceAudioComponent())
	{
		M_ResourceTickAudioComponent->SetSound(ResourceTickSettings.M_ResourceTickSound);
		// Set basic intensity.
		M_ResourceTickAudioComponent->SetFloatParameter(
			ResourceTickSettings.M_ResourceTickParamName,
			ResourceTickSettings.M_ResourceTickInitSpeed
		);
	}
}

void UPlayerAudioController::NoValidTickSound_SetTimer()
{
	// Wait for the resource tick sound to be set.
	TWeakObjectPtr<UPlayerAudioController> WeakThis(this);
	auto CheckTickSound = [WeakThis]()-> void
	{
		if (WeakThis.IsValid() && WeakThis->ResourceTickSettings.M_ResourceTickSound)
		{
			WeakThis->OnValidResourceSound();
			if (const UWorld* World = WeakThis->GetWorld())
			{
				World->GetTimerManager().ClearTimer(WeakThis->ResourceTickSettings.M_ResourceTickSoundInitHandle);
			}
			return;
		}
		if (WeakThis.IsValid())
		{
			RTSFunctionLibrary::ReportError(
				"Still no valid Resource tick sound set on PlayerAudioController trying again in 1 second.");
		}
	};
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ResourceTickSettings.M_ResourceTickSoundInitHandle,
			FTimerDelegate::CreateLambda(CheckTickSound),
			1.0f, true
		);
	}
}

void UPlayerAudioController::BeginPlay_InitSpatialAudioPool()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UPlayerAudioController::BeginPlay_InitSpatialAudioPool - World is null."));
		return;
	}

	const URTSSpatialAudioSettings* Settings = GetDefault<URTSSpatialAudioSettings>();
	if (Settings == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"URTSSpatialAudioSettings missing. Ensure DefaultEngine.ini has [/Script/RTS_Survival.RTSSpatialAudioSettings]."));
		return;
	}

	const int32 PoolSize = FMath::Max(1, Settings->DefaultPoolSize);
	M_DefaultSpatialAttenuation = Settings->DefaultSpatialAttenuation.LoadSynchronous();
	if (not IsValid(M_DefaultSpatialAttenuation))
	{
		RTSFunctionLibrary::ReportWarning(TEXT(
			"URTSSpatialAudioSettings::DefaultSpatialAttenuation is not set; "
			"pooled spatial audio will not use a default attenuation override."));
	}

	Init_CreateSpatialAudioPoolOwner();
	if (not GetIsValidSpatialAudioPoolOwner())
	{
		return;
	}

	Init_SpawnSpatialAudioPool(PoolSize);
}

void UPlayerAudioController::EndPlay_ShutdownSpatialAudioPool()
{
	for (FRTSPooledSpatialAudioInstance& Instance : M_SpatialAudioPoolInstances)
	{
		if (IsValid(Instance.Component))
		{
			Instance.Component->Stop();
			Instance.Component->DestroyComponent();
		}
	}
	M_SpatialAudioPoolInstances.Empty();
	M_SpatialAudioFreeList.Reset();
	M_SpatialAudioActiveList.Reset();

	if (IsValid(M_SpatialAudioPoolOwner))
	{
		M_SpatialAudioPoolOwner->Destroy();
		M_SpatialAudioPoolOwner = nullptr;
	}

	M_DefaultSpatialAttenuation = nullptr;
}


void UPlayerAudioController::Init_CreateSpatialAudioPoolOwner()
{
	if (IsValid(M_SpatialAudioPoolOwner))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Init_CreateSpatialAudioPoolOwner: World is null."));
		return;
	}

	FActorSpawnParameters Params;
	Params.Name = TEXT("RTS_PooledSpatialAudioOwner");
	Params.ObjectFlags = RF_Transient;

	M_SpatialAudioPoolOwner = World->SpawnActor<ARTSPooledAudio>(
		ARTSPooledAudio::StaticClass(),
		FTransform::Identity,
		Params);

	if (not IsValid(M_SpatialAudioPoolOwner))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to spawn ARTSPooledAudio."));
	}
}

void UPlayerAudioController::Init_SpawnSpatialAudioPool(const int32 PoolSize)
{
	if (not GetIsValidSpatialAudioPoolOwner())
	{
		return;
	}

	M_SpatialAudioPoolInstances.SetNum(PoolSize, EAllowShrinking::No);
	M_SpatialAudioFreeList.Reset();
	M_SpatialAudioFreeList.Reserve(PoolSize);
	M_SpatialAudioActiveList.Reset();
	M_SpatialAudioActiveList.Reserve(PoolSize);

	USceneComponent* RootComp = M_SpatialAudioPoolOwner->GetRootComponent();

	for (int32 i = 0; i < PoolSize; ++i)
	{
		UAudioComponent* AudioComp = NewObject<UAudioComponent>(M_SpatialAudioPoolOwner);
		if (not IsValid(AudioComp))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Spatial audio pool: Failed to create UAudioComponent for pool index %d."), i));
			continue;
		}

		// [CRITICAL] This ensures components are kept alive after playing
		AudioComp->bAutoActivate = false;
		AudioComp->bAutoDestroy = false;
		AudioComp->bStopWhenOwnerDestroyed = true;
		AudioComp->SetUISound(false);

		// Default attenuation + from existing state.
		if (IsValid(M_DefaultSpatialAttenuation))
		{
			AudioComp->AttenuationSettings = M_DefaultSpatialAttenuation;
		}
		if (IsValid(RootComp))
		{
			AudioComp->AttachToComponent(
				RootComp,
				FAttachmentTransformRules::KeepRelativeTransform);
		}

		AudioComp->RegisterComponent();

		FRTSPooledSpatialAudioInstance NewInst;
		NewInst.Component = AudioComp;
		NewInst.bIsActive = false;
		NewInst.StartTimeSeconds = -1.0f;

		M_SpatialAudioPoolInstances[i] = NewInst;
		M_SpatialAudioFreeList.Add(i);
	}
}

bool UPlayerAudioController::GetIsValidSpatialAudioPoolOwner() const
{
	if (IsValid(M_SpatialAudioPoolOwner))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT(
		"UPlayerAudioController::M_SpatialAudioPoolOwner was not created. "
		"Check BeginPlay_InitSpatialAudioPool / Init_CreateSpatialAudioPoolOwner."));
	return false;
}

int32 UPlayerAudioController::FindOldestActiveSpatialAudioIndex() const
{
	int32 OldestIndex = INDEX_NONE;
	float OldestTime = TNumericLimits<float>::Max();

	for (const int32 PoolIndex : M_SpatialAudioActiveList)
	{
		if (!M_SpatialAudioPoolInstances.IsValidIndex(PoolIndex))
		{
			continue;
		}

		const FRTSPooledSpatialAudioInstance& Instance = M_SpatialAudioPoolInstances[PoolIndex];
		if (!Instance.bIsActive || Instance.StartTimeSeconds < 0.0f)
		{
			continue;
		}
		if (Instance.StartTimeSeconds < OldestTime)
		{
			OldestTime = Instance.StartTimeSeconds;
			OldestIndex = PoolIndex;
		}
	}
	return OldestIndex;
}

int32 UPlayerAudioController::AllocateSpatialAudioIndex(const bool bIsForceRequest)
{
	if (M_SpatialAudioPoolInstances.Num() <= 0)
	{
		return INDEX_NONE;
	}

	if (M_SpatialAudioFreeList.Num() > 0)
	{
		const int32 PoolIndex = M_SpatialAudioFreeList.Pop(EAllowShrinking::No);
		return PoolIndex;
	}

	if (not bIsForceRequest)
	{
		// Pool exhausted and we are not allowed to interrupt anyone: just do not play.
		return INDEX_NONE;
	}

	const int32 OldestIndex = FindOldestActiveSpatialAudioIndex();
	if (OldestIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// Force interrupt the oldest active instance; we will immediately reuse its component.
	FRTSPooledSpatialAudioInstance& Instance = M_SpatialAudioPoolInstances[OldestIndex];
	if (IsValid(Instance.Component))
	{
		Instance.Component->Stop();
		Instance.Component->SetSound(nullptr);
	}
	Instance.bIsActive = false;
	Instance.StartTimeSeconds = -1.0f;

	// Keep it in the active list; AcquirePooledSpatialAudioComponent will ensure it stays unique.
	return OldestIndex;
}


void UPlayerAudioController::MarkSpatialIndexDormant(const int32 Index)
{
	if (!M_SpatialAudioPoolInstances.IsValidIndex(Index))
	{
		return;
	}

	FRTSPooledSpatialAudioInstance& Instance = M_SpatialAudioPoolInstances[Index];
	if (!Instance.bIsActive)
	{
		// Already dormant, nothing to do.
		return;
	}

	// [New Logic] Debug visualization when returning to pool
	if constexpr (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
	{
		if (IsValid(Instance.Component) && GetWorld())
		{
			// Draw "Free" text at the location where the sound just finished
			DrawDebugString(
				GetWorld(),
				Instance.Component->GetComponentLocation(),
				FString::Printf(TEXT("Pool Freed: %d"), Index),
				nullptr,
				FColor::Green,
				1.5f // Duration
			);
		}
	}

	Instance.bIsActive = false;
	Instance.StartTimeSeconds = -1.0f;

	if (IsValid(Instance.Component))
	{
		Instance.Component->SetSound(nullptr);
	}

	const int32 ActiveIdx = M_SpatialAudioActiveList.Find(Index);
	if (ActiveIdx != INDEX_NONE)
	{
		M_SpatialAudioActiveList.RemoveAtSwap(ActiveIdx, 1, EAllowShrinking::No);
	}

	M_SpatialAudioFreeList.Add(Index);
}


void UPlayerAudioController::Tick_UpdateSpatialAudioPool(const float DeltaTime)
{
	if (M_SpatialAudioActiveList.Num() == 0)
	{
		return;
	}

	// Iterate backwards so we can safely remove from the active list via MarkSpatialIndexDormant
	for (int32 ActiveListIndex = M_SpatialAudioActiveList.Num() - 1; ActiveListIndex >= 0; --ActiveListIndex)
	{
		const int32 PoolIndex = M_SpatialAudioActiveList[ActiveListIndex];
		if (!M_SpatialAudioPoolInstances.IsValidIndex(PoolIndex))
		{
			M_SpatialAudioActiveList.RemoveAtSwap(ActiveListIndex, 1, EAllowShrinking::No);
			continue;
		}

		FRTSPooledSpatialAudioInstance& PooledInstance = M_SpatialAudioPoolInstances[PoolIndex];
		if (!PooledInstance.bIsActive)
		{
			M_SpatialAudioActiveList.RemoveAtSwap(ActiveListIndex, 1, EAllowShrinking::No);
			continue;
		}

		// [Logic Check] If component is invalid OR has stopped playing, recycle it.
		if (!IsValid(PooledInstance.Component) || !PooledInstance.Component->IsPlaying())
		{
			// This will trigger the debug string and return it to M_SpatialAudioFreeList
			MarkSpatialIndexDormant(PoolIndex);
		}
	}
}


UAudioComponent* UPlayerAudioController::AcquirePooledSpatialAudioComponent(const bool bIsForceRequest)
{
	const int32 PoolIndex = AllocateSpatialAudioIndex(bIsForceRequest);
	if (PoolIndex == INDEX_NONE)
	{
		return nullptr;
	}

	if (!M_SpatialAudioPoolInstances.IsValidIndex(PoolIndex))
	{
		return nullptr;
	}

	FRTSPooledSpatialAudioInstance& Instance = M_SpatialAudioPoolInstances[PoolIndex];
	if (!IsValid(Instance.Component))
	{
		RTSFunctionLibrary::ReportError(TEXT("Spatial audio pool entry has invalid UAudioComponent."));
		return nullptr;
	}

	Instance.bIsActive = true;

	if (UWorld* World = GetWorld())
	{
		Instance.StartTimeSeconds = World->GetTimeSeconds();
	}
	else
	{
		Instance.StartTimeSeconds = 0.0f;
	}

	if (M_SpatialAudioActiveList.Find(PoolIndex) == INDEX_NONE)
	{
		M_SpatialAudioActiveList.Add(PoolIndex);
	}

	return Instance.Component;
}
