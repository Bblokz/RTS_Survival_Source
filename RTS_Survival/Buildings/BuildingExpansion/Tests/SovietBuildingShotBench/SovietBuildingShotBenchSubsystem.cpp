// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietBuildingShotBenchSubsystem.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSovietBuildingShotBench, Log, All);

namespace SovietBuildingShotBenchPrivate
{
	const TCHAR* const BenchSwitch = TEXT("SovietBuildingShotBench");
	const TCHAR* const LocationSwitch = TEXT("SovietBuildingShotBenchAt=");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FString RusBuildingsFolder =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/");

	// Every Soviet base building Blueprint, children included, so inherited data is checked too.
	const TCHAR* const BuildingBlueprints[] =
	{
		TEXT("Rus_Building_CoolingTowers"),
		TEXT("Rus_Building_CoolingTowers_Soviet"),
		TEXT("Rus_Building_CoolingTowers_Concrete"),
		TEXT("Rus_Building_Barracks"),
		TEXT("Rus_Building_Barracks_Concrete"),
		TEXT("Rus_Building_LargeTent"),
		TEXT("Rus_Building_LargeTent_Soviet"),
		TEXT("Rus_Building_AmmoStorage"),
		TEXT("Rus_Building_FuelStorage"),
		TEXT("Rus_Building_FuelStorageSMall"),
		TEXT("Rus_Building_FuelStorageLarge"),
		TEXT("Rus_Building_FuelStorageTank"),
		TEXT("Rus_Building_FuelStorageTankSmall"),
		TEXT("Rus_Building_FuelStorage2"),
		TEXT("Rus_Building_Supplies"),
		TEXT("Rus_Building_ResearchCenter"),
		TEXT("Rus_Building_AdminBunker"),
		TEXT("Rus_Building_Factory"),
		TEXT("Rus_Building_PlatformFactory"),
		TEXT("AssemblyYard/Rus_Building_AssemblyYard"),
	};

	constexpr uint8 SovietPlayer = 2;
	constexpr uint8 GermanPlayer = 1;
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 600.0;
	constexpr float SettleGameSeconds = 5.f;
	// One reference shot per building per interval, slow enough for death logic between hits.
	constexpr float ShotIntervalGameSeconds = 0.25f;
	constexpr float ShootTimeoutGameSeconds = 120.f;
	constexpr float BuildingSpacingCm = 6000.f;
	constexpr int32 BuildingsPerRow = 5;
	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
	const FVector DefaultFieldLocation(16796.f, -15808.f, 0.f);
}

bool USovietBuildingShotBenchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), SovietBuildingShotBenchPrivate::BenchSwitch);
}

void USovietBuildingShotBenchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB started"));
}

TStatId USovietBuildingShotBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovietBuildingShotBenchSubsystem, STATGROUP_Tickables);
}

void USovietBuildingShotBenchSubsystem::Tick(float DeltaTime)
{
	using namespace SovietBuildingShotBenchPrivate;
	if (not bM_IsActive || M_Step == ESovietBuildingShotBenchStep::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxRunRealSeconds)
	{
		Finish(TEXT("timeout"));
		return;
	}
	KeepGameRunning();
	M_StepGameSeconds += DeltaTime;
	switch (M_Step)
	{
	case ESovietBuildingShotBenchStep::StartGame:
		if (RealSeconds >= HudBuildSeconds && TryStartGame())
		{
			M_Step = ESovietBuildingShotBenchStep::Spawn;
		}
		break;
	case ESovietBuildingShotBenchStep::Spawn:
		SpawnBuildings();
		break;
	case ESovietBuildingShotBenchStep::Settle:
		TickSettle();
		break;
	case ESovietBuildingShotBenchStep::Shoot:
		TickShoot();
		break;
	default:
		break;
	}
}

bool USovietBuildingShotBenchSubsystem::TryStartGame() const
{
	ACPPController* const PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not IsValid(PlayerController))
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	return true;
}

void USovietBuildingShotBenchSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(SovietBuildingShotBenchPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* const PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void USovietBuildingShotBenchSubsystem::SpawnBuildings()
{
	using namespace SovietBuildingShotBenchPrivate;
	ACPPGameState* const GameState = FRTS_Statics::GetGameState(this);
	const FWeaponData* const KwK40Data = IsValid(GameState)
		                                     ? GameState->GetWeaponDataOfPlayer(GermanPlayer, EWeaponName::KwK40_L48_75MM)
		                                     : nullptr;
	if (KwK40Data == nullptr)
	{
		Finish(TEXT("no_kwk40_data"));
		return;
	}
	// APHE is the KwK 40's default shell and fires at its stored damage.
	M_ShotDamage = KwK40Data->BaseDamage;

	FVector FieldLocation = DefaultFieldLocation;
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		FieldLocation = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	int32 Index = 0;
	for (const TCHAR* const BlueprintName : BuildingBlueprints)
	{
		const FVector Offset((Index % BuildingsPerRow) * BuildingSpacingCm, (Index / BuildingsPerRow) * BuildingSpacingCm,
		                     0.f);
		SpawnBuilding(BlueprintName, FieldLocation + Offset);
		++Index;
	}
	UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB spawned=%d shot_damage=%.1f"), M_Targets.Num(),
	       M_ShotDamage);
	M_Step = ESovietBuildingShotBenchStep::Settle;
	M_StepGameSeconds = 0.f;
}

void USovietBuildingShotBenchSubsystem::SpawnBuilding(const FString& BlueprintName, const FVector& Location)
{
	using namespace SovietBuildingShotBenchPrivate;
	const FString AssetName = FPaths::GetBaseFilename(BlueprintName);
	const FString ClassPath = RusBuildingsFolder + BlueprintName + TEXT(".") + AssetName + TEXT("_C");
	UClass* const BuildingClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (not IsValid(BuildingClass))
	{
		UE_LOG(LogRTSSovietBuildingShotBench, Error, TEXT("RTS_SBSB missing_class %s"), *ClassPath);
		return;
	}
	FVector GroundLocation = Location;
	FHitResult GroundHit;
	const FVector TraceStart = Location + FVector(0.f, 0.f, GroundTraceUpCm);
	const FVector TraceEnd = Location - FVector(0.f, 0.f, GroundTraceDownCm);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic))
	{
		GroundLocation = GroundHit.ImpactPoint;
	}
	AActor* const Building = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), BuildingClass, FTransform(GroundLocation), SovietPlayer);
	FSovietBuildingShotTarget Target;
	Target.Label = AssetName;
	Target.Building = Building;
	if (not IsValid(Building))
	{
		UE_LOG(LogRTSSovietBuildingShotBench, Error, TEXT("RTS_SBSB spawn_failed %s"), *AssetName);
		return;
	}
	M_Targets.Add(Target);
}

void USovietBuildingShotBenchSubsystem::TickSettle()
{
	if (M_StepGameSeconds < SovietBuildingShotBenchPrivate::SettleGameSeconds)
	{
		return;
	}
	for (FSovietBuildingShotTarget& Target : M_Targets)
	{
		AActor* const Building = Target.Building.Get();
		UHealthComponent* const Health = IsValid(Building) ? Building->FindComponentByClass<UHealthComponent>() : nullptr;
		if (not IsValid(Health))
		{
			UE_LOG(LogRTSSovietBuildingShotBench, Error, TEXT("RTS_SBSB no_health %s"), *Target.Label);
			Target.bDestroyed = true;
			continue;
		}
		Target.Health = Health;
		Target.MaxHealth = Health->GetMaxHealth();
		UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB start %s max_health=%.0f current=%.0f"),
		       *Target.Label, Target.MaxHealth, Health->GetCurrentHealth());
	}
	M_Step = ESovietBuildingShotBenchStep::Shoot;
	M_StepGameSeconds = 0.f;
}

void USovietBuildingShotBenchSubsystem::TickShoot()
{
	using namespace SovietBuildingShotBenchPrivate;
	if (M_StepGameSeconds > ShootTimeoutGameSeconds)
	{
		Finish(TEXT("shoot_timeout"));
		return;
	}
	if (M_StepGameSeconds < M_LastShotGameSeconds + ShotIntervalGameSeconds)
	{
		return;
	}
	M_LastShotGameSeconds = M_StepGameSeconds;
	bool bAnyStanding = false;
	for (FSovietBuildingShotTarget& Target : M_Targets)
	{
		if (Target.bDestroyed)
		{
			continue;
		}
		ShootTarget(Target);
		bAnyStanding = bAnyStanding || not Target.bDestroyed;
	}
	if (not bAnyStanding)
	{
		Finish(TEXT("all_destroyed"));
	}
}

void USovietBuildingShotBenchSubsystem::ShootTarget(FSovietBuildingShotTarget& Target) const
{
	AActor* const Building = Target.Building.Get();
	UHealthComponent* const Health = Target.Health.Get();
	if (not IsValid(Building) || not IsValid(Health))
	{
		Target.bDestroyed = true;
		UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB RESULT %s max_health=%.0f shots=%d (actor gone)"),
		       *Target.Label, Target.MaxHealth, Target.ShotsTaken);
		return;
	}
	// The same call a projectile makes on a hit actor without armor: full shell damage, kinetic point event.
	const FPointDamageEvent DamageEvent = FRTSWeaponHelpers::MakePointDamageEvent(
		ERTSDamageType::Kinetic, M_ShotDamage, FHitResult());
	const float HealthBefore = Health->GetCurrentHealth();
	Building->TakeDamage(M_ShotDamage, DamageEvent, nullptr, nullptr);
	++Target.ShotsTaken;
	const float HealthAfter = Health->GetCurrentHealth();
	UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB hit %s shot=%d applied=%.1f health=%.1f"),
	       *Target.Label, Target.ShotsTaken, HealthBefore - HealthAfter, HealthAfter);
	if (HealthAfter > 0.f)
	{
		return;
	}
	Target.bDestroyed = true;
	UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB RESULT %s max_health=%.0f shots=%d"),
	       *Target.Label, Target.MaxHealth, Target.ShotsTaken);
}

void USovietBuildingShotBenchSubsystem::Finish(const TCHAR* Outcome)
{
	M_Step = ESovietBuildingShotBenchStep::Done;
	bM_IsActive = false;
	UE_LOG(LogRTSSovietBuildingShotBench, Display, TEXT("RTS_SBSB done outcome=%s"), Outcome);
	FPlatformMisc::RequestExit(false, TEXT("SovietBuildingShotBench"));
}

#else

bool USovietBuildingShotBenchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void USovietBuildingShotBenchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

TStatId USovietBuildingShotBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USovietBuildingShotBenchSubsystem, STATGROUP_Tickables);
}

void USovietBuildingShotBenchSubsystem::Tick(float DeltaTime)
{
}

#endif
