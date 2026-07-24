#include "TrailerComponent.h"

#if PLATFORM_WINDOWS && (WITH_EDITOR || UE_BUILD_DEVELOPMENT)

#include "AudioDevice.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "RTSTrailerCaptureSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/DeveloperSettings/RTSGameInstancePIEDeveloperSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTrailerCaptureTest, Log, All);

namespace RTSTrailerCaptureTest
{
	constexpr double StartConfirmationTimeoutSeconds = 30.0;
	constexpr double MissionLoadTimeoutSeconds = 180.0;
	constexpr double FowRevealWaitSeconds = 5.0;
	constexpr double CompletionTimeoutMarginSeconds = 60.0;
	constexpr float DefaultRecordingSeconds = 3.0f;
	constexpr double VerificationToneDelaySeconds = 0.5;
	constexpr float VerificationToneDurationSeconds = 0.5f;
	constexpr float VerificationToneFrequencyHz = 880.0f;
	constexpr float VerificationToneAmplitude = 0.25f;
	constexpr float VerificationToneFadeSeconds = 0.02f;
	constexpr int32 VerificationToneSampleRate = 48000;
	constexpr int32 DefaultResolutionX = 1280;
	constexpr int32 DefaultResolutionY = 720;
	const TCHAR* const MissionMapLoadPath = TEXT("/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission");
	const TCHAR* const MissionMapName = TEXT("Ger_FirstMission");

	enum class ERTSTrailerTestPhase : uint8
	{
		WaitingForMissionWorld,
		WaitingForStartConfirmation,
		WaitingForFowReveal,
		Recording,
		WaitingForCompletion,
		Finished
	};

	UWorld* FindAnyGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (IsValid(World)
				&& (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))
			{
				return World;
			}
		}
		return nullptr;
	}

	UWorld* FindGerFirstMissionWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (IsValid(World) && World->GetMapName().Contains(MissionMapName))
			{
				return World;
			}
		}
		return nullptr;
	}

	ACPPController* FindLocalPlayerController(UWorld& World)
	{
		return Cast<ACPPController>(UGameplayStatics::GetPlayerController(&World, 0));
	}

	void ApplyTestStartupSettings(UWorld& World)
	{
		URTSGameInstance* GameInstance = Cast<URTSGameInstance>(World.GetGameInstance());
		const URTSGameInstancePIEDeveloperSettings* StartupSettings =
			GetDefault<URTSGameInstancePIEDeveloperSettings>();
		if (not IsValid(GameInstance) || not IsValid(StartupSettings))
		{
			return;
		}

		FCampaignGenerationSettings CampaignSettings;
		CampaignSettings.bAreSettingsLoaded = true;
		CampaignSettings.GenerationSeed = StartupSettings->CampaignGenerationSeed;
		CampaignSettings.EnemyWorldPersonality = StartupSettings->EnemyWorldPersonality;
		GameInstance->SetCampaignGenerationSettings(CampaignSettings);

		FRTSGameDifficulty Difficulty;
		Difficulty.DifficultyLevel = StartupSettings->DifficultyLevel;
		Difficulty.DifficultyPercentage = StartupSettings->DifficultyPercentage;
		Difficulty.bIsInitialized = true;
		GameInstance->SetSelectedGameDifficulty(Difficulty);
		GameInstance->SetPlayerFaction(StartupSettings->PlayerFaction);
		GameInstance->SetPlayerCommander(StartupSettings->PlayerCommander);
		GameInstance->SetWorldMissionContext(StartupSettings->WorldMissionContext);
	}

	bool TryConfirmStartGame(UWorld& World, bool& bOutWasWaitingForConfirmation)
	{
		bOutWasWaitingForConfirmation = false;
		ACPPController* PlayerController = FindLocalPlayerController(World);
		if (not IsValid(PlayerController))
		{
			return false;
		}

		UPlayerStartGameControl* StartGameControl =
			PlayerController->FindComponentByClass<UPlayerStartGameControl>();
		if (not IsValid(StartGameControl) || not StartGameControl->HasBegunPlay())
		{
			return false;
		}

		bOutWasWaitingForConfirmation = StartGameControl->GetIsWaitingForPlayerToStart();
		if (bOutWasWaitingForConfirmation)
		{
			StartGameControl->PlayerStartedGame();
		}
		return not StartGameControl->GetIsWaitingForPlayerToStart();
	}

	class FRTSTrailerStartConfirmationRequest
	{
	public:
		FRTSTrailerStartConfirmationRequest()
			: M_RequestStartSeconds(FPlatformTime::Seconds())
		{
		}

		bool Tick(float DeltaTime)
		{
			UWorld* World = FindAnyGameWorld();
			bool bWasWaitingForConfirmation = false;
			if (IsValid(World) && TryConfirmStartGame(*World, bWasWaitingForConfirmation))
			{
				UE_LOG(
					LogRTSTrailerCaptureTest,
					Display,
					TEXT("RTS_TRAILER_START_CONFIRMED Result=%s Map=%s"),
					bWasWaitingForConfirmation ? TEXT("Confirmed") : TEXT("AlreadyOpen"),
					*World->GetMapName());
				return false;
			}

			if (FPlatformTime::Seconds() - M_RequestStartSeconds < StartConfirmationTimeoutSeconds)
			{
				return true;
			}

			UE_LOG(LogRTSTrailerCaptureTest, Error, TEXT("RTS_TRAILER_START_CONFIRM_FAILED Timeout"));
			return false;
		}

	private:
		double M_RequestStartSeconds = 0.0;
	};

	class FRTSTrailerGerMissionTestRun
	{
	public:
		FRTSTrailerGerMissionTestRun(
			const float RecordingSeconds,
			const int32 ResolutionX,
			const int32 ResolutionY,
			const bool bOverrideResolution,
			const bool bQuitWhenDone)
			: M_Resolution(FMath::Max(2, ResolutionX), FMath::Max(2, ResolutionY))
			, M_RecordingSeconds(FMath::Max(0.5f, RecordingSeconds))
			, M_RunStartSeconds(FPlatformTime::Seconds())
			, bM_OverrideResolution(bOverrideResolution)
			, bM_QuitWhenDone(bQuitWhenDone)
		{
		}

		bool Tick(float DeltaTime)
		{
			switch (M_Phase)
			{
			case ERTSTrailerTestPhase::WaitingForMissionWorld: return TickWaitingForMissionWorld();
			case ERTSTrailerTestPhase::WaitingForStartConfirmation: return TickWaitingForStartConfirmation();
			case ERTSTrailerTestPhase::WaitingForFowReveal: return TickWaitingForFowReveal();
			case ERTSTrailerTestPhase::Recording: return TickRecording();
			case ERTSTrailerTestPhase::WaitingForCompletion: return TickWaitingForCompletion();
			case ERTSTrailerTestPhase::Finished: return false;
			default: return Finish(false, TEXT("Unknown test phase."));
			}
		}

		bool GetIsFinished() const
		{
			return M_Phase == ERTSTrailerTestPhase::Finished;
		}

	private:
		bool TickWaitingForMissionWorld()
		{
			UWorld* MissionWorld = FindGerFirstMissionWorld();
			if (IsValid(MissionWorld) && MissionWorld->HasBegunPlay())
			{
				ApplyTestStartupSettings(*MissionWorld);
				M_MissionWorld = MissionWorld;
				M_Phase = ERTSTrailerTestPhase::WaitingForStartConfirmation;
				M_PhaseStartSeconds = FPlatformTime::Seconds();
				UE_LOG(LogRTSTrailerCaptureTest, Display, TEXT("RTS_TRAILER_TEST_MAP_READY Map=%s"), *MissionWorld->GetMapName());
				return true;
			}

			if (not bM_RequestedMissionOpen)
			{
				UWorld* CurrentWorld = FindAnyGameWorld();
				if (IsValid(CurrentWorld))
				{
					ApplyTestStartupSettings(*CurrentWorld);
					bM_RequestedMissionOpen = true;
					UE_LOG(LogRTSTrailerCaptureTest, Display, TEXT("Opening trailer test map: %s"), MissionMapLoadPath);
					UGameplayStatics::OpenLevel(CurrentWorld, FName(MissionMapLoadPath));
				}
			}

			if (FPlatformTime::Seconds() - M_RunStartSeconds < MissionLoadTimeoutSeconds)
			{
				return true;
			}
			return Finish(false, TEXT("Timed out waiting for Ger_FirstMission to load."));
		}

		bool TickWaitingForStartConfirmation()
		{
			UWorld* World = M_MissionWorld.Get();
			bool bWasWaitingForConfirmation = false;
			if (IsValid(World) && TryConfirmStartGame(*World, bWasWaitingForConfirmation))
			{
				M_Phase = ERTSTrailerTestPhase::WaitingForFowReveal;
				M_PhaseStartSeconds = FPlatformTime::Seconds();
				UE_LOG(
					LogRTSTrailerCaptureTest,
					Display,
					TEXT("RTS_TRAILER_TEST_START_CONFIRMED Result=%s"),
					bWasWaitingForConfirmation ? TEXT("Confirmed") : TEXT("AlreadyOpen"));
				return true;
			}

			if (FPlatformTime::Seconds() - M_PhaseStartSeconds < StartConfirmationTimeoutSeconds)
			{
				return true;
			}
			return Finish(false, TEXT("Timed out confirming the start-game dialog."));
		}

		bool TickWaitingForFowReveal()
		{
			const double WaitedSeconds = FPlatformTime::Seconds() - M_PhaseStartSeconds;
			if (WaitedSeconds < FowRevealWaitSeconds)
			{
				return true;
			}

			UWorld* World = M_MissionWorld.Get();
			ACPPController* PlayerController = IsValid(World) ? FindLocalPlayerController(*World) : nullptr;
			UTrailerComponent* TrailerComponent = IsValid(PlayerController)
				? PlayerController->FindComponentByClass<UTrailerComponent>()
				: nullptr;
			if (not IsValid(TrailerComponent))
			{
				return Finish(false, TEXT("TrailerComponent was not available after the FoW wait."));
			}
			if (not TrailerComponent->StartRecording(
				bM_OverrideResolution,
				M_Resolution.X,
				M_Resolution.Y))
			{
				return Finish(false, TEXT("StartRecording failed: ") + TrailerComponent->GetLastError());
			}
			BeginAudioVerification();

			M_Phase = ERTSTrailerTestPhase::Recording;
			M_PhaseStartSeconds = FPlatformTime::Seconds();
			UE_LOG(
				LogRTSTrailerCaptureTest,
				Display,
				TEXT("RTS_TRAILER_TEST_CAPTURE_STARTED FowWait=%.3f Duration=%.3f Mode=%s RequestedResolution=%dx%d"),
				WaitedSeconds,
				M_RecordingSeconds,
				bM_OverrideResolution ? TEXT("Override") : TEXT("Native"),
				M_Resolution.X,
				M_Resolution.Y);
			return true;
		}

		bool TickRecording()
		{
			MaintainAudioVerification();
			const double RecordingElapsedSeconds = FPlatformTime::Seconds() - M_PhaseStartSeconds;
			if (not bM_PlayedVerificationTone && RecordingElapsedSeconds >= VerificationToneDelaySeconds)
			{
				if (not PlayVerificationTone())
				{
					return Finish(false, TEXT("Failed to play the trailer audio verification cue."));
				}
				bM_PlayedVerificationTone = true;
			}

			if (RecordingElapsedSeconds < M_RecordingSeconds)
			{
				return true;
			}

			UTrailerComponent* TrailerComponent = FindTrailerComponent();
			if (not IsValid(TrailerComponent) || not TrailerComponent->StopRecording())
			{
				return Finish(false, TEXT("StopRecording failed or TrailerComponent disappeared."));
			}

			M_Phase = ERTSTrailerTestPhase::WaitingForCompletion;
			M_PhaseStartSeconds = FPlatformTime::Seconds();
			UE_LOG(LogRTSTrailerCaptureTest, Display, TEXT("RTS_TRAILER_TEST_CAPTURE_STOP_REQUESTED"));
			return true;
		}

		bool PlayVerificationTone()
		{
			UWorld* World = M_MissionWorld.Get();
			if (not IsValid(World))
			{
				return false;
			}

			USoundWaveProcedural* VerificationTone = NewObject<USoundWaveProcedural>();
			if (not IsValid(VerificationTone))
			{
				return false;
			}

			VerificationTone->SetSampleRate(VerificationToneSampleRate);
			VerificationTone->NumChannels = 1;
			VerificationTone->Duration = VerificationToneDurationSeconds;
			VerificationTone->SoundGroup = SOUNDGROUP_Default;
			VerificationTone->bLooping = false;

			const int32 ToneSampleCount = FMath::RoundToInt(
				VerificationToneSampleRate * VerificationToneDurationSeconds);
			const int32 FadeSampleCount = FMath::Max(
				1,
				FMath::RoundToInt(VerificationToneSampleRate * VerificationToneFadeSeconds));
			TArray<int16> ToneSamples;
			ToneSamples.SetNumUninitialized(ToneSampleCount);
			for (int32 SampleIndex = 0; SampleIndex < ToneSampleCount; ++SampleIndex)
			{
				const float FadeIn = FMath::Clamp(
					static_cast<float>(SampleIndex) / FadeSampleCount,
					0.0f,
					1.0f);
				const float FadeOut = FMath::Clamp(
					static_cast<float>(ToneSampleCount - SampleIndex - 1) / FadeSampleCount,
					0.0f,
					1.0f);
				const double SampleTimeSeconds = static_cast<double>(SampleIndex) / VerificationToneSampleRate;
				const double ToneValue = FMath::Sin(2.0 * PI * VerificationToneFrequencyHz * SampleTimeSeconds);
				ToneSamples[SampleIndex] = static_cast<int16>(FMath::RoundToInt(
					ToneValue * FMath::Min(FadeIn, FadeOut) * VerificationToneAmplitude * MAX_int16));
			}

			VerificationTone->QueueAudio(
				reinterpret_cast<const uint8*>(ToneSamples.GetData()),
				ToneSamples.Num() * sizeof(int16));
			M_VerificationTone.Reset(VerificationTone);
			UGameplayStatics::PlaySound2D(World, VerificationTone);
			UE_LOG(
				LogRTSTrailerCaptureTest,
				Display,
				TEXT("RTS_TRAILER_TEST_AUDIO_CUE_PLAYED FrequencyHz=%.1f Duration=%.3f AppVolume=%.3f PrimaryVolume=%.3f"),
				VerificationToneFrequencyHz,
				VerificationToneDurationSeconds,
				FApp::GetVolumeMultiplier(),
				World->GetAudioDevice() ? World->GetAudioDevice()->GetPrimaryVolume() : -1.0f);
			return true;
		}

		void BeginAudioVerification()
		{
			M_PreviousAppVolumeMultiplier = FApp::GetVolumeMultiplier();
			M_PreviousUnfocusedVolumeMultiplier = FApp::GetUnfocusedVolumeMultiplier();
			FApp::SetUnfocusedVolumeMultiplier(1.0f);
			bM_OverrodeAppVolumeMultiplier = true;
			MaintainAudioVerification();
		}

		void MaintainAudioVerification() const
		{
			FApp::SetVolumeMultiplier(1.0f);
		}

		void RestoreAppVolumeMultiplier()
		{
			if (not bM_OverrodeAppVolumeMultiplier)
			{
				return;
			}
			FApp::SetUnfocusedVolumeMultiplier(M_PreviousUnfocusedVolumeMultiplier);
			FApp::SetVolumeMultiplier(M_PreviousAppVolumeMultiplier);
			bM_OverrodeAppVolumeMultiplier = false;
		}

		bool TickWaitingForCompletion()
		{
			UTrailerComponent* TrailerComponent = FindTrailerComponent();
			if (not IsValid(TrailerComponent))
			{
				return Finish(false, TEXT("TrailerComponent disappeared during finalization."));
			}

			if (TrailerComponent->GetRecordingState() == ERTSTrailerRecordingState::Completed)
			{
				return Finish(true, TrailerComponent->GetLastOutputPath());
			}
			if (TrailerComponent->GetRecordingState() == ERTSTrailerRecordingState::Failed)
			{
				return Finish(false, TEXT("Trailer capture failed: ") + TrailerComponent->GetLastError());
			}

			const URTSTrailerCaptureSettings* Settings = GetDefault<URTSTrailerCaptureSettings>();
			const double MaximumWaitSeconds = IsValid(Settings)
				? Settings->M_FinalizationTimeoutSeconds + Settings->M_EncodingTimeoutSeconds + CompletionTimeoutMarginSeconds
				: 900.0;
			if (FPlatformTime::Seconds() - M_PhaseStartSeconds < MaximumWaitSeconds)
			{
				return true;
			}
			return Finish(false, TEXT("Timed out waiting for trailer finalization and encoding."));
		}

		UTrailerComponent* FindTrailerComponent() const
		{
			UWorld* World = M_MissionWorld.Get();
			ACPPController* PlayerController = IsValid(World) ? FindLocalPlayerController(*World) : nullptr;
			return IsValid(PlayerController)
				? PlayerController->FindComponentByClass<UTrailerComponent>()
				: nullptr;
		}

		bool Finish(const bool bSuccess, const FString& Result)
		{
			M_Phase = ERTSTrailerTestPhase::Finished;
			M_VerificationTone.Reset();
			RestoreAppVolumeMultiplier();
			if (bSuccess)
			{
				UE_LOG(LogRTSTrailerCaptureTest, Display, TEXT("RTS_TRAILER_TEST_SUCCESS MP4=\"%s\""), *Result);
			}
			else
			{
				UE_LOG(LogRTSTrailerCaptureTest, Error, TEXT("RTS_TRAILER_TEST_FAILED Reason=\"%s\""), *Result);
			}

			if (bM_QuitWhenDone)
			{
				FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1, TEXT("RTS Trailer Capture Test"));
			}
			return false;
		}

		TWeakObjectPtr<UWorld> M_MissionWorld;
		TStrongObjectPtr<USoundWaveProcedural> M_VerificationTone;
		ERTSTrailerTestPhase M_Phase = ERTSTrailerTestPhase::WaitingForMissionWorld;
		FIntPoint M_Resolution = FIntPoint::ZeroValue;
		float M_RecordingSeconds = 0.0f;
		double M_RunStartSeconds = 0.0;
		double M_PhaseStartSeconds = 0.0;
		float M_PreviousAppVolumeMultiplier = 1.0f;
		float M_PreviousUnfocusedVolumeMultiplier = 0.0f;
		bool bM_RequestedMissionOpen = false;
		bool bM_PlayedVerificationTone = false;
		bool bM_OverrodeAppVolumeMultiplier = false;
		bool bM_OverrideResolution = true;
		bool bM_QuitWhenDone = false;
	};

	TWeakPtr<FRTSTrailerGerMissionTestRun> GActiveTestRun;

	void ConfirmStartGameCommand(const TArray<FString>& Args, UWorld* World)
	{
		const TSharedRef<FRTSTrailerStartConfirmationRequest> ConfirmationRequest =
			MakeShared<FRTSTrailerStartConfirmationRequest>();
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([ConfirmationRequest](const float DeltaTime)
			{
				return ConfirmationRequest->Tick(DeltaTime);
			}));
	}

	void RunGerFirstMissionTestCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (const TSharedPtr<FRTSTrailerGerMissionTestRun> ActiveRun = GActiveTestRun.Pin())
		{
			if (not ActiveRun->GetIsFinished())
			{
				UE_LOG(LogRTSTrailerCaptureTest, Error, TEXT("A trailer capture test is already active."));
				return;
			}
		}

		const float RecordingSeconds = Args.Num() >= 1 ? FCString::Atof(*Args[0]) : DefaultRecordingSeconds;
		const int32 ResolutionX = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : DefaultResolutionX;
		const int32 ResolutionY = Args.Num() >= 3 ? FCString::Atoi(*Args[2]) : DefaultResolutionY;
		bool bOverrideResolution = true;
		bool bQuitWhenDone = false;
		for (const FString& Arg : Args)
		{
			FString NormalizedArgument = Arg;
			NormalizedArgument.ReplaceInline(TEXT(";"), TEXT(""));
			if (NormalizedArgument.Equals(TEXT("Quit"), ESearchCase::IgnoreCase)
				|| NormalizedArgument.Equals(TEXT("Exit"), ESearchCase::IgnoreCase))
			{
				bQuitWhenDone = true;
			}
			if (NormalizedArgument.Equals(TEXT("Native"), ESearchCase::IgnoreCase)
				|| NormalizedArgument.Equals(TEXT("Viewport"), ESearchCase::IgnoreCase))
			{
				bOverrideResolution = false;
			}
		}

		const TSharedRef<FRTSTrailerGerMissionTestRun> TestRun = MakeShared<FRTSTrailerGerMissionTestRun>(
			RecordingSeconds,
			ResolutionX,
			ResolutionY,
			bOverrideResolution,
			bQuitWhenDone);
		GActiveTestRun = TestRun;
		UE_LOG(LogRTSTrailerCaptureTest, Display, TEXT("RTS_TRAILER_TEST_SETUP_STARTED"));
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([TestRun](const float DeltaTime)
			{
				return TestRun->Tick(DeltaTime);
			}));
	}

	FAutoConsoleCommandWithWorldAndArgs GConfirmStartGameCommand(
		TEXT("RTS.Trailer.ConfirmStartGame"),
		TEXT("Confirms the same start-game gate as clicking the Start dialog button."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConfirmStartGameCommand));

	FAutoConsoleCommandWithWorldAndArgs GRunGerFirstMissionTestCommand(
		TEXT("RTS.Trailer.RunGerFirstMissionTest"),
		TEXT("Opens Ger_FirstMission, confirms Start, waits five seconds for FoW, records and verifies completion. ")
		TEXT("Args: [RecordingSeconds] [ResolutionX] [ResolutionY] [Native] [Quit]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunGerFirstMissionTestCommand));
}

#endif
