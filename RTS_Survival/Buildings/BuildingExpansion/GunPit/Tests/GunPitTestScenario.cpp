#include "GunPitTestScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
#include "RTS_Survival/Weapons/Turret/TurretAnimInstance/TurretAnimInstance.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGunPitTest, Log, All);

namespace GunPitTestPrivate
{
	const FName CrewTag = TEXT("PitCrew");
	const FName RoundTag = TEXT("PitRound");
	const FName HandBone = TEXT("hand_r");
	const FName ReadyCrateName = TEXT("ReadyCrate");
	const FName MuzzleSocket = TEXT("Muzzle");

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
	// A sector pit is first ordered this far past its arc's edge, then this far inside it.
	constexpr float SectorOutsideMarginDegrees = 35.f;
	constexpr float SectorInsideMarginDegrees = 15.f;
	constexpr float MaximumAttachOffset = 1.f;
	constexpr float MaximumScaleError = 0.01f;
	constexpr float MaximumLoaderSpotError = 30.f;
	constexpr float MaximumGunHeightError = 1.f;
	// From the round's pivot to the palm bone that holds it; the bake keeps the palms on the round's body.
	constexpr float MaximumRoundToHandDistance = 70.f;
	constexpr float StatusIntervalSeconds = 5.f;
	constexpr float DrillCaptureIntervalSeconds = 1.f;

	const TCHAR* const PitRoot = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/");
	const FVector PakLoaderSpot(-148.6f, 76.f, 0.f);
	const FVector HowitzerLoaderSpot(-234.5f, 39.7f, 0.f);
	// The Flak 37 team weapon's loader spot; his animation stands him on the gun's platform beside the feed.
	const FVector FlakLoaderSpot(-97.92f, 85.05f, 0.f);
	constexpr float PakViewScale = 0.8f;

	const TCHAR* const DShKPitClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit/Rus_BXP_DShKPit.Rus_BXP_DShKPit_C");
	constexpr int32 EnemyOwner = 2;
	constexpr int32 DShKCrewCount = 2;
	constexpr float DShKViewScale = 0.55f;
	const FName LeftHandBone = TEXT("hand_l");
	constexpr int32 GunnerCrewIndex = 0;
	constexpr int32 DrillLoaderCrewIndex = 1;
	// The loader holds a box by its ends (or the ampoule by its sides, the lever by its knob): palm bone to the bone is
	// half the prop plus the hand.
	constexpr float MaximumHandToDrillPropDistance = 35.f;
	// The empty box goes from the gun down onto the spare stack; the ampoule from the crate into the breech.
	constexpr float MinimumDrillPropTravel = 30.f;

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
			MakePit(TEXT("flak37"), TEXT("Flak37"), TEXT("SM_GerPit_Flak"), FlakLoaderSpot, PakViewScale),
		};
	}

	TArray<FGunPitUnderTest> GetDShKPitToTest()
	{
		FGunPitUnderTest Pit;
		Pit.Label = TEXT("dshk");
		Pit.ClassPath = DShKPitClassPath;
		Pit.PitMeshName = TEXT("SM_DShKPit");
		// The crew are snapped to PitOrigin; their animations carry their spots.
		Pit.LoaderSpot = FVector::ZeroVector;
		Pit.ViewScale = DShKViewScale;
		Pit.ExpectedOwner = EnemyOwner;
		Pit.ExpectedCrewCount = DShKCrewCount;
		Pit.bCarriesRounds = false;
		Pit.bHasReadyCrate = false;
		Pit.bWatchesFireReaction = false;
		return {Pit};
	}

	const TCHAR* const AmpulometPitClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusAmpulometPit/Rus_BXP_AmpulometPit.Rus_BXP_AmpulometPit_C");
	constexpr int32 AmpulometCrewCount = 2;
	constexpr float AmpulometViewScale = 0.55f;

	TArray<FGunPitUnderTest> GetAmpulometPitToTest()
	{
		FGunPitUnderTest Pit;
		Pit.Label = TEXT("ampulomet");
		Pit.ClassPath = AmpulometPitClassPath;
		Pit.PitMeshName = TEXT("SM_AmpulometPit");
		// The crew are snapped to PitOrigin; their animations carry their spots.
		Pit.LoaderSpot = FVector::ZeroVector;
		Pit.ViewScale = AmpulometViewScale;
		Pit.ExpectedOwner = EnemyOwner;
		Pit.ExpectedCrewCount = AmpulometCrewCount;
		Pit.bCarriesRounds = false;
		Pit.bHasReadyCrate = false;
		Pit.DrillPropBone = TEXT("Ampoule");
		Pit.DrillSecondBone = TEXT("BreechCap");
		// Both men stand idle at the shot: the reload starts at the next fire attempt, after the cooldown.
		Pit.FireReaction.FirstIdleCrewIndex = 0;
		// The ampoule flies like the Bazooka's rocket, not in a lob.
		Pit.bFiresDirect = true;
		return {Pit};
	}

	constexpr int32 GerSmallPitCrewCount = 2;
	constexpr float PuppchenViewScale = 0.5f;
	constexpr float PanzerschreckViewScale = 0.45f;

	TArray<FGunPitUnderTest> GetGerSmallPitsToTest()
	{
		// The player's two-man field defenses: the loader carries a rocket from the crate into the weapon.
		auto MakeSmallPit = [](const TCHAR* Label, const TCHAR* Name, const TCHAR* PitMesh, const TCHAR* SecondBone,
		                       const float ViewScale)
		{
			FGunPitUnderTest Pit;
			Pit.Label = Label;
			Pit.ClassPath = FString::Printf(TEXT("%sGer_BXP_GerPit_%s.Ger_BXP_GerPit_%s_C"), PitRoot, Name, Name);
			Pit.PitMeshName = PitMesh;
			Pit.LoaderSpot = FVector::ZeroVector;
			Pit.ViewScale = ViewScale;
			Pit.ExpectedCrewCount = GerSmallPitCrewCount;
			Pit.bCarriesRounds = false;
			Pit.bHasReadyCrate = false;
			Pit.DrillPropBone = TEXT("Rocket");
			Pit.DrillSecondBone = SecondBone;
			// Both men stand idle at the shot: the reload starts at the next fire attempt, after the cooldown.
			Pit.FireReaction.FirstIdleCrewIndex = 0;
			return Pit;
		};
		return {
			MakeSmallPit(TEXT("puppchen"), TEXT("Puppchen"), TEXT("SM_GerPuppchenPit"), TEXT("BreechBlock"),
			             PuppchenViewScale),
			MakeSmallPit(TEXT("panzerschreck"), TEXT("Panzerschreck"), TEXT("SM_GerPanzerschreckPit"), TEXT("Terminal"),
			             PanzerschreckViewScale),
		};
	}

	const TCHAR* const At53KPitClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit.Rus_BXP_53KPit_C");

	TArray<FGunPitUnderTest> GetAt53KPitToTest()
	{
		// The German Pak 40 pit's crew and drill on the Soviet 45 mm 53-K: same spots, enemy owned.
		FGunPitUnderTest Pit;
		Pit.Label = TEXT("53k");
		Pit.ClassPath = At53KPitClassPath;
		Pit.PitMeshName = TEXT("SM_53KPit");
		Pit.LoaderSpot = PakLoaderSpot;
		Pit.ViewScale = PakViewScale;
		Pit.ExpectedOwner = EnemyOwner;
		Pit.PitOriginSocket = TEXT("PitOrigin53K");
		return {Pit};
	}

	const TCHAR* const BS3PitClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit.Rus_BXP_BS3Pit_C");
	// The Pak 40 pit's loader spot turned 20 degrees about the BS-3's breech, so the whole drill fits between the
	// BS-3's wide trails (bs3_crew.py writes it to Exports/Crew/bs3_crew_layout.json).
	const FVector BS3LoaderSpot(-184.75f, 45.22f, 0.f);
	constexpr float BS3ViewScale = 1.25f;

	TArray<FGunPitUnderTest> GetBS3PitToTest()
	{
		FGunPitUnderTest Pit;
		Pit.Label = TEXT("bs3");
		Pit.ClassPath = BS3PitClassPath;
		Pit.PitMeshName = TEXT("SM_BS3Pit");
		Pit.LoaderSpot = BS3LoaderSpot;
		Pit.ViewScale = BS3ViewScale;
		Pit.ExpectedOwner = EnemyOwner;
		Pit.PitOriginSocket = TEXT("PitOriginBS3");
		return {Pit};
	}

	const TCHAR* const At53KSectorPitClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KSectorPit.Rus_BXP_53KSectorPit_C");
	constexpr float At53KSectorHalfDegrees = 45.f;

	TArray<FGunPitUnderTest> GetAt53KSectorPitToTest()
	{
		// The 53-K pit's loader and handover in a small square pit; the crate and commander are pulled in.
		FGunPitUnderTest Pit = GetAt53KPitToTest()[0];
		Pit.Label = TEXT("53ksector");
		Pit.ClassPath = At53KSectorPitClassPath;
		Pit.PitMeshName = TEXT("SM_53KSectorPit");
		Pit.SectorHalfDegrees = At53KSectorHalfDegrees;
		return {Pit};
	}

	const TCHAR* const RailPakBunkerClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/RailPakBunker/"
			"Ger_BXP_RailPakBunker.Ger_BXP_RailPakBunker_C");

	TArray<FGunPitUnderTest> GetRailPakBunkerToTest()
	{
		// The Pak 40 pit's gun, crew and drill on the bunker roof; the gun's barrel is the rail Pak's.
		FGunPitUnderTest Pit;
		Pit.Label = TEXT("railpak");
		Pit.ClassPath = RailPakBunkerClassPath;
		Pit.PitMeshName = TEXT("SM_GerRailPakBunker");
		Pit.LoaderSpot = PakLoaderSpot;
		return {Pit};
	}

	const TCHAR* const Flak88PitClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak88.Ger_BXP_GerPit_Flak88_C");
	constexpr int32 Flak88CrewCount = 2;
	// The 88 stands on the pit's ground (flak88_pit_layout.json gun_sink 0): its legs show, level with the deck.
	constexpr float Flak88GunHeight = 0.f;
	// The runner's round as he takes it from the niche and as he lays it on his crate, on PitStatic
	// (flak88_pit_crew.py: NICHE_ROUND and the drop crate's round).
	const FVector Flak88RunnerNicheRound(112.f, -376.55f, 44.68f);
	const FVector Flak88RunnerDropRound(-200.70f, -168.41f, 42.4f);

	TArray<FGunPitUnderTest> GetGerFlak88PitToTest()
	{
		// The player's 88 emplacement: the handler hands the loader rounds from the crate ring, which the runner
		// keeps filled from the niche.
		FGunPitUnderTest Pit;
		Pit.Label = TEXT("flak88");
		Pit.ClassPath = Flak88PitClassPath;
		Pit.PitMeshName = TEXT("SM_GerFlak88Pit");
		// The crew are snapped to PitOrigin; their animations carry their spots.
		Pit.LoaderSpot = FVector::ZeroVector;
		Pit.ExpectedCrewCount = Flak88CrewCount;
		Pit.bChecksGunHeight = true;
		Pit.ExpectedGunHeight = Flak88GunHeight;
		Pit.bHasReadyCrate = false;
		// Both men stand idle at the shot: the reload starts at the next fire attempt, after the cooldown.
		Pit.FireReaction.FirstIdleCrewIndex = 0;
		Pit.Runner.RunnerTag = TEXT("PitRunner");
		Pit.Runner.StaticSocket = TEXT("PitStatic");
		Pit.Runner.RoundName = TEXT("RunnerRound");
		Pit.Runner.NicheRound = Flak88RunnerNicheRound;
		Pit.Runner.DropRound = Flak88RunnerDropRound;
		return {Pit};
	}

	float GetFirstTargetYawOffset(const FGunPitUnderTest& Pit)
	{
		return Pit.SectorHalfDegrees > 0.f ? Pit.SectorHalfDegrees + SectorOutsideMarginDegrees : GroundTargetYawOffsetDegrees;
	}

	FString GetActiveMontageName(const USkeletalMeshComponent* Mesh)
	{
		const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		return IsValid(Montage) ? Montage->GetName() : FString();
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

	FTransform GetPitOriginFrame(const USkeletalMeshComponent& GunMesh, const FName PitOriginSocket)
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
	if (FParse::Param(FCommandLine::Get(), TEXT("DShKPitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetDShKPitToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("RailPakBunkerValidate")))
	{
		M_Pits = GunPitTestPrivate::GetRailPakBunkerToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("At53KSectorPitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetAt53KSectorPitToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("At53KPitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetAt53KPitToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("BS3PitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetBS3PitToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("AmpulometPitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetAmpulometPitToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("GerSmallPitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetGerSmallPitsToTest();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("GerFlak88PitValidate")))
	{
		M_Pits = GunPitTestPrivate::GetGerFlak88PitToTest();
	}
	else
	{
		M_Pits = GunPitTestPrivate::GetPitsToTest();
	}
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
	case EGunPitTestPhase::HoldingOutsideSector:
		TickHoldingOutsideSector(*World);
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
		Check(Owner == Pit.ExpectedOwner, Pit.Label + TEXT("_owned_as_placed"), FString::Printf(TEXT("owner=%d"), Owner));
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
	// A pit with a runner has a second crew component for him; the gun crew's is the one tagged CrewTag.
	TArray<UGunPitCrewComponent*> CrewComponents;
	Pit.Turret->GetComponents<UGunPitCrewComponent>(CrewComponents);
	UGunPitCrewComponent* const* GunCrew = CrewComponents.FindByPredicate([](const UGunPitCrewComponent* Crew)
	{
		return IsValid(Crew) && Crew->GetCrewTag() == CrewTag;
	});
	Pit.Crew = GunCrew ? *GunCrew : nullptr;
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
		if (Pit.Runner.GetIsUsed() && Mesh->ComponentHasTag(Pit.Runner.RunnerTag))
		{
			continue;
		}
		Pit.GunMesh = Mesh;
	}
	if (Pit.Runner.GetIsUsed())
	{
		Pit.Runner.Find(*Pit.Turret);
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
	Check(Pit.CrewMeshes.Num() == Pit.ExpectedCrewCount, Pit.Label + TEXT("_has_its_crew"),
	      FString::Printf(TEXT("crew=%d"), Pit.CrewMeshes.Num()));
	const int32 ExpectedRounds = Pit.bCarriesRounds ? ExpectedRoundCount : 0;
	Check(Pit.Rounds.Num() == ExpectedRounds, Pit.Label + TEXT("_has_its_rounds"),
	      FString::Printf(TEXT("rounds=%d"), Pit.Rounds.Num()));
	const FTransform RootBone = GunMesh->GetBoneTransform(0, FTransform::Identity);
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s gun mesh=%s anim=%s component_scale=%s root_bone=%s "
	       "root_bone_scale=%s"), *Pit.Label, *GetNameSafe(GunMesh->GetSkeletalMeshAsset()),
	       *GetNameSafe(GunMesh->GetAnimInstance()), *GunMesh->GetComponentScale().ToCompactString(),
	       *GunMesh->GetBoneName(0).ToString(), *RootBone.GetScale3D().ToCompactString());
	if (Pit.bChecksGunHeight && Pit.Pit.IsValid())
	{
		const float GunHeight = GunMesh->GetComponentLocation().Z - Pit.Pit->GetActorLocation().Z;
		Check(FMath::IsNearlyEqual(GunHeight, Pit.ExpectedGunHeight, MaximumGunHeightError),
		      Pit.Label + TEXT("_gun_at_pit_height"), FString::Printf(TEXT("gun_height=%.1f"), GunHeight));
	}
	const float OriginScale = GunMesh->GetSocketTransform(Pit.PitOriginSocket).GetScale3D().GetMax();
	Check(FMath::IsNearlyEqual(OriginScale, 1.f, MaximumScaleError), Pit.Label + TEXT("_pit_origin_unscaled"),
	      FString::Printf(TEXT("scale=%.3f"), OriginScale));
	const FTransform Frame = GetPitOriginFrame(*GunMesh, Pit.PitOriginSocket);
	for (const TWeakObjectPtr<USkeletalMeshComponent>& CrewMesh : Pit.CrewMeshes)
	{
		const bool bIsAttached = CrewMesh.IsValid() && CrewMesh->GetAttachParent() == GunMesh &&
			CrewMesh->GetAttachSocketName() == Pit.PitOriginSocket;
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
	const FName PitOriginSocket = Pit.PitOriginSocket;
	const bool bCrateOnOrigin = StaticMeshes.ContainsByPredicate([GunMesh, PitOriginSocket](const UStaticMeshComponent* Mesh)
	{
		return IsValid(Mesh) && Mesh->GetFName() == ReadyCrateName && Mesh->GetAttachParent() == GunMesh &&
			Mesh->GetAttachSocketName() == PitOriginSocket;
	});
	if (Pit.bHasReadyCrate)
	{
		Check(bCrateOnOrigin, Pit.Label + TEXT("_ready_crate_on_pit_origin"));
	}
	if (Pit.Runner.GetIsUsed())
	{
		Check(Pit.Runner.GetIsOnStaticSocket(*GunMesh), Pit.Label + TEXT("_runner_on_static_socket"),
		      FString::Printf(TEXT("runner=%s"), *GetNameSafe(Pit.Runner.GetRunner())));
	}
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
		if (not Pit.bCarriesRounds)
		{
			return false;
		}
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
		if (Pit.bCarriesRounds)
		{
			Check(IsValid(Holder) && Round->IsVisible(), Pit.Label + TEXT("_handler_holds_first_round"),
			      FString::Printf(TEXT("holder=%s hand_distance=%.1f"), *GetNameSafe(Holder),
			                      IsValid(Holder) ? GetDistanceToHand(*Round, *Holder) : -1.f));
		}
		CapturePit(World, Pit, TEXT("idle"));
		OrderAttackGround(World, Pit, GetFirstTargetYawOffset(Pit));
	}
	const bool bHasSectorPit = M_Pits.ContainsByPredicate([](const FGunPitUnderTest& Pit)
	{
		return Pit.SectorHalfDegrees > 0.f;
	});
	if (bHasSectorPit)
	{
		EnterPhase(World, EGunPitTestPhase::HoldingOutsideSector, FGunPitSectorArcWatch::HoldSeconds);
		return;
	}
	EnterPhase(World, EGunPitTestPhase::Observing, ObserveTimeoutSeconds);
}

void FGunPitTestScenario::TickHoldingOutsideSector(UWorld& World)
{
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		GunPitTestPrivate::SetFullHealth(Pit.Pit.Get());
		const int32 ShotsBefore = Pit.ShotSeconds.Num();
		CountShots(World, Pit);
		Pit.SectorArc.Sample(GetGunYawFromPitForward(Pit), Pit.ShotSeconds.Num() > ShotsBefore);
	}
	ReportStatus(World);
	if (World.GetTimeSeconds() < M_PhaseDeadlineSeconds)
	{
		return;
	}
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		if (Pit.SectorHalfDegrees > 0.f)
		{
			CheckSectorArc(World, Pit);
		}
	}
	EnterPhase(World, EGunPitTestPhase::Observing, GunPitTestPrivate::ObserveTimeoutSeconds);
}

void FGunPitTestScenario::CheckSectorArc(UWorld& World, FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	const FString Details = Pit.SectorArc.Describe();
	Check(Pit.SectorArc.GetHoldsAtLimit(Pit.SectorHalfDegrees), Pit.Label + TEXT("_gun_holds_at_sector_limit"), Details);
	Check(Pit.SectorArc.GetStaysInsideArc(Pit.SectorHalfDegrees), Pit.Label + TEXT("_gun_never_leaves_sector"), Details);
	Check(Pit.SectorArc.GetHeldFire(), Pit.Label + TEXT("_holds_fire_outside_sector"), Details);
	CapturePit(World, Pit, TEXT("sector_limit"));
	const FVector OutsideTarget = Pit.GroundTarget;
	Pit.ShotSeconds.Reset();
	OrderAttackGround(World, Pit, Pit.SectorHalfDegrees - SectorInsideMarginDegrees);
	// The auto-engage search uses the same test to skip targets the gun can never face.
	const bool bSkipsOutside = Pit.Turret.IsValid() && not Pit.Turret->GetCanTraverseToLocation(OutsideTarget);
	const bool bTakesInside = Pit.Turret.IsValid() && Pit.Turret->GetCanTraverseToLocation(Pit.GroundTarget);
	Check(bSkipsOutside && bTakesInside, Pit.Label + TEXT("_target_search_keeps_to_sector"),
	      FString::Printf(TEXT("skips_outside=%d takes_inside=%d"), bSkipsOutside, bTakesInside));
}

float FGunPitTestScenario::GetGunYawFromPitForward(const FGunPitUnderTest& Pit) const
{
	if (not Pit.GunMesh.IsValid() || not Pit.Pit.IsValid())
	{
		return 0.f;
	}
	const float GunYaw = GunPitTestPrivate::GetPitOriginFrame(*Pit.GunMesh, Pit.PitOriginSocket).Rotator().Yaw;
	return FMath::FindDeltaAngleDegrees(Pit.Pit->GetActorRotation().Yaw, GunYaw);
}

void FGunPitTestScenario::OrderAttackGround(UWorld& World, FGunPitUnderTest& Pit, const float YawOffsetDegrees)
{
	using namespace GunPitTestPrivate;
	const float TargetDistance = Pit.Turret.IsValid() ? Pit.Turret->GetTurretRange() * GroundTargetRangeFraction : 0.f;
	const FVector Forward = Pit.Pit.IsValid() ? Pit.Pit->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	const FVector Direction = Forward.RotateAngleAxis(YawOffsetDegrees, FVector::UpVector);
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
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s weapon=%s data_range=%.0f reload=%.1f"), *Pit.Label,
	       IsValid(Weapon) ? *UEnum::GetValueAsString(Weapon->GetRawWeaponData().WeaponName) : TEXT("none"),
	       IsValid(Weapon) ? Weapon->GetRawWeaponData().Range : 0.f,
	       IsValid(Weapon) ? Weapon->GetRawWeaponData().ReloadSpeed : 0.f);
}

void FGunPitTestScenario::TickObserving(UWorld& World)
{
	for (FGunPitUnderTest& Pit : M_Pits)
	{
		GunPitTestPrivate::SetFullHealth(Pit.Pit.Get());
		CountShots(World, Pit);
		WatchProjectileFlight(World, Pit);
		if (not Pit.ShotSeconds.IsEmpty() && not Pit.bTraverseChecked)
		{
			CheckTraverse(Pit);
		}
		if (Pit.bWatchesFireReaction)
		{
			WatchFireReaction(World, Pit);
		}
		WatchRunner(World, Pit);
		if (Pit.bCarriesRounds)
		{
			WatchDrill(World, Pit);
			continue;
		}
		const USkeletalMeshComponent* Gunner = Pit.CrewMeshes.IsValidIndex(GunPitTestPrivate::GunnerCrewIndex)
			                                       ? Pit.CrewMeshes[GunPitTestPrivate::GunnerCrewIndex].Get()
			                                       : nullptr;
		Pit.bGunnerFireMontageSeen |= GunPitTestPrivate::GetActiveMontageName(Gunner).Contains(TEXT("Fire"));
		WatchBoneDrill(World, Pit);
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
		UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s shot t=%.2f mag=%d target=%s"), *Pit.Label,
		       World.GetTimeSeconds(), Magazine, *GetNameSafe(Pit.Turret->GetCurrentTargetActor()));
	}
	Pit.LastMagazine = Magazine;
}

void FGunPitTestScenario::WatchFireReaction(UWorld& World, FGunPitUnderTest& Pit)
{
	FGunPitFireReactionWatch& Watch = Pit.FireReaction;
	const USkeletalMeshComponent* GunMesh = Pit.GunMesh.Get();
	if (Watch.bChecked || Pit.ShotSeconds.IsEmpty() || not IsValid(GunMesh))
	{
		return;
	}
	if (not Watch.GetHasStarted())
	{
		Watch.Start(World.GetTimeSeconds(), *GunMesh, Pit.CrewMeshes);
		CapturePit(World, Pit, TEXT("shot_0.00"));
		return;
	}
	Watch.Sample(*GunMesh, Pit.CrewMeshes);
	// The kick peaks about 0.1 s after the shot; the crew hold the brace to 0.25 s.
	static const float CaptureMoments[] = {0.1f, 0.25f};
	if (Watch.CapturesTaken < UE_ARRAY_COUNT(CaptureMoments)
		&& Watch.GetSecondsSinceShot(World.GetTimeSeconds()) >= CaptureMoments[Watch.CapturesTaken])
	{
		CapturePit(World, Pit, FString::Printf(TEXT("shot_%.2f"), CaptureMoments[Watch.CapturesTaken]));
		++Watch.CapturesTaken;
	}
	if (not Watch.GetIsWatchOver(World.GetTimeSeconds()))
	{
		return;
	}
	Watch.bChecked = true;
	Check(Watch.GetDidGunBuck(), Pit.Label + TEXT("_gun_bucks_on_shot"), Watch.Describe());
	Check(Watch.GetDidCrewBrace(), Pit.Label + TEXT("_crew_brace_on_shot"), Watch.Describe());
}

void FGunPitTestScenario::WatchProjectileFlight(const UWorld& World, FGunPitUnderTest& Pit)
{
	FGunPitProjectileFlightWatch& Flight = Pit.Flight;
	const USkeletalMeshComponent* GunMesh = Pit.GunMesh.Get();
	if (Flight.bChecked || Pit.ShotSeconds.IsEmpty() || not IsValid(GunMesh))
	{
		return;
	}
	if (not Flight.GetHasStarted())
	{
		const FVector Muzzle = GunMesh->DoesSocketExist(GunPitTestPrivate::MuzzleSocket)
			                       ? GunMesh->GetSocketLocation(GunPitTestPrivate::MuzzleSocket)
			                       : GunMesh->GetComponentLocation();
		Flight.Start(Pit.ShotSeconds[0], Muzzle);
	}
	Flight.Sample(World);
	if (not Flight.GetIsFlightOver())
	{
		return;
	}
	Flight.bChecked = true;
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s flight %s target=%s closest_to_target=%.0f"), *Pit.Label,
	       *Flight.Describe(), *Pit.GroundTarget.ToCompactString(), Flight.GetClosestApproach(Pit.GroundTarget));
	if (Pit.bFiresDirect)
	{
		Check(Flight.GetFlewDirect(), Pit.Label + TEXT("_shot_flies_direct"), Flight.Describe());
	}
}

void FGunPitTestScenario::WatchRunner(const UWorld& World, FGunPitUnderTest& Pit)
{
	FGunPitRunnerWatch& Runner = Pit.Runner;
	if (not Runner.GetIsUsed() || Runner.bChecked || not Pit.GunMesh.IsValid())
	{
		return;
	}
	Runner.Sample(World.GetTimeSeconds(), *Pit.GunMesh);
	if (not Runner.GetIsWatchOver(World.GetTimeSeconds()))
	{
		return;
	}
	Runner.bChecked = true;
	const FString Details = Runner.Describe();
	Check(Runner.GetReachedNiche(), Pit.Label + TEXT("_runner_takes_round_at_niche"), Details);
	Check(Runner.GetReachedDrop(), Pit.Label + TEXT("_runner_lays_round_on_crate"), Details);
	Check(Runner.GetShowedAndHidRound(), Pit.Label + TEXT("_runner_round_shows_only_when_carried"), Details);
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
	const FTransform Frame = GetPitOriginFrame(*Pit.GunMesh, Pit.PitOriginSocket);
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
	const FTransform Frame = GetPitOriginFrame(*Pit.GunMesh, Pit.PitOriginSocket);
	const FVector RoundLocal = Frame.InverseTransformPosition(Round.GetComponentLocation());
	const FVector RightLocal = Frame.InverseTransformPosition(Loader.GetBoneLocation(HandBone));
	const FVector LeftLocal = Frame.InverseTransformPosition(Loader.GetBoneLocation(TEXT("hand_l")));
	const FVector RootLocal = Frame.InverseTransformPosition(Loader.GetComponentLocation());
	const float LoaderYaw = Frame.InverseTransformRotation(Loader.GetComponentQuat()).Rotator().Yaw;
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s handover round=%s loader_hand_r=%s loader_hand_l=%s "
	       "loader_root=%s loader_yaw=%.1f"), *Pit.Label, *RoundLocal.ToCompactString(), *RightLocal.ToCompactString(),
	       *LeftLocal.ToCompactString(), *RootLocal.ToCompactString(), LoaderYaw);
}

void FGunPitTestScenario::WatchBoneDrill(UWorld& World, FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	if (Pit.bDrillChecked || not Pit.Crew.IsValid() || not Pit.GunMesh.IsValid())
	{
		return;
	}
	const float Now = World.GetTimeSeconds();
	const bool bIsLoading = Pit.Crew->GetIsLoading();
	if (bIsLoading && not Pit.bWasLoading)
	{
		Pit.DrillStartSeconds = Now;
		Pit.DrillPropStart = Pit.GunMesh->GetBoneLocation(Pit.DrillPropBone);
		Pit.NextCaptureSeconds = Now;
	}
	Pit.bWasLoading = bIsLoading;
	if (Pit.DrillStartSeconds < 0.f)
	{
		return;
	}
	SampleBoneDrill(Pit);
	if (M_CaptureRig.GetIsEnabled() && bIsLoading && Now >= Pit.NextCaptureSeconds)
	{
		CapturePit(World, Pit, FString::Printf(TEXT("drill_t%04.1f"), Now - Pit.DrillStartSeconds));
		Pit.NextCaptureSeconds = Now + DrillCaptureIntervalSeconds;
	}
	if (not bIsLoading)
	{
		Pit.DrillEndSeconds = Now;
		CheckBoneDrill(Pit);
	}
}

void FGunPitTestScenario::SampleBoneDrill(FGunPitUnderTest& Pit) const
{
	using namespace GunPitTestPrivate;
	const USkeletalMeshComponent* GunMesh = Pit.GunMesh.Get();
	const USkeletalMeshComponent* Loader = Pit.CrewMeshes.IsValidIndex(DrillLoaderCrewIndex)
		                                       ? Pit.CrewMeshes[DrillLoaderCrewIndex].Get()
		                                       : nullptr;
	if (not IsValid(GunMesh) || not IsValid(Loader))
	{
		return;
	}
	const FVector DrillProp = GunMesh->GetBoneLocation(Pit.DrillPropBone);
	const FVector SecondProp = GunMesh->GetBoneLocation(Pit.DrillSecondBone);
	for (const FName Hand : {LeftHandBone, HandBone})
	{
		const FVector HandLocation = Loader->GetBoneLocation(Hand);
		Pit.ClosestHandToDrillProp = FMath::Min(Pit.ClosestHandToDrillProp, FVector::Dist(HandLocation, DrillProp));
		Pit.ClosestHandToSecondProp = FMath::Min(Pit.ClosestHandToSecondProp, FVector::Dist(HandLocation, SecondProp));
	}
	Pit.DrillPropTravel = FMath::Max(Pit.DrillPropTravel, FVector::Dist(DrillProp, Pit.DrillPropStart));
	Pit.bGunReloadMontageSeen |= GetActiveMontageName(GunMesh).Contains(TEXT("Reload"));
}

void FGunPitTestScenario::CheckBoneDrill(FGunPitUnderTest& Pit)
{
	using namespace GunPitTestPrivate;
	Pit.bDrillChecked = true;
	const FString PropName = Pit.DrillPropBone.ToString();
	const FString SecondName = Pit.DrillSecondBone.ToString();
	UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST %s bone drill seconds=%.2f %s_travel=%.1f "
	       "hand_to_%s=%.1f hand_to_%s=%.1f"), *Pit.Label, Pit.DrillEndSeconds - Pit.DrillStartSeconds, *PropName,
	       Pit.DrillPropTravel, *PropName, Pit.ClosestHandToDrillProp, *SecondName, Pit.ClosestHandToSecondProp);
	Check(Pit.bGunReloadMontageSeen, Pit.Label + TEXT("_gun_plays_reload_drill"));
	Check(Pit.DrillPropTravel > MinimumDrillPropTravel, Pit.Label + TEXT("_") + PropName + TEXT("_moved"),
	      FString::Printf(TEXT("travel=%.1f"), Pit.DrillPropTravel));
	Check(Pit.ClosestHandToDrillProp < MaximumHandToDrillPropDistance, Pit.Label + TEXT("_loader_handles_") + PropName,
	      FString::Printf(TEXT("closest=%.1f"), Pit.ClosestHandToDrillProp));
	Check(Pit.ClosestHandToSecondProp < MaximumHandToDrillPropDistance, Pit.Label + TEXT("_loader_handles_") + SecondName,
	      FString::Printf(TEXT("closest=%.1f"), Pit.ClosestHandToSecondProp));
	Check(Pit.bGunnerFireMontageSeen, Pit.Label + TEXT("_gunner_shakes_when_firing"));
}

void FGunPitTestScenario::RevealPit(const FGunPitUnderTest& Pit) const
{
	for (AActor* Actor : {Pit.Pit.Get(), static_cast<AActor*>(Pit.Turret.Get())})
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
}

bool FGunPitTestScenario::GetIsPitDone(const FGunPitUnderTest& Pit) const
{
	const bool bFlightDone = not Pit.bFiresDirect || Pit.Flight.bChecked;
	const bool bRunnerDone = not Pit.Runner.GetIsUsed() || Pit.Runner.bChecked;
	return Pit.ShotSeconds.Num() >= GunPitTestPrivate::ShotsToObserve && Pit.bDrillChecked && Pit.bTraverseChecked
		&& bFlightDone && bRunnerDone;
}

void FGunPitTestScenario::KeepMeshesAnimating() const
{
	for (const FGunPitUnderTest& Pit : M_Pits)
	{
		GunPitTestPrivate::ForceAnimate(Pit.GunMesh.Get());
		GunPitTestPrivate::ForceAnimate(Pit.Runner.GetRunner());
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
	// -GerPitStatusInterval=<s> logs the pits more often (e.g. to follow a traverse).
	float StatusInterval = GunPitTestPrivate::StatusIntervalSeconds;
	FParse::Value(FCommandLine::Get(), TEXT("GerPitStatusInterval="), StatusInterval);
	M_NextStatusSeconds = World.GetTimeSeconds() + StatusInterval;
	for (const FGunPitUnderTest& Pit : M_Pits)
	{
		const ACPPTurretsMaster* Turret = Pit.Turret.Get();
		const UWeaponState* Weapon = IsValid(Turret) ? Turret->GetLargestCalibreWeapon() : nullptr;
		const float OriginYaw = Pit.GunMesh.IsValid() ? GunPitTestPrivate::GetPitOriginFrame(*Pit.GunMesh, Pit.PitOriginSocket).Rotator().Yaw : 0.f;
		const UTurretAnimInstance* GunAnim = Pit.GunMesh.IsValid() ? Cast<UTurretAnimInstance>(Pit.GunMesh->GetAnimInstance()) : nullptr;
		const float AnimYaw = IsValid(GunAnim) ? GunAnim->GetCurrentYawAngle() : -999.f;
		UE_LOG(LogRTSGunPitTest, Display, TEXT("RTS_GERPIT_TEST status %s t=%.1f mag=%d reloading=%d loading=%d "
		       "rotated_to_engage=%d pit_origin_yaw=%.1f anim_yaw=%.1f target_yaw=%.1f shots=%d"), *Pit.Label, World.GetTimeSeconds(),
		       IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : -1, IsValid(Weapon) && Weapon->GetIsReloading(),
		       Pit.Crew.IsValid() && Pit.Crew->GetIsLoading(), IsValid(Turret) && Turret->GetIsRotatedToEngage(), OriginYaw, AnimYaw,
		       (Pit.GroundTarget - Pit.SpawnLocation).GetSafeNormal2D().Rotation().Yaw, Pit.ShotSeconds.Num());
	}
}

void FGunPitTestScenario::CapturePit(UWorld& World, const FGunPitUnderTest& Pit, const FString& Moment)
{
	if (not M_CaptureRig.GetIsEnabled() || not Pit.GunMesh.IsValid())
	{
		return;
	}
	RevealPit(Pit);
	M_CaptureRig.CaptureViews(World, GunPitTestPrivate::GetPitOriginFrame(*Pit.GunMesh, Pit.PitOriginSocket),
	                          GunPitTestPrivate::GetPitViews(Pit.ViewScale), Pit.Label + TEXT("_") + Moment);
}

#endif
