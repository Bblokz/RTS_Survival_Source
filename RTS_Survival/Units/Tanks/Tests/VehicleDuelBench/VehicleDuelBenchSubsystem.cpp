// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleDuelBenchSubsystem.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/Projectile/Tests/ProjectileMapTestHooks.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSVehicleDuelBench, Log, All);

namespace VehicleDuelBenchPrivate
{
	const TCHAR* const BenchSwitch = TEXT("VehicleDuelBench");
	const TCHAR* const ScenarioSwitch = TEXT("VehicleDuelBenchScenario=");
	const TCHAR* const RepeatsSwitch = TEXT("VehicleDuelBenchRepeats=");
	const TCHAR* const LocationSwitch = TEXT("VehicleDuelBenchAt=");
	const TCHAR* const DirectorySwitch = TEXT("VehicleDuelBenchDir=");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const WreckClassPrefix = TEXT("Env_Dest");
	const TCHAR* const AmbushBehaviourNamePart = TEXT("Ambush");

	constexpr uint8 GermanPlayer = 1;
	constexpr uint8 SovietPlayer = 2;
	constexpr int32 SideCount = 2;
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 3.0 * 3600.0;
	constexpr float SettleGameSeconds = 4.f;
	constexpr float FightTimeoutGameSeconds = 150.f;
	// Shells in flight and burning wrecks still land after the last unit of a side died.
	constexpr float AfterDefeatGameSeconds = 3.f;
	constexpr float CleanupGameSeconds = 3.f;
	constexpr float RetargetIntervalGameSeconds = 1.f;
	constexpr float AmbushConfirmGameSeconds = 1.f;
	// Default 26 m: inside the 34.8 m light cannon range, so every gun of every scenario reaches.
	constexpr float DefaultDuelDistanceCm = 2600.f;
	constexpr float LineSpacingCm = 1000.f;
	constexpr float SpawnLiftCm = 150.f;
	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
	constexpr float ArenaCleanupRadiusCm = 15000.f;
	constexpr int32 DefaultRepeats = 3;
	constexpr int32 MaxOwnerWalkSteps = 6;
	// Default arena on RTS_ToCopySmall: the flat field the turret toss audit spawns every tank on.
	const FVector DefaultArenaLocation(16796.f, -15808.f, 0.f);

	struct FDuelUnitSpec
	{
		ETankSubtype Subtype = ETankSubtype::Tank_None;
		// Picks the variant whose class name holds this text; null spawns the base unit.
		const TCHAR* VariantClassKeyword = nullptr;
		bool bActivatesAmbush = false;
		int32 Count = 1;
	};

	struct FDuelScenario
	{
		const TCHAR* Name = TEXT("");
		// Index 0: German side (player 1), index 1: Soviet side (player 2). Both lines face each other head on.
		FDuelUnitSpec Sides[SideCount];
		float DistanceCm = DefaultDuelDistanceCm;
	};

	const TArray<FDuelScenario>& GetScenarios()
	{
		static const TArray<FDuelScenario> Scenarios = {
			{TEXT("Tigers3_vs_T34_76x3"), {{ETankSubtype::Tank_Tiger, nullptr, false, 3},
			                               {ETankSubtype::Tank_T34_76, nullptr, false, 3}}},
			{TEXT("Tiger_vs_KV1"), {{ETankSubtype::Tank_Tiger}, {ETankSubtype::Tank_KV_1}}},
			{TEXT("PzIVG_vs_T34_76"), {{ETankSubtype::Tank_PzIV_G}, {ETankSubtype::Tank_T34_76}}},
			{TEXT("Pz38tRail_vs_T34_76"), {{ETankSubtype::Tank_Pz38t_RailGun}, {ETankSubtype::Tank_T34_76}}},
			{TEXT("PzIIF_vs_BA14"), {{ETankSubtype::Tank_PzII_F}, {ETankSubtype::Tank_Ba14}}},
			{TEXT("KingTiger_vs_IS2"), {{ETankSubtype::Tank_KingTiger}, {ETankSubtype::Tank_IS_2}}},
			{TEXT("E25HunterAmbush_vs_KV1"), {{ETankSubtype::Tank_E25, TEXT("Hunter"), true},
			                                  {ETankSubtype::Tank_KV_1}}},
			{TEXT("Pz38tRailOvercharge_vs_KV1"), {{ETankSubtype::Tank_Pz38t_RailGun, TEXT("Overcharge")},
			                                      {ETankSubtype::Tank_KV_1}}},
		};
		return Scenarios;
	}

	const FDuelUnitSpec& GetSpec(const int32 ScenarioIndex, const int32 Side)
	{
		return GetScenarios()[ScenarioIndex].Sides[Side];
	}

	FString GetVerdictName(const EMapTestArmorHitVerdict Verdict)
	{
		switch (Verdict)
		{
		case EMapTestArmorHitVerdict::Bounce:
			return TEXT("Bounce");
		case EMapTestArmorHitVerdict::Penetration:
			return TEXT("Penetration");
		case EMapTestArmorHitVerdict::OverPenetration:
			return TEXT("OverPenetration");
		case EMapTestArmorHitVerdict::ArmorDestroyed:
			return TEXT("ArmorDestroyed");
		case EMapTestArmorHitVerdict::FlankAmmoDetonation:
			return TEXT("FlankAmmoDetonation");
		default:
			return TEXT("Unknown");
		}
	}

	FString GetSideName(const int32 Side)
	{
		return Side == 0 ? TEXT("German") : TEXT("Soviet");
	}

	FString FormatSeconds(const float Seconds)
	{
		return Seconds < 0.f ? FString(TEXT("-")) : FString::Printf(TEXT("%.1f"), Seconds);
	}

	/** @return The tank that owns the weapon: the weapon owner is the tank, one of its turrets or a component. */
	ATankMaster* GetTankOfWeaponOwner(const UObject* WeaponOwner)
	{
		const AActor* Actor = Cast<AActor>(WeaponOwner);
		if (Actor == nullptr)
		{
			const UActorComponent* const Component = Cast<UActorComponent>(WeaponOwner);
			Actor = Component != nullptr ? Component->GetOwner() : nullptr;
		}
		for (int32 Step = 0; Step < MaxOwnerWalkSteps && Actor != nullptr; ++Step)
		{
			if (const ATankMaster* const Tank = Cast<ATankMaster>(Actor))
			{
				return const_cast<ATankMaster*>(Tank);
			}
			const AActor* const AttachParent = Actor->GetAttachParentActor();
			Actor = AttachParent != nullptr ? AttachParent : Actor->GetOwner();
		}
		return nullptr;
	}
}

bool UVehicleDuelBenchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), VehicleDuelBenchPrivate::BenchSwitch);
}

void UVehicleDuelBenchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace VehicleDuelBenchPrivate;
	Super::OnWorldBeginPlay(InWorld);
	M_ArenaCentre = DefaultArenaLocation;
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		M_ArenaCentre = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	M_Repeats = DefaultRepeats;
	FParse::Value(FCommandLine::Get(), RepeatsSwitch, M_Repeats);
	M_Repeats = FMath::Max(1, M_Repeats);
	FParse::Value(FCommandLine::Get(), ScenarioSwitch, M_ScenarioFilter);
	M_OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VehicleDuelBench"),
	                                    FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	M_ShellLaunchedHandle = FProjectileMapTestHooks::OnShellLaunched().AddUObject(
		this, &UVehicleDuelBenchSubsystem::OnShellLaunched);
	M_ArmorHitHandle = FProjectileMapTestHooks::OnArmorHit().AddUObject(this, &UVehicleDuelBenchSubsystem::OnArmorHit);
	TWeakObjectPtr<UVehicleDuelBenchSubsystem> WeakThis(this);
	M_ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
		[WeakThis](AActor* SpawnedActor)
		{
			const bool bRunGoing = WeakThis.IsValid() && (WeakThis->M_Step == EVehicleDuelBenchStep::Fight
				|| WeakThis->M_Step == EVehicleDuelBenchStep::Settle);
			if (bRunGoing)
			{
				WeakThis->M_RunSpawnedActors.Add(SpawnedActor);
			}
		}));
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB started dir=%s arena=%s repeats=%d filter=%s"),
	       *M_OutputDirectory, *M_ArenaCentre.ToCompactString(), M_Repeats, *M_ScenarioFilter);
}

void UVehicleDuelBenchSubsystem::Deinitialize()
{
	FProjectileMapTestHooks::OnShellLaunched().Remove(M_ShellLaunchedHandle);
	FProjectileMapTestHooks::OnArmorHit().Remove(M_ArmorHitHandle);
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

TStatId UVehicleDuelBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVehicleDuelBenchSubsystem, STATGROUP_Tickables);
}

void UVehicleDuelBenchSubsystem::Tick(float DeltaTime)
{
	using namespace VehicleDuelBenchPrivate;
	if (not bM_IsActive || M_Step == EVehicleDuelBenchStep::Done || not IsValid(GetWorld()))
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
	if (M_Step == EVehicleDuelBenchStep::Settle || M_Step == EVehicleDuelBenchStep::Fight)
	{
		KeepGermanUnitsInEnemyVision();
	}
	switch (M_Step)
	{
	case EVehicleDuelBenchStep::StartGame:
		if (RealSeconds >= HudBuildSeconds && TryStartGame())
		{
			if (not ForceRealisticVehicleBalance())
			{
				Finish(TEXT("realistic_balance_not_set"));
				return;
			}
			BuildRunList();
			OpenReportFiles();
			M_Step = M_RunScenarioIndices.Num() > 0 ? EVehicleDuelBenchStep::SpawnRun : EVehicleDuelBenchStep::Done;
			if (M_Step == EVehicleDuelBenchStep::Done)
			{
				Finish(TEXT("no_scenario_matches_filter"));
			}
		}
		break;
	case EVehicleDuelBenchStep::SpawnRun:
		SpawnRun();
		break;
	case EVehicleDuelBenchStep::Settle:
		TickSettle();
		break;
	case EVehicleDuelBenchStep::Fight:
		TickFight();
		break;
	case EVehicleDuelBenchStep::Cleanup:
		TickCleanup();
		break;
	default:
		break;
	}
}

bool UVehicleDuelBenchSubsystem::TryStartGame()
{
	M_PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	return true;
}

void UVehicleDuelBenchSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(VehicleDuelBenchPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* PlayerController = M_PlayerController.Get();
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UVehicleDuelBenchSubsystem::ForceRealisticVehicleBalance() const
{
	if (URTSGameInstance* const GameInstance = Cast<URTSGameInstance>(GetWorld()->GetGameInstance()))
	{
		GameInstance->SetVehicleBalance(ERTSVehicleBalance::Realistic);
	}
	ACPPGameState* const GameState = GetWorld()->GetGameState<ACPPGameState>();
	if (not IsValid(GameState))
	{
		return false;
	}
	GameState->SetVehicleBalance(ERTSVehicleBalance::Realistic);
	return ACPPGameState::GetIsRealisticVehicleBalance(this);
}

void UVehicleDuelBenchSubsystem::BuildRunList()
{
	using namespace VehicleDuelBenchPrivate;
	const TArray<FDuelScenario>& Scenarios = GetScenarios();
	for (int32 ScenarioIndex = 0; ScenarioIndex < Scenarios.Num(); ++ScenarioIndex)
	{
		if (not M_ScenarioFilter.IsEmpty() && not M_ScenarioFilter.Equals(Scenarios[ScenarioIndex].Name,
		                                                                   ESearchCase::IgnoreCase))
		{
			continue;
		}
		for (int32 Repeat = 1; Repeat <= M_Repeats; ++Repeat)
		{
			M_RunScenarioIndices.Add(ScenarioIndex);
			M_RunRepeatNumbers.Add(Repeat);
		}
	}
	// Base units must spawn as their base class whatever variant this machine's player picked last.
	const UUnitVariantDataAsset* const VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
	ARTSAsyncSpawner* const Spawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	M_VariantDataAsset = VariantDataAsset;
	if (not IsValid(VariantDataAsset) || not IsValid(Spawner))
	{
		return;
	}
	for (const FTrainingOption& Unit : VariantDataAsset->GetUnitsWithChoice())
	{
		(void)Spawner->SetPlayerUnitVariant(*VariantDataAsset, Unit, EUnitVariant::Base);
	}
}

void UVehicleDuelBenchSubsystem::OpenReportFiles()
{
	M_HitsCsvPath = FPaths::Combine(M_OutputDirectory, TEXT("hits.csv"));
	M_SummaryPath = FPaths::Combine(M_OutputDirectory, TEXT("summary.md"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*M_OutputDirectory);
	FFileHelper::SaveStringToFile(
		TEXT("scenario,repeat,fight_s,shell_id,shooter,weapon,calibre_mm,shell,victim,plate,raw_armor,effective_armor,"
			"pen_at_impact,pen_to_armor,shell_damage,damage_mlt,verdict,health_before,health_after,health_lost\n"),
		*M_HitsCsvPath);
	M_SummaryLines.Add(TEXT("# Vehicle duel bench (Realistic vehicle balance)"));
	M_SummaryLines.Add(FString::Printf(TEXT("Started %s, %d repeats per scenario, arena %s."),
	                                   *FDateTime::Now().ToString(), M_Repeats, *M_ArenaCentre.ToCompactString()));
	M_SummaryLines.Add(TEXT("Times are seconds since the units spawned facing each other (they open fire at once; attack orders follow"
		" after 4 s). Damage is health really lost to shell hits;"
		" damage taken from other sources (machine guns, cook-off and engine fires) is listed apart."));
}

void UVehicleDuelBenchSubsystem::SpawnRun()
{
	using namespace VehicleDuelBenchPrivate;
	const int32 ScenarioIndex = M_RunScenarioIndices[M_RunIndex];
	M_Units.Reset();
	M_RunSpawnedActors.Reset();
	M_ShellShooterIndices.Reset();
	bM_RunFailed = false;
	M_SideDefeatedGameSeconds = -1.f;
	FVector ArenaCentre = M_ArenaCentre;
	float GroundZ = 0.f;
	if (not TryGetGroundZ(ArenaCentre, GroundZ))
	{
		Finish(TEXT("no_ground_at_arena"));
		return;
	}
	ArenaCentre.Z = GroundZ;
	for (int32 Side = 0; Side < SideCount; ++Side)
	{
		SpawnSide(ScenarioIndex, Side, ArenaCentre);
	}
	UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB run %d/%d scenario=%s repeat=%d units=%d"), M_RunIndex + 1,
	       M_RunScenarioIndices.Num(), GetScenarios()[ScenarioIndex].Name, M_RunRepeatNumbers[M_RunIndex],
	       M_Units.Num());
	M_Step = EVehicleDuelBenchStep::Settle;
	M_StepStartGameSeconds = GetGameSeconds();
	// Units open fire on anything in range as soon as they spawn, so the clock and the hit log start here.
	M_FightStartGameSeconds = GetGameSeconds();
}

void UVehicleDuelBenchSubsystem::SpawnSide(const int32 ScenarioIndex, const int32 Side, const FVector& ArenaCentre)
{
	using namespace VehicleDuelBenchPrivate;
	const FDuelUnitSpec& Spec = GetSpec(ScenarioIndex, Side);
	const float HalfDistance = GetScenarios()[ScenarioIndex].DistanceCm * 0.5f;
	const float SideSign = Side == 0 ? -1.f : 1.f;
	const FRotator Facing(0.f, Side == 0 ? 0.f : 180.f, 0.f);
	for (int32 CopyIndex = 0; CopyIndex < Spec.Count; ++CopyIndex)
	{
		const float LineOffset = (CopyIndex - (Spec.Count - 1) * 0.5f) * LineSpacingCm;
		FVector Location = ArenaCentre + FVector(SideSign * HalfDistance, LineOffset, 0.f);
		float GroundZ = ArenaCentre.Z;
		(void)TryGetGroundZ(Location, GroundZ);
		Location.Z = GroundZ + SpawnLiftCm;
		if (not SpawnDuelUnit(ScenarioIndex, Side, Side, CopyIndex, Location, Facing))
		{
			bM_RunFailed = true;
		}
	}
}

bool UVehicleDuelBenchSubsystem::SpawnDuelUnit(const int32 ScenarioIndex, const int32 SpecIndex, const int32 Side,
                                               const int32 CopyIndex, const FVector& Location, const FRotator& Facing)
{
	using namespace VehicleDuelBenchPrivate;
	const FDuelUnitSpec& Spec = GetSpec(ScenarioIndex, SpecIndex);
	UClass* const UnitClass = ResolveUnitClass(ScenarioIndex, SpecIndex);
	const FString SubtypeName = Global_GetTankSubtypeEnumAsString(Spec.Subtype);
	if (not IsValid(UnitClass))
	{
		UE_LOG(LogRTSVehicleDuelBench, Error, TEXT("RTS_VDB no class for %s keyword=%s"), *SubtypeName,
		       Spec.VariantClassKeyword != nullptr ? Spec.VariantClassKeyword : TEXT("base"));
		return false;
	}
	const uint8 OwningPlayer = Side == 0 ? GermanPlayer : SovietPlayer;
	ATankMaster* const Tank = Cast<ATankMaster>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), UnitClass, FTransform(Facing, Location), OwningPlayer));
	if (not IsValid(Tank))
	{
		UE_LOG(LogRTSVehicleDuelBench, Error, TEXT("RTS_VDB spawn failed class=%s"), *UnitClass->GetName());
		return false;
	}
	FVehicleDuelUnit& Unit = M_Units.AddDefaulted_GetRef();
	Unit.Tank = Tank;
	Unit.TankUniqueId = Tank->GetUniqueID();
	Unit.ClassName = UnitClass->GetName();
	Unit.ClassName.RemoveFromEnd(TEXT("_C"));
	Unit.Label = Spec.Count > 1 ? FString::Printf(TEXT("%s#%d"), *Unit.ClassName, CopyIndex + 1) : Unit.ClassName;
	Unit.Side = Side;
	Unit.bActivatesAmbush = Spec.bActivatesAmbush;
	const ACPPGameState* const GameState = GetWorld()->GetGameState<ACPPGameState>();
	Unit.ExpectedRealisticMaxHealth = IsValid(GameState)
		                                  ? GameState->GetTankDataOfPlayer(OwningPlayer, Spec.Subtype).MaxHealth
		                                  : 0.f;
	return true;
}

UClass* UVehicleDuelBenchSubsystem::ResolveUnitClass(const int32 ScenarioIndex, const int32 SpecIndex) const
{
	using namespace VehicleDuelBenchPrivate;
	const FDuelUnitSpec& Spec = GetSpec(ScenarioIndex, SpecIndex);
	const FTrainingOption Option(EAllUnitType::UNType_Tank, static_cast<uint8>(Spec.Subtype));
	if (Spec.VariantClassKeyword == nullptr)
	{
		ARTSAsyncSpawner* const Spawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
		return IsValid(Spawner) ? Spawner->GetTrainingOptionClass(Option).LoadSynchronous() : nullptr;
	}
	const UUnitVariantDataAsset* const VariantDataAsset = M_VariantDataAsset.Get();
	if (not IsValid(VariantDataAsset))
	{
		return nullptr;
	}
	for (const EUnitVariant Variant : {EUnitVariant::VariantA, EUnitVariant::VariantB, EUnitVariant::VariantC})
	{
		UClass* const VariantClass = VariantDataAsset->FindVariantClass(Option, Variant).LoadSynchronous();
		if (IsValid(VariantClass) && VariantClass->GetName().Contains(Spec.VariantClassKeyword))
		{
			return VariantClass;
		}
	}
	return nullptr;
}

void UVehicleDuelBenchSubsystem::TickSettle()
{
	using namespace VehicleDuelBenchPrivate;
	RecordDeaths();
	if (GetGameSeconds() - M_StepStartGameSeconds < SettleGameSeconds)
	{
		return;
	}
	for (FVehicleDuelUnit& Unit : M_Units)
	{
		if (Unit.DeathSeconds >= 0.f)
		{
			// Killed before the attack orders by a shot fired on sight.
			Unit.MaxHealth = Unit.ExpectedRealisticMaxHealth;
			continue;
		}
		CheckRealisticMaxHealth(Unit);
	}
	if (not ACPPGameState::GetIsRealisticVehicleBalance(this))
	{
		// The bench is only worth anything in Realistic.
		bM_RunFailed = true;
	}
	if (bM_RunFailed || M_Units.Num() == 0)
	{
		FinishRun(TEXT("setup_failed"));
		return;
	}
	StartFight();
}

void UVehicleDuelBenchSubsystem::CheckRealisticMaxHealth(FVehicleDuelUnit& Unit) const
{
	using namespace VehicleDuelBenchPrivate;
	const ATankMaster* const Tank = Unit.Tank.Get();
	if (not IsValid(Tank) || not IsValid(Tank->GetHealthComponent()))
	{
		return;
	}
	Unit.MaxHealth = Tank->GetHealthComponent()->GetMaxHealth();
	const URTSComponent* const RTSComponent = Tank->FindComponentByClass<URTSComponent>();
	const ACPPGameState* const GameState = GetWorld()->GetGameState<ACPPGameState>();
	if (FMath::IsNearlyEqual(Unit.MaxHealth, Unit.ExpectedRealisticMaxHealth, 1.f) || not IsValid(RTSComponent)
		|| not IsValid(GameState))
	{
		return;
	}
	// A Blueprint set up as another subtype gets that subtype's health; name it so the data can be fixed.
	const ETankSubtype ReportedSubtype = RTSComponent->GetSubtypeAsTankSubtype();
	const int32 OwningPlayer = Unit.Side == 0 ? GermanPlayer : SovietPlayer;
	const float ReportedSubtypeHealth = GameState->GetTankDataOfPlayer(OwningPlayer, ReportedSubtype).MaxHealth;
	Unit.HealthNote = FString::Printf(
		TEXT("max HP %.0f, but the scenario subtype's Realistic data says %.0f; the Blueprint reports %s (data %.0f)"),
		Unit.MaxHealth, Unit.ExpectedRealisticMaxHealth, *Global_GetTankSubtypeEnumAsString(ReportedSubtype),
		ReportedSubtypeHealth);
	UE_LOG(LogRTSVehicleDuelBench, Warning, TEXT("RTS_VDB %s %s"), *Unit.Label, *Unit.HealthNote);
}

void UVehicleDuelBenchSubsystem::StartFight()
{
	using namespace VehicleDuelBenchPrivate;
	TArray<int32> SideCopyCounters = {0, 0};
	for (FVehicleDuelUnit& Unit : M_Units)
	{
		// Each tank aims at the tank opposite it in the other line; the retarget pass covers the rest.
		const int32 CopyIndex = SideCopyCounters[Unit.Side]++;
		int32 EnemyCopy = 0;
		for (FVehicleDuelUnit& Enemy : M_Units)
		{
			if (Enemy.Side != Unit.Side && EnemyCopy++ == CopyIndex)
			{
				Unit.Target = Enemy.Tank;
			}
		}
	}
	M_Step = EVehicleDuelBenchStep::Fight;
	M_LastRetargetGameSeconds = -RetargetIntervalGameSeconds;
	bM_IsFirstTargetPass = true;
	RetargetUnits();
	bM_IsFirstTargetPass = false;
	for (FVehicleDuelUnit& Unit : M_Units)
	{
		if (Unit.bActivatesAmbush)
		{
			ActivateAmbush(Unit);
		}
	}
}

void UVehicleDuelBenchSubsystem::KeepGermanUnitsInEnemyVision() const
{
	// The enemy's fog of war tags player units it sees some seconds late; tagging them here lets both sides open
	// fire at the same moment, like two players who both see the other line.
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	for (const FVehicleDuelUnit& Unit : M_Units)
	{
		ATankMaster* const Tank = Unit.Tank.Get();
		if (Unit.Side == 0 && IsValid(Tank) && not Tank->Tags.Contains(TagUnitInEnemyVision))
		{
			Tank->Tags.Add(TagUnitInEnemyVision);
		}
	}
}

void UVehicleDuelBenchSubsystem::ActivateAmbush(FVehicleDuelUnit& Unit) const
{
	ICommands* const Commands = Cast<ICommands>(Unit.Tank.Get());
	if (Commands == nullptr)
	{
		return;
	}
	const ECommandQueueError Error = Commands->ActivateBehaviourAbility(EBehaviourAbilityType::E25HunterAmbush, true);
	UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB %s ambush order error=%d"), *Unit.Label,
	       static_cast<int32>(Error));
}

void UVehicleDuelBenchSubsystem::TickFight()
{
	using namespace VehicleDuelBenchPrivate;
	RecordDeaths();
	if (GetFightSeconds() >= AmbushConfirmGameSeconds)
	{
		for (FVehicleDuelUnit& Unit : M_Units)
		{
			const UBehaviourComp* const Behaviours = Unit.Tank.IsValid()
				                                         ? Unit.Tank->FindComponentByClass<UBehaviourComp>()
				                                         : nullptr;
			if (not Unit.bActivatesAmbush || Unit.bAmbushConfirmed || not IsValid(Behaviours))
			{
				continue;
			}
			for (const UBehaviour* const Behaviour : Behaviours->GetBehaviours())
			{
				Unit.bAmbushConfirmed |= IsValid(Behaviour) && Behaviour->GetClass()->GetName().Contains(
					AmbushBehaviourNamePart);
			}
		}
	}
	if (M_SideDefeatedGameSeconds < 0.f && (GetIsSideDefeated(0) || GetIsSideDefeated(1)))
	{
		M_SideDefeatedGameSeconds = GetGameSeconds();
	}
	if (M_SideDefeatedGameSeconds >= 0.f && GetGameSeconds() - M_SideDefeatedGameSeconds >= AfterDefeatGameSeconds)
	{
		const bool bGermanLost = GetIsSideDefeated(0);
		const bool bSovietLost = GetIsSideDefeated(1);
		FinishRun(bGermanLost && bSovietLost ? TEXT("both_destroyed") : bGermanLost ? TEXT("Soviet_win") : TEXT("German_win"));
		return;
	}
	if (GetFightSeconds() >= FightTimeoutGameSeconds)
	{
		FinishRun(TEXT("timeout"));
		return;
	}
	if (GetGameSeconds() - M_LastRetargetGameSeconds >= RetargetIntervalGameSeconds)
	{
		RetargetUnits();
	}
}

void UVehicleDuelBenchSubsystem::RetargetUnits()
{
	M_LastRetargetGameSeconds = GetGameSeconds();
	for (FVehicleDuelUnit& Unit : M_Units)
	{
		ATankMaster* const Tank = Unit.Tank.Get();
		if (not IsValid(Tank) || not Tank->IsUnitAlive())
		{
			continue;
		}
		const bool bTargetAlive = Unit.Target.IsValid() && Unit.Target->IsUnitAlive();
		const bool bNeedsOrder = not bTargetAlive || bM_IsFirstTargetPass;
		if (not bNeedsOrder)
		{
			continue;
		}
		ATankMaster* NearestEnemy = bTargetAlive ? Unit.Target.Get() : nullptr;
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (const FVehicleDuelUnit& Enemy : M_Units)
		{
			ATankMaster* const EnemyTank = Enemy.Tank.Get();
			if (bTargetAlive || Enemy.Side == Unit.Side || not IsValid(EnemyTank) || not EnemyTank->IsUnitAlive())
			{
				continue;
			}
			const float DistanceSquared = FVector::DistSquared(Tank->GetActorLocation(), EnemyTank->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestEnemy = EnemyTank;
			}
		}
		ICommands* const Commands = Cast<ICommands>(Tank);
		if (IsValid(NearestEnemy) && Commands != nullptr)
		{
			Unit.Target = NearestEnemy;
			const ECommandQueueError OrderError = Commands->AttackActor(NearestEnemy, true);
			UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB order %s attacks %s error=%d t=%.1f"), *Unit.Label,
			       *NearestEnemy->GetName(), static_cast<int32>(OrderError), GetFightSeconds());
		}
	}
}

void UVehicleDuelBenchSubsystem::RecordDeaths()
{
	for (FVehicleDuelUnit& Unit : M_Units)
	{
		if (Unit.DeathSeconds >= 0.f)
		{
			continue;
		}
		ATankMaster* const Tank = Unit.Tank.Get();
		if (IsValid(Tank) && Tank->IsUnitAlive())
		{
			continue;
		}
		Unit.DeathSeconds = GetFightSeconds();
		if (Unit.DeathCause.IsEmpty())
		{
			// No shell hit took the last health: a cook-off, engine fire or machine guns finished it.
			Unit.DeathCause = TEXT("Burn or other damage");
		}
	}
}

bool UVehicleDuelBenchSubsystem::GetIsSideDefeated(const int32 Side) const
{
	for (const FVehicleDuelUnit& Unit : M_Units)
	{
		if (Unit.Side == Side && Unit.DeathSeconds < 0.f)
		{
			return false;
		}
	}
	return true;
}

void UVehicleDuelBenchSubsystem::FinishRun(const TCHAR* Outcome)
{
	RecordDeaths();
	const float FightSeconds = GetFightSeconds();
	for (FVehicleDuelUnit& Unit : M_Units)
	{
		Unit.EndHealth = FProjectileMapTestHooks::GetHealthOrZero(Unit.Tank.Get());
		if (Unit.DeathSeconds >= 0.f)
		{
			Unit.EndHealth = 0.f;
		}
	}
	AppendRunToSummary(Outcome, FightSeconds);
	AddRunToTotals(Outcome, FightSeconds);
	WriteSummary();
	UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB run_done scenario=%s repeat=%d outcome=%s fight_s=%.1f"),
	       VehicleDuelBenchPrivate::GetScenarios()[M_RunScenarioIndices[M_RunIndex]].Name,
	       M_RunRepeatNumbers[M_RunIndex], Outcome, FightSeconds);
	DestroyRunActors();
	M_Step = EVehicleDuelBenchStep::Cleanup;
	M_StepStartGameSeconds = GetGameSeconds();
}

void UVehicleDuelBenchSubsystem::TickCleanup()
{
	using namespace VehicleDuelBenchPrivate;
	if (GetGameSeconds() - M_StepStartGameSeconds < CleanupGameSeconds)
	{
		return;
	}
	// Wrecks can appear a moment after the death, so sweep once more before the next run.
	DestroyRunActors();
	M_Units.Reset();
	++M_RunIndex;
	if (M_RunIndex >= M_RunScenarioIndices.Num())
	{
		AppendScenarioAverages();
		WriteSummary();
		Finish(TEXT("done"));
		return;
	}
	M_Step = EVehicleDuelBenchStep::SpawnRun;
}

void UVehicleDuelBenchSubsystem::DestroyRunActors()
{
	using namespace VehicleDuelBenchPrivate;
	for (const FVehicleDuelUnit& Unit : M_Units)
	{
		if (Unit.Tank.IsValid())
		{
			Unit.Tank->Destroy();
		}
	}
	for (const TWeakObjectPtr<AActor>& SpawnedActor : M_RunSpawnedActors)
	{
		AActor* const Actor = SpawnedActor.Get();
		if (not IsValid(Actor))
		{
			continue;
		}
		const bool bIsWreck = Actor->GetClass()->GetName().StartsWith(WreckClassPrefix);
		const bool bInArena = FVector::Dist2D(Actor->GetActorLocation(), M_ArenaCentre) < ArenaCleanupRadiusCm;
		if ((bIsWreck || Actor->IsA<ATankMaster>()) && bInArena)
		{
			Actor->Destroy();
		}
	}
}

void UVehicleDuelBenchSubsystem::OnShellLaunched(const FMapTestShellLaunchReport& Report)
{
	if (M_Step != EVehicleDuelBenchStep::Fight && M_Step != EVehicleDuelBenchStep::Settle)
	{
		return;
	}
	FVehicleDuelUnit* const Shooter = FindUnitByWeaponOwner(Report.WeaponOwner.Get());
	if (Shooter == nullptr)
	{
		return;
	}
	M_ShellShooterIndices.Add(Report.ShellLaunchId, static_cast<int32>(Shooter - M_Units.GetData()));
	++Shooter->ShellsFired;
	if (Shooter->FirstShotSeconds < 0.f)
	{
		Shooter->FirstShotSeconds = GetFightSeconds();
	}
}

void UVehicleDuelBenchSubsystem::OnArmorHit(const FMapTestArmorHitReport& Report)
{
	if (M_Step != EVehicleDuelBenchStep::Fight && M_Step != EVehicleDuelBenchStep::Settle)
	{
		return;
	}
	FVehicleDuelUnit* const Victim = FindUnitByUniqueId(Report.HitActorUniqueId);
	FVehicleDuelUnit* Shooter = FindUnitByWeaponOwner(Report.WeaponOwner.Get());
	const int32* const LaunchShooterIndex = M_ShellShooterIndices.Find(Report.ShellLaunchId);
	if (Shooter == nullptr && LaunchShooterIndex != nullptr && M_Units.IsValidIndex(*LaunchShooterIndex))
	{
		Shooter = &M_Units[*LaunchShooterIndex];
	}
	if (Victim == nullptr && Shooter == nullptr)
	{
		return;
	}
	const float HealthLost = FMath::Max(0.f, Report.HealthBefore - Report.HealthAfter);
	const FString ShooterLabel = Shooter != nullptr ? Shooter->Label : FString(TEXT("unknown"));
	const FString VictimLabel = Victim != nullptr ? Victim->Label : FString(TEXT("other"));
	if (Shooter != nullptr)
	{
		CountHitForShooter(*Shooter, Report, HealthLost);
	}
	if (Victim != nullptr)
	{
		++Victim->HitsTaken;
		Victim->ShellDamageTaken += HealthLost;
		if (Global_GetDamageTypeFromPlate(Report.PlateHit) != EArmorPlateDamageType::DamageFront)
		{
			++Victim->NonFrontHitsTaken;
		}
		const ATankMaster* const VictimTank = Victim->Tank.Get();
		const bool bVictimDied = Report.HealthBefore > 0.f
			&& (Report.HealthAfter <= 0.f || not IsValid(VictimTank) || not const_cast<ATankMaster*>(VictimTank)->IsUnitAlive());
		if (bVictimDied && Victim->DeathCause.IsEmpty())
		{
			Victim->KilledBy = ShooterLabel;
			Victim->DeathCause = VehicleDuelBenchPrivate::GetVerdictName(Report.Verdict);
		}
	}
	AppendHitCsvLine(Report, ShooterLabel, VictimLabel, HealthLost);
}

void UVehicleDuelBenchSubsystem::CountHitForShooter(FVehicleDuelUnit& Shooter, const FMapTestArmorHitReport& Report,
                                                    const float HealthLost)
{
	++Shooter.Hits;
	Shooter.DamageDealt += HealthLost;
	switch (Report.Verdict)
	{
	case EMapTestArmorHitVerdict::Bounce:
		++Shooter.Bounces;
		break;
	case EMapTestArmorHitVerdict::Penetration:
		++Shooter.Penetrations;
		break;
	case EMapTestArmorHitVerdict::OverPenetration:
		++Shooter.OverPenetrations;
		break;
	case EMapTestArmorHitVerdict::ArmorDestroyed:
		++Shooter.ArmorDestroyedHits;
		break;
	case EMapTestArmorHitVerdict::FlankAmmoDetonation:
		++Shooter.FlankDetonations;
		break;
	default:
		break;
	}
}

FVehicleDuelUnit* UVehicleDuelBenchSubsystem::FindUnitByTank(const ATankMaster* Tank)
{
	if (Tank == nullptr)
	{
		return nullptr;
	}
	return M_Units.FindByPredicate([Tank](const FVehicleDuelUnit& Unit) { return Unit.Tank.Get() == Tank; });
}

FVehicleDuelUnit* UVehicleDuelBenchSubsystem::FindUnitByUniqueId(const uint32 UniqueId)
{
	if (UniqueId == 0)
	{
		return nullptr;
	}
	return M_Units.FindByPredicate([UniqueId](const FVehicleDuelUnit& Unit) { return Unit.TankUniqueId == UniqueId; });
}

FVehicleDuelUnit* UVehicleDuelBenchSubsystem::FindUnitByWeaponOwner(const UObject* WeaponOwner)
{
	return FindUnitByTank(VehicleDuelBenchPrivate::GetTankOfWeaponOwner(WeaponOwner));
}

void UVehicleDuelBenchSubsystem::AppendHitCsvLine(const FMapTestArmorHitReport& Report, const FString& ShooterLabel,
                                                  const FString& VictimLabel, const float HealthLost) const
{
	using namespace VehicleDuelBenchPrivate;
	const float PenToArmor = Report.EffectiveArmor > KINDA_SMALL_NUMBER ? Report.PenAtImpact / Report.EffectiveArmor : 0.f;
	const FString Line = FString::Printf(
		TEXT("%s,%d,%.2f,%u,%s,%s,%d,%s,%s,%s,%.0f,%.0f,%.0f,%.2f,%.0f,%.2f,%s,%.0f,%.0f,%.0f"),
		GetScenarios()[M_RunScenarioIndices[M_RunIndex]].Name, M_RunRepeatNumbers[M_RunIndex], GetFightSeconds(),
		Report.ShellLaunchId, *ShooterLabel, *Global_GetWeaponEnumAsString(Report.WeaponName), Report.WeaponCalibre,
		*UEnum::GetDisplayValueAsText(Report.ShellType).ToString(), *VictimLabel,
		*UEnum::GetDisplayValueAsText(Report.PlateHit).ToString(), Report.RawArmor, Report.EffectiveArmor,
		Report.PenAtImpact, PenToArmor, Report.ShellDamage, Report.DamageMlt, *GetVerdictName(Report.Verdict),
		Report.HealthBefore, Report.HealthAfter, HealthLost);
	AppendLine(M_HitsCsvPath, Line);
	UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB hit %s"), *Line);
}

void UVehicleDuelBenchSubsystem::AppendRunToSummary(const TCHAR* Outcome, const float FightSeconds)
{
	using namespace VehicleDuelBenchPrivate;
	const FDuelScenario& Scenario = GetScenarios()[M_RunScenarioIndices[M_RunIndex]];
	M_SummaryLines.Add(FString::Printf(TEXT("\n## %s, repeat %d: %s after %.1f s"), Scenario.Name,
	                                   M_RunRepeatNumbers[M_RunIndex], Outcome, FightSeconds));
	M_SummaryLines.Add(TEXT("| Unit | Side | Max HP | Shells | Hits | Bounce | Pen | Overpen | Armor destroyed | "
		"Flank detonation | Dmg dealt | Hits taken (not front) | Shell dmg taken | Other dmg taken | End HP | "
		"First shot | Died at | Killed by (cause) |"));
	M_SummaryLines.Add(TEXT("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"));
	for (const FVehicleDuelUnit& Unit : M_Units)
	{
		const float OtherDamage = FMath::Max(0.f, Unit.MaxHealth - Unit.EndHealth - Unit.ShellDamageTaken);
		const FString Killer = Unit.DeathSeconds >= 0.f
			                       ? FString::Printf(TEXT("%s (%s)"), *Unit.KilledBy, *Unit.DeathCause)
			                       : FString(TEXT("-"));
		const FString AmbushNote = Unit.bActivatesAmbush
			                           ? (Unit.bAmbushConfirmed ? TEXT(" [Ambush on]") : TEXT(" [Ambush NOT applied]"))
			                           : TEXT("");
		M_SummaryLines.Add(FString::Printf(
			TEXT("| %s%s | %s | %.0f | %d | %d | %d | %d | %d | %d | %d | %.0f | %d (%d) | %.0f | %.0f | %.0f | %s | %s | %s |"),
			*Unit.Label, *AmbushNote, *GetSideName(Unit.Side), Unit.MaxHealth, Unit.ShellsFired, Unit.Hits,
			Unit.Bounces, Unit.Penetrations, Unit.OverPenetrations, Unit.ArmorDestroyedHits, Unit.FlankDetonations,
			Unit.DamageDealt, Unit.HitsTaken, Unit.NonFrontHitsTaken, Unit.ShellDamageTaken, OtherDamage,
			Unit.EndHealth, *FormatSeconds(Unit.FirstShotSeconds), *FormatSeconds(Unit.DeathSeconds), *Killer));
	}
	for (const FVehicleDuelUnit& Unit : M_Units)
	{
		if (not Unit.HealthNote.IsEmpty())
		{
			M_SummaryLines.Add(FString::Printf(TEXT("\n> Health note, %s: %s"), *Unit.Label, *Unit.HealthNote));
		}
	}
}

void UVehicleDuelBenchSubsystem::AddRunToTotals(const TCHAR* Outcome, const float FightSeconds)
{
	FVehicleDuelScenarioTotals& Totals = M_ScenarioTotals.FindOrAdd(M_RunScenarioIndices[M_RunIndex]);
	++Totals.Runs;
	Totals.FightSecondsSum += FightSeconds;
	const FString OutcomeText(Outcome);
	Totals.Timeouts += OutcomeText == TEXT("timeout") ? 1 : 0;
	Totals.Wins[0] += OutcomeText == TEXT("German_win") ? 1 : 0;
	Totals.Wins[1] += OutcomeText == TEXT("Soviet_win") ? 1 : 0;
	for (const FVehicleDuelUnit& Unit : M_Units)
	{
		Totals.ArmorDestroyedHits[Unit.Side] += Unit.ArmorDestroyedHits;
		Totals.FlankDetonations[Unit.Side] += Unit.FlankDetonations;
		Totals.Bounces[Unit.Side] += Unit.Bounces;
		Totals.Hits[Unit.Side] += Unit.Hits;
		if (Unit.DeathSeconds < 0.f)
		{
			continue;
		}
		// The kill counts for the side that did not lose this unit.
		const int32 KillerSide = 1 - Unit.Side;
		++Totals.Kills[KillerSide];
		Totals.KillSecondsSum[KillerSide] += Unit.DeathSeconds;
		Totals.HitsToKillSum[KillerSide] += Unit.HitsTaken;
	}
}

void UVehicleDuelBenchSubsystem::AppendScenarioAverages()
{
	using namespace VehicleDuelBenchPrivate;
	M_SummaryLines.Add(TEXT("\n# Averages per scenario"));
	M_SummaryLines.Add(TEXT("| Scenario | Runs | German wins | Soviet wins | Timeouts | Avg fight s | "
		"German kills: avg time / avg hits taken | Soviet kills: avg time / avg hits taken | "
		"Armor destroyed (Ger/Sov) | Flank detonations (Ger/Sov) | Bounce share (Ger/Sov) |"));
	M_SummaryLines.Add(TEXT("|---|---|---|---|---|---|---|---|---|---|---|"));
	for (const TPair<int32, FVehicleDuelScenarioTotals>& Pair : M_ScenarioTotals)
	{
		const FVehicleDuelScenarioTotals& Totals = Pair.Value;
		auto KillText = [&Totals](const int32 Side)
		{
			return Totals.Kills[Side] == 0
				       ? FString(TEXT("no kills"))
				       : FString::Printf(TEXT("%d kills: %.1f s / %.1f hits"), Totals.Kills[Side],
				                         Totals.KillSecondsSum[Side] / Totals.Kills[Side],
				                         static_cast<float>(Totals.HitsToKillSum[Side]) / Totals.Kills[Side]);
		};
		auto BounceShare = [&Totals](const int32 Side)
		{
			return Totals.Hits[Side] == 0 ? 0.f : static_cast<float>(Totals.Bounces[Side]) / Totals.Hits[Side];
		};
		M_SummaryLines.Add(FString::Printf(
			TEXT("| %s | %d | %d | %d | %d | %.1f | %s | %s | %d / %d | %d / %d | %.0f%% / %.0f%% |"),
			GetScenarios()[Pair.Key].Name, Totals.Runs, Totals.Wins[0], Totals.Wins[1], Totals.Timeouts,
			Totals.Runs > 0 ? Totals.FightSecondsSum / Totals.Runs : 0.f, *KillText(0), *KillText(1),
			Totals.ArmorDestroyedHits[0], Totals.ArmorDestroyedHits[1], Totals.FlankDetonations[0],
			Totals.FlankDetonations[1], BounceShare(0) * 100.f, BounceShare(1) * 100.f));
	}
}

void UVehicleDuelBenchSubsystem::WriteSummary() const
{
	FFileHelper::SaveStringToFile(FString::Join(M_SummaryLines, TEXT("\n")) + TEXT("\n"), *M_SummaryPath);
}

void UVehicleDuelBenchSubsystem::AppendLine(const FString& FilePath, const FString& Line) const
{
	// Appended and flushed per line so a crash mid-campaign still leaves the hits so far.
	FFileHelper::SaveStringToFile(Line + TEXT("\n"), *FilePath, FFileHelper::EEncodingOptions::AutoDetect,
	                              &IFileManager::Get(), FILEWRITE_Append);
}

bool UVehicleDuelBenchSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace VehicleDuelBenchPrivate;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleDuelBenchGround), false);
	FHitResult GroundHit;
	const FVector TraceStart(Location.X, Location.Y, Location.Z + GroundTraceUpCm);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - GroundTraceDownCm);
	if (not GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE,
	                                             QueryParams))
	{
		return false;
	}
	OutGroundZ = GroundHit.ImpactPoint.Z;
	return true;
}

float UVehicleDuelBenchSubsystem::GetFightSeconds() const
{
	return GetGameSeconds() - M_FightStartGameSeconds;
}

float UVehicleDuelBenchSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UVehicleDuelBenchSubsystem::Finish(const TCHAR* Reason)
{
	M_Step = EVehicleDuelBenchStep::Done;
	if (not M_SummaryPath.IsEmpty())
	{
		WriteSummary();
	}
	UE_LOG(LogRTSVehicleDuelBench, Display, TEXT("RTS_VDB RESULT reason=%s summary=%s real_s=%.0f"), Reason,
	       *M_SummaryPath, FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("VehicleDuelBench"));
}

#else

bool UVehicleDuelBenchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UVehicleDuelBenchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UVehicleDuelBenchSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVehicleDuelBenchSubsystem::Tick(float DeltaTime)
{
}

TStatId UVehicleDuelBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVehicleDuelBenchSubsystem, STATGROUP_Tickables);
}

#endif
