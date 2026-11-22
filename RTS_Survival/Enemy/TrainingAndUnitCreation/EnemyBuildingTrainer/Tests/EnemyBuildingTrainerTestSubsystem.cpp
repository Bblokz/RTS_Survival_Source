// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyBuildingTrainerTestSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyBuildingTrainer/SquadDisplay/EnemyTrainingSquadDisplayComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyAITechLevel/EnemyAITechLevel.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadControllerHpComp/SquadWeaponIcons/SquadWeaponIconSettings/SquadWeaponIconSettings.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyBuildingTrainerTest, Log, All);

#if RTS_WITH_MAP_TESTS
namespace EnemyBuildingTrainerTestPrivate
{
	const TCHAR* const TestSwitch = TEXT("EnemyBuildingTrainerTest");
	const TCHAR* const BuildingsSwitch = TEXT("EnemyBuildingTrainerTestBuildings=");
	const TCHAR* const AddMissingSwitch = TEXT("EnemyBuildingTrainerTestAddMissing");
	const TCHAR* const CaptureSwitch = TEXT("EnemyBuildingTrainerTestCapture");
	const TCHAR* const ExposureSwitch = TEXT("EnemyBuildingTrainerTestExposure=");
	const TCHAR* const AtSwitch = TEXT("EnemyBuildingTrainerTestAt=");
	const TCHAR* const DirectorySwitch = TEXT("EnemyBuildingTrainerTestDir=");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 25.0 * 60.0;
	// The vehicle duel bench's flat field on RTS_ToCopySmall (also the four-player skirmish arena).
	const FVector DefaultGridOrigin(16796.f, -15808.f, 0.f);
	// The assembly yard is 44 x 32 m; 60 m between building centres keeps every exit clear of its neighbours.
	constexpr float GridSpacingCm = 6000.f;
	// Slots are tried in rings around the origin; a slot needs navmesh at its centre and on all four sides, since
	// part of RTS_ToCopySmall's flat ground has no navmesh and a unit there has nowhere to appear.
	constexpr int32 MaxGridRing = 4;
	constexpr float SlotNavProbeOffsetCm = 3000.f;
	const FVector SlotNavProbeExtent(300.f, 300.f, 1000.f);
	constexpr float DefaultCaptureExposureBias = 2.f;
	constexpr float CaptureDistanceRadiusMlt = 2.6f;
	// Mesh components larger than this (sky domes, nav or selection helpers) are left out of the capture framing.
	constexpr float MaxFramedComponentExtentCm = 6000.f;
	constexpr float BuildingSpawnLiftCm = 5.f;
	constexpr float GroundTraceUpCm = 5000.f;
	constexpr float GroundTraceDownCm = 20000.f;
	constexpr float WaitForBuildingsSeconds = 4.f;
	// A unit may come at most this much before its training time (timer granularity) ...
	constexpr float EarlySpawnToleranceSeconds = 0.25f;
	// ... and this much after (async load and spawn of the unit's assets).
	constexpr float LateSpawnToleranceSeconds = 15.f;
	constexpr float MaxSpawnDistanceFromExitCm = 1500.f;
	constexpr float RegistrationTimeoutSeconds = 20.f;
	// Tanks are ordered this far out of the exit; they must get at least ExitClearMinMovedCm away within the window.
	constexpr float ExitClearMoveDistanceCm = 2500.f;
	constexpr float ExitClearMinMovedCm = 800.f;
	constexpr float ExitClearWindowSeconds = 25.f;
	constexpr float ExitClearLateralSpacingCm = 900.f;
	constexpr float SettleSeconds = 3.f;
	// Long enough for a 30 s crane loop to finish its cycle (FinishCycle) before the "rest" capture.
	constexpr float RestCaptureSeconds = 33.f;
	constexpr float RealThinkWindowSeconds = 12.f;
	constexpr float CancelProgressToDestroy = 0.3f;
	constexpr float CancelWatchSeconds = 12.f;
	constexpr float TrainTimeoutSlackSeconds = 60.f;
	// -EnemyBuildingTrainerTestSquad=<ESquadSubtype name> / -EnemyBuildingTrainerTestTank=<ETankSubtype name>.
	// Spawns every squad and tank the AI can train (all tech levels) and checks each comes out as itself.
	const TCHAR* const SpawnerAuditSwitch = TEXT("EnemyBuildingTrainerTestSpawnerAudit");
	constexpr float AuditSpawnTimeoutSeconds = 90.f;
	constexpr float AuditSettleSeconds = 3.f;
	constexpr float AuditUnitSpacingCm = 1500.f;
	const TCHAR* const SquadSwitch = TEXT("EnemyBuildingTrainerTestSquad=");
	const TCHAR* const TankSwitch = TEXT("EnemyBuildingTrainerTestTank=");
	ESquadSubtype TestSquad = ESquadSubtype::Squad_Rus_Mosin;
	// -EnemyBuildingTrainerTestDisplaySquads=A,B,C: every infantry building trains these squads in order (the first
	// through the AI routing, the rest queued straight on the trainer) and a frame + squad-display line is taken at
	// half of each squad's training.
	const TCHAR* const DisplaySquadsSwitch = TEXT("EnemyBuildingTrainerTestDisplaySquads=");
	TArray<ESquadSubtype> DisplaySquads;
	constexpr float DisplayCaptureProgress = 0.5f;
	ETankSubtype TestTank = ETankSubtype::Tank_BT7;
	// -EnemyBuildingTrainerTestSpawnQA: frames and measurements of how a trained unit appears at its building.
	const TCHAR* const SpawnQASwitch = TEXT("EnemyBuildingTrainerTestSpawnQA");
	// The "pre" frame is taken once the active job is this far along.
	constexpr float SpawnQAPreProgress = 0.97f;
	struct FSpawnQAStage
	{
		const TCHAR* Tag;
		float SecondsAfterSpawn;
	};
	constexpr FSpawnQAStage SpawnQAStages[] = {
		{TEXT("b_spawn"), 0.f}, {TEXT("c_1s"), 1.f}, {TEXT("d_3s"), 3.f}, {TEXT("e_8s"), 8.f}, {TEXT("f_20s"), 20.f}
	};
	// Bodies are sampled every tick this long after the spawn for teleports and drops.
	constexpr float SpawnQASampleSeconds = 2.5f;
	// A body that moved this much further in one tick than its speed explains was teleported.
	constexpr float SpawnQAMaxBodyJumpCm = 80.f;
	// The fog control: a human vision bubble this wide over the first building's exit, for the whole run.
	constexpr float SpawnQAFogControlRadiusCm = 2500.f;
	constexpr float SpawnQAFogControlSeconds = 900.f;
	// From this stage on the fog has had its ticks to show the control building's units.
	constexpr int32 SpawnQAFogControlFirstCheckedStage = 1;
	// Ignoring the first moments, while a unit may settle onto the ground.
	constexpr float SpawnQAAirborneIgnoreSeconds = 0.25f;
	constexpr float SpawnQAMaxAirborneCm = 35.f;
	// At the last stage no body of the unit may stand this close to its exit (where the next unit appears).
	constexpr float SpawnQAMinBodyExitDistanceCm = 250.f;
	constexpr int32 SpawnQAStageCount = UE_ARRAY_COUNT(SpawnQAStages);
	// Exit view: looks back at the exit and the building's front from outside, 30 degrees off the exit axis.
	constexpr float SpawnQAViewYawOffset = 30.f;
	constexpr float SpawnQAViewPitch = -38.f;
	constexpr float SpawnQAViewTargetTowardBuildingMlt = 0.3f;
	constexpr float SpawnQAViewDistanceRadiusMlt = 1.5f;
	constexpr float SpawnQAViewMinDistanceCm = 2200.f;
	constexpr float SpawnQAViewMaxDistanceCm = 6500.f;
	constexpr int32 CaptureWidth = 1280;
	constexpr int32 CaptureHeight = 720;

	struct FBuildingSpec
	{
		const TCHAR* Label;
		const TCHAR* ClassPath;
		EEnemyBuildingTrainerKind Kind;
	};

	const TArray<FBuildingSpec>& GetBuildingSpecs()
	{
		static const TArray<FBuildingSpec> Specs = {
			{TEXT("Barracks"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Barracks.Rus_Building_Barracks_C"), EEnemyBuildingTrainerKind::Infantry},
			{TEXT("BarracksConcrete"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Barracks_Concrete.Rus_Building_Barracks_Concrete_C"), EEnemyBuildingTrainerKind::Infantry},
			{TEXT("LargeTent"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_LargeTent.Rus_Building_LargeTent_C"), EEnemyBuildingTrainerKind::Infantry},
			{TEXT("LargeTentSoviet"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_LargeTent_Soviet.Rus_Building_LargeTent_Soviet_C"), EEnemyBuildingTrainerKind::Infantry},
			{TEXT("Factory"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Factory.Rus_Building_Factory_C"), EEnemyBuildingTrainerKind::Vehicles},
			{TEXT("AssemblyYard"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/AssemblyYard/Rus_Building_AssemblyYard.Rus_Building_AssemblyYard_C"), EEnemyBuildingTrainerKind::Vehicles},
			{TEXT("PlatformFactory"), TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_PlatformFactory.Rus_Building_PlatformFactory_C"), EEnemyBuildingTrainerKind::Vehicles},
		};
		return Specs;
	}

	const TCHAR* GetEventName(const EEnemyBuildingTrainingEvent Event)
	{
		switch (Event)
		{
		case EEnemyBuildingTrainingEvent::Queued: return TEXT("queued");
		case EEnemyBuildingTrainingEvent::Started: return TEXT("started");
		case EEnemyBuildingTrainingEvent::Finished: return TEXT("finished");
		case EEnemyBuildingTrainingEvent::Cancelled: return TEXT("cancelled");
		case EEnemyBuildingTrainingEvent::ActivityOn: return TEXT("activity_on");
		case EEnemyBuildingTrainingEvent::ActivityOff: return TEXT("activity_off");
		default: return TEXT("unknown");
		}
	}

	int32 GetOptionCost(const FTrainingOption& Option)
	{
		if (Option.UnitType == EAllUnitType::UNType_Squad)
		{
			return EnemyTrainingHelpers::GetSquadTrainingPointCost(static_cast<ESquadSubtype>(Option.SubtypeValue));
		}
		return EnemyTrainingHelpers::GetTankTrainingPointsCost(static_cast<ETankSubtype>(Option.SubtypeValue));
	}

	FTrainingOption GetTestOption(const EEnemyBuildingTrainerKind Kind)
	{
		return Kind == EEnemyBuildingTrainerKind::Infantry
			       ? FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(TestSquad))
			       : FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TestTank));
	}
}
#endif

bool UEnemyBuildingTrainerTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), EnemyBuildingTrainerTestPrivate::TestSwitch);
#else
	return false;
#endif
}

TStatId UEnemyBuildingTrainerTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyBuildingTrainerTestSubsystem, STATGROUP_Tickables);
}

void UEnemyBuildingTrainerTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
#if RTS_WITH_MAP_TESTS
	ReadCommandLine();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*M_OutputDirectory);
	if (bM_Capture || bM_SpawnQA)
	{
		// One-shot captures of freshly spawned buildings otherwise render before their textures stream in.
		if (IConsoleVariable* TextureStreaming = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming")))
		{
			TextureStreaming->Set(0, ECVF_SetByCode);
		}
		// The capture camera jumps 60 m between buildings: stale occlusion results dropped whole meshes from frames.
		if (IConsoleVariable* OcclusionQueries = IConsoleManager::Get().FindConsoleVariable(
			TEXT("r.AllowOcclusionQueries")))
		{
			OcclusionQueries->Set(0, ECVF_SetByCode);
		}
	}
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	Log(FString::Printf(TEXT("RTS_EBT started dir=%s grid=%s add_missing=%d capture=%d"), *M_OutputDirectory,
	                    *M_GridOrigin.ToCompactString(), bM_AddMissingTrainers ? 1 : 0, bM_Capture ? 1 : 0));
#endif
}

void UEnemyBuildingTrainerTestSubsystem::Tick(float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	using namespace EnemyBuildingTrainerTestPrivate;
	if (not bM_IsActive || M_Step == EEnemyBuildingTrainerTestStep::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxRunRealSeconds)
	{
		M_GlobalFailures.Add(TEXT("real_time_watchdog"));
		Finish();
		return;
	}
	KeepGameRunning();
	switch (M_Step)
	{
	case EEnemyBuildingTrainerTestStep::StartGame:
		TickStartGame(RealSeconds);
		break;
	case EEnemyBuildingTrainerTestStep::Spawn:
		SpawnPhase();
		break;
	case EEnemyBuildingTrainerTestStep::WaitForBuildings:
		TickWaitForBuildings();
		break;
	case EEnemyBuildingTrainerTestStep::Train:
		TickTrain();
		break;
	case EEnemyBuildingTrainerTestStep::Settle:
		TickSettle();
		break;
	case EEnemyBuildingTrainerTestStep::RealThink:
		TickRealThink();
		break;
	case EEnemyBuildingTrainerTestStep::Cancel:
		TickCancel();
		break;
	case EEnemyBuildingTrainerTestStep::SpawnerAudit:
		TickSpawnerAudit();
		break;
	default:
		break;
	}
#endif
}

#if RTS_WITH_MAP_TESTS

void UEnemyBuildingTrainerTestSubsystem::ReadCommandLine()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	const TCHAR* const CommandLine = FCommandLine::Get();
	M_GridOrigin = DefaultGridOrigin;
	FString LocationText;
	if (FParse::Value(CommandLine, AtSwitch, LocationText, false))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		M_GridOrigin = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	FString BuildingsText;
	if (FParse::Value(CommandLine, BuildingsSwitch, BuildingsText, false))
	{
		BuildingsText.ParseIntoArray(M_RequestedLabels, TEXT(","), true);
	}
	bM_AddMissingTrainers = FParse::Param(CommandLine, AddMissingSwitch);
	bM_SpawnerAudit = FParse::Param(CommandLine, SpawnerAuditSwitch);
	bM_SpawnQA = FParse::Param(CommandLine, SpawnQASwitch);
	FString SubtypeName;
	if (FParse::Value(CommandLine, SquadSwitch, SubtypeName))
	{
		const int64 Value = StaticEnum<ESquadSubtype>()->GetValueByNameString(SubtypeName);
		if (Value == INDEX_NONE)
		{
			M_GlobalFailures.Add(TEXT("unknown_test_squad_") + SubtypeName);
		}
		else
		{
			TestSquad = static_cast<ESquadSubtype>(Value);
		}
	}
	if (FParse::Value(CommandLine, TankSwitch, SubtypeName))
	{
		const int64 Value = StaticEnum<ETankSubtype>()->GetValueByNameString(SubtypeName);
		if (Value == INDEX_NONE)
		{
			M_GlobalFailures.Add(TEXT("unknown_test_tank_") + SubtypeName);
		}
		else
		{
			TestTank = static_cast<ETankSubtype>(Value);
		}
	}
	FString DisplaySquadsText;
	if (FParse::Value(CommandLine, DisplaySquadsSwitch, DisplaySquadsText, false))
	{
		TArray<FString> Names;
		DisplaySquadsText.ParseIntoArray(Names, TEXT(","), true);
		for (const FString& Name : Names)
		{
			const int64 Value = StaticEnum<ESquadSubtype>()->GetValueByNameString(Name);
			if (Value == INDEX_NONE)
			{
				M_GlobalFailures.Add(TEXT("unknown_display_squad_") + Name);
				continue;
			}
			DisplaySquads.Add(static_cast<ESquadSubtype>(Value));
		}
		if (not DisplaySquads.IsEmpty())
		{
			TestSquad = DisplaySquads[0];
		}
	}
	bM_Capture = FParse::Param(CommandLine, CaptureSwitch);
	bM_HasExposureBias = FParse::Value(CommandLine, ExposureSwitch, M_ExposureBias);
	M_OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EnemyBuildingTrainer"),
	                                    FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	FParse::Value(CommandLine, DirectorySwitch, M_OutputDirectory, false);
}

void UEnemyBuildingTrainerTestSubsystem::TickStartGame(const double RealSeconds)
{
	if (RealSeconds < EnemyBuildingTrainerTestPrivate::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	SetStep(EEnemyBuildingTrainerTestStep::Spawn);
}

bool UEnemyBuildingTrainerTestSubsystem::TryStartGame()
{
	ACPPController* const PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	M_PlayerController = PlayerController;
	if (not IsValid(PlayerController))
	{
		return false;
	}
	UPlayerStartGameControl* const StartGameControl = PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	return true;
}

void UEnemyBuildingTrainerTestSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(EnemyBuildingTrainerTestPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* const PlayerController = M_PlayerController.Get();
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UEnemyBuildingTrainerTestSubsystem::SpawnPhase()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	UEnemyControllerSubsystem* const ControllerSubsystem = GetWorld()->GetSubsystem<UEnemyControllerSubsystem>();
	AEnemyController* const EnemyController = IsValid(ControllerSubsystem)
		                                          ? ControllerSubsystem->GetEnemyControllerForPlayer(
			                                          RTSPlayers::FirstAIPlayer)
		                                          : nullptr;
	UEnemyStrategicAIComponent* const StrategicAI = IsValid(EnemyController)
		                                                ? EnemyController->GetEnemyStrategicAIComponent()
		                                                : nullptr;
	if (not IsValid(StrategicAI))
	{
		M_GlobalFailures.Add(TEXT("no_player2_enemy_controller_or_strategic_ai"));
		Finish();
		return;
	}
	M_EnemyController = EnemyController;
	M_StrategicAI = StrategicAI;
	if (bM_SpawnerAudit)
	{
		StartSpawnerAudit();
		return;
	}
	// The test owns the buildings' queues: the live AI must not queue its own picks in between.
	StrategicAI->MapTest_SetTrainingThinkSuspended(true);

	TArray<FVector> Slots;
	CollectNavigableGridSlots(Slots);
	int32 Slot = 0;
	for (const FBuildingSpec& Spec : GetBuildingSpecs())
	{
		const FString Label(Spec.Label);
		if (not M_RequestedLabels.IsEmpty() && not M_RequestedLabels.Contains(Label)
			&& not M_RequestedLabels.Contains(FString(Spec.ClassPath)))
		{
			continue;
		}
		FEnemyBuildingTrainerTestCase& Case = M_Cases.AddDefaulted_GetRef();
		Case.Label = Label;
		Case.ClassPath = Spec.ClassPath;
		Case.ExpectedKind = Spec.Kind;
		if (not Slots.IsValidIndex(Slot))
		{
			Case.Failures.Add(TEXT("no_navigable_grid_slot"));
			continue;
		}
		const FVector Location = Slots[Slot];
		++Slot;
		if (not SpawnCase(Case, Location))
		{
			Case.Failures.Add(TEXT("spawn_failed"));
		}
	}
	if (M_Cases.IsEmpty())
	{
		M_GlobalFailures.Add(TEXT("no_buildings_selected"));
		Finish();
		return;
	}
	SetStep(EEnemyBuildingTrainerTestStep::WaitForBuildings);
}

void UEnemyBuildingTrainerTestSubsystem::CollectNavigableGridSlots(TArray<FVector>& OutSlots)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	UNavigationSystemV1* const NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (not IsValid(NavSystem))
	{
		return;
	}
	auto IsNavigable = [NavSystem](const FVector& Location)
	{
		FNavLocation Projected;
		return NavSystem->ProjectPointToNavigation(Location, Projected, SlotNavProbeExtent);
	};
	for (int32 Ring = 0; Ring <= MaxGridRing; ++Ring)
	{
		for (int32 X = -Ring; X <= Ring; ++X)
		{
			for (int32 Y = -Ring; Y <= Ring; ++Y)
			{
				if (FMath::Max(FMath::Abs(X), FMath::Abs(Y)) != Ring)
				{
					continue;
				}
				FVector Location = M_GridOrigin + FVector(X * GridSpacingCm, Y * GridSpacingCm, 0.f);
				float GroundZ = Location.Z;
				if (not TryGetGroundZ(Location, GroundZ))
				{
					continue;
				}
				Location.Z = GroundZ;
				const bool bIsOpenGround = IsNavigable(Location)
					&& IsNavigable(Location + FVector(SlotNavProbeOffsetCm, 0.f, 0.f))
					&& IsNavigable(Location - FVector(SlotNavProbeOffsetCm, 0.f, 0.f))
					&& IsNavigable(Location + FVector(0.f, SlotNavProbeOffsetCm, 0.f))
					&& IsNavigable(Location - FVector(0.f, SlotNavProbeOffsetCm, 0.f));
				if (bIsOpenGround)
				{
					OutSlots.Add(Location);
				}
			}
		}
	}
	Log(FString::Printf(TEXT("RTS_EBT navigable grid slots=%d"), OutSlots.Num()));
}

bool UEnemyBuildingTrainerTestSubsystem::SpawnCase(FEnemyBuildingTrainerTestCase& Case, const FVector& Location)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	UClass* const BuildingClass = LoadClass<AActor>(nullptr, *Case.ClassPath);
	if (not IsValid(BuildingClass))
	{
		Log(FString::Printf(TEXT("RTS_EBT %s no class %s"), *Case.Label, *Case.ClassPath));
		return false;
	}
	float GroundZ = Location.Z;
	if (not TryGetGroundZ(Location, GroundZ))
	{
		Log(FString::Printf(TEXT("RTS_EBT %s no ground at %s"), *Case.Label, *Location.ToCompactString()));
	}
	const FVector SpawnLocation(Location.X, Location.Y, GroundZ + BuildingSpawnLiftCm);
	AActor* const Building = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*GetWorld(), BuildingClass, FTransform(FRotator::ZeroRotator, SpawnLocation),
		static_cast<uint8>(RTSPlayers::FirstAIPlayer));
	if (not IsValid(Building))
	{
		Log(FString::Printf(TEXT("RTS_EBT %s spawn failed"), *Case.Label));
		return false;
	}
	Case.Building = Building;
	Log(FString::Printf(TEXT("RTS_EBT %s spawned %s at %s owner=%d"), *Case.Label, *Building->GetName(),
	                    *SpawnLocation.ToCompactString(), FRTSPlayers::GetOwningPlayerOfActor(Building)));
	return true;
}

void UEnemyBuildingTrainerTestSubsystem::TickWaitForBuildings()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	if (GetGameSeconds() - M_StepStartGameSeconds < WaitForBuildingsSeconds)
	{
		return;
	}
	for (int32 CaseIndex = 0; CaseIndex < M_Cases.Num(); ++CaseIndex)
	{
		FEnemyBuildingTrainerTestCase& Case = M_Cases[CaseIndex];
		AActor* const Building = Case.Building.Get();
		if (not IsValid(Building))
		{
			continue;
		}
		UEnemyBuildingTrainerComponent* Trainer = Building->FindComponentByClass<UEnemyBuildingTrainerComponent>();
		if (not IsValid(Trainer) && bM_AddMissingTrainers)
		{
			Trainer = NewObject<UEnemyBuildingTrainerComponent>(Building, TEXT("EnemyBuildingTrainer_Test"));
			Trainer->TrainerKind = Case.ExpectedKind;
			Trainer->RegisterComponent();
			Case.bComponentAddedByTest = true;
		}
		if (not IsValid(Trainer))
		{
			Case.Failures.Add(TEXT("no_trainer_component_in_blueprint"));
			continue;
		}
		Case.Trainer = Trainer;
		Case.TaggedActivityComponents = Trainer->MapTest_GetTaggedActivityComponentCount();
		if (Trainer->TrainerKind != Case.ExpectedKind)
		{
			Case.Failures.Add(TEXT("wrong_trainer_kind"));
		}
		if (not Trainer->GetCanTrainNow())
		{
			Case.Failures.Add(TEXT("trainer_cannot_train_after_spawn"));
		}
		if (Case.TaggedActivityComponents == 0)
		{
			// Not a failure of the training itself; the building shows nothing while it trains.
			Log(FString::Printf(TEXT("RTS_EBT %s WARNING no tagged activity components"), *Case.Label));
		}
		BindTrainer(CaseIndex);
	}

	// Registration: the strategic AI must find every trainer through the unit manager's buildings of player 2.
	const TArray<UEnemyBuildingTrainerComponent*> KnownTrainers = M_StrategicAI->MapTest_GetBuildingTrainers();
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (Case.Trainer.IsValid() && not KnownTrainers.Contains(Case.Trainer.Get()))
		{
			Case.Failures.Add(TEXT("trainer_not_found_by_strategic_ai"));
		}
		if (bM_Capture && Case.Trainer.IsValid())
		{
			CaptureCase(Case, TEXT("0_idle"));
		}
	}
	QueueFirstJobs();
	SetStep(EEnemyBuildingTrainerTestStep::Train);
}

void UEnemyBuildingTrainerTestSubsystem::BindTrainer(const int32 CaseIndex)
{
	UEnemyBuildingTrainerComponent* const Trainer = M_Cases[CaseIndex].Trainer.Get();
	if (not IsValid(Trainer))
	{
		return;
	}
	Trainer->OnTrainingEventNative.AddWeakLambda(
		this, [this, CaseIndex](const EEnemyBuildingTrainingEvent Event, const FTrainingOption& Option, AActor* Unit,
		                        const float Duration)
		{
			OnTrainerEvent(CaseIndex, Event, Option, Unit, Duration);
		});
}

void UEnemyBuildingTrainerTestSubsystem::QueueFirstJobs()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	TArray<ETankSubtype> Tanks;
	TArray<ESquadSubtype> Squads;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (not Case.Trainer.IsValid())
		{
			continue;
		}
		if (Case.ExpectedKind == EEnemyBuildingTrainerKind::Infantry)
		{
			Squads.Add(TestSquad);
		}
		else
		{
			Tanks.Add(TestTank);
		}
	}
	// Through the strategic AI's routing: one unit per building means every building gets exactly one.
	const int32 Queued = M_StrategicAI->MapTest_QueueTrainingAtBuildings(Tanks, Squads);
	Log(FString::Printf(TEXT("RTS_EBT routed tanks=%d squads=%d queued=%d"), Tanks.Num(), Squads.Num(), Queued));
	if (Queued != Tanks.Num() + Squads.Num())
	{
		M_GlobalFailures.Add(FString::Printf(TEXT("routing_queued_%d_of_%d"), Queued, Tanks.Num() + Squads.Num()));
	}
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
		if (not IsValid(Trainer))
		{
			continue;
		}
		if (Trainer->GetQueueLength() != 1)
		{
			Case.Failures.Add(FString::Printf(TEXT("routing_gave_%d_jobs"), Trainer->GetQueueLength()));
		}
		if (not Trainer->GetIsTraining() || not Trainer->MapTest_GetIsActivityOn())
		{
			Case.Failures.Add(TEXT("not_active_right_after_queue"));
		}
		if (Case.ExpectedKind != EEnemyBuildingTrainerKind::Infantry)
		{
			continue;
		}
		for (int32 Index = 1; Index < DisplaySquads.Num(); ++Index)
		{
			const FTrainingOption Option(EAllUnitType::UNType_Squad, static_cast<uint8>(DisplaySquads[Index]));
			if (not Trainer->EnqueueTraining(Option, GetOptionCost(Option), M_StrategicAI.Get()))
			{
				Case.Failures.Add(TEXT("display_enqueue_failed_") + Option.GetTrainingName());
			}
		}
	}
	if (bM_SpawnQA)
	{
		for (int32 CaseIndex = 0; CaseIndex < M_Cases.Num(); ++CaseIndex)
		{
			const UEnemyBuildingTrainerComponent* const Trainer = M_Cases[CaseIndex].Trainer.Get();
			if (not IsValid(Trainer))
			{
				continue;
			}
			FActorSpawnParameters BubbleParameters;
			BubbleParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AGlobalAbilityVisionBubble* const Bubble = GetWorld()->SpawnActor<AGlobalAbilityVisionBubble>(
				Trainer->GetExitTransform().GetLocation(), FRotator::ZeroRotator, BubbleParameters);
			if (IsValid(Bubble))
			{
				Bubble->StartVision(SpawnQAFogControlRadiusCm, SpawnQAFogControlSeconds,
				                    static_cast<uint8>(RTSPlayers::HumanPlayer));
				M_FogControlCaseIndex = CaseIndex;
				Log(FString::Printf(TEXT("RTS_EBT spawnqa fog control: human vision %.0f cm over %s's exit"),
				                    SpawnQAFogControlRadiusCm, *M_Cases[CaseIndex].Label));
			}
			break;
		}
	}
	// A second unit at the first building: it must start right after the first, with the activity left on.
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
		if (not IsValid(Trainer))
		{
			continue;
		}
		const FTrainingOption Option = GetTestOption(Case.ExpectedKind);
		bM_SecondJobQueued = Trainer->EnqueueTraining(Option, GetOptionCost(Option), M_StrategicAI.Get());
		if (not bM_SecondJobQueued)
		{
			Case.Failures.Add(TEXT("second_job_not_queued"));
		}
		if (not bM_SpawnQA)
		{
			break;
		}
	}
}

void UEnemyBuildingTrainerTestSubsystem::OnTrainerEvent(const int32 CaseIndex, const EEnemyBuildingTrainingEvent Event,
                                                        const FTrainingOption& Option, AActor* Unit,
                                                        const float Duration)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	if (not M_Cases.IsValidIndex(CaseIndex))
	{
		return;
	}
	FEnemyBuildingTrainerTestCase& Case = M_Cases[CaseIndex];
	const float Now = GetGameSeconds();
	switch (Event)
	{
	case EEnemyBuildingTrainingEvent::Started:
		{
			FEnemyBuildingTrainerTestJob& Job = Case.Jobs.AddDefaulted_GetRef();
			Job.Option = Option;
			Job.StartedAt = Now;
			Job.DurationSeconds = Duration;
			break;
		}
	case EEnemyBuildingTrainingEvent::Finished:
		{
			FEnemyBuildingTrainerTestJob* Job = Case.Jobs.FindByPredicate(
				[](const FEnemyBuildingTrainerTestJob& Candidate)
				{
					return Candidate.FinishedAt < 0.f && not Candidate.bCancelled && Candidate.StartedAt >= 0.f;
				});
			if (Job == nullptr)
			{
				Case.Failures.Add(TEXT("finished_without_started_job"));
				break;
			}
			Job->FinishedAt = Now;
			Job->Unit = Unit;
			Job->SpawnLocation = IsValid(Unit) ? Unit->GetActorLocation() : FVector::ZeroVector;
			if (Case.Trainer.IsValid())
			{
				const FTransform Exit = Case.Trainer->GetExitTransform();
				Job->ExitLocation = Exit.GetLocation();
				Job->ExitForward = Exit.GetRotation().GetForwardVector().GetSafeNormal2D();
			}
			break;
		}
	case EEnemyBuildingTrainingEvent::Cancelled:
		{
			FEnemyBuildingTrainerTestJob* Job = Case.Jobs.FindByPredicate(
				[](const FEnemyBuildingTrainerTestJob& Candidate)
				{
					return Candidate.FinishedAt < 0.f && not Candidate.bCancelled;
				});
			if (Job == nullptr)
			{
				// Cancelled while still waiting in the queue: it never started.
				Job = &Case.Jobs.AddDefaulted_GetRef();
				Job->Option = Option;
			}
			Job->bCancelled = true;
			Job->FinishedAt = Now;
			break;
		}
	case EEnemyBuildingTrainingEvent::ActivityOn:
		++Case.ActivityOnCount;
		break;
	case EEnemyBuildingTrainingEvent::ActivityOff:
		++Case.ActivityOffCount;
		Case.LastActivityOffAt = Now;
		break;
	default:
		break;
	}
	Log(FString::Printf(TEXT("RTS_EBT event %s %s %s t=%.2f duration=%.2f unit=%s"), *Case.Label,
	                    GetEventName(Event), *Option.GetDisplayName(), Now, Duration, *GetNameSafe(Unit)));
}

void UEnemyBuildingTrainerTestSubsystem::TickTrain()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		TickJobChecks(Case);
	}
	const bool bAllDone = GetAreAllJobsDone();
	float LongestJob = 0.f;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		for (const FEnemyBuildingTrainerTestJob& Job : Case.Jobs)
		{
			LongestJob = FMath::Max(LongestJob, Job.DurationSeconds);
		}
	}
	const float Elapsed = GetGameSeconds() - M_StepStartGameSeconds;
	const float QueuedJobs = FMath::Max(2, DisplaySquads.Num() + 1);
	if (Elapsed > QueuedJobs * LongestJob + TrainTimeoutSlackSeconds + ExitClearWindowSeconds)
	{
		M_GlobalFailures.Add(TEXT("training_phase_timeout"));
		SetStep(EEnemyBuildingTrainerTestStep::Settle);
		return;
	}
	if (bAllDone)
	{
		SetStep(EEnemyBuildingTrainerTestStep::Settle);
	}
}

void UEnemyBuildingTrainerTestSubsystem::TickJobChecks(FEnemyBuildingTrainerTestCase& Case)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
	const float Now = GetGameSeconds();
	const AEnemyController* const EnemyController = M_EnemyController.Get();
	UEnemyDirectControlComponent* const DirectControl = IsValid(EnemyController)
		                                                    ? EnemyController->GetEnemyDirectControlComponent()
		                                                    : nullptr;
	for (int32 JobIndex = 0; JobIndex < Case.Jobs.Num(); ++JobIndex)
	{
		FEnemyBuildingTrainerTestJob& Job = Case.Jobs[JobIndex];
		if (Job.bCancelled || Job.StartedAt < 0.f)
		{
			continue;
		}
		if (Job.FinishedAt < 0.f)
		{
			// Training: the building must show it is working, and the progress drives the mid-training frames.
			if (IsValid(Trainer) && not Trainer->MapTest_GetIsActivityOn())
			{
				Job.bActivityOffWhileTraining = true;
			}
			const float Progress = IsValid(Trainer) ? Trainer->GetActiveTrainingProgress() : 0.f;
			if (bM_SpawnQA && not Job.bQAPreCaptured && Progress >= SpawnQAPreProgress)
			{
				CaptureExitView(Case, FString::Printf(TEXT("job%d_a_pre"), JobIndex));
				Job.bQAPreCaptured = true;
			}
			if (not DisplaySquads.IsEmpty())
			{
				if (Job.CapturesTaken == 0 && Progress >= DisplayCaptureProgress)
				{
					const UEnemyTrainingSquadDisplayComponent* const Display = IsValid(Case.Building.Get())
						? Case.Building->FindComponentByClass<UEnemyTrainingSquadDisplayComponent>()
						: nullptr;
					Log(FString::Printf(TEXT("RTS_EBT display case=%s job=%d trains=%s %s"), *Case.Label, JobIndex,
					                    *Job.Option.GetTrainingName(),
					                    IsValid(Display) ? *Display->MapTest_DescribeShownState() : TEXT("no_display")));
					if (IsValid(Display) && not Display->GetIsShowingSquad())
					{
						Case.Failures.Add(FString::Printf(TEXT("display_not_showing_job%d"), JobIndex));
					}
					if (bM_Capture)
					{
						CaptureCase(Case, FString::Printf(TEXT("sq%d_%s"), JobIndex, *Job.Option.GetTrainingName()));
						// From the exit, looking back over the porch where the squad display stands.
						CaptureExitView(Case, FString::Printf(TEXT("sq%d_exitview"), JobIndex));
					}
					Job.CapturesTaken = 1;
				}
				continue;
			}
			if (bM_Capture && JobIndex == 0 && Job.CapturesTaken == 0 && Progress >= 0.3f)
			{
				CaptureCase(Case, TEXT("1_training30"));
				Job.CapturesTaken = 1;
			}
			else if (bM_Capture && JobIndex == 0 && Job.CapturesTaken == 1 && Progress >= 0.7f)
			{
				CaptureCase(Case, TEXT("2_training70"));
				Job.CapturesTaken = 2;
			}
			continue;
		}
		AActor* const Unit = Job.Unit.Get();
		if (Job.RegisteredAt < 0.f && IsValid(DirectControl) && IsValid(Unit)
			&& DirectControl->GetRegisteredDirectControlUnits().Contains(Unit))
		{
			Job.RegisteredAt = Now;
		}
		const bool bIsTank = Job.Option.UnitType == EAllUnitType::UNType_Tank;
		if (bIsTank && IsValid(Unit))
		{
			Job.MaxMovedCm = FMath::Max(Job.MaxMovedCm, FVector::Dist2D(Unit->GetActorLocation(), Job.SpawnLocation));
			// Spawn QA watches what the unit does on its own: no test move order.
			if (not bM_SpawnQA && not Job.bMoveOrdered && Job.RegisteredAt >= 0.f)
			{
				if (ICommands* const Commands = Cast<ICommands>(Unit))
				{
					const FVector ExitRight = FVector::CrossProduct(FVector::UpVector, Job.ExitForward);
					const FVector Target = Job.SpawnLocation + Job.ExitForward * ExitClearMoveDistanceCm
						+ ExitRight * (JobIndex * ExitClearLateralSpacingCm);
					Commands->MoveToLocation(Target, true, Job.ExitForward.Rotation(), false);
				}
				Job.bMoveOrdered = true;
			}
		}
		if (bM_SpawnQA)
		{
			TickSpawnQA(Case, JobIndex);
		}
	}
}

bool UEnemyBuildingTrainerTestSubsystem::GetAreAllJobsDone() const
{
	using namespace EnemyBuildingTrainerTestPrivate;
	const float Now = GetGameSeconds();
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (Case.Trainer.IsValid() && Case.Trainer->GetIsTraining())
		{
			return false;
		}
		for (const FEnemyBuildingTrainerTestJob& Job : Case.Jobs)
		{
			if (Job.bCancelled)
			{
				continue;
			}
			if (Job.FinishedAt < 0.f)
			{
				return false;
			}
			const bool bIsTank = Job.Option.UnitType == EAllUnitType::UNType_Tank;
			const bool bWaitsForRegistration = Job.RegisteredAt < 0.f && Now - Job.FinishedAt < RegistrationTimeoutSeconds;
			const bool bWaitsForExit = bIsTank && Job.MaxMovedCm < ExitClearMinMovedCm
				&& Now - Job.FinishedAt < ExitClearWindowSeconds;
			const bool bWaitsForSpawnQA = bM_SpawnQA && Job.QAStagesDone < SpawnQAStageCount
				&& Now - Job.FinishedAt < SpawnQAStages[SpawnQAStageCount - 1].SecondsAfterSpawn + 5.f;
			if (bWaitsForRegistration || bWaitsForExit || bWaitsForSpawnQA)
			{
				return false;
			}
		}
	}
	return true;
}

void UEnemyBuildingTrainerTestSubsystem::TickSettle()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	const float Elapsed = GetGameSeconds() - M_StepStartGameSeconds;
	if (Elapsed < SettleSeconds)
	{
		return;
	}
	static const FString AfterSuffix(TEXT("3_after"));
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (Case.Jobs.IsEmpty() || Case.Jobs[0].CapturesTaken >= 3 || not bM_Capture)
		{
			continue;
		}
		CaptureCase(Case, AfterSuffix);
		Case.Jobs[0].CapturesTaken = 3;
	}
	if (bM_Capture && Elapsed < RestCaptureSeconds)
	{
		return;
	}
	if (bM_Capture && not bM_RestCaptured)
	{
		for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
		{
			CaptureCase(Case, TEXT("4_rest"));
		}
		bM_RestCaptured = true;
	}

	// Per-job verdicts of the routed units and the second unit.
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		const UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
		if (IsValid(Trainer) && (Trainer->GetIsTraining() || Trainer->MapTest_GetIsActivityOn()))
		{
			Case.Failures.Add(TEXT("still_active_after_queue"));
		}
		for (int32 JobIndex = 0; JobIndex < Case.Jobs.Num(); ++JobIndex)
		{
			const FEnemyBuildingTrainerTestJob& Job = Case.Jobs[JobIndex];
			const FString JobTag = FString::Printf(TEXT("job%d"), JobIndex);
			const int32 Cost = GetOptionCost(Job.Option);
			const float ExpectedDuration = UEnemyBuildingTrainerComponent::GetTrainingSecondsForCost(Cost)
				* (IsValid(Trainer) ? Trainer->TrainingTimeMlt : 1.f);
			if (Job.bCancelled)
			{
				Case.Failures.Add(JobTag + TEXT("_cancelled"));
				continue;
			}
			if (not FMath::IsNearlyEqual(Job.DurationSeconds, ExpectedDuration, 0.05f))
			{
				Case.Failures.Add(FString::Printf(TEXT("%s_duration_%.2f_expected_%.2f"), *JobTag, Job.DurationSeconds,
				                                  ExpectedDuration));
			}
			const float TrainedSeconds = Job.FinishedAt - Job.StartedAt;
			if (Job.FinishedAt < 0.f)
			{
				Case.Failures.Add(JobTag + TEXT("_never_finished"));
				continue;
			}
			if (TrainedSeconds < Job.DurationSeconds - EarlySpawnToleranceSeconds)
			{
				Case.Failures.Add(FString::Printf(TEXT("%s_spawned_early_%.2f_of_%.2f"), *JobTag, TrainedSeconds,
				                                  Job.DurationSeconds));
			}
			if (TrainedSeconds > Job.DurationSeconds + LateSpawnToleranceSeconds)
			{
				Case.Failures.Add(FString::Printf(TEXT("%s_spawned_late_%.2f_of_%.2f"), *JobTag, TrainedSeconds,
				                                  Job.DurationSeconds));
			}
			if (Job.bActivityOffWhileTraining)
			{
				Case.Failures.Add(JobTag + TEXT("_activity_off_while_training"));
			}
			if (not Job.Unit.IsValid())
			{
				Case.Failures.Add(JobTag + TEXT("_no_unit"));
				continue;
			}
			const int32 UnitOwner = FRTSPlayers::GetOwningPlayerOfActor(Job.Unit.Get());
			if (UnitOwner != RTSPlayers::FirstAIPlayer)
			{
				Case.Failures.Add(FString::Printf(TEXT("%s_unit_owner_%d"), *JobTag, UnitOwner));
			}
			// The spawned Blueprint must be the unit that was ordered (spawner row and the Blueprint's own subtype).
			if (const URTSComponent* const RTSComponent = Job.Unit->FindComponentByClass<URTSComponent>())
			{
				if (RTSComponent->GetUnitType() != Job.Option.UnitType
					|| RTSComponent->GetUnitSubType() != Job.Option.SubtypeValue)
				{
					Case.Failures.Add(FString::Printf(TEXT("%s_spawned_%s_as_type%d_subtype%d"), *JobTag,
					                                  *GetNameSafe(Job.Unit->GetClass()),
					                                  static_cast<int32>(RTSComponent->GetUnitType()),
					                                  static_cast<int32>(RTSComponent->GetUnitSubType())));
				}
			}
			const float ExitDistance = FVector::Dist2D(Job.SpawnLocation, Job.ExitLocation);
			if (ExitDistance > MaxSpawnDistanceFromExitCm)
			{
				Case.Failures.Add(FString::Printf(TEXT("%s_spawned_%.0fcm_from_exit"), *JobTag, ExitDistance));
			}
			if (Job.RegisteredAt < 0.f)
			{
				Case.Failures.Add(JobTag + TEXT("_not_in_direct_control_pool"));
			}
			const bool bIsTank = Job.Option.UnitType == EAllUnitType::UNType_Tank;
			if (bIsTank && Job.MaxMovedCm < ExitClearMinMovedCm && not bM_SpawnQA)
			{
				Case.Failures.Add(FString::Printf(TEXT("%s_stuck_at_exit_moved_%.0fcm"), *JobTag, Job.MaxMovedCm));
			}
			Log(FString::Printf(
				TEXT("RTS_EBT job %s %s %s duration=%.2f trained=%.2f exit_dist=%.0f registered_after=%.2f moved=%.0f owner=%d"),
				*Case.Label, *JobTag, *Job.Option.GetDisplayName(), Job.DurationSeconds, TrainedSeconds, ExitDistance,
				Job.RegisteredAt >= 0.f ? Job.RegisteredAt - Job.FinishedAt : -1.f, Job.MaxMovedCm, UnitOwner));
		}
	}
	// The second unit at the first building ran back to back, with the activity on the whole time.
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (not Case.Trainer.IsValid())
		{
			continue;
		}
		if (bM_SecondJobQueued)
		{
			// Display runs queue their extra squads on every infantry building, the first one included.
			const int32 ExpectedJobs = 2 + (Case.ExpectedKind == EEnemyBuildingTrainerKind::Infantry
				                                ? FMath::Max(0, DisplaySquads.Num() - 1)
				                                : 0);
			if (Case.Jobs.Num() != ExpectedJobs)
			{
				Case.Failures.Add(FString::Printf(TEXT("expected_%d_jobs_got_%d"), ExpectedJobs, Case.Jobs.Num()));
			}
			else if (FMath::Abs(Case.Jobs[1].StartedAt - Case.Jobs[0].FinishedAt) > 1.f)
			{
				Case.Failures.Add(TEXT("second_job_did_not_start_after_first"));
			}
			if (Case.ActivityOnCount != 1 || Case.ActivityOffCount != 1)
			{
				Case.Failures.Add(FString::Printf(TEXT("activity_toggled_on%d_off%d_for_two_queued_units"),
				                                  Case.ActivityOnCount, Case.ActivityOffCount));
			}
		}
		break;
	}
	if (bM_SpawnQA)
	{
		// The QA run is about the units' appearance; the think and cancel phases are covered by the plain run.
		Finish();
		return;
	}
	StartRealThink();
}

void UEnemyBuildingTrainerTestSubsystem::StartRealThink()
{
	UEnemyStrategicAIComponent* const StrategicAI = M_StrategicAI.Get();
	if (not IsValid(StrategicAI))
	{
		StartCancel();
		return;
	}
	// One real think: tech unlocked for the kinds of buildings that stand, pressure for both kinds, enough points.
	FEnemyStrategicTrainingState& TrainingState = StrategicAI->MapTest_GetEditableTrainingState();
	TrainingState.TechLevelUnlockedMap.Add(EEnemyAITechLevel::BasicInfantry, true);
	TrainingState.TechLevelUnlockedMap.Add(EEnemyAITechLevel::LightTanks, true);
	bool bHasInfantryTrainer = false;
	bool bHasVehicleTrainer = false;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (Case.Trainer.IsValid())
		{
			bHasInfantryTrainer |= Case.ExpectedKind == EEnemyBuildingTrainerKind::Infantry;
			bHasVehicleTrainer |= Case.ExpectedKind == EEnemyBuildingTrainerKind::Vehicles;
		}
	}
	// Pressure only for kinds a building can train, so the pick cannot land on a kind nothing here trains.
	FEnemyStrategicTrainingPressureContribution Pressure;
	Pressure.SpecialtyPressure = EAITrainingFocusSpecialty::GeneralPurpose;
	Pressure.PressureAmount = 50.f;
	Pressure.SourceDebugName = TEXT("EnemyBuildingTrainerTest");
	if (bHasInfantryTrainer)
	{
		Pressure.FocusPressure = EAITrainingFocus::Infantry;
		TrainingState.AddTrainingPressureContribution(Pressure);
	}
	if (bHasVehicleTrainer)
	{
		Pressure.FocusPressure = EAITrainingFocus::LightTanks;
		TrainingState.AddTrainingPressureContribution(Pressure);
	}
	StrategicAI->MapTest_SetTrainingPoints(200);
	M_ThinkPointsBefore = StrategicAI->MapTest_GetTrainingPoints();
	M_ThinkJobsBefore = GetTotalJobCount();
	M_ThinkFinishedBefore = GetTotalFinishedCount();
	int32 QueuedBefore = 0;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		QueuedBefore += Case.Trainer.IsValid() ? Case.Trainer->GetQueueLength() : 0;
	}
	StrategicAI->MapTest_RunTrainingThinkStep();
	int32 QueuedAfter = 0;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		QueuedAfter += Case.Trainer.IsValid() ? Case.Trainer->GetQueueLength() : 0;
	}
	const int32 PointsSpent = M_ThinkPointsBefore - StrategicAI->MapTest_GetTrainingPoints();
	Log(FString::Printf(TEXT("RTS_EBT real_think points_spent=%d queued=%d"), PointsSpent, QueuedAfter - QueuedBefore));
	if (PointsSpent <= 0 || QueuedAfter <= QueuedBefore)
	{
		M_GlobalFailures.Add(FString::Printf(TEXT("real_think_spent_%d_queued_%d"), PointsSpent,
		                                     QueuedAfter - QueuedBefore));
	}
	SetStep(EEnemyBuildingTrainerTestStep::RealThink);
}

void UEnemyBuildingTrainerTestSubsystem::TickRealThink()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	// Nothing the think queued may come out before the shortest training time (20 s base).
	if (GetTotalFinishedCount() != M_ThinkFinishedBefore)
	{
		M_GlobalFailures.Add(TEXT("real_think_unit_appeared_before_training_time"));
	}
	if (GetGameSeconds() - M_StepStartGameSeconds < RealThinkWindowSeconds)
	{
		return;
	}
	// Drop what the think queued (refunds the waiting ones) so the cancel phase starts from empty queues.
	for (FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		if (Case.Trainer.IsValid())
		{
			Case.Trainer->CancelAllTraining();
		}
	}
	StartCancel();
}

void UEnemyBuildingTrainerTestSubsystem::StartCancel()
{
	M_CancelCaseIndex = INDEX_NONE;
	for (int32 CaseIndex = 0; CaseIndex < M_Cases.Num(); ++CaseIndex)
	{
		if (M_Cases[CaseIndex].Trainer.IsValid() && M_Cases[CaseIndex].Building.IsValid())
		{
			M_CancelCaseIndex = CaseIndex;
			break;
		}
	}
	UEnemyStrategicAIComponent* const StrategicAI = M_StrategicAI.Get();
	if (M_CancelCaseIndex == INDEX_NONE || not IsValid(StrategicAI))
	{
		Finish();
		return;
	}
	FEnemyBuildingTrainerTestCase& Case = M_Cases[M_CancelCaseIndex];
	const FTrainingOption Option = EnemyBuildingTrainerTestPrivate::GetTestOption(Case.ExpectedKind);
	const int32 Cost = EnemyBuildingTrainerTestPrivate::GetOptionCost(Option);
	M_CancelRefundBefore = StrategicAI->MapTest_GetRefundedTrainingPoints();
	M_CancelFinishedBefore = GetTotalFinishedCount();
	M_CancelCancelledBefore = GetTotalCancelledCount();
	const bool bFirst = Case.Trainer->EnqueueTraining(Option, Cost, StrategicAI);
	const bool bSecond = Case.Trainer->EnqueueTraining(Option, Cost, StrategicAI);
	// Only the unit still waiting is refunded; the one in training is lost with the building.
	M_CancelExpectedRefund = Cost;
	if (not bFirst || not bSecond)
	{
		M_GlobalFailures.Add(TEXT("cancel_phase_could_not_queue"));
	}
	SetStep(EEnemyBuildingTrainerTestStep::Cancel);
}

void UEnemyBuildingTrainerTestSubsystem::TickCancel()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	if (not M_Cases.IsValidIndex(M_CancelCaseIndex))
	{
		Finish();
		return;
	}
	FEnemyBuildingTrainerTestCase& Case = M_Cases[M_CancelCaseIndex];
	if (not bM_CancelBuildingDestroyed)
	{
		UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
		if (IsValid(Trainer) && Trainer->GetActiveTrainingProgress() < CancelProgressToDestroy)
		{
			return;
		}
		if (AActor* const Building = Case.Building.Get())
		{
			Log(FString::Printf(TEXT("RTS_EBT cancel destroying %s mid-training"), *Case.Label));
			Building->Destroy();
		}
		bM_CancelBuildingDestroyed = true;
		M_CancelDestroyedAt = GetGameSeconds();
		return;
	}
	if (GetGameSeconds() - M_CancelDestroyedAt < CancelWatchSeconds)
	{
		return;
	}
	const UEnemyStrategicAIComponent* const StrategicAI = M_StrategicAI.Get();
	const int32 Refunded = IsValid(StrategicAI)
		                       ? StrategicAI->MapTest_GetRefundedTrainingPoints() - M_CancelRefundBefore
		                       : -1;
	const int32 Cancelled = GetTotalCancelledCount() - M_CancelCancelledBefore;
	const int32 Finished = GetTotalFinishedCount() - M_CancelFinishedBefore;
	Log(FString::Printf(TEXT("RTS_EBT cancel %s cancelled=%d refunded=%d expected_refund=%d finished=%d"),
	                    *Case.Label, Cancelled, Refunded, M_CancelExpectedRefund, Finished));
	if (Cancelled != 2)
	{
		M_GlobalFailures.Add(FString::Printf(TEXT("cancel_expected_2_cancelled_got_%d"), Cancelled));
	}
	if (Refunded != M_CancelExpectedRefund)
	{
		M_GlobalFailures.Add(FString::Printf(TEXT("cancel_refund_%d_expected_%d"), Refunded, M_CancelExpectedRefund));
	}
	if (Finished != 0)
	{
		M_GlobalFailures.Add(FString::Printf(TEXT("cancel_destroyed_building_still_trained_%d"), Finished));
	}
	// The cancel jobs are no failure of the per-building verdicts.
	Finish();
}

void UEnemyBuildingTrainerTestSubsystem::StartSpawnerAudit()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	// Every unit of every tech level: the C++ defaults and what this map's controller carries.
	TArray<FTrainingOption> Options;
	auto AddLevel = [&Options](const FEnemyTrainingOptionsForTechLevel& Level)
	{
		for (const ESquadSubtype Squad : Level.TrainableSquadSubtypes)
		{
			Options.AddUnique(FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(Squad)));
		}
		for (const ETankSubtype Tank : Level.TrainableTankSubtypes)
		{
			Options.AddUnique(FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(Tank)));
		}
	};
	auto AddAllLevels = [&AddLevel](const FEnemyLevelTraining& Training)
	{
		AddLevel(Training.BasicInfantryOptions);
		AddLevel(Training.LightTankOptions);
		AddLevel(Training.MediumTankOptions);
		AddLevel(Training.Tier2Options);
		AddLevel(Training.AdvancedInfantryOptions);
		AddLevel(Training.Tier3Options);
		AddLevel(Training.ExperimentalOptions);
	};
	AddAllLevels(FEnemyLevelTraining());
	if (UEnemyStrategicAIComponent* const StrategicAI = M_StrategicAI.Get())
	{
		AddAllLevels(StrategicAI->MapTest_GetEditableTrainingState().EnemyLevelTraining);
	}
	ARTSAsyncSpawner* const AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		M_GlobalFailures.Add(TEXT("audit_no_async_spawner"));
		Finish();
		return;
	}
	TArray<FVector> Slots;
	CollectNavigableGridSlots(Slots);
	if (Slots.IsEmpty())
	{
		M_GlobalFailures.Add(TEXT("audit_no_navigable_slots"));
		Finish();
		return;
	}
	const int32 OwnerOverride = FRTSUnitOwnerSpawning::GetOwnerOverrideForAIPlayer(RTSPlayers::FirstAIPlayer);
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		FEnemyBuildingTrainerAuditEntry& Entry = M_AuditEntries.AddDefaulted_GetRef();
		Entry.Option = Options[Index];
		Entry.bHasSpawnerRow = AsyncSpawner->GetHasClassForTrainingOption(Entry.Option);
		if (not Entry.bHasSpawnerRow)
		{
			continue;
		}
		const FVector Location = Slots[Index % Slots.Num()]
			+ FVector((Index / Slots.Num()) * AuditUnitSpacingCm, 0.f, 0.f);
		const int32 EntryIndex = M_AuditEntries.Num() - 1;
		const TWeakObjectPtr<UEnemyBuildingTrainerTestSubsystem> WeakThis(this);
		const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
			Entry.Option, Location, this, EntryIndex,
			[WeakThis](const FTrainingOption& /*Option*/, AActor* SpawnedActor, const int32 ID)
			{
				if (not WeakThis.IsValid() || not WeakThis->M_AuditEntries.IsValidIndex(ID))
				{
					return;
				}
				WeakThis->M_AuditEntries[ID].bHasCallback = true;
				WeakThis->M_AuditEntries[ID].Unit = SpawnedActor;
			}, FRotator::ZeroRotator, OwnerOverride);
		// The array may not grow after this point is taken, so the flag is written by index.
		M_AuditEntries[EntryIndex].bIsSpawnRequested = bRequested;
	}
	Log(FString::Printf(TEXT("RTS_EBT audit units=%d"), M_AuditEntries.Num()));
	SetStep(EEnemyBuildingTrainerTestStep::SpawnerAudit);
}

void UEnemyBuildingTrainerTestSubsystem::TickSpawnerAudit()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	const float Elapsed = GetGameSeconds() - M_StepStartGameSeconds;
	bool bAllCallbacks = true;
	for (const FEnemyBuildingTrainerAuditEntry& Entry : M_AuditEntries)
	{
		bAllCallbacks &= not Entry.bIsSpawnRequested || Entry.bHasCallback;
	}
	if (not bAllCallbacks && Elapsed < AuditSpawnTimeoutSeconds)
	{
		return;
	}
	if (Elapsed < AuditSettleSeconds)
	{
		return;
	}
	int32 OkCount = 0;
	for (const FEnemyBuildingTrainerAuditEntry& Entry : M_AuditEntries)
	{
		const FString OptionName = Entry.Option.GetTrainingName();
		FString Result;
		FString ClassName = TEXT("-");
		FString Reported = TEXT("-");
		if (not Entry.bHasSpawnerRow)
		{
			Result = TEXT("NO_SPAWNER_ROW");
		}
		else if (not Entry.bIsSpawnRequested || not Entry.Unit.IsValid())
		{
			Result = TEXT("NO_SPAWN");
		}
		else
		{
			AActor* const Unit = Entry.Unit.Get();
			ClassName = GetNameSafe(Unit->GetClass());
			const URTSComponent* const RTSComponent = Unit->FindComponentByClass<URTSComponent>();
			if (not IsValid(RTSComponent))
			{
				Result = TEXT("NO_RTS_COMPONENT");
			}
			else
			{
				Reported = FTrainingOption(RTSComponent->GetUnitType(), RTSComponent->GetUnitSubType())
					.GetTrainingName();
				const bool bMatches = RTSComponent->GetUnitType() == Entry.Option.UnitType
					&& RTSComponent->GetUnitSubType() == Entry.Option.SubtypeValue;
				Result = bMatches ? TEXT("OK") : TEXT("SUBTYPE_MISMATCH");
			}
			if (ASquadController* const Squad = Cast<ASquadController>(Unit))
			{
				// The squad healthbar's icon rule: the highest valued weapon of any member.
				EWeaponName BestWeapon = EWeaponName::DEFAULT_WEAPON;
				int32 BestValue = -1;
				for (ASquadUnit* const Member : Squad->GetSquadUnitsChecked())
				{
					UWeaponState* const State = IsValid(Member) ? Member->GetWeaponState() : nullptr;
					if (State == nullptr)
					{
						continue;
					}
					const EWeaponName Weapon = State->GetRawWeaponData().WeaponName;
					if (Global_GetWeaponValue(Weapon) > BestValue)
					{
						BestValue = Global_GetWeaponValue(Weapon);
						BestWeapon = Weapon;
					}
				}
				Log(FString::Printf(TEXT("RTS_EBT audit_icon %s members=%d icon=%s"), *OptionName,
				                    Squad->GetSquadUnitsCount(),
				                    *UEnum::GetValueAsString(Global_GetWeaponIconForWeapon(BestWeapon))));
			}
		}
		if (Result == TEXT("OK"))
		{
			++OkCount;
		}
		else
		{
			M_GlobalFailures.Add(OptionName + TEXT(":") + Result);
		}
		Log(FString::Printf(TEXT("RTS_EBT audit %s class=%s reports=%s result=%s"), *OptionName, *ClassName,
		                    *Reported, *Result));
	}
	Log(FString::Printf(TEXT("RTS_EBT audit ok=%d of %d"), OkCount, M_AuditEntries.Num()));
	for (const FEnemyBuildingTrainerAuditEntry& Entry : M_AuditEntries)
	{
		if (AActor* const Unit = Entry.Unit.Get())
		{
			Unit->Destroy();
		}
	}
	Finish();
}

int32 UEnemyBuildingTrainerTestSubsystem::GetTotalJobCount() const
{
	int32 Count = 0;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		Count += Case.Jobs.Num();
	}
	return Count;
}

int32 UEnemyBuildingTrainerTestSubsystem::GetTotalFinishedCount() const
{
	int32 Count = 0;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		for (const FEnemyBuildingTrainerTestJob& Job : Case.Jobs)
		{
			Count += (Job.FinishedAt >= 0.f && not Job.bCancelled) ? 1 : 0;
		}
	}
	return Count;
}

int32 UEnemyBuildingTrainerTestSubsystem::GetTotalCancelledCount() const
{
	int32 Count = 0;
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		for (const FEnemyBuildingTrainerTestJob& Job : Case.Jobs)
		{
			Count += Job.bCancelled ? 1 : 0;
		}
	}
	return Count;
}

bool UEnemyBuildingTrainerTestSubsystem::EnsureCaptureActor()
{
	using namespace EnemyBuildingTrainerTestPrivate;
	if (IsValid(M_CaptureActor) && IsValid(M_CaptureTarget))
	{
		return true;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_CaptureActor = GetWorld()->SpawnActor<ASceneCapture2D>(FVector::ZeroVector, FRotator::ZeroRotator,
	                                                         SpawnParameters);
	if (not IsValid(M_CaptureActor))
	{
		return false;
	}
	M_CaptureTarget = NewObject<UTextureRenderTarget2D>(this);
	M_CaptureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
	M_CaptureTarget->InitAutoFormat(CaptureWidth, CaptureHeight);
	M_CaptureTarget->UpdateResourceImmediate(true);
	USceneCaptureComponent2D* const CaptureComponent = M_CaptureActor->GetCaptureComponent2D();
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = 50.f;
	CaptureComponent->TextureTarget = M_CaptureTarget;
	// The fog of war is a post-process material: it would darken the enemy building the capture is about.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CaptureComponent->PostProcessSettings.AutoExposureBias = bM_HasExposureBias ? M_ExposureBias
		                                                         : DefaultCaptureExposureBias;
	CaptureComponent->PostProcessBlendWeight = 1.f;
	return true;
}

void UEnemyBuildingTrainerTestSubsystem::CaptureCase(const FEnemyBuildingTrainerTestCase& Case, const FString& Suffix)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	AActor* const Building = Case.Building.Get();
	const UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
	if (not IsValid(Building) || not EnsureCaptureActor())
	{
		return;
	}
	// Frame on what is drawn: the union of the visible meshes (actor bounds include tall invisible helpers).
	FBox VisibleBox(ForceInit);
	TInlineComponentArray<UMeshComponent*> Meshes(Building);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (not IsValid(Mesh) || not Mesh->IsVisible() || Mesh->bHiddenInGame)
		{
			continue;
		}
		const FBox MeshBox = Mesh->Bounds.GetBox();
		if (MeshBox.GetExtent().GetMax() > MaxFramedComponentExtentCm)
		{
			continue;
		}
		VisibleBox += MeshBox;
	}
	if (not VisibleBox.IsValid)
	{
		VisibleBox = FBox::BuildAABB(Building->GetActorLocation(), FVector(1000.f));
	}
	const FVector Origin = VisibleBox.GetCenter();
	const FVector Extent = VisibleBox.GetExtent();
	const float Radius = FMath::Max(FMath::Max(Extent.X, Extent.Y), 800.f);
	// From in front of the exit, 35 degrees off to the side, looking down like the RTS camera.
	const FVector ExitForward = IsValid(Trainer)
		                            ? Trainer->GetExitTransform().GetRotation().GetForwardVector().GetSafeNormal2D()
		                            : Building->GetActorForwardVector();
	const FVector CameraDirection = ExitForward.RotateAngleAxis(35.f, FVector::UpVector);
	const FRotator LookRotation(-40.f, (-CameraDirection).Rotation().Yaw, 0.f);
	const FVector ViewLocation = Origin - LookRotation.Vector() * Radius * CaptureDistanceRadiusMlt;
	// The fog of war hides enemy buildings the human player cannot see.
	Building->SetActorHiddenInGame(false);
	// Meshes that tick their pose only while rendered would show the pose of the previous capture.
	TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(Building);
	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
	{
		if (IsValid(SkeletalMesh) && SkeletalMesh->IsVisible())
		{
			SkeletalMesh->TickAnimation(0.f, false);
			SkeletalMesh->RefreshBoneTransforms();
		}
	}
	M_CaptureActor->SetActorLocationAndRotation(ViewLocation, LookRotation);
	USceneCaptureComponent2D* const CaptureComponent = M_CaptureActor->GetCaptureComponent2D();
	CaptureComponent->CaptureScene();
	FImage Image;
	if (not FImageUtils::GetRenderTargetImage(M_CaptureTarget, Image))
	{
		Log(FString::Printf(TEXT("RTS_EBT capture %s %s failed"), *Case.Label, *Suffix));
		return;
	}
	const FString Path = FPaths::Combine(M_OutputDirectory, Case.Label + TEXT("_") + Suffix + TEXT(".png"));
	FImageUtils::SaveImageByExtension(*Path, Image);
	Log(FString::Printf(TEXT("RTS_EBT capture %s"), *Path));
}

void UEnemyBuildingTrainerTestSubsystem::TickSpawnQA(FEnemyBuildingTrainerTestCase& Case, const int32 JobIndex)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	FEnemyBuildingTrainerTestJob& Job = Case.Jobs[JobIndex];
	if (Job.FinishedAt < 0.f || Job.QAStagesDone >= SpawnQAStageCount)
	{
		return;
	}
	const float Now = GetGameSeconds();
	const float SinceSpawn = Now - Job.FinishedAt;
	if (SinceSpawn <= SpawnQASampleSeconds)
	{
		const float SampleDelta = Job.QALastSampleSeconds >= 0.f ? Now - Job.QALastSampleSeconds : 0.f;
		Job.QALastSampleSeconds = Now;
		TArray<AActor*> Bodies;
		CollectUnitBodies(Job.Unit.Get(), Bodies);
		for (AActor* const Body : Bodies)
		{
			// The squad unit optimizer moves soldiers out of the player's view in 0.5 s ticks (250 cm steps at walking
			// speed); per-frame movement here, as SquadMarchTestScenario does, so a step is a real teleport.
			if (const ACharacter* const Character = Cast<ACharacter>(Body))
			{
				if (UCharacterMovementComponent* const Movement = Character->GetCharacterMovement())
				{
					Movement->SetComponentTickInterval(0.f);
				}
			}
			const FVector Location = Body->GetActorLocation();
			const float Speed = Body->GetVelocity().Size();
			FEnemyBuildingTrainerQABodySample* const Last = Job.QABodySamples.FindByPredicate(
				[Body](const FEnemyBuildingTrainerQABodySample& Sample) { return Sample.Body.Get() == Body; });
			if (Last != nullptr)
			{
				// Movement its speed explains (either end of the tick) is walking or driving, the rest a teleport.
				const float Explained = FMath::Max(Last->Speed, Speed) * SampleDelta;
				const float Jump = FVector::Dist(Last->Location, Location) - Explained;
				if (Jump > SpawnQAMaxBodyJumpCm)
				{
					const ACharacter* const Character = Cast<ACharacter>(Body);
					const USkeletalMeshComponent* const BodyMesh = IsValid(Character) ? Character->GetMesh() : nullptr;
					Log(FString::Printf(
						TEXT("RTS_EBT spawnqa jump %s %s t=%.3f dt=%.3f from=%s to=%s jump=%.0f speed=%.0f..%.0f hidden=%d rendered=%d"),
						*Case.Label, *GetNameSafe(Body), SinceSpawn, SampleDelta, *Last->Location.ToCompactString(),
						*Location.ToCompactString(), Jump, Last->Speed, Speed, Body->IsHidden() ? 1 : 0,
						IsValid(BodyMesh) && BodyMesh->WasRecentlyRendered(0.5f) ? 1 : 0));
				}
				Job.QAMaxBodyJumpCm = FMath::Max(Job.QAMaxBodyJumpCm, Jump);
				Last->Location = Location;
				Last->Speed = Speed;
			}
			else
			{
				Job.QABodySamples.Add({Body, Location, Speed});
			}
			const UPrimitiveComponent* const BodyRoot = Cast<UPrimitiveComponent>(Body->GetRootComponent());
			float GroundZ = Location.Z;
			if (SinceSpawn >= SpawnQAAirborneIgnoreSeconds && IsValid(BodyRoot) && TryGetGroundZ(Location, GroundZ))
			{
				Job.QAMaxAirborneCm = FMath::Max(Job.QAMaxAirborneCm, BodyRoot->Bounds.GetBox().Min.Z - GroundZ);
			}
		}
	}
	const FSpawnQAStage& Stage = SpawnQAStages[Job.QAStagesDone];
	if (GetGameSeconds() - Job.FinishedAt < Stage.SecondsAfterSpawn)
	{
		return;
	}
	SpawnQAStage(Case, JobIndex, Stage.Tag);
	++Job.QAStagesDone;
}

void UEnemyBuildingTrainerTestSubsystem::CollectUnitBodies(AActor* Unit, TArray<AActor*>& OutBodies)
{
	if (not IsValid(Unit))
	{
		return;
	}
	if (ASquadController* const SquadController = Cast<ASquadController>(Unit))
	{
		for (ASquadUnit* const SquadUnit : SquadController->GetSquadUnitsChecked())
		{
			if (IsValid(SquadUnit))
			{
				OutBodies.Add(SquadUnit);
			}
		}
		return;
	}
	OutBodies.Add(Unit);
}

void UEnemyBuildingTrainerTestSubsystem::SpawnQAStage(FEnemyBuildingTrainerTestCase& Case, const int32 JobIndex,
                                                      const TCHAR* StageTag)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	FEnemyBuildingTrainerTestJob& Job = Case.Jobs[JobIndex];
	const FString Suffix = FString::Printf(TEXT("job%d_%s"), JobIndex, StageTag);
	AActor* const Unit = Job.Unit.Get();
	AActor* const Building = Case.Building.Get();
	const UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
	TArray<AActor*> Bodies;
	CollectUnitBodies(Unit, Bodies);
	// Measured before the capture, which un-hides fogged units for the frame.
	int32 HiddenByFog = 0;
	for (const AActor* const Body : Bodies)
	{
		HiddenByFog += Body->IsHidden() ? 1 : 0;
	}
	CaptureExitView(Case, Suffix);
	if (Bodies.IsEmpty() || not IsValid(Building) || not IsValid(Trainer))
	{
		Log(FString::Printf(TEXT("RTS_EBT spawnqa %s %s no_unit_bodies"), *Case.Label, *Suffix));
		return;
	}

	const FTransform Exit = Trainer->GetExitTransform();
	const float ExitYaw = Exit.Rotator().Yaw;
	TInlineComponentArray<UPrimitiveComponent*> BuildingPrimitives(Building);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBuildingTrainerSpawnQA), false);
	FVector Centroid = FVector::ZeroVector;
	float MinGroundOffset = TNumericLimits<float>::Max();
	float MaxGroundOffset = TNumericLimits<float>::Lowest();
	float MinBuildingClearance = TNumericLimits<float>::Max();
	float MaxYawOffset = 0.f;
	int32 InsideBuilding = 0;
	for (AActor* const Body : Bodies)
	{
		const FVector BodyLocation = Body->GetActorLocation();
		Centroid += BodyLocation;
		MaxYawOffset = FMath::Max(MaxYawOffset,
		                          FMath::Abs(FMath::FindDeltaAngleDegrees(ExitYaw, Body->GetActorRotation().Yaw)));
		UPrimitiveComponent* const BodyRoot = Cast<UPrimitiveComponent>(Body->GetRootComponent());
		if (not IsValid(BodyRoot))
		{
			continue;
		}
		float GroundZ = BodyLocation.Z;
		if (TryGetGroundZ(BodyLocation, GroundZ))
		{
			const float GroundOffset = BodyRoot->Bounds.GetBox().Min.Z - GroundZ;
			MinGroundOffset = FMath::Min(MinGroundOffset, GroundOffset);
			MaxGroundOffset = FMath::Max(MaxGroundOffset, GroundOffset);
		}
		const ECollisionChannel BodyChannel = BodyRoot->GetCollisionObjectType();
		bool bIsInside = false;
		for (UPrimitiveComponent* const Primitive : BuildingPrimitives)
		{
			if (not IsValid(Primitive) || not Primitive->IsCollisionEnabled()
				|| Primitive->GetCollisionResponseToChannel(BodyChannel) != ECR_Block)
			{
				continue;
			}
			FVector ClosestPoint;
			const float Distance = Primitive->GetDistanceToCollision(BodyLocation, ClosestPoint);
			if (Distance >= 0.f)
			{
				MinBuildingClearance = FMath::Min(MinBuildingClearance, Distance);
			}
			if (not bIsInside && Primitive->ComponentOverlapComponent(BodyRoot, BodyRoot->GetComponentLocation(),
			                                                         BodyRoot->GetComponentQuat(), QueryParams))
			{
				bIsInside = true;
			}
		}
		InsideBuilding += bIsInside ? 1 : 0;
	}
	Centroid /= Bodies.Num();
	if (Job.QAStagesDone == 0)
	{
		Job.QASpawnCentroid = Centroid;
	}
	const float MovedCm = FVector::Dist2D(Centroid, Job.QASpawnCentroid);
	Job.MaxMovedCm = FMath::Max(Job.MaxMovedCm, MovedCm);

	// Stacking: bodies of this unit that overlap the previous unit of the same building, and the closest pair.
	int32 OverlapsPrevious = 0;
	float NearestPreviousCm = -1.f;
	if (Case.Jobs.IsValidIndex(JobIndex - 1))
	{
		TArray<AActor*> PreviousBodies;
		CollectUnitBodies(Case.Jobs[JobIndex - 1].Unit.Get(), PreviousBodies);
		NearestPreviousCm = PreviousBodies.IsEmpty() ? -1.f : TNumericLimits<float>::Max();
		for (AActor* const Body : Bodies)
		{
			UPrimitiveComponent* const BodyRoot = Cast<UPrimitiveComponent>(Body->GetRootComponent());
			for (AActor* const PreviousBody : PreviousBodies)
			{
				NearestPreviousCm = FMath::Min(NearestPreviousCm, FVector::Dist2D(Body->GetActorLocation(),
				                                                                  PreviousBody->GetActorLocation()));
				UPrimitiveComponent* const PreviousRoot = Cast<UPrimitiveComponent>(PreviousBody->GetRootComponent());
				if (IsValid(BodyRoot) && IsValid(PreviousRoot)
					&& PreviousRoot->ComponentOverlapComponent(BodyRoot, BodyRoot->GetComponentLocation(),
					                                           BodyRoot->GetComponentQuat(), QueryParams))
				{
					++OverlapsPrevious;
				}
			}
		}
	}

	// Stacking on any other trained unit of the test, at any building.
	int32 OverlapsAny = 0;
	float NearestAnyCm = -1.f;
	for (const FEnemyBuildingTrainerTestCase& OtherCase : M_Cases)
	{
		for (const FEnemyBuildingTrainerTestJob& OtherJob : OtherCase.Jobs)
		{
			if (OtherJob.Unit.Get() == Unit || not OtherJob.Unit.IsValid())
			{
				continue;
			}
			TArray<AActor*> OtherBodies;
			CollectUnitBodies(OtherJob.Unit.Get(), OtherBodies);
			for (AActor* const Body : Bodies)
			{
				UPrimitiveComponent* const BodyRoot = Cast<UPrimitiveComponent>(Body->GetRootComponent());
				for (AActor* const OtherBody : OtherBodies)
				{
					const float Distance = FVector::Dist2D(Body->GetActorLocation(), OtherBody->GetActorLocation());
					NearestAnyCm = NearestAnyCm < 0.f ? Distance : FMath::Min(NearestAnyCm, Distance);
					UPrimitiveComponent* const OtherRoot = Cast<UPrimitiveComponent>(OtherBody->GetRootComponent());
					if (IsValid(BodyRoot) && IsValid(OtherRoot)
						&& OtherRoot->ComponentOverlapComponent(BodyRoot, BodyRoot->GetComponentLocation(),
						                                        BodyRoot->GetComponentQuat(), QueryParams))
					{
						++OverlapsAny;
						// Which soldiers, where, and the cover each holds (cover state, point id).
						const ASquadUnit* const Soldier = Cast<ASquadUnit>(Body);
						const ASquadUnit* const OtherSoldier = Cast<ASquadUnit>(OtherBody);
						Log(FString::Printf(
							TEXT("RTS_EBT spawnqa overlap %s %s at %s cover=%d/%lld with %s (%s) at %s cover=%d/%lld"),
							*Case.Label, *GetNameSafe(Body), *Body->GetActorLocation().ToCompactString(),
							IsValid(Soldier) ? static_cast<int32>(Soldier->GetCoverRuntimeState().State) : -1,
							IsValid(Soldier) ? Soldier->GetCoverRuntimeState().AssignedCoverPoint.PointId : -1ll,
							*GetNameSafe(OtherBody), *OtherCase.Label, *OtherBody->GetActorLocation().ToCompactString(),
							IsValid(OtherSoldier) ? static_cast<int32>(OtherSoldier->GetCoverRuntimeState().State) : -1,
							IsValid(OtherSoldier) ? OtherSoldier->GetCoverRuntimeState().AssignedCoverPoint.PointId : -1ll));
					}
				}
			}
		}
	}
	// In the strip straight out of its own exit (where the next unit comes out).
	const FVector ExitForward2D = Exit.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector ToCentroid = FVector(Centroid.X - Exit.GetLocation().X, Centroid.Y - Exit.GetLocation().Y, 0.f);
	const float LaneHalfWidth = Trainer->TrainerKind == EEnemyBuildingTrainerKind::Vehicles
		                            ? EnemyAISettings::UnitTraining::StandSpotVehicleExitLaneHalfWidthCm
		                            : EnemyAISettings::UnitTraining::StandSpotInfantryExitLaneHalfWidthCm;
	const float Along = FVector::DotProduct(ToCentroid, ExitForward2D);
	const bool bInExitLane = Along >= 0.f && Along <= EnemyAISettings::UnitTraining::StandSpotExitLaneLengthCm
		&& FMath::Abs(FVector::DotProduct(ToCentroid, FVector::CrossProduct(FVector::UpVector, ExitForward2D)))
		<= LaneHalfWidth;
	// Bodies standing in any trainer's exit lane (soldiers spread out of their squad, into cover).
	int32 BodiesInExitLanes = 0;
	for (const FEnemyBuildingTrainerTestCase& LaneCase : M_Cases)
	{
		const UEnemyBuildingTrainerComponent* const LaneTrainer = LaneCase.Trainer.Get();
		if (not IsValid(LaneTrainer))
		{
			continue;
		}
		const FTransform LaneExit = LaneTrainer->GetExitTransform();
		const FVector LaneForward = LaneExit.GetRotation().GetForwardVector().GetSafeNormal2D();
		const FVector LaneRight = FVector::CrossProduct(FVector::UpVector, LaneForward);
		const float LaneWidth = LaneTrainer->TrainerKind == EEnemyBuildingTrainerKind::Vehicles
			                        ? EnemyAISettings::UnitTraining::StandSpotVehicleExitLaneHalfWidthCm
			                        : EnemyAISettings::UnitTraining::StandSpotInfantryExitLaneHalfWidthCm;
		for (const AActor* const Body : Bodies)
		{
			const FVector Offset = Body->GetActorLocation() - LaneExit.GetLocation();
			const float BodyAlong = FVector::DotProduct(FVector(Offset.X, Offset.Y, 0.f), LaneForward);
			if (BodyAlong >= 0.f && BodyAlong <= EnemyAISettings::UnitTraining::StandSpotExitLaneLengthCm
				&& FMath::Abs(FVector::DotProduct(FVector(Offset.X, Offset.Y, 0.f), LaneRight)) <= LaneWidth)
			{
				++BodiesInExitLanes;
			}
		}
	}
	float NearestBodyToExitCm = TNumericLimits<float>::Max();
	for (const AActor* const Body : Bodies)
	{
		NearestBodyToExitCm = FMath::Min(NearestBodyToExitCm,
		                                 FVector::Dist2D(Body->GetActorLocation(), Exit.GetLocation()));
	}
	const bool bIsLastStage = Job.QAStagesDone == SpawnQAStageCount - 1;
	const FString JobTag = FString::Printf(TEXT("job%d"), JobIndex);
	const bool bIsFogControl = M_Cases.IsValidIndex(M_FogControlCaseIndex) && &M_Cases[M_FogControlCaseIndex] == &Case;
	if (bIsFogControl)
	{
		// The human player sees this exit: hidden-until-seen must show its units once the fog has ticked.
		if (Job.QAStagesDone >= SpawnQAFogControlFirstCheckedStage && HiddenByFog > 0
			&& not Case.Failures.Contains(JobTag + TEXT("_hidden_although_in_vision")))
		{
			Case.Failures.Add(JobTag + TEXT("_hidden_although_in_vision"));
		}
	}
	else if (HiddenByFog < Bodies.Num() && not Case.Failures.Contains(JobTag + TEXT("_seen_through_fog")))
	{
		Case.Failures.Add(JobTag + TEXT("_seen_through_fog"));
	}
	if (bIsLastStage)
	{
		if (Job.QAMaxBodyJumpCm > SpawnQAMaxBodyJumpCm)
		{
			Case.Failures.Add(FString::Printf(TEXT("%s_teleported_%.0fcm_after_spawn"), *JobTag, Job.QAMaxBodyJumpCm));
		}
		if (Job.QAMaxAirborneCm > SpawnQAMaxAirborneCm)
		{
			Case.Failures.Add(FString::Printf(TEXT("%s_dropped_from_%.0fcm"), *JobTag, Job.QAMaxAirborneCm));
		}
		if (OverlapsAny > 0)
		{
			Case.Failures.Add(FString::Printf(TEXT("%s_overlaps_%d_bodies_of_other_units"), *JobTag, OverlapsAny));
		}
		// Per body: a squad split into cover on both sides of a door has its centre in front of it, blocking nothing.
		if (NearestBodyToExitCm < SpawnQAMinBodyExitDistanceCm)
		{
			Case.Failures.Add(FString::Printf(TEXT("%s_body_%.0fcm_from_exit"), *JobTag, NearestBodyToExitCm));
		}
		if (BodiesInExitLanes > 0)
		{
			Case.Failures.Add(FString::Printf(TEXT("%s_%d_bodies_in_an_exit_lane"), *JobTag, BodiesInExitLanes));
		}
	}

	// Where the exit sits: in front of the building (positive) and how far from the building's collision.
	FVector BuildingOrigin;
	FVector BuildingExtent;
	Building->GetActorBounds(true, BuildingOrigin, BuildingExtent);
	const FVector ExitForward = Exit.GetRotation().GetForwardVector().GetSafeNormal2D();
	const float ExitFacesOut = FVector::DotProduct(ExitForward,
	                                               (Exit.GetLocation() - Building->GetActorLocation()).GetSafeNormal2D());
	ICommands* const Commands = Cast<ICommands>(Unit);
	const FString ActiveCommand = Commands != nullptr
		                              ? StaticEnum<EAbilityID>()->GetNameStringByValue(
			                              static_cast<int64>(Commands->GetActiveCommandID()))
		                              : FString(TEXT("none"));
	Log(FString::Printf(
		TEXT("RTS_EBT spawnqa %s %s %s t=%.2f bodies=%d fog_hidden=%d ground_dz=%.0f..%.0f inside_building=%d ")
		TEXT("clear_min=%.0f overlap_prev=%d near_prev=%.0f overlap_any=%d near_any=%.0f in_exit_lane=%d bodies_in_lanes=%d nearest_body_to_exit=%.0f yaw_off=%.0f ")
		TEXT("moved=%.0f exit_dist=%.0f exit_faces_out=%.2f max_jump=%.0f max_air=%.0f fog_control=%d command=%s"),
		*Case.Label, *Suffix, *Job.Option.GetDisplayName(), GetGameSeconds() - Job.FinishedAt, Bodies.Num(),
		HiddenByFog, MinGroundOffset == TNumericLimits<float>::Max() ? -1.f : MinGroundOffset,
		MaxGroundOffset == TNumericLimits<float>::Lowest() ? -1.f : MaxGroundOffset, InsideBuilding,
		MinBuildingClearance == TNumericLimits<float>::Max() ? -1.f : MinBuildingClearance, OverlapsPrevious,
		NearestPreviousCm, OverlapsAny, NearestAnyCm, bInExitLane ? 1 : 0, BodiesInExitLanes, NearestBodyToExitCm, MaxYawOffset, MovedCm,
		FVector::Dist2D(Centroid, Exit.GetLocation()), ExitFacesOut, Job.QAMaxBodyJumpCm, Job.QAMaxAirborneCm,
		bIsFogControl ? 1 : 0, *ActiveCommand));
}

void UEnemyBuildingTrainerTestSubsystem::CaptureExitView(const FEnemyBuildingTrainerTestCase& Case,
                                                         const FString& Suffix)
{
	using namespace EnemyBuildingTrainerTestPrivate;
	AActor* const Building = Case.Building.Get();
	const UEnemyBuildingTrainerComponent* const Trainer = Case.Trainer.Get();
	if (not IsValid(Building) || not IsValid(Trainer) || not EnsureCaptureActor())
	{
		return;
	}
	FBox VisibleBox(ForceInit);
	TInlineComponentArray<UMeshComponent*> Meshes(Building);
	for (const UMeshComponent* Mesh : Meshes)
	{
		if (IsValid(Mesh) && Mesh->IsVisible() && not Mesh->bHiddenInGame
			&& Mesh->Bounds.GetBox().GetExtent().GetMax() <= MaxFramedComponentExtentCm)
		{
			VisibleBox += Mesh->Bounds.GetBox();
		}
	}
	if (not VisibleBox.IsValid)
	{
		VisibleBox = FBox::BuildAABB(Building->GetActorLocation(), FVector(1000.f));
	}
	const float Radius = FMath::Max(FMath::Max(VisibleBox.GetExtent().X, VisibleBox.GetExtent().Y), 800.f);
	const FTransform Exit = Trainer->GetExitTransform();
	const FVector ExitForward = Exit.GetRotation().GetForwardVector().GetSafeNormal2D();
	FVector Target = Exit.GetLocation() - ExitForward * Radius * SpawnQAViewTargetTowardBuildingMlt;
	float GroundZ = Target.Z;
	if (TryGetGroundZ(Target, GroundZ))
	{
		Target.Z = GroundZ + 150.f;
	}
	const FVector CameraDirection = ExitForward.RotateAngleAxis(SpawnQAViewYawOffset, FVector::UpVector);
	const FRotator LookRotation(SpawnQAViewPitch, (-CameraDirection).Rotation().Yaw, 0.f);
	const float Distance = FMath::Clamp(Radius * SpawnQAViewDistanceRadiusMlt, SpawnQAViewMinDistanceCm,
	                                    SpawnQAViewMaxDistanceCm);
	const FVector ViewLocation = Target - LookRotation.Vector() * Distance;

	// The fog of war hides the AI's building and units; show them for this frame only.
	TArray<AActor*> Shown;
	TArray<AActor*> Bodies;
	for (const FEnemyBuildingTrainerTestJob& Job : Case.Jobs)
	{
		CollectUnitBodies(Job.Unit.Get(), Bodies);
	}
	Bodies.Add(Building);
	for (AActor* const Body : Bodies)
	{
		if (Body->IsHidden())
		{
			Body->SetActorHiddenInGame(false);
			if (Body != Building)
			{
				Shown.Add(Body);
			}
		}
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(Body);
		for (USkeletalMeshComponent* const SkeletalMesh : SkeletalMeshes)
		{
			if (IsValid(SkeletalMesh) && SkeletalMesh->IsVisible())
			{
				SkeletalMesh->TickAnimation(0.f, false);
				SkeletalMesh->RefreshBoneTransforms();
			}
		}
	}
	M_CaptureActor->SetActorLocationAndRotation(ViewLocation, LookRotation);
	M_CaptureActor->GetCaptureComponent2D()->CaptureScene();
	for (AActor* const Body : Shown)
	{
		Body->SetActorHiddenInGame(true);
	}
	FImage Image;
	if (not FImageUtils::GetRenderTargetImage(M_CaptureTarget, Image))
	{
		Log(FString::Printf(TEXT("RTS_EBT capture %s %s failed"), *Case.Label, *Suffix));
		return;
	}
	const FString Path = FPaths::Combine(M_OutputDirectory, TEXT("SpawnQA_") + Case.Label + TEXT("_") + Suffix + TEXT(".png"));
	FImageUtils::SaveImageByExtension(*Path, Image);
	Log(FString::Printf(TEXT("RTS_EBT capture %s"), *Path));
}

bool UEnemyBuildingTrainerTestSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace EnemyBuildingTrainerTestPrivate;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBuildingTrainerTestGround), false);
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

float UEnemyBuildingTrainerTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UEnemyBuildingTrainerTestSubsystem::SetStep(const EEnemyBuildingTrainerTestStep NewStep)
{
	M_Step = NewStep;
	M_StepStartGameSeconds = GetGameSeconds();
	Log(FString::Printf(TEXT("RTS_EBT step %d t=%.2f"), static_cast<int32>(NewStep), M_StepStartGameSeconds));
}

void UEnemyBuildingTrainerTestSubsystem::Log(const FString& Line)
{
	UE_LOG(LogRTSEnemyBuildingTrainerTest, Display, TEXT("%s"), *Line);
	M_Lines.Add(Line);
}

void UEnemyBuildingTrainerTestSubsystem::Finish()
{
	M_Step = EEnemyBuildingTrainerTestStep::Done;
	if (UEnemyStrategicAIComponent* const StrategicAI = M_StrategicAI.Get())
	{
		StrategicAI->MapTest_SetTrainingThinkSuspended(false);
	}
	TArray<FString> Reasons = M_GlobalFailures;
	FString Summary = TEXT("# Enemy building trainer test\n\n| Building | Component | Tagged | Jobs | Result |\n|---|---|---|---|---|\n");
	for (const FEnemyBuildingTrainerTestCase& Case : M_Cases)
	{
		const FString CaseResult = Case.Failures.IsEmpty() ? TEXT("PASS") : FString::Join(Case.Failures, TEXT(", "));
		Summary += FString::Printf(TEXT("| %s | %s | %d | %d | %s |\n"), *Case.Label,
		                           Case.bComponentAddedByTest ? TEXT("runtime (test)") : TEXT("blueprint"),
		                           Case.TaggedActivityComponents, Case.Jobs.Num(), *CaseResult);
		Log(FString::Printf(TEXT("RTS_EBT case %s component=%s tagged=%d result=%s"), *Case.Label,
		                    Case.bComponentAddedByTest ? TEXT("runtime") : TEXT("blueprint"),
		                    Case.TaggedActivityComponents, *CaseResult));
		for (const FString& Failure : Case.Failures)
		{
			Reasons.Add(Case.Label + TEXT(":") + Failure);
		}
	}
	Summary += TEXT("\nGlobal: ") + (M_GlobalFailures.IsEmpty() ? FString(TEXT("none")) : FString::Join(M_GlobalFailures, TEXT(", ")));
	Summary += TEXT("\n\n```\n") + FString::Join(M_Lines, TEXT("\n")) + TEXT("\n```\n");
	FFileHelper::SaveStringToFile(Summary, *FPaths::Combine(M_OutputDirectory, TEXT("summary.md")));
	const bool bPassed = Reasons.IsEmpty();
	UE_LOG(LogRTSEnemyBuildingTrainerTest, Display, TEXT("RTS_EBT RESULT %s reasons=%s dir=%s"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), bPassed ? TEXT("none") : *FString::Join(Reasons, TEXT(";")),
	       *M_OutputDirectory);
	FPlatformMisc::RequestExit(false, TEXT("EnemyBuildingTrainerTest"));
}

#endif
