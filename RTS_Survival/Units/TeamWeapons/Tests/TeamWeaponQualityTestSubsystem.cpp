// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TeamWeaponQualityTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/RTSComponents/TowMechanic/VehicleTow/VehicleTow.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponMover.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTeamWeaponQuality, Log, All);

namespace TeamWeaponQualityPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("TeamWeaponQualityValidate");
	const TCHAR* const DirectorySwitch = TEXT("TWQualityDir=");
	const TCHAR* const LocationSwitch = TEXT("TWQualityAt=");
	const TCHAR* const SquadsSwitch = TEXT("TWQualitySquads=");
	const TCHAR* const TowsSwitch = TEXT("TWQualityTows=");
	const TCHAR* const ExitCargoSwitch = TEXT("TWQualityExitCargo");
	const TCHAR* const LegLengthSwitch = TEXT("TWQualityLegCm=");
	const TCHAR* const SearchRadiusSwitch = TEXT("TWQualitySearchCm=");
	const TCHAR* const CrewAnimsSwitch = TEXT("TWQualityCrewAnims");
	const TCHAR* const CrewAnimDirectorySwitch = TEXT("TWQualityCrewAnimDir=");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	// Only subtypes the async spawner has training options for; the GrW 42 and the Kettenkrad have none.
	const ESquadSubtype DefaultSquads[] = {
		ESquadSubtype::Squad_Ger_PaK40,
		ESquadSubtype::Squad_Rus_Zis_76MM,
		ESquadSubtype::Squad_Rus_80mm_Mortar,
		ESquadSubtype::Squad_Ger_LefH18,
		ESquadSubtype::Squad_Rus_120mm_Mortar
	};
	const TPair<ETankSubtype, ESquadSubtype> DefaultTows[] = {
		{ETankSubtype::Tank_Sdkfz250, ESquadSubtype::Squad_Ger_PaK38},
		{ETankSubtype::Tank_Sdkfz250, ESquadSubtype::Squad_Rus_53K_45MM}
	};

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartRealSeconds = 90.0;
	constexpr double MaxRunRealSeconds = 2400.0;
	constexpr float MaxRunGameSeconds = 520.f;
	constexpr float MaxSpawnGameSeconds = 45.f;
	constexpr float MaxDeployGameSeconds = 45.f;
	constexpr float MaxLegGameSeconds = 110.f;
	constexpr float MaxTowDriveGameSeconds = 90.f;
	constexpr float DeployedSettleSeconds = 4.f;
	constexpr float DeployedMeasureSeconds = 1.5f;
	constexpr float TowedSettleSeconds = 1.5f;
	constexpr float StoppedSettleSeconds = 3.f;
	constexpr float DefaultLegLengthCm = 3600.f;
	// Firing step of the crew animation audit: enough reloads to see the loader react and return to its loop.
	constexpr int32 FiringReloadsWanted = 3;
	constexpr float MaxFiringSeconds = 75.f;
	// Without a reload by then the gun cannot hit the ground target; reloads are then started by the test hook.
	constexpr float ForcedReloadAfterSeconds = 20.f;
	constexpr float ForcedReloadPauseSeconds = 3.f;
	constexpr float FireTargetRangeFraction = 0.55f;
	constexpr float MinFireTargetDistanceCm = 1500.f;
	constexpr float TowFirstSegmentCm = 3000.f;
	constexpr float TowTurnSegmentCm = 2200.f;
	constexpr float TowTurnDegrees = 70.f;
	constexpr float GunMoveAfterDetachCm = 1500.f;
	constexpr float RetowDriveCm = 2000.f;
	constexpr float RetowDriveTurnDegrees = -60.f;
	constexpr float MinDriveSecondsBeforeIdle = 2.f;
	constexpr float MaxTruckArrivalCm = 800.f;
	constexpr float StoppedMeasureSeconds = 1.f;
	// Leg index used to name the pictures of the pushed move after detaching.
	constexpr int32 DetachedGunMoveLegIndex = 9;
	constexpr float OrderDebugLogIntervalSeconds = 10.f;
	constexpr float RetowDebugLogIntervalSeconds = 2.f;

	constexpr float SquadRingRadiusCm = 3500.f;
	constexpr float TowRingRadiusCm = 7000.f;
	constexpr int32 LaneDirectionCount = 16;
	constexpr float LaneSampleSpacingCm = 250.f;
	constexpr float MaxLaneSlopeDegrees = 24.f;
	constexpr float MaxLanePathStretch = 1.45f;
	constexpr float MinDistanceToOtherSubjectsCm = 1600.f;
	const FVector NavProjectExtent(400.f, 400.f, 800.f);
	constexpr float GroundTraceUpCm = 600.f;
	constexpr float GroundTraceDownCm = 3000.f;
	constexpr int32 MaxGroundTraceAttempts = 5;
	constexpr float CentreDropTraceHalfHeightCm = 30000.f;
	constexpr float DefaultHillSearchRadiusCm = 40000.f;
	constexpr double RandomCentreAfterSeconds = 15.0;
	constexpr double GameUnlockRetrySeconds = 2.0;
	constexpr float KeepAliveHealthPerFrame = 100000.f;
	constexpr float HillSearchSpacingCm = 5000.f;
	// Relief is judged over the area the subjects use: the rings plus their lanes.
	constexpr float ReliefSampleRadiusCm = 7000.f;
	constexpr int32 ReliefSamplesPerAxis = 9;
	constexpr float MaxWalkableReliefSlopeDegrees = 22.f;
	constexpr float MinDistanceToEnemiesCm = 20000.f;
	constexpr int32 PlayerOwnerIndex = 1;
	constexpr float LargeTowWheelGapCm = 40.f;
	const FVector CentreNavProjectExtent(2000.f, 2000.f, 5000.f);

	// Footprint corners at this fraction of the mesh's half extent; a pushed gun rests on wheels and trail there.
	constexpr float FootprintExtentFraction = 0.3f;
	constexpr float MovingSpeedThresholdCmPerSec = 30.f;
	// An operator this far in front of the weapon origin walks ahead of the gun he is supposed to push.
	constexpr float AheadOfWeaponThresholdCm = 150.f;

	// Thresholds.
	constexpr float MaxDeployedOriginGapCm = 10.f;
	constexpr float MaxDeployedContactGapCm = 8.f;
	constexpr float MaxPenetrationCm = 30.f;
	constexpr float MaxMovingOriginGapP95Cm = 12.f;
	constexpr float MaxMovingContactGapP95Cm = 12.f;
	// The game snaps an operator back once he is beyond the crew position's acceptance radius (75 cm).
	constexpr float MaxOperatorPostDistanceCm = 80.f;
	constexpr float MaxOperatorFeetGapCm = 30.f;
	constexpr float MinGuardDistanceCm = 120.f;
	constexpr float MaxGuardDistanceCm = 1000.f;
	constexpr float MaxOperatorSlotDistanceP90Cm = 150.f;
	constexpr float MaxOperatorSlotDistanceCm = 400.f;
	constexpr float MaxOperatorAheadFraction = 0.03f;
	constexpr float MaxGuardAheadFraction = 0.1f;
	constexpr float MaxLegEndDistanceCm = 400.f;
	constexpr float MinTowSwayYawDegrees = 4.f;
	constexpr float MinTowSwayPitchOrRollDegrees = 0.75f;
	constexpr float MaxTowYawDegrees = 70.f;
	constexpr float MaxTowPitchRollDegrees = 35.f;
	constexpr float MaxTowRateP95DegreesPerSec = 120.f;
	constexpr float MaxTowHitchDriftCm = 12.f;
	constexpr float MaxTowWheelGapP90Cm = 15.f;
	constexpr float MaxStoppedRateDegreesPerSec = 12.f;

	// Capture rig.
	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float PictureFieldOfView = 45.f;
	const FVector PictureCameraOffset(-950.f, 700.f, 480.f);
	const FVector PictureLookAtOffset(0.f, 0.f, 60.f);
	constexpr int32 PictureAimFrames = 4;
	constexpr float PictureCameraClearanceCm = 300.f;

	bool GetIsUnitActor(const AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return false;
		}
		return Actor->IsA<APawn>() || Actor->IsA<ATeamWeapon>() || Actor->FindComponentByClass<URTSComponent>() != nullptr;
	}

	FVector GetPlanar(const FVector& Vector)
	{
		return FVector(Vector.X, Vector.Y, 0.f);
	}

	FString GetSquadName(const ESquadSubtype Subtype)
	{
		FString Name = UEnum::GetValueAsString(Subtype);
		Name.RemoveFromStart(TEXT("ESquadSubtype::"));
		return Name;
	}

	FString GetTankName(const ETankSubtype Subtype)
	{
		FString Name = UEnum::GetValueAsString(Subtype);
		Name.RemoveFromStart(TEXT("ETankSubtype::"));
		return Name;
	}

	FString GetStateName(const ATeamWeaponController& Controller)
	{
		FString Name = UEnum::GetValueAsString(Controller.GetTeamWeaponState());
		Name.RemoveFromStart(TEXT("ETeamWeaponState::"));
		return Name;
	}

	float GetTiltDegrees(const AActor& Actor)
	{
		const float UpDot = FMath::Clamp(FVector::DotProduct(Actor.GetActorUpVector(), FVector::UpVector), -1.f, 1.f);
		return FMath::RadiansToDegrees(FMath::Acos(UpDot));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// FTWQualityMetric

float FTWQualityMetric::GetMax() const
{
	float MaxValue = -BIG_NUMBER;
	for (const float Value : Values)
	{
		MaxValue = FMath::Max(MaxValue, Value);
	}
	return GetHasValues() ? MaxValue : 0.f;
}

float FTWQualityMetric::GetMin() const
{
	float MinValue = BIG_NUMBER;
	for (const float Value : Values)
	{
		MinValue = FMath::Min(MinValue, Value);
	}
	return GetHasValues() ? MinValue : 0.f;
}

float FTWQualityMetric::GetMean() const
{
	if (not GetHasValues())
	{
		return 0.f;
	}
	double Sum = 0.0;
	for (const float Value : Values)
	{
		Sum += Value;
	}
	return static_cast<float>(Sum / Values.Num());
}

float FTWQualityMetric::GetPercentile(const float Fraction) const
{
	if (not GetHasValues())
	{
		return 0.f;
	}
	TArray<float> Sorted = Values;
	Sorted.Sort();
	const int32 Index = FMath::Clamp(FMath::FloorToInt(Fraction * (Sorted.Num() - 1)), 0, Sorted.Num() - 1);
	return Sorted[Index];
}

FString FTWQualityMetric::Describe() const
{
	if (not GetHasValues())
	{
		return TEXT("n=0");
	}
	return FString::Printf(TEXT("min=%.1f mean=%.1f p90=%.1f p95=%.1f max=%.1f n=%d"),
	                       GetMin(), GetMean(), GetPercentile(0.9f), GetPercentile(0.95f), GetMax(), Values.Num());
}

// ---------------------------------------------------------------------------------------------------------------
// Subsystem lifecycle

bool UTeamWeaponQualityTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TeamWeaponQualityPrivate::ValidateSwitch);
}

void UTeamWeaponQualityTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace TeamWeaponQualityPrivate;
	Super::OnWorldBeginPlay(InWorld);
	bM_TakePictures = FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	M_LegLengthCm = DefaultLegLengthCm;
	FParse::Value(FCommandLine::Get(), LegLengthSwitch, M_LegLengthCm);
	bM_AuditCrewAnims = FParse::Param(FCommandLine::Get(), CrewAnimsSwitch);
	if (bM_AuditCrewAnims)
	{
		FString CrewAnimDirectory;
		FParse::Value(FCommandLine::Get(), CrewAnimDirectorySwitch, CrewAnimDirectory);
		M_CrewAnimAudit.Initialise(CrewAnimDirectory);
	}
	// Error dialogs stall unattended runs; they still reach the log.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	bM_IsActive = true;
	M_StartRealSeconds = FPlatformTime::Seconds();
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC started pictures=%s leg_cm=%.0f"),
	       bM_TakePictures ? *M_OutputDirectory : TEXT("off"), M_LegLengthCm);
}

void UTeamWeaponQualityTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

TStatId UTeamWeaponQualityTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTeamWeaponQualityTestSubsystem, STATGROUP_Tickables);
}

float UTeamWeaponQualityTestSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UTeamWeaponQualityTestSubsystem::Check(const bool bCondition, const FString& Subject, const TCHAR* Description,
                                            const FString& Details)
{
	(bCondition ? M_PassedCheckCount : M_FailedCheckCount)++;
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC check=%s subject=%s %s %s (t=%.1f)"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Subject, Description, *Details, GetGameSeconds());
}

void UTeamWeaponQualityTestSubsystem::Finish(const TCHAR* Reason)
{
	if (bM_IsFinished)
	{
		return;
	}
	bM_IsFinished = true;
	if (bM_AuditCrewAnims)
	{
		M_CrewAnimAudit.Finish(GetGameSeconds());
	}
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC RESULT %s passed=%d failed=%d reason=%s game_t=%.1f real_s=%.0f"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount, Reason,
	       GetGameSeconds(), FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("TeamWeaponQualityTest"));
}

void UTeamWeaponQualityTestSubsystem::Tick(float DeltaTime)
{
	using namespace TeamWeaponQualityPrivate;
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
		if (RealSeconds < HudBuildSeconds)
		{
			return;
		}
		if (not TryStartGame())
		{
			UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC waiting for the RTS player controller"));
			return;
		}
		if (not TryFindTestCentre() && not TryUseRandomNavigableCentre(RealSeconds))
		{
			return;
		}
		float HillSearchRadiusCm = DefaultHillSearchRadiusCm;
		FParse::Value(FCommandLine::Get(), SearchRadiusSwitch, HillSearchRadiusCm);
		if (HillSearchRadiusCm > 0.f)
		{
			MoveCentreToHillyGround(HillSearchRadiusCm);
		}
		bM_HasStartedGame = true;
		if (bM_TakePictures && not CreateCaptureRig())
		{
			bM_TakePictures = false;
			UE_LOG(LogRTSTeamWeaponQuality, Warning, TEXT("RTS_TWQC capture rig could not be created"));
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
	for (FTWQualitySquadSubject& Subject : M_Squads)
	{
		TickSquadSubject(Subject);
		bAllDone &= Subject.Step == ETWQualitySquadStep::Done;
	}
	for (FTWQualityTowSubject& Subject : M_Tows)
	{
		TickTowSubject(Subject);
		bAllDone &= Subject.Step == ETWQualityTowStep::Done;
	}
	if (bM_AuditCrewAnims)
	{
		M_CrewAnimAudit.Tick(GetGameSeconds());
	}
	TickPictures();
	if (bAllDone && M_Pictures.Num() == 0)
	{
		Finish(TEXT("all_subjects_done"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Start up

bool UTeamWeaponQualityTestSubsystem::TryStartGame()
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

void UTeamWeaponQualityTestSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(TeamWeaponQualityPrivate::DifficultyButtonName));
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
	// Mission maps lock the game behind their opening cinematic; unpausing alone cannot end that.
	const double RealSeconds = FPlatformTime::Seconds();
	if (PlayerController->GetIsGameOnPause() && RealSeconds >= M_NextGameUnlockRealSeconds)
	{
		M_NextGameUnlockRealSeconds = RealSeconds + TeamWeaponQualityPrivate::GameUnlockRetrySeconds;
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC unlocked a locked game paused=%d"),
		       PlayerController->GetIsGameOnPause() ? 1 : 0);
	}
}

bool UTeamWeaponQualityTestSubsystem::TryFindTestCentre()
{
	using namespace TeamWeaponQualityPrivate;
	UWorld* World = GetWorld();
	FString LocationText;
	bool bFound = false;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		TArray<FString> Coordinates;
		LocationText.ParseIntoArray(Coordinates, TEXT(","), true);
		if (Coordinates.Num() >= 2)
		{
			M_TestCentre = FVector(FCString::Atof(*Coordinates[0]), FCString::Atof(*Coordinates[1]), 0.f);
			bFound = true;
		}
	}
	for (TActorIterator<AActor> It(World); It && not bFound; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			M_TestCentre = It->GetActorLocation();
			bFound = true;
		}
	}
	for (TActorIterator<APlayerStart> It(World); It && not bFound; ++It)
	{
		M_TestCentre = It->GetActorLocation();
		bFound = true;
	}
	if (not bFound)
	{
		return false;
	}
	// Map anchors sit at camera height, far above the ground the short unit traces reach.
	FHitResult GroundHit;
	const FVector AnchorTraceOffset(0.f, 0.f, CentreDropTraceHalfHeightCm);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TeamWeaponQualityCentre), false);
	const bool bHitLandscape = World->LineTraceSingleByChannel(
		GroundHit, M_TestCentre + AnchorTraceOffset, M_TestCentre - AnchorTraceOffset, COLLISION_TRACE_LANDSCAPE,
		QueryParams);
	const bool bHitGround = bHitLandscape || World->LineTraceSingleByChannel(
		GroundHit, M_TestCentre + AnchorTraceOffset, M_TestCentre - AnchorTraceOffset, ECC_Visibility, QueryParams);
	if (bHitGround)
	{
		M_TestCentre = GroundHit.ImpactPoint;
	}
	// The navmesh can arrive a little after the game starts; keep trying until the start timeout.
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation NavLocation;
	if (IsValid(NavigationSystem) &&
		NavigationSystem->ProjectPointToNavigation(M_TestCentre, NavLocation, CentreNavProjectExtent))
	{
		M_TestCentre = NavLocation.Location;
		return true;
	}
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC centre %s not on the navmesh yet landscape_hit=%d ground_hit=%d nav=%d"),
	       *M_TestCentre.ToCompactString(), bHitLandscape ? 1 : 0, bHitGround ? 1 : 0, IsValid(NavigationSystem) ? 1 : 0);
	return false;
}

bool UTeamWeaponQualityTestSubsystem::TryUseRandomNavigableCentre(const double RealSeconds)
{
	using namespace TeamWeaponQualityPrivate;
	// Some maps keep their player start or camera anchor off the playable ground; any walkable spot will do.
	if (RealSeconds < HudBuildSeconds + RandomCentreAfterSeconds)
	{
		return false;
	}
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation RandomLocation;
	if (not IsValid(NavigationSystem) || not NavigationSystem->GetRandomPoint(RandomLocation))
	{
		return false;
	}
	M_TestCentre = RandomLocation.Location;
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC centre from a random navmesh point %s"),
	       *M_TestCentre.ToCompactString());
	return true;
}

void UTeamWeaponQualityTestSubsystem::MoveCentreToHillyGround(const float SearchRadiusCm)
{
	using namespace TeamWeaponQualityPrivate;
	// Enemy units fight the subjects and their combat repositioning hides what the test measures; stay away from them.
	TArray<FVector> EnemyLocations;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() != PlayerOwnerIndex)
		{
			EnemyLocations.Add(It->GetActorLocation());
		}
	}
	float BestRelief = -1.f;
	float BestSlope = 0.f;
	FVector BestCentre = M_TestCentre;
	const int32 StepsPerSide = FMath::FloorToInt(SearchRadiusCm / HillSearchSpacingCm);
	for (int32 StepX = -StepsPerSide; StepX <= StepsPerSide; ++StepX)
	{
		for (int32 StepY = -StepsPerSide; StepY <= StepsPerSide; ++StepY)
		{
			FVector Candidate = M_TestCentre + FVector(StepX * HillSearchSpacingCm, StepY * HillSearchSpacingCm, 0.f);
			float GroundZ = 0.f;
			if (not TryGetGroundZ(Candidate + FVector(0.f, 0.f, CentreDropTraceHalfHeightCm * 0.1f), GroundZ))
			{
				continue;
			}
			Candidate.Z = GroundZ;
			if (not TryProjectToNavMesh(Candidate))
			{
				continue;
			}
			const bool bIsNearEnemies = EnemyLocations.ContainsByPredicate([&Candidate](const FVector& EnemyLocation)
			{
				return FVector::Dist2D(EnemyLocation, Candidate) < MinDistanceToEnemiesCm;
			});
			if (bIsNearEnemies)
			{
				continue;
			}
			float MaxSlopeDegrees = 0.f;
			const float Relief = GetReliefAround(Candidate, MaxSlopeDegrees);
			if (MaxSlopeDegrees > MaxWalkableReliefSlopeDegrees || Relief <= BestRelief)
			{
				continue;
			}
			BestRelief = Relief;
			BestSlope = MaxSlopeDegrees;
			BestCentre = Candidate;
		}
	}
	UE_LOG(LogRTSTeamWeaponQuality, Display,
	       TEXT("RTS_TWQC hill_search from=%s best=%s relief_cm=%.0f max_slope_deg=%.1f enemies_on_map=%d"),
	       *M_TestCentre.ToCompactString(), *BestCentre.ToCompactString(), BestRelief, BestSlope, EnemyLocations.Num());
	M_TestCentre = BestCentre;
}

float UTeamWeaponQualityTestSubsystem::GetReliefAround(const FVector& Centre, float& OutMaxSlopeDegrees) const
{
	using namespace TeamWeaponQualityPrivate;
	const float SpacingCm = 2.f * ReliefSampleRadiusCm / (ReliefSamplesPerAxis - 1);
	TArray<float> Heights;
	Heights.Init(0.f, ReliefSamplesPerAxis * ReliefSamplesPerAxis);
	float MinHeight = BIG_NUMBER;
	float MaxHeight = -BIG_NUMBER;
	for (int32 IndexX = 0; IndexX < ReliefSamplesPerAxis; ++IndexX)
	{
		for (int32 IndexY = 0; IndexY < ReliefSamplesPerAxis; ++IndexY)
		{
			const FVector SampleLocation = Centre + FVector(-ReliefSampleRadiusCm + IndexX * SpacingCm,
			                                                -ReliefSampleRadiusCm + IndexY * SpacingCm, 0.f);
			float Height = 0.f;
			if (not TryGetGroundZ(SampleLocation, Height))
			{
				OutMaxSlopeDegrees = 90.f;
				return 0.f;
			}
			Heights[IndexX * ReliefSamplesPerAxis + IndexY] = Height;
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}
	}
	OutMaxSlopeDegrees = 0.f;
	for (int32 IndexX = 0; IndexX < ReliefSamplesPerAxis - 1; ++IndexX)
	{
		for (int32 IndexY = 0; IndexY < ReliefSamplesPerAxis - 1; ++IndexY)
		{
			const float Height = Heights[IndexX * ReliefSamplesPerAxis + IndexY];
			const float RiseX = FMath::Abs(Heights[(IndexX + 1) * ReliefSamplesPerAxis + IndexY] - Height);
			const float RiseY = FMath::Abs(Heights[IndexX * ReliefSamplesPerAxis + IndexY + 1] - Height);
			OutMaxSlopeDegrees = FMath::Max(OutMaxSlopeDegrees,
			                                FMath::RadiansToDegrees(FMath::Atan2(FMath::Max(RiseX, RiseY), SpacingCm)));
		}
	}
	return MaxHeight - MinHeight;
}

void UTeamWeaponQualityTestSubsystem::ReadSubjectsFromCommandLine(
	TArray<ESquadSubtype>& OutSquads, TArray<TPair<ETankSubtype, ESquadSubtype>>& OutTows) const
{
	using namespace TeamWeaponQualityPrivate;
	OutSquads.Reset();
	OutTows.Reset();
	FString SquadsText;
	if (FParse::Value(FCommandLine::Get(), SquadsSwitch, SquadsText, false))
	{
		TArray<FString> SquadNames;
		SquadsText.ParseIntoArray(SquadNames, TEXT(","), true);
		for (const FString& SquadName : SquadNames)
		{
			const int64 Value = StaticEnum<ESquadSubtype>()->GetValueByNameString(SquadName);
			if (Value != INDEX_NONE)
			{
				OutSquads.Add(static_cast<ESquadSubtype>(Value));
			}
		}
	}
	else
	{
		OutSquads.Append(DefaultSquads, UE_ARRAY_COUNT(DefaultSquads));
	}

	FString TowsText;
	if (FParse::Value(FCommandLine::Get(), TowsSwitch, TowsText, false))
	{
		TArray<FString> TowPairs;
		TowsText.ParseIntoArray(TowPairs, TEXT(","), true);
		for (const FString& TowPair : TowPairs)
		{
			FString TankName;
			FString SquadName;
			if (not TowPair.Split(TEXT(":"), &TankName, &SquadName))
			{
				continue;
			}
			const int64 TankValue = StaticEnum<ETankSubtype>()->GetValueByNameString(TankName);
			const int64 SquadValue = StaticEnum<ESquadSubtype>()->GetValueByNameString(SquadName);
			if (TankValue != INDEX_NONE && SquadValue != INDEX_NONE)
			{
				OutTows.Emplace(static_cast<ETankSubtype>(TankValue), static_cast<ESquadSubtype>(SquadValue));
			}
		}
	}
	else
	{
		OutTows.Append(DefaultTows, UE_ARRAY_COUNT(DefaultTows));
	}
}

void UTeamWeaponQualityTestSubsystem::PlanAndSpawnSubjects()
{
	using namespace TeamWeaponQualityPrivate;
	TArray<ESquadSubtype> SquadSubtypes;
	TArray<TPair<ETankSubtype, ESquadSubtype>> TowPairs;
	ReadSubjectsFromCommandLine(SquadSubtypes, TowPairs);
	M_SubjectsSpawnedGameSeconds = GetGameSeconds();
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC centre=%s squads=%d tows=%d"),
	       *M_TestCentre.ToCompactString(), SquadSubtypes.Num(), TowPairs.Num());

	// Origins on two rings round the centre so lanes of different subjects do not cross.
	TArray<FVector> Origins;
	for (int32 SquadIndex = 0; SquadIndex < SquadSubtypes.Num(); ++SquadIndex)
	{
		const float Angle = 360.f * SquadIndex / FMath::Max(1, SquadSubtypes.Num());
		Origins.Add(M_TestCentre + FRotator(0.f, Angle, 0.f).Vector() * SquadRingRadiusCm);
	}
	for (int32 TowIndex = 0; TowIndex < TowPairs.Num(); ++TowIndex)
	{
		const float Angle = 90.f + 180.f * TowIndex;
		Origins.Add(M_TestCentre + FRotator(0.f, Angle, 0.f).Vector() * TowRingRadiusCm);
	}
	for (FVector& Origin : Origins)
	{
		float GroundZ = 0.f;
		if (TryGetGroundZ(Origin, GroundZ))
		{
			Origin.Z = GroundZ;
		}
		TryProjectToNavMesh(Origin);
	}

	for (int32 SquadIndex = 0; SquadIndex < SquadSubtypes.Num(); ++SquadIndex)
	{
		FTWQualitySquadSubject Subject;
		Subject.SquadSubtype = SquadSubtypes[SquadIndex];
		Subject.Name = GetSquadName(Subject.SquadSubtype);
		Subject.SpawnLocation = Origins[SquadIndex];
		TArray<FVector> AvoidLocations = Origins;
		AvoidLocations.RemoveAt(SquadIndex);
		FVector LaneEnd;
		if (not TryFindSlopedLane(Subject.SpawnLocation, AvoidLocations, LaneEnd))
		{
			Check(false, Subject.Name, TEXT("found_lane"), Subject.SpawnLocation.ToCompactString());
			continue;
		}
		// Spawned facing away from the lane so the first move starts with a large turn, the second with a U-turn.
		const FVector LaneDirection = GetPlanar(LaneEnd - Subject.SpawnLocation).GetSafeNormal();
		Subject.SpawnRotation = FRotator(0.f, LaneDirection.Rotation().Yaw + 135.f, 0.f);
		Subject.LegDestinations = {LaneEnd, Subject.SpawnLocation};
		M_Squads.Add(Subject);
	}
	for (int32 SquadIndex = 0; SquadIndex < M_Squads.Num(); ++SquadIndex)
	{
		if (not SpawnSquadSubject(M_Squads[SquadIndex], SquadIndex))
		{
			Check(false, M_Squads[SquadIndex].Name, TEXT("spawn_requested"), TEXT(""));
			M_Squads[SquadIndex].Step = ETWQualitySquadStep::Done;
		}
	}

	AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(GetWorld());
	for (int32 TowIndex = 0; TowIndex < TowPairs.Num(); ++TowIndex)
	{
		FTWQualityTowSubject Subject;
		Subject.TankSubtype = TowPairs[TowIndex].Key;
		Subject.SquadSubtype = TowPairs[TowIndex].Value;
		Subject.Name = GetTankName(Subject.TankSubtype) + TEXT("+") + GetSquadName(Subject.SquadSubtype);
		Subject.SpawnLocation = Origins[SquadSubtypes.Num() + TowIndex];
		TArray<FVector> AvoidLocations = Origins;
		AvoidLocations.RemoveAt(SquadSubtypes.Num() + TowIndex);
		FVector FirstEnd;
		const float SavedLegLength = M_LegLengthCm;
		M_LegLengthCm = TowFirstSegmentCm;
		const bool bHasLane = TryFindSlopedLane(Subject.SpawnLocation, AvoidLocations, FirstEnd);
		M_LegLengthCm = SavedLegLength;
		if (not bHasLane || not IsValid(MissionManager))
		{
			Check(false, Subject.Name, TEXT("tow_lane_and_mission_manager"), TEXT(""));
			continue;
		}
		const FVector FirstDirection = GetPlanar(FirstEnd - Subject.SpawnLocation).GetSafeNormal();
		FVector TurnEnd = FirstEnd + FRotator(0.f, TowTurnDegrees, 0.f).RotateVector(FirstDirection) * TowTurnSegmentCm;
		FVector LastEnd = TurnEnd + FRotator(0.f, -TowTurnDegrees * 0.5f, 0.f).RotateVector(FirstDirection) *
			TowTurnSegmentCm;
		Subject.DriveWaypoints.Add(FirstEnd);
		if (TryProjectToNavMesh(TurnEnd))
		{
			Subject.DriveWaypoints.Add(TurnEnd);
			if (TryProjectToNavMesh(LastEnd))
			{
				Subject.DriveWaypoints.Add(LastEnd);
			}
		}
		// The mission spawn faces +X; start the truck on the lane so the first segment needs no reversing.
		Subject.SpawnLocation = Subject.SpawnLocation;
		MissionManager->SpawnTowedTeamWeapon(Subject.TankSubtype, Subject.SquadSubtype, Subject.SpawnLocation);
		Subject.StepStartSeconds = GetGameSeconds();
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC tow_spawn subject=%s at=%s waypoints=%d"),
		       *Subject.Name, *Subject.SpawnLocation.ToCompactString(), Subject.DriveWaypoints.Num());
		M_Tows.Add(Subject);
	}
	bM_HasSpawnedSubjects = true;
}

bool UTeamWeaponQualityTestSubsystem::TryProjectToNavMesh(FVector& InOutLocation) const
{
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation NavLocation;
	if (not IsValid(NavigationSystem) ||
		not NavigationSystem->ProjectPointToNavigation(InOutLocation, NavLocation, TeamWeaponQualityPrivate::NavProjectExtent))
	{
		return false;
	}
	InOutLocation = NavLocation.Location;
	return true;
}

bool UTeamWeaponQualityTestSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace TeamWeaponQualityPrivate;
	UWorld* World = GetWorld();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TeamWeaponQualityGround), false);
	const FVector TraceStart = Location + FVector::UpVector * GroundTraceUpCm;
	const FVector TraceEnd = Location - FVector::UpVector * GroundTraceDownCm;
	// Units block the landscape channel too; step through them to the ground.
	for (int32 Attempt = 0; Attempt < MaxGroundTraceAttempts; ++Attempt)
	{
		FHitResult GroundHit;
		if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			return false;
		}
		if (GetIsUnitActor(GroundHit.GetActor()))
		{
			QueryParams.AddIgnoredActor(GroundHit.GetActor());
			continue;
		}
		OutGroundZ = GroundHit.ImpactPoint.Z;
		return true;
	}
	return false;
}

bool UTeamWeaponQualityTestSubsystem::TryFindSlopedLane(const FVector& Origin, const TArray<FVector>& AvoidLocations,
                                                        FVector& OutLaneEnd) const
{
	using namespace TeamWeaponQualityPrivate;
	UWorld* World = GetWorld();
	float BestScore = -1.f;
	FString BestDescription;
	for (int32 DirectionIndex = 0; DirectionIndex < LaneDirectionCount; ++DirectionIndex)
	{
		const FVector Direction = FRotator(0.f, 360.f * DirectionIndex / LaneDirectionCount, 0.f).Vector();
		FVector LaneEnd = Origin + Direction * M_LegLengthCm;
		if (not TryProjectToNavMesh(LaneEnd))
		{
			continue;
		}
		bool bTooCloseToOthers = false;
		for (const FVector& AvoidLocation : AvoidLocations)
		{
			const FVector LaneMiddle = (Origin + LaneEnd) * 0.5f;
			bTooCloseToOthers |= FVector::Dist2D(LaneEnd, AvoidLocation) < MinDistanceToOtherSubjectsCm ||
				FVector::Dist2D(LaneMiddle, AvoidLocation) < MinDistanceToOtherSubjectsCm;
		}
		if (bTooCloseToOthers)
		{
			continue;
		}
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Origin, LaneEnd);
		if (not IsValid(Path) || not Path->IsValid() || Path->IsPartial() ||
			Path->GetPathLength() > M_LegLengthCm * MaxLanePathStretch)
		{
			continue;
		}
		float MinHeight = BIG_NUMBER;
		float MaxHeight = -BIG_NUMBER;
		float MaxSlopeDegrees = 0.f;
		float PreviousHeight = 0.f;
		bool bHasPrevious = false;
		bool bLaneHasGround = true;
		const int32 SampleCount = FMath::Max(2, FMath::CeilToInt(M_LegLengthCm / LaneSampleSpacingCm));
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const FVector SampleLocation = FMath::Lerp(Origin, LaneEnd, static_cast<float>(SampleIndex) / SampleCount);
			float Height = 0.f;
			if (not TryGetGroundZ(SampleLocation, Height))
			{
				bLaneHasGround = false;
				break;
			}
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
			if (bHasPrevious)
			{
				const float SlopeDegrees = FMath::RadiansToDegrees(
					FMath::Atan2(FMath::Abs(Height - PreviousHeight), M_LegLengthCm / SampleCount));
				MaxSlopeDegrees = FMath::Max(MaxSlopeDegrees, SlopeDegrees);
			}
			PreviousHeight = Height;
			bHasPrevious = true;
		}
		if (not bLaneHasGround || MaxSlopeDegrees > MaxLaneSlopeDegrees)
		{
			continue;
		}
		const float Score = (MaxHeight - MinHeight) + MaxSlopeDegrees * 10.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			OutLaneEnd = LaneEnd;
			BestDescription = FString::Printf(TEXT("dir_yaw=%.0f height_range_cm=%.0f max_slope_deg=%.1f path_cm=%.0f"),
			                                  Direction.Rotation().Yaw, MaxHeight - MinHeight, MaxSlopeDegrees,
			                                  Path->GetPathLength());
		}
	}
	if (BestScore < 0.f)
	{
		return false;
	}
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC lane origin=%s end=%s %s"),
	       *Origin.ToCompactString(), *OutLaneEnd.ToCompactString(), *BestDescription);
	return true;
}

bool UTeamWeaponQualityTestSubsystem::SpawnSquadSubject(FTWQualitySquadSubject& Subject, const int32 SpawnId)
{
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(AsyncSpawner))
	{
		return false;
	}
	const FTrainingOption TrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(Subject.SquadSubtype));
	TWeakObjectPtr<UTeamWeaponQualityTestSubsystem> WeakThis(this);
	Subject.StepStartSeconds = GetGameSeconds();
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC squad_spawn subject=%s at=%s yaw=%.0f"),
	       *Subject.Name, *Subject.SpawnLocation.ToCompactString(), Subject.SpawnRotation.Yaw);
	return AsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption,
		Subject.SpawnLocation,
		this,
		SpawnId,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32 SpawnedId)
		{
			if (not WeakThis.IsValid() || not WeakThis->M_Squads.IsValidIndex(SpawnedId))
			{
				return;
			}
			WeakThis->M_Squads[SpawnedId].Controller = Cast<ATeamWeaponController>(SpawnedActor);
		},
		Subject.SpawnRotation);
}

// ---------------------------------------------------------------------------------------------------------------
// Shared measurements

void UTeamWeaponQualityTestSubsystem::KeepUnitsUnthrottled(AActor* Unit) const
{
	if (not IsValid(Unit))
	{
		return;
	}
	// Unseen units otherwise tick their movement and poses coarsely, which reads as crew lagging behind the gun.
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
	// Mission maps hold enemies; a test subject that dies or loses its crew measures nothing.
	TInlineComponentArray<UHealthComponent*> HealthComponents(Unit);
	for (UHealthComponent* HealthComponent : HealthComponents)
	{
		HealthComponent->AddHealth(TeamWeaponQualityPrivate::KeepAliveHealthPerFrame);
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

void UTeamWeaponQualityTestSubsystem::KeepSquadUnthrottled(ATeamWeaponController& Controller,
                                                            const bool bKeepTurretEnabled) const
{
	for (ASquadUnit* SquadUnit : Controller.GetSquadUnitsChecked())
	{
		KeepUnitsUnthrottled(SquadUnit);
	}
	ATeamWeapon* Weapon = Controller.GetControlledTeamWeaponNoReport();
	KeepUnitsUnthrottled(Weapon);
	// Soviet subjects are enemies of the German ones by their Blueprint owner; engaging each other makes the guns
	// pack and turn on their own, which hides what the test measures.
	if (IsValid(Weapon) && not bKeepTurretEnabled)
	{
		Weapon->DisableTurret();
	}
}

void UTeamWeaponQualityTestSubsystem::SampleGround(const ATeamWeapon& Weapon, FTWQualityGroundMetrics& Metrics) const
{
	using namespace TeamWeaponQualityPrivate;
	float OriginGroundZ = 0.f;
	if (TryGetGroundZ(Weapon.GetActorLocation(), OriginGroundZ))
	{
		Metrics.OriginGapCm.Add(Weapon.GetActorLocation().Z - OriginGroundZ);
	}
	Metrics.TiltDegrees.Add(GetTiltDegrees(Weapon));

	const USkeletalMeshComponent* Mesh = Weapon.FindComponentByClass<USkeletalMeshComponent>();
	if (not IsValid(Mesh))
	{
		return;
	}
	const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(FTransform::Identity);
	const FVector Centre = LocalBounds.Origin;
	const FVector Extent = LocalBounds.BoxExtent * FootprintExtentFraction;
	const FVector2D CornerSigns[] = {{1.f, 1.f}, {1.f, -1.f}, {-1.f, 1.f}, {-1.f, -1.f}};
	float SmallestGap = BIG_NUMBER;
	float DeepestPenetration = 0.f;
	int32 MeasuredCorners = 0;
	for (const FVector2D& Sign : CornerSigns)
	{
		const FVector LocalCorner(Centre.X + Sign.X * Extent.X, Centre.Y + Sign.Y * Extent.Y, 0.f);
		const FVector WorldCorner = Mesh->GetComponentTransform().TransformPosition(LocalCorner);
		float CornerGroundZ = 0.f;
		if (not TryGetGroundZ(WorldCorner, CornerGroundZ))
		{
			continue;
		}
		const float CornerGap = WorldCorner.Z - CornerGroundZ;
		SmallestGap = FMath::Min(SmallestGap, CornerGap);
		DeepestPenetration = FMath::Max(DeepestPenetration, -CornerGap);
		MeasuredCorners++;
	}
	if (MeasuredCorners == 0)
	{
		return;
	}
	Metrics.ContactGapCm.Add(SmallestGap);
	Metrics.PenetrationCm.Add(DeepestPenetration);
}

void UTeamWeaponQualityTestSubsystem::SampleDeployed(ATeamWeaponController& Controller, ATeamWeapon& Weapon,
                                                     FTWQualityDeployedMetrics& Metrics) const
{
	SampleGround(Weapon, Metrics.Ground);
	TArray<ASquadUnit*> Operators;
	Controller.GetCrewOperators(Operators);
	for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
	{
		ASquadUnit* Operator = Operators[OperatorIndex];
		if (not IsValid(Operator) || not Operator->IsUnitAlive())
		{
			continue;
		}
		FVector PostLocation;
		if (Controller.TryGetOperatorCrewPositionLocation(OperatorIndex, PostLocation))
		{
			Metrics.OperatorPostDistanceCm.Add(FVector::Dist2D(Operator->GetActorLocation(), PostLocation));
		}
		float GroundZ = 0.f;
		const UCapsuleComponent* Capsule = Operator->GetCapsuleComponent();
		if (IsValid(Capsule) && TryGetGroundZ(Operator->GetActorLocation(), GroundZ))
		{
			Metrics.OperatorFeetGapCm.Add(Operator->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight() - GroundZ);
		}
	}
	TArray<ASquadUnit*> Guards;
	Controller.GetCrewGuards(Guards);
	for (ASquadUnit* Guard : Guards)
	{
		if (IsValid(Guard) && Guard->IsUnitAlive())
		{
			Metrics.GuardDistanceToWeaponCm.Add(FVector::Dist2D(Guard->GetActorLocation(), Weapon.GetActorLocation()));
		}
	}
}

void UTeamWeaponQualityTestSubsystem::ReportGround(const FString& SubjectName, const FString& Label,
                                                   const FTWQualityGroundMetrics& Metrics, const bool bIsMoving)
{
	using namespace TeamWeaponQualityPrivate;
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s origin_gap_cm[%s]"),
	       *SubjectName, *Label, *Metrics.OriginGapCm.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s contact_gap_cm[%s]"),
	       *SubjectName, *Label, *Metrics.ContactGapCm.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s penetration_cm[%s] tilt_deg[%s]"),
	       *SubjectName, *Label, *Metrics.PenetrationCm.Describe(), *Metrics.TiltDegrees.Describe());
	if (bIsMoving)
	{
		Check(Metrics.OriginGapCm.GetPercentile(0.95f) <= MaxMovingOriginGapP95Cm &&
		      Metrics.OriginGapCm.GetPercentile(0.05f) >= -MaxMovingOriginGapP95Cm,
		      SubjectName, *(Label + TEXT("_origin_on_ground")),
		      FString::Printf(TEXT("p05=%.1f p95=%.1f"), Metrics.OriginGapCm.GetPercentile(0.05f),
		                      Metrics.OriginGapCm.GetPercentile(0.95f)));
		Check(Metrics.ContactGapCm.GetPercentile(0.95f) <= MaxMovingContactGapP95Cm, SubjectName,
		      *(Label + TEXT("_footprint_touches_ground")),
		      FString::Printf(TEXT("p95=%.1f"), Metrics.ContactGapCm.GetPercentile(0.95f)));
	}
	else
	{
		Check(FMath::Abs(Metrics.OriginGapCm.GetMean()) <= MaxDeployedOriginGapCm, SubjectName,
		      *(Label + TEXT("_origin_on_ground")), FString::Printf(TEXT("mean=%.1f"), Metrics.OriginGapCm.GetMean()));
		Check(Metrics.ContactGapCm.GetMean() <= MaxDeployedContactGapCm, SubjectName,
		      *(Label + TEXT("_footprint_touches_ground")),
		      FString::Printf(TEXT("mean=%.1f"), Metrics.ContactGapCm.GetMean()));
	}
	Check(Metrics.PenetrationCm.GetPercentile(0.95f) <= MaxPenetrationCm, SubjectName,
	      *(Label + TEXT("_not_sunk")), FString::Printf(TEXT("p95=%.1f"), Metrics.PenetrationCm.GetPercentile(0.95f)));
}

void UTeamWeaponQualityTestSubsystem::ReportDeployed(const FString& SubjectName, const FString& Label,
                                                     const FTWQualityDeployedMetrics& Metrics)
{
	using namespace TeamWeaponQualityPrivate;
	ReportGround(SubjectName, Label, Metrics.Ground, false);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s operator_post_cm[%s]"),
	       *SubjectName, *Label, *Metrics.OperatorPostDistanceCm.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s operator_feet_gap_cm[%s]"),
	       *SubjectName, *Label, *Metrics.OperatorFeetGapCm.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s guard_distance_cm[%s]"),
	       *SubjectName, *Label, *Metrics.GuardDistanceToWeaponCm.Describe());
	Check(Metrics.OperatorPostDistanceCm.GetHasValues() &&
	      Metrics.OperatorPostDistanceCm.GetMax() <= MaxOperatorPostDistanceCm, SubjectName,
	      *(Label + TEXT("_operators_at_posts")),
	      FString::Printf(TEXT("max=%.1f"), Metrics.OperatorPostDistanceCm.GetMax()));
	Check(Metrics.OperatorFeetGapCm.GetMax() <= MaxOperatorFeetGapCm, SubjectName,
	      *(Label + TEXT("_operators_on_ground")),
	      FString::Printf(TEXT("max=%.1f"), Metrics.OperatorFeetGapCm.GetMax()));
	if (Metrics.GuardDistanceToWeaponCm.GetHasValues())
	{
		// Percentiles, not extremes: a guard walking past the gun to his spot is fine, one standing on it is not.
		const float NearGuardDistanceCm = Metrics.GuardDistanceToWeaponCm.GetPercentile(0.1f);
		const float FarGuardDistanceCm = Metrics.GuardDistanceToWeaponCm.GetPercentile(0.9f);
		Check(NearGuardDistanceCm >= MinGuardDistanceCm && FarGuardDistanceCm <= MaxGuardDistanceCm, SubjectName,
		      *(Label + TEXT("_guards_around_weapon")),
		      FString::Printf(TEXT("p10=%.1f p90=%.1f"), NearGuardDistanceCm, FarGuardDistanceCm));
	}
}

void UTeamWeaponQualityTestSubsystem::LogTeamWeaponConfig(const FString& SubjectName, const ATeamWeapon& Weapon) const
{
	const FTeamWeaponConfig& Config = Weapon.GetTeamWeaponConfig();
	FString MovementTypeName = UEnum::GetValueAsString(Config.M_MovementType);
	UE_LOG(LogRTSTeamWeaponQuality, Display,
	       TEXT("RTS_TWQC config subject=%s class=%s movement=%s push_speed=%.0f turn_speed=%.0f operators=%d deploy_s=%.1f"),
	       *SubjectName, *Weapon.GetClass()->GetName(), *MovementTypeName, Config.M_PushedMoveSpeedCmPerSec,
	       Config.M_TurnSpeedYaw, Config.M_OperatorCount, Config.M_DeploymentTime);
}

// ---------------------------------------------------------------------------------------------------------------
// Squads

void UTeamWeaponQualityTestSubsystem::EnterSquadStep(FTWQualitySquadSubject& Subject, const ETWQualitySquadStep NewStep)
{
	Subject.Step = NewStep;
	Subject.StepStartSeconds = GetGameSeconds();
}

void UTeamWeaponQualityTestSubsystem::TickSquadSubject(FTWQualitySquadSubject& Subject)
{
	using namespace TeamWeaponQualityPrivate;
	if (Subject.Step == ETWQualitySquadStep::Done)
	{
		return;
	}
	const float StepSeconds = GetGameSeconds() - Subject.StepStartSeconds;
	ATeamWeaponController* Controller = Subject.Controller.Get();
	ATeamWeapon* Weapon = IsValid(Controller) ? Controller->GetControlledTeamWeaponNoReport() : nullptr;
	if (Subject.Step == ETWQualitySquadStep::WaitingForSpawn)
	{
		TickSquadWaitingForSpawn(Subject, Weapon, StepSeconds);
		return;
	}
	if (not IsValid(Controller) || not IsValid(Weapon))
	{
		Check(false, Subject.Name, TEXT("squad_and_weapon_alive"), TEXT(""));
		EnterSquadStep(Subject, ETWQualitySquadStep::Done);
		return;
	}
	KeepSquadUnthrottled(*Controller, Subject.Step == ETWQualitySquadStep::Firing);
	if (bM_AuditCrewAnims && not Subject.bIsRegisteredForCrewAnimAudit)
	{
		M_CrewAnimAudit.RegisterSubject(Subject.Name, Subject.SquadSubtype, Controller);
		Subject.bIsRegisteredForCrewAnimAudit = true;
	}
	if (not Subject.bLoggedConfig)
	{
		LogTeamWeaponConfig(Subject.Name, *Weapon);
		Subject.bLoggedConfig = true;
	}
	switch (Subject.Step)
	{
	case ETWQualitySquadStep::WaitingForDeploy:
		TickSquadWaitingForDeploy(Subject, *Controller, StepSeconds);
		break;
	case ETWQualitySquadStep::SettlingDeployed:
		TickSquadSettlingDeployed(Subject, *Controller, *Weapon, StepSeconds);
		break;
	case ETWQualitySquadStep::MeasuringDeployed:
		TickSquadMeasuringDeployed(Subject, *Controller, *Weapon, StepSeconds);
		break;
	case ETWQualitySquadStep::Firing:
		TickSquadFiring(Subject, *Controller, *Weapon, StepSeconds);
		break;
	case ETWQualitySquadStep::Moving:
		TickSquadMoving(Subject, *Controller, *Weapon, StepSeconds);
		break;
	default:
		break;
	}
}

bool UTeamWeaponQualityTestSubsystem::GetIsNotDeployedWithoutOrder(ATeamWeaponController& Controller)
{
	// A final-facing turn after a move is a queued order; only a pack that no command explains is the gun turning itself.
	const EAbilityID ActiveCommand = Controller.GetCurrentActiveCommand();
	const bool bHasNoOrder = ActiveCommand == EAbilityID::IdNoAbility || ActiveCommand == EAbilityID::IdIdle;
	return bHasNoOrder && Controller.GetTeamWeaponState() != ETeamWeaponState::Ready_Deployed;
}

FString UTeamWeaponQualityTestSubsystem::GetSquadDeployedLabel(const FTWQualitySquadSubject& Subject)
{
	return Subject.LegIndex == 0 ? TEXT("deployed_at_spawn") : FString::Printf(TEXT("deployed_after_leg%d"), Subject.LegIndex);
}

void UTeamWeaponQualityTestSubsystem::TickSquadWaitingForSpawn(FTWQualitySquadSubject& Subject, const ATeamWeapon* Weapon,
                                                               const float StepSeconds)
{
	if (IsValid(Weapon))
	{
		EnterSquadStep(Subject, ETWQualitySquadStep::WaitingForDeploy);
		return;
	}
	if (StepSeconds > TeamWeaponQualityPrivate::MaxSpawnGameSeconds)
	{
		Check(false, Subject.Name, TEXT("spawned_with_weapon"), TEXT(""));
		EnterSquadStep(Subject, ETWQualitySquadStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::TickSquadWaitingForDeploy(FTWQualitySquadSubject& Subject,
                                                                const ATeamWeaponController& Controller,
                                                                const float StepSeconds)
{
	if (Controller.GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed)
	{
		EnterSquadStep(Subject, ETWQualitySquadStep::SettlingDeployed);
		return;
	}
	if (StepSeconds > TeamWeaponQualityPrivate::MaxDeployGameSeconds)
	{
		Check(false, Subject.Name, TEXT("deployed"), TeamWeaponQualityPrivate::GetStateName(Controller));
		EnterSquadStep(Subject, ETWQualitySquadStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::TickSquadSettlingDeployed(FTWQualitySquadSubject& Subject,
                                                                ATeamWeaponController& Controller,
                                                                ATeamWeapon& Weapon, const float StepSeconds)
{
	// A deployed gun with no orders and no enemy near must stay deployed; packing here means it turned by itself.
	Subject.UnorderedStateChanges += GetIsNotDeployedWithoutOrder(Controller) ? 1 : 0;
	if (StepSeconds < TeamWeaponQualityPrivate::DeployedSettleSeconds)
	{
		return;
	}
	Subject.Deployed = FTWQualityDeployedMetrics();
	RequestPicture(&Weapon, Subject.Name + TEXT("_") + GetSquadDeployedLabel(Subject));
	EnterSquadStep(Subject, ETWQualitySquadStep::MeasuringDeployed);
}

void UTeamWeaponQualityTestSubsystem::TickSquadMeasuringDeployed(FTWQualitySquadSubject& Subject,
                                                                 ATeamWeaponController& Controller, ATeamWeapon& Weapon,
                                                                 const float StepSeconds)
{
	Subject.UnorderedStateChanges += GetIsNotDeployedWithoutOrder(Controller) ? 1 : 0;
	SampleDeployed(Controller, Weapon, Subject.Deployed);
	if (StepSeconds < TeamWeaponQualityPrivate::DeployedMeasureSeconds)
	{
		return;
	}
	const FString Label = GetSquadDeployedLabel(Subject);
	ReportDeployed(Subject.Name, Label, Subject.Deployed);
	Check(Subject.UnorderedStateChanges == 0, Subject.Name, *(Label + TEXT("_stays_deployed_without_orders")),
	      FString::Printf(TEXT("frames_not_deployed=%d"), Subject.UnorderedStateChanges));
	Subject.UnorderedStateChanges = 0;
	if (bM_AuditCrewAnims && not Subject.bHasFired)
	{
		StartSquadFiring(Subject, Controller, Weapon);
		return;
	}
	OrderNextLeg(Subject);
}

void UTeamWeaponQualityTestSubsystem::StartSquadFiring(FTWQualitySquadSubject& Subject,
                                                       ATeamWeaponController& Controller, ATeamWeapon& Weapon)
{
	using namespace TeamWeaponQualityPrivate;
	Subject.bHasFired = true;
	Subject.ForcedReloads = 0;
	const float TargetDistanceCm = FMath::Max(Controller.GetTeamWeaponRange() * FireTargetRangeFraction,
	                                          MinFireTargetDistanceCm);
	FVector Target = Weapon.GetActorLocation() + Weapon.GetActorForwardVector().GetSafeNormal2D() * TargetDistanceCm;
	float GroundZ = 0.f;
	if (TryGetGroundZ(Target, GroundZ))
	{
		Target.Z = GroundZ;
	}
	Subject.FireTarget = Target;
	// The turret was switched off every frame until now; the attack ground order restarts its targeting.
	const ECommandQueueError Error = Controller.AttackGround(Target, true);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC fire subject=%s target=%s distance=%.0f error=%d"),
	       *Subject.Name, *Target.ToCompactString(), TargetDistanceCm, static_cast<int32>(Error));
	EnterSquadStep(Subject, ETWQualitySquadStep::Firing);
	Subject.NextForcedReloadSeconds = GetGameSeconds() + ForcedReloadAfterSeconds;
}

void UTeamWeaponQualityTestSubsystem::TickSquadFiring(FTWQualitySquadSubject& Subject,
                                                      ATeamWeaponController& Controller, ATeamWeapon& Weapon,
                                                      const float StepSeconds)
{
	using namespace TeamWeaponQualityPrivate;
	const int32 Reloads = M_CrewAnimAudit.GetReloadCount(Subject.Name);
	const bool bIsReloading = M_CrewAnimAudit.GetIsReloading(Subject.Name);
	if ((Reloads >= FiringReloadsWanted && not bIsReloading) || StepSeconds > MaxFiringSeconds)
	{
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC fire_done subject=%s reloads=%d forced=%d seconds=%.1f"),
		       *Subject.Name, Reloads, Subject.ForcedReloads, StepSeconds);
		Controller.StopCommand();
		OrderNextLeg(Subject);
		return;
	}
	// Any reload pushes the forced reload back: only a gun that cannot shoot the ground (minimum range, no line)
	// gets its reloads from the test hook.
	if (bIsReloading)
	{
		Subject.NextForcedReloadSeconds = GetGameSeconds() + ForcedReloadAfterSeconds;
		return;
	}
	if (GetGameSeconds() < Subject.NextForcedReloadSeconds)
	{
		return;
	}
	TArray<UWeaponState*> Weapons = Weapon.GetWeapons();
	if (Weapons.Num() == 0 || not IsValid(Weapons[0]))
	{
		return;
	}
	Weapons[0]->Debug_StartReload();
	Subject.ForcedReloads++;
	Subject.NextForcedReloadSeconds = GetGameSeconds() + ForcedReloadPauseSeconds;
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC fire_forced_reload subject=%s count=%d"),
	       *Subject.Name, Subject.ForcedReloads);
}

void UTeamWeaponQualityTestSubsystem::TickSquadMoving(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller,
                                                      ATeamWeapon& Weapon, const float StepSeconds)
{
	const ETeamWeaponState State = Controller.GetTeamWeaponState();
	if (State != ETeamWeaponState::Ready_Deployed)
	{
		Subject.bLeftDeployedSinceOrder = true;
	}
	if (State == ETeamWeaponState::Moving)
	{
		SampleSquadMove(Subject, Controller, Weapon, Subject.Move, Subject.LastWeaponLocation,
		                Subject.bHasLastWeaponLocation);
	}
	const FString LegLabel = FString::Printf(TEXT("leg%d"), Subject.LegIndex);
	const float DistanceToDestinationCm = FVector::Dist2D(Weapon.GetActorLocation(),
	                                                      Subject.LegDestinations[Subject.LegIndex]);
	if (Subject.bLeftDeployedSinceOrder && State == ETeamWeaponState::Ready_Deployed)
	{
		ReportSquadLeg(Subject.Name, LegLabel, Subject.Move, DistanceToDestinationCm);
		Subject.LegIndex++;
		EnterSquadStep(Subject, ETWQualitySquadStep::SettlingDeployed);
		return;
	}
	if (StepSeconds > TeamWeaponQualityPrivate::MaxLegGameSeconds)
	{
		Check(false, Subject.Name, *(LegLabel + TEXT("_completed")), TeamWeaponQualityPrivate::GetStateName(Controller));
		ReportSquadLeg(Subject.Name, LegLabel, Subject.Move, DistanceToDestinationCm);
		EnterSquadStep(Subject, ETWQualitySquadStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::OrderNextLeg(FTWQualitySquadSubject& Subject)
{
	ATeamWeaponController* Controller = Subject.Controller.Get();
	if (Subject.LegIndex >= Subject.LegDestinations.Num() || not IsValid(Controller))
	{
		EnterSquadStep(Subject, ETWQualitySquadStep::Done);
		return;
	}
	Subject.Move = FTWQualityMoveMetrics();
	Subject.bLeftDeployedSinceOrder = false;
	Subject.bHasLastWeaponLocation = false;
	const FVector Destination = Subject.LegDestinations[Subject.LegIndex];
	const ECommandQueueError Error = Controller->MoveToLocation(Destination, true, FRotator::ZeroRotator);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC order subject=%s leg=%d to=%s error=%d"),
	       *Subject.Name, Subject.LegIndex, *Destination.ToCompactString(), static_cast<int32>(Error));
	EnterSquadStep(Subject, ETWQualitySquadStep::Moving);
}

void UTeamWeaponQualityTestSubsystem::SampleSquadMove(FTWQualitySquadSubject& Subject, ATeamWeaponController& Controller,
                                                      ATeamWeapon& Weapon, FTWQualityMoveMetrics& Metrics,
                                                      FVector& InOutLastWeaponLocation, bool& bInOutHasLast)
{
	using namespace TeamWeaponQualityPrivate;
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	const FVector WeaponLocation = Weapon.GetActorLocation();
	FVector WeaponVelocity = FVector::ZeroVector;
	if (bInOutHasLast && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		WeaponVelocity = GetPlanar(WeaponLocation - InOutLastWeaponLocation) / DeltaSeconds;
	}
	InOutLastWeaponLocation = WeaponLocation;
	bInOutHasLast = true;
	Metrics.MovingSeconds += DeltaSeconds;
	SampleGround(Weapon, Metrics.Ground);
	const float WeaponSpeed = WeaponVelocity.Size();
	Metrics.WeaponSpeedCmPerSec.Add(WeaponSpeed);
	Metrics.StalledSeconds = WeaponSpeed < MovingSpeedThresholdCmPerSec ? Metrics.StalledSeconds + DeltaSeconds : 0.f;
	if (Metrics.StalledSeconds >= Metrics.NextStallLogSeconds)
	{
		Metrics.NextStallLogSeconds += 5.f;
		const UTeamWeaponMover* Mover = Weapon.GetTeamWeaponMover();
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC stall subject=%s stalled_s=%.1f rotating=%d mover[%s]"),
		       *Subject.Name, Metrics.StalledSeconds, Controller.GetIsRotationRequestActive() ? 1 : 0,
		       IsValid(Mover) ? *Mover->GetPushedMoveDebugDescription() : TEXT("none"));
	}
	else if (Metrics.StalledSeconds <= KINDA_SMALL_NUMBER)
	{
		Metrics.NextStallLogSeconds = 3.f;
	}
	const bool bIsWeaponMoving = WeaponSpeed > MovingSpeedThresholdCmPerSec;
	const FVector TravelDirection = bIsWeaponMoving ? WeaponVelocity / WeaponSpeed : FVector::ZeroVector;
	constexpr float PictureMovingSeconds[] = {0.5f, 4.f};
	if (Metrics.PicturesTaken < UE_ARRAY_COUNT(PictureMovingSeconds) &&
		Metrics.MovingSeconds >= PictureMovingSeconds[Metrics.PicturesTaken])
	{
		RequestPicture(&Weapon, Subject.Name + FString::Printf(TEXT("_leg%d_moving_%.0fs"), Subject.LegIndex,
		                                                         Metrics.MovingSeconds));
		Metrics.PicturesTaken++;
	}

	TArray<ASquadUnit*> Operators;
	Controller.GetCrewOperators(Operators);
	for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
	{
		ASquadUnit* Operator = Operators[OperatorIndex];
		if (not IsValid(Operator) || not Operator->IsUnitAlive())
		{
			continue;
		}
		FVector SlotLocation;
		if (Controller.TryGetOperatorCrewPositionLocation(OperatorIndex, SlotLocation))
		{
			Metrics.OperatorSlotDistanceCm.Add(FVector::Dist2D(Operator->GetActorLocation(), SlotLocation));
		}
		if (not bIsWeaponMoving)
		{
			continue;
		}
		Metrics.OperatorSamples++;
		const float AlongTravel = FVector::DotProduct(GetPlanar(Operator->GetActorLocation() - WeaponLocation), TravelDirection);
		Metrics.OperatorAheadSamples += AlongTravel > AheadOfWeaponThresholdCm ? 1 : 0;
	}
	TArray<ASquadUnit*> Guards;
	Controller.GetCrewGuards(Guards);
	for (ASquadUnit* Guard : Guards)
	{
		if (not IsValid(Guard) || not Guard->IsUnitAlive())
		{
			continue;
		}
		Metrics.GuardDistanceToWeaponCm.Add(FVector::Dist2D(Guard->GetActorLocation(), WeaponLocation));
		if (not bIsWeaponMoving)
		{
			continue;
		}
		Metrics.GuardSamples++;
		const float AlongTravel = FVector::DotProduct(GetPlanar(Guard->GetActorLocation() - WeaponLocation), TravelDirection);
		Metrics.GuardAheadSamples += AlongTravel > AheadOfWeaponThresholdCm ? 1 : 0;
	}
}

void UTeamWeaponQualityTestSubsystem::ReportSquadLeg(const FString& SubjectName, const FString& LegLabel,
                                                     const FTWQualityMoveMetrics& Metrics,
                                                     const float DistanceToDestinationCm)
{
	using namespace TeamWeaponQualityPrivate;
	const FString MovingLabel = LegLabel + TEXT("_moving");
	ReportGround(SubjectName, MovingLabel, Metrics.Ground, true);
	const float OperatorAheadFraction = Metrics.OperatorSamples > 0
		                                    ? static_cast<float>(Metrics.OperatorAheadSamples) / Metrics.OperatorSamples
		                                    : 0.f;
	const float GuardAheadFraction = Metrics.GuardSamples > 0
		                                 ? static_cast<float>(Metrics.GuardAheadSamples) / Metrics.GuardSamples
		                                 : 0.f;
	UE_LOG(LogRTSTeamWeaponQuality, Display,
	       TEXT("RTS_TWQC metric subject=%s phase=%s moving_s=%.1f operator_slot_cm[%s] operator_ahead=%d/%d guard_ahead=%d/%d"),
	       *SubjectName, *MovingLabel, Metrics.MovingSeconds, *Metrics.OperatorSlotDistanceCm.Describe(),
	       Metrics.OperatorAheadSamples, Metrics.OperatorSamples, Metrics.GuardAheadSamples, Metrics.GuardSamples);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s weapon_speed[%s] guard_distance_cm[%s]"),
	       *SubjectName, *MovingLabel, *Metrics.WeaponSpeedCmPerSec.Describe(),
	       *Metrics.GuardDistanceToWeaponCm.Describe());
	Check(Metrics.OperatorSlotDistanceCm.GetPercentile(0.9f) <= MaxOperatorSlotDistanceP90Cm, SubjectName,
	      *(MovingLabel + TEXT("_operators_keep_posts")),
	      FString::Printf(TEXT("p90=%.1f"), Metrics.OperatorSlotDistanceCm.GetPercentile(0.9f)));
	Check(Metrics.OperatorSlotDistanceCm.GetMax() <= MaxOperatorSlotDistanceCm, SubjectName,
	      *(MovingLabel + TEXT("_operators_never_far")),
	      FString::Printf(TEXT("max=%.1f"), Metrics.OperatorSlotDistanceCm.GetMax()));
	Check(OperatorAheadFraction <= MaxOperatorAheadFraction, SubjectName,
	      *(MovingLabel + TEXT("_operators_not_leading_gun")),
	      FString::Printf(TEXT("fraction=%.3f"), OperatorAheadFraction));
	Check(GuardAheadFraction <= MaxGuardAheadFraction, SubjectName, *(MovingLabel + TEXT("_guards_not_leading_gun")),
	      FString::Printf(TEXT("fraction=%.3f"), GuardAheadFraction));
	Check(DistanceToDestinationCm <= MaxLegEndDistanceCm, SubjectName, *(LegLabel + TEXT("_arrived")),
	      FString::Printf(TEXT("distance=%.0f"), DistanceToDestinationCm));
}

// ---------------------------------------------------------------------------------------------------------------
// Towing

void UTeamWeaponQualityTestSubsystem::EnterTowStep(FTWQualityTowSubject& Subject, const ETWQualityTowStep NewStep)
{
	Subject.Step = NewStep;
	Subject.StepStartSeconds = GetGameSeconds();
	Subject.bHasLastRelativeRotation = false;
}

bool UTeamWeaponQualityTestSubsystem::TryFindTowedPair(FTWQualityTowSubject& Subject)
{
	constexpr float MaxSearchDistanceCm = 2500.f;
	for (TActorIterator<ATankMaster> It(GetWorld()); It; ++It)
	{
		ATankMaster* Truck = *It;
		if (FVector::Dist2D(Truck->GetActorLocation(), Subject.SpawnLocation) > MaxSearchDistanceCm)
		{
			continue;
		}
		const UVehicleTowComponent* TowComponent = Truck->FindComponentByClass<UVehicleTowComponent>();
		if (not IsValid(TowComponent) || TowComponent->IsTowFree())
		{
			continue;
		}
		ATeamWeapon* Weapon = Cast<ATeamWeapon>(TowComponent->GetTowedActor());
		if (not IsValid(Weapon) || not IsValid(Weapon->GetTeamWeaponController()))
		{
			continue;
		}
		Subject.Truck = Truck;
		Subject.Weapon = Weapon;
		Subject.Controller = Weapon->GetTeamWeaponController();
		return true;
	}
	return false;
}

void UTeamWeaponQualityTestSubsystem::OrderTruckRoute(FTWQualityTowSubject& Subject, const TArray<FVector>& Waypoints) const
{
	ATankMaster* Truck = Subject.Truck.Get();
	if (not IsValid(Truck))
	{
		return;
	}
	for (int32 WaypointIndex = 0; WaypointIndex < Waypoints.Num(); ++WaypointIndex)
	{
		const bool bSetUnitToIdle = WaypointIndex == 0;
		const ECommandQueueError Error = Truck->MoveToLocation(Waypoints[WaypointIndex], bSetUnitToIdle, FRotator::ZeroRotator);
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC order subject=%s truck_waypoint=%d to=%s error=%d"),
		       *Subject.Name, WaypointIndex, *Waypoints[WaypointIndex].ToCompactString(), static_cast<int32>(Error));
	}
}

bool UTeamWeaponQualityTestSubsystem::GetIsTruckIdle(const FTWQualityTowSubject& Subject) const
{
	constexpr float IdleSpeedCmPerSec = 25.f;
	ATankMaster* Truck = Subject.Truck.Get();
	if (not IsValid(Truck))
	{
		return true;
	}
	const EAbilityID ActiveCommand = Truck->GetCurrentActiveCommand();
	const bool bHasNoCommand = ActiveCommand == EAbilityID::IdNoAbility || ActiveCommand == EAbilityID::IdIdle;
	return bHasNoCommand && Truck->GetVelocity().Size2D() < IdleSpeedCmPerSec;
}

void UTeamWeaponQualityTestSubsystem::SampleTow(FTWQualityTowSubject& Subject, FTWQualityTowMetrics& Metrics)
{
	ATeamWeapon* Weapon = Subject.Weapon.Get();
	ATankMaster* Truck = Subject.Truck.Get();
	if (not IsValid(Weapon) || not IsValid(Truck))
	{
		return;
	}
	const USceneComponent* WeaponRoot = Weapon->GetRootComponent();
	const USceneComponent* AttachParent = IsValid(WeaponRoot) ? WeaponRoot->GetAttachParent() : nullptr;
	if (not IsValid(AttachParent))
	{
		return;
	}
	// Hook location in the truck hull's frame: what a player sees the gun do relative to the truck.
	const FTransform HitchFrame(Truck->GetActorQuat(), AttachParent->GetSocketLocation(WeaponRoot->GetAttachSocketName()));
	const FTransform RelativeTransform = Weapon->GetActorTransform().GetRelativeTransform(HitchFrame);
	if (not Subject.bHasRestRelativeTransform)
	{
		Subject.RestRelativeTransform = RelativeTransform;
		Subject.bHasRestRelativeTransform = true;
	}
	const FQuat DeltaQuat = Subject.RestRelativeTransform.GetRotation().Inverse() * RelativeTransform.GetRotation();
	const FRotator DeltaRotation = DeltaQuat.Rotator();
	Metrics.RelativeYawDegrees.Add(DeltaRotation.Yaw);
	Metrics.RelativePitchDegrees.Add(DeltaRotation.Pitch);
	Metrics.RelativeRollDegrees.Add(DeltaRotation.Roll);
	Metrics.HitchDriftCm.Add(FMath::Abs(RelativeTransform.GetLocation().Size() -
		Subject.RestRelativeTransform.GetLocation().Size()));
	float GroundZ = 0.f;
	if (TryGetGroundZ(Weapon->GetActorLocation(), GroundZ))
	{
		const float WheelGapCm = Weapon->GetActorLocation().Z - GroundZ;
		Metrics.WheelGroundGapCm.Add(WheelGapCm);
		if (FMath::Abs(WheelGapCm) > TeamWeaponQualityPrivate::LargeTowWheelGapCm)
		{
			LogLargeTowWheelGap(Subject, *Weapon, *Truck, WheelGapCm);
		}
	}
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	Metrics.TruckSpeedCmPerSec.Add(Truck->GetVelocity().Size2D());
	if (Subject.bHasLastRelativeRotation && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Metrics.YawRateDegreesPerSec.Add(FMath::Abs(FMath::FindDeltaAngleDegrees(
			Subject.LastRelativeRotation.Yaw, DeltaRotation.Yaw)) / DeltaSeconds);
		Metrics.PitchRateDegreesPerSec.Add(FMath::Max(
			FMath::Abs(FMath::FindDeltaAngleDegrees(Subject.LastRelativeRotation.Pitch, DeltaRotation.Pitch)),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Subject.LastRelativeRotation.Roll, DeltaRotation.Roll))) / DeltaSeconds);
		Metrics.TruckYawRateDegreesPerSec.Add(FMath::Abs(FMath::FindDeltaAngleDegrees(
			Subject.LastTruckYaw, Truck->GetActorRotation().Yaw)) / DeltaSeconds);
	}
	Subject.LastRelativeRotation = DeltaRotation;
	Subject.LastTruckYaw = Truck->GetActorRotation().Yaw;
	Subject.bHasLastRelativeRotation = true;
}

void UTeamWeaponQualityTestSubsystem::LogLargeTowWheelGap(const FTWQualityTowSubject& Subject, const ATeamWeapon& Weapon,
                                                          const ATankMaster& Truck, const float WheelGapCm) const
{
	// What the weapon's own ground trace stands on; the test's trace skips every RTS unit, the weapon only pawns.
	FHitResult WeaponTraceHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TeamWeaponQualityGapProbe), false, &Weapon);
	QueryParams.AddIgnoredActor(&Truck);
	const FVector ProbeStart = Weapon.GetActorLocation() + FVector(0.f, 0.f, TeamWeaponQualityPrivate::GroundTraceUpCm);
	const FVector ProbeEnd = Weapon.GetActorLocation() - FVector(0.f, 0.f, TeamWeaponQualityPrivate::GroundTraceDownCm);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(WeaponTraceHit, ProbeStart, ProbeEnd, COLLISION_TRACE_LANDSCAPE,
	                                                       QueryParams);
	float GameGroundZ = 0.f;
	const bool bGameGround = Weapon.TryGetGroundHeightAt(Weapon.GetActorLocation(), GameGroundZ);
	UE_LOG(LogRTSTeamWeaponQuality, Display,
	       TEXT("RTS_TWQC tow_gap subject=%s gap_cm=%.1f weapon=%s first_hit=%s at_z=%.0f game_ground=%d z=%.0f truck_pitch=%.1f truck_speed=%.0f"),
	       *Subject.Name, WheelGapCm, *Weapon.GetActorLocation().ToCompactString(),
	       bHit && IsValid(WeaponTraceHit.GetActor()) ? *WeaponTraceHit.GetActor()->GetName() : TEXT("none"),
	       bHit ? WeaponTraceHit.ImpactPoint.Z : 0.f, bGameGround ? 1 : 0, GameGroundZ, Truck.GetActorRotation().Pitch,
	       Truck.GetVelocity().Size());
}

void UTeamWeaponQualityTestSubsystem::ReportTow(const FString& SubjectName, const FString& Label,
                                                const FTWQualityTowMetrics& Metrics, const bool bExpectSway)
{
	using namespace TeamWeaponQualityPrivate;
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s rel_yaw_deg[%s]"),
	       *SubjectName, *Label, *Metrics.RelativeYawDegrees.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s rel_pitch_deg[%s]"),
	       *SubjectName, *Label, *Metrics.RelativePitchDegrees.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s rel_roll_deg[%s]"),
	       *SubjectName, *Label, *Metrics.RelativeRollDegrees.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s yaw_rate[%s] pitch_roll_rate[%s]"),
	       *SubjectName, *Label, *Metrics.YawRateDegreesPerSec.Describe(), *Metrics.PitchRateDegreesPerSec.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s hitch_drift_cm[%s] wheel_gap_cm[%s]"),
	       *SubjectName, *Label, *Metrics.HitchDriftCm.Describe(), *Metrics.WheelGroundGapCm.Describe());
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC metric subject=%s phase=%s truck_speed[%s] truck_yaw_rate[%s]"),
	       *SubjectName, *Label, *Metrics.TruckSpeedCmPerSec.Describe(), *Metrics.TruckYawRateDegreesPerSec.Describe());
	if (not Metrics.RelativeYawDegrees.GetHasValues())
	{
		Check(false, SubjectName, *(Label + TEXT("_sampled")), TEXT(""));
		return;
	}
	const float MaxAbsYaw = FMath::Max(FMath::Abs(Metrics.RelativeYawDegrees.GetMin()),
	                                   FMath::Abs(Metrics.RelativeYawDegrees.GetMax()));
	const float MaxAbsPitch = FMath::Max(FMath::Abs(Metrics.RelativePitchDegrees.GetMin()),
	                                     FMath::Abs(Metrics.RelativePitchDegrees.GetMax()));
	const float MaxAbsRoll = FMath::Max(FMath::Abs(Metrics.RelativeRollDegrees.GetMin()),
	                                    FMath::Abs(Metrics.RelativeRollDegrees.GetMax()));
	Check(MaxAbsYaw <= MaxTowYawDegrees && MaxAbsPitch <= MaxTowPitchRollDegrees && MaxAbsRoll <= MaxTowPitchRollDegrees,
	      SubjectName, *(Label + TEXT("_sway_bounded")),
	      FString::Printf(TEXT("yaw=%.1f pitch=%.1f roll=%.1f"), MaxAbsYaw, MaxAbsPitch, MaxAbsRoll));
	Check(Metrics.HitchDriftCm.GetMax() <= MaxTowHitchDriftCm, SubjectName, *(Label + TEXT("_stays_on_hitch")),
	      FString::Printf(TEXT("max=%.1f"), Metrics.HitchDriftCm.GetMax()));
	if (not bExpectSway)
	{
		Check(Metrics.YawRateDegreesPerSec.GetPercentile(0.9f) <= MaxStoppedRateDegreesPerSec &&
		      Metrics.PitchRateDegreesPerSec.GetPercentile(0.9f) <= MaxStoppedRateDegreesPerSec,
		      SubjectName, *(Label + TEXT("_settles")),
		      FString::Printf(TEXT("yaw_rate_p90=%.1f pitch_rate_p90=%.1f"),
		                      Metrics.YawRateDegreesPerSec.GetPercentile(0.9f),
		                      Metrics.PitchRateDegreesPerSec.GetPercentile(0.9f)));
		return;
	}
	Check(MaxAbsYaw >= MinTowSwayYawDegrees, SubjectName, *(Label + TEXT("_swings_in_turns")),
	      FString::Printf(TEXT("max_abs_yaw=%.1f"), MaxAbsYaw));
	Check(FMath::Max(MaxAbsPitch, MaxAbsRoll) >= MinTowSwayPitchOrRollDegrees, SubjectName,
	      *(Label + TEXT("_reacts_to_ground")), FString::Printf(TEXT("pitch=%.1f roll=%.1f"), MaxAbsPitch, MaxAbsRoll));
	Check(Metrics.YawRateDegreesPerSec.GetPercentile(0.95f) <= MaxTowRateP95DegreesPerSec &&
	      Metrics.PitchRateDegreesPerSec.GetPercentile(0.95f) <= MaxTowRateP95DegreesPerSec,
	      SubjectName, *(Label + TEXT("_no_jitter")),
	      FString::Printf(TEXT("yaw_rate_p95=%.1f pitch_rate_p95=%.1f"),
	                      Metrics.YawRateDegreesPerSec.GetPercentile(0.95f),
	                      Metrics.PitchRateDegreesPerSec.GetPercentile(0.95f)));
	Check(Metrics.WheelGroundGapCm.GetPercentile(0.9f) <= MaxTowWheelGapP90Cm &&
	      Metrics.WheelGroundGapCm.GetPercentile(0.1f) >= -MaxTowWheelGapP90Cm, SubjectName,
	      *(Label + TEXT("_wheels_on_ground")),
	      FString::Printf(TEXT("p10=%.1f p90=%.1f"), Metrics.WheelGroundGapCm.GetPercentile(0.1f),
	                      Metrics.WheelGroundGapCm.GetPercentile(0.9f)));
}

void UTeamWeaponQualityTestSubsystem::LogTowStatusChanges(FTWQualityTowSubject& Subject) const
{
	const ATankMaster* Truck = Subject.Truck.Get();
	const ATeamWeaponController* Controller = Subject.Controller.Get();
	const ATeamWeapon* Weapon = Subject.Weapon.Get();
	const UVehicleTowComponent* TowComponent = IsValid(Truck) ? Truck->FindComponentByClass<UVehicleTowComponent>() : nullptr;
	const USceneComponent* TowMesh = IsValid(TowComponent) ? TowComponent->GetTowMeshComponent() : nullptr;
	const FString Status = FString::Printf(TEXT("state=%s attached_to=%s truck_tow_free=%d tow_mesh=%s"),
	                                       IsValid(Controller) ? *TeamWeaponQualityPrivate::GetStateName(*Controller) : TEXT("-"),
	                                       IsValid(Weapon) && IsValid(Weapon->GetAttachParentActor())
		                                       ? *Weapon->GetAttachParentActor()->GetName()
		                                       : TEXT("none"),
	                                       IsValid(TowComponent) && TowComponent->IsTowFree() ? 1 : 0,
	                                       IsValid(TowMesh) ? *TowMesh->GetName() : TEXT("-"));
	if (Status == Subject.LastLoggedTowStatus)
	{
		return;
	}
	Subject.LastLoggedTowStatus = Status;
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC tow_status subject=%s %s (t=%.1f)"), *Subject.Name, *Status,
	       GetGameSeconds());
}

void UTeamWeaponQualityTestSubsystem::TickTowSubject(FTWQualityTowSubject& Subject)
{
	if (Subject.Step == ETWQualityTowStep::Done)
	{
		return;
	}
	const float StepSeconds = GetGameSeconds() - Subject.StepStartSeconds;
	if (Subject.Step == ETWQualityTowStep::WaitingForSpawn)
	{
		TickTowWaitingForSpawn(Subject, StepSeconds);
		return;
	}
	FTWQualityTowFrame Frame;
	Frame.Truck = Subject.Truck.Get();
	Frame.Controller = Subject.Controller.Get();
	Frame.Weapon = Subject.Weapon.Get();
	Frame.StepSeconds = StepSeconds;
	if (not IsValid(Frame.Truck) || not IsValid(Frame.Controller) || not IsValid(Frame.Weapon))
	{
		Check(false, Subject.Name, TEXT("truck_and_weapon_alive"), TEXT(""));
		EnterTowStep(Subject, ETWQualityTowStep::Done);
		return;
	}
	KeepUnitsUnthrottled(Frame.Truck);
	KeepSquadUnthrottled(*Frame.Controller, false);
	LogTowStatusChanges(Subject);
	Frame.State = Frame.Controller->GetTeamWeaponState();
	switch (Subject.Step)
	{
	case ETWQualityTowStep::SettlingTowed:
		TickTowSettlingTowed(Subject, Frame);
		break;
	case ETWQualityTowStep::Driving:
		TickTowDriving(Subject, Frame);
		break;
	case ETWQualityTowStep::SettlingStopped:
		TickTowSettlingStopped(Subject, Frame);
		break;
	case ETWQualityTowStep::WaitingForDetachDeploy:
		TickTowWaitingForDetachDeploy(Subject, Frame);
		break;
	case ETWQualityTowStep::SettlingDetached:
		TickTowSettlingDeployed(Subject, Frame, Subject.DetachedDeployed, ETWQualityTowStep::MeasuringDetached);
		break;
	case ETWQualityTowStep::MeasuringDetached:
		TickTowMeasuringDetached(Subject, Frame);
		break;
	case ETWQualityTowStep::MovingDetachedGun:
		TickTowMovingDetachedGun(Subject, Frame);
		break;
	case ETWQualityTowStep::SettlingAfterGunMove:
		TickTowSettlingDeployed(Subject, Frame, Subject.AfterGunMove, ETWQualityTowStep::MeasuringAfterGunMove);
		break;
	case ETWQualityTowStep::MeasuringAfterGunMove:
		TickTowMeasuringAfterGunMove(Subject, Frame);
		break;
	case ETWQualityTowStep::WaitingForRetow:
		TickTowWaitingForRetow(Subject, Frame);
		break;
	case ETWQualityTowStep::SettlingRetowed:
		TickTowSettlingRetowed(Subject, Frame);
		break;
	case ETWQualityTowStep::RetowDriving:
		TickTowRetowDriving(Subject, Frame);
		break;
	default:
		break;
	}
}

void UTeamWeaponQualityTestSubsystem::TickTowWaitingForSpawn(FTWQualityTowSubject& Subject, const float StepSeconds)
{
	if (TryFindTowedPair(Subject))
	{
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC tow_found subject=%s truck=%s weapon=%s"),
		       *Subject.Name, *Subject.Truck->GetName(), *Subject.Weapon->GetName());
		EnterTowStep(Subject, ETWQualityTowStep::SettlingTowed);
		return;
	}
	if (StepSeconds > TeamWeaponQualityPrivate::MaxSpawnGameSeconds)
	{
		Check(false, Subject.Name, TEXT("spawned_towed"), TEXT(""));
		EnterTowStep(Subject, ETWQualityTowStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::TickTowSettlingTowed(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	if (Frame.StepSeconds < TeamWeaponQualityPrivate::TowedSettleSeconds)
	{
		return;
	}
	LogTeamWeaponConfig(Subject.Name, *Frame.Weapon);
	Check(Frame.State == ETeamWeaponState::Towed && Frame.Weapon->GetAttachParentActor() == Frame.Truck, Subject.Name,
	      TEXT("towed_after_spawn"), TeamWeaponQualityPrivate::GetStateName(*Frame.Controller));
	// The first sample only takes the rest pose the sway is measured against.
	SampleTow(Subject, Subject.Drive);
	Subject.Drive = FTWQualityTowMetrics();
	RequestPicture(Frame.Truck, Subject.Name + TEXT("_towed_at_spawn"));
	OrderTruckRoute(Subject, Subject.DriveWaypoints);
	EnterTowStep(Subject, ETWQualityTowStep::Driving);
}

void UTeamWeaponQualityTestSubsystem::TickTowDriving(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	using namespace TeamWeaponQualityPrivate;
	SampleTow(Subject, Subject.Drive);
	constexpr float PictureDriveSeconds[] = {4.f, 8.f, 12.f};
	if (Subject.DrivePicturesTaken < UE_ARRAY_COUNT(PictureDriveSeconds) &&
		Frame.StepSeconds >= PictureDriveSeconds[Subject.DrivePicturesTaken])
	{
		RequestPicture(Frame.Truck, Subject.Name + FString::Printf(TEXT("_towing_%.0fs"), Frame.StepSeconds));
		Subject.DrivePicturesTaken++;
	}
	const bool bArrived = Frame.StepSeconds > MinDriveSecondsBeforeIdle && GetIsTruckIdle(Subject);
	if (not bArrived && Frame.StepSeconds <= MaxTowDriveGameSeconds)
	{
		return;
	}
	ReportTow(Subject.Name, TEXT("tow_drive"), Subject.Drive, true);
	Check(bArrived && FVector::Dist2D(Frame.Truck->GetActorLocation(), Subject.DriveWaypoints.Last()) < MaxTruckArrivalCm,
	      Subject.Name, TEXT("tow_drive_arrived"), Frame.Truck->GetActorLocation().ToCompactString());
	Subject.Stopped = FTWQualityTowMetrics();
	EnterTowStep(Subject, ETWQualityTowStep::SettlingStopped);
}

void UTeamWeaponQualityTestSubsystem::TickTowSettlingStopped(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	using namespace TeamWeaponQualityPrivate;
	if (Frame.StepSeconds >= StoppedSettleSeconds - StoppedMeasureSeconds)
	{
		SampleTow(Subject, Subject.Stopped);
	}
	if (Frame.StepSeconds < StoppedSettleSeconds)
	{
		return;
	}
	ReportTow(Subject.Name, TEXT("tow_stopped"), Subject.Stopped, false);
	RequestPicture(Frame.Truck, Subject.Name + TEXT("_tow_stopped"));
	const bool bExitCargo = FParse::Param(FCommandLine::Get(), ExitCargoSwitch);
	const ECommandQueueError Error = bExitCargo ? Frame.Truck->ExitCargo(true) : Frame.Truck->DetachTow(true);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC order subject=%s %s error=%d"),
	       *Subject.Name, bExitCargo ? TEXT("exit_cargo") : TEXT("detach"), static_cast<int32>(Error));
	if (bExitCargo)
	{
		const UCargo* const TruckCargo = Frame.Truck->FindComponentByClass<UCargo>();
		const UCargoSquad* const CrewCargo = Frame.Controller->FindComponentByClass<UCargoSquad>();
		const UVehicleTowComponent* const VehicleTow = Frame.Truck->FindComponentByClass<UVehicleTowComponent>();
		Check(Error == ECommandQueueError::NoError, Subject.Name, TEXT("exit_cargo_accepted"), TEXT(""));
		Check(Frame.Weapon->GetAttachParentActor() == nullptr, Subject.Name, TEXT("exit_cargo_detaches_gun"), TEXT(""));
		Check(IsValid(TruckCargo) && TruckCargo->GetIsEmpty(), Subject.Name, TEXT("exit_cargo_empties_truck"), TEXT(""));
		Check(IsValid(CrewCargo) && not CrewCargo->GetIsInsideCargo(), Subject.Name,
		      TEXT("exit_cargo_releases_crew"), TEXT(""));
		Check(IsValid(VehicleTow) && VehicleTow->IsTowFree(), Subject.Name, TEXT("exit_cargo_clears_tow"), TEXT(""));
	}
	EnterTowStep(Subject, ETWQualityTowStep::WaitingForDetachDeploy);
}

void UTeamWeaponQualityTestSubsystem::TickTowWaitingForDetachDeploy(FTWQualityTowSubject& Subject,
                                                                    const FTWQualityTowFrame& Frame)
{
	if (Frame.State == ETeamWeaponState::Ready_Deployed)
	{
		Check(Frame.Weapon->GetAttachParentActor() == nullptr, Subject.Name, TEXT("detached_from_truck"), TEXT(""));
		EnterTowStep(Subject, ETWQualityTowStep::SettlingDetached);
		return;
	}
	if (Frame.StepSeconds > TeamWeaponQualityPrivate::MaxDeployGameSeconds)
	{
		Check(false, Subject.Name, TEXT("deployed_after_detach"), TeamWeaponQualityPrivate::GetStateName(*Frame.Controller));
		EnterTowStep(Subject, ETWQualityTowStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::TickTowSettlingDeployed(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame,
                                                              FTWQualityDeployedMetrics& OutMetrics,
                                                              const ETWQualityTowStep MeasuringStep)
{
	// No order is pending here: a gun that packs now turned or moved by itself.
	Subject.UnorderedStateChanges += GetIsNotDeployedWithoutOrder(*Frame.Controller) ? 1 : 0;
	if (Frame.StepSeconds < TeamWeaponQualityPrivate::DeployedSettleSeconds)
	{
		return;
	}
	OutMetrics = FTWQualityDeployedMetrics();
	if (MeasuringStep == ETWQualityTowStep::MeasuringDetached)
	{
		RequestPicture(Frame.Weapon, Subject.Name + TEXT("_deployed_after_detach"));
	}
	EnterTowStep(Subject, MeasuringStep);
}

void UTeamWeaponQualityTestSubsystem::ReportTowDeployedWindow(FTWQualityTowSubject& Subject, const TCHAR* Label,
                                                              const FTWQualityDeployedMetrics& Metrics)
{
	ReportDeployed(Subject.Name, Label, Metrics);
	Check(Subject.UnorderedStateChanges == 0, Subject.Name, *(FString(Label) + TEXT("_stays_deployed_without_orders")),
	      FString::Printf(TEXT("frames_not_deployed=%d"), Subject.UnorderedStateChanges));
	Subject.UnorderedStateChanges = 0;
}

void UTeamWeaponQualityTestSubsystem::TickTowMeasuringDetached(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	Subject.UnorderedStateChanges += GetIsNotDeployedWithoutOrder(*Frame.Controller) ? 1 : 0;
	SampleDeployed(*Frame.Controller, *Frame.Weapon, Subject.DetachedDeployed);
	if (Frame.StepSeconds < TeamWeaponQualityPrivate::DeployedMeasureSeconds)
	{
		return;
	}
	ReportTowDeployedWindow(Subject, TEXT("deployed_after_detach"), Subject.DetachedDeployed);
	const FVector Side = FRotator(0.f, 90.f, 0.f).RotateVector(Frame.Truck->GetActorForwardVector());
	Subject.GunMoveDestination = Frame.Weapon->GetActorLocation() + Side * TeamWeaponQualityPrivate::GunMoveAfterDetachCm;
	TryProjectToNavMesh(Subject.GunMoveDestination);
	Subject.GunMove = FTWQualityMoveMetrics();
	Subject.bHasLastWeaponLocation = false;
	Subject.bLeftDeployedSinceOrder = false;
	const ECommandQueueError Error = Frame.Controller->MoveToLocation(Subject.GunMoveDestination, true,
	                                                                  FRotator::ZeroRotator);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC order subject=%s gun_move to=%s error=%d"),
	       *Subject.Name, *Subject.GunMoveDestination.ToCompactString(), static_cast<int32>(Error));
	EnterTowStep(Subject, ETWQualityTowStep::MovingDetachedGun);
}

void UTeamWeaponQualityTestSubsystem::TickTowMovingDetachedGun(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	if (Frame.State != ETeamWeaponState::Ready_Deployed)
	{
		Subject.bLeftDeployedSinceOrder = true;
	}
	if (Frame.State == ETeamWeaponState::Moving)
	{
		FTWQualitySquadSubject PictureNameSource;
		PictureNameSource.Name = Subject.Name;
		PictureNameSource.LegIndex = TeamWeaponQualityPrivate::DetachedGunMoveLegIndex;
		SampleSquadMove(PictureNameSource, *Frame.Controller, *Frame.Weapon, Subject.GunMove, Subject.LastWeaponLocation,
		                Subject.bHasLastWeaponLocation);
	}
	if (Subject.bLeftDeployedSinceOrder && Frame.State == ETeamWeaponState::Ready_Deployed)
	{
		ReportSquadLeg(Subject.Name, TEXT("gun_move_after_detach"), Subject.GunMove,
		               FVector::Dist2D(Frame.Weapon->GetActorLocation(), Subject.GunMoveDestination));
		EnterTowStep(Subject, ETWQualityTowStep::SettlingAfterGunMove);
		return;
	}
	if (Frame.State == ETeamWeaponState::Ready_Packed && Frame.StepSeconds >= Subject.NextOrderDebugLogSeconds)
	{
		// Packed and not moving although a move was ordered: show what the controller still has pending.
		Subject.NextOrderDebugLogSeconds = Frame.StepSeconds + TeamWeaponQualityPrivate::OrderDebugLogIntervalSeconds;
		UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC order_debug subject=%s active_command=%s %s"),
		       *Subject.Name, *UEnum::GetValueAsString(Frame.Controller->GetCurrentActiveCommand()),
		       *Frame.Controller->GetOrderDebugDescription());
	}
	if (Frame.StepSeconds > TeamWeaponQualityPrivate::MaxLegGameSeconds)
	{
		Check(false, Subject.Name, TEXT("gun_move_after_detach_completed"),
		      TeamWeaponQualityPrivate::GetStateName(*Frame.Controller));
		EnterTowStep(Subject, ETWQualityTowStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::TickTowMeasuringAfterGunMove(FTWQualityTowSubject& Subject,
                                                                   const FTWQualityTowFrame& Frame)
{
	Subject.UnorderedStateChanges += GetIsNotDeployedWithoutOrder(*Frame.Controller) ? 1 : 0;
	SampleDeployed(*Frame.Controller, *Frame.Weapon, Subject.AfterGunMove);
	if (Frame.StepSeconds < TeamWeaponQualityPrivate::DeployedMeasureSeconds)
	{
		return;
	}
	ReportTowDeployedWindow(Subject, TEXT("deployed_after_gun_move"), Subject.AfterGunMove);
	Subject.RetowOrderWeaponLocation = Frame.Weapon->GetActorLocation();
	Subject.NextRetowDebugLogSeconds = 0.f;
	const ECommandQueueError Error = Frame.Truck->TowActor(Frame.Weapon, ETowedActorTarget::TowTeamWeapon, true);
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC order subject=%s retow error=%d"),
	       *Subject.Name, static_cast<int32>(Error));
	EnterTowStep(Subject, ETWQualityTowStep::WaitingForRetow);
}

void UTeamWeaponQualityTestSubsystem::TickTowWaitingForRetow(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	if (Frame.StepSeconds >= Subject.NextRetowDebugLogSeconds)
	{
		Subject.NextRetowDebugLogSeconds = Frame.StepSeconds + TeamWeaponQualityPrivate::RetowDebugLogIntervalSeconds;
		LogRetowApproach(Subject, Frame);
	}
	if (Frame.State == ETeamWeaponState::Towed && Frame.Weapon->GetAttachParentActor() == Frame.Truck)
	{
		Check(true, Subject.Name, TEXT("retowed"), TEXT(""));
		EnterTowStep(Subject, ETWQualityTowStep::SettlingRetowed);
		return;
	}
	if (Frame.StepSeconds > TeamWeaponQualityPrivate::MaxTowDriveGameSeconds)
	{
		Check(false, Subject.Name, TEXT("retowed"), TeamWeaponQualityPrivate::GetStateName(*Frame.Controller));
		EnterTowStep(Subject, ETWQualityTowStep::Done);
	}
}

void UTeamWeaponQualityTestSubsystem::LogRetowApproach(const FTWQualityTowSubject& Subject,
                                                       const FTWQualityTowFrame& Frame) const
{
	UCommandData* const TruckCommandData = Frame.Truck->GetIsValidCommandData();
	const EAbilityID TruckCommand = IsValid(TruckCommandData)
		                                ? TruckCommandData->GetCurrentlyActiveCommandType()
		                                : EAbilityID::IdNoAbility;
	const AAITankMaster* const TruckController = Frame.Truck->GetAIController();
	const UPathFollowingComponent* const PathFollowing = IsValid(TruckController)
		                                                     ? TruckController->GetPathFollowingComponent()
		                                                     : nullptr;
	const int32 MoveStatus = IsValid(PathFollowing) ? static_cast<int32>(PathFollowing->GetStatus()) : -1;
	const FVector PathDestination = IsValid(PathFollowing) ? PathFollowing->GetPathDestination() : FVector::ZeroVector;
	UE_LOG(LogRTSTeamWeaponQuality, Display,
	       TEXT("RTS_TWQC retow_debug subject=%s dist2d=%.0f truck_speed=%.0f truck_command=%s move_status=%d "
		       "path_dest_to_gun=%.0f truck=%s gun=%s gun_moved=%.0f gun_state=%s (t=%.1f)"),
	       *Subject.Name, FVector::Dist2D(Frame.Truck->GetActorLocation(), Frame.Weapon->GetActorLocation()),
	       Frame.Truck->GetVelocity().Size2D(), *UEnum::GetValueAsString(TruckCommand), MoveStatus,
	       FVector::Dist2D(PathDestination, Frame.Weapon->GetActorLocation()),
	       *Frame.Truck->GetActorLocation().ToCompactString(), *Frame.Weapon->GetActorLocation().ToCompactString(),
	       FVector::Dist2D(Frame.Weapon->GetActorLocation(), Subject.RetowOrderWeaponLocation),
	       *TeamWeaponQualityPrivate::GetStateName(*Frame.Controller), Frame.StepSeconds);
}

void UTeamWeaponQualityTestSubsystem::TickTowSettlingRetowed(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	using namespace TeamWeaponQualityPrivate;
	// The sway settles the hooked gun onto its trail and wheels first; that pose is the rest to measure from.
	if (Frame.StepSeconds < TowedSettleSeconds)
	{
		return;
	}
	Subject.bHasRestRelativeTransform = false;
	Subject.Retow = FTWQualityTowMetrics();
	SampleTow(Subject, Subject.Retow);
	Subject.Retow = FTWQualityTowMetrics();
	Subject.RetowDriveDestination = Frame.Truck->GetActorLocation() +
		FRotator(0.f, RetowDriveTurnDegrees, 0.f).RotateVector(Frame.Truck->GetActorForwardVector()) * RetowDriveCm;
	TryProjectToNavMesh(Subject.RetowDriveDestination);
	OrderTruckRoute(Subject, {Subject.RetowDriveDestination});
	EnterTowStep(Subject, ETWQualityTowStep::RetowDriving);
}

void UTeamWeaponQualityTestSubsystem::TickTowRetowDriving(FTWQualityTowSubject& Subject, const FTWQualityTowFrame& Frame)
{
	using namespace TeamWeaponQualityPrivate;
	SampleTow(Subject, Subject.Retow);
	const bool bArrived = Frame.StepSeconds > MinDriveSecondsBeforeIdle && GetIsTruckIdle(Subject);
	if (not bArrived && Frame.StepSeconds <= MaxTowDriveGameSeconds)
	{
		return;
	}
	ReportTow(Subject.Name, TEXT("retow_drive"), Subject.Retow, true);
	RequestPicture(Frame.Truck, Subject.Name + TEXT("_retow_end"));
	EnterTowStep(Subject, ETWQualityTowStep::Done);
}

// ---------------------------------------------------------------------------------------------------------------
// Pictures

bool UTeamWeaponQualityTestSubsystem::CreateCaptureRig()
{
	using namespace TeamWeaponQualityPrivate;
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
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the units, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	return true;
}

void UTeamWeaponQualityTestSubsystem::RequestPicture(AActor* Focus, const FString& Label)
{
	if (not bM_TakePictures || not IsValid(Focus))
	{
		return;
	}
	FTWQualityPictureRequest Request;
	Request.Focus = Focus;
	Request.Label = FString::Printf(TEXT("%02d_%s"), M_PictureIndex++, *Label);
	M_Pictures.Add(Request);
}

void UTeamWeaponQualityTestSubsystem::TickPictures()
{
	using namespace TeamWeaponQualityPrivate;
	if (M_Pictures.Num() == 0)
	{
		return;
	}
	ASceneCapture2D* Capture = M_Capture.Get();
	AActor* Focus = M_Pictures[0].Focus.Get();
	if (not IsValid(Capture) || not IsValid(Focus) || not IsValid(M_RenderTarget))
	{
		M_Pictures.RemoveAt(0);
		return;
	}
	const FVector LookAt = Focus->GetActorLocation() + PictureLookAtOffset;
	FVector CameraLocation = LookAt + FRotator(0.f, Focus->GetActorRotation().Yaw, 0.f).RotateVector(PictureCameraOffset);
	// On a slope the fixed offset can end inside the hill; keep the camera above the ground it stands over.
	float GroundBelowCameraZ = 0.f;
	if (TryGetGroundZ(CameraLocation, GroundBelowCameraZ))
	{
		CameraLocation.Z = FMath::Max(CameraLocation.Z, GroundBelowCameraZ + PictureCameraClearanceCm);
	}
	Capture->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());
	if (++M_Pictures[0].FramesAimed < PictureAimFrames)
	{
		return;
	}
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory,
	                                            M_Pictures[0].Label + TEXT(".png"));
	UE_LOG(LogRTSTeamWeaponQuality, Display, TEXT("RTS_TWQC picture %s"), *M_Pictures[0].Label);
	M_Pictures.RemoveAt(0);
}

#else

bool UTeamWeaponQualityTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UTeamWeaponQualityTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTeamWeaponQualityTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTeamWeaponQualityTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UTeamWeaponQualityTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTeamWeaponQualityTestSubsystem, STATGROUP_Tickables);
}

#endif
