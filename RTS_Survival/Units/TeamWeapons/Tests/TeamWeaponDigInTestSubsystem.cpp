// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TeamWeaponDigInTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Units/TeamWeapons/DigIn/TeamWeaponDigInComponent.h"
#include "RTS_Survival/Units/TeamWeapons/DigIn/TeamWeaponDigInWall.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTeamWeaponDigIn, Log, All);

namespace TeamWeaponDigInTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("TeamWeaponDigInValidate");
	const TCHAR* const SurveySwitch = TEXT("TWDigInSurvey");
	const TCHAR* const DirectorySwitch = TEXT("TWDigInDir=");
	const TCHAR* const LocationSwitch = TEXT("TWDigInAt=");
	const TCHAR* const OnlySwitch = TEXT("TWDigInOnly=");
	// With -TWDigInSurvey: also the faction copies (*_Soviet), to check every squad Blueprint's setup.
	const TCHAR* const AllBlueprintsSwitch = TEXT("TWDigInAllBlueprints");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const TeamWeaponFolder = TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/");

	// One squad Blueprint per team weapon subtype; the *_Soviet copies only differ in their owner.
	const TCHAR* const SquadBlueprints[] = {
		TEXT("Pak38_50mm/Ger_SquadTW_Pak38"),
		TEXT("Pak40_75MM/Ger_SquadTW_Pak40_75MM"),
		TEXT("Flak37mm/Ger_SquadTW_Flak37MM"),
		TEXT("Flak88Wheels/Ger_SquadTW_Flak88"),
		TEXT("WheeledMortar/Ger_SquadTW_80mmMortar"),
		TEXT("LefhHowitzer/Ger_SquadTW_Lefh"),
		TEXT("Morser210MM/Ger_SquadTW_Morser210"),
		TEXT("Soviet/53K_45MM/Rus_TWSquad_53K_45mm"),
		TEXT("Soviet/Zis_76MM/Rus_TWSquad_Zis3_76"),
		TEXT("Soviet/BS3_100MM/Rus_TWSquad_BS3_100mm"),
		TEXT("Soviet/MaximMG/Rus_TWSquad_Maxim"),
		TEXT("Soviet/80MMMortar/Rus_SquadTW_80mmMortar"),
		TEXT("Soviet/120Mortar/Rus_SquadTW_120mmMortar"),
		TEXT("Soviet/SovietHowitzer/Rus_SquadTW_M1938"),
		TEXT("Soviet/ML20_152MM/Rus_SquadTW_ML20"),
		TEXT("Soviet/B4_203MM/Rus_SquadTW_B4"),
	};
	const TCHAR* const FactionCopyBlueprints[] = {
		TEXT("Soviet/53K_45MM/Rus_TWSquad_53K_45mm_Soviet"),
		TEXT("Soviet/Zis_76MM/Rus_TWSquad_Zis3_76_Soviet"),
		TEXT("Soviet/MaximMG/Rus_TWSquad_Maxim_Soviet"),
		TEXT("Soviet/80MMMortar/Rus_SquadTW_80mmMortar_Soviet"),
		TEXT("Soviet/120Mortar/Rus_SquadTW_120mmMortar_Soviet"),
		TEXT("Soviet/SovietHowitzer/Rus_SquadTW_M1938_Soviet"),
		TEXT("Soviet/B4_203MM/Rus_SquadTW_B4_Soviet"),
	};

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartRealSeconds = 90.0;
	constexpr double MaxRunRealSeconds = 2400.0;
	constexpr double GameUnlockRetrySeconds = 2.0;
	constexpr float MaxRunGameSeconds = 1200.f;
	constexpr float MaxSpawnGameSeconds = 45.f;
	constexpr float MaxDeployGameSeconds = 60.f;
	constexpr float SettleGameSeconds = 10.f;
	// Build time plus loading; the pit must stand by then.
	constexpr float MaxDigInGameSeconds = 45.f;
	// Guards walk round to the rear entrance and into the pit.
	constexpr float DugInSettleGameSeconds = 15.f;
	constexpr float MaxBuildingWaitGameSeconds = 20.f;
	constexpr float MaxNavBuildWaitGameSeconds = 600.f;
	constexpr float MaxNavMeshRestoreGameSeconds = 300.f;
	constexpr float MaxDriveOutGameSeconds = 60.f;
	// The drive-out order goes this far ahead, through where the front wall stood.
	constexpr float DriveOutDistanceCm = 900.f;
	constexpr float MinDriveOutProgressCm = 400.f;
	const TCHAR* const CompareMeshSwitch = TEXT("TWDigInCompareMesh=");
	constexpr float ComparisonMeshSideOffsetCm = 1500.f;
	// The cancel test breaks cover once this much of the wall has risen.
	constexpr float CancelAtBuildProgress = 0.2f;
	constexpr float TestHitDamage = 200.f;
	constexpr float HealthTolerance = 0.01f;
	constexpr float HitDropToleranceHealth = 0.5f;
	constexpr float MaxOperatorPostDistanceCm = 80.f;
	constexpr float MaxGuardPostDistanceCm = 110.f;
	constexpr float NavBoxProbeExtentCm = 20.f;
	// A path into the pit from in front must go round to the entrance: this much longer than the straight line.
	constexpr float MinDetourFactor = 1.2f;
	constexpr float MaxPathDistanceToEntranceCm = 250.f;
	// The pit's footing reaches 30 cm into the ground; above the ground any gap shows.
	constexpr float MaxWallFloatCm = 12.f;
	// Slit pits sink less into uneven ground so the slit stays at the barrel; a little more gap is allowed.
	constexpr float MaxSlitWallFloatCm = 20.f;
	// The points just outside the pit (in front, behind the entrance) must be on the navmesh within this.
	constexpr float NavMeshAroundGunExtentCm = 60.f;
	constexpr float NavMeshAroundGunHeightCm = 400.f;
	constexpr float GroundProbeRaiseCm = 500.f;
	// Scenery between a probe and the pit is stepped through; the probe tests the pit, not the map.
	constexpr int32 MaxTraceStepsThroughScenery = 4;
	constexpr float MinPitPictureDistanceScale = 1.f;
	constexpr float PitPictureExtentPerScaleCm = 450.f;
	constexpr float SubjectSpacingCm = 2000.f;
	constexpr float NavProjectionExtentCm = 600.f;
	constexpr float GroundTraceHalfHeightCm = 5000.f;

	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float PictureFieldOfView = 50.f;
	const FVector PictureCameraOffset(-900.f, 600.f, 900.f);
	const FVector PictureLookAtOffset(0.f, 0.f, 80.f);

	FString VectorToJson(const FVector& Vector)
	{
		return FString::Printf(TEXT("[%.1f,%.1f,%.1f]"), Vector.X, Vector.Y, Vector.Z);
	}

	FString GetBlueprintName(const FString& ClassPath)
	{
		FString Name;
		ClassPath.Split(TEXT("/"), nullptr, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		return Name;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Subsystem lifecycle

bool UTeamWeaponDigInTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TeamWeaponDigInTestPrivate::ValidateSwitch);
}

void UTeamWeaponDigInTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace TeamWeaponDigInTestPrivate;
	Super::OnWorldBeginPlay(InWorld);
	bM_IsSurvey = FParse::Param(FCommandLine::Get(), SurveySwitch);
	bM_TakePictures = FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	// Error dialogs stall unattended runs; they still reach the log.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	bM_IsActive = true;
	M_StartRealSeconds = FPlatformTime::Seconds();
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG started mode=%s pictures=%s"),
	       bM_IsSurvey ? TEXT("survey") : TEXT("validate"), bM_TakePictures ? *M_OutputDirectory : TEXT("off"));
}

void UTeamWeaponDigInTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

TStatId UTeamWeaponDigInTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTeamWeaponDigInTestSubsystem, STATGROUP_Tickables);
}

float UTeamWeaponDigInTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UTeamWeaponDigInTestSubsystem::Check(const bool bCondition, const FString& Subject, const TCHAR* Description,
                                          const FString& Details)
{
	(bCondition ? M_PassedCheckCount : M_FailedCheckCount)++;
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG check=%s subject=%s %s %s (t=%.1f)"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Subject, Description, *Details, GetGameSeconds());
}

void UTeamWeaponDigInTestSubsystem::Finish(const TCHAR* Reason)
{
	if (bM_IsFinished)
	{
		return;
	}
	bM_IsFinished = true;
	if (bM_IsSurvey)
	{
		WriteSurveyFile();
	}
	UE_LOG(LogRTSTeamWeaponDigIn, Display,
	       TEXT("RTS_TWDIG RESULT %s passed=%d failed=%d reason=%s game_t=%.1f real_s=%.0f"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount, Reason,
	       GetGameSeconds(), FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("TeamWeaponDigInTest"));
}

void UTeamWeaponDigInTestSubsystem::Tick(float DeltaTime)
{
	using namespace TeamWeaponDigInTestPrivate;
	if (not bM_IsActive || bM_IsFinished || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (not bM_HasStartedGame)
	{
		if (RealSeconds > MaxStartRealSeconds)
		{
			Check(false, TEXT("run"), TEXT("game_started"), TEXT(""));
			Finish(TEXT("no_start"));
			return;
		}
		if (RealSeconds < HudBuildSeconds || not TryStartGame() || not TryFindTestCentre())
		{
			return;
		}
		bM_HasStartedGame = true;
		if (bM_TakePictures && not CreateCaptureRig())
		{
			bM_TakePictures = false;
			UE_LOG(LogRTSTeamWeaponDigIn, Warning, TEXT("RTS_TWDIG capture rig could not be created"));
		}
		PlanAndSpawnSubjects();
		return;
	}
	KeepGameRunning();
	if (RealSeconds > MaxRunRealSeconds || GetGameSeconds() - M_SubjectsSpawnedGameSeconds > MaxRunGameSeconds)
	{
		Check(false, TEXT("run"), TEXT("finished_in_time"), TEXT(""));
		Finish(TEXT("timeout"));
		return;
	}
	bool bAllDone = true;
	for (FTWDigInTestSubject& Subject : M_Subjects)
	{
		TickSubject(Subject);
		bAllDone &= Subject.Step == ETWDigInTestStep::Done;
	}
	if (bAllDone)
	{
		Finish(TEXT("all_subjects_done"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Start up

bool UTeamWeaponDigInTestSubsystem::TryStartGame()
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
	KeepGameRunning();
	return true;
}

void UTeamWeaponDigInTestSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(TeamWeaponDigInTestPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* PlayerController = M_PlayerController.Get();
	if (not IsValid(PlayerController) || not PlayerController->GetIsGameOnPause())
	{
		return;
	}
	PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	const double RealSeconds = FPlatformTime::Seconds();
	if (PlayerController->GetIsGameOnPause() && RealSeconds >= M_NextGameUnlockRealSeconds)
	{
		M_NextGameUnlockRealSeconds = RealSeconds + TeamWeaponDigInTestPrivate::GameUnlockRetrySeconds;
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UTeamWeaponDigInTestSubsystem::TryFindTestCentre()
{
	FString LocationText;
	if (not FParse::Value(FCommandLine::Get(), TeamWeaponDigInTestPrivate::LocationSwitch, LocationText, false))
	{
		Check(false, TEXT("run"), TEXT("test_centre_given"), TEXT("pass -TWDigInAt=X,Y"));
		Finish(TEXT("no_centre"));
		return false;
	}
	FString XText;
	FString YText;
	LocationText.Split(TEXT(","), &XText, &YText);
	M_TestCentre = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	float GroundZ = 0.f;
	if (TryGetGroundZ(M_TestCentre, GroundZ))
	{
		M_TestCentre.Z = GroundZ;
	}
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG centre %s"), *M_TestCentre.ToCompactString());
	return true;
}

void UTeamWeaponDigInTestSubsystem::PlanAndSpawnSubjects()
{
	using namespace TeamWeaponDigInTestPrivate;
	M_SubjectsSpawnedGameSeconds = GetGameSeconds();
	FString OnlyFilter;
	FParse::Value(FCommandLine::Get(), OnlySwitch, OnlyFilter);
	int32 SlotIndex = 0;
	TArray<const TCHAR*> BlueprintPaths(SquadBlueprints, UE_ARRAY_COUNT(SquadBlueprints));
	if (FParse::Param(FCommandLine::Get(), AllBlueprintsSwitch))
	{
		BlueprintPaths.Append(FactionCopyBlueprints, UE_ARRAY_COUNT(FactionCopyBlueprints));
	}
	const int32 SubjectsPerRow = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(BlueprintPaths.Num())));
	for (const TCHAR* const BlueprintPath : BlueprintPaths)
	{
		FTWDigInTestSubject Subject;
		Subject.ClassPath = FString(TeamWeaponFolder) + BlueprintPath;
		Subject.Name = GetBlueprintName(Subject.ClassPath);
		if (not OnlyFilter.IsEmpty() && not Subject.Name.Contains(OnlyFilter))
		{
			continue;
		}
		const int32 Row = SlotIndex / SubjectsPerRow;
		const int32 Column = SlotIndex % SubjectsPerRow;
		++SlotIndex;
		const FVector GridOffset((Column - (SubjectsPerRow - 1) * 0.5f) * SubjectSpacingCm,
		                         (Row - (SubjectsPerRow - 1) * 0.5f) * SubjectSpacingCm, 0.f);
		Subject.SpawnLocation = M_TestCentre + GridOffset;
		if (not TryProjectToNavMesh(Subject.SpawnLocation))
		{
			Check(false, Subject.Name, TEXT("spawn_on_navmesh"), Subject.SpawnLocation.ToCompactString());
			continue;
		}
		Subject.StepStartGameSeconds = GetGameSeconds();
		M_Subjects.Add(Subject);
	}
	for (FTWDigInTestSubject& Subject : M_Subjects)
	{
		if (not SpawnSubject(Subject))
		{
			Check(false, Subject.Name, TEXT("spawned"), Subject.ClassPath);
			Subject.Step = ETWDigInTestStep::Done;
		}
	}
	bM_HasSpawnedSubjects = true;
}

bool UTeamWeaponDigInTestSubsystem::SpawnSubject(FTWDigInTestSubject& Subject) const
{
	const FString GeneratedClassPath = Subject.ClassPath + TEXT(".") + TeamWeaponDigInTestPrivate::GetBlueprintName(
		Subject.ClassPath) + TEXT("_C");
	UClass* SquadClass = LoadClass<ATeamWeaponController>(nullptr, *GeneratedClassPath);
	if (not IsValid(SquadClass))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	// Facing +X so the world box of the gun is its box in its own frame.
	ATeamWeaponController* Controller = GetWorld()->SpawnActor<ATeamWeaponController>(
		SquadClass, Subject.SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Controller))
	{
		return false;
	}
	// Same as the async spawner does for trained units.
	if (IRTSUnit* RTSUnit = Cast<IRTSUnit>(Controller))
	{
		RTSUnit->OnRTSUnitSpawned(true);
		RTSUnit->OnRTSUnitSpawned(false, 1.f, Subject.SpawnLocation);
	}
	Subject.Controller = Controller;
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG spawned subject=%s at=%s"), *Subject.Name,
	       *Subject.SpawnLocation.ToCompactString());
	return true;
}

bool UTeamWeaponDigInTestSubsystem::TryProjectToNavMesh(FVector& InOutLocation) const
{
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not IsValid(NavigationSystem))
	{
		return false;
	}
	FNavLocation Projected;
	const FVector Extent(TeamWeaponDigInTestPrivate::NavProjectionExtentCm, TeamWeaponDigInTestPrivate::NavProjectionExtentCm,
	                     TeamWeaponDigInTestPrivate::GroundTraceHalfHeightCm);
	if (not NavigationSystem->ProjectPointToNavigation(InOutLocation, Projected, Extent))
	{
		return false;
	}
	InOutLocation = Projected.Location;
	return true;
}

bool UTeamWeaponDigInTestSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	const FVector Start = Location + FVector(0.f, 0.f, TeamWeaponDigInTestPrivate::GroundTraceHalfHeightCm);
	const FVector End = Location - FVector(0.f, 0.f, TeamWeaponDigInTestPrivate::GroundTraceHalfHeightCm);
	FHitResult Hit;
	if (not GetWorld()->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE))
	{
		return false;
	}
	OutGroundZ = Hit.ImpactPoint.Z;
	return true;
}

void UTeamWeaponDigInTestSubsystem::KeepUnitsUnthrottled(AActor* Unit) const
{
	if (not IsValid(Unit))
	{
		return;
	}
	TInlineComponentArray<URTSOptimizer*> Optimizers(Unit);
	for (URTSOptimizer* Optimizer : Optimizers)
	{
		if (M_DisabledOptimizers.Contains(Optimizer))
		{
			continue;
		}
		Optimizer->SetOptimizationEnabled(false);
		M_DisabledOptimizers.Add(Optimizer);
	}
	// Mission maps hold enemies; a subject that dies measures nothing. Test hits are measured within one frame.
	TInlineComponentArray<UHealthComponent*> HealthComponents(Unit);
	for (UHealthComponent* HealthComponent : HealthComponents)
	{
		HealthComponent->SetCurrentHealth(HealthComponent->GetMaxHealth());
	}
	TInlineComponentArray<UPawnMovementComponent*> MovementComponents(Unit);
	for (UPawnMovementComponent* MovementComponent : MovementComponents)
	{
		MovementComponent->SetComponentTickInterval(0.f);
	}
	TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(Unit);
	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
	{
		SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		SkeletalMesh->SetComponentTickInterval(0.f);
	}
}

void UTeamWeaponDigInTestSubsystem::KeepSquadUnthrottled(ATeamWeaponController& Controller) const
{
	for (ASquadUnit* SquadUnit : Controller.GetSquadUnitsChecked())
	{
		KeepUnitsUnthrottled(SquadUnit);
	}
	ATeamWeapon* Weapon = Controller.GetControlledTeamWeaponNoReport();
	KeepUnitsUnthrottled(Weapon);
	// German and Soviet subjects are enemies by their Blueprint owner; engaging each other would turn the guns.
	if (IsValid(Weapon))
	{
		Weapon->DisableTurret();
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Steps

void UTeamWeaponDigInTestSubsystem::EnterStep(FTWDigInTestSubject& Subject, const ETWDigInTestStep NewStep)
{
	Subject.Step = NewStep;
	Subject.StepStartGameSeconds = GetGameSeconds();
}

void UTeamWeaponDigInTestSubsystem::TickSubject(FTWDigInTestSubject& Subject)
{
	using namespace TeamWeaponDigInTestPrivate;
	if (Subject.Step == ETWDigInTestStep::Done)
	{
		return;
	}
	const float StepSeconds = GetGameSeconds() - Subject.StepStartGameSeconds;
	ATeamWeaponController* Controller = Subject.Controller.Get();
	ATeamWeapon* Weapon = IsValid(Controller) ? Controller->GetControlledTeamWeaponNoReport() : nullptr;
	if (not IsValid(Controller) || not IsValid(Weapon))
	{
		if (StepSeconds > MaxSpawnGameSeconds)
		{
			Check(false, Subject.Name, TEXT("spawned_with_weapon"), TEXT(""));
			EnterStep(Subject, ETWDigInTestStep::Done);
		}
		return;
	}
	KeepSquadUnthrottled(*Controller);
	switch (Subject.Step)
	{
	case ETWDigInTestStep::WaitingForSpawn:
		EnterStep(Subject, ETWDigInTestStep::WaitingForDeploy);
		break;
	case ETWDigInTestStep::WaitingForDeploy:
		if (Controller->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed)
		{
			EnterStep(Subject, ETWDigInTestStep::Settling);
		}
		else if (StepSeconds > MaxDeployGameSeconds)
		{
			Check(false, Subject.Name, TEXT("deployed"), TEXT(""));
			EnterStep(Subject, ETWDigInTestStep::Done);
		}
		break;
	case ETWDigInTestStep::Settling:
		if (StepSeconds > SettleGameSeconds)
		{
			TickSettled(Subject, *Controller, *Weapon);
		}
		break;
	default:
		TickDigInSteps(Subject, *Controller, *Weapon, StepSeconds);
		break;
	}
}

void UTeamWeaponDigInTestSubsystem::TickSettled(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
                                                ATeamWeapon& Weapon)
{
	if (bM_IsSurvey)
	{
		SurveySubject(Subject, Controller, Weapon);
		if (bM_TakePictures)
		{
			CapturePicture(Weapon, Subject.Name + TEXT("_deployed"));
		}
		EnterStep(Subject, ETWDigInTestStep::Done);
		return;
	}
	StartDigInRound(Subject, Controller, Weapon);
}

// ---------------------------------------------------------------------------------------------------------------
// Dig-in

float UTeamWeaponDigInTestSubsystem::TakeTestHit(AActor& HitActor, UHealthComponent& Health)
{
	const float HealthBefore = Health.GetCurrentHealth();
	HitActor.TakeDamage(TeamWeaponDigInTestPrivate::TestHitDamage, FDamageEvent(), nullptr, nullptr);
	const float HealthDrop = HealthBefore - Health.GetCurrentHealth();
	// The test only measures; the gun keeps its health.
	Health.SetCurrentHealth(HealthBefore);
	return HealthDrop;
}

bool UTeamWeaponDigInTestSubsystem::GetIsPointOnNavMesh(const FVector& Location, const float RadiusCm,
                                                       const float HeightCm) const
{
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	return IsValid(NavigationSystem) &&
		NavigationSystem->ProjectPointToNavigation(Location, Projected, FVector(RadiusCm, RadiusCm, HeightCm));
}

bool UTeamWeaponDigInTestSubsystem::GetIsPitCarvedIntoNavMesh(const ATeamWeaponDigInWall& Wall) const
{
	using namespace TeamWeaponDigInTestPrivate;
	const UStaticMeshSocket* FrontWallBox = Wall.GetWallMeshComponent()->GetStaticMesh()->FindSocket(TEXT("NavBox_0"));
	if (not IsValid(FrontWallBox))
	{
		return true;
	}
	const FVector FrontWallCentre = Wall.GetActorTransform().TransformPosition(FrontWallBox->RelativeLocation);
	return not GetIsPointOnNavMesh(FrontWallCentre, NavBoxProbeExtentCm, FrontWallBox->RelativeLocation.Z * 2.f);
}

bool UTeamWeaponDigInTestSubsystem::GetHasNavMeshAroundPit(const ATeamWeaponDigInWall& Wall) const
{
	using namespace TeamWeaponDigInTestPrivate;
	FVector Outside;
	FVector Entrance;
	if (not Wall.TryGetBuiltSocketLocation(TEXT("ProbeWall"), Outside) ||
		not Wall.TryGetBuiltSocketLocation(TEXT("Entrance"), Entrance))
	{
		return false;
	}
	return GetIsPointOnNavMesh(Outside, NavMeshAroundGunExtentCm, NavMeshAroundGunHeightCm) &&
		GetIsPointOnNavMesh(Entrance, NavMeshAroundGunExtentCm, NavMeshAroundGunHeightCm);
}

void UTeamWeaponDigInTestSubsystem::StartDigInRound(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
                                                    ATeamWeapon& Weapon)
{
	UHealthComponent* Health = Weapon.FindComponentByClass<UHealthComponent>();
	UTeamWeaponDigInComponent* DigIn = Weapon.GetDigInComponent();
	Check(IsValid(Health) && IsValid(DigIn), Subject.Name, TEXT("has_health_and_dig_in"), TEXT(""));
	if (not IsValid(Health) || not IsValid(DigIn))
	{
		EnterStep(Subject, ETWDigInTestStep::Done);
		return;
	}
	Subject.BaseMaxHealth = Health->GetMaxHealth();
	Subject.BaseDamageMultiplier = Health->GetDamageReductionMultiplier();
	Subject.BaselineHitDrop = TakeTestHit(Weapon, *Health);
	Subject.CardBeforeDigIn = Controller.GetUnitAbilities();

	UE_LOG(LogRTSTeamWeaponDigIn, Display,
	       TEXT("RTS_TWDIG base subject=%s gun=%s max_health=%.1f damage_mlt=%.3f hit_drop=%.1f"), *Subject.Name,
	       *Weapon.GetClass()->GetName(), Subject.BaseMaxHealth, Subject.BaseDamageMultiplier, Subject.BaselineHitDrop);
	const ECommandQueueError OrderError = Controller.DigIn(true);
	Check(OrderError == ECommandQueueError::NoError, Subject.Name, TEXT("dig_in_order_accepted"),
	      UEnum::GetValueAsString(OrderError));
	EnterStep(Subject, ETWDigInTestStep::WaitingForDigIn);
}

void UTeamWeaponDigInTestSubsystem::TickDigInSteps(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
                                                   ATeamWeapon& Weapon, const float StepSeconds)
{
	UTeamWeaponDigInComponent* DigIn = Weapon.GetDigInComponent();
	if (not IsValid(DigIn))
	{
		EnterStep(Subject, ETWDigInTestStep::Done);
		return;
	}
	switch (Subject.Step)
	{
	case ETWDigInTestStep::WaitingForDigIn:
		TickWaitingForDigIn(Subject, Controller, *DigIn, StepSeconds);
		break;
	case ETWDigInTestStep::SettlingDugIn:
		TickSettlingDugIn(Subject, Controller, Weapon, *DigIn, StepSeconds);
		break;
	case ETWDigInTestStep::CheckingBrokenCover:
		CheckDigInRemoved(Subject, Controller, Weapon, TEXT("break_cover"));
		// Second round: cancel while the sandbags are still going up.
		Check(Controller.DigIn(true) == ECommandQueueError::NoError, Subject.Name, TEXT("second_dig_in_order_accepted"),
		      TEXT(""));
		EnterStep(Subject, ETWDigInTestStep::WaitingForBuilding);
		break;
	case ETWDigInTestStep::WaitingForBuilding:
		TickWaitingForBuilding(Subject, Controller, *DigIn, StepSeconds);
		break;
	case ETWDigInTestStep::CheckingCancel:
		CheckDigInRemoved(Subject, Controller, Weapon, TEXT("cancel"));
		EnterStep(Subject, Subject.bHasNavMeshAroundGun ? ETWDigInTestStep::WaitingForNavMeshRestore
			                   : ETWDigInTestStep::Done);
		break;
	case ETWDigInTestStep::WaitingForNavMeshRestore:
		TickWaitingForNavMeshRestore(Subject, Controller, Weapon, StepSeconds);
		break;
	case ETWDigInTestStep::DrivingOutOfPit:
		TickDrivingOutOfPit(Subject, Weapon, StepSeconds);
		break;
	default:
		break;
	}
}

void UTeamWeaponDigInTestSubsystem::TickWaitingForDigIn(FTWDigInTestSubject& Subject,
                                                        ATeamWeaponController& Controller,
                                                        const UTeamWeaponDigInComponent& DigIn, const float StepSeconds)
{
	if (DigIn.GetDigInStatus() == ETeamWeaponDigInStatus::Building && not Subject.bHasCheckedBuildingCard)
	{
		Subject.bHasCheckedBuildingCard = true;
		CheckDugInCard(Subject, Controller, TEXT("building"));
	}
	if (DigIn.GetDigInStatus() == ETeamWeaponDigInStatus::DugIn)
	{
		Check(true, Subject.Name, TEXT("dug_in"), FString::Printf(TEXT("after %.1f s"), StepSeconds));
		EnterStep(Subject, ETWDigInTestStep::SettlingDugIn);
		return;
	}
	if (StepSeconds > TeamWeaponDigInTestPrivate::MaxDigInGameSeconds)
	{
		Check(false, Subject.Name, TEXT("dug_in"), UEnum::GetValueAsString(DigIn.GetDigInStatus()));
		EnterStep(Subject, ETWDigInTestStep::Done);
	}
}

void UTeamWeaponDigInTestSubsystem::TickSettlingDugIn(FTWDigInTestSubject& Subject, ATeamWeaponController& Controller,
                                                      ATeamWeapon& Weapon, UTeamWeaponDigInComponent& DigIn,
                                                      const float StepSeconds)
{
	using namespace TeamWeaponDigInTestPrivate;
	if (StepSeconds < DugInSettleGameSeconds)
	{
		return;
	}
	ATeamWeaponDigInWall* Wall = DigIn.GetDigInWall();
	// A large map rebuilds its navmesh for minutes after loading; the pit's modifiers wait behind that build.
	if (IsValid(Wall) && not GetIsPitCarvedIntoNavMesh(*Wall) &&
		StepSeconds < DugInSettleGameSeconds + MaxNavBuildWaitGameSeconds)
	{
		return;
	}
	UHealthComponent* Health = Weapon.FindComponentByClass<UHealthComponent>();
	Check(IsValid(Wall) && Wall->GetIsBuilt(), Subject.Name, TEXT("wall_built"), TEXT(""));
	if (not IsValid(Wall) || not IsValid(Health))
	{
		EnterStep(Subject, ETWDigInTestStep::Done);
		return;
	}
	CheckDugInCard(Subject, Controller, TEXT("dug_in"));
	Check(Controller.GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed, Subject.Name, TEXT("deployed_in_pit"),
	      UEnum::GetValueAsString(Controller.GetTeamWeaponState()));
	CheckProtection(Subject, *Health);
	CheckHitTransfer(Subject, Weapon, *Wall, *Health);
	CheckWallTraces(Subject, Weapon, *Wall);
	Subject.bHasNavMeshAroundGun = GetHasNavMeshAroundPit(*Wall);
	if (const UStaticMeshSocket* FrontWallBox = Wall->GetWallMeshComponent()->GetStaticMesh()->FindSocket(TEXT("NavBox_0")))
	{
		Subject.FormerFrontWallCentre = Wall->GetActorTransform().TransformPosition(FrontWallBox->RelativeLocation);
		Subject.FormerFrontWallHalfHeightCm = FrontWallBox->RelativeLocation.Z;
	}
	CaptureComparisonMesh(Subject, Weapon);
	if (Subject.bHasNavMeshAroundGun)
	{
		CheckNavigation(Subject, *Wall);
		CheckCrewInsidePit(Subject, Controller, DigIn);
	}
	else
	{
		UE_LOG(LogRTSTeamWeaponDigIn, Display,
		       TEXT("RTS_TWDIG skip_nav_checks subject=%s reason=no navmesh just outside the pit (map edge)"),
		       *Subject.Name);
	}
	CheckWallOnGround(Subject, Weapon, *Wall);
	if (bM_TakePictures)
	{
		const float PitExtentCm = Wall->GetWallMeshComponent()->Bounds.BoxExtent.Size2D();
		CapturePicture(Weapon, Subject.Name + TEXT("_dug_in"),
		               FMath::Max(MinPitPictureDistanceScale, PitExtentCm / PitPictureExtentPerScaleCm));
	}
	Check(Controller.BreakCover(true) == ECommandQueueError::NoError, Subject.Name, TEXT("break_cover_order_accepted"),
	      TEXT(""));
	EnterStep(Subject, ETWDigInTestStep::CheckingBrokenCover);
}

void UTeamWeaponDigInTestSubsystem::TickWaitingForBuilding(FTWDigInTestSubject& Subject,
                                                           ATeamWeaponController& Controller,
                                                           const UTeamWeaponDigInComponent& DigIn,
                                                           const float StepSeconds)
{
	using namespace TeamWeaponDigInTestPrivate;
	const ATeamWeaponDigInWall* Wall = DigIn.GetDigInWall();
	if (IsValid(Wall) && DigIn.GetDigInStatus() == ETeamWeaponDigInStatus::Building &&
		Wall->GetBuildProgress() >= CancelAtBuildProgress)
	{
		Check(Controller.BreakCover(true) == ECommandQueueError::NoError, Subject.Name, TEXT("cancel_order_accepted"),
		      FString::Printf(TEXT("progress=%.2f"), Wall->GetBuildProgress()));
		EnterStep(Subject, ETWDigInTestStep::CheckingCancel);
		return;
	}
	if (StepSeconds > MaxBuildingWaitGameSeconds)
	{
		Check(false, Subject.Name, TEXT("second_dig_in_building"), UEnum::GetValueAsString(DigIn.GetDigInStatus()));
		EnterStep(Subject, ETWDigInTestStep::Done);
	}
}

void UTeamWeaponDigInTestSubsystem::CheckDugInCard(const FTWDigInTestSubject& Subject,
                                                   ATeamWeaponController& Controller, const TCHAR* Label)
{
	const bool bHasBreakCover = Controller.HasAbility(EAbilityID::IdBreakCover);
	const bool bHasDigIn = Controller.HasAbility(EAbilityID::IdDigIn);
	const bool bHasMove = Controller.HasAbility(EAbilityID::IdMove);
	const bool bHasRotate = Controller.HasAbility(EAbilityID::IdRotateTowards);
	Check(bHasBreakCover && not bHasDigIn && not bHasMove && not bHasRotate, Subject.Name,
	      *FString::Printf(TEXT("dug_in_card_%s"), Label),
	      FString::Printf(TEXT("break_cover=%d dig_in=%d move=%d rotate=%d"), bHasBreakCover, bHasDigIn, bHasMove,
	                      bHasRotate));
	if (FCString::Strcmp(Label, TEXT("dug_in")) != 0)
	{
		return;
	}
	// A move order must not pull the gun out of its pit.
	const FVector FarAway = Controller.GetActorLocation() + FVector(2000.f, 0.f, 0.f);
	const ECommandQueueError MoveError = Controller.MoveToLocation(FarAway, false, FRotator::ZeroRotator);
	Check(MoveError != ECommandQueueError::NoError, Subject.Name, TEXT("move_refused_while_dug_in"),
	      UEnum::GetValueAsString(MoveError));
}

void UTeamWeaponDigInTestSubsystem::CheckProtection(const FTWDigInTestSubject& Subject, const UHealthComponent& Health)
{
	using namespace TeamWeaponDigInTestPrivate;
	const float ExpectedMaxHealth = Subject.BaseMaxHealth * TeamWeaponDigInDefaults::MaxHealthMultiplier;
	const float ExpectedMultiplier = Subject.BaseDamageMultiplier * TeamWeaponDigInDefaults::DamageTakenMultiplier;
	Check(FMath::IsNearlyEqual(Health.GetMaxHealth(), ExpectedMaxHealth, ExpectedMaxHealth * HealthTolerance),
	      Subject.Name, TEXT("max_health_plus_33_percent"),
	      FString::Printf(TEXT("%.1f -> %.1f (expected %.1f)"), Subject.BaseMaxHealth, Health.GetMaxHealth(),
	                      ExpectedMaxHealth));
	Check(FMath::IsNearlyEqual(Health.GetDamageReductionMultiplier(), ExpectedMultiplier, HealthTolerance),
	      Subject.Name, TEXT("damage_multiplier_times_0_8"),
	      FString::Printf(TEXT("%.3f -> %.3f"), Subject.BaseDamageMultiplier, Health.GetDamageReductionMultiplier()));
}

void UTeamWeaponDigInTestSubsystem::CheckHitTransfer(const FTWDigInTestSubject& Subject, ATeamWeapon& Weapon,
                                                     ATeamWeaponDigInWall& Wall, UHealthComponent& Health)
{
	using namespace TeamWeaponDigInTestPrivate;
	const float WallHitDrop = TakeTestHit(Wall, Health);
	const float DirectHitDrop = TakeTestHit(Weapon, Health);
	Check(WallHitDrop > 0.f && FMath::IsNearlyEqual(WallHitDrop, DirectHitDrop, HitDropToleranceHealth), Subject.Name,
	      TEXT("sandbag_hit_hurts_gun"),
	      FString::Printf(TEXT("wall_hit=%.1f direct_hit=%.1f"), WallHitDrop, DirectHitDrop));
	// With a flat reduction F as well: dug in 0.8*m*D - F against m*D - F before.
	Check(DirectHitDrop <= Subject.BaselineHitDrop * TeamWeaponDigInDefaults::DamageTakenMultiplier +
	      HitDropToleranceHealth, Subject.Name, TEXT("dug_in_hit_reduced"),
	      FString::Printf(TEXT("before=%.1f dug_in=%.1f"), Subject.BaselineHitDrop, DirectHitDrop));
}

void UTeamWeaponDigInTestSubsystem::CheckWallTraces(const FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon,
                                                    const ATeamWeaponDigInWall& Wall)
{
	const URTSComponent* RTSComponent = Weapon.FindComponentByClass<URTSComponent>();
	// The walls block the weapons of the gun's opponent: the channel their shots trace on.
	const ECollisionChannel OpponentShots = IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == 1
		                                        ? COLLISION_TRACE_PLAYER
		                                        : COLLISION_TRACE_ENEMY;
	const UStaticMesh* WallMesh = Wall.GetWallMeshComponent()->GetStaticMesh();
	auto TraceInwardFrom = [this, &Wall, WallMesh, OpponentShots](const FName ProbeName, FHitResult& OutHit,
	                                                              bool& bOutHit)
	{
		const UStaticMeshSocket* Probe = WallMesh->FindSocket(ProbeName);
		if (not IsValid(Probe))
		{
			return false;
		}
		const FVector Start = Wall.GetActorTransform().TransformPosition(Probe->RelativeLocation);
		const FVector End = Wall.GetActorTransform().TransformPosition(
			FVector(0.f, Probe->RelativeLocation.Y, Probe->RelativeLocation.Z));
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TWDigInProbe), true);
		for (int32 Step = 0; Step < TeamWeaponDigInTestPrivate::MaxTraceStepsThroughScenery; ++Step)
		{
			bOutHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, OpponentShots, QueryParams);
			if (not bOutHit || OutHit.GetActor() == &Wall || OutHit.GetActor() == Wall.GetOwner())
			{
				break;
			}
			QueryParams.AddIgnoredActor(OutHit.GetActor());
		}
		return true;
	};
	FHitResult Hit;
	bool bHit = false;
	if (TraceInwardFrom(TEXT("ProbeWall"), Hit, bHit))
	{
		Check(bHit && Hit.GetActor() == &Wall, Subject.Name, TEXT("wall_stops_shots"),
		      bHit ? FString::Printf(TEXT("%s.%s at pit-local %s"), *GetNameSafe(Hit.GetActor()),
		                             *GetNameSafe(Hit.GetComponent()),
		                             *Wall.GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).ToCompactString())
		           : FString(TEXT("nothing hit")));
	}
	if (TraceInwardFrom(TEXT("ProbeSlit"), Hit, bHit))
	{
		Check(not bHit || Hit.GetActor() != &Wall, Subject.Name, TEXT("slit_open_at_barrel_height"),
		      bHit ? GetNameSafe(Hit.GetActor()) : TEXT("nothing hit"));
	}
}

void UTeamWeaponDigInTestSubsystem::CheckNavigation(const FTWDigInTestSubject& Subject,
                                                    const ATeamWeaponDigInWall& Wall)
{
	using namespace TeamWeaponDigInTestPrivate;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const UStaticMesh* WallMesh = Wall.GetWallMeshComponent()->GetStaticMesh();
	const UStaticMeshSocket* FrontWallBox = WallMesh->FindSocket(TEXT("NavBox_0"));
	FVector Outside;
	FVector Inside;
	FVector Entrance;
	const bool bHasSockets = IsValid(FrontWallBox) && Wall.TryGetBuiltSocketLocation(TEXT("ProbeWall"), Outside) &&
		Wall.TryGetBuiltSocketLocation(TEXT("Inside"), Inside) &&
		Wall.TryGetBuiltSocketLocation(TEXT("Entrance"), Entrance);
	if (not IsValid(NavigationSystem) || not bHasSockets)
	{
		Check(false, Subject.Name, TEXT("pit_has_nav_sockets"), TEXT(""));
		return;
	}
	FNavLocation Projected;
	const FVector FrontWallCentre = Wall.GetActorTransform().TransformPosition(FrontWallBox->RelativeLocation);
	const FVector ProbeExtent(NavBoxProbeExtentCm, NavBoxProbeExtentCm, FrontWallBox->RelativeLocation.Z * 2.f);
	const bool bWallOnNavMesh = NavigationSystem->ProjectPointToNavigation(FrontWallCentre, Projected, ProbeExtent);
	Check(not bWallOnNavMesh, Subject.Name, TEXT("wall_blocks_navmesh"), FrontWallCentre.ToCompactString());

	TryProjectToNavMesh(Outside);
	TryProjectToNavMesh(Inside);
	const UNavigationPath* Path = NavigationSystem->FindPathToLocationSynchronously(GetWorld(), Outside, Inside);
	const bool bHasPath = IsValid(Path) && Path->IsValid() && not Path->IsPartial();
	const float StraightCm = FVector::Dist2D(Outside, Inside);
	const float PathCm = bHasPath ? Path->GetPathLength() : 0.f;
	float ClosestToEntranceCm = BIG_NUMBER;
	if (bHasPath)
	{
		for (const FVector& PathPoint : Path->PathPoints)
		{
			ClosestToEntranceCm = FMath::Min(ClosestToEntranceCm, FVector::Dist2D(PathPoint, Entrance));
		}
	}
	// Which leg breaks when there is no path: reaching the entrance from outside, or the inside from the entrance.
	FVector EntranceOnNavMesh = Entrance;
	const bool bEntranceOnNavMesh = TryProjectToNavMesh(EntranceOnNavMesh);
	const UNavigationPath* ToEntrance = NavigationSystem->FindPathToLocationSynchronously(GetWorld(), Outside,
		EntranceOnNavMesh);
	const UNavigationPath* EntranceToInside = NavigationSystem->FindPathToLocationSynchronously(GetWorld(),
		EntranceOnNavMesh, Inside);
	auto DescribePath = [](const UNavigationPath* LegPath)
	{
		return not IsValid(LegPath) || not LegPath->IsValid() ? FString(TEXT("none"))
			       : LegPath->IsPartial() ? FString(TEXT("partial")) : FString(TEXT("full"));
	};
	Check(bHasPath && PathCm >= StraightCm * MinDetourFactor && ClosestToEntranceCm <= MaxPathDistanceToEntranceCm,
	      Subject.Name, TEXT("path_into_pit_uses_entrance"),
	      FString::Printf(TEXT("path=%d length=%.0f straight=%.0f closest_to_entrance=%.0f into_pit=%s "
		                      "outside_to_entrance=%s entrance_to_inside=%s entrance_on_navmesh=%d entrance_dz=%.0f "
		                      "entrance_nav=%s inside_nav=%s"), bHasPath ? 1 : 0,
	                      PathCm, StraightCm, ClosestToEntranceCm, *DescribePath(Path), *DescribePath(ToEntrance),
	                      *DescribePath(EntranceToInside), bEntranceOnNavMesh ? 1 : 0, EntranceOnNavMesh.Z - Inside.Z,
	                      *EntranceOnNavMesh.ToCompactString(), *Inside.ToCompactString()));
}

void UTeamWeaponDigInTestSubsystem::CheckCrewInsidePit(const FTWDigInTestSubject& Subject,
                                                       const ATeamWeaponController& Controller,
                                                       const UTeamWeaponDigInComponent& DigIn)
{
	using namespace TeamWeaponDigInTestPrivate;
	TArray<ASquadUnit*> Operators;
	Controller.GetCrewOperators(Operators);
	float WorstOperatorCm = 0.f;
	for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
	{
		FVector Post;
		if (IsValid(Operators[OperatorIndex]) && Controller.TryGetOperatorCrewPositionLocation(OperatorIndex, Post))
		{
			WorstOperatorCm = FMath::Max(WorstOperatorCm,
			                             FVector::Dist2D(Operators[OperatorIndex]->GetActorLocation(), Post));
		}
	}
	Check(WorstOperatorCm <= MaxOperatorPostDistanceCm, Subject.Name, TEXT("operators_at_posts_in_pit"),
	      FString::Printf(TEXT("worst=%.0f cm"), WorstOperatorCm));
	TArray<ASquadUnit*> Guards;
	Controller.GetCrewGuards(Guards);
	float WorstGuardCm = 0.f;
	for (int32 GuardIndex = 0; GuardIndex < Guards.Num(); ++GuardIndex)
	{
		FVector Post;
		if (IsValid(Guards[GuardIndex]) && DigIn.TryGetGuardPostLocation(GuardIndex, Post))
		{
			const float GuardDistanceCm = FVector::Dist2D(Guards[GuardIndex]->GetActorLocation(), Post);
			WorstGuardCm = FMath::Max(WorstGuardCm, GuardDistanceCm);
			const ATeamWeaponDigInWall* Wall = DigIn.GetDigInWall();
			const FTransform PitTransform = IsValid(Wall) ? Wall->GetActorTransform() : FTransform::Identity;
			UE_LOG(LogRTSTeamWeaponDigIn, Display,
			       TEXT("RTS_TWDIG guard_debug subject=%s guard=%d distance=%.0f local=%s post_local=%s path_following=%d"),
			       *Subject.Name, GuardIndex, GuardDistanceCm,
			       *PitTransform.InverseTransformPosition(Guards[GuardIndex]->GetActorLocation()).ToCompactString(),
			       *PitTransform.InverseTransformPosition(Post).ToCompactString(),
			       Guards[GuardIndex]->GetIsPathFollowingActive() ? 1 : 0);
		}
	}
	Check(WorstGuardCm <= MaxGuardPostDistanceCm, Subject.Name, TEXT("guards_at_pit_posts"),
	      FString::Printf(TEXT("guards=%d worst=%.0f cm"), Guards.Num(), WorstGuardCm));
}

void UTeamWeaponDigInTestSubsystem::CheckWallOnGround(const FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon,
                                                      const ATeamWeaponDigInWall& Wall)
{
	using namespace TeamWeaponDigInTestPrivate;
	const FBox Bounds = Wall.GetWallMeshComponent()->GetStaticMesh()->GetBoundingBox();
	float WorstFloatCm = -BIG_NUMBER;
	float DeepestBuryCm = BIG_NUMBER;
	const double LocalXs[] = {Bounds.Min.X, (Bounds.Min.X + Bounds.Max.X) * 0.5, Bounds.Max.X};
	const double LocalYs[] = {Bounds.Min.Y, 0.0, Bounds.Max.Y};
	for (int32 SampleIndex = 0; SampleIndex < UE_ARRAY_COUNT(LocalXs) * UE_ARRAY_COUNT(LocalYs); ++SampleIndex)
	{
		const float GapCm = GetWallGapAt(Weapon, Wall, LocalXs[SampleIndex / UE_ARRAY_COUNT(LocalYs)],
		                                 LocalYs[SampleIndex % UE_ARRAY_COUNT(LocalYs)]);
		if (GapCm == BIG_NUMBER)
		{
			continue;
		}
		WorstFloatCm = FMath::Max(WorstFloatCm, GapCm);
		DeepestBuryCm = FMath::Min(DeepestBuryCm, GapCm);
	}
	const bool bIsSlitPit = IsValid(Wall.GetWallMeshComponent()->GetStaticMesh()->FindSocket(TEXT("ProbeSlit")));
	Check(WorstFloatCm <= (bIsSlitPit ? MaxSlitWallFloatCm : MaxWallFloatCm), Subject.Name, TEXT("wall_on_ground"),
	      FString::Printf(TEXT("worst_float=%.1f deepest=%.1f cm tilt=%s"), WorstFloatCm, DeepestBuryCm,
	                      *Wall.GetActorRotation().ToCompactString()));
}

float UTeamWeaponDigInTestSubsystem::GetWallGapAt(const ATeamWeapon& Weapon, const ATeamWeaponDigInWall& Wall,
                                                   const double LocalX, const double LocalY) const
{
	const FVector WallBase = Wall.GetActorTransform().TransformPosition(FVector(LocalX, LocalY, 0.0));
	float GroundZ = 0.f;
	const FVector ProbeStart = WallBase + FVector(0.f, 0.f, TeamWeaponDigInTestPrivate::GroundProbeRaiseCm);
	// Positive: the wall's base is above the ground there (a gap); BIG_NUMBER when no ground was found.
	return Weapon.TryGetGroundHeightAt(ProbeStart, GroundZ) ? WallBase.Z - GroundZ : BIG_NUMBER;
}

void UTeamWeaponDigInTestSubsystem::TickWaitingForNavMeshRestore(FTWDigInTestSubject& Subject,
                                                                 ATeamWeaponController& Controller,
                                                                 const ATeamWeapon& Weapon, const float StepSeconds)
{
	using namespace TeamWeaponDigInTestPrivate;
	const bool bRestored = GetIsPointOnNavMesh(Subject.FormerFrontWallCentre, NavBoxProbeExtentCm,
	                                           Subject.FormerFrontWallHalfHeightCm * 2.f);
	if (not bRestored && StepSeconds < MaxNavMeshRestoreGameSeconds)
	{
		return;
	}
	Check(bRestored, Subject.Name, TEXT("navmesh_back_after_pit_removed"),
	      FString::Printf(TEXT("after %.1f s"), StepSeconds));
	// Drive out through where the front wall stood: the pit must not leave the gun boxed in.
	FVector DriveTo = Weapon.GetActorLocation() + FRotator(0.f, Weapon.GetActorRotation().Yaw, 0.f).RotateVector(
		FVector(DriveOutDistanceCm, 0.f, 0.f));
	TryProjectToNavMesh(DriveTo);
	Subject.LocationBeforeDrivingOut = Weapon.GetActorLocation();
	const ECommandQueueError MoveError = Controller.MoveToLocation(DriveTo, true, FRotator::ZeroRotator);
	Check(MoveError == ECommandQueueError::NoError, Subject.Name, TEXT("move_order_accepted_after_pit"),
	      UEnum::GetValueAsString(MoveError));
	EnterStep(Subject, ETWDigInTestStep::DrivingOutOfPit);
}

void UTeamWeaponDigInTestSubsystem::TickDrivingOutOfPit(FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon,
                                                        const float StepSeconds)
{
	using namespace TeamWeaponDigInTestPrivate;
	const float DrivenCm = FVector::Dist2D(Weapon.GetActorLocation(), Subject.LocationBeforeDrivingOut);
	if (DrivenCm < MinDriveOutProgressCm && StepSeconds < MaxDriveOutGameSeconds)
	{
		return;
	}
	Check(DrivenCm >= MinDriveOutProgressCm, Subject.Name, TEXT("gun_drives_out_through_former_wall"),
	      FString::Printf(TEXT("moved %.0f cm in %.1f s"), DrivenCm, StepSeconds));
	EnterStep(Subject, ETWDigInTestStep::Done);
}

void UTeamWeaponDigInTestSubsystem::CaptureComparisonMesh(FTWDigInTestSubject& Subject, const ATeamWeapon& Weapon)
{
	using namespace TeamWeaponDigInTestPrivate;
	FString MeshPath;
	if (not bM_TakePictures || Subject.bHasComparisonPicture ||
		not FParse::Value(FCommandLine::Get(), CompareMeshSwitch, MeshPath))
	{
		return;
	}
	Subject.bHasComparisonPicture = true;
	UStaticMesh* ComparisonMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (not IsValid(ComparisonMesh))
	{
		UE_LOG(LogRTSTeamWeaponDigIn, Warning, TEXT("RTS_TWDIG comparison mesh not found: %s"), *MeshPath);
		return;
	}
	const FRotator Heading(0.f, Weapon.GetActorRotation().Yaw, 0.f);
	FVector Location = Weapon.GetActorLocation() + Heading.RotateVector(FVector(0.f, ComparisonMeshSideOffsetCm, 0.f));
	TryProjectToNavMesh(Location);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Comparison = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location,
	                                                                       Heading, SpawnParameters);
	if (not IsValid(Comparison))
	{
		return;
	}
	Comparison->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Comparison->GetStaticMeshComponent()->SetStaticMesh(ComparisonMesh);
	CapturePicture(*Comparison, Subject.Name + TEXT("_comparison"), MinPitPictureDistanceScale);
}

void UTeamWeaponDigInTestSubsystem::CheckDigInRemoved(const FTWDigInTestSubject& Subject,
                                                      ATeamWeaponController& Controller, const ATeamWeapon& Weapon,
                                                      const TCHAR* Label)
{
	using namespace TeamWeaponDigInTestPrivate;
	const UTeamWeaponDigInComponent* DigIn = Weapon.GetDigInComponent();
	const UHealthComponent* Health = Weapon.FindComponentByClass<UHealthComponent>();
	int32 WallsLeft = 0;
	for (TActorIterator<ATeamWeaponDigInWall> It(GetWorld()); It; ++It)
	{
		WallsLeft += (It->GetOwner() == &Weapon && IsValid(*It)) ? 1 : 0;
	}
	Check(IsValid(DigIn) && not DigIn->GetIsDigInActive() && WallsLeft == 0, Subject.Name,
	      *FString::Printf(TEXT("%s_removes_pit"), Label), FString::Printf(TEXT("walls_left=%d"), WallsLeft));
	if (IsValid(Health))
	{
		const bool bMaxHealthBack = FMath::IsNearlyEqual(Health->GetMaxHealth(), Subject.BaseMaxHealth,
		                                                 Subject.BaseMaxHealth * HealthTolerance);
		const bool bMultiplierBack = FMath::IsNearlyEqual(Health->GetDamageReductionMultiplier(),
		                                                  Subject.BaseDamageMultiplier, HealthTolerance);
		Check(bMaxHealthBack && bMultiplierBack, Subject.Name,
		      *FString::Printf(TEXT("%s_restores_health_and_damage"), Label),
		      FString::Printf(TEXT("max=%.1f (base %.1f) mlt=%.3f (base %.3f)"), Health->GetMaxHealth(),
		                      Subject.BaseMaxHealth, Health->GetDamageReductionMultiplier(),
		                      Subject.BaseDamageMultiplier));
	}
	const TArray<EAbilityID> CardNow = Controller.GetUnitAbilities();
	Check(CardNow == Subject.CardBeforeDigIn, Subject.Name, *FString::Printf(TEXT("%s_restores_card"), Label),
	      FString::Printf(TEXT("move=%d dig_in=%d break_cover=%d"), Controller.HasAbility(EAbilityID::IdMove),
	                      Controller.HasAbility(EAbilityID::IdDigIn), Controller.HasAbility(EAbilityID::IdBreakCover)));
}

// ---------------------------------------------------------------------------------------------------------------
// Survey

FString UTeamWeaponDigInTestSubsystem::GetRosterJson(const ATeamWeaponController& Controller, ATeamWeapon& Weapon) const
{
	const URTSComponent* SquadRTS = Controller.FindComponentByClass<URTSComponent>();
	const URTSComponent* GunRTS = Weapon.FindComponentByClass<URTSComponent>();
	FString WeaponsJson;
	for (const UWeaponState* WeaponState : Weapon.GetWeapons())
	{
		if (not IsValid(WeaponState))
		{
			continue;
		}
		const FWeaponData& Data = WeaponState->GetRawWeaponData();
		WeaponsJson += FString::Printf(TEXT("%s{\"name\":\"%s\",\"calibre\":%d,\"range\":%.0f,\"damage\":%.0f}"),
		                               WeaponsJson.IsEmpty() ? TEXT("") : TEXT(","),
		                               *UEnum::GetValueAsString(Data.WeaponName), Data.WeaponCalibre, Data.Range,
		                               Data.BaseDamage);
	}
	FString InfantryJson;
	for (const ASquadUnit* SquadUnit : const_cast<ATeamWeaponController&>(Controller).GetSquadUnitsChecked())
	{
		const AActor* InfantryWeapon = IsValid(SquadUnit) ? SquadUnit->GetInfantryWeapon() : nullptr;
		InfantryJson += FString::Printf(TEXT("%s\"%s\""), InfantryJson.IsEmpty() ? TEXT("") : TEXT(","),
		                                IsValid(InfantryWeapon) ? *InfantryWeapon->GetClass()->GetName() : TEXT("none"));
	}
	return FString::Printf(
		TEXT("\"squad_subtype\":\"%s\",\"squad_owner\":%d,\"gun_subtype\":\"%s\",\"gun_owner\":%d,")
		TEXT("\"weapons\":[%s],\"infantry\":[%s]"),
		IsValid(SquadRTS) ? *UEnum::GetValueAsString(SquadRTS->GetSubtypeAsSquadSubtype()) : TEXT("none"),
		IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : -1,
		IsValid(GunRTS) ? *UEnum::GetValueAsString(GunRTS->GetSubtypeAsSquadSubtype()) : TEXT("none"),
		IsValid(GunRTS) ? GunRTS->GetOwningPlayer() : -1, *WeaponsJson, *InfantryJson);
}

void UTeamWeaponDigInTestSubsystem::SurveySubject(const FTWDigInTestSubject& Subject,
                                                  const ATeamWeaponController& Controller, ATeamWeapon& Weapon)
{
	using namespace TeamWeaponDigInTestPrivate;
	const FVector Origin = Weapon.GetActorLocation();
	const FRotator Heading(0.f, Weapon.GetActorRotation().Yaw, 0.f);
	auto ToLocal = [&Origin, &Heading](const FVector& WorldLocation)
	{
		return Heading.UnrotateVector(WorldLocation - Origin);
	};
	float GroundZ = Origin.Z;
	TryGetGroundZ(Origin, GroundZ);

	const URTSComponent* RTSComponent = Controller.FindComponentByClass<URTSComponent>();
	const FString SubtypeName = IsValid(RTSComponent)
		                            ? UEnum::GetValueAsString(RTSComponent->GetSubtypeAsSquadSubtype())
		                            : TEXT("unknown");
	const int32 OwningPlayer = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1;

	FBox BoneBox(ForceInit);
	FString BarrelBone;
	float BarrelTipX = -BIG_NUMBER;
	float BarrelTipZ = 0.f;
	FString SocketsJson;
	const USkeletalMeshComponent* Mesh = Weapon.FindComponentByClass<USkeletalMeshComponent>();
	FBox BoundsBox(ForceInit);
	if (IsValid(Mesh))
	{
		for (int32 BoneIndex = 0; BoneIndex < Mesh->GetNumBones(); ++BoneIndex)
		{
			const FName BoneName = Mesh->GetBoneName(BoneIndex);
			const FVector Local = ToLocal(Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace));
			BoneBox += Local;
			if (Local.X > BarrelTipX)
			{
				BarrelTipX = Local.X;
				BarrelTipZ = Local.Z;
				BarrelBone = BoneName.ToString();
			}
		}
		const FBox WorldBounds = Mesh->Bounds.GetBox();
		BoundsBox += ToLocal(WorldBounds.Min);
		BoundsBox += ToLocal(WorldBounds.Max);
		const USkeletalMesh* MeshAsset = Mesh->GetSkeletalMeshAsset();
		if (IsValid(MeshAsset))
		{
			for (const USkeletalMeshSocket* Socket : MeshAsset->GetActiveSocketList())
			{
				if (not IsValid(Socket))
				{
					continue;
				}
				SocketsJson += FString::Printf(TEXT("%s\"%s\":%s"), SocketsJson.IsEmpty() ? TEXT("") : TEXT(","),
				                               *Socket->SocketName.ToString(),
				                               *VectorToJson(ToLocal(Mesh->GetSocketLocation(Socket->SocketName))));
			}
		}
	}

	FString PostsJson;
	FString OperatorsJson;
	TArray<ASquadUnit*> Operators;
	Controller.GetCrewOperators(Operators);
	for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
	{
		FVector PostLocation;
		if (Controller.TryGetOperatorCrewPositionLocation(OperatorIndex, PostLocation))
		{
			PostsJson += (PostsJson.IsEmpty() ? TEXT("") : TEXT(",")) + VectorToJson(ToLocal(PostLocation));
		}
		if (IsValid(Operators[OperatorIndex]))
		{
			OperatorsJson += (OperatorsJson.IsEmpty() ? TEXT("") : TEXT(",")) +
				VectorToJson(ToLocal(Operators[OperatorIndex]->GetActorLocation()));
		}
	}
	FString GuardsJson;
	TArray<ASquadUnit*> Guards;
	Controller.GetCrewGuards(Guards);
	for (const ASquadUnit* Guard : Guards)
	{
		if (IsValid(Guard))
		{
			GuardsJson += (GuardsJson.IsEmpty() ? TEXT("") : TEXT(",")) + VectorToJson(ToLocal(Guard->GetActorLocation()));
		}
	}

	const FString Entry = FString::Printf(
		TEXT("{\"name\":\"%s\",\"class\":\"%s\",\"weapon_class\":\"%s\",\"weapon_parent\":\"%s\",\"subtype\":\"%s\",\"owner\":%d,\"origin_above_ground\":%.1f,")
		TEXT("\"bones_min\":%s,\"bones_max\":%s,\"bounds_min\":%s,\"bounds_max\":%s,")
		TEXT("\"barrel_bone\":\"%s\",\"barrel_tip_x\":%.1f,\"barrel_tip_z\":%.1f,")
		TEXT("\"posts\":[%s],\"operators\":[%s],\"guards\":[%s],\"sockets\":{%s},%s}"),
		*Subject.Name, *Subject.ClassPath, *Weapon.GetClass()->GetName(), *Weapon.GetClass()->GetSuperClass()->GetName(),
		*SubtypeName, OwningPlayer, Origin.Z - GroundZ,
		*VectorToJson(BoneBox.Min), *VectorToJson(BoneBox.Max), *VectorToJson(BoundsBox.Min),
		*VectorToJson(BoundsBox.Max), *BarrelBone, BarrelTipX, BarrelTipZ, *PostsJson, *OperatorsJson, *GuardsJson,
		*SocketsJson, *GetRosterJson(Controller, Weapon));
	M_SurveyEntries.Add(Entry);
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG survey %s"), *Entry);
	Check(true, Subject.Name, TEXT("surveyed"), SubtypeName);
}

void UTeamWeaponDigInTestSubsystem::WriteSurveyFile() const
{
	const FString SurveyPath = FPaths::ProjectSavedDir() / TEXT("TWDigIn") / TEXT("tw_digin_survey.json");
	const FString Json = TEXT("[\n") + FString::Join(M_SurveyEntries, TEXT(",\n")) + TEXT("\n]\n");
	FFileHelper::SaveStringToFile(Json, *SurveyPath);
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG survey_file %s entries=%d"), *SurveyPath,
	       M_SurveyEntries.Num());
}

// ---------------------------------------------------------------------------------------------------------------
// Pictures

bool UTeamWeaponDigInTestSubsystem::CreateCaptureRig()
{
	using namespace TeamWeaponDigInTestPrivate;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* Capture = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
	                                                                   FTransform::Identity, SpawnParameters);
	M_RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), PictureWidth, PictureHeight,
	                                                               RTF_RGBA8_SRGB);
	if (not IsValid(Capture) || not IsValid(M_RenderTarget))
	{
		return false;
	}
	M_Capture = Capture;
	USceneCaptureComponent2D* CaptureComponent = Capture->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = M_RenderTarget;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = PictureFieldOfView;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the units, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	// -CaptureExposureBias=<stops> brightens pictures on maps lit for evening or night.
	float ExposureBias = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CaptureExposureBias="), ExposureBias))
	{
		CaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
		CaptureComponent->PostProcessSettings.AutoExposureBias = ExposureBias;
		CaptureComponent->PostProcessBlendWeight = 1.f;
	}
	return true;
}

void UTeamWeaponDigInTestSubsystem::CapturePicture(const AActor& Focus, const FString& Label,
                                                   const float DistanceScale)
{
	using namespace TeamWeaponDigInTestPrivate;
	ASceneCapture2D* Capture = M_Capture.Get();
	if (not IsValid(Capture) || not IsValid(M_RenderTarget))
	{
		return;
	}
	const FVector LookAt = Focus.GetActorLocation() + PictureLookAtOffset;
	const FVector CameraLocation = LookAt + FRotator(0.f, Focus.GetActorRotation().Yaw, 0.f).RotateVector(
		PictureCameraOffset * DistanceScale);
	Capture->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());
	Capture->GetCaptureComponent2D()->CaptureScene();
	const FString FileName = FString::Printf(TEXT("%02d_%s.png"), M_PictureIndex++, *Label);
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory, FileName);
	UE_LOG(LogRTSTeamWeaponDigIn, Display, TEXT("RTS_TWDIG picture %s"), *FileName);
}

#else

bool UTeamWeaponDigInTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UTeamWeaponDigInTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTeamWeaponDigInTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTeamWeaponDigInTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UTeamWeaponDigInTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTeamWeaponDigInTestSubsystem, STATGROUP_Tickables);
}

#endif
