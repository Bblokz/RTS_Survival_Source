#include "GunPitTestScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/GunPitCrewComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGunPitTest, Log, All);

namespace GunPitTestPrivate
{
	const FName CrewTag = TEXT("PitCrew");
	const FName RoundTag = TEXT("PitRound");
	const FName PitOriginSocket = TEXT("PitOrigin");
	const FName HandBone = TEXT("hand_r");
	const FName ReadyCrateName = TEXT("ReadyCrate");

	constexpr int32 PlayerOwner = 1;
	constexpr int32 ExpectedCrewCount = 4;
	constexpr int32 ExpectedRoundCount = 2;
	constexpr int32 ShotsToObserve = 2;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float TurretTimeoutSeconds = 30.f;
	constexpr float FirstRoundsTimeoutSeconds = 10.f;
	constexpr float ObserveTimeoutSeconds = 180.f;
	constexpr float CameraTraceLength = 200000.f;
	constexpr float PitClearRadius = 900.f;
	constexpr float MinimumDistanceBetweenPits = 3500.f;
	constexpr float SearchRingStep = 2000.f;
	constexpr float SearchRingMax = 40000.f;
	constexpr float SearchAngleStepDegrees = 30.f;
	// TestCover squads fight by themselves; the pits stay out of their way.
	constexpr float MinimumDistanceToOtherUnits = 8000.f;
	constexpr float NavProjectExtentXY = 400.f;
	constexpr float NavProjectExtentZ = 3000.f;
	// Of the weapon's real range: the game scales the data table ranges, so a fixed distance can fall outside it.
	constexpr float GroundTargetRangeFraction = 0.6f;
	// Well past the arc of a carriage gun: the whole pit gun and its crew must turn.
	constexpr float GroundTargetYawOffsetDegrees = 60.f;
	constexpr float MaximumAimYawErrorDegrees = 12.f;
	constexpr float MaximumAttachOffset = 1.f;
	constexpr float MaximumScaleError = 0.01f;
	constexpr float MaximumLoaderSpotError = 30.f;
	// From the round's pivot to the palm bone that holds it; the bake keeps the palms on the round's body.
	constexpr float MaximumRoundToHandDistance = 70.f;
	constexpr float StatusIntervalSeconds = 5.f;
	constexpr float DrillCaptureIntervalSeconds = 1.f;

	const TCHAR* const PitRoot = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/");
	const FVector PakLoaderSpot(-148.6f, 76.f, 0.f);
	const FVector HowitzerLoaderSpot(-234.5f, 39.7f, 0.f);
	constexpr float PakViewScale = 0.8f;

	TArray<FB4HowitzerCameraView> GetPitViews(const float Scale)
	{
		// In the gun's PitOrigin socket space (gun forward +X); the handler works on the +Y side behind the breech.
		const TArray<FB4HowitzerCameraView> Views = {
			{TEXT("wide"), FVector(700.f, -900.f, 900.f), FVector(-200.f, 0.f, 50.f), 70.f},
			{TEXT("drill"), FVector(-250.f, 900.f, 450.f), FVector(-250.f, 120.f, 90.f), 60.f},
			{TEXT("rear"), FVector(-1100.f, 250.f, 500.f), FVector(-150.f, 50.f, 100.f), 60.f},
			{TEXT("top"), FVector(-200.f, 1.f, 1700.f), FVector(-200.f, 0.f, 0.f), 70.f},
		};
		TArray<FB4HowitzerCameraView> Scaled = Views;
		for (FB4HowitzerCameraView& View : Scaled)
		{
			View.Location *= Scale;
			View.LookAt *= Scale;
		}
		return Scaled;
	}

	TArray<FGunPitUnderTest> GetPitsToTest()
	{
		auto MakePit = [](const TCHAR* Label, const TCHAR* Gun, const TCHAR* PitMesh, const FVector& LoaderSpot,
		                  const float ViewScale)
		{
			FGunPitUnderTest Pit;
			Pit.Label = Label;
			Pit.ClassPath = FString::Printf(TEXT("%sGer_BXP_GerPit_%s.Ger_BXP_GerPit_%s_C"), PitRoot, Gun, Gun);
			Pit.PitMeshName = PitMesh;
			Pit.LoaderSpot = LoaderSpot;
			Pit.ViewScale = ViewScale;
			return Pit;
		};
		return {
			MakePit(TEXT("pak40"), TEXT("Pak40"), TEXT("SM_GerPit_Pak"), PakLoaderSpot, PakViewScale),
			MakePit(TEXT("lefh18"), TEXT("LeFH18"), TEXT("SM_GerPit_Howitzer"), HowitzerLoaderSpot, 1.f),
			MakePit(TEXT("sfh18"), TEXT("SFH18"), TEXT("SM_GerPit_Howitzer"), HowitzerLoaderSpot, 1.f),
		};
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

	void SetFullHealth(const AActor* Actor)
	{
		UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		if (IsValid(Health))
		{
			Health->SetCurrentHealth(Health->GetMaxHealth());
		}
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
		return not Overlaps.ContainsByPredicate([](const FOverlapResult& Overlap)
		{
			const AActor* Actor = Overlap.GetActor();
			return not (IsValid(Actor) && Actor->IsA<ALandscapeProxy>());
		});
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

	bool GetIsFarFromAll(const FVector& Candidate, const TArray<FVector>& Taken)
	{
		return not Taken.ContainsByPredicate([&Candidate](const FVector& Spot)
		{
			return FVector::Dist2D(Spot, Candidate) < MinimumDistanceBetweenPits;
		});
	}

	// Walks rings around the anchor for a navigable spot with nothing but landscape within the radius.
	bool FindClearSpot(UWorld& World, const FVector& Anchor, const TArray<FVector>& Taken, FVector& OutLocation)
	{
		for (float RingRadius = 0.f; RingRadius <= SearchRingMax; RingRadius += SearchRingStep)
		{
			for (float AngleDegrees = 0.f; AngleDegrees < 360.f; AngleDegrees += SearchAngleStepDegrees)
			{
				const FVector Candidate = Anchor + FRotator(0.f, AngleDegrees, 0.f).Vector() * RingRadius;
				FVector NavCandidate;
				const bool bIsUsable = ProjectToNav(World, Candidate, NavCandidate) && GetIsFarFromAll(NavCandidate, Taken) &&
					GetIsAreaClear(World, NavCandidate, PitClearRadius) && GetIsAwayFromOtherUnits(World, NavCandidate);
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

	FTransform GetPitOriginFrame(const USkeletalMeshComponent& GunMesh)
	{
		FTransform Frame = GunMesh.GetSocketTransform(PitOriginSocket);
		Frame.SetScale3D(FVector::OneVector);
		return Frame;
	}

	USkeletalMeshComponent* GetHoldingCrewMesh(const UStaticMeshComponent& Round)
	{
		USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(Round.GetAttachParent());
		return IsValid(Parent) && Parent->ComponentHasTag(CrewTag) ? Parent : nullptr;
	}

	float GetDistanceToHand(const UStaticMeshComponent& Round, const USkeletalMeshComponent& CrewMesh)
	{
		return FVector::Dist(Round.GetComponentLocation(), CrewMesh.GetBoneLocation(HandBone));
	}
}

void FGunPitTestScenario::Start()
{
	M_Phase = EGunPitTestPhase::WaitingForStart;
	M_PhaseStartSeconds = 0.f;
	M_Pits = GunPitTestPrivate::GetPitsToTest();
	FParse::Value(FCommandLine::Get(), TEXT("GerPitCapture="), M_CaptureDirectory);
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST started"));
}

void FGunPitTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == EGunPitTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	KeepMeshesAnimating();
	switch (M_Phase)
	{
	case EGunPitTestPhase::WaitingForStart:
		TickWaitingForStart(*World);
		break;
	case EGunPitTestPhase::WaitingForTurrets:
		TickWaitingForTurrets(*World);
		break;
	case EGunPitTestPhase::WaitingForFirstRounds:
		TickWaitingForFirstRounds(*World);
		break;
	case EGunPitTestPhase::Observing:
		TickObserving(*World);
		break;
	default:
		break;
	}
}

void FGunPitTestScenario::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void FGunPitTestScenario::EnterPhase(const UWorld& World, const EGunPitTestPhase NewPhase, const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = World.GetTimeSeconds();
	M_PhaseDeadlineSeconds = M_PhaseStartSeconds + TimeoutSeconds;
}

void FGunPitTestScenario::Finish(const UWorld& World)
{
	M_Phase = EGunPitTestPhase::Inactive;
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST RESULT %s failed_checks=%d map=%s"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount, *World.GetName());
	FPlatformMisc::RequestExit(false, TEXT("GerPitValidate"));
}

void FGunPitTestScenario::TickWaitingForStart(UWorld& World)
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
	if (World.GetTimeSeconds() - M_PhaseStartSeconds < GunPitTestPrivate::StartDelaySeconds)
	{
		return;
	}
	if (not M_CaptureDirectory.IsEmpty())
	{
		M_CaptureRig.Init(World, M_CaptureDirectory);
	}
	if (not SpawnPits(World))
	{
		Finish(World);
		return;
	}
	EnterPhase(World, EGunPitTestPhase::WaitingForTurrets, GunPitTestPrivate::TurretTimeoutSeconds);
}

bool FGunPitTestScenario::SpawnPits(UWorld& World)
{
	using namespace GunPitTestPrivate;
	const APlayerController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		Check(false, TEXT("player_camera_found"));
		return false;
	}
	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	const bool bHitGround = World.LineTraceSingleByChannel(GroundHit, CameraLocation,
	                                                       CameraLocation + CameraForward * CameraTraceLength,
	                                                       ECC_Visibility);
	const FVector Anchor = bHitGround ? GroundHit.Location : CameraLocation;
	TArray<FVector> TakenSpots;
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		if (not FindClearSpot(World, Anchor, TakenSpots, Pit.SpawnLocation))
		{
			Check(false, Pit.Label + TEXT("_found_clear_spot"));
			return false;
		}
		TakenSpots.Add(Pit.SpawnLocation);
		UClass* PitClass = LoadClass<AActor>(nullptr, *Pit.ClassPath);
		if (not IsValid(PitClass))
		{
			Check(false, Pit.Label + TEXT("_class_loaded"), Pit.ClassPath);
			return false;
		}
		// Spawned as a level designer places it: Blueprint defaults, no owning building.
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Pit.Pit = World.SpawnActor<AActor>(PitClass, FTransform(Pit.SpawnLocation), SpawnParameters);
		Check(Pit.Pit.IsValid(), Pit.Label + TEXT("_spawned"), Pit.SpawnLocation.ToCompactString());
		if (not Pit.Pit.IsValid())
		{
			return false;
		}
		const int32 Owner = GetOwningPlayer(Pit.Pit.Get());
		Check(Owner == PlayerOwner, Pit.Label + TEXT("_owned_by_player"), FString::Printf(TEXT("owner=%d"), Owner));
		TArray<UStaticMeshComponent*> StaticMeshes;
		Pit.Pit->GetComponents<UStaticMeshComponent>(StaticMeshes);
		const FName PitMeshName = Pit.PitMeshName;
		const bool bShowsPit = StaticMeshes.ContainsByPredicate([PitMeshName](const UStaticMeshComponent* Mesh)
		{
			return IsValid(Mesh) && IsValid(Mesh->GetStaticMesh()) && Mesh->GetStaticMesh()->GetFName() == PitMeshName;
		});
		Check(bShowsPit, Pit.Label + TEXT("_shows_pit_mesh"), PitMeshName.ToString());
	}
	return true;
}

bool FGunPitTestScenario::FindTurret(FGunPitUnderTest& Pit) const
{
	using namespace GunPitTestPrivate;
	if (not Pit.Pit.IsValid())
	{
		return false;
	}
	TArray<UChildActorComponent*> ChildActors;
	Pit.Pit->GetComponents<UChildActorComponent>(ChildActors);
	for (const UChildActorComponent* ChildActor : ChildActors)
	{
		ACPPTurretsMaster* Turret = IsValid(ChildActor) ? Cast<ACPPTurretsMaster>(ChildActor->GetChildActor()) : nullptr;
		if (IsValid(Turret))
		{
			Pit.Turret = Turret;
			break;
		}
	}
	if (not Pit.Turret.IsValid())
	{
		return false;
	}
	Pit.Crew = Pit.Turret->FindComponentByClass<UGunPitCrewComponent>();
	TArray<USkeletalMeshComponent*> Meshes;
	Pit.Turret->GetComponents<USkeletalMeshComponent>(Meshes);
	Pit.CrewMeshes.Reset();
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->ComponentHasTag(CrewTag))
		{
			Pit.CrewMeshes.Add(Mesh);
			continue;
		}
		Pit.GunMesh = Mesh;
	}
	Pit.CrewMeshes.Sort([](const TWeakObjectPtr<USkeletalMeshComponent>& A, const TWeakObjectPtr<USkeletalMeshComponent>& B)
	{
		return A.IsValid() && B.IsValid() && A->GetName() < B->GetName();
	});
	TArray<UStaticMeshComponent*> StaticMeshes;
	Pit.Turret->GetComponents<UStaticMeshComponent>(StaticMeshes);
	Pit.Rounds.Reset();
	for (UStaticMeshComponent* StaticMesh : StaticMeshes)
	{
		if (StaticMesh->ComponentHasTag(RoundTag))
		{
			Pit.Rounds.Add(StaticMesh);
		}
	}
	return Pit.GunMesh.IsValid();
}

void FGunPitTestScenario::CheckAttachments(FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	const USkeletalMeshComponent* GunMesh = Pit.GunMesh.Get();
	if (not IsValid(GunMesh) || not Pit.Turret.IsValid())
	{
		Check(false, Pit.Label + TEXT("_gun_mesh_valid"));
		return;
	}
	Check(Pit.Crew.IsValid(), Pit.Label + TEXT("_has_crew_component"));
	Check(Pit.CrewMeshes.Num() == ExpectedCrewCount, Pit.Label + TEXT("_has_four_crew"),
	      FString::Printf(TEXT("crew=%d"), Pit.CrewMeshes.Num()));
	Check(Pit.Rounds.Num() == ExpectedRoundCount, Pit.Label + TEXT("_has_two_rounds"),
	      FString::Printf(TEXT("rounds=%d"), Pit.Rounds.Num()));
	const FTransform RootBone = GunMesh->GetBoneTransform(0, FTransform::Identity);
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s gun mesh=%s anim=%s component_scale=%s root_bone=%s "
	       "root_bone_scale=%s"), *Pit.Label, *GetNameSafe(GunMesh->GetSkeletalMeshAsset()),
	       *GetNameSafe(GunMesh->GetAnimInstance()), *GunMesh->GetComponentScale().ToCompactString(),
	       *GunMesh->GetBoneName(0).ToString(), *RootBone.GetScale3D().ToCompactString());
	const float OriginScale = GunMesh->GetSocketTransform(PitOriginSocket).GetScale3D().GetMax();
	Check(FMath::IsNearlyEqual(OriginScale, 1.f, MaximumScaleError), Pit.Label + TEXT("_pit_origin_unscaled"),
	      FString::Printf(TEXT("scale=%.3f"), OriginScale));
	const FTransform Frame = GetPitOriginFrame(*GunMesh);
	for (const TWeakObjectPtr<USkeletalMeshComponent>& CrewMesh : Pit.CrewMeshes)
	{
		const bool bIsAttached = CrewMesh.IsValid() && CrewMesh->GetAttachParent() == GunMesh &&
			CrewMesh->GetAttachSocketName() == PitOriginSocket;
		const float CrewScale = CrewMesh.IsValid() ? CrewMesh->GetComponentScale().GetMax() : 0.f;
		const FVector LocalSpot = CrewMesh.IsValid()
			                          ? Frame.InverseTransformPosition(CrewMesh->GetComponentLocation())
			                          : FVector::ZeroVector;
		Check(bIsAttached && FMath::IsNearlyEqual(CrewScale, 1.f, MaximumScaleError),
		      Pit.Label + TEXT("_crew_on_pit_origin"), FString::Printf(
			      TEXT("%s scale=%.3f local=%s"), CrewMesh.IsValid() ? *CrewMesh->GetName() : TEXT("none"), CrewScale,
			      *LocalSpot.ToCompactString()));
	}
	TArray<UStaticMeshComponent*> StaticMeshes;
	Pit.Turret->GetComponents<UStaticMeshComponent>(StaticMeshes);
	const bool bCrateOnOrigin = StaticMeshes.ContainsByPredicate([GunMesh](const UStaticMeshComponent* Mesh)
	{
		return IsValid(Mesh) && Mesh->GetFName() == ReadyCrateName && Mesh->GetAttachParent() == GunMesh &&
			Mesh->GetAttachSocketName() == PitOriginSocket;
	});
	Check(bCrateOnOrigin, Pit.Label + TEXT("_ready_crate_on_pit_origin"));
}

void FGunPitTestScenario::TickWaitingForTurrets(UWorld& World)
{
	bool bAllFound = true;
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		bAllFound &= Pit.GunMesh.IsValid() || FindTurret(Pit);
	}
	if (not bAllFound)
	{
		if (World.GetTimeSeconds() >= M_PhaseDeadlineSeconds)
		{
			Check(false, TEXT("pit_turrets_spawned"), TEXT("timed out"));
			Finish(World);
		}
		return;
	}
	Check(true, TEXT("pit_turrets_spawned"));
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		CheckAttachments(Pit);
	}
	EnterPhase(World, EGunPitTestPhase::WaitingForFirstRounds, GunPitTestPrivate::FirstRoundsTimeoutSeconds);
}

void FGunPitTestScenario::TickWaitingForFirstRounds(UWorld& World)
{
	using namespace GunPitTestPrivate;
	// The handler holds the first round out once his idle has played for a moment.
	const bool bAllHeld = not M_Pits.ContainsByPredicate([](const FGunPitUnderTest& Pit)
	{
		const UStaticMeshComponent* Round = Pit.Crew.IsValid() ? Pit.Crew->GetRoundInFlight() : nullptr;
		return not IsValid(Round) || not Round->IsVisible() || GetHoldingCrewMesh(*Round) == nullptr;
	});
	const bool bTimedOut = World.GetTimeSeconds() >= M_PhaseDeadlineSeconds;
	if (not bAllHeld && not bTimedOut)
	{
		return;
	}
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		const UStaticMeshComponent* Round = Pit.Crew.IsValid() ? Pit.Crew->GetRoundInFlight() : nullptr;
		const USkeletalMeshComponent* Holder = IsValid(Round) ? GetHoldingCrewMesh(*Round) : nullptr;
		Check(IsValid(Holder) && Round->IsVisible(), Pit.Label + TEXT("_handler_holds_first_round"),
		      FString::Printf(TEXT("holder=%s hand_distance=%.1f"), *GetNameSafe(Holder),
		                      IsValid(Holder) ? GetDistanceToHand(*Round, *Holder) : -1.f));
		CapturePit(World, Pit, TEXT("idle"));
		OrderAttackGround(World, Pit);
	}
	EnterPhase(World, EGunPitTestPhase::Observing, ObserveTimeoutSeconds);
}

void FGunPitTestScenario::OrderAttackGround(UWorld& World, FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	const float TargetDistance = Pit.Turret.IsValid() ? Pit.Turret->GetTurretRange() * GroundTargetRangeFraction : 0.f;
	const FVector Forward = Pit.Pit.IsValid() ? Pit.Pit->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	const FVector Direction = Forward.RotateAngleAxis(GroundTargetYawOffsetDegrees, FVector::UpVector);
	Pit.GroundTarget = Pit.SpawnLocation + Direction * TargetDistance;
	ProjectToNav(World, Pit.GroundTarget, Pit.GroundTarget);
	ICommands* Commands = Cast<ICommands>(Pit.Pit.Get());
	const ECommandQueueError AttackError = Commands
		                                       ? Commands->AttackGround(Pit.GroundTarget, true)
		                                       : ECommandQueueError::AbilityNotAllowed;
	Check(AttackError == ECommandQueueError::NoError, Pit.Label + TEXT("_attack_ground_accepted"),
	      FString::Printf(TEXT("range=%.0f distance=%.0f"), Pit.Turret.IsValid() ? Pit.Turret->GetTurretRange() : 0.f,
	                      TargetDistance));
	const UWeaponState* Weapon = Pit.Turret.IsValid() ? Pit.Turret->GetLargestCalibreWeapon() : nullptr;
	Pit.LastMagazine = IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : INDEX_NONE;
}

void FGunPitTestScenario::TickObserving(UWorld& World)
{
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		GunPitTestPrivate::SetFullHealth(Pit.Pit.Get());
		CountShots(World, Pit);
		if (not Pit.ShotSeconds.IsEmpty() && not Pit.bTraverseChecked)
		{
			CheckTraverse(Pit);
		}
		WatchDrill(World, Pit);
	}
	ReportStatus(World);
	const bool bAllDone = not M_Pits.ContainsByPredicate([this](const FGunPitUnderTest& Pit)
	{
		return not GetIsPitDone(Pit);
	});
	if (bAllDone)
	{
		Finish(World);
		return;
	}
	if (World.GetTimeSeconds() < M_PhaseDeadlineSeconds)
	{
		return;
	}
	for (const FGunPitUnderTest& Pit : M_Pits)
	{
		Check(GetIsPitDone(Pit), Pit.Label + TEXT("_fired_twice_with_one_drill"), FString::Printf(
			      TEXT("shots=%d drill_checked=%d traverse_checked=%d"), Pit.ShotSeconds.Num(), Pit.bDrillChecked,
			      Pit.bTraverseChecked));
	}
	Finish(World);
}

void FGunPitTestScenario::CountShots(const UWorld& World, FGunPitUnderTest& Pit) const
{
	const UWeaponState* Weapon = Pit.Turret.IsValid() ? Pit.Turret->GetLargestCalibreWeapon() : nullptr;
	if (not IsValid(Weapon))
	{
		return;
	}
	const int32 Magazine = Weapon->GetCurrentMagCapacity();
	if (Pit.LastMagazine != INDEX_NONE && Magazine < Pit.LastMagazine)
	{
		Pit.ShotSeconds.Add(World.GetTimeSeconds());
		UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s shot t=%.2f mag=%d"), *Pit.Label,
		       World.GetTimeSeconds(), Magazine);
	}
	Pit.LastMagazine = Magazine;
}

void FGunPitTestScenario::CheckTraverse(FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	Pit.bTraverseChecked = true;
	if (not Pit.GunMesh.IsValid())
	{
		Check(false, Pit.Label + TEXT("_gun_turned_to_target"), TEXT("no gun mesh"));
		return;
	}
	const FTransform Frame = GetPitOriginFrame(*Pit.GunMesh);
	const FVector ToTarget = (Pit.GroundTarget - Frame.GetLocation()).GetSafeNormal2D();
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Frame.Rotator().Yaw, ToTarget.Rotation().Yaw));
	Check(YawError < MaximumAimYawErrorDegrees, Pit.Label + TEXT("_gun_turned_to_target"),
	      FString::Printf(TEXT("yaw_error=%.1f pit_origin_yaw=%.1f target_yaw=%.1f"), YawError, Frame.Rotator().Yaw,
	                      ToTarget.Rotation().Yaw));
	const USkeletalMeshComponent* Loader = Pit.CrewMeshes.IsEmpty() ? nullptr : Pit.CrewMeshes[0].Get();
	const FVector LoaderLocal = IsValid(Loader) ? Frame.InverseTransformPosition(Loader->GetComponentLocation()) : FVector::ZeroVector;
	const float SpotError = FVector::Dist2D(LoaderLocal, Pit.LoaderSpot);
	Check(IsValid(Loader) && SpotError < MaximumLoaderSpotError, Pit.Label + TEXT("_crew_turned_with_gun"),
	      FString::Printf(TEXT("loader_local=%s error=%.1f"), *LoaderLocal.ToCompactString(), SpotError));
}

void FGunPitTestScenario::WatchDrill(UWorld& World, FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	if (Pit.bDrillChecked || not Pit.Crew.IsValid())
	{
		return;
	}
	const float Now = World.GetTimeSeconds();
	const bool bIsLoading = Pit.Crew->GetIsLoading();
	if (bIsLoading && not Pit.bWasLoading)
	{
		Pit.DrillStartSeconds = Now;
		Pit.RoundAtDrillStart = Pit.Crew->GetRoundInFlight();
		const UStaticMeshComponent* Round = Pit.RoundAtDrillStart.Get();
		Pit.HandlerAtDrillStart = IsValid(Round) ? GetHoldingCrewMesh(*Round) : nullptr;
		Pit.NextCaptureSeconds = Now;
	}
	Pit.bWasLoading = bIsLoading;
	if (Pit.DrillStartSeconds < 0.f)
	{
		return;
	}
	UStaticMeshComponent* Round = Pit.RoundAtDrillStart.Get();
	USkeletalMeshComponent* Holder = IsValid(Round) ? GetHoldingCrewMesh(*Round) : nullptr;
	if (Pit.HandoverSeconds < 0.f && IsValid(Holder) && Holder != Pit.HandlerAtDrillStart.Get())
	{
		Pit.HandoverSeconds = Now;
		Pit.LoaderAtHandover = Holder;
		Pit.HandoverHandDistance = GetDistanceToHand(*Round, *Holder);
		LogHandover(Pit, *Round, *Holder);
		CapturePit(World, Pit, TEXT("handover"));
	}
	if (Pit.RamSeconds < 0.f && IsValid(Round) && not Round->IsVisible())
	{
		Pit.RamSeconds = Now;
		CapturePit(World, Pit, TEXT("rammed"));
	}
	for (const TWeakObjectPtr<UStaticMeshComponent>& Other : Pit.Rounds)
	{
		USkeletalMeshComponent* OtherHolder = Other.IsValid() && Other.Get() != Round ? GetHoldingCrewMesh(*Other) : nullptr;
		if (Pit.PickSeconds >= 0.f || not IsValid(OtherHolder) || not Other->IsVisible())
		{
			continue;
		}
		Pit.PickSeconds = Now;
		Pit.HolderOfNextRound = OtherHolder;
		Pit.PickHandDistance = GetDistanceToHand(*Other, *OtherHolder);
		CapturePit(World, Pit, TEXT("picked"));
	}
	if (M_CaptureRig.GetIsEnabled() && bIsLoading && Now >= Pit.NextCaptureSeconds)
	{
		CapturePit(World, Pit, FString::Printf(TEXT("drill_t%04.1f"), Now - Pit.DrillStartSeconds));
		Pit.NextCaptureSeconds = Now + DrillCaptureIntervalSeconds;
	}
	if (not bIsLoading)
	{
		Pit.DrillEndSeconds = Now;
		CheckDrill(Pit);
	}
}

void FGunPitTestScenario::CheckDrill(FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	Pit.bDrillChecked = true;
	const float DrillSeconds = Pit.DrillEndSeconds - Pit.DrillStartSeconds;
	auto Fraction = [&Pit, DrillSeconds](const float EventSeconds)
	{
		return EventSeconds < 0.f || DrillSeconds <= 0.f ? -1.f : (EventSeconds - Pit.DrillStartSeconds) / DrillSeconds;
	};
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s drill seconds=%.2f handover=%.3f pick=%.3f ram=%.3f "
	       "handover_hand=%.1f pick_hand=%.1f"), *Pit.Label, DrillSeconds, Fraction(Pit.HandoverSeconds),
	       Fraction(Pit.PickSeconds), Fraction(Pit.RamSeconds), Pit.HandoverHandDistance, Pit.PickHandDistance);
	Check(Pit.HandlerAtDrillStart.IsValid(), Pit.Label + TEXT("_drill_starts_with_round_in_handler_hands"));
	Check(Pit.HandoverSeconds >= 0.f && Pit.LoaderAtHandover.Get() != Pit.HandlerAtDrillStart.Get(),
	      Pit.Label + TEXT("_round_handed_to_loader"), GetNameSafe(Pit.LoaderAtHandover.Get()));
	Check(Pit.RamSeconds > Pit.HandoverSeconds && Pit.HandoverSeconds >= 0.f, Pit.Label + TEXT("_round_rammed_after_handover"));
	Check(Pit.PickSeconds >= 0.f && Pit.HolderOfNextRound.Get() == Pit.HandlerAtDrillStart.Get(),
	      Pit.Label + TEXT("_handler_picks_next_round"), GetNameSafe(Pit.HolderOfNextRound.Get()));
	Check(Pit.HandoverHandDistance >= 0.f && Pit.HandoverHandDistance < MaximumRoundToHandDistance,
	      Pit.Label + TEXT("_loader_hand_on_round"), FString::Printf(TEXT("distance=%.1f"), Pit.HandoverHandDistance));
	Check(Pit.PickHandDistance >= 0.f && Pit.PickHandDistance < MaximumRoundToHandDistance,
	      Pit.Label + TEXT("_handler_hand_on_picked_round"), FString::Printf(TEXT("distance=%.1f"), Pit.PickHandDistance));
}

void FGunPitTestScenario::LogHandover(const FGunPitUnderTest& Pit, const UStaticMeshComponent& Round,
                                      const USkeletalMeshComponent& Loader) const
{
	using namespace GunPitTestPrivate;
	if (not Pit.GunMesh.IsValid())
	{
		return;
	}
	// In the PitOrigin frame, the space of the crew bake, so the numbers compare with gerpit_*_carry.json.
	const FTransform Frame = GetPitOriginFrame(*Pit.GunMesh);
	const FVector RoundLocal = Frame.InverseTransformPosition(Round.GetComponentLocation());
	const FVector RightLocal = Frame.InverseTransformPosition(Loader.GetBoneLocation(HandBone));
	const FVector LeftLocal = Frame.InverseTransformPosition(Loader.GetBoneLocation(TEXT("hand_l")));
	const FVector RootLocal = Frame.InverseTransformPosition(Loader.GetComponentLocation());
	const float LoaderYaw = Frame.InverseTransformRotation(Loader.GetComponentQuat()).Rotator().Yaw;
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s handover round=%s loader_hand_r=%s loader_hand_l=%s "
	       "loader_root=%s loader_yaw=%.1f"), *Pit.Label, *RoundLocal.ToCompactString(), *RightLocal.ToCompactString(),
	       *LeftLocal.ToCompactString(), *RootLocal.ToCompactString(), LoaderYaw);
}

bool FGunPitTestScenario::GetIsPitDone(const FGunPitUnderTest& Pit) const
{
	return Pit.ShotSeconds.Num() >= GunPitTestPrivate::ShotsToObserve && Pit.bDrillChecked && Pit.bTraverseChecked;
}

void FGunPitTestScenario::KeepMeshesAnimating() const
{
	for (const FGunPitUnderTest& Pit : M_Pits)
	{
		GunPitTestPrivate::ForceAnimate(Pit.GunMesh.Get());
		for (const TWeakObjectPtr<USkeletalMeshComponent>& CrewMesh : Pit.CrewMeshes)
		{
			GunPitTestPrivate::ForceAnimate(CrewMesh.Get());
		}
	}
}

void FGunPitTestScenario::ReportStatus(const UWorld& World)
{
	if (World.GetTimeSeconds() < M_NextStatusSeconds)
	{
		return;
	}
	M_NextStatusSeconds = World.GetTimeSeconds() + GunPitTestPrivate::StatusIntervalSeconds;
	for (const FGunPitUnderTest& Pit : M_Pits)
	{
		const ACPPTurretsMaster* Turret = Pit.Turret.Get();
		const UWeaponState* Weapon = IsValid(Turret) ? Turret->GetLargestCalibreWeapon() : nullptr;
		const float OriginYaw = Pit.GunMesh.IsValid() ? GunPitTestPrivate::GetPitOriginFrame(*Pit.GunMesh).Rotator().Yaw : 0.f;
		UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST status %s t=%.1f mag=%d reloading=%d loading=%d "
		       "rotated_to_engage=%d pit_origin_yaw=%.1f target_yaw=%.1f shots=%d"), *Pit.Label, World.GetTimeSeconds(),
		       IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : -1, IsValid(Weapon) && Weapon->GetIsReloading(),
		       Pit.Crew.IsValid() && Pit.Crew->GetIsLoading(), IsValid(Turret) && Turret->GetIsRotatedToEngage(), OriginYaw,
		       (Pit.GroundTarget - Pit.SpawnLocation).GetSafeNormal2D().Rotation().Yaw, Pit.ShotSeconds.Num());
	}
}

void FGunPitTestScenario::CapturePit(UWorld& World, const FGunPitUnderTest& Pit, const FString& Moment)
{
	if (not M_CaptureRig.GetIsEnabled() || not Pit.GunMesh.IsValid())
	{
		return;
	}
	M_CaptureRig.CaptureViews(World, GunPitTestPrivate::GetPitOriginFrame(*Pit.GunMesh),
	                          GunPitTestPrivate::GetPitViews(Pit.ViewScale), Pit.Label + TEXT("_") + Moment);
}

#endif
