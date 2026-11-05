// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FireEdgeTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireManager/RTSFireManager.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFireEdge, Log, All);

namespace FireEdgeTestPrivate
{
	constexpr int32 FirePlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr float StartDelaySeconds = 5.f;
	constexpr float WarmupSeconds = 6.f;
	constexpr float FireStartSeconds = 3.f;
	// After the first burn tick: the start sample of the soldiers inside is already below full health.
	constexpr float StartSampleSeconds = 4.5f;
	constexpr float EndSampleSeconds = 10.5f;
	constexpr float FireDurationSeconds = 14.f;
	constexpr float FireDamagePerSecond = 25.f;
	constexpr float CaseSpacingCm = 2200.f;
	constexpr float SpawnAboveGroundCm = 120.f;
	constexpr float MinInsideLoss = 20.f;
	// Health regained or lost by rounding only.
	constexpr float MaxOutsideLoss = 0.5f;
	constexpr float SecondsBetweenShots = 0.6f;
	constexpr float RtsPitch = -62.f;
	constexpr float RtsDistanceCm = 2200.f;
	constexpr float RtsYaw = 35.f;
	// Soldiers beyond the listed probes are held far outside.
	constexpr float ExtraProbeOffsetCm = 600.f;
	const TCHAR* const SquadClassPath = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
}

bool UFireEdgeTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FireEdgeValidate"));
}

TStatId UFireEdgeTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireEdgeTestSubsystem, STATGROUP_Tickables);
}

void UFireEdgeTestSubsystem::Tick(const float DeltaTime)
{
	using namespace FireEdgeTestPrivate;
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
	PinSoldiers();
	if (not bM_FiresStarted && GetRunSeconds() >= FireStartSeconds)
	{
		StartFires();
	}
	if (not bM_SampledStart && GetRunSeconds() >= StartSampleSeconds)
	{
		SampleHealth(true);
	}
	if (bM_SampledStart)
	{
		TickShots();
	}
	if (GetRunSeconds() >= EndSampleSeconds)
	{
		SampleHealth(false);
		CheckAndFinish();
	}
}

float UFireEdgeTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float UFireEdgeTestSubsystem::GetRunSeconds() const
{
	return GetNow() - M_RunStartSeconds;
}

void UFireEdgeTestSubsystem::TickStart()
{
	using namespace FireEdgeTestPrivate;
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
		DisableFogDarkening();
		BuildCases();
		M_StartSeconds = GetNow();
		return;
	}
	AimRts(M_Anchor);
	if (GetNow() - M_StartSeconds < WarmupSeconds)
	{
		return;
	}
	bM_Started = true;
	M_RunStartSeconds = GetNow();
	SpawnSquads();
}

void UFireEdgeTestSubsystem::BuildCases()
{
	using namespace FireEdgeTestPrivate;
	// The radii and flame scales the incendiary weapons really use (VariantBatchThreeWeaponData, CPPGameState,
	// author_fire_strikes.py).
	const TArray<TTuple<FString, float, float>> Weapons = {
		MakeTuple(FString(TEXT("FlameCharge234_R300")), 300.f, 1.4f),
		MakeTuple(FString(TEXT("NapalmStummel_R350")), 350.f, 1.6f),
		MakeTuple(FString(TEXT("FireStrikes_R400")), 400.f, 3.f),
		MakeTuple(FString(TEXT("IncendiaryRockets_R450")), 450.f, 2.f)};
	for (int32 CaseIndex = 0; CaseIndex < Weapons.Num(); ++CaseIndex)
	{
		FEdgeCase Case;
		Case.Label = Weapons[CaseIndex].Get<0>();
		Case.RadiusCm = Weapons[CaseIndex].Get<1>();
		Case.VfxScale = Weapons[CaseIndex].Get<2>();
		const float AlongX = (CaseIndex - (Weapons.Num() - 1) * 0.5f) * CaseSpacingCm;
		GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + FVector(AlongX, 0.f, 0.f), Case.Centre);
		// Inside the flames: half radius and 60 cm inside the edge (two sides). Outside: just beyond the flames
		// (80 cm past the edge; an infantry capsule is ~40 cm wide each side) and 1.5 m beyond.
		Case.Probes = {
			{0.5f, 0.f, 0.f, true}, {1.f, -60.f, 60.f, true}, {1.f, -60.f, 180.f, true},
			{1.f, 80.f, 240.f, false}, {1.f, 150.f, 300.f, false}};
		for (FProbe& Probe : Case.Probes)
		{
			const float Distance = Case.RadiusCm * Probe.RadiusShare + Probe.OffsetCm;
			const float Angle = FMath::DegreesToRadians(Probe.AngleDegrees);
			GlobalAbilityHelpers::TryFindGroundLocation(
				this, Case.Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance, Probe.Location);
		}
		M_Cases.Add(Case);
	}
}

void UFireEdgeTestSubsystem::SpawnSquads()
{
	using namespace FireEdgeTestPrivate;
	UWorld* World = GetWorld();
	UClass* SquadClass = LoadClass<AActor>(nullptr, SquadClassPath);
	if (not IsValid(SquadClass))
	{
		Check(false, TEXT("squad_class"), SquadClassPath);
		return;
	}
	for (FEdgeCase& Case : M_Cases)
	{
		Case.Squad = Cast<ASquadController>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*World, SquadClass, FTransform(Case.Centre + FVector(0.f, 0.f, SpawnAboveGroundCm)), EnemyPlayer));
	}
}

void UFireEdgeTestSubsystem::AssignProbes(FEdgeCase& Case) const
{
	using namespace FireEdgeTestPrivate;
	if (not Case.Squad.IsValid())
	{
		return;
	}
	const TArray<ASquadUnit*> Soldiers = Case.Squad->GetSquadUnitsChecked();
	int32 ProbeIndex = 0;
	for (ASquadUnit* Soldier : Soldiers)
	{
		if (not IsValid(Soldier))
		{
			continue;
		}
		FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, EnemyPlayer);
		if (Case.Probes.IsValidIndex(ProbeIndex))
		{
			Case.Probes[ProbeIndex].Soldier = Soldier;
		}
		else
		{
			// More soldiers than probes: held far outside, expected unharmed.
			FProbe Extra;
			Extra.RadiusShare = 1.f;
			Extra.OffsetCm = ExtraProbeOffsetCm;
			Extra.AngleDegrees = 120.f;
			Extra.Soldier = Soldier;
			GlobalAbilityHelpers::TryFindGroundLocation(
				this, Case.Centre + FVector(-0.5f, 0.866f, 0.f) * (Case.RadiusCm + ExtraProbeOffsetCm),
				Extra.Location);
			Case.Probes.Add(Extra);
		}
		++ProbeIndex;
	}
}

void UFireEdgeTestSubsystem::PinSoldiers()
{
	for (FEdgeCase& Case : M_Cases)
	{
		const bool bNeedsAssign = Case.Probes.IsEmpty() || not Case.Probes[0].Soldier.IsValid();
		if (bNeedsAssign && Case.Squad.IsValid() && not bM_SampledStart)
		{
			AssignProbes(Case);
		}
		for (const FProbe& Probe : Case.Probes)
		{
			ASquadUnit* Soldier = Probe.Soldier.Get();
			if (not IsValid(Soldier))
			{
				continue;
			}
			// Held on the spot every frame: the squad's own formation logic would walk them together again.
			const float HalfHeight = Soldier->GetSimpleCollisionHalfHeight();
			Soldier->SetActorLocation(Probe.Location + FVector(0.f, 0.f, HalfHeight), false, nullptr,
			                          ETeleportType::TeleportPhysics);
			if (UCharacterMovementComponent* Movement = Soldier->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}
	}
}

void UFireEdgeTestSubsystem::StartFires()
{
	using namespace FireEdgeTestPrivate;
	bM_FiresStarted = true;
	URTSFireSubsystem* FireSubsystem = GetWorld()->GetSubsystem<URTSFireSubsystem>();
	for (const FEdgeCase& Case : M_Cases)
	{
		FRTSGroundFireSettings Settings;
		Settings.RadiusCm = Case.RadiusCm;
		Settings.DurationSeconds = FireDurationSeconds;
		Settings.DamagePerSecond = FireDamagePerSecond;
		Settings.VfxScale = Case.VfxScale;
		const int32 Handle = IsValid(FireSubsystem)
			                     ? FireSubsystem->RequestGroundFire(Settings, Case.Centre, FirePlayer)
			                     : INDEX_NONE;
		const ARTSFireManager* FireManager = nullptr;
		for (TActorIterator<ARTSFireManager> It(GetWorld()); It; ++It)
		{
			FireManager = *It;
		}
		const FRTSGroundFireFlameLayout Layout = IsValid(FireManager)
			                                         ? FireManager->GetGroundFireFlameLayout(Settings)
			                                         : FRTSGroundFireFlameLayout();
		UE_LOG(LogRTSFireEdge, Display,
		       TEXT("RTS_FIRE_EDGE FIRE %s radius=%.0f vfx=%.2f handle=%d at=%s flames=%d ring=%d ring_radius=%.0f "
			       "flame_scale=%.2f"),
		       *Case.Label, Case.RadiusCm, Case.VfxScale, Handle, *Case.Centre.ToCompactString(), Layout.FlameCount,
		       Layout.RingCount, Layout.RingRadiusCm, Layout.FlameScale);
	}
}

void UFireEdgeTestSubsystem::SampleHealth(const bool bStart)
{
	if (bStart)
	{
		bM_SampledStart = true;
	}
	for (FEdgeCase& Case : M_Cases)
	{
		for (FProbe& Probe : Case.Probes)
		{
			const UHealthComponent* Health = Probe.Soldier.IsValid()
				                                 ? Probe.Soldier->FindComponentByClass<UHealthComponent>()
				                                 : nullptr;
			// A soldier burnt to death counts as zero health at the end.
			const float Value = IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
			if (bStart)
			{
				Probe.StartHealth = Probe.Soldier.IsValid() ? Value : -1.f;
				continue;
			}
			Probe.EndHealth = Value;
		}
	}
}

void UFireEdgeTestSubsystem::TickShots()
{
	if (bM_TookShots || GetNow() < M_NextShotSeconds)
	{
		return;
	}
	if (not M_Cases.IsValidIndex(M_ShotCase))
	{
		bM_TookShots = true;
		return;
	}
	// First aim, next round shoot: the view must be on the case for a frame before the picture.
	AimRts(M_Cases[M_ShotCase].Centre);
	if (not bM_ShotAimed)
	{
		bM_ShotAimed = true;
		M_NextShotSeconds = GetNow() + FireEdgeTestPrivate::SecondsBetweenShots * 0.5f;
		return;
	}
	bM_ShotAimed = false;
	RequestScreenshot(M_Cases[M_ShotCase].Label + TEXT("_Rts"));
	++M_ShotCase;
	M_NextShotSeconds = GetNow() + FireEdgeTestPrivate::SecondsBetweenShots * 0.5f;
}

void UFireEdgeTestSubsystem::CheckAndFinish()
{
	using namespace FireEdgeTestPrivate;
	bM_Finished = true;
	for (const FEdgeCase& Case : M_Cases)
	{
		for (const FProbe& Probe : Case.Probes)
		{
			const float Distance = FVector::Dist2D(Probe.Location, Case.Centre);
			const float Loss = Probe.StartHealth >= 0.f ? Probe.StartHealth - Probe.EndHealth : -1.f;
			const FString Description = FString::Printf(TEXT("%s.%s_at_%.0fcm"), *Case.Label,
			                                            Probe.bExpectBurn ? TEXT("burns") : TEXT("unharmed"),
			                                            Distance);
			const FString Details = FString::Printf(TEXT("radius=%.0f health %.0f -> %.0f (loss %.0f)"),
			                                        Case.RadiusCm, Probe.StartHealth, Probe.EndHealth, Loss);
			if (Probe.StartHealth < 0.f)
			{
				Check(false, Description, Details + TEXT(" no soldier held there"));
				continue;
			}
			Check(Probe.bExpectBurn ? Loss >= MinInsideLoss : Loss <= MaxOutsideLoss, Description, Details);
		}
	}
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSFireEdge, Display, TEXT("RTS_FIRE_EDGE RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UFireEdgeTestSubsystem::AimRts(const FVector& Target) const
{
	using namespace FireEdgeTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(RtsPitch, RtsYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(Target - ViewRotation.Vector() * RtsDistanceCm, ViewRotation);
	Controller->SetViewTarget(M_Camera);
}

void UFireEdgeTestSubsystem::DisableFogDarkening() const
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

void UFireEdgeTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSFireEdge, Display, TEXT("RTS_FIRE_EDGE CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSFireEdge, Error, TEXT("RTS_FIRE_EDGE CHECK FAIL %s %s"), *Description, *Details);
}

void UFireEdgeTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("FireEdgeTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSFireEdge, Display, TEXT("RTS_FIRE_EDGE SCREENSHOT %s"), *FileName);
}

#else

bool UFireEdgeTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UFireEdgeTestSubsystem::Tick(float DeltaTime) {}
TStatId UFireEdgeTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireEdgeTestSubsystem, STATGROUP_Tickables);
}

#endif
