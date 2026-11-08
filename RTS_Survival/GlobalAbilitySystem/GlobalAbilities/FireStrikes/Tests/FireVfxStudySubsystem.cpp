// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FireVfxStudySubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "DynamicRHI.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RenderTimer.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Components/PostProcessComponent.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FirePoolSettings/RTSFirePoolSettings.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFireVfxStudy, Log, All);

namespace FireVfxStudyPrivate
{
	constexpr float StartDelaySeconds = 5.f;
	constexpr float CameraWarmupSeconds = 6.f;
	// Flames need a few seconds to fill their area after activation.
	constexpr float GalleryBuildUpSeconds = 5.f;
	constexpr float SecondsBetweenShots = 0.6f;
	constexpr float PerfSeconds = 5.f;
	constexpr float RtsPitch = -62.f;
	constexpr float RtsDistance = 3000.f;
	constexpr float GroundPitch = -7.f;
	constexpr float GroundDistance = 1300.f;
	constexpr float CameraYaw = 35.f;
	constexpr int32 ItemsPerRow = 4;
	constexpr float ItemSpacingCm = 2400.f;
	constexpr float RowSpacingCm = 2400.f;
	constexpr float StudyFireSeconds = 120.f;
	constexpr float PerfFireSpacingCm = 1000.f;
	constexpr int32 HumanPlayer = 1;
	const TCHAR* const GroundFirePath = TEXT("/Game/RTS_Survival/SpecialEffects/Fire/RTS_GroundFire_0.RTS_GroundFire_0");
	const TCHAR* const FireloopDir = TEXT("/Game/RTS_Survival/SpecialEffects/M5VFXVOL2/Niagara/Reference/Fireloop/");
	const TCHAR* const FireDir = TEXT("/Game/RTS_Survival/SpecialEffects/Fire/");

	float Average(const TArray<float>& Values)
	{
		float Sum = 0.f;
		for (const float Value : Values)
		{
			Sum += Value;
		}
		return Values.IsEmpty() ? 0.f : Sum / Values.Num();
	}

	FRTSGroundFireSettings MakeGroundFire(const float RadiusCm, const float VfxScale)
	{
		FRTSGroundFireSettings Settings;
		Settings.RadiusCm = RadiusCm;
		Settings.DurationSeconds = StudyFireSeconds;
		Settings.DamagePerSecond = 1.f;
		Settings.VfxScale = VfxScale;
		return Settings;
	}
}

bool UFireVfxStudySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FireVfxStudy"));
}

TStatId UFireVfxStudySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireVfxStudySubsystem, STATGROUP_Tickables);
}

void UFireVfxStudySubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EFireVfxStudyPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EFireVfxStudyPhase::Gallery:
		TickGallery();
		return;
	case EFireVfxStudyPhase::PerfBaseline:
		TickPerf(M_Baseline);
		return;
	case EFireVfxStudyPhase::PerfBurning:
		TickPerf(M_Burning);
		return;
	case EFireVfxStudyPhase::Finished:
		return;
	}
}

float UFireVfxStudySubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UFireVfxStudySubsystem::SetPhase(const EFireVfxStudyPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
}

void UFireVfxStudySubsystem::TickWaitingForStart()
{
	using namespace FireVfxStudyPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return;
	}
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_PhaseStartSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not IsValid(M_Camera))
	{
		float AnchorX = 0.f;
		float AnchorY = 0.f;
		FParse::Value(FCommandLine::Get(), TEXT("FireStrikesAnchorX="), AnchorX);
		FParse::Value(FCommandLine::Get(), TEXT("FireStrikesAnchorY="), AnchorY);
		GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), M_Anchor);
		M_Camera = GetWorld()->SpawnActor<ACameraActor>();
		BuildItems();
		DisableFogDarkening();
		M_PhaseStartSeconds = GetNow();
		return;
	}
	AimCamera(M_Anchor, RtsPitch, RtsDistance * 2.f);
	if (GetNow() - M_PhaseStartSeconds < CameraWarmupSeconds)
	{
		return;
	}
	// The baseline is measured first, before any fire burns on the map.
	SetPhase(EFireVfxStudyPhase::PerfBaseline);
}

void UFireVfxStudySubsystem::DisableFogDarkening() const
{
	// Without units of the player the whole map is fogged; the fog post process would black out the pictures.
	for (TActorIterator<AFowManager> It(GetWorld()); It; ++It)
	{
		TArray<UPostProcessComponent*> PostProcesses;
		It->GetComponents<UPostProcessComponent>(PostProcesses);
		for (UPostProcessComponent* PostProcess : PostProcesses)
		{
			if (IsValid(PostProcess))
			{
				PostProcess->bEnabled = false;
			}
		}
	}
}

void UFireVfxStudySubsystem::BuildItems()
{
	using namespace FireVfxStudyPrivate;
	auto AddPool = [this](const TCHAR* Label, const float Radius, const float VfxScale)
	{
		FStudyItem Item;
		Item.Label = Label;
		Item.GroundFireRadius = Radius;
		Item.UserScale = VfxScale;
		M_Items.Add(Item);
	};
	auto AddSystem = [this](const FString& Label, const FString& Path, const FVector& Scale, const float UserScale)
	{
		FStudyItem Item;
		Item.Label = Label;
		Item.SystemPath = Path;
		Item.ComponentScale = Scale;
		Item.UserScale = UserScale;
		M_Items.Add(Item);
	};
	// The real path every incendiary weapon uses, at the radii of the fire strikes and of the incendiary weapons
	// (flamethrower 300 cm / 1.4, napalm 350 / 1.6, incendiary rockets 450 / 2, fire strikes 400 / 3, molotov-size 100).
	AddPool(TEXT("Pool_R400_Vfx3"), 400.f, 3.f);
	AddPool(TEXT("Pool_R450_Vfx2"), 450.f, 2.f);
	AddPool(TEXT("Pool_R300_Vfx1.4"), 300.f, 1.4f);
	AddPool(TEXT("Pool_R100_Vfx1"), 100.f, 1.f);
	// Upright flame candidates at uniform scale.
	const FString Bonefire = FString(FireloopDir) + TEXT("2_Nbonefire_pt.2_Nbonefire_pt");
	const FString BigFire = FString(FireloopDir) + TEXT("4_Nbigfire_pt.4_Nbigfire_pt");
	const FString Fire05 = FString(FireDir) + TEXT("N_RTS_Fire05_DitherAA.N_RTS_Fire05_DitherAA");
	const FString Fire01 = FString(FireDir) + TEXT("N_RTS_FIre01_DitherAA.N_RTS_FIre01_DitherAA");
	AddSystem(TEXT("Bonefire_S1"), Bonefire, FVector(1.f), 1.f);
	AddSystem(TEXT("Bonefire_S1.6"), Bonefire, FVector(1.6f), 1.f);
	AddSystem(TEXT("BigFire_S0.6"), BigFire, FVector(0.6f), 1.f);
	AddSystem(TEXT("BigFire_S1"), BigFire, FVector(1.f), 1.f);
	AddSystem(TEXT("Fire05_S1_P1"), Fire05, FVector(1.f), 1.f);
	AddSystem(TEXT("Fire05_S1_P2"), Fire05, FVector(1.f), 2.f);
	AddSystem(TEXT("Fire01_S1_P1.5"), Fire01, FVector(1.f), 1.5f);
	AddSystem(TEXT("GroundFire_Carpet_S1_P3"), GroundFirePath, FVector(1.f), 3.f);
	for (int32 ItemIndex = 0; ItemIndex < M_Items.Num(); ++ItemIndex)
	{
		const int32 Row = ItemIndex / ItemsPerRow;
		const int32 Column = ItemIndex % ItemsPerRow;
		// Rows from the anchor towards +Y: the test map's landscape ends about 3 m beyond Y = -24500.
		const FVector Offset((Column - 1.5f) * ItemSpacingCm, (Row - 1) * RowSpacingCm, 0.f);
		GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + Offset, M_Items[ItemIndex].Location);
	}
}

void UFireVfxStudySubsystem::SpawnGallery()
{
	using namespace FireVfxStudyPrivate;
	URTSFireSubsystem* FireSubsystem = GetWorld()->GetSubsystem<URTSFireSubsystem>();
	for (const FStudyItem& Item : M_Items)
	{
		if (Item.GroundFireRadius > 0.f)
		{
			const int32 Handle = IsValid(FireSubsystem)
				                     ? FireSubsystem->RequestGroundFire(
					                     MakeGroundFire(Item.GroundFireRadius, Item.UserScale), Item.Location,
					                     HumanPlayer)
				                     : INDEX_NONE;
			UE_LOG(LogRTSFireVfxStudy, Display, TEXT("RTS_FIRE_STUDY ITEM %s pool handle=%d at=%s"), *Item.Label,
			       Handle, *Item.Location.ToCompactString());
			continue;
		}
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Item.SystemPath);
		UNiagaraComponent* Component = IsValid(System)
			                               ? UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				                               GetWorld(), System, Item.Location, FRotator::ZeroRotator,
				                               Item.ComponentScale, false)
			                               : nullptr;
		if (IsValid(Component))
		{
			Component->SetVariableFloat(FName("Scale"), Item.UserScale);
			M_GalleryComponents.Add(Component);
		}
		UE_LOG(LogRTSFireVfxStudy, Display, TEXT("RTS_FIRE_STUDY ITEM %s system=%s spawned=%d at=%s"), *Item.Label,
		       *Item.SystemPath, IsValid(Component), *Item.Location.ToCompactString());
	}
}

void UFireVfxStudySubsystem::TickGallery()
{
	using namespace FireVfxStudyPrivate;
	if (GetNow() < M_NextShotSeconds)
	{
		return;
	}
	// Each item gets two shots: aim, wait a beat, shoot; even shot indices are RTS views, odd ones ground views.
	const int32 ShotCount = M_Items.Num() * 4;
	if (M_ShotIndex >= ShotCount)
	{
		ClearGallery();
		// At the cap the gallery's pool fires are replaced, so exactly the cap burns during the measurement.
		SetPhase(EFireVfxStudyPhase::PerfBurning);
		StartBurningPerf();
		return;
	}
	const int32 ItemIndex = M_ShotIndex / 4;
	const int32 Step = M_ShotIndex % 4;
	const FStudyItem& Item = M_Items[ItemIndex];
	const bool bGroundView = Step >= 2;
	AimCamera(Item.Location, bGroundView ? GroundPitch : RtsPitch, bGroundView ? GroundDistance : RtsDistance);
	if (Step % 2 == 1)
	{
		RequestScreenshot(FString::Printf(TEXT("%s_%s"), *Item.Label, bGroundView ? TEXT("Ground") : TEXT("Rts")));
	}
	++M_ShotIndex;
	M_NextShotSeconds = GetNow() + SecondsBetweenShots;
}

void UFireVfxStudySubsystem::AimCamera(const FVector& Target, const float Pitch, const float Distance) const
{
	using namespace FireVfxStudyPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(Pitch, CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(Target - ViewRotation.Vector() * Distance, ViewRotation);
	Controller->SetViewTarget(M_Camera);
}

void UFireVfxStudySubsystem::ClearGallery()
{
	for (UNiagaraComponent* Component : M_GalleryComponents)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	M_GalleryComponents.Reset();
}

void UFireVfxStudySubsystem::StartBurningPerf()
{
	using namespace FireVfxStudyPrivate;
	URTSFireSubsystem* FireSubsystem = GetWorld()->GetSubsystem<URTSFireSubsystem>();
	const URTSFirePoolSettings* Settings = GetDefault<URTSFirePoolSettings>();
	if (not IsValid(FireSubsystem) || not IsValid(Settings))
	{
		return;
	}
	// Spaced so none merge: the full cap burns at once, the worst case the settings allow.
	M_PerfFireCount = Settings->MaxActiveGroundFires;
	const int32 Columns = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(M_PerfFireCount)));
	for (int32 FireIndex = 0; FireIndex < M_PerfFireCount; ++FireIndex)
	{
		const FVector Offset((FireIndex % Columns - Columns / 2) * PerfFireSpacingCm,
		                     (FireIndex / Columns - Columns / 2) * PerfFireSpacingCm, 0.f);
		FVector Ground;
		GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + Offset, Ground);
		FireSubsystem->RequestGroundFire(MakeGroundFire(400.f, 3.f), Ground, HumanPlayer);
	}
	AimCamera(M_Anchor, RtsPitch, RtsDistance * 2.f);
}

void UFireVfxStudySubsystem::TickPerf(FPerfSamples& Samples)
{
	using namespace FireVfxStudyPrivate;
	const bool bBurning = M_Phase == EFireVfxStudyPhase::PerfBurning;
	const float SettleSeconds = bBurning ? GalleryBuildUpSeconds : 1.f;
	AimCamera(M_Anchor, RtsPitch, RtsDistance * 2.f);
	if (GetNow() - M_PhaseStartSeconds < SettleSeconds)
	{
		// The new fires build up to full size before sampling.
		return;
	}
	Samples.GameMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime)));
	Samples.RenderMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime)));
	Samples.GpuMs.Add(static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0))));
	if (GetNow() - M_PhaseStartSeconds < SettleSeconds + PerfSeconds)
	{
		return;
	}
	LogPerf(bBurning ? TEXT("burning") : TEXT("baseline"), Samples);
	if (not bBurning)
	{
		SpawnGallery();
		SetPhase(EFireVfxStudyPhase::Gallery);
		M_NextShotSeconds = GetNow() + GalleryBuildUpSeconds;
		return;
	}
	RequestScreenshot(TEXT("Perf_AllFires"));
	UE_LOG(LogRTSFireVfxStudy, Display, TEXT("RTS_FIRE_STUDY PERF fires=%d game_delta=%.3f render_delta=%.3f gpu_delta=%.3f ms"),
	       M_PerfFireCount, Average(M_Burning.GameMs) - Average(M_Baseline.GameMs),
	       Average(M_Burning.RenderMs) - Average(M_Baseline.RenderMs),
	       Average(M_Burning.GpuMs) - Average(M_Baseline.GpuMs));
	Finish();
}

void UFireVfxStudySubsystem::LogPerf(const TCHAR* Label, const FPerfSamples& Samples) const
{
	using namespace FireVfxStudyPrivate;
	UE_LOG(LogRTSFireVfxStudy, Display, TEXT("RTS_FIRE_STUDY PERF %s frames=%d game=%.3f render=%.3f gpu=%.3f ms"),
	       Label, Samples.GameMs.Num(), Average(Samples.GameMs), Average(Samples.RenderMs), Average(Samples.GpuMs));
}

void UFireVfxStudySubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("FireVfxStudy") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSFireVfxStudy, Display, TEXT("RTS_FIRE_STUDY SCREENSHOT %s"), *FileName);
}

void UFireVfxStudySubsystem::Finish()
{
	SetPhase(EFireVfxStudyPhase::Finished);
	UE_LOG(LogRTSFireVfxStudy, Display, TEXT("RTS_FIRE_STUDY DONE"));
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

#else

bool UFireVfxStudySubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UFireVfxStudySubsystem::Tick(float DeltaTime) {}
TStatId UFireVfxStudySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireVfxStudySubsystem, STATGROUP_Tickables);
}

#endif
