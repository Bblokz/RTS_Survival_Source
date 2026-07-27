#if WITH_DEV_AUTOMATION_TESTS

#include "Audio.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "DSP/AlignedBuffer.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/SteamCapture/RTSSteamCaptureAudio.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWaveProcedural.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

namespace RTSSteamCaptureAudioTestConstants
{
	constexpr int32 LiveCaptureVideoFramesPerSecond = 60;
	constexpr int32 LiveCaptureVideoFrameCount = 30;
	constexpr float LiveCaptureMaximumDurationSeconds = 2.0f;
	constexpr float LiveCaptureWaitSeconds = 1.0f;
	constexpr int32 LiveToneSampleRate = 48000;
	constexpr int32 LiveToneChannelCount = 2;
	constexpr int32 LiveToneSampleFrameCount = LiveToneSampleRate;
	constexpr float LiveToneAmplitude = 0.5f;
	constexpr float LiveToneFrequencyHz = 440.0f;
	constexpr float LiveToneComparisonFrequencyHz = 1000.0f;
	constexpr double LiveToneMinimumDetectedAmplitude = 0.05;
	constexpr double LiveToneMinimumFrequencyRatio = 5.0;
	constexpr double SignalMetricTolerance = 0.0001;
	constexpr int32 SyntheticChannelCount = 2;
	constexpr int32 SyntheticSampleRate = 48000;
	constexpr int32 SyntheticVideoFramesPerSecond = 60;
	constexpr int32 SyntheticWrittenVideoFrameCount = 60;
	constexpr int32 SyntheticAdditionalSourceSampleFrames = 4800;
	constexpr float SyntheticLeftChannelAmplitude = 0.5f;
	constexpr float SyntheticRightChannelAmplitude = 0.25f;
	constexpr float SyntheticSignalFrequencyHz = 440.0f;
	constexpr double SyntheticExpectedRootMeanSquareAmplitude = 0.2795;
	constexpr double SyntheticSignalMetricTolerance = 0.002;
	const TCHAR* const DisableAppVolumeCvarName = TEXT("au.DisableAppVolume");
	const TCHAR* const AutomationDirectoryName = TEXT("Automation/SteamCaptureAudio");
}

namespace
{
	struct FMasterSubmixTestState
	{
		FAudioDeviceHandle M_AudioDevice;
		TStrongObjectPtr<USoundBase> M_AudibleTestSound;
		TStrongObjectPtr<UAudioComponent> M_AudioComponent;
		float M_PreviousAppVolumeMultiplier = 1.0f;
		int32 M_PreviousDisableAppVolume = 0;
		bool bM_RecordingStarted = false;
	};

	void RestoreTestOutputVolume(const FMasterSubmixTestState& TestState)
	{
		FApp::SetVolumeMultiplier(TestState.M_PreviousAppVolumeMultiplier);
		IConsoleVariable* DisableAppVolumeCvar = IConsoleManager::Get().FindConsoleVariable(
			RTSSteamCaptureAudioTestConstants::DisableAppVolumeCvarName);
		if (DisableAppVolumeCvar == nullptr)
		{
			return;
		}

		DisableAppVolumeCvar->Set(TestState.M_PreviousDisableAppVolume, ECVF_SetByCode);
	}

	void EnableTestOutputVolume(FMasterSubmixTestState& TestState)
	{
		TestState.M_PreviousAppVolumeMultiplier = FApp::GetVolumeMultiplier();
		IConsoleVariable* DisableAppVolumeCvar = IConsoleManager::Get().FindConsoleVariable(
			RTSSteamCaptureAudioTestConstants::DisableAppVolumeCvarName);
		if (DisableAppVolumeCvar != nullptr)
		{
			TestState.M_PreviousDisableAppVolume = DisableAppVolumeCvar->GetInt();
			DisableAppVolumeCvar->Set(1, ECVF_SetByCode);
		}
		FApp::SetVolumeMultiplier(1.0f);
	}

	USoundWaveProcedural* CreateAudibleTestTone()
	{
		USoundWaveProcedural* TestTone = NewObject<USoundWaveProcedural>();
		if (not IsValid(TestTone))
		{
			return nullptr;
		}

		TestTone->SetSampleRate(RTSSteamCaptureAudioTestConstants::LiveToneSampleRate);
		TestTone->NumChannels = RTSSteamCaptureAudioTestConstants::LiveToneChannelCount;
		TestTone->Duration = RTSSteamCaptureAudioTestConstants::LiveCaptureWaitSeconds;
		TestTone->bLooping = false;

		TArray<int16> TonePcmSamples;
		TonePcmSamples.SetNumUninitialized(
			RTSSteamCaptureAudioTestConstants::LiveToneSampleFrameCount
			* RTSSteamCaptureAudioTestConstants::LiveToneChannelCount);
		for (int32 SampleFrameIndex = 0;
		     SampleFrameIndex < RTSSteamCaptureAudioTestConstants::LiveToneSampleFrameCount;
		     ++SampleFrameIndex)
		{
			const float Phase = UE_TWO_PI
				* RTSSteamCaptureAudioTestConstants::LiveToneFrequencyHz
				* SampleFrameIndex
				/ RTSSteamCaptureAudioTestConstants::LiveToneSampleRate;
			const int16 ToneSample = static_cast<int16>(
				RTSSteamCaptureAudioTestConstants::LiveToneAmplitude
				* FMath::Sin(Phase)
				* MAX_int16);
			for (int32 ChannelIndex = 0;
			     ChannelIndex < RTSSteamCaptureAudioTestConstants::LiveToneChannelCount;
			     ++ChannelIndex)
			{
				TonePcmSamples[
					SampleFrameIndex * RTSSteamCaptureAudioTestConstants::LiveToneChannelCount
					+ ChannelIndex] = ToneSample;
			}
		}

		TestTone->QueueAudio(
			reinterpret_cast<const uint8*>(TonePcmSamples.GetData()),
			TonePcmSamples.Num() * sizeof(int16));
		return TestTone;
	}

	FString PrepareAudioTestWavePath(const TCHAR* const WaveFileName)
	{
		const FString TestDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			RTSSteamCaptureAudioTestConstants::AutomationDirectoryName);
		const FString WavePath = FPaths::Combine(TestDirectory, WaveFileName);
		IFileManager::Get().MakeDirectory(*TestDirectory, true);
		IFileManager::Get().Delete(*WavePath, false, true);
		return WavePath;
	}

	void TestLiveSignalMetrics(
		const FRTSSteamCaptureAudioResult& AudioResult,
		const FRTSSteamCaptureWaveInfo& WaveInfo,
		FAutomationTestBase& Test)
	{
		Test.TestTrue(TEXT("Live master-submix WAV contains audible sample data"), WaveInfo.bM_HasNonSilentSamples);
		Test.TestTrue(
			TEXT("On-disk peak matches the captured master mix"),
			FMath::IsNearlyEqual(
				WaveInfo.M_PeakAmplitude,
				AudioResult.M_PeakAmplitude,
				RTSSteamCaptureAudioTestConstants::SignalMetricTolerance));
		Test.TestTrue(
			TEXT("On-disk RMS matches the captured master mix"),
			FMath::IsNearlyEqual(
				WaveInfo.M_RootMeanSquareAmplitude,
				AudioResult.M_RootMeanSquareAmplitude,
				RTSSteamCaptureAudioTestConstants::SignalMetricTolerance));
	}

	bool LoadWavePcmForToneTest(
		const FString& WavePath,
		TArray<int16>& OutPcmSamples,
		int32& OutChannelCount,
		int32& OutSampleRate)
	{
		TArray<uint8> WaveBytes;
		FWaveModInfo ParsedWaveInfo;
		FString ParseError;
		if (not FFileHelper::LoadFileToArray(WaveBytes, *WavePath)
			|| not ParsedWaveInfo.ReadWaveInfo(WaveBytes.GetData(), WaveBytes.Num(), &ParseError)
			|| ParsedWaveInfo.pChannels == nullptr
			|| ParsedWaveInfo.pSamplesPerSec == nullptr
			|| ParsedWaveInfo.pBitsPerSample == nullptr
			|| ParsedWaveInfo.SampleDataStart == nullptr
			|| *ParsedWaveInfo.pBitsPerSample != sizeof(int16) * 8
			|| ParsedWaveInfo.SampleDataSize % sizeof(int16) != 0)
		{
			return false;
		}

		OutChannelCount = *ParsedWaveInfo.pChannels;
		OutSampleRate = *ParsedWaveInfo.pSamplesPerSec;
		OutPcmSamples.SetNumUninitialized(ParsedWaveInfo.SampleDataSize / sizeof(int16));
		FMemory::Memcpy(
			OutPcmSamples.GetData(),
			ParsedWaveInfo.SampleDataStart,
			ParsedWaveInfo.SampleDataSize);
		return OutChannelCount > 0 && OutSampleRate > 0 && not OutPcmSamples.IsEmpty();
	}

	double CalculateFirstChannelFrequencyAmplitude(
		const TArrayView<const int16> PcmSamples,
		const int32 ChannelCount,
		const int32 SampleRate,
		const float FrequencyHz)
	{
		const int32 SampleFrameCount = PcmSamples.Num() / ChannelCount;
		double SineProjection = 0.0;
		double CosineProjection = 0.0;
		for (int32 SampleFrameIndex = 0; SampleFrameIndex < SampleFrameCount; ++SampleFrameIndex)
		{
			const double Phase = UE_TWO_PI * FrequencyHz * SampleFrameIndex / SampleRate;
			const double Sample = static_cast<double>(PcmSamples[SampleFrameIndex * ChannelCount]) / MAX_int16;
			SineProjection += Sample * FMath::Sin(Phase);
			CosineProjection += Sample * FMath::Cos(Phase);
		}

		return 2.0 * FMath::Sqrt(
			SineProjection * SineProjection + CosineProjection * CosineProjection)
			/ SampleFrameCount;
	}

	void TestLiveWaveContainsKnownTone(
		const FString& WavePath,
		FAutomationTestBase& Test)
	{
		TArray<int16> PcmSamples;
		int32 ChannelCount = 0;
		int32 SampleRate = 0;
		const bool bLoadedPcm = LoadWavePcmForToneTest(
			WavePath,
			PcmSamples,
			ChannelCount,
			SampleRate);
		Test.TestTrue(TEXT("Live WAV PCM can be decoded for signal-content verification"), bLoadedPcm);
		if (not bLoadedPcm)
		{
			return;
		}

		const double ToneAmplitude = CalculateFirstChannelFrequencyAmplitude(
			PcmSamples,
			ChannelCount,
			SampleRate,
			RTSSteamCaptureAudioTestConstants::LiveToneFrequencyHz);
		const double ComparisonAmplitude = CalculateFirstChannelFrequencyAmplitude(
			PcmSamples,
			ChannelCount,
			SampleRate,
			RTSSteamCaptureAudioTestConstants::LiveToneComparisonFrequencyHz);
		Test.TestTrue(
			TEXT("Live WAV contains the injected 440 Hz master-output tone"),
			ToneAmplitude >= RTSSteamCaptureAudioTestConstants::LiveToneMinimumDetectedAmplitude);
		Test.TestTrue(
			TEXT("Injected tone dominates an unrelated comparison frequency"),
			ToneAmplitude >= ComparisonAmplitude
				* RTSSteamCaptureAudioTestConstants::LiveToneMinimumFrequencyRatio);
	}

	bool SaveAndValidateLiveWave(
		const TArrayView<const float> MasterOutputSamples,
		const int32 ChannelCount,
		const int32 SampleRate,
		FAutomationTestBase& Test)
	{
		const FString WavePath = PrepareAudioTestWavePath(TEXT("MasterSubmixRecording.wav"));
		FRTSSteamCaptureAudioResult AudioResult;
		FString SaveError;
		const bool bSaved = FRTSSteamCaptureAudio::SaveSynchronizedWave(
			MasterOutputSamples,
			ChannelCount,
			SampleRate,
			RTSSteamCaptureAudioTestConstants::LiveCaptureVideoFrameCount,
			RTSSteamCaptureAudioTestConstants::LiveCaptureVideoFramesPerSecond,
			WavePath,
			AudioResult,
			SaveError);
		Test.TestTrue(TEXT("Live master-submix samples save as a synchronized WAV"), bSaved);
		Test.TestTrue(TEXT("Live WAV save reports no error"), SaveError.IsEmpty());

		FRTSSteamCaptureWaveInfo WaveInfo;
		FString ValidationError;
		const bool bValidated = bSaved && FRTSSteamCaptureAudio::ValidateWaveFile(
			WavePath,
			ChannelCount,
			SampleRate,
			AudioResult.M_SynchronizedSampleFrameCount,
			WaveInfo,
			ValidationError);
		Test.TestTrue(TEXT("Live master-submix WAV validates from disk"), bValidated);
		Test.TestTrue(TEXT("Live WAV validation reports no error"), ValidationError.IsEmpty());
		if (bValidated)
		{
			TestLiveSignalMetrics(AudioResult, WaveInfo, Test);
			TestLiveWaveContainsKnownTone(WavePath, Test);
		}

		IFileManager::Get().Delete(*WavePath, false, true);
		return bValidated;
	}

	bool SaveAndValidateLiveMasterMix(
		FAudioDeviceHandle& AudioDevice,
		FAutomationTestBase& Test)
	{
		USoundSubmix& MainSubmix = AudioDevice->GetMainSubmixObject();
		float ChannelCount = 0.0f;
		float SampleRate = 0.0f;
		Audio::FAlignedFloatBuffer& MasterOutputSamples = AudioDevice->StopRecording(
			&MainSubmix,
			ChannelCount,
			SampleRate);
		const int32 RoundedChannelCount = FMath::RoundToInt(ChannelCount);
		const int32 RoundedSampleRate = FMath::RoundToInt(SampleRate);
		Test.TestTrue(TEXT("Main submix returned interleaved output samples"), not MasterOutputSamples.IsEmpty());
		Test.TestTrue(TEXT("Main submix returned an output channel count"), RoundedChannelCount > 0);
		Test.TestTrue(TEXT("Main submix returned a sample rate"), RoundedSampleRate > 0);
		if (MasterOutputSamples.IsEmpty() || RoundedChannelCount <= 0 || RoundedSampleRate <= 0)
		{
			return false;
		}

		return SaveAndValidateLiveWave(
			MasterOutputSamples,
			RoundedChannelCount,
			RoundedSampleRate,
			Test);
	}

	TArray<float> CreateSyntheticMasterMix()
	{
		const int32 SourceSampleFrameCount =
			RTSSteamCaptureAudioTestConstants::SyntheticSampleRate
			+ RTSSteamCaptureAudioTestConstants::SyntheticAdditionalSourceSampleFrames;
		TArray<float> MasterOutputSamples;
		MasterOutputSamples.SetNumUninitialized(
			SourceSampleFrameCount * RTSSteamCaptureAudioTestConstants::SyntheticChannelCount);
		for (int32 SampleFrameIndex = 0; SampleFrameIndex < SourceSampleFrameCount; ++SampleFrameIndex)
		{
			const float Phase = UE_TWO_PI
				* RTSSteamCaptureAudioTestConstants::SyntheticSignalFrequencyHz
				* SampleFrameIndex
				/ RTSSteamCaptureAudioTestConstants::SyntheticSampleRate;
			MasterOutputSamples[
				SampleFrameIndex * RTSSteamCaptureAudioTestConstants::SyntheticChannelCount] =
				RTSSteamCaptureAudioTestConstants::SyntheticLeftChannelAmplitude * FMath::Sin(Phase);
			MasterOutputSamples[
				SampleFrameIndex * RTSSteamCaptureAudioTestConstants::SyntheticChannelCount + 1] =
				RTSSteamCaptureAudioTestConstants::SyntheticRightChannelAmplitude * FMath::Cos(Phase);
		}
		return MasterOutputSamples;
	}

	void TestSyntheticWaveResults(
		const FRTSSteamCaptureAudioResult& AudioResult,
		const FRTSSteamCaptureWaveInfo& WaveInfo,
		FAutomationTestBase& Test)
	{
		Test.TestEqual(
			TEXT("WAV channel count"),
			WaveInfo.M_ChannelCount,
			RTSSteamCaptureAudioTestConstants::SyntheticChannelCount);
		Test.TestEqual(
			TEXT("WAV sample rate"),
			WaveInfo.M_SampleRate,
			RTSSteamCaptureAudioTestConstants::SyntheticSampleRate);
		Test.TestEqual(TEXT("WAV PCM bit depth"), WaveInfo.M_BitsPerSample, 16);
		Test.TestEqual(
			TEXT("WAV duration sample frames"),
			WaveInfo.M_SampleFrameCount,
			static_cast<int64>(RTSSteamCaptureAudioTestConstants::SyntheticSampleRate));
		Test.TestEqual(
			TEXT("Source tail trimmed to the exact 60-frame duration"),
			AudioResult.M_TrimmedSampleFrameCount,
			static_cast<int64>(RTSSteamCaptureAudioTestConstants::SyntheticAdditionalSourceSampleFrames));
		Test.TestEqual(
			TEXT("No padding needed for a longer source"),
			AudioResult.M_PaddedSampleFrameCount,
			static_cast<int64>(0));
		Test.TestTrue(TEXT("WAV contains non-silent PCM samples"), WaveInfo.bM_HasNonSilentSamples);
		Test.TestTrue(
			TEXT("Decoded WAV peak matches the source signal"),
			FMath::IsNearlyEqual(
				WaveInfo.M_PeakAmplitude,
				RTSSteamCaptureAudioTestConstants::SyntheticLeftChannelAmplitude,
				RTSSteamCaptureAudioTestConstants::SyntheticSignalMetricTolerance));
		Test.TestTrue(
			TEXT("Decoded WAV RMS matches the two-channel source signal"),
			FMath::IsNearlyEqual(
				WaveInfo.M_RootMeanSquareAmplitude,
				RTSSteamCaptureAudioTestConstants::SyntheticExpectedRootMeanSquareAmplitude,
				RTSSteamCaptureAudioTestConstants::SyntheticSignalMetricTolerance));
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FStartMasterSubmixRecordingCommand,
	FAutomationTestBase*,
	Test,
	TSharedRef<FMasterSubmixTestState>,
	TestState);

bool FStartMasterSubmixRecordingCommand::Update()
{
	if (not IsValid(GEngine))
	{
		Test->AddError(TEXT("The engine is unavailable for the master-submix test."));
		return true;
	}

	USoundBase* AudibleTestSound = CreateAudibleTestTone();
	if (not IsValid(AudibleTestSound))
	{
		Test->AddError(TEXT("The audible procedural tone could not be created for the master-submix test."));
		return true;
	}

	TestState->M_AudioDevice = GEngine->GetMainAudioDevice();
	if (not TestState->M_AudioDevice)
	{
		Test->AddError(TEXT("The main audio device is unavailable for the master-submix test."));
		return true;
	}

	FAudioDevice::FCreateComponentParams ComponentParams(TestState->M_AudioDevice.GetAudioDevice());
	ComponentParams.bAutoDestroy = false;
	ComponentParams.bPlay = false;
	UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(AudibleTestSound, ComponentParams);
	if (not IsValid(AudioComponent))
	{
		Test->AddError(TEXT("The audible test component could not be created on the main audio device."));
		return true;
	}

	TestState->M_AudibleTestSound = TStrongObjectPtr<USoundBase>(AudibleTestSound);
	TestState->M_AudioComponent = TStrongObjectPtr<UAudioComponent>(AudioComponent);
	EnableTestOutputVolume(*TestState);
	AudioComponent->bIsUISound = true;
	AudioComponent->bAllowSpatialization = false;
	USoundSubmix& MainSubmix = TestState->M_AudioDevice->GetMainSubmixObject();
	TestState->M_AudioDevice->StartRecording(
		&MainSubmix,
		RTSSteamCaptureAudioTestConstants::LiveCaptureMaximumDurationSeconds);
	TestState->bM_RecordingStarted = true;
	AudioComponent->Play();
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FStopAndValidateMasterSubmixRecordingCommand,
	FAutomationTestBase*,
	Test,
	TSharedRef<FMasterSubmixTestState>,
	TestState);

bool FStopAndValidateMasterSubmixRecordingCommand::Update()
{
	if (not TestState->M_AudioDevice
		|| not TestState->bM_RecordingStarted
		|| not TestState->M_AudioComponent.IsValid())
	{
		Test->AddError(TEXT("The master-submix test recording state became unavailable."));
		RestoreTestOutputVolume(*TestState);
		return true;
	}

	SaveAndValidateLiveMasterMix(TestState->M_AudioDevice, *Test);
	TestState->M_AudioComponent->Stop();
	RestoreTestOutputVolume(*TestState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRTSSteamCaptureWaveWriteAndSyncTest,
	"RTS.SteamCapture.Audio.WaveWriteAndSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRTSSteamCaptureWaveWriteAndSyncTest::RunTest(const FString& Parameters)
{
	const TArray<float> MasterOutputSamples = CreateSyntheticMasterMix();
	const FString WavePath = PrepareAudioTestWavePath(TEXT("WaveWriteAndSync.wav"));

	FRTSSteamCaptureAudioResult AudioResult;
	FString SaveError;
	const bool bSaved = FRTSSteamCaptureAudio::SaveSynchronizedWave(
		MasterOutputSamples,
		RTSSteamCaptureAudioTestConstants::SyntheticChannelCount,
		RTSSteamCaptureAudioTestConstants::SyntheticSampleRate,
		RTSSteamCaptureAudioTestConstants::SyntheticWrittenVideoFrameCount,
		RTSSteamCaptureAudioTestConstants::SyntheticVideoFramesPerSecond,
		WavePath,
		AudioResult,
		SaveError);
	TestTrue(TEXT("Synthetic master-mix samples save as a WAV"), bSaved);
	TestTrue(TEXT("WAV save reports no error"), SaveError.IsEmpty());

	FRTSSteamCaptureWaveInfo WaveInfo;
	FString ValidationError;
	const bool bValidated = bSaved && FRTSSteamCaptureAudio::ValidateWaveFile(
		WavePath,
		RTSSteamCaptureAudioTestConstants::SyntheticChannelCount,
		RTSSteamCaptureAudioTestConstants::SyntheticSampleRate,
		RTSSteamCaptureAudioTestConstants::SyntheticSampleRate,
		WaveInfo,
		ValidationError);
	TestTrue(TEXT("Saved WAV validates after being read from disk"), bValidated);
	TestTrue(TEXT("WAV validation reports no error"), ValidationError.IsEmpty());

	if (bValidated)
	{
		TestSyntheticWaveResults(AudioResult, WaveInfo, *this);
	}

	IFileManager::Get().Delete(*WavePath, false, true);
	return not HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRTSSteamCaptureSilencePaddingTest,
	"RTS.SteamCapture.Audio.SilencePaddingAt60Fps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRTSSteamCaptureSilencePaddingTest::RunTest(const FString& Parameters)
{
	constexpr int32 ChannelCount = 2;
	constexpr int32 SampleRate = 48000;
	constexpr int32 VideoFramesPerSecond = 60;
	constexpr int32 WrittenVideoFrameCount = 1;
	constexpr int64 ExpectedSampleFrameCount = SampleRate / VideoFramesPerSecond;
	const TArray<float> ShortMasterOutput = {0.25f, -0.25f};
	const FString WavePath = PrepareAudioTestWavePath(TEXT("SilencePadding.wav"));

	FRTSSteamCaptureAudioResult AudioResult;
	FString SaveError;
	const bool bSaved = FRTSSteamCaptureAudio::SaveSynchronizedWave(
		ShortMasterOutput,
		ChannelCount,
		SampleRate,
		WrittenVideoFrameCount,
		VideoFramesPerSecond,
		WavePath,
		AudioResult,
		SaveError);
	TestTrue(TEXT("A short master mix is padded to one 60 FPS video frame"), bSaved);
	TestEqual(
		TEXT("Padding reaches the exact video duration"),
		AudioResult.M_PaddedSampleFrameCount,
		ExpectedSampleFrameCount - 1);

	FRTSSteamCaptureWaveInfo WaveInfo;
	FString ValidationError;
	const bool bValidated = bSaved && FRTSSteamCaptureAudio::ValidateWaveFile(
		WavePath,
		ChannelCount,
		SampleRate,
		ExpectedSampleFrameCount,
		WaveInfo,
		ValidationError);
	TestTrue(TEXT("Silence-padded WAV validates from disk"), bValidated);
	TestTrue(TEXT("The retained source sample remains non-silent"), WaveInfo.bM_HasNonSilentSamples);

	IFileManager::Get().Delete(*WavePath, false, true);
	return not HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRTSSteamCaptureMasterSubmixRecordingTest,
	"RTS.SteamCapture.Audio.MasterSubmixRecording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRTSSteamCaptureMasterSubmixRecordingTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FMasterSubmixTestState> TestState = MakeShared<FMasterSubmixTestState>();
	ADD_LATENT_AUTOMATION_COMMAND(FStartMasterSubmixRecordingCommand(this, TestState));
	ADD_LATENT_AUTOMATION_COMMAND(
		FWaitLatentCommand(RTSSteamCaptureAudioTestConstants::LiveCaptureWaitSeconds));
	ADD_LATENT_AUTOMATION_COMMAND(FStopAndValidateMasterSubmixRecordingCommand(this, TestState));
	return true;
}

#endif
