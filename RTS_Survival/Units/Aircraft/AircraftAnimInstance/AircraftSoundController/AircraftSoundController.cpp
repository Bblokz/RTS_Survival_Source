#include "AircraftSoundController.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "TimerManager.h"

#include "RTS_Survival/Units/Aircraft/AircraftAnimInstance/AircraftAnimInstance.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

// ==================== Existing init (extended to also init fly-by) ====================

void FAircraftSoundController::InitEngineSound(AAircraftMaster* OwningAircraft,
                                               UAircraftAnimInstance* OwningAnimInst)
{
	if (not IsValid(OwningAircraft))
	{
		RTSFunctionLibrary::ReportError("Invalid owning aircraft for sound controller!");
		return;
	}

	// Cache world once (requested for timer convenience).
	M_OwningAircraftAnimInstance = OwningAnimInst;
	M_World = OwningAircraft->GetWorld();

	// If we already have a valid component, don't create another one.
	if (GetIsValidEngineSystem())
	{
		// Still ensure fly-by component exists if we re-enter init.
		FlyBySound_Init(OwningAircraft);
		return;
	}

	// Create the engine sound component on the aircraft as outer.
	M_EngineSoundSystem = NewObject<UAudioComponent>(
		OwningAircraft,
		UAudioComponent::StaticClass(),
		TEXT("AircraftEngineAudio")
	);

	if (not EnsureIsValidEngineSystem())
	{
		return;
	}

	// Must have a valid root to attach to.
	USceneComponent* const RootComp = OwningAircraft->GetRootComponent();
	if (not IsValid(RootComp))
	{
		RTSFunctionLibrary::ReportError("Owning aircraft has no valid RootComponent to attach engine audio.");
		return;
	}

	// Configure basics before registering.
	M_EngineSoundSystem->bAutoActivate = false;
	M_EngineSoundSystem->bAutoDestroy = false;
	M_EngineSoundSystem->bAllowSpatialization = true;

	// Optional: apply attenuation & concurrency if provided.
	if (IsValid(EngineSoundAttenuation))
	{
		M_EngineSoundSystem->AttenuationSettings = EngineSoundAttenuation;
	}
	if (IsValid(EngineSoundConcurrency))
	{
		M_EngineSoundSystem->ConcurrencySet.Add(EngineSoundConcurrency);
	}

	// Add as an instance component so the actor owns it, then attach & register.
	OwningAircraft->AddInstanceComponent(M_EngineSoundSystem);
	M_EngineSoundSystem->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
	M_EngineSoundSystem->RegisterComponent();

	// Initialize parameters (e.g., speed at 0).
	M_EngineSoundSystem->SetFloatParameter(AudioSpeedParam, 0.f);

	// Ensure the fly-by component is created/configured as well.
	FlyBySound_Init(OwningAircraft);

	PlaySoundForAircraftState(OwningAircraft->GetLandedState(), OwningAnimInst);
}

// ==================== Existing plays (touched only to hook fly-by) ====================

void FAircraftSoundController::PlayVto_Prep(UAircraftAnimInstance* OwningAircraftAnimator, const float VtoPrepTime)
{
	if (not EnsureSoundIsValid(VTOPrepSound, "VTOPrepSound") || not GetIsValidEngineSystem())
	{
		return;
	}
	M_EngineSoundSystem->Sound = VTOPrepSound;
	const float StartOffset = ComputeJumpAheadTime(VTOPrepSound, VtoPrepTime);
	M_EngineSoundSystem->Play(StartOffset);

	// Not airborne: stop fly-by if running.
	FlyBySound_OnNotAirborne(OwningAircraftAnimator);
}

void FAircraftSoundController::PlayVto_Start(UAircraftAnimInstance* OwningAircraftAnimator, const float TotalVTOTime)
{
	if (not EnsureSoundIsValid(VTOSound, "VTO Start Sound (active VTO)") || not GetIsValidEngineSystem())
	{
		return;
	}
	// Make sure the sound fits for the VTO time window.
	const float StartOffset = ComputeJumpAheadTime(VTOSound, TotalVTOTime);
	M_EngineSoundSystem->Sound = VTOSound;
	M_EngineSoundSystem->Play(StartOffset);

	// Possibly we went from airborne to VTO; make sure engine update timer is off and fly-by is stopped.
	CancelTimerIfActive(OwningAircraftAnimator);
	FlyBySound_OnNotAirborne(OwningAircraftAnimator);
}

void FAircraftSoundController::PlayVto_Landing(UAircraftAnimInstance* OwningAircraftAnimator,
                                               const float VtoLandingTime)
{
	if (not EnsureSoundIsValid(LandingSound, "LandedEngineSound") || not GetIsValidEngineSystem())
	{
		return;
	}

	// Ensure that the sound is jumped ahead if we have a short landing time.
	const float StartOffset = ComputeJumpAheadTime(LandedEngineSound, VtoLandingTime);

	M_EngineSoundSystem->Sound = LandingSound;
	M_EngineSoundSystem->Play(StartOffset);

	CancelTimerIfActive(OwningAircraftAnimator);
	FlyBySound_OnNotAirborne(OwningAircraftAnimator);
}

void FAircraftSoundController::Play_Airborne(UAircraftAnimInstance* OwningAircraftAnimator)
{
	if (not EnsureValidAnimState(OwningAircraftAnimator) || not EnsureSoundIsValid(
		FlightEngineSound, "FlightEngineSound"))
	{
		return;
	}
	UWorld* World = OwningAircraftAnimator->GetWorld();
	using DeveloperSettings::Optimization::UpdateEngineSoundsInterval;
	TWeakObjectPtr<UAircraftAnimInstance> WeakOwningAircraftAnimator = OwningAircraftAnimator;
	auto UpdateLambda = [WeakOwningAircraftAnimator, this]()
	{
		if (not WeakOwningAircraftAnimator.IsValid())
		{
			return;
		}
		UAircraftAnimInstance* StrongOwningAircraftAnimator = WeakOwningAircraftAnimator.Get();
		const float Speed = StrongOwningAircraftAnimator->GetLastAirSpeed();
		UpdateEngineSound(Speed);
	};
	World->GetTimerManager().SetTimer(
		M_EngineSoundUpdate,
		FTimerDelegate::CreateLambda(UpdateLambda),
		UpdateEngineSoundsInterval,
		true,
		0.f
	);
	M_EngineSoundSystem->Sound = FlightEngineSound;
	M_EngineSoundSystem->Play();
	bIsTimerActive = true;
	UpdateEngineSound(OwningAircraftAnimator->GetLastAirSpeed());

	// Start fly-by scheduling when we become airborne.
	FlyBySound_OnAirborne(OwningAircraftAnimator);
}

void FAircraftSoundController::Play_Landed(UAircraftAnimInstance* OwningAircraftAnimator)
{
	if (not GetIsValidEngineSystem() || not EnsureSoundIsValid(LandedEngineSound, "LandedEngineSound"))
	{
		return;
	}
	M_EngineSoundSystem->Sound = LandedEngineSound;
	M_EngineSoundSystem->Play();

	// No longer airborne -> stop fly-by.
	FlyBySound_OnNotAirborne(OwningAircraftAnimator);
}

void FAircraftSoundController::PlaySoundForAircraftState(const EAircraftLandingState LandingState,
                                                         UAircraftAnimInstance* OwningAnimInst)
{
	if (not OwningAnimInst)
	{
		return;
	}
	switch (LandingState)
	{
	case EAircraftLandingState::None:
		RTSFunctionLibrary::ReportError("Engine sound init but landing state is set to none; not playing any sound!");
		break;
	case EAircraftLandingState::Landed:
		Play_Landed(OwningAnimInst);
		break;
	case EAircraftLandingState::VerticalTakeOff:
		PlayVto_Start(OwningAnimInst, 3.f);
		break;
	case EAircraftLandingState::Airborne:
		Play_Airborne(OwningAnimInst);
		break;
	case EAircraftLandingState::WaitForBayToOpen:
		Play_Airborne(OwningAnimInst);
		break;
	case EAircraftLandingState::VerticalLanding:
		PlayVto_Landing(OwningAnimInst, 3.f);
		break;
	}
}

// ==================== Existing helpers (unchanged) ====================

bool FAircraftSoundController::EnsureIsValidEngineSystem() const
{
	if (not GetIsValidEngineSystem())
	{
		RTSFunctionLibrary::ReportError("Invalid engine sound system for aircraft!");
		return false;
	}
	return true;
}

bool FAircraftSoundController::EnsureSoundIsValid(const USoundBase* SoundToCheck, const FString& SoundName) const
{
	if (not IsValid(SoundToCheck))
	{
		RTSFunctionLibrary::ReportError("Invalid sound: " + SoundName +
			"\n For FAircraftSoundController");
		return false;
	}
	return true;
}

bool FAircraftSoundController::GetIsValidEngineSystem() const
{
	return IsValid(M_EngineSoundSystem);
}

bool FAircraftSoundController::EnsureValidAnimState(const UAircraftAnimInstance* OwningAircraftAnimator) const
{
	if (not IsValid(OwningAircraftAnimator))
	{
		return false;
	}
	UWorld* World = OwningAircraftAnimator->GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("Invalid world context for aircraft sound controller!");
		return false;
	}
	return true;
}

void FAircraftSoundController::UpdateEngineSound(const float Speed) const
{
	if (not GetIsValidEngineSystem())
	{
		return;
	}
	M_EngineSoundSystem->SetFloatParameter(AudioSpeedParam, Speed);
}

void FAircraftSoundController::CancelTimerIfActive(UAircraftAnimInstance* OwningAircraftAnimator)
{
	if (not bIsTimerActive || not EnsureValidAnimState(OwningAircraftAnimator))
	{
		return;
	}
	UWorld* World = OwningAircraftAnimator->GetWorld();
	if (World->GetTimerManager().IsTimerActive(M_EngineSoundUpdate))
	{
		World->GetTimerManager().ClearTimer(M_EngineSoundUpdate);
	}
	bIsTimerActive = false;
}

float FAircraftSoundController::ComputeJumpAheadTime(const USoundBase* Sound, const float TotalWindowSeconds) const
{
	if (not IsValid(Sound))
	{
		RTSFunctionLibrary::ReportError("ComputeJumpAheadTime called with invalid sound.");
		return 0.f;
	}

	// Negative windows are not allowed; clamp to zero.
	const float Window = FMath::Max(0.f, TotalWindowSeconds);

	const float Duration = Sound->GetDuration(); // seconds; may be < 0 for “infinite/unknown”
	if (Duration < 0.f)
	{
		// Unknown/looping duration: just start at 0; we can play indefinitely.
		return 0.f;
	}

	// If the clip is shorter than the window, we can’t stretch it to fit.
	if (Window > Duration + KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Sound '%s' (%.2fs) is shorter than required window %.2fs."),
			                *Sound->GetName(), Duration, Window));
		return 0.f;
	}

	// We want: (Duration - Offset) == Window  ->  Offset = Duration - Window
	const float Offset = Duration - Window;

	// Guard against tiny negative due to FP precision and cap at Duration.
	return FMath::Clamp(Offset, 0.f, Duration);
}

// ==================== New: Fly-by component & logic ====================

void FAircraftSoundController::FlyBySound_Init(AAircraftMaster* OwningAircraft)
{
	if (GetIsValidFlyBySystem())
	{
		return;
	}

	if (not IsValid(OwningAircraft))
	{
		RTSFunctionLibrary::ReportError("FlyBySound_Init: invalid owning aircraft.");
		return;
	}

	USceneComponent* const RootComp = OwningAircraft->GetRootComponent();
	if (not IsValid(RootComp))
	{
		RTSFunctionLibrary::ReportError("FlyBySound_Init: aircraft has no valid RootComponent.");
		return;
	}

	M_FlyBySoundSystem = NewObject<UAudioComponent>(
		OwningAircraft,
		UAudioComponent::StaticClass(),
		TEXT("AircraftFlyByAudio")
	);

	if (not EnsureIsValidFlyBySystem())
	{
		return;
	}

	// Configure identical spatial behavior to engine (and persist).
	M_FlyBySoundSystem->bAutoActivate = false;
	M_FlyBySoundSystem->bAutoDestroy = false;
	M_FlyBySoundSystem->bAllowSpatialization = true;

	if (IsValid(EngineSoundAttenuation))
	{
		M_FlyBySoundSystem->AttenuationSettings = EngineSoundAttenuation;
	}
	if (IsValid(EngineSoundConcurrency))
	{
		M_FlyBySoundSystem->ConcurrencySet.Add(EngineSoundConcurrency);
	}

	OwningAircraft->AddInstanceComponent(M_FlyBySoundSystem);
	M_FlyBySoundSystem->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
	M_FlyBySoundSystem->RegisterComponent();

	// Ensure timers start from a clean state.
	FlyBySound_ClearTimerIfActive();
	FlyBySound_ClearDeactivateTimerIfActive();
	M_FlyByShuffleState.Reset();
}

void FAircraftSoundController::FlyBySound_OnAirborne(UAircraftAnimInstance* OwningAircraftAnimator)
{
	if (not EnsureValidAnimState(OwningAircraftAnimator))
	{
		return;
	}

	// Make sure component exists.
	if (not GetIsValidFlyBySystem())
	{
		if (AAircraftMaster* OwnerAircraft = Cast<AAircraftMaster>(OwningAircraftAnimator->GetOwningActor()))
		{
			FlyBySound_Init(OwnerAircraft);
		}
		else
		{
			RTSFunctionLibrary::ReportError("FlyBySound_OnAirborne: cannot resolve owning aircraft to create audio.");
			return;
		}
	}

	// Nothing to do if we have no content to play.
	if (RandomFlyBySounds.Num() <= 0)
	{
		return;
	}

	// World cache fallback (if not set yet).
	if (not M_World.IsValid())
	{
		M_World = OwningAircraftAnimator->GetWorld();
	}

	// If a schedule is already active, keep it.
	if (bM_FlyByTimerActive)
	{
		return;
	}

	// Schedule first play after a random delay.
	FlyBySound_ScheduleNext();
}

void FAircraftSoundController::FlyBySound_OnNotAirborne(UAircraftAnimInstance* OwningAircraftAnimator)
{
	// Stop schedule timer.
	FlyBySound_ClearTimerIfActive();
	FlyBySound_ClearDeactivateTimerIfActive();

	// Stop and deactivate component to make it dormant.
	if (GetIsValidFlyBySystem())
	{
		if (M_FlyBySoundSystem->IsPlaying())
		{
			M_FlyBySoundSystem->Stop();
		}
		M_FlyBySoundSystem->Deactivate();
	}
}

void FAircraftSoundController::FlyBySound_ScheduleNext()
{
	if (not FlyBySound_CanPlay())
	{
		return;
	}

	UWorld* World = M_World.Get();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("FlyBySound_ScheduleNext: invalid world.");
		return;
	}

	const float Delay = FlyBySound_PickRandomDelay();

	const TWeakObjectPtr<UAircraftAnimInstance> WeakOwningAircraftAnimInstance = M_OwningAircraftAnimInstance;
	World->GetTimerManager().SetTimer(
		M_FlyByTimer,
		FTimerDelegate::CreateLambda([WeakOwningAircraftAnimInstance]()
		{
			if (not WeakOwningAircraftAnimInstance.IsValid())
			{
				return;
			}

			UAircraftAnimInstance* StrongOwningAircraftAnimInstance = WeakOwningAircraftAnimInstance.Get();
			StrongOwningAircraftAnimInstance->GetSoundController().FlyBySound_PlayNext();
		}),
		Delay,
		false
	);
	bM_FlyByTimerActive = true;
}

void FAircraftSoundController::FlyBySound_PlayNext()
{
	bM_FlyByTimerActive = false;

	if (not FlyBySound_CanPlay())
	{
		return;
	}

	const int32 NextIndex = FlyBySound_GetNextIndex();
	if (NextIndex == INDEX_NONE)
	{
		// Nothing playable; try scheduling again later.
		FlyBySound_ScheduleNext();
		return;
	}

	USoundBase* const Chosen = RandomFlyBySounds[NextIndex];
	if (not IsValid(Chosen))
	{
		// Skip invalid entries gracefully.
		FlyBySound_ScheduleNext();
		return;
	}

	// Set sound and play immediately (component becomes active).
	M_FlyBySoundSystem->Sound = Chosen;
	M_FlyBySoundSystem->Play(0.f);

	// Schedule a deactivate after clip finishes to minimize overhead.
	const float Duration = Chosen->GetDuration();
	if (Duration > 0.f)
	{
		UWorld* World = M_World.Get();
		if (IsValid(World))
		{
			const TWeakObjectPtr<UAircraftAnimInstance> WeakOwningAircraftAnimInstance = M_OwningAircraftAnimInstance;
			World->GetTimerManager().SetTimer(
				M_FlyByDeactivateTimer,
				FTimerDelegate::CreateLambda([WeakOwningAircraftAnimInstance]()
				{
					if (not WeakOwningAircraftAnimInstance.IsValid())
					{
						return;
					}

					UAircraftAnimInstance* StrongOwningAircraftAnimInstance = WeakOwningAircraftAnimInstance.Get();
					StrongOwningAircraftAnimInstance->GetSoundController().FlyBySound_DeactivateAfterPlay();
				}),
				Duration,
				false
			);
			bM_FlyByDeactivateTimerActive = true;
		}
	}

	// Schedule the next play window now.
	FlyBySound_ScheduleNext();
}

void FAircraftSoundController::FlyBySound_DeactivateAfterPlay()
{
	bM_FlyByDeactivateTimerActive = false;

	if (not GetIsValidFlyBySystem())
	{
		return;
	}

	// Put the component back to a light state.
	if (M_FlyBySoundSystem->IsPlaying())
	{
		M_FlyBySoundSystem->Stop();
	}
	M_FlyBySoundSystem->Deactivate();
}

int32 FAircraftSoundController::FlyBySound_GetNextIndex()
{
	const int32 Num = RandomFlyBySounds.Num();
	if (Num <= 0)
	{
		return INDEX_NONE;
	}

	// First-time init of start/current.
	if (M_FlyByShuffleState.StartIndex == INDEX_NONE || M_FlyByShuffleState.CurrentIndex == INDEX_NONE)
	{
		M_FlyByShuffleState.StartIndex = FMath::RandRange(0, Num - 1);
		M_FlyByShuffleState.CurrentIndex = M_FlyByShuffleState.StartIndex;
	}

	// Index to play now.
	const int32 Result = M_FlyByShuffleState.CurrentIndex;

	// Advance for next time.
	M_FlyByShuffleState.CurrentIndex = (M_FlyByShuffleState.CurrentIndex + 1) % Num;

	// If we wrapped back to the start, reshuffle a new start and continue from there.
	if (M_FlyByShuffleState.CurrentIndex == M_FlyByShuffleState.StartIndex)
	{
		M_FlyByShuffleState.StartIndex = FMath::RandRange(0, Num - 1);
		M_FlyByShuffleState.CurrentIndex = M_FlyByShuffleState.StartIndex;
	}

	return Result;
}

float FAircraftSoundController::FlyBySound_PickRandomDelay() const
{
	// Clamp to sane values: non-negative, min <= max.
	const float MinS = FMath::Max(0.f, MinMaxTimeBetweenFlyBySounds.X);
	const float MaxS = FMath::Max(MinS, MinMaxTimeBetweenFlyBySounds.Y);
	return FMath::FRandRange(MinS, MaxS);
}

bool FAircraftSoundController::FlyBySound_CanPlay() const
{
	if (not GetIsValidFlyBySystem())
	{
		return false;
	}
	if (RandomFlyBySounds.Num() <= 0)
	{
		return false;
	}
	if (not M_World.IsValid())
	{
		return false;
	}
	return true;
}

bool FAircraftSoundController::EnsureIsValidFlyBySystem() const
{
	if (not GetIsValidFlyBySystem())
	{
		RTSFunctionLibrary::ReportError("Invalid fly-by sound system for aircraft!");
		return false;
	}
	return true;
}

bool FAircraftSoundController::GetIsValidFlyBySystem() const
{
	return IsValid(M_FlyBySoundSystem);
}

void FAircraftSoundController::FlyBySound_ClearTimerIfActive()
{
	if (not M_World.IsValid())
	{
		bM_FlyByTimerActive = false;
		return;
	}

	UWorld* World = M_World.Get();
	if (bM_FlyByTimerActive && IsValid(World) && World->GetTimerManager().IsTimerActive(M_FlyByTimer))
	{
		World->GetTimerManager().ClearTimer(M_FlyByTimer);
	}
	bM_FlyByTimerActive = false;
}

void FAircraftSoundController::FlyBySound_ClearDeactivateTimerIfActive()
{
	if (not M_World.IsValid())
	{
		bM_FlyByDeactivateTimerActive = false;
		return;
	}

	UWorld* World = M_World.Get();
	if (bM_FlyByDeactivateTimerActive && IsValid(World) && World->GetTimerManager().IsTimerActive(M_FlyByDeactivateTimer))
	{
		World->GetTimerManager().ClearTimer(M_FlyByDeactivateTimer);
	}
	bM_FlyByDeactivateTimerActive = false;
}
