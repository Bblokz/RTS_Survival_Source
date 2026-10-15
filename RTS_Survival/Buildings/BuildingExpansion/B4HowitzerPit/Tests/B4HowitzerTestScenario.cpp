#include "B4HowitzerTestScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/B4HowitzerPitCrewComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSB4Test, Log, All);

namespace B4HowitzerTestPrivate
{
	const TCHAR* const SquadClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Rus_SquadTW_B4.Rus_SquadTW_B4_C");
	const TCHAR* const PitClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit.Rus_BXP_B4HowitzerPit_C");
	// The team weapon a freshly spawned squad unpacks stands right next to the squad's spawn point.
	constexpr float TeamWeaponSearchRadius = 3000.f;
	const TCHAR* const PitTurretClassMarker = TEXT("RTS_BP_EmbTurret_B4Pit");
	const FName CrewTag = TEXT("B4PitCrew");
	const FName PitOriginSocket = TEXT("PitOrigin");
	const FName ShellMountSocket = TEXT("ShellMount");
	const FName ShellComponentName = TEXT("Shell");
	const FName LeftHandBone = TEXT("hand_l");
	const FName RightHandBone = TEXT("hand_r");

	constexpr int32 EnemyPlayer = 2;
	constexpr int32 ExpectedCrewCount = 4;
	// Two loaders and a gunner man the crew positions.
	constexpr int32 MinimumSquadCrewCount = 3;
	constexpr int32 ShotsToObserve = 2;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float SquadDeployTimeoutSeconds = 90.f;
	constexpr float MoveTimeoutSeconds = 150.f;
	constexpr float ShotsTimeoutSeconds = 150.f;
	constexpr float PitTurretTimeoutSeconds = 30.f;
	constexpr float CameraTraceLength = 200000.f;
	constexpr float PitClearRadius = 1100.f;
	constexpr float SquadClearRadius = 700.f;
	constexpr float MinimumSquadToPitDistance = 2600.f;
	constexpr float SearchRingStep = 2000.f;
	constexpr float SearchRingMax = 40000.f;
	// TestCover squads fight by themselves; the test's units stay out of their way.
	constexpr float MinimumDistanceToOtherUnits = 8000.f;
	constexpr float SearchAngleStepDegrees = 30.f;
	constexpr float NavProjectExtentXY = 400.f;
	constexpr float NavProjectExtentZ = 3000.f;
	constexpr float MoveDistance = 1500.f;
	constexpr float ArrivedTolerance = 450.f;
	// Of the weapon's real range: the game scales the data table ranges, so a fixed distance can fall outside it.
	constexpr float GroundTargetRangeFraction = 0.75f;
	// Off the gun's forward so the attack-ground order also has to turn the carriage past its yaw arc.
	constexpr float GroundTargetYawOffsetDegrees = 25.f;
	constexpr float SquadReloadSeconds = 14.f;
	constexpr float PitReloadSeconds = 24.f;
	constexpr float MinimumReloadFraction = 0.8f;
	constexpr float MaximumReloadFraction = 1.6f;
	constexpr float MaximumAttachOffset = 1.f;
	constexpr float StatusIntervalSeconds = 5.f;
	constexpr float CaptureIntervalSeconds = 1.f;
	constexpr float WreckCaptureIntervalSeconds = 0.5f;
	constexpr float PitDestroyedTimeoutSeconds = 20.f;
	constexpr float WreckPictureSeconds = 5.f;
	constexpr float ObservePlacedTimeoutSeconds = 300.f;
	constexpr int32 HumanPlayer = 1;
	constexpr float HqConvertTimeoutSeconds = 240.f;
	constexpr float BxpLoadTimeoutSeconds = 60.f;
	constexpr float PitConstructionTimeoutSeconds = 400.f;
	constexpr float BuiltPitDrillTimeoutSeconds = 150.f;
	const TCHAR* const PlayerHqMarker = TEXT("Ger_HQ_Truck");
	const FName TruckMontageFinishedFunction = TEXT("OnTruckMontageFinished");
	// Clear of the HQ itself; like a placement click only the pit's centre has to be inside the expansion range.
	constexpr float BuildSpotMinimumRadius = 1400.f;
	constexpr float BuildSpotRadiusStep = 200.f;
	// The revetment and the inner berm; the outer berm may run over open ground.
	constexpr float BuildSpotClearRadius = 900.f;
	constexpr float GroundTraceHalfHeight = 20000.f;
	constexpr float WreckSearchRadius = 1500.f;
	// A montage played at a negative rate must take (nearly) its full stretched length, not end at once.
	constexpr float MinimumReversedMontageFraction = 0.75f;
	const TCHAR* const PitWreckMarker = TEXT("BP_Obstacle_B4Pit_Destroyed");
	const TCHAR* const GunWreckMarker = TEXT("DestroyedTW_B4");
	const FName EmplacementMeshName = TEXT("SM_B4Pit_Emplacement");
	const FName PitOriginSocketName = TEXT("PitOrigin");

	TArray<FB4HowitzerCameraView> GetSquadViews()
	{
		// In the team weapon's actor space: gun forward is +X.
		return {
			{TEXT("side"), FVector(150.f, -950.f, 380.f), FVector(-50.f, 0.f, 120.f), 55.f},
			{TEXT("rear"), FVector(-1000.f, -450.f, 520.f), FVector(0.f, 0.f, 120.f), 55.f},
		};
	}

	TArray<FB4HowitzerCameraView> GetPitViews()
	{
		// In the gun's PitOrigin socket space, the frame the crew drill was authored in.
		return {
			{TEXT("wide_rack"), FVector(-250.f, -1150.f, 700.f), FVector(-300.f, -120.f, 80.f), 65.f},
			{TEXT("wide_cart"), FVector(-250.f, 1150.f, 700.f), FVector(-300.f, 80.f, 80.f), 65.f},
			{TEXT("breech"), FVector(-760.f, -330.f, 330.f), FVector(-180.f, 0.f, 150.f), 60.f},
			{TEXT("top"), FVector(-280.f, 1.f, 1600.f), FVector(-280.f, 0.f, 0.f), 70.f},
		};
	}
	// The crane carries the shell; somebody must still touch it on the cart and at the trays.
	constexpr float MaximumClosestHandToShell = 45.f;

	bool GetIsLandscape(const AActor* Actor)
	{
		return IsValid(Actor) && Actor->IsA<ALandscapeProxy>();
	}

	FName GetActiveMontageName(const USkeletalMeshComponent* Mesh)
	{
		const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		return IsValid(Montage) ? Montage->GetFName() : NAME_None;
	}

	void ForceAnimate(USkeletalMeshComponent* Mesh)
	{
		if (not IsValid(Mesh))
		{
			return;
		}
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->SetComponentTickEnabled(true);
		Mesh->SetComponentTickInterval(0.f);
	}

	int32 GetOwningPlayer(const AActor* Actor)
	{
		const URTSComponent* RTSComponent = IsValid(Actor) ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : INDEX_NONE;
	}

	bool GetIsAreaClear(const UWorld& World, const FVector& Location, const float Radius)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectTypes;
		ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
		ObjectTypes.AddObjectTypesToQuery(ECC_Vehicle);
		const FVector Center = Location + FVector(0.f, 0.f, Radius);
		World.OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(Radius));
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (not GetIsLandscape(Overlap.GetActor()))
			{
				return false;
			}
		}
		return true;
	}

	bool GetIsAwayFromOtherUnits(UWorld& World, const FVector& Location)
	{
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			const AActor* Actor = *It;
			if (IsValid(Actor) && Actor->FindComponentByClass<URTSComponent>() &&
				FVector::Dist2D(Actor->GetActorLocation(), Location) < MinimumDistanceToOtherUnits)
			{
				return false;
			}
		}
		return true;
	}

	void SetFullHealth(const AActor* Actor)
	{
		UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		if (IsValid(Health))
		{
			Health->SetCurrentHealth(Health->GetMaxHealth());
		}
	}

	bool TraceGround(const UWorld& World, const FVector& Location, FVector& OutGround)
	{
		FHitResult Hit;
		const FVector Up(0.f, 0.f, GroundTraceHalfHeight);
		if (not World.LineTraceSingleByChannel(Hit, Location + Up, Location - Up, ECC_Visibility) ||
			not GetIsLandscape(Hit.GetActor()))
		{
			return false;
		}
		OutGround = Hit.Location;
		return true;
	}

	bool ProjectToNav(UWorld& World, const FVector& Location, FVector& OutLocation)
	{
		const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		FNavLocation NavLocation;
		if (not IsValid(NavSystem) || not NavSystem->ProjectPointToNavigation(
			Location, NavLocation, FVector(NavProjectExtentXY, NavProjectExtentXY, NavProjectExtentZ)))
		{
			return false;
		}
		OutLocation = NavLocation.Location;
		return true;
	}

	// Walks rings around the anchor for a navigable spot with nothing but landscape within the radius.
	bool FindClearSpot(UWorld& World, const FVector& Anchor, const float Radius, const FVector& AvoidLocation,
	                   FVector& OutLocation)
	{
		for (float RingRadius = 0.f; RingRadius <= SearchRingMax; RingRadius += SearchRingStep)
		{
			for (float AngleDegrees = 0.f; AngleDegrees < 360.f; AngleDegrees += SearchAngleStepDegrees)
			{
				const FVector Candidate = Anchor + FRotator(0.f, AngleDegrees, 0.f).Vector() * RingRadius;
				FVector NavCandidate;
				const bool bIsUsable = ProjectToNav(World, Candidate, NavCandidate) &&
					FVector::Dist2D(NavCandidate, AvoidLocation) > MinimumSquadToPitDistance &&
					GetIsAreaClear(World, NavCandidate, Radius) && GetIsAwayFromOtherUnits(World, NavCandidate);
				if (bIsUsable)
				{
					OutLocation = NavCandidate;
					return true;
				}
				if (RingRadius <= 0.f)
				{
					break;
				}
			}
		}
		return false;
	}
}

void FB4HowitzerTestScenario::Start()
{
	M_Phase = EB4HowitzerTestPhase::WaitingForStart;
	M_PhaseStartSeconds = 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("B4HowitzerCapture="), M_CaptureDirectory);
	bM_ObservePlaced = FParse::Param(FCommandLine::Get(), TEXT("B4HowitzerObserve"));
	bM_PlayerBuild = FParse::Param(FCommandLine::Get(), TEXT("B4HowitzerPlayerBuild"));
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST started"));
}

void FB4HowitzerTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == EB4HowitzerTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	KeepMeshesAnimating();
	KeepTestUnitsAliveAndMoving();
	WatchMontages(*World);
	TickCaptures(*World);
	switch (M_Phase)
	{
	case EB4HowitzerTestPhase::WaitingForStart:
		TickWaitingForStart(*World);
		break;
	case EB4HowitzerTestPhase::WaitingForSquadDeployed:
		TickWaitingForSquadDeployed(*World);
		break;
	case EB4HowitzerTestPhase::WaitingForMoveDone:
		TickWaitingForMoveDone(*World);
		break;
	case EB4HowitzerTestPhase::WaitingForSquadShots:
		TickWaitingForSquadShots(*World);
		break;
	case EB4HowitzerTestPhase::WaitingForPitTurret:
		TickWaitingForPitTurret(*World);
		break;
	case EB4HowitzerTestPhase::WaitingForPitShots:
		TickWaitingForPitShots(*World);
		break;
	case EB4HowitzerTestPhase::WaitingForPitDestroyed:
		TickWaitingForPitDestroyed(*World);
		break;
	case EB4HowitzerTestPhase::ObservingPlaced:
		TickObservingPlaced(*World);
		break;
	case EB4HowitzerTestPhase::PlayerWaitingForHq:
		TickPlayerWaitingForHq(*World);
		break;
	case EB4HowitzerTestPhase::PlayerWaitingForBxpLoaded:
		TickPlayerWaitingForBxpLoaded(*World);
		break;
	case EB4HowitzerTestPhase::PlayerWaitingForConstruction:
		TickPlayerWaitingForConstruction(*World);
		break;
	case EB4HowitzerTestPhase::PlayerWaitingForBuiltPitDrill:
		TickPlayerWaitingForBuiltPitDrill(*World);
		break;
	default:
		break;
	}
}

void FB4HowitzerTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details);
}

void FB4HowitzerTestScenario::EnterPhase(const UWorld& World, const EB4HowitzerTestPhase NewPhase,
                                         const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = World.GetTimeSeconds();
	M_PhaseDeadlineSeconds = M_PhaseStartSeconds + TimeoutSeconds;
}

bool FB4HowitzerTestScenario::FinishWhenTimedOut(const UWorld& World, const TCHAR* ExpectedEvent)
{
	if (World.GetTimeSeconds() < M_PhaseDeadlineSeconds)
	{
		return false;
	}
	Check(false, ExpectedEvent, TEXT("timed out"));
	Finish(World);
	return true;
}

void FB4HowitzerTestScenario::Finish(const UWorld& World)
{
	M_Phase = EB4HowitzerTestPhase::Inactive;
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST RESULT %s failed_checks=%d map=%s"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount, *World.GetName());
	FPlatformMisc::RequestExit(false, TEXT("B4HowitzerValidate"));
}

void FB4HowitzerTestScenario::TickWaitingForStart(UWorld& World)
{
	// The map boots paused behind the start-game widget.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_PhaseStartSeconds <= 0.f)
	{
		M_PhaseStartSeconds = World.GetTimeSeconds();
	}
	if (World.GetTimeSeconds() - M_PhaseStartSeconds < B4HowitzerTestPrivate::StartDelaySeconds)
	{
		return;
	}
	if (bM_PlayerBuild)
	{
		if (not M_CaptureDirectory.IsEmpty())
		{
			M_CaptureRig.Init(World, M_CaptureDirectory);
		}
		EnterPhase(World, EB4HowitzerTestPhase::PlayerWaitingForHq, B4HowitzerTestPrivate::HqConvertTimeoutSeconds);
		return;
	}
	if (bM_ObservePlaced)
	{
		if (not M_CaptureDirectory.IsEmpty())
		{
			M_CaptureRig.Init(World, M_CaptureDirectory);
		}
		StartObservingPlaced(World);
		return;
	}
	if (not ChooseSpawnLocations(World))
	{
		Check(false, TEXT("found_clear_spawn_spots"));
		Finish(World);
		return;
	}
	if (not M_CaptureDirectory.IsEmpty())
	{
		M_CaptureRig.Init(World, M_CaptureDirectory);
	}
	SpawnSquad(World);
	EnterPhase(World, EB4HowitzerTestPhase::WaitingForSquadDeployed, B4HowitzerTestPrivate::SquadDeployTimeoutSeconds);
}

bool FB4HowitzerTestScenario::ChooseSpawnLocations(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	const APlayerController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	FVector Anchor = CameraLocation;
	if (World.LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                   ECC_Visibility))
	{
		Anchor = GroundHit.Location;
	}
	const FVector FarAway(TNumericLimits<float>::Max());
	if (not FindClearSpot(World, Anchor, PitClearRadius, FarAway, M_PitSpawnLocation))
	{
		return false;
	}
	if (not FindClearSpot(World, Anchor, SquadClearRadius, M_PitSpawnLocation, M_SquadSpawnLocation))
	{
		return false;
	}
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST spots anchor=%s squad=%s pit=%s"),
	       *Anchor.ToCompactString(), *M_SquadSpawnLocation.ToCompactString(), *M_PitSpawnLocation.ToCompactString());
	return true;
}

void FB4HowitzerTestScenario::SpawnSquad(UWorld& World)
{
	// -B4HowitzerSquad=<class path> runs another team weapon through the same steps, to compare.
	FString SquadClassPath = B4HowitzerTestPrivate::SquadClassPath;
	FParse::Value(FCommandLine::Get(), TEXT("B4HowitzerSquad="), SquadClassPath);
	UClass* SquadClass = LoadClass<ASquadController>(nullptr, *SquadClassPath);
	Check(IsValid(SquadClass), TEXT("squad_class_loaded"));
	if (not IsValid(SquadClass))
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Squad = World.SpawnActor<ASquadController>(SquadClass, FTransform(M_SquadSpawnLocation), SpawnParameters);
	Check(M_Squad.IsValid(), TEXT("squad_spawned"));
	const int32 Owner = B4HowitzerTestPrivate::GetOwningPlayer(M_Squad.Get());
	Check(Owner == B4HowitzerTestPrivate::EnemyPlayer, TEXT("squad_default_owner_is_enemy"),
	      FString::Printf(TEXT("owner=%d"), Owner));
}

void FB4HowitzerTestScenario::FindTeamWeapon(UWorld& World)
{
	float ClosestDistance = B4HowitzerTestPrivate::TeamWeaponSearchRadius;
	for (TActorIterator<ATeamWeapon> It(&World); It; ++It)
	{
		ATeamWeapon* TeamWeapon = *It;
		if (not IsValid(TeamWeapon))
		{
			continue;
		}
		const float Distance = FVector::Dist(TeamWeapon->GetActorLocation(), M_SquadSpawnLocation);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			M_TeamWeapon = TeamWeapon;
		}
	}
	if (not M_TeamWeapon.IsValid())
	{
		return;
	}
	FB4HowitzerMontageWatch Watch;
	Watch.Mesh = M_TeamWeapon->FindComponentByClass<USkeletalMeshComponent>();
	Watch.Label = TEXT("team_weapon");
	M_MontageWatches.Add(Watch);
}

void FB4HowitzerTestScenario::TickWaitingForSquadDeployed(UWorld& World)
{
	if (not M_Squad.IsValid())
	{
		Check(false, TEXT("squad_alive"));
		Finish(World);
		return;
	}
	if (not M_TeamWeapon.IsValid())
	{
		FindTeamWeapon(World);
	}
	const ATeamWeaponController* Controller = Cast<ATeamWeaponController>(M_Squad.Get());
	const bool bIsDeployed = M_TeamWeapon.IsValid() && IsValid(Controller) && Controller->GetIsReadyDeployed();
	if (not bIsDeployed)
	{
		FinishWhenTimedOut(World, TEXT("squad_spawned_and_deployed"));
		return;
	}
	Check(true, TEXT("squad_spawned_and_deployed"), FString::Printf(
		      TEXT("seconds=%.1f units=%d"), World.GetTimeSeconds() - M_PhaseStartSeconds,
		      M_Squad->GetSquadUnitsCount()));
	Check(M_Squad->GetSquadUnitsCount() >= B4HowitzerTestPrivate::MinimumSquadCrewCount, TEXT("squad_has_crew"),
	      FString::Printf(TEXT("units=%d"), M_Squad->GetSquadUnitsCount()));

	const FVector Forward = M_TeamWeapon->GetActorForwardVector().GetSafeNormal2D();
	B4HowitzerTestPrivate::ProjectToNav(World, M_TeamWeapon->GetActorLocation() + Forward * B4HowitzerTestPrivate::MoveDistance,
	                                    M_MoveDestination);
	ICommands* Commands = Cast<ICommands>(M_Squad.Get());
	const ECommandQueueError MoveError = Commands
		                                     ? Commands->MoveToLocation(M_MoveDestination, true, Forward.Rotation())
		                                     : ECommandQueueError::AbilityNotAllowed;
	Check(MoveError == ECommandQueueError::NoError, TEXT("move_order_accepted"));
	bM_HasLeftDeployedForMove = false;
	EnterPhase(World, EB4HowitzerTestPhase::WaitingForMoveDone, B4HowitzerTestPrivate::MoveTimeoutSeconds);
}

void FB4HowitzerTestScenario::TickWaitingForMoveDone(UWorld& World)
{
	const ATeamWeaponController* Controller = Cast<ATeamWeaponController>(M_Squad.Get());
	if (not IsValid(Controller) || not M_TeamWeapon.IsValid())
	{
		Check(false, TEXT("squad_alive_during_move"));
		Finish(World);
		return;
	}
	if (not Controller->GetIsReadyDeployed())
	{
		bM_HasLeftDeployedForMove = true;
	}
	const float DistanceToGoal = FVector::Dist2D(M_TeamWeapon->GetActorLocation(), M_MoveDestination);
	const bool bIsDone = bM_HasLeftDeployedForMove && Controller->GetIsReadyDeployed() &&
		DistanceToGoal < B4HowitzerTestPrivate::ArrivedTolerance;
	if (not bIsDone)
	{
		FinishWhenTimedOut(World, TEXT("squad_packed_moved_and_redeployed"));
		return;
	}
	Check(true, TEXT("squad_packed_moved_and_redeployed"), FString::Printf(
		      TEXT("seconds=%.1f distance_to_goal=%.0f"), World.GetTimeSeconds() - M_PhaseStartSeconds, DistanceToGoal));

	const FVector Forward = M_TeamWeapon->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetDirection = Forward.RotateAngleAxis(B4HowitzerTestPrivate::GroundTargetYawOffsetDegrees,
	                                                        FVector::UpVector);
	const float TargetDistance = M_TeamWeapon->GetTurretRange() * B4HowitzerTestPrivate::GroundTargetRangeFraction;
	M_GroundTarget = M_TeamWeapon->GetActorLocation() + TargetDirection * TargetDistance;
	B4HowitzerTestPrivate::ProjectToNav(World, M_GroundTarget, M_GroundTarget);
	ICommands* Commands = Cast<ICommands>(M_Squad.Get());
	const ECommandQueueError AttackError = Commands
		                                       ? Commands->AttackGround(M_GroundTarget, true)
		                                       : ECommandQueueError::AbilityNotAllowed;
	Check(AttackError == ECommandQueueError::NoError, TEXT("squad_attack_ground_accepted"));
	M_SquadShotSeconds.Reset();
	EnterPhase(World, EB4HowitzerTestPhase::WaitingForSquadShots, B4HowitzerTestPrivate::ShotsTimeoutSeconds);
}

void FB4HowitzerTestScenario::TickWaitingForSquadShots(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	const UWeaponState* Weapon = M_TeamWeapon.IsValid() ? M_TeamWeapon->GetLargestCalibreWeapon() : nullptr;
	const bool bIsReloading = IsValid(Weapon) && Weapon->GetIsReloading();
	if (bIsReloading && not bM_WasSquadReloading)
	{
		UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST squad reload_started t=%.2f"), World.GetTimeSeconds());
	}
	bM_WasSquadReloading = bIsReloading;
	if (M_SquadShotSeconds.Num() < ShotsToObserve)
	{
		ReportSquadStatus(World);
		FinishWhenTimedOut(World, TEXT("squad_fired_twice"));
		return;
	}
	const float Interval = M_SquadShotSeconds[1] - M_SquadShotSeconds[0];
	Check(Interval > SquadReloadSeconds * MinimumReloadFraction && Interval < SquadReloadSeconds * MaximumReloadFraction,
	      TEXT("squad_reload_interval"), FString::Printf(TEXT("seconds=%.1f"), Interval));
	if (M_TeamWeapon.IsValid())
	{
		const FVector ToTarget = (M_GroundTarget - M_TeamWeapon->GetActorLocation()).GetSafeNormal2D();
		const float YawError = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
			FVector::DotProduct(ToTarget, M_TeamWeapon->GetActorForwardVector().GetSafeNormal2D()), -1.f, 1.f)));
		UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST squad carriage_yaw_error_to_target=%.1f"), YawError);
	}
	ICommands* Commands = Cast<ICommands>(M_Squad.Get());
	if (Commands)
	{
		Commands->StopCommand();
	}
	SpawnPit(World);
	EnterPhase(World, EB4HowitzerTestPhase::WaitingForPitTurret, PitTurretTimeoutSeconds);
}

void FB4HowitzerTestScenario::ReportSquadStatus(const UWorld& World)
{
	if (World.GetTimeSeconds() < M_NextStatusSeconds || not M_TeamWeapon.IsValid())
	{
		return;
	}
	M_NextStatusSeconds = World.GetTimeSeconds() + B4HowitzerTestPrivate::StatusIntervalSeconds;
	const ATeamWeaponController* Controller = Cast<ATeamWeaponController>(M_Squad.Get());
	const FVector ToTarget = (M_GroundTarget - M_TeamWeapon->GetActorLocation()).GetSafeNormal2D();
	const float YawToTarget = FMath::FindDeltaAngleDegrees(M_TeamWeapon->GetActorRotation().Yaw, ToTarget.Rotation().Yaw);
	const UWeaponState* Weapon = M_TeamWeapon->GetLargestCalibreWeapon();
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST status weapon=%s mag=%d reloading=%d"),
	       IsValid(Weapon) ? *Weapon->GetName() : TEXT("none"), IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : -1,
	       IsValid(Weapon) && Weapon->GetIsReloading());
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST status t=%.1f deployed=%d abandoned=%d yaw_to_target=%.1f units=%d "
	       "range=%.0f distance=%.0f rotated_to_engage=%d class=%s"),
	       World.GetTimeSeconds(), IsValid(Controller) && Controller->GetIsReadyDeployed(),
	       M_TeamWeapon->GetIsAbandoned(), YawToTarget, M_Squad.IsValid() ? M_Squad->GetSquadUnitsCount() : 0,
	       M_TeamWeapon->GetTurretRange(), FVector::Dist(M_TeamWeapon->GetActorLocation(), M_GroundTarget),
	       M_TeamWeapon->GetIsRotatedToEngage(), *M_TeamWeapon->GetClass()->GetName());
}

void FB4HowitzerTestScenario::ReportPitStatus(const UWorld& World)
{
	const ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(M_PitTurret.Get());
	if (World.GetTimeSeconds() < M_NextStatusSeconds || not IsValid(Turret))
	{
		return;
	}
	M_NextStatusSeconds = World.GetTimeSeconds() + B4HowitzerTestPrivate::StatusIntervalSeconds;
	const UWeaponState* Weapon = Turret->GetLargestCalibreWeapon();
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST pit_status t=%.1f mag=%d reloading=%d rotated_to_engage=%d "
	       "range=%.0f distance=%.0f target=%s loading=%d"),
	       World.GetTimeSeconds(), IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : -1,
	       IsValid(Weapon) && Weapon->GetIsReloading(), Turret->GetIsRotatedToEngage(), Turret->GetTurretRange(),
	       FVector::Dist(Turret->GetActorLocation(), M_PitGroundTarget), *GetNameSafe(Turret->GetCurrentTargetActor()),
	       M_PitCrew.IsValid() && M_PitCrew->GetIsLoading());
}

void FB4HowitzerTestScenario::SpawnPit(UWorld& World)
{
	UClass* PitClass = LoadClass<AActor>(nullptr, B4HowitzerTestPrivate::PitClassPath);
	Check(IsValid(PitClass), TEXT("pit_class_loaded"));
	if (not IsValid(PitClass))
	{
		return;
	}
	const FRotator FacingTarget = (M_GroundTarget - M_PitSpawnLocation).GetSafeNormal2D().Rotation();
	// Spawned as a level designer places it: Blueprint defaults, no owning building.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Pit = World.SpawnActor<AActor>(PitClass, FTransform(FacingTarget, M_PitSpawnLocation), SpawnParameters);
	if (not IsValid(Pit))
	{
		Check(false, TEXT("pit_spawned"));
		return;
	}
	M_PitBxp = Pit;
	Check(true, TEXT("pit_spawned"));
	const int32 PitOwner = B4HowitzerTestPrivate::GetOwningPlayer(Pit);
	Check(PitOwner == B4HowitzerTestPrivate::EnemyPlayer, TEXT("pit_default_owner_is_enemy"),
	      FString::Printf(TEXT("owner=%d"), PitOwner));
	TArray<UStaticMeshComponent*> StaticMeshes;
	Pit->GetComponents<UStaticMeshComponent>(StaticMeshes);
	const bool bHasEmplacement = StaticMeshes.ContainsByPredicate([](const UStaticMeshComponent* Mesh)
	{
		return IsValid(Mesh) && IsValid(Mesh->GetStaticMesh()) &&
			Mesh->GetStaticMesh()->GetFName() == B4HowitzerTestPrivate::EmplacementMeshName;
	});
	Check(bHasEmplacement, TEXT("pit_shows_emplacement"));
}

bool FB4HowitzerTestScenario::FindPitTurret(UWorld& World)
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Actor = *It;
		if (not IsValid(Actor) || not Actor->GetClass()->GetName().Contains(B4HowitzerTestPrivate::PitTurretClassMarker))
		{
			continue;
		}
		M_PitTurret = Actor;
		break;
	}
	if (not M_PitTurret.IsValid())
	{
		return false;
	}
	M_PitCrew = M_PitTurret->FindComponentByClass<UB4HowitzerPitCrewComponent>();
	TArray<USkeletalMeshComponent*> Meshes;
	M_PitTurret->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(B4HowitzerTestPrivate::CrewTag))
		{
			M_PitCrewMeshes.Add(Mesh);
			continue;
		}
		M_PitGunMesh = Mesh;
	}
	M_PitCrewMeshes.Sort([](const TWeakObjectPtr<USkeletalMeshComponent>& A,
	                        const TWeakObjectPtr<USkeletalMeshComponent>& B)
	{
		return A.IsValid() && B.IsValid() && A->GetName() < B->GetName();
	});
	TArray<USceneComponent*> SceneComponents;
	M_PitTurret->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component->GetFName() == B4HowitzerTestPrivate::ShellComponentName)
		{
			M_PitShell = Component;
		}
	}
	FB4HowitzerMontageWatch GunWatch;
	GunWatch.Mesh = M_PitGunMesh;
	GunWatch.Label = TEXT("pit_gun");
	M_MontageWatches.Add(GunWatch);
	for (int32 CrewIndex = 0; CrewIndex < M_PitCrewMeshes.Num(); ++CrewIndex)
	{
		FB4HowitzerMontageWatch CrewWatch;
		CrewWatch.Mesh = M_PitCrewMeshes[CrewIndex];
		CrewWatch.Label = FString::Printf(TEXT("pit_crew%d"), CrewIndex + 1);
		M_MontageWatches.Add(CrewWatch);
	}
	M_CrewReach.SetNum(M_PitCrewMeshes.Num());
	return true;
}

void FB4HowitzerTestScenario::CheckPitAttachments()
{
	using namespace B4HowitzerTestPrivate;
	Check(M_PitCrew.IsValid(), TEXT("pit_has_crew_component"));
	Check(M_PitCrewMeshes.Num() == ExpectedCrewCount, TEXT("pit_has_four_crew"),
	      FString::Printf(TEXT("crew=%d"), M_PitCrewMeshes.Num()));
	const USkeletalMeshComponent* GunMesh = M_PitGunMesh.Get();
	for (const TWeakObjectPtr<USkeletalMeshComponent>& CrewMesh : M_PitCrewMeshes)
	{
		const bool bIsAttached = CrewMesh.IsValid() && CrewMesh->GetAttachParent() == GunMesh &&
			CrewMesh->GetAttachSocketName() == PitOriginSocket;
		const float Offset = bIsAttached
			                     ? FVector::Dist(CrewMesh->GetComponentLocation(), GunMesh->GetSocketLocation(PitOriginSocket))
			                     : -1.f;
		Check(bIsAttached && Offset < MaximumAttachOffset, TEXT("crew_attached_to_pit_origin"),
		      FString::Printf(TEXT("%s offset=%.1f"), CrewMesh.IsValid() ? *CrewMesh->GetName() : TEXT("none"), Offset));
	}
	const bool bIsShellOnMount = M_PitShell.IsValid() && M_PitShell->GetAttachParent() == GunMesh &&
		M_PitShell->GetAttachSocketName() == ShellMountSocket;
	Check(bIsShellOnMount, TEXT("shell_attached_to_shell_mount"));
}

void FB4HowitzerTestScenario::TickWaitingForPitTurret(UWorld& World)
{
	if (not FindPitTurret(World))
	{
		FinishWhenTimedOut(World, TEXT("pit_turret_spawned"));
		return;
	}
	Check(true, TEXT("pit_turret_spawned"));
	CheckPitAttachments();
	// The pit stands elsewhere than the squad; its own target lies well inside its own range.
	const ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(M_PitTurret.Get());
	const float PitTargetDistance = IsValid(Turret)
		                                ? Turret->GetTurretRange() * B4HowitzerTestPrivate::GroundTargetRangeFraction
		                                : 0.f;
	const FVector PitForward = M_PitBxp.IsValid() ? M_PitBxp->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	M_PitGroundTarget = M_PitSpawnLocation + PitForward * PitTargetDistance;
	B4HowitzerTestPrivate::ProjectToNav(World, M_PitGroundTarget, M_PitGroundTarget);
	ICommands* Commands = Cast<ICommands>(M_PitBxp.Get());
	const ECommandQueueError AttackError = Commands
		                                       ? Commands->AttackGround(M_PitGroundTarget, true)
		                                       : ECommandQueueError::AbilityNotAllowed;
	Check(AttackError == ECommandQueueError::NoError, TEXT("pit_attack_ground_accepted"));
	M_PitShotSeconds.Reset();
	EnterPhase(World, EB4HowitzerTestPhase::WaitingForPitShots, B4HowitzerTestPrivate::ShotsTimeoutSeconds);
}

void FB4HowitzerTestScenario::TickWaitingForPitShots(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	TrackPitLoading(World);
	if (M_PitShotSeconds.Num() < ShotsToObserve)
	{
		ReportPitStatus(World);
		FinishWhenTimedOut(World, TEXT("pit_fired_twice"));
		return;
	}
	const float Interval = M_PitShotSeconds[1] - M_PitShotSeconds[0];
	Check(Interval > PitReloadSeconds * MinimumReloadFraction && Interval < PitReloadSeconds * MaximumReloadFraction,
	      TEXT("pit_reload_interval"), FString::Printf(TEXT("seconds=%.1f"), Interval));
	KillPit(World);
}

void FB4HowitzerTestScenario::TrackPitLoading(const UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	SampleCrewReach(World);
	const bool bIsLoading = M_PitCrew.IsValid() && M_PitCrew->GetIsLoading();
	if (bIsLoading && not bM_WasPitLoading)
	{
		M_PitLoadingStartSeconds = World.GetTimeSeconds();
	}
	if (not bIsLoading && bM_WasPitLoading)
	{
		const float LoadingSeconds = World.GetTimeSeconds() - M_PitLoadingStartSeconds;
		Check(FMath::IsNearlyEqual(LoadingSeconds, PitReloadSeconds, PitReloadSeconds * (1.f - MinimumReloadFraction)),
		      TEXT("pit_loading_drill_matches_reload"), FString::Printf(TEXT("seconds=%.1f"), LoadingSeconds));
		ReportCrewReach();
	}
	bM_WasPitLoading = bIsLoading;
}

void FB4HowitzerTestScenario::StartObservingPlaced(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Actor = *It;
		const FString ClassName = IsValid(Actor) ? Actor->GetClass()->GetName() : FString();
		if (ClassName.Contains(TEXT("Rus_BXP_B4HowitzerPit")))
		{
			M_PitBxp = Actor;
		}
		if (ClassName.Contains(TEXT("Rus_SquadTW_B4")))
		{
			M_Squad = Cast<ASquadController>(Actor);
			M_SquadSpawnLocation = Actor->GetActorLocation();
		}
	}
	Check(M_PitBxp.IsValid(), TEXT("placed_pit_found"));
	Check(M_Squad.IsValid(), TEXT("placed_squad_found"));
	Check(GetOwningPlayer(M_PitBxp.Get()) == EnemyPlayer, TEXT("placed_pit_owner_is_enemy"),
	      FString::Printf(TEXT("owner=%d"), GetOwningPlayer(M_PitBxp.Get())));
	Check(GetOwningPlayer(M_Squad.Get()) == EnemyPlayer, TEXT("placed_squad_owner_is_enemy"),
	      FString::Printf(TEXT("owner=%d"), GetOwningPlayer(M_Squad.Get())));
	if (not M_PitBxp.IsValid() || not M_Squad.IsValid())
	{
		Finish(World);
		return;
	}
	M_PitSpawnLocation = M_PitBxp->GetActorLocation();
	M_PitShotSeconds.Reset();
	M_SquadShotSeconds.Reset();
	EnterPhase(World, EB4HowitzerTestPhase::ObservingPlaced, ObservePlacedTimeoutSeconds);
}

void FB4HowitzerTestScenario::KeepHqUnfolding() const
{
	ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		return;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Hq->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		B4HowitzerTestPrivate::ForceAnimate(Mesh);
	}
	if (Hq->GetNomadicStatus() != ENomadStatus::CreatingBuildingTruckAnim)
	{
		return;
	}
	if (UFunction* MontageFinished = Hq->FindFunction(B4HowitzerTestPrivate::TruckMontageFinishedFunction))
	{
		Hq->ProcessEvent(MontageFinished, nullptr);
	}
}

void FB4HowitzerTestScenario::TickPlayerWaitingForHq(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (not M_Hq.IsValid())
	{
		for (TActorIterator<ANomadicVehicle> It(&World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(PlayerHqMarker) && GetOwningPlayer(*It) == HumanPlayer)
			{
				M_Hq = *It;
				break;
			}
		}
	}
	KeepHqUnfolding();
	if (not M_Hq.IsValid() || M_Hq->GetNomadicStatus() != ENomadStatus::Building)
	{
		FinishWhenTimedOut(World, TEXT("player_hq_converted_to_building"));
		return;
	}
	Check(true, TEXT("player_hq_converted_to_building"));
	if (not ClickCapturedPitOption(World))
	{
		Finish(World);
		return;
	}
	EnterPhase(World, EB4HowitzerTestPhase::PlayerWaitingForBxpLoaded, BxpLoadTimeoutSeconds);
}

bool FB4HowitzerTestScenario::ClickCapturedPitOption(UWorld& World)
{
	ANomadicVehicle* Hq = M_Hq.Get();
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(Hq) || not IsValid(PlayerController))
	{
		Check(false, TEXT("player_hq_and_controller_valid"));
		return false;
	}
	const TArray<FBxpOptionData> Options = Hq->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* PitOption = Options.FindByPredicate([](const FBxpOptionData& Option)
	{
		return Option.ExpansionType == EBuildingExpansionType::BTX_GerCapturedB4Pit;
	});
	Check(PitOption != nullptr, TEXT("captured_pit_offered_on_german_hq"),
	      FString::Printf(TEXT("options=%d"), Options.Num()));
	const TArray<FBuildingExpansionItem>& Slots = Hq->GetBuildingExpansions();
	M_HqSlotIndex = Slots.IndexOfByPredicate([](const FBuildingExpansionItem& Item)
	{
		return Item.Expansion == nullptr && Item.ExpansionStatus == EBuildingExpansionStatus::BXS_UnlockedButNotBuild;
	});
	Check(M_HqSlotIndex != INDEX_NONE, TEXT("hq_has_free_expansion_slot"),
	      FString::Printf(TEXT("slots=%d"), Slots.Num()));
	if (PitOption == nullptr || M_HqSlotIndex == INDEX_NONE)
	{
		return false;
	}
	TScriptInterface<IBuildingExpansionOwner> Owner;
	Owner.SetObject(Hq);
	Owner.SetInterface(static_cast<IBuildingExpansionOwner*>(Hq));
	// What the option button does when the player clicks it.
	PlayerController->ExpandBuildingWithBxpAndStartPreview(*PitOption, Owner, M_HqSlotIndex, false);
	return true;
}

bool FB4HowitzerTestScenario::FindBuildSpotNearHq(UWorld& World, FVector& OutLocation) const
{
	using namespace B4HowitzerTestPrivate;
	const ANomadicVehicle* Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		return false;
	}
	const FVector HqLocation = Hq->GetBxpOwnerLocation();
	const float MaximumRadius = Hq->GetBxpExpansionRange();
	UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST hq expansion_range=%.0f"), MaximumRadius);
	for (float Radius = BuildSpotMinimumRadius; Radius <= MaximumRadius; Radius += BuildSpotRadiusStep)
	{
		for (float AngleDegrees = 0.f; AngleDegrees < 360.f; AngleDegrees += SearchAngleStepDegrees)
		{
			FVector Candidate;
			// Free BXP placement goes by the ground under the cursor, not by the navmesh.
			if (TraceGround(World, HqLocation + FRotator(0.f, AngleDegrees, 0.f).Vector() * Radius, Candidate) &&
				GetIsAreaClear(World, Candidate, BuildSpotClearRadius))
			{
				OutLocation = Candidate;
				return true;
			}
		}
	}
	return false;
}

void FB4HowitzerTestScenario::TickPlayerWaitingForBxpLoaded(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(PlayerController) || not PlayerController->MapTest_GetIsBxpWaitingForPlacement())
	{
		FinishWhenTimedOut(World, TEXT("captured_pit_loaded_for_placement"));
		return;
	}
	Check(true, TEXT("captured_pit_loaded_for_placement"));
	FVector BuildLocation;
	if (not FindBuildSpotNearHq(World, BuildLocation))
	{
		Check(false, TEXT("found_build_spot_near_hq"));
		Finish(World);
		return;
	}
	M_PitSpawnLocation = BuildLocation;
	// The test map's player starts poor; hand over exactly the pit's cost so the real payment can run.
	UPlayerResourceManager* Resources = PlayerController->GetPlayerResourceManager();
	const FBxpData PitData = URTSBlueprintFunctionLibrary::BP_GetPlayerBxpData(EBuildingExpansionType::BTX_GerCapturedB4Pit,
	                                                                           PlayerController);
	if (IsValid(Resources))
	{
		for (const TPair<ERTSResourceType, int32>& Cost : PitData.Cost.ResourceCosts)
		{
			Resources->CheatAddStorage(Cost.Key, Cost.Value);
			Resources->AddResource(Cost.Key, Cost.Value);
		}
	}
	// The placement click: pays for the pit and starts its construction.
	const bool bPlaced = PlayerController->MapTest_PlaceLoadedBxpAtLocation(BuildLocation);
	Check(bPlaced, TEXT("player_placed_and_paid_for_pit"), BuildLocation.ToCompactString());
	M_PlayerPit = M_Hq.IsValid() ? M_Hq->GetBuildingExpansionAtIndex(M_HqSlotIndex) : nullptr;
	M_PitBxp = M_PlayerPit.Get();
	if (not bPlaced || not M_PlayerPit.IsValid())
	{
		Finish(World);
		return;
	}
	const int32 PitOwner = GetOwningPlayer(M_PlayerPit.Get());
	Check(PitOwner == HumanPlayer, TEXT("player_pit_owned_by_player"), FString::Printf(TEXT("owner=%d"), PitOwner));
	Check(M_PlayerPit->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_BeingBuild,
	      TEXT("player_pit_under_construction"));
	M_PlayerPitConstructionStartSeconds = World.GetTimeSeconds();
	M_PitShotSeconds.Reset();
	EnterPhase(World, EB4HowitzerTestPhase::PlayerWaitingForConstruction, PitConstructionTimeoutSeconds);
}

void FB4HowitzerTestScenario::TickPlayerWaitingForConstruction(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	if (not M_PitTurret.IsValid() && FindPitTurret(World))
	{
		CheckPitAttachments();
	}
	if (not M_PlayerPit.IsValid())
	{
		Check(false, TEXT("player_pit_alive_during_construction"));
		Finish(World);
		return;
	}
	if (M_PlayerPit->GetBuildingExpansionStatus() != EBuildingExpansionStatus::BXS_Built)
	{
		FinishWhenTimedOut(World, TEXT("player_pit_constructed"));
		return;
	}
	Check(true, TEXT("player_pit_constructed"), FString::Printf(
		      TEXT("seconds=%.1f"), World.GetTimeSeconds() - M_PlayerPitConstructionStartSeconds));
	Check(M_PitShotSeconds.IsEmpty(), TEXT("player_pit_holds_fire_while_under_construction"),
	      FString::Printf(TEXT("shots=%d"), M_PitShotSeconds.Num()));
	const ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(M_PitTurret.Get());
	const float TargetDistance = IsValid(Turret) ? Turret->GetTurretRange() * GroundTargetRangeFraction : 0.f;
	M_PitGroundTarget = M_PlayerPit->GetActorLocation() + M_PlayerPit->GetActorForwardVector().GetSafeNormal2D() * TargetDistance;
	ProjectToNav(World, M_PitGroundTarget, M_PitGroundTarget);
	ICommands* Commands = Cast<ICommands>(M_PlayerPit.Get());
	const ECommandQueueError AttackError = Commands
		                                       ? Commands->AttackGround(M_PitGroundTarget, true)
		                                       : ECommandQueueError::AbilityNotAllowed;
	Check(AttackError == ECommandQueueError::NoError, TEXT("player_pit_attack_ground_accepted"));
	EnterPhase(World, EB4HowitzerTestPhase::PlayerWaitingForBuiltPitDrill, BuiltPitDrillTimeoutSeconds);
}

void FB4HowitzerTestScenario::TickPlayerWaitingForBuiltPitDrill(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	TrackPitLoading(World);
	const bool bHasReloaded = not M_PitShotSeconds.IsEmpty() && not bM_WasPitLoading &&
		World.GetTimeSeconds() > M_PitShotSeconds[0] + PitReloadSeconds * MinimumReloadFraction;
	if (not bHasReloaded)
	{
		ReportPitStatus(World);
		FinishWhenTimedOut(World, TEXT("player_pit_fires_and_crew_reloads"));
		return;
	}
	Check(true, TEXT("player_pit_fires_and_crew_reloads"), FString::Printf(TEXT("first_shot=%.1f"), M_PitShotSeconds[0]));
	Finish(World);
}

void FB4HowitzerTestScenario::TickObservingPlaced(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	if (not M_TeamWeapon.IsValid())
	{
		FindTeamWeapon(World);
	}
	if (not M_PitTurret.IsValid() && FindPitTurret(World))
	{
		CheckPitAttachments();
	}
	TrackPitLoading(World);
	// Each shot may destroy its target; with nothing else in sight a placed gun rightly holds fire. The two-shot
	// reload timing is covered by the spawned run; here it is enough that both engage and the pit reloads once.
	const bool bHasPitReloaded = not M_PitShotSeconds.IsEmpty() && not bM_WasPitLoading &&
		World.GetTimeSeconds() > M_PitShotSeconds[0] + PitReloadSeconds * MinimumReloadFraction;
	const bool bHaveBothFired = bHasPitReloaded && not M_SquadShotSeconds.IsEmpty();
	if (not bHaveBothFired)
	{
		ReportPitStatus(World);
		FinishWhenTimedOut(World, TEXT("placed_pit_and_squad_engage_the_base"));
		return;
	}
	Check(true, TEXT("placed_pit_and_squad_engage_the_base"), FString::Printf(
		      TEXT("pit_first_shot=%.1f squad_first_shot=%.1f"), M_PitShotSeconds[0], M_SquadShotSeconds[0]));
	Finish(World);
}

void FB4HowitzerTestScenario::KillPit(const UWorld& World)
{
	AHPActorObjectsMaster* Pit = Cast<AHPActorObjectsMaster>(M_PitBxp.Get());
	if (not IsValid(Pit))
	{
		Check(false, TEXT("pit_alive_before_kill"));
		Finish(World);
		return;
	}
	M_PitDeathLocation = Pit->GetActorLocation();
	bM_IsKillingPit = true;
	Pit->TakeFatalDamage();
	EnterPhase(World, EB4HowitzerTestPhase::WaitingForPitDestroyed, B4HowitzerTestPrivate::PitDestroyedTimeoutSeconds);
}

bool FB4HowitzerTestScenario::GetHasWreckNear(UWorld& World, const TCHAR* ClassMarker) const
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		const AActor* Actor = *It;
		if (IsValid(Actor) && Actor->GetClass()->GetName().Contains(ClassMarker) &&
			FVector::Dist2D(Actor->GetActorLocation(), M_PitDeathLocation) < B4HowitzerTestPrivate::WreckSearchRadius)
		{
			return true;
		}
	}
	return false;
}

void FB4HowitzerTestScenario::TickWaitingForPitDestroyed(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	const bool bHasWrecks = GetHasWreckNear(World, PitWreckMarker) && GetHasWreckNear(World, GunWreckMarker);
	if (M_PitBxp.IsValid() || not bHasWrecks)
	{
		FinishWhenTimedOut(World, TEXT("pit_destroyed_into_wrecks"));
		return;
	}
	// A few more pictures of the wrecks before the run ends.
	if (M_CaptureRig.GetIsEnabled() && World.GetTimeSeconds() - M_PhaseStartSeconds < WreckPictureSeconds)
	{
		return;
	}
	Check(true, TEXT("pit_destroyed_into_wrecks"));
	Finish(World);
}

void FB4HowitzerTestScenario::TickCaptures(UWorld& World)
{
	using namespace B4HowitzerTestPrivate;
	if (not M_CaptureRig.GetIsEnabled() || World.GetTimeSeconds() < M_NextCaptureSeconds)
	{
		return;
	}
	const FString Label = FString::Printf(TEXT("t%05.1f"), World.GetTimeSeconds());
	switch (M_Phase)
	{
	case EB4HowitzerTestPhase::WaitingForMoveDone:
	case EB4HowitzerTestPhase::WaitingForSquadShots:
		if (M_TeamWeapon.IsValid())
		{
			RevealTestActors();
			M_CaptureRig.CaptureViews(World, M_TeamWeapon->GetActorTransform(), GetSquadViews(), TEXT("squad_") + Label);
		}
		M_NextCaptureSeconds = World.GetTimeSeconds() + CaptureIntervalSeconds;
		return;
	case EB4HowitzerTestPhase::WaitingForPitShots:
		if (M_PitGunMesh.IsValid())
		{
			RevealTestActors();
			FTransform Frame = M_PitGunMesh->GetSocketTransform(PitOriginSocketName);
			Frame.SetScale3D(FVector::OneVector);
			M_CaptureRig.CaptureViews(World, Frame, GetPitViews(), TEXT("pit_") + Label);
		}
		M_NextCaptureSeconds = World.GetTimeSeconds() + CaptureIntervalSeconds;
		return;
	case EB4HowitzerTestPhase::PlayerWaitingForConstruction:
	case EB4HowitzerTestPhase::PlayerWaitingForBuiltPitDrill:
	case EB4HowitzerTestPhase::ObservingPlaced:
		RevealTestActors();
		if (M_PitGunMesh.IsValid())
		{
			FTransform Frame = M_PitGunMesh->GetSocketTransform(PitOriginSocketName);
			Frame.SetScale3D(FVector::OneVector);
			M_CaptureRig.CaptureViews(World, Frame, GetPitViews(), TEXT("placedpit_") + Label);
		}
		if (M_TeamWeapon.IsValid())
		{
			M_CaptureRig.CaptureViews(World, M_TeamWeapon->GetActorTransform(), GetSquadViews(), TEXT("placedsquad_") + Label);
		}
		M_NextCaptureSeconds = World.GetTimeSeconds() + CaptureIntervalSeconds * 2.f;
		return;
	case EB4HowitzerTestPhase::WaitingForPitDestroyed:
		M_CaptureRig.CaptureViews(World, FTransform(M_PitDeathLocation), {GetPitViews()[0]}, TEXT("wreck_") + Label);
		M_NextCaptureSeconds = World.GetTimeSeconds() + WreckCaptureIntervalSeconds;
		return;
	default:
		return;
	}
}

void FB4HowitzerTestScenario::RevealTestActors() const
{
	// The fog of war hides enemy units from the player; the pictures must show them.
	TArray<AActor*> Actors = {M_TeamWeapon.Get(), M_PitBxp.Get(), M_PitTurret.Get()};
	if (ASquadController* Squad = M_Squad.Get())
	{
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			Actors.Add(Soldier);
		}
	}
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
}

void FB4HowitzerTestScenario::KeepMeshesAnimating() const
{
	for (const FB4HowitzerMontageWatch& Watch : M_MontageWatches)
	{
		B4HowitzerTestPrivate::ForceAnimate(Watch.Mesh.Get());
	}
}

void FB4HowitzerTestScenario::KeepTestUnitsAliveAndMoving() const
{
	B4HowitzerTestPrivate::SetFullHealth(M_TeamWeapon.Get());
	if (not bM_IsKillingPit)
	{
		B4HowitzerTestPrivate::SetFullHealth(M_PitBxp.Get());
	}
	ASquadController* Squad = M_Squad.Get();
	if (not IsValid(Squad))
	{
		return;
	}
	for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		B4HowitzerTestPrivate::SetFullHealth(Soldier);
		// Unseen soldiers move in coarse steps and overshoot their crew positions forever; the optimizer keeps
		// resetting this, so it is set every frame.
		UCharacterMovementComponent* Movement = IsValid(Soldier) ? Soldier->GetCharacterMovement() : nullptr;
		if (IsValid(Movement))
		{
			Movement->SetComponentTickInterval(0.f);
		}
	}
}

void FB4HowitzerTestScenario::WatchMontages(const UWorld& World)
{
	for (FB4HowitzerMontageWatch& Watch : M_MontageWatches)
	{
		const FName MontageName = B4HowitzerTestPrivate::GetActiveMontageName(Watch.Mesh.Get());
		if (MontageName == Watch.LastMontageName)
		{
			continue;
		}
		CheckFinishedMontage(World, Watch);
		Watch.LastMontageName = MontageName;
		const UAnimInstance* AnimInstance = Watch.Mesh.IsValid() ? Watch.Mesh->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST montage t=%.2f mesh=%s montage=%s rate=%.3f position=%.2f"),
		       World.GetTimeSeconds(), *Watch.Label, *MontageName.ToString(),
		       IsValid(Montage) ? AnimInstance->Montage_GetPlayRate(Montage) : 0.f,
		       IsValid(Montage) ? AnimInstance->Montage_GetPosition(Montage) : 0.f);
		const float PlayRate = IsValid(Montage) ? AnimInstance->Montage_GetPlayRate(Montage) : 0.f;
		Watch.MontageStartSeconds = World.GetTimeSeconds();
		Watch.bIsPlayingReversed = PlayRate < 0.f;
		// A reversed montage runs from where it starts back to 0; a forward one from there to its end.
		const float StartPosition = IsValid(Montage) ? AnimInstance->Montage_GetPosition(Montage) : 0.f;
		const float LengthToPlay = IsValid(Montage)
			                           ? (PlayRate < 0.f ? StartPosition : Montage->GetPlayLength() - StartPosition)
			                           : 0.f;
		Watch.ExpectedMontageSeconds = FMath::IsNearlyZero(PlayRate) ? 0.f : LengthToPlay / FMath::Abs(PlayRate);
		const FString MontageString = MontageName.ToString();
		if (Watch.Label == TEXT("team_weapon") && MontageString.Contains(TEXT("Fire")))
		{
			M_SquadShotSeconds.Add(World.GetTimeSeconds());
		}
		// Every shot empties the one-round magazine and starts the loading drill.
		const bool bIsPitShot = MontageString.Contains(TEXT("B4Pit_Reload"));
		if (Watch.Label == TEXT("pit_gun") && bIsPitShot)
		{
			M_PitShotSeconds.Add(World.GetTimeSeconds());
		}
	}
}

void FB4HowitzerTestScenario::CheckFinishedMontage(const UWorld& World, const FB4HowitzerMontageWatch& Watch)
{
	if (not Watch.bIsPlayingReversed || Watch.LastMontageName.IsNone())
	{
		return;
	}
	const float PlayedSeconds = World.GetTimeSeconds() - Watch.MontageStartSeconds;
	Check(PlayedSeconds >= Watch.ExpectedMontageSeconds * B4HowitzerTestPrivate::MinimumReversedMontageFraction,
	      TEXT("reversed_montage_plays_through"), FString::Printf(
		      TEXT("%s %s played=%.2f expected=%.2f"), *Watch.Label, *Watch.LastMontageName.ToString(), PlayedSeconds,
		      Watch.ExpectedMontageSeconds));
}

void FB4HowitzerTestScenario::SampleCrewReach(const UWorld& World)
{
	if (not M_PitShell.IsValid() || not M_PitShell->IsVisible() || not bM_WasPitLoading)
	{
		return;
	}
	const FVector ShellLocation = M_PitShell->GetComponentLocation();
	for (int32 CrewIndex = 0; CrewIndex < M_PitCrewMeshes.Num() && CrewIndex < M_CrewReach.Num(); ++CrewIndex)
	{
		const USkeletalMeshComponent* CrewMesh = M_PitCrewMeshes[CrewIndex].Get();
		if (not IsValid(CrewMesh))
		{
			continue;
		}
		const float LeftDistance = FVector::Dist(CrewMesh->GetBoneLocation(B4HowitzerTestPrivate::LeftHandBone), ShellLocation);
		const float RightDistance = FVector::Dist(CrewMesh->GetBoneLocation(B4HowitzerTestPrivate::RightHandBone), ShellLocation);
		const float Closest = FMath::Min(LeftDistance, RightDistance);
		if (Closest < M_CrewReach[CrewIndex].ClosestHandToShell)
		{
			M_CrewReach[CrewIndex].ClosestHandToShell = Closest;
			M_CrewReach[CrewIndex].SecondsAtClosest = World.GetTimeSeconds() - M_PitLoadingStartSeconds;
		}
	}
}

void FB4HowitzerTestScenario::ReportCrewReach()
{
	float ClosestOfAll = TNumericLimits<float>::Max();
	for (int32 CrewIndex = 0; CrewIndex < M_CrewReach.Num(); ++CrewIndex)
	{
		const FB4HowitzerCrewReach& Reach = M_CrewReach[CrewIndex];
		UE_LOG(LogRTSB4Test, Display, TEXT("RTS_B4_TEST crew%d closest_hand_to_shell=%.1f at_loading_seconds=%.1f"),
		       CrewIndex + 1, Reach.ClosestHandToShell, Reach.SecondsAtClosest);
		ClosestOfAll = FMath::Min(ClosestOfAll, Reach.ClosestHandToShell);
		M_CrewReach[CrewIndex] = FB4HowitzerCrewReach();
	}
	Check(ClosestOfAll < B4HowitzerTestPrivate::MaximumClosestHandToShell, TEXT("crew_handles_the_shell"),
	      FString::Printf(TEXT("closest=%.1f"), ClosestOfAll));
}

#endif
