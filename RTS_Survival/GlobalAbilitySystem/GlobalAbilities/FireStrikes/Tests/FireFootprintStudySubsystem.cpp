// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FireFootprintStudySubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFireFootprint, Log, All);

namespace FireFootprintPrivate
{
	constexpr float StartDelaySeconds = 5.f;
	constexpr float WarmupSeconds = 6.f;
	constexpr float FireSeconds = 9.f;
	// The next fire starts once the previous one is out (fire manager ticks once per second).
	constexpr float CaseSeconds = 11.f;
	constexpr float TopDownDistanceCm = 1800.f;
	constexpr float TopDownFieldOfView = 90.f;
	// Just short of straight down: an exact -90 pitch makes the yaw of the view undefined.
	constexpr float TopDownPitch = -89.9f;
	constexpr float RtsPitch = -62.f;
	constexpr float RtsDistanceCm = 3000.f;
	constexpr float RtsYaw = 35.f;
	constexpr int32 HumanPlayer = 1;
	constexpr float StudyDamagePerSecond = 1.f;
}

bool UFireFootprintStudySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FireFootprintStudy"));
}

TStatId UFireFootprintStudySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireFootprintStudySubsystem, STATGROUP_Tickables);
}

void UFireFootprintStudySubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (bM_Finished || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (not bM_Started)
	{
		TickStart();
		return;
	}
	TickCase();
}

float UFireFootprintStudySubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UFireFootprintStudySubsystem::TickStart()
{
	using namespace FireFootprintPrivate;
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
	if (M_StartSeconds < 0.f)
	{
		M_StartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_StartSeconds < StartDelaySeconds)
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
		M_Camera->GetCameraComponent()->SetFieldOfView(TopDownFieldOfView);
		DisableFogDarkening();
		// The radii and flame scales the incendiary weapons really use (VariantBatchThreeWeaponData, CPPGameState,
		// author_fire_strikes.py).
		M_Cases = {
			{TEXT("FlameCharge234_R300_V1.4"), 300.f, 1.4f},
			{TEXT("NapalmStummel_R350_V1.6"), 350.f, 1.6f},
			{TEXT("FireStrikes_R400_V3"), 400.f, 3.f},
			{TEXT("IncendiaryRockets_R450_V2"), 450.f, 2.f}};
		M_StartSeconds = GetNow();
		UE_LOG(LogRTSFireFootprint, Display,
		       TEXT("RTS_FIRE_FOOTPRINT CAMERA topdown_distance=%.0f fov=%.0f res_from_screenshot anchor=%s"),
		       TopDownDistanceCm, TopDownFieldOfView, *M_Anchor.ToCompactString());
		return;
	}
	AimTopDown();
	if (GetNow() - M_StartSeconds < WarmupSeconds)
	{
		return;
	}
	RequestScreenshot(TEXT("Empty_TopDown"));
	bM_Started = true;
	StartCase(0);
}

void UFireFootprintStudySubsystem::StartCase(const int32 CaseIndex)
{
	using namespace FireFootprintPrivate;
	M_CaseIndex = CaseIndex;
	M_CaseStartSeconds = GetNow();
	M_NextShot = 0;
	M_Shots = {{3.5f, true, TEXT("TopDown_A")}, {4.3f, true, TEXT("TopDown_B")}, {5.1f, true, TEXT("TopDown_C")},
	           {5.9f, false, TEXT("Rts")}};
	const FFootprintCase& Case = M_Cases[CaseIndex];
	URTSFireSubsystem* FireSubsystem = GetWorld()->GetSubsystem<URTSFireSubsystem>();
	FRTSGroundFireSettings Settings;
	Settings.RadiusCm = Case.RadiusCm;
	Settings.DurationSeconds = FireSeconds;
	Settings.DamagePerSecond = StudyDamagePerSecond;
	Settings.VfxScale = Case.VfxScale;
	M_AreaHandle = IsValid(FireSubsystem) ? FireSubsystem->RequestGroundFire(Settings, M_Anchor, HumanPlayer) : INDEX_NONE;
	UE_LOG(LogRTSFireFootprint, Display, TEXT("RTS_FIRE_FOOTPRINT CASE %d %s radius=%.0f vfx=%.2f handle=%d"), CaseIndex,
	       *Case.Label, Case.RadiusCm, Case.VfxScale, M_AreaHandle);
}

void UFireFootprintStudySubsystem::TickCase()
{
	using namespace FireFootprintPrivate;
	const float ElapsedSeconds = GetNow() - M_CaseStartSeconds;
	if (M_Shots.IsValidIndex(M_NextShot))
	{
		const FShot& Shot = M_Shots[M_NextShot];
		if (Shot.bTopDown)
		{
			AimTopDown();
		}
		else
		{
			AimRts();
		}
		// Aim one frame-ish ahead, shoot at the shot's time.
		if (ElapsedSeconds >= Shot.AtSeconds)
		{
			RequestScreenshot(FString::Printf(TEXT("%s_%s"), *M_Cases[M_CaseIndex].Label, *Shot.Suffix));
			++M_NextShot;
			if (M_Shots.IsValidIndex(M_NextShot) && M_Shots[M_NextShot].bTopDown != Shot.bTopDown)
			{
				// Switching views: the next shot waits at least a few frames on its own view.
				M_Shots[M_NextShot].AtSeconds = FMath::Max(M_Shots[M_NextShot].AtSeconds, ElapsedSeconds + 0.3f);
			}
		}
		return;
	}
	AimTopDown();
	if (ElapsedSeconds < CaseSeconds)
	{
		return;
	}
	if (M_Cases.IsValidIndex(M_CaseIndex + 1))
	{
		StartCase(M_CaseIndex + 1);
		return;
	}
	bM_Finished = true;
	UE_LOG(LogRTSFireFootprint, Display, TEXT("RTS_FIRE_FOOTPRINT DONE"));
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UFireFootprintStudySubsystem::AimTopDown() const
{
	using namespace FireFootprintPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(TopDownPitch, 0.f, 0.f);
	M_Camera->SetActorLocationAndRotation(M_Anchor + FVector(0.f, 0.f, TopDownDistanceCm), ViewRotation);
	Controller->SetViewTarget(M_Camera);
}

void UFireFootprintStudySubsystem::AimRts() const
{
	using namespace FireFootprintPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(RtsPitch, RtsYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(M_Anchor - ViewRotation.Vector() * RtsDistanceCm, ViewRotation);
	Controller->SetViewTarget(M_Camera);
}

void UFireFootprintStudySubsystem::DisableFogDarkening() const
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

void UFireFootprintStudySubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("FireFootprint") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSFireFootprint, Display, TEXT("RTS_FIRE_FOOTPRINT SCREENSHOT %s"), *FileName);
}

#else

bool UFireFootprintStudySubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UFireFootprintStudySubsystem::Tick(float DeltaTime) {}
TStatId UFireFootprintStudySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireFootprintStudySubsystem, STATGROUP_Tickables);
}

#endif
