#include "MissionCommandVehicleWarningState.h"

#include "Algo/RandomShuffle.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"

void FMissionCommandVehicleWarningState::Init(
	ATankMaster* CommandVehicle,
	const float HealthPercentageTrigger,
	const float TimeBetweenTriggers,
	const FMissionCommandVehicleWarningPortraitSettings& PortraitSettings,
	UNiagaraSystem* OneShotVFX,
	const FVector& CommanderOffset,
	const FVector& VFXScale,
	USoundBase* OneShotSound,
	USoundAttenuation* SoundAttenuation,
	USoundConcurrency* SoundConcurrency)
{
	Reset();

	constexpr float MinimumHealthPercentage = 0.0f;
	constexpr float MaximumHealthPercentage = 1.0f;
	constexpr float MinimumTriggerInterval = 0.0f;
	M_CommandVehicle = CommandVehicle;
	M_HealthPercentageTrigger = FMath::Clamp(
		HealthPercentageTrigger,
		MinimumHealthPercentage,
		MaximumHealthPercentage);
	M_TimeBetweenTriggers = FMath::Max(MinimumTriggerInterval, TimeBetweenTriggers);
	M_PortraitType = PortraitSettings.PortraitType;
	M_OneShotVFX = OneShotVFX;
	M_CommanderOffset = CommanderOffset;
	M_VFXScale = VFXScale;
	M_OneShotSound = OneShotSound;
	M_SoundAttenuation = SoundAttenuation;
	M_SoundConcurrency = SoundConcurrency;

	for (USoundBase* VoiceLine : PortraitSettings.VoiceLineOptions)
	{
		if (not IsValid(VoiceLine))
		{
			continue;
		}

		M_ShuffledPortraitVoiceLines.Add(VoiceLine);
	}

	Algo::RandomShuffle(M_ShuffledPortraitVoiceLines);
	bM_IsActive = true;
	bM_IsArmedForLowHealthTrigger = true;
}

void FMissionCommandVehicleWarningState::Reset()
{
	CleanupTransientEffects();
	M_CommandVehicle.Reset();
	M_ShuffledPortraitVoiceLines.Reset();
	M_OneShotVFX = nullptr;
	M_OneShotSound = nullptr;
	M_SoundAttenuation = nullptr;
	M_SoundConcurrency = nullptr;
	M_CommanderOffset = FVector::ZeroVector;
	M_VFXScale = FVector::OneVector;
	M_PortraitType = ERTSPortraitTypes::None;
	M_HealthPercentageTrigger = 0.0f;
	M_TimeBetweenTriggers = 0.0f;
	M_NextAllowedTriggerTimeSeconds = 0.0;
	M_NextPortraitVoiceLineIndex = 0;
	bM_IsActive = false;
	bM_IsArmedForLowHealthTrigger = false;
}

void FMissionCommandVehicleWarningState::CleanupTransientEffects()
{
	UAudioComponent* WarningAudioComponent = M_WarningAudioComponent.Get();
	if (IsValid(WarningAudioComponent))
	{
		WarningAudioComponent->Stop();
		WarningAudioComponent->DestroyComponent();
	}
	M_WarningAudioComponent.Reset();

	UNiagaraComponent* WarningNiagaraComponent = M_WarningNiagaraComponent.Get();
	if (IsValid(WarningNiagaraComponent))
	{
		WarningNiagaraComponent->DeactivateImmediate();
		WarningNiagaraComponent->DestroyComponent();
	}
	M_WarningNiagaraComponent.Reset();
}

bool FMissionCommandVehicleWarningState::TryBeginTrigger(
	const float CurrentHealthPercentage,
	const double CurrentTimeSeconds)
{
	if (not bM_IsActive)
	{
		return false;
	}

	if (CurrentTimeSeconds < M_NextAllowedTriggerTimeSeconds)
	{
		return false;
	}

	if (not bM_IsArmedForLowHealthTrigger)
	{
		bM_IsArmedForLowHealthTrigger = CurrentHealthPercentage >= M_HealthPercentageTrigger;
		return false;
	}

	if (CurrentHealthPercentage >= M_HealthPercentageTrigger)
	{
		return false;
	}

	M_NextAllowedTriggerTimeSeconds = CurrentTimeSeconds + M_TimeBetweenTriggers;
	bM_IsArmedForLowHealthTrigger = false;
	return true;
}

USoundBase* FMissionCommandVehicleWarningState::GetNextPortraitVoiceLine()
{
	if (M_ShuffledPortraitVoiceLines.IsEmpty() || M_PortraitType == ERTSPortraitTypes::None)
	{
		return nullptr;
	}

	if (not M_ShuffledPortraitVoiceLines.IsValidIndex(M_NextPortraitVoiceLineIndex))
	{
		M_NextPortraitVoiceLineIndex = 0;
	}

	USoundBase* VoiceLine = M_ShuffledPortraitVoiceLines[M_NextPortraitVoiceLineIndex];
	++M_NextPortraitVoiceLineIndex;
	return VoiceLine;
}

void FMissionCommandVehicleWarningState::SetWarningAudioComponent(UAudioComponent* AudioComponent)
{
	M_WarningAudioComponent = AudioComponent;
}

void FMissionCommandVehicleWarningState::SetWarningNiagaraComponent(UNiagaraComponent* NiagaraComponent)
{
	M_WarningNiagaraComponent = NiagaraComponent;
}
