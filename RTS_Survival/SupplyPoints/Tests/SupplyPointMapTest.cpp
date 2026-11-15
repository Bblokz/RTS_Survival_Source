// Copyright (C) Bas Blokzijl - All rights reserved.

// -SupplyPointValidate: puts a supply point and a truck base on the map, lets a player vehicle capture the point,
// follows the first truck through bay, loading and delivery (resources must arrive, clamped to storage), then
// destroys the second truck on its way home (its cargo must be lost). Also checks the truck cannot be selected and
// that the announcer deck never repeats a line before the deck is empty. A third truck is recalled by losing the
// point. With -SupplyPointUseHq the base is a real deployed player HQ (spawn socket, real storage) instead of a
// scripted base spot with cheated storage. -SupplyPointClass=<Blueprint class path> tests a real supply point
// Blueprint instead of the bare C++ class (its crane must then put every load into the truck), and
// -SupplyPointPictures saves a picture from the RTS camera angle every 1.5 s while a truck is in the bay
// (Saved/SupplyPointTest/Pictures).
// Logs RTS_SUPPLYPOINT_TEST lines and quits. Everything lives in this file, behind RTS_WITH_MAP_TESTS.

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Containers/Ticker.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/SupplyPoints/SupplyPointCraneComponent.h"
#include "RTS_Survival/SupplyPoints/SupplyPointSubsystem.h"
#include "RTS_Survival/SupplyPoints/SupplyTruckComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSupplyPointTest, Display, All);

namespace SupplyPointMapTestPrivate
{
	constexpr float StepTimeoutSeconds = 240.f;
	constexpr float PointDistanceFromBaseCm = 3500.f;
	constexpr float CapturerOffsetCm = 500.f;
	constexpr float TraceIntervalSeconds = 5.f;
	constexpr int32 DeckLineCount = 5;
	constexpr int32 DeckDraws = 40;
	constexpr int32 RadixiteStorage = 100;
	constexpr int32 MetalStorage = 500;
	constexpr int32 RadixiteSpent = 70;
	constexpr int32 MetalSpent = 400;

	const TCHAR* const LocationSwitch = TEXT("SupplyPointAt=");
	// Infantry captures (earlier runs of this test captured with a Pz IV).
	const TCHAR* const CapturerClass =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/SteelFistAssaultSquad/Squad_Ger_Faust.Squad_Ger_Faust_C");
	const TCHAR* const UseHqSwitch = TEXT("SupplyPointUseHq");
	const TCHAR* const PointClassSwitch = TEXT("SupplyPointClass=");
	const TCHAR* const PicturesSwitch = TEXT("SupplyPointPictures");
	constexpr float PictureIntervalSeconds = 1.5f;
	constexpr float PictureFieldOfView = 50.f;
	constexpr float HqForceDeploySeconds = 3.f;
	constexpr float ParkYawToleranceDegrees = 25.f;

	const ERTSResourceType CargoTypes[] = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts
	};

	enum class EStep : uint8
	{
		StartingGame,
		WaitingForHq,
		Capturing,
		WaitingForFirstTruck,
		FirstTruckRun,
		WaitingForSecondTruck,
		SecondTruckLoading,
		SecondTruckLost,
		ThirdTruckRecall,
		Finished
	};

	UWorld* FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (IsValid(World) && World->IsGameWorld() && World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}
}

/** @brief Runs the supply run scenario from a core ticker, so nothing in the game has to know about this test. */
class FSupplyPointMapTest
{
public:
	bool Tick()
	{
		using namespace SupplyPointMapTestPrivate;
		UWorld* World = FindGameWorld();
		if (not IsValid(World))
		{
			return true;
		}
		if (M_Step != EStep::StartingGame)
		{
			FGrenadeTestHelpers::KeepGameRunning(*World, M_PlayerController.Get());
			Trace(*World);
			TakePictures(*World);
		}
		if (M_Step != EStep::StartingGame and M_Step != EStep::Finished and GetStepSeconds(*World) > StepTimeoutSeconds)
		{
			Check(false, TEXT("step_timeout"), FString::Printf(TEXT("step=%d"), static_cast<int32>(M_Step)));
			FinishTest(*World);
			return false;
		}
		switch (M_Step)
		{
		case EStep::StartingGame:
			TickStartingGame(*World);
			break;
		case EStep::WaitingForHq:
			TickWaitingForHq(*World);
			break;
		case EStep::Capturing:
			TickCapturing(*World);
			break;
		case EStep::WaitingForFirstTruck:
			TickWaitingForFirstTruck(*World);
			break;
		case EStep::FirstTruckRun:
			TickFirstTruckRun(*World);
			break;
		case EStep::WaitingForSecondTruck:
			TickWaitingForSecondTruck(*World);
			break;
		case EStep::SecondTruckLoading:
			TickSecondTruckLoading(*World);
			break;
		case EStep::SecondTruckLost:
			TickSecondTruckLost(*World);
			break;
		case EStep::ThirdTruckRecall:
			TickThirdTruckRecall(*World);
			break;
		case EStep::Finished:
			return false;
		}
		return true;
	}

private:
	SupplyPointMapTestPrivate::EStep M_Step = SupplyPointMapTestPrivate::EStep::StartingGame;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<ASupplyPoint> M_SupplyPoint;
	TWeakObjectPtr<ANomadicVehicle> M_Hq;
	TWeakObjectPtr<ASceneCapture2D> M_PictureCamera;
	TWeakObjectPtr<UTextureRenderTarget2D> M_PictureTarget;
	float M_NextPictureSeconds = 0.f;
	int32 M_PictureCount = 0;
	bool bM_TakePictures = false;
	FVector M_BaseLocation = FVector::ZeroVector;
	bool bM_UseHq = false;
	bool bM_HqDeployOrdered = false;
	TWeakObjectPtr<AActor> M_Capturer;
	TWeakObjectPtr<USupplyTruckComponent> M_Truck;
	TWeakObjectPtr<AActor> M_TruckActor;
	FTransform M_BaseTransform;
	FSupplyPointSettings M_Settings;
	TMap<ERTSResourceType, int32> M_ResourcesBeforeRun;
	ESupplyTruckState M_LastTruckState = ESupplyTruckState::Idle;
	float M_StepStartSeconds = 0.f;
	float M_NextTraceSeconds = 0.f;
	float M_LastFlagProgress = 0.f;
	bool bM_SawFlagRising = false;
	bool bM_SawTruckLoading = false;
	bool bM_FirstTruckDelivered = false;
	bool bM_FirstTruckRunEnded = false;
	bool bM_SecondTruckRunEnded = false;
	bool bM_SecondTruckDelivered = false;
	int32 M_FailedChecks = 0;

	void Check(const bool bCondition, const TCHAR* Description, const FString& Details = FString())
	{
		M_FailedChecks += bCondition ? 0 : 1;
		UE_LOG(LogRTSSupplyPointTest, Display, TEXT("RTS_SUPPLYPOINT_TEST %s check='%s' %s"),
		       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details);
	}

	void EnterStep(const UWorld& World, const SupplyPointMapTestPrivate::EStep NewStep)
	{
		M_Step = NewStep;
		M_StepStartSeconds = World.GetTimeSeconds();
		UE_LOG(LogRTSSupplyPointTest, Display, TEXT("RTS_SUPPLYPOINT_TEST step=%d t=%.1f"),
		       static_cast<int32>(NewStep), World.GetTimeSeconds());
	}

	float GetStepSeconds(const UWorld& World) const
	{
		return World.GetTimeSeconds() - M_StepStartSeconds;
	}

	void Trace(const UWorld& World)
	{
		if (World.GetTimeSeconds() < M_NextTraceSeconds or not M_SupplyPoint.IsValid())
		{
			return;
		}
		M_NextTraceSeconds = World.GetTimeSeconds() + SupplyPointMapTestPrivate::TraceIntervalSeconds;
		const FString TruckText = M_Truck.IsValid() and M_TruckActor.IsValid()
			                          ? FString::Printf(TEXT("truck_state=%d truck_at=%s"),
			                                            static_cast<int32>(M_Truck->GetSupplyTruckState()),
			                                            *M_TruckActor->GetActorLocation().ToCompactString())
			                          : FString(TEXT("truck=none"));
		UE_LOG(LogRTSSupplyPointTest, Display,
		       TEXT("RTS_SUPPLYPOINT_TEST trace t=%.1f owner=%d point_state=%d flag=%.2f %s"),
		       World.GetTimeSeconds(), M_SupplyPoint->GetOwningPlayer(),
		       static_cast<int32>(M_SupplyPoint->GetSupplyPointState()), M_SupplyPoint->GetCaptureProgress(),
		       *TruckText);
	}

	void TakePictures(UWorld& World)
	{
		using namespace SupplyPointMapTestPrivate;
		if (not bM_TakePictures or not M_SupplyPoint.IsValid() or World.GetTimeSeconds() < M_NextPictureSeconds)
		{
			return;
		}
		// The bay with a truck in it (waiting, loading) and the first seconds of the truck leaving.
		const bool bTruckAtPoint = M_Truck.IsValid()
			and (M_Truck->GetSupplyTruckState() == ESupplyTruckState::WaitingForSupplies
				or M_Truck->GetSupplyTruckState() == ESupplyTruckState::BeingLoaded
				or M_Truck->GetSupplyTruckState() == ESupplyTruckState::DrivingToExit
				or M_Truck->GetSupplyTruckState() == ESupplyTruckState::DrivingToPark);
		if (not bTruckAtPoint and M_PictureCount > 0)
		{
			return;
		}
		if (not M_PictureCamera.IsValid())
		{
			ASceneCapture2D* Capture = nullptr;
			UTextureRenderTarget2D* Target = nullptr;
			if (not FGrenadeTestHelpers::CreateCaptureCamera(World, PictureFieldOfView, Capture, Target))
			{
				bM_TakePictures = false;
				return;
			}
			M_PictureCamera = Capture;
			M_PictureTarget = Target;
			FGrenadeTestHelpers::AimLikeGameCamera(*Capture, M_SupplyPoint->GetActorLocation());
			// The first frames of a capture are dark until exposure settles.
			M_NextPictureSeconds = World.GetTimeSeconds() + PictureIntervalSeconds;
			return;
		}
		M_NextPictureSeconds = World.GetTimeSeconds() + PictureIntervalSeconds;
		const FString FileName = FString::Printf(TEXT("supply_%03d_point%d_truck%d.png"), M_PictureCount++,
		                                         static_cast<int32>(M_SupplyPoint->GetSupplyPointState()),
		                                         M_Truck.IsValid()
			                                         ? static_cast<int32>(M_Truck->GetSupplyTruckState())
			                                         : -1);
		FGrenadeTestHelpers::SaveCapture(World, *M_PictureTarget, FPaths::ProjectSavedDir() / TEXT("SupplyPointTest/Pictures"),
		                                 FileName);
	}

	UClass* GetSupplyPointClass()
	{
		FString ClassPath;
		if (not FParse::Value(FCommandLine::Get(), SupplyPointMapTestPrivate::PointClassSwitch, ClassPath))
		{
			return ASupplyPoint::StaticClass();
		}
		UClass* PointClass = LoadClass<ASupplyPoint>(nullptr, *ClassPath);
		Check(PointClass != nullptr, TEXT("supply_point_class_loaded"), ClassPath);
		return PointClass != nullptr ? PointClass : ASupplyPoint::StaticClass();
	}

	void TickStartingGame(UWorld& World)
	{
		if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
		{
			return;
		}
		FVector BaseLocation = FVector::ZeroVector;
		if (not FGrenadeTestHelpers::FindTestSpot(World, SupplyPointMapTestPrivate::LocationSwitch, BaseLocation))
		{
			Check(false, TEXT("test_spot"));
			FinishTest(World);
			return;
		}
		CheckAnnouncementDeck();
		M_BaseLocation = BaseLocation;
		bM_UseHq = FParse::Param(FCommandLine::Get(), SupplyPointMapTestPrivate::UseHqSwitch);
		bM_TakePictures = FParse::Param(FCommandLine::Get(), SupplyPointMapTestPrivate::PicturesSwitch);
		if (bM_UseHq)
		{
			RequestHq(World);
			EnterStep(World, SupplyPointMapTestPrivate::EStep::WaitingForHq);
			return;
		}
		SetupBaseAndPoint(World, BaseLocation);
		EnterStep(World, SupplyPointMapTestPrivate::EStep::Capturing);
	}

	void RequestHq(UWorld& World)
	{
		ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(&World);
		bool bRequested = false;
		if (IsValid(Spawner))
		{
			bRequested = Spawner->AsyncSpawnOptionAtLocation(
				FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq)),
				M_BaseLocation, M_PlayerController.Get(), 0,
				[this](const FTrainingOption&, AActor* SpawnedActor, const int32)
				{
					M_Hq = Cast<ANomadicVehicle>(SpawnedActor);
				}, FRotator::ZeroRotator);
		}
		Check(bRequested, TEXT("hq_spawn_requested"));
	}

	void TickWaitingForHq(UWorld& World)
	{
		ANomadicVehicle* Hq = M_Hq.Get();
		if (not IsValid(Hq) or not M_PlayerController.IsValid())
		{
			return;
		}
		if (M_PlayerController->GetPlayerHq() != Hq)
		{
			M_PlayerController->RegisterPlayerHQ(Hq);
		}
		if (Hq->GetNomadicStatus() == ENomadStatus::Building)
		{
			Check(true, TEXT("hq_deployed"), Hq->GetActorLocation().ToCompactString());
			SetupBaseAndPoint(World, Hq->GetActorLocation());
			EnterStep(World, SupplyPointMapTestPrivate::EStep::Capturing);
			return;
		}
		if (not bM_HqDeployOrdered and Hq->GetNomadicStatus() == ENomadStatus::Truck)
		{
			const ECommandQueueError Error = Hq->CreateBuildingAtLocation(Hq->GetActorLocation(),
			                                                              Hq->GetActorRotation(), true);
			bM_HqDeployOrdered = Error == ECommandQueueError::NoError;
			M_StepStartSeconds = World.GetTimeSeconds();
			return;
		}
		if (bM_HqDeployOrdered and GetStepSeconds(World) > SupplyPointMapTestPrivate::HqForceDeploySeconds)
		{
			// Skips the deploy animation, which waits on rendering in an unattended run.
			Hq->ShippingTest_ForceFinishConversionToBuilding();
		}
	}

	void CheckAnnouncementDeck()
	{
		using namespace SupplyPointMapTestPrivate;
		FSupplyAnnouncementDeck Deck;
		bool bNoImmediateRepeat = true;
		bool bEveryLineOncePerDeck = true;
		int32 PreviousLine = INDEX_NONE;
		TSet<int32> LinesThisDeck;
		for (int32 Draw = 0; Draw < DeckDraws; ++Draw)
		{
			const int32 Line = Deck.DrawNextLineIndex(DeckLineCount);
			bNoImmediateRepeat &= Line != PreviousLine and Line >= 0 and Line < DeckLineCount;
			bEveryLineOncePerDeck &= not LinesThisDeck.Contains(Line);
			LinesThisDeck.Add(Line);
			if (LinesThisDeck.Num() == DeckLineCount)
			{
				LinesThisDeck.Reset();
			}
			PreviousLine = Line;
		}
		Check(bNoImmediateRepeat, TEXT("announcer_no_immediate_repeat"));
		Check(bEveryLineOncePerDeck, TEXT("announcer_every_line_before_repeat"));
		Check(Deck.DrawNextLineIndex(0) == INDEX_NONE, TEXT("announcer_empty_deck_silent"));
	}

	void SetupBaseAndPoint(UWorld& World, const FVector& BaseLocation)
	{
		using namespace SupplyPointMapTestPrivate;
		USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(&World);
		Check(SupplySubsystem != nullptr, TEXT("subsystem_exists"));
		if (SupplySubsystem == nullptr)
		{
			return;
		}
		FVector NavBase = BaseLocation;
		FGrenadeTestHelpers::ProjectOntoNavMesh(World, NavBase, FVector(1000.f));
		M_BaseTransform = FTransform(FRotator::ZeroRotator, NavBase);
		UPlayerResourceManager* Resources = FRTS_Statics::GetPlayerResourceManager(&World);
		if (bM_UseHq)
		{
			// The real path: trucks start and deliver at the HQ's spawn socket, storage is the HQ's own.
			Check(SupplySubsystem->GetSupplyBaseTransform(SupplyPointConstants::PlayerOwner, M_BaseTransform),
			      TEXT("hq_is_supply_base"), M_BaseTransform.GetLocation().ToCompactString());
			for (const ERTSResourceType Type : CargoTypes)
			{
				// Empty the stores so a delivery shows.
				Resources->SubtractResource(Type, Resources->GetResourceAmount(Type));
			}
			Check(Resources->GetResourceStorage(ERTSResourceType::Resource_Radixite) > 0, TEXT("hq_gives_storage"),
			      FString::FromInt(Resources->GetResourceStorage(ERTSResourceType::Resource_Radixite)));
		}
		else
		{
			SupplySubsystem->SetSupplyBaseOverride(SupplyPointConstants::PlayerOwner, M_BaseTransform);
		}

		FVector PointLocation = NavBase + FVector(PointDistanceFromBaseCm, 0.f, 0.f);
		FGrenadeTestHelpers::DropToGround(World, PointLocation);
		// The test map has no drop-offs, so no storage: give radixite less room than one load (the rest must be
		// lost) and metal plenty.
		if (not bM_UseHq and IsValid(Resources))
		{
			Resources->CheatAddStorage(ERTSResourceType::Resource_Radixite, RadixiteStorage);
			Resources->CheatAddStorage(ERTSResourceType::Resource_Metal, MetalStorage);
			// The cheat fills what it adds; make room again so a delivery shows.
			Resources->SubtractResource(ERTSResourceType::Resource_Radixite, RadixiteSpent);
			Resources->SubtractResource(ERTSResourceType::Resource_Metal, MetalSpent);
		}
		if (SupplyPointForDefaults(World) != nullptr)
		{
			// A Blueprint brings its own flag height, bar offset and so on: only the timings and the cargo are test values.
			M_Settings = SupplyPointForDefaults(World)->GetSupplyPointSettings();
		}
		M_Settings.CaptureDuration = 4.f;
		M_Settings.SuppliesReadyTime = 20.f;
		M_Settings.TruckLoadingTime = 5.f;
		// -SupplyPointLoadingTime=12 to watch the crane at the real loading time.
		FParse::Value(FCommandLine::Get(), TEXT("SupplyPointLoadingTime="), M_Settings.TruckLoadingTime);
		M_Settings.TimeBetweenTrucks = 2.f;
		M_Settings.SuppliesPerTrip.Radixite = 120;
		M_Settings.SuppliesPerTrip.Metal = 80;
		// A kind set to zero must not be loaded or paid out.
		M_Settings.SuppliesPerTrip.VehicleParts = 0;
		ASupplyPoint* SupplyPoint = World.SpawnActorDeferred<ASupplyPoint>(
			GetSupplyPointClass(), FTransform(PointLocation), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (IsValid(SupplyPoint))
		{
			SupplyPoint->SetSupplyPointSettings(M_Settings);
			SupplyPoint->FinishSpawning(FTransform(PointLocation));
		}
		M_SupplyPoint = SupplyPoint;
		Check(M_SupplyPoint.IsValid(), TEXT("supply_point_spawned"), PointLocation.ToCompactString());
		Check(M_SupplyPoint.IsValid() and M_SupplyPoint->GetOwningPlayer() == SupplyPointConstants::EnemyOwner,
		      TEXT("point_starts_enemy_held"));

		const FTransform CapturerTransform(PointLocation + FVector(0.f, -CapturerOffsetCm, 0.f));
		M_Capturer = FGrenadeTestHelpers::SpawnUnitForPlayer(World, CapturerClass, CapturerTransform,
		                                                     SupplyPointConstants::PlayerOwner);
		Check(M_Capturer.IsValid(), TEXT("capturer_spawned"));
	}

	const ASupplyPoint* SupplyPointForDefaults(UWorld& World)
	{
		const UClass* PointClass = GetSupplyPointClass();
		return PointClass != nullptr ? Cast<ASupplyPoint>(PointClass->GetDefaultObject()) : nullptr;
	}

	void CheckCraneLoadedTruck()
	{
		const USupplyPointCraneComponent* Crane = M_SupplyPoint.IsValid()
			                                          ? M_SupplyPoint->FindComponentByClass<USupplyPointCraneComponent>()
			                                          : nullptr;
		if (Crane == nullptr)
		{
			// The bare C++ supply point has no crane.
			return;
		}
		int32 ExpectedLoads = 0;
		for (const ERTSResourceType Type : SupplyPointMapTestPrivate::CargoTypes)
		{
			ExpectedLoads += M_Settings.SuppliesPerTrip.GetAmount(Type) > 0 ? 1 : 0;
		}
		Check(Crane->GetLoadsPlacedInTruck() == ExpectedLoads, TEXT("crane_put_every_load_in_truck"),
		      FString::Printf(TEXT("placed=%d expected=%d"), Crane->GetLoadsPlacedInTruck(), ExpectedLoads));
		TInlineComponentArray<UStaticMeshComponent*> TruckMeshes(M_TruckActor.Get());
		int32 LoadsOnTruck = 0;
		for (const UStaticMeshComponent* TruckMesh : TruckMeshes)
		{
			const USceneComponent* Parent = TruckMesh->GetAttachParent();
			LoadsOnTruck += Parent != nullptr and Parent->GetName().StartsWith(TEXT("CargoSlot_")) ? 1 : 0;
		}
		Check(LoadsOnTruck == ExpectedLoads, TEXT("loads_ride_in_truck_bed"),
		      FString::Printf(TEXT("on_truck=%d expected=%d"), LoadsOnTruck, ExpectedLoads));
	}

	void TickCapturing(UWorld& World)
	{
		if (not M_SupplyPoint.IsValid())
		{
			FinishTest(World);
			return;
		}
		const float FlagProgress = M_SupplyPoint->GetCaptureProgress();
		bM_SawFlagRising |= FlagProgress > 0.05f and FlagProgress < 0.95f and FlagProgress > M_LastFlagProgress;
		M_LastFlagProgress = FlagProgress;
		if (M_SupplyPoint->GetOwningPlayer() != SupplyPointConstants::PlayerOwner)
		{
			return;
		}
		Check(bM_SawFlagRising, TEXT("flag_rose_gradually"));
		// The enemy flag comes down first, then the player's goes up: twice the capture duration.
		Check(GetStepSeconds(World) >= M_Settings.CaptureDuration * 1.8f, TEXT("capture_took_lower_plus_raise"),
		      FString::Printf(TEXT("seconds=%.1f"), GetStepSeconds(World)));
		Check(M_SupplyPoint->GetSupplyPointState() == ESupplyPointState::PreparingSupplies,
		      TEXT("supplies_preparing_after_capture"));
		RememberResources();
		EnterStep(World, SupplyPointMapTestPrivate::EStep::WaitingForFirstTruck);
	}

	bool FindTruckOfPoint()
	{
		const USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(M_SupplyPoint.Get());
		USupplyTruckComponent* Truck = SupplySubsystem != nullptr
			                               ? SupplySubsystem->GetSupplyTruckOfPoint(M_SupplyPoint.Get())
			                               : nullptr;
		if (not IsValid(Truck) or not IsValid(Truck->GetOwner()))
		{
			return false;
		}
		M_Truck = Truck;
		M_TruckActor = Truck->GetOwner();
		M_LastTruckState = ESupplyTruckState::Idle;
		return true;
	}

	void TickWaitingForFirstTruck(UWorld& World)
	{
		if (not FindTruckOfPoint())
		{
			return;
		}
		ATankMaster* TruckTank = M_Truck->GetTruck();
		const USelectionComponent* Selection = IsValid(TruckTank) ? TruckTank->GetSelectionComponent() : nullptr;
		Check(IsValid(Selection) and not Selection->GetCanBeSelected(), TEXT("truck_not_marquee_selectable"));
		Check(M_PlayerController.IsValid() and not M_PlayerController->GetIsClickedUnitSelectable(TruckTank),
		      TEXT("truck_not_click_selectable"));
		Check(M_Truck->GetTruckOwningPlayer() == SupplyPointConstants::PlayerOwner, TEXT("truck_owned_by_player"));
		Check(FVector::Dist2D(M_TruckActor->GetActorLocation(), M_BaseTransform.GetLocation()) < 600.f,
		      TEXT("truck_spawned_at_base"));
		Check(M_Truck->GetCargo().GetIsEmpty(), TEXT("truck_leaves_base_empty"));
		M_Truck->OnSupplyTruckRunEnded.AddLambda([this](USupplyTruckComponent*, const bool bDelivered)
		{
			bM_FirstTruckRunEnded = true;
			bM_FirstTruckDelivered = bDelivered;
		});
		EnterStep(World, SupplyPointMapTestPrivate::EStep::FirstTruckRun);
	}

	void RememberResources()
	{
		const UPlayerResourceManager* Resources = FRTS_Statics::GetPlayerResourceManager(M_SupplyPoint.Get());
		for (const ERTSResourceType Type : SupplyPointMapTestPrivate::CargoTypes)
		{
			M_ResourcesBeforeRun.Add(Type, IsValid(Resources) ? Resources->GetResourceAmount(Type) : 0);
		}
	}

	void CheckResourcesAfterRun(const bool bExpectCargo, const TCHAR* Description)
	{
		const UPlayerResourceManager* Resources = FRTS_Statics::GetPlayerResourceManager(M_SupplyPoint.Get());
		if (not IsValid(Resources))
		{
			Check(false, Description, TEXT("no resource manager"));
			return;
		}
		for (const ERTSResourceType Type : SupplyPointMapTestPrivate::CargoTypes)
		{
			const int32 Before = M_ResourcesBeforeRun.FindRef(Type);
			const int32 Cargo = bExpectCargo ? M_Settings.SuppliesPerTrip.GetAmount(Type) : 0;
			// Whatever does not fit the storage is lost.
			const int32 Expected = FMath::Min(Before + Cargo, FMath::Max(Before, Resources->GetResourceStorage(Type)));
			const int32 Now = Resources->GetResourceAmount(Type);
			Check(Now == Expected, Description, FString::Printf(
				      TEXT("type=%d before=%d cargo=%d storage=%d now=%d expected=%d"), static_cast<int32>(Type),
				      Before, Cargo, Resources->GetResourceStorage(Type), Now, Expected));
		}
	}

	void WatchTruckState()
	{
		if (not M_Truck.IsValid())
		{
			return;
		}
		const ESupplyTruckState State = M_Truck->GetSupplyTruckState();
		if (State == M_LastTruckState)
		{
			return;
		}
		UE_LOG(LogRTSSupplyPointTest, Display, TEXT("RTS_SUPPLYPOINT_TEST truck_state %d -> %d"),
		       static_cast<int32>(M_LastTruckState), static_cast<int32>(State));
		if (State == ESupplyTruckState::BeingLoaded and M_SupplyPoint.IsValid() and M_TruckActor.IsValid())
		{
			bM_SawTruckLoading = true;
			const FTransform Park = M_SupplyPoint->GetTruckParkTransform();
			const float ParkError = FVector::Dist2D(M_TruckActor->GetActorLocation(), Park.GetLocation());
			Check(ParkError < 300.f, TEXT("truck_parked_in_bay"), FString::Printf(TEXT("error_cm=%.0f"), ParkError));
			Check(M_SupplyPoint->GetSupplyPointState() == ESupplyPointState::LoadingTruck,
			      TEXT("point_loading_while_truck_loaded"));
			const ATankMaster* TruckTank = M_Truck->GetTruck();
			const USelectionComponent* Selection = IsValid(TruckTank) ? TruckTank->GetSelectionComponent() : nullptr;
			Check(IsValid(Selection) and not Selection->GetCanBeSelected(), TEXT("truck_still_not_selectable_in_bay"));
		}
		if (State == ESupplyTruckState::DrivingToExit and bM_SawTruckLoading)
		{
			Check(M_Truck->GetCargo().Radixite == M_Settings.SuppliesPerTrip.Radixite
			      and M_Truck->GetCargo().Metal == M_Settings.SuppliesPerTrip.Metal
			      and M_Truck->GetCargo().VehicleParts == 0, TEXT("truck_carries_supplies_per_trip"));
			const float YawError = FMath::Abs(FRotator::NormalizeAxis(
				M_TruckActor->GetActorRotation().Yaw - M_SupplyPoint->GetTruckParkTransform().Rotator().Yaw));
			Check(YawError < SupplyPointMapTestPrivate::ParkYawToleranceDegrees, TEXT("truck_parked_along_bay"),
			      FString::Printf(TEXT("yaw_error=%.0f"), YawError));
			CheckCraneLoadedTruck();
			// Nothing reaches the player before the truck is home.
			CheckResourcesAfterRun(false, TEXT("no_resources_before_delivery"));
		}
		M_LastTruckState = State;
	}

	void TickFirstTruckRun(UWorld& World)
	{
		WatchTruckState();
		if (not bM_FirstTruckRunEnded)
		{
			return;
		}
		Check(bM_FirstTruckDelivered, TEXT("first_truck_delivered"));
		Check(bM_SawTruckLoading, TEXT("first_truck_was_loaded"));
		CheckResourcesAfterRun(true, TEXT("resources_added_on_delivery"));
		RememberResources();
		bM_SawTruckLoading = false;
		M_Truck.Reset();
		EnterStep(World, SupplyPointMapTestPrivate::EStep::WaitingForSecondTruck);
	}

	void TickWaitingForSecondTruck(UWorld& World)
	{
		if (M_TruckActor.IsValid())
		{
			// The delivered truck is being removed.
			return;
		}
		if (not FindTruckOfPoint())
		{
			return;
		}
		Check(true, TEXT("second_truck_dispatched_after_delivery"),
		      FString::Printf(TEXT("seconds=%.1f"), GetStepSeconds(World)));
		M_Truck->OnSupplyTruckRunEnded.AddLambda([this](USupplyTruckComponent*, const bool bDelivered)
		{
			bM_SecondTruckRunEnded = true;
			bM_SecondTruckDelivered = bDelivered;
		});
		EnterStep(World, SupplyPointMapTestPrivate::EStep::SecondTruckLoading);
	}

	void TickSecondTruckLoading(UWorld& World)
	{
		WatchTruckState();
		if (not M_Truck.IsValid() or M_Truck->GetSupplyTruckState() != ESupplyTruckState::DrivingToBase)
		{
			return;
		}
		// Loaded and on its way home: destroy it.
		if (ATankMaster* TruckTank = M_Truck->GetTruck())
		{
			TruckTank->TakeFatalDamage();
		}
		EnterStep(World, SupplyPointMapTestPrivate::EStep::SecondTruckLost);
	}

	void TickSecondTruckLost(UWorld& World)
	{
		constexpr float SettleSeconds = 3.f;
		if (GetStepSeconds(World) < SettleSeconds)
		{
			return;
		}
		Check(bM_SecondTruckRunEnded and not bM_SecondTruckDelivered, TEXT("destroyed_truck_run_ended_undelivered"));
		CheckResourcesAfterRun(false, TEXT("destroyed_truck_cargo_lost"));
		M_Truck.Reset();
		EnterStep(World, SupplyPointMapTestPrivate::EStep::ThirdTruckRecall);
	}

	void TickThirdTruckRecall(UWorld& World)
	{
		if (not M_SupplyPoint.IsValid() or not FindTruckOfPoint())
		{
			return;
		}
		Check(true, TEXT("third_truck_dispatched_after_loss"), FString::Printf(TEXT("seconds=%.1f"), GetStepSeconds(World)));
		// The enemy takes the point back while the truck is on its way: the truck must turn for home.
		M_SupplyPoint->ForceCaptureByPlayer(SupplyPointConstants::EnemyOwner);
		Check(M_Truck->GetSupplyTruckState() == ESupplyTruckState::DrivingToBase, TEXT("truck_recalled_when_point_lost"),
		      FString::Printf(TEXT("state=%d"), static_cast<int32>(M_Truck->GetSupplyTruckState())));
		const USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(&World);
		Check(SupplySubsystem != nullptr and SupplySubsystem->GetSupplyTruckOfPoint(M_SupplyPoint.Get()) == nullptr,
		      TEXT("lost_point_has_no_truck"));
		FinishTest(World);
	}

	void FinishTest(UWorld& World)
	{
		UE_LOG(LogRTSSupplyPointTest, Display, TEXT("RTS_SUPPLYPOINT_TEST RESULT %s failed_checks=%d"),
		       M_FailedChecks == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedChecks);
		EnterStep(World, SupplyPointMapTestPrivate::EStep::Finished);
		FPlatformMisc::RequestExit(false);
	}
};

static FDelayedAutoRegisterHelper GRegisterSupplyPointMapTest(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (not FParse::Param(FCommandLine::Get(), TEXT("SupplyPointValidate")))
	{
		return;
	}
	const TSharedRef<FSupplyPointMapTest> Test = MakeShared<FSupplyPointMapTest>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float)
	{
		return Test->Tick();
	}));
});

#endif
