// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldDefenseTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Game/GameState/BxpDataHelpers/BxpConstructionRulesHelpers.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/ConstructionPreview/CPPConstructionPreview.h"
#include "RTS_Survival/Player/ConstructionPreview/StaticMeshPreview/StaticPreviewMesh.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/W_TrainingItem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Slate/SceneViewport.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFieldDefenseTest, Log, All);

namespace FieldDefenseTestPrivate
{
	constexpr float StartDelaySeconds = 3.f;
	constexpr float ExpandTimeoutSeconds = 400.f;
	constexpr float ConversionFastForwardSeconds = 30.f;
	constexpr float UnitTrainingTimeoutSeconds = 400.f;
	constexpr float ConstructionTimeoutSeconds = 200.f;
	constexpr float DeathCheckDelaySeconds = 3.f;
	// How long the pit builds before its bar is compared with the queue item and the queue is paused.
	constexpr float BuildBeforePauseSeconds = 4.f;
	constexpr float PauseSeconds = 3.f;
	// The queue ticks once a second; the pit's bar is synced on every tick.
	constexpr float BarToQueueTolerance = 1.25f;
	constexpr float PausedProgressTolerance = 0.05f;
	constexpr float SpotMatchDistance = 60.f;
	constexpr float InclineTolerance = 0.5f;
	constexpr float SteepSlabDegrees = 10.f;
	constexpr float GentleSlabDegrees = 2.f;
	constexpr float SlabSize = 3000.f;
	constexpr float SlabLift = 300.f;
	constexpr float SpotSearchMinRadius = 1600.f;
	constexpr float SpotSearchMaxRadius = 4200.f;
	constexpr float SpotSearchRadiusStep = 400.f;
	constexpr float SpotSearchAngleStep = 20.f;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr int32 TestResourceAmount = 1000000;
	// Pits planned ahead after the first: the most allowed at once.
	constexpr int32 PitsToPlanAhead = DeveloperSettings::GamePlay::Training::MaxPlannedFieldDefenses;

	// Building time the cancelled pit gets before it is cancelled, so it has paid for a few seconds.
	constexpr float BuildBeforeCancelSeconds = 4.f;
	constexpr float TierTwoBuildingDistance = 3500.f;
	// The HQ layout's second tab.
	constexpr int32 FieldDefenseTabIndex = 1;
	// The preview snaps to the building grid, so it may stand up to a tile from the ground under the cursor.
	constexpr float CursorToPreviewTolerance = 250.f;
	// How long the viewport click may take to reach the controller through the input stack.
	constexpr float ClickSettleSeconds = 1.f;
	// How long the preview may take to follow the cursor while the camera still glides to the spot.
	constexpr float CursorSettleSeconds = 2.f;
	// Slate moves the viewport cursor back to the machine's cursor after input unless this is off.
	const TCHAR* const SyntheticCursorMovesCVar = TEXT("Slate.EnableSyntheticCursorMoves");
	// The mouse phase's step that checks the selection and right clicks the item, after the pit is planned.
	constexpr int32 MousePhaseRightClickStep = 5;
	constexpr int32 TierTwoSpawnId = 1;
	const FTrainingOption CommunicationCenterOption(EAllUnitType::UNType_Nomadic,
	                                               static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter));
	const FTrainingOption LeFHPitOption(EAllUnitType::UNType_BuildingExpansion,
	                                    static_cast<uint8>(EBuildingExpansionType::BTX_GerLeFH18Pit));
	const FTrainingOption SFHPitOption(EAllUnitType::UNType_BuildingExpansion,
	                                   static_cast<uint8>(EBuildingExpansionType::BTX_GerSFH18Pit));
	const FTrainingOption HarvesterOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester));
	const FTrainingOption Pak40PitOption(EAllUnitType::UNType_BuildingExpansion,
	                                     static_cast<uint8>(EBuildingExpansionType::BTX_GerPak40Pit));
	const TArray<EBuildingExpansionType> GermanPits = {
		EBuildingExpansionType::BTX_GerPak40Pit,
		EBuildingExpansionType::BTX_GerFlak37Pit,
		EBuildingExpansionType::BTX_GerLeFH18Pit,
		EBuildingExpansionType::BTX_GerSFH18Pit
	};
}

bool UFieldDefenseTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FieldDefenseValidate"));
}

TStatId UFieldDefenseTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldDefenseTestSubsystem, STATGROUP_Tickables);
}

void UFieldDefenseTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (bM_IsExitPending)
	{
		TickExitPending();
		return;
	}
	if (M_Phase == EFieldDefenseTestPhase::Inactive || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	TakeDueCapture();
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		FParse::Value(FCommandLine::Get(), TEXT("FieldDefenseCapture="), M_CaptureDirectory);
		UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST started map=%s"), *World->GetName());
	}
	if (M_PhaseTimeoutSeconds > 0.f && GetPhaseSeconds() > M_PhaseTimeoutSeconds)
	{
		Check(false, TEXT("phase_timeout"), FString::Printf(TEXT("phase=%d step=%d"),
		                                                     static_cast<int32>(M_Phase), M_PhaseStep));
		Finish();
		return;
	}
	KeepHQAnimating();
	HoldCameraStill();
	switch (M_Phase)
	{
	case EFieldDefenseTestPhase::Inactive:
		return;
	case EFieldDefenseTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EFieldDefenseTestPhase::ExpandingHQ:
		TickExpandingHQ();
		return;
	case EFieldDefenseTestPhase::PlanningFirstPit:
		TickPlanningFirstPit();
		return;
	case EFieldDefenseTestPhase::WaitingBehindUnit:
		TickWaitingBehindUnit();
		return;
	case EFieldDefenseTestPhase::Constructing:
		TickConstructing();
		return;
	case EFieldDefenseTestPhase::WaitingForFinish:
		TickWaitingForFinish();
		return;
	case EFieldDefenseTestPhase::CancellingPlanned:
		TickCancellingPlanned();
		return;
	case EFieldDefenseTestPhase::CancellingConstructing:
		TickCancellingConstructing();
		return;
	case EFieldDefenseTestPhase::LosingConstruction:
		TickLosingConstruction();
		return;
	case EFieldDefenseTestPhase::CheckingTierTwo:
		TickCheckingTierTwo();
		return;
	case EFieldDefenseTestPhase::PlacingWithMouse:
		TickPlacingWithMouse();
		return;
	case EFieldDefenseTestPhase::PlanningAhead:
		TickPlanningAhead();
		return;
	case EFieldDefenseTestPhase::WaitingForNextConstruction:
		TickWaitingForNextConstruction();
		return;
	case EFieldDefenseTestPhase::CheckingOwnerDeath:
		TickCheckingOwnerDeath();
		return;
	}
}

void UFieldDefenseTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UFieldDefenseTestSubsystem::EnterPhase(const EFieldDefenseTestPhase NewPhase, const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseStep = 0;
	M_PhaseTimeoutSeconds = TimeoutSeconds;
	M_PhaseStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UFieldDefenseTestSubsystem::GetPhaseSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds : 0.f;
}

void UFieldDefenseTestSubsystem::Finish()
{
	M_Phase = EFieldDefenseTestPhase::Inactive;
	UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	// A screenshot is written a frame after it is requested.
	constexpr int32 FramesToWriteLastCapture = 3;
	bM_IsExitPending = true;
	M_ExitFramesLeft = FramesToWriteLastCapture;
}

void UFieldDefenseTestSubsystem::TickExitPending()
{
	if (not M_PendingCaptures.IsEmpty())
	{
		TakeDueCapture();
		return;
	}
	if (--M_ExitFramesLeft > 0)
	{
		return;
	}
	bM_IsExitPending = false;
	FPlatformMisc::RequestExit(false, TEXT("FieldDefenseValidate"));
}

void UFieldDefenseTestSubsystem::ScheduleCapture(const FString& MomentName, const FVector& FocusLocation,
                                                 const float DelaySeconds, const bool bFocusCamera)
{
	if (M_CaptureDirectory.IsEmpty() || not IsValid(GetWorld()))
	{
		return;
	}
	ACPPController* Controller = GetController();
	if (bFocusCamera && IsValid(Controller))
	{
		Controller->FocusCameraOnLocation(FocusLocation);
	}
	M_PendingCaptures.Add(TPair<float, FString>(GetWorld()->GetTimeSeconds() + DelaySeconds, MomentName));
}

void UFieldDefenseTestSubsystem::HoldCameraStill() const
{
	if (M_Phase == EFieldDefenseTestPhase::WaitingForStart)
	{
		return;
	}
	// Edge scrolling reads the machine's cursor even offscreen, so it would move the camera off the spots the
	// scenario aims it at. Held every tick because starting the game turns camera movement back on.
	const ACPPController* Controller = GetController();
	if (IsValid(Controller))
	{
		Controller->SetCameraMovementDisabled(true);
	}
}

void UFieldDefenseTestSubsystem::TakeDueCapture()
{
	if (M_PendingCaptures.IsEmpty() || not IsValid(GetWorld()))
	{
		return;
	}
	// One request per frame: the engine keeps only the last screenshot request of a frame.
	const int32 DueIndex = M_PendingCaptures.IndexOfByPredicate([Now = GetWorld()->GetTimeSeconds()](
		const TPair<float, FString>& PendingCapture)
		{
			return Now >= PendingCapture.Key;
		});
	if (DueIndex == INDEX_NONE)
	{
		return;
	}
	const FString CapturePath = FPaths::Combine(M_CaptureDirectory, M_PendingCaptures[DueIndex].Value + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
	UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST capture %s"), *CapturePath);
	M_PendingCaptures.RemoveAt(DueIndex);
}

void UFieldDefenseTestSubsystem::TickWaitingForStart()
{
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	ACPPController* Controller = GetController();
	UPlayerStartGameControl* StartGameControl = IsValid(Controller)
		                                            ? Controller->FindComponentByClass<UPlayerStartGameControl>()
		                                            : nullptr;
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (GetPhaseSeconds() < FieldDefenseTestPrivate::StartDelaySeconds)
	{
		return;
	}
	GivePlayerResources();
	if (not FindHQ())
	{
		Check(false, TEXT("hq_found"));
		Finish();
		return;
	}
	ANomadicVehicle* HQ = M_HQ.Get();
	UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST hq=%s status=%d location=%s"),
	       *HQ->GetName(), static_cast<int32>(HQ->GetNomadicStatus()), *HQ->GetActorLocation().ToString());
	if (HQ->GetIsInTruckMode())
	{
		const ECommandQueueError Error = HQ->CreateBuildingAtLocation(
			HQ->GetActorLocation(), HQ->GetActorRotation(), true);
		Check(Error == ECommandQueueError::NoError, TEXT("hq_expand_ordered"));
	}
	EnterPhase(EFieldDefenseTestPhase::ExpandingHQ, FieldDefenseTestPrivate::ExpandTimeoutSeconds);
}

void UFieldDefenseTestSubsystem::TickExpandingHQ()
{
	ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(HQ))
	{
		Check(false, TEXT("hq_alive_while_expanding"));
		Finish();
		return;
	}
	UTrainerComponent* Trainer = HQ->GetTrainerComponent();
	if (not IsValid(Trainer) || not Trainer->GetIsAbleToTrain())
	{
		LogHQStatus(*HQ, Trainer);
		// Headless runs never draw the truck, so its expansion montage may never finish on its own.
		if (GetPhaseSeconds() >= FieldDefenseTestPrivate::ConversionFastForwardSeconds)
		{
			(void)HQ->ShippingTest_ForceFinishConversionToBuilding();
		}
		return;
	}
	M_Trainer = Trainer;
	UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST hq_expanded after=%.1fs"),
	       GetPhaseSeconds());
	CheckLayoutAndRequirements();
	EnterPhase(EFieldDefenseTestPhase::PlanningFirstPit, FieldDefenseTestPrivate::UnitTrainingTimeoutSeconds);
}

void UFieldDefenseTestSubsystem::LogHQStatus(const ANomadicVehicle& HQ, const UTrainerComponent* Trainer)
{
	constexpr float StatusIntervalSeconds = 10.f;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < M_NextStatusSeconds)
	{
		return;
	}
	M_NextStatusSeconds = Now + StatusIntervalSeconds;
	UE_LOG(LogRTSFieldDefenseTest, Display,
	       TEXT("RTS_FIELD_DEFENSE_TEST waiting_for_hq status=%d trainer=%d able_to_train=%d after=%.0fs"),
	       static_cast<int32>(HQ.GetNomadicStatus()), IsValid(Trainer) ? 1 : 0,
	       IsValid(Trainer) && Trainer->GetIsAbleToTrain() ? 1 : 0, GetPhaseSeconds());
}

void UFieldDefenseTestSubsystem::CheckLayoutAndRequirements()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	const FTrainingSubSectionState* FieldDefenseSection = Trainer->GetTrainingSubSections().FindByPredicate(
		[](const FTrainingSubSectionState& SubSection)
		{
			return SubSection.DisplayName.ToString() == TEXT("Field Defenses");
		});
	Check(FieldDefenseSection != nullptr, TEXT("hq_has_field_defense_tab"));
	for (const EBuildingExpansionType PitType : GermanPits)
	{
		const FTrainingOption PitOption(EAllUnitType::UNType_BuildingExpansion, static_cast<uint8>(PitType));
		const bool bIsInTab = FieldDefenseSection && FieldDefenseSection->Options.Contains(PitOption);
		Check(bIsInTab, TEXT("pit_in_field_defense_tab"), Global_GetBxpTypeEnumAsString(PitType));
	}

	// The pits are no longer free expansions of the HQ.
	const IBuildingExpansionOwner* BxpOwner = Cast<IBuildingExpansionOwner>(M_HQ.Get());
	bool bHQStillOffersPit = false;
	if (BxpOwner)
	{
		for (const FBxpOptionData& Option : BxpOwner->GetUnlockedBuildingExpansionTypes())
		{
			bHQStillOffersPit |= GermanPits.Contains(Option.ExpansionType);
		}
	}
	Check(BxpOwner && not bHQStillOffersPit, TEXT("pits_removed_from_hq_expansions"));

	const FTrainingOption LeFHOption(EAllUnitType::UNType_BuildingExpansion,
	                                 static_cast<uint8>(EBuildingExpansionType::BTX_GerLeFH18Pit));
	const FTrainingOption SFHOption(EAllUnitType::UNType_BuildingExpansion,
	                                static_cast<uint8>(EBuildingExpansionType::BTX_GerSFH18Pit));
	URTSRequirement* LeFHRequirement = GetRequirementTemplate(LeFHOption);
	URTSRequirement* SFHRequirement = GetRequirementTemplate(SFHOption);
	const FTrainingOption RequiredUnit = IsValid(LeFHRequirement) ? LeFHRequirement->GetRequiredUnit() : FTrainingOption();
	Check(RequiredUnit == FTrainingOption(EAllUnitType::UNType_Nomadic,
	                                      static_cast<uint8>(ENomadicSubtype::Nomadic_GerCommunicationCenter)),
	      TEXT("lefh_requires_communication_center"), RequiredUnit.GetTrainingName());
	Check(IsValid(SFHRequirement), TEXT("sfh_has_requirement"));
	Check(not IsValid(GetRequirementTemplate(Pak40PitOption)), TEXT("pak40_has_no_requirement"));
	constexpr bool bIsCheckingForQueuedItem = false;
	const bool bLeFHUnlocked = IsValid(LeFHRequirement)
		&& LeFHRequirement->CheckIsRequirementMet(Trainer, bIsCheckingForQueuedItem);
	Check(not bLeFHUnlocked, TEXT("lefh_locked_without_communication_center"));
}

void UFieldDefenseTestSubsystem::TickPlanningFirstPit()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer))
	{
		Check(false, TEXT("trainer_valid"));
		Finish();
		return;
	}
	switch (M_PhaseStep++)
	{
	case 0:
		{
			// A unit trains first so that the pit has to wait for its turn.
			Trainer->AddToQueue(MakeQueueItem(HarvesterOption));
			Check(GetQueuedCount(HarvesterOption) == 1, TEXT("unit_queued_first"));
			const bool bFirstPitPlaced = PlacePitAtValidSpot(M_FirstPitSpot, Pak40PitOption);
			Check(bFirstPitPlaced, TEXT("first_pit_placed"), M_FirstPitSpot.ToString());
			Check(GetQueuedCount(Pak40PitOption) == 1, TEXT("first_pit_queued"));
			Check(Trainer->GetPlannedFieldDefenseCount() == 1, TEXT("first_pit_planned"));
			Check(IsValid(FindPreviewNear(M_FirstPitSpot)), TEXT("first_pit_preview_at_spot"));
			ScheduleCapture(TEXT("1_planned_pit_waits_behind_unit"), M_FirstPitSpot, 1.f);
			return;
		}
	case 1:
		{
			// The planned footprint blocks the spot and the tiles around it for the next placement.
			Check(StartPlacing(Pak40PitOption), TEXT("second_placement_started"));
			float InclineDegrees = 0.f;
			ACPPController* Controller = GetController();
			const bool bSameSpotValid = Controller->CPPConstructionPreviewRef->MapTest_EvaluateFieldDefensePlacement(
				M_FirstPitSpot, InclineDegrees);
			Check(not bSameSpotValid, TEXT("planned_spot_blocked"));
			const FBuildingFootprint Footprint = BxpConstructionRules::TGameBxpConstructionRules.FindRef(
				EBuildingExpansionType::BTX_GerPak40Pit).Footprint;
			// Half a footprint over: the two footprints overlap by half.
			const FVector HalfOver = M_FirstPitSpot + FVector(Footprint.SizeX * 0.5f, 0.f, 0.f);
			const bool bHalfOverValid = Controller->CPPConstructionPreviewRef->MapTest_EvaluateFieldDefensePlacement(
				HalfOver, InclineDegrees);
			Check(not bHalfOverValid, TEXT("overlapping_footprint_blocked"));
			const FBox2D PlannedArea = Controller->CPPConstructionPreviewRef->MapTest_GetFieldDefenseFootprintArea();
			Check(PlannedArea.GetSize().X >= Footprint.SizeX && PlannedArea.GetSize().Y >= Footprint.SizeY,
			      TEXT("footprint_covers_rules_size"), PlannedArea.GetSize().ToString());
			return;
		}
	case 2:
		{
			FVector SlopeSpot;
			Check(FindValidSpot(SlopeSpot), TEXT("slope_test_spot_found"));
			CheckInclineOnSlab(SlopeSpot);
			EnterPhase(EFieldDefenseTestPhase::WaitingBehindUnit, UnitTrainingTimeoutSeconds);
			return;
		}
	default:
		return;
	}
}

void UFieldDefenseTestSubsystem::CheckInclineOnSlab(const FVector& Spot)
{
	using namespace FieldDefenseTestPrivate;
	UWorld* World = GetWorld();
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	ACPPController* Controller = GetController();
	if (not IsValid(World) || not IsValid(CubeMesh) || not IsValid(Controller))
	{
		Check(false, TEXT("slab_setup"));
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SlabCenter = Spot + FVector(0.f, 0.f, SlabLift);
	AStaticMeshActor* Slab = World->SpawnActor<AStaticMeshActor>(SlabCenter, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Slab))
	{
		Check(false, TEXT("slab_spawned"));
		return;
	}
	UStaticMeshComponent* SlabMesh = Slab->GetStaticMeshComponent();
	SlabMesh->SetMobility(EComponentMobility::Movable);
	SlabMesh->SetStaticMesh(CubeMesh);
	// The engine cube is 100 cm a side.
	SlabMesh->SetWorldScale3D(FVector(SlabSize / 100.f, SlabSize / 100.f, 1.f));
	SlabMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlabMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Only ground for the incline traces; the placement grid does not see it.
	SlabMesh->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Block);

	auto EvaluateOnSlab = [&](const float PitchDegrees, float& OutInclineDegrees)
	{
		Slab->SetActorRotation(FRotator(PitchDegrees, 0.f, 0.f));
		return Controller->CPPConstructionPreviewRef->MapTest_EvaluateFieldDefensePlacement(SlabCenter,
			OutInclineDegrees);
	};
	float SteepIncline = 0.f;
	const bool bSteepValid = EvaluateOnSlab(SteepSlabDegrees, SteepIncline);
	Check(not bSteepValid, TEXT("steep_slope_refused"), FString::Printf(TEXT("incline=%.2f"), SteepIncline));
	Check(FMath::Abs(SteepIncline - SteepSlabDegrees) < InclineTolerance, TEXT("steep_slope_measured"),
	      FString::Printf(TEXT("incline=%.2f"), SteepIncline));
	float GentleIncline = 0.f;
	const bool bGentleValid = EvaluateOnSlab(GentleSlabDegrees, GentleIncline);
	Check(bGentleValid, TEXT("gentle_slope_accepted"), FString::Printf(TEXT("incline=%.2f"), GentleIncline));
	Check(FMath::Abs(GentleIncline - GentleSlabDegrees) < InclineTolerance, TEXT("gentle_slope_measured"),
	      FString::Printf(TEXT("incline=%.2f"), GentleIncline));
	Slab->Destroy();
}

void UFieldDefenseTestSubsystem::TickWaitingBehindUnit()
{
	using namespace FieldDefenseTestPrivate;
	const UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer))
	{
		Check(false, TEXT("trainer_valid"));
		Finish();
		return;
	}
	ABuildingExpansion* Pit = FindPitNear(M_FirstPitSpot);
	if (GetQueuedCount(HarvesterOption) > 0)
	{
		// The unit ahead of the pit is still in training: the pit must not be built yet.
		bM_SawPitBeforeItsTurn |= IsValid(Pit) || Trainer->GetPlannedFieldDefenseCount() != 1;
		return;
	}
	if (not IsValid(Pit))
	{
		return;
	}
	Check(not bM_SawPitBeforeItsTurn, TEXT("pit_waited_for_unit_ahead"));
	Check(Pit->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_BeingBuild,
	      TEXT("pit_building_at_its_turn"));
	Check(FVector::Dist2D(Pit->GetActorLocation(), M_FirstPitSpot) < SpotMatchDistance, TEXT("pit_at_planned_spot"),
	      FString::Printf(TEXT("pit=%s planned=%s"), *Pit->GetActorLocation().ToString(), *M_FirstPitSpot.ToString()));
	Check(not IsValid(FindPreviewNear(M_FirstPitSpot)), TEXT("preview_replaced_by_pit"));
	Check(Trainer->GetPlannedFieldDefenseCount() == 0, TEXT("started_pit_not_planned"));
	M_FirstPit = Pit;
	M_ConstructionStartSeconds = GetWorld()->GetTimeSeconds();
	ScheduleCapture(TEXT("2_pit_building_at_its_turn"), M_FirstPitSpot, 2.f);
	EnterPhase(EFieldDefenseTestPhase::Constructing, ConstructionTimeoutSeconds);
}

void UFieldDefenseTestSubsystem::TickConstructing()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	const ABuildingExpansion* Pit = M_FirstPit.Get();
	if (not IsValid(Trainer) || not IsValid(Pit))
	{
		Check(false, TEXT("pit_and_trainer_valid_while_building"));
		Finish();
		return;
	}
	float Progress = 0.f;
	(void)Trainer->GetActiveTrainingProgress(Progress);
	const float TotalTime = MakeQueueItem(Pak40PitOption).TotalTrainingTime;
	const float TrainedSeconds = Progress * TotalTime;
	const float BarElapsed = Pit->MapTest_GetConstructionBarElapsedTime();
	if (M_PhaseStep == 0 && GetPhaseSeconds() >= BuildBeforePauseSeconds)
	{
		Check(FMath::Abs(BarElapsed - TrainedSeconds) <= BarToQueueTolerance, TEXT("pit_bar_follows_queue"),
		      FString::Printf(TEXT("bar=%.2f queue=%.2f"), BarElapsed, TrainedSeconds));
		Trainer->SetPauseQueue(true);
		M_PausedTrainedSeconds = TrainedSeconds;
		// Pausing puts the bar exactly at the queue's progress; from then on it must hold.
		M_PausedBarElapsedSeconds = Pit->MapTest_GetConstructionBarElapsedTime();
		Check(FMath::Abs(M_PausedBarElapsedSeconds - TrainedSeconds) <= PausedProgressTolerance,
		      TEXT("paused_pit_bar_at_queue_progress"),
		      FString::Printf(TEXT("bar=%.2f queue=%.2f"), M_PausedBarElapsedSeconds, TrainedSeconds));
		++M_PhaseStep;
		return;
	}
	if (M_PhaseStep == 1 && GetPhaseSeconds() >= BuildBeforePauseSeconds + PauseSeconds)
	{
		Check(FMath::IsNearlyEqual(TrainedSeconds, M_PausedTrainedSeconds), TEXT("queue_held_during_pause"),
		      FString::Printf(TEXT("before=%.2f after=%.2f"), M_PausedTrainedSeconds, TrainedSeconds));
		Check(FMath::Abs(BarElapsed - M_PausedBarElapsedSeconds) <= PausedProgressTolerance,
		      TEXT("pit_bar_held_during_pause"),
		      FString::Printf(TEXT("before=%.2f after=%.2f"), M_PausedBarElapsedSeconds, BarElapsed));
		Check(Pit->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_BeingBuild,
		      TEXT("pit_still_building_while_paused"));
		Trainer->SetPauseQueue(false);
		EnterPhase(EFieldDefenseTestPhase::WaitingForFinish, ConstructionTimeoutSeconds);
	}
}

void UFieldDefenseTestSubsystem::TickWaitingForFinish()
{
	using namespace FieldDefenseTestPrivate;
	const ABuildingExpansion* Pit = M_FirstPit.Get();
	if (not IsValid(Pit))
	{
		Check(false, TEXT("pit_valid_until_built"));
		Finish();
		return;
	}
	if (Pit->GetBuildingExpansionStatus() != EBuildingExpansionStatus::BXS_Built)
	{
		return;
	}
	const float BuildSeconds = GetWorld()->GetTimeSeconds() - M_ConstructionStartSeconds;
	const float ExpectedSeconds = MakeQueueItem(Pak40PitOption).TotalTrainingTime + PauseSeconds;
	Check(FMath::Abs(BuildSeconds - ExpectedSeconds) <= 2.f * BarToQueueTolerance, TEXT("pit_built_in_training_time"),
	      FString::Printf(TEXT("took=%.1f expected=%.1f"), BuildSeconds, ExpectedSeconds));
	Check(GetQueuedCount(Pak40PitOption) == 0, TEXT("finished_pit_left_queue"));
	Check(Pit->GetIsFieldDefense(), TEXT("pit_marked_field_defense"));
	ScheduleCapture(TEXT("3_pit_built"), M_FirstPitSpot, 1.f);
	EnterPhase(EFieldDefenseTestPhase::CancellingPlanned, UnitTrainingTimeoutSeconds);
}

void UFieldDefenseTestSubsystem::TickCancellingPlanned()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	ACPPController* Controller = GetController();
	if (not IsValid(Trainer) || not IsValid(Controller))
	{
		Check(false, TEXT("trainer_valid"));
		Finish();
		return;
	}
	switch (M_PhaseStep)
	{
	case 0:
		{
			// Cancelling happens from the training menu, so the HQ is selected as it would be.
			Controller->SelectSingleUnitFromUI(M_HQ.Get());
			Trainer->AddToQueue(MakeQueueItem(HarvesterOption));
			Check(PlacePitAtValidSpot(M_PhaseSpot, Pak40PitOption), TEXT("pit_placed_to_cancel"));
			++M_PhaseStep;
			return;
		}
	case 1:
		{
			// The queue is paused around the cancel, so nothing is paid between the two readings.
			Trainer->SetPauseQueue(true);
			const int32 RadixiteBefore = GetResourceAmount(ERTSResourceType::Resource_Radixite);
			const int32 MetalBefore = GetResourceAmount(ERTSResourceType::Resource_Metal);
			const bool bRemoved = Trainer->RemoveLastInstanceOfTypeFromQueue(Pak40PitOption);
			const int32 RadixiteAfter = GetResourceAmount(ERTSResourceType::Resource_Radixite);
			const int32 MetalAfter = GetResourceAmount(ERTSResourceType::Resource_Metal);
			Trainer->SetPauseQueue(false);
			Check(bRemoved, TEXT("planned_pit_cancelled"));
			Check(GetQueuedCount(Pak40PitOption) == 0 && Trainer->GetPlannedFieldDefenseCount() == 0,
			      TEXT("cancelled_pit_left_queue"));
			Check(RadixiteAfter == RadixiteBefore && MetalAfter == MetalBefore, TEXT("unstarted_pit_refunds_nothing"),
			      FString::Printf(TEXT("radixite %d->%d metal %d->%d"), RadixiteBefore, RadixiteAfter, MetalBefore,
			                      MetalAfter));
			Check(not IsValid(FindPreviewNear(M_PhaseSpot)), TEXT("cancelled_preview_destroyed"));
			++M_PhaseStep;
			return;
		}
	default:
		// The next pit must start right away, so the harvester ahead finishes first.
		if (GetQueuedCount(HarvesterOption) == 0)
		{
			EnterPhase(EFieldDefenseTestPhase::CancellingConstructing, ConstructionTimeoutSeconds);
		}
		return;
	}
}

void UFieldDefenseTestSubsystem::TickCancellingConstructing()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer))
	{
		Check(false, TEXT("trainer_valid"));
		Finish();
		return;
	}
	if (M_PhaseStep == 0)
	{
		Check(PlacePitAtValidSpot(M_PhaseSpot, Pak40PitOption), TEXT("pit_placed_on_empty_queue"));
		++M_PhaseStep;
		return;
	}
	if (M_PhaseStep == 1)
	{
		ABuildingExpansion* Pit = FindPitNear(M_PhaseSpot);
		if (not IsValid(Pit))
		{
			return;
		}
		// The queue was empty: the pit starts at once, with no unit or queue tick in between.
		Check(GetPhaseSeconds() <= BarToQueueTolerance, TEXT("pit_starts_at_once_on_empty_queue"),
		      FString::Printf(TEXT("after=%.2f"), GetPhaseSeconds()));
		M_PhasePit = Pit;
		M_PhaseMarkSeconds = GetWorld()->GetTimeSeconds();
		++M_PhaseStep;
		return;
	}
	if (M_PhaseStep == 2 && GetWorld()->GetTimeSeconds() - M_PhaseMarkSeconds >= BuildBeforeCancelSeconds)
	{
		const FTrainingQueueItem PitItem = MakeQueueItem(Pak40PitOption);
		// Cancelling is only reachable from the HQ's own training menu, so the HQ is the selected unit.
		GetController()->SelectSingleUnitFromUI(M_HQ.Get());
		Trainer->SetPauseQueue(true);
		float Progress = 0.f;
		(void)Trainer->GetActiveTrainingProgress(Progress);
		const int32 TrainedTicks = FMath::RoundToInt32(Progress * PitItem.TotalTrainingTime);
		const int32 RadixiteBefore = GetResourceAmount(ERTSResourceType::Resource_Radixite);
		const int32 MetalBefore = GetResourceAmount(ERTSResourceType::Resource_Metal);
		const bool bRemoved = Trainer->RemoveLastInstanceOfTypeFromQueue(Pak40PitOption);
		const int32 RadixiteRefund = GetResourceAmount(ERTSResourceType::Resource_Radixite) - RadixiteBefore;
		const int32 MetalRefund = GetResourceAmount(ERTSResourceType::Resource_Metal) - MetalBefore;
		Trainer->SetPauseQueue(false);
		// Each tick pays its share of the total, so after N ticks N * cost / time (rounded down) was paid.
		const int32 ExpectedRadixite = TrainedTicks * PitItem.TotalResourceCosts.FindRef(
			ERTSResourceType::Resource_Radixite) / PitItem.TotalTrainingTime;
		const int32 ExpectedMetal = TrainedTicks * PitItem.TotalResourceCosts.FindRef(
			ERTSResourceType::Resource_Metal) / PitItem.TotalTrainingTime;
		Check(bRemoved, TEXT("building_pit_cancelled"));
		Check(RadixiteRefund == ExpectedRadixite && MetalRefund == ExpectedMetal && ExpectedRadixite > 0,
		      TEXT("building_pit_refunds_paid_part"),
		      FString::Printf(TEXT("ticks=%d radixite=%d/%d metal=%d/%d"), TrainedTicks, RadixiteRefund,
		                      ExpectedRadixite, MetalRefund, ExpectedMetal));
		++M_PhaseStep;
		return;
	}
	if (M_PhaseStep == 3)
	{
		Check(not RTSFunctionLibrary::RTSIsValid(M_PhasePit.Get()), TEXT("cancelled_construction_destroyed"));
		Check(GetQueuedCount(Pak40PitOption) == 0, TEXT("cancelled_construction_left_queue"));
		EnterPhase(EFieldDefenseTestPhase::LosingConstruction, ConstructionTimeoutSeconds);
	}
}

void UFieldDefenseTestSubsystem::TickLosingConstruction()
{
	using namespace FieldDefenseTestPrivate;
	switch (M_PhaseStep)
	{
	case 0:
		Check(PlacePitAtValidSpot(M_PhaseSpot, Pak40PitOption), TEXT("pit_placed_to_lose"));
		++M_PhaseStep;
		return;
	case 1:
		Check(PlacePitAtValidSpot(M_SecondPhaseSpot, Pak40PitOption), TEXT("pit_planned_behind_lost_pit"));
		++M_PhaseStep;
		return;
	case 2:
		{
			ABuildingExpansion* Pit = FindPitNear(M_PhaseSpot);
			if (not IsValid(Pit))
			{
				return;
			}
			M_PhasePit = Pit;
			// The enemy destroys the pit while it is being built.
			Pit->TakeFatalDamage();
			++M_PhaseStep;
			return;
		}
	case 3:
		{
			// The one second requirement check takes the lost pit out; the planned one behind it starts.
			const ABuildingExpansion* NextPit = FindPitNear(M_SecondPhaseSpot);
			if (GetQueuedCount(Pak40PitOption) != 1 || not IsValid(NextPit))
			{
				return;
			}
			Check(NextPit->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_BeingBuild,
			      TEXT("next_pit_starts_after_loss"), FString::Printf(TEXT("after=%.1fs"), GetPhaseSeconds()));
			Check(CancelLastQueuedPit(Pak40PitOption), TEXT("next_pit_cancelled_for_cleanup"));
			++M_PhaseStep;
			return;
		}
	default:
		Check(GetQueuedCount(Pak40PitOption) == 0, TEXT("loss_phase_left_empty_queue"));
		EnterPhase(EFieldDefenseTestPhase::CheckingTierTwo, UnitTrainingTimeoutSeconds);
		return;
	}
}

void UFieldDefenseTestSubsystem::SpawnTierTwoBuilding()
{
	using namespace FieldDefenseTestPrivate;
	const ACPPController* Controller = GetController();
	ARTSAsyncSpawner* Spawner = IsValid(Controller) ? Controller->GetRTSAsyncSpawner() : nullptr;
	const ANomadicVehicle* HQ = M_HQ.Get();
	FVector Ground;
	if (not IsValid(Spawner) || not IsValid(HQ)
		|| not GetGroundAt(FVector2D(HQ->GetActorLocation()) + FVector2D(0.f, -TierTwoBuildingDistance), Ground))
	{
		Check(false, TEXT("tier_two_spawn_setup"));
		return;
	}
	TWeakObjectPtr<UFieldDefenseTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		CommunicationCenterOption, Ground, this, TierTwoSpawnId,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->M_TierTwoBuilding = SpawnedActor;
			}
		},
		FRotator::ZeroRotator);
	Check(bRequested, TEXT("tier_two_spawn_requested"));
}

void UFieldDefenseTestSubsystem::TickCheckingTierTwo()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	URTSRequirement* LeFHRequirement = GetRequirementTemplate(LeFHPitOption);
	URTSRequirement* SFHRequirement = GetRequirementTemplate(SFHPitOption);
	if (not IsValid(Trainer) || not IsValid(LeFHRequirement) || not IsValid(SFHRequirement))
	{
		Check(false, TEXT("trainer_and_requirements_valid"));
		Finish();
		return;
	}
	constexpr bool bIsCheckingForQueuedItem = false;
	switch (M_PhaseStep)
	{
	case 0:
		SpawnTierTwoBuilding();
		++M_PhaseStep;
		return;
	case 1:
		{
			if (not RTSFunctionLibrary::RTSIsValid(M_TierTwoBuilding.Get())
				|| not LeFHRequirement->CheckIsRequirementMet(Trainer, bIsCheckingForQueuedItem))
			{
				return;
			}
			Check(true, TEXT("lefh_unlocked_by_communication_center"));
			Check(SFHRequirement->CheckIsRequirementMet(Trainer, bIsCheckingForQueuedItem),
			      TEXT("sfh_unlocked_by_communication_center"));
			// A unit first, so the leFH pit stays planned while its requirement is taken away.
			Trainer->AddToQueue(MakeQueueItem(HarvesterOption));
			Check(PlacePitAtValidSpot(M_PhaseSpot, LeFHPitOption), TEXT("lefh_pit_planned"));
			const FBox2D LeFHArea = GetController()->CPPConstructionPreviewRef->MapTest_GetFieldDefenseFootprintArea();
			const FBuildingFootprint LeFHFootprint = BxpConstructionRules::TGameBxpConstructionRules.FindRef(
				EBuildingExpansionType::BTX_GerLeFH18Pit).Footprint;
			Check(LeFHArea.GetSize().X >= LeFHFootprint.SizeX && LeFHArea.GetSize().Y >= LeFHFootprint.SizeY,
			      TEXT("lefh_footprint_covers_rules_size"), LeFHArea.GetSize().ToString());
			++M_PhaseStep;
			return;
		}
	case 2:
		{
			AHpPawnMaster* TierTwoBuilding = Cast<AHpPawnMaster>(M_TierTwoBuilding.Get());
			Check(IsValid(TierTwoBuilding), TEXT("tier_two_building_can_die"));
			if (IsValid(TierTwoBuilding))
			{
				TierTwoBuilding->TakeFatalDamage();
			}
			++M_PhaseStep;
			return;
		}
	case 3:
		if (GetQueuedCount(LeFHPitOption) != 0)
		{
			return;
		}
		Check(true, TEXT("lefh_pit_removed_when_tier_two_lost"),
		      FString::Printf(TEXT("after=%.1fs"), GetPhaseSeconds()));
		Check(not IsValid(FindPreviewNear(M_PhaseSpot)), TEXT("lefh_preview_destroyed_when_tier_two_lost"));
		Check(not LeFHRequirement->CheckIsRequirementMet(Trainer, bIsCheckingForQueuedItem),
		      TEXT("lefh_locked_again_without_tier_two"));
		EnterPhase(EFieldDefenseTestPhase::PlacingWithMouse, UnitTrainingTimeoutSeconds);
		return;
	default:
		return;
	}
}

void UFieldDefenseTestSubsystem::TickPlacingWithMouse()
{
	using namespace FieldDefenseTestPrivate;
	ACPPController* Controller = GetController();
	UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Controller) || not IsValid(Trainer) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		Check(false, TEXT("controller_and_trainer_valid"));
		Finish();
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	switch (M_PhaseStep)
	{
	case 0:
		{
			// An empty queue, so the placed pit becomes the active item whose menu clock a right click pauses.
			if (GetQueuedCount(HarvesterOption) > 0)
			{
				return;
			}
			SetSyntheticCursorMovesEnabled(false);
			Controller->SelectSingleUnitFromUI(M_HQ.Get());
			const UMainGameUI* MainGameUI = Controller->GetMainMenuUI();
			UTrainingMenuManager* MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
			Check(IsValid(MenuManager), TEXT("training_menu_found"));
			if (IsValid(MenuManager))
			{
				MenuManager->OnSubSectionTabClicked(FieldDefenseTabIndex);
			}
			M_QueuedBeforeMouse = GetQueuedCount(Pak40PitOption);
			++M_PhaseStep;
			return;
		}
	case 1:
		Check(ClickTrainingItemWidget(Pak40PitOption, true), TEXT("menu_item_clicked"));
		Check(Controller->MapTest_GetIsFieldDefensePreviewActive(), TEXT("menu_click_starts_placement"));
		Check(FindValidSpot(M_PhaseSpot), TEXT("mouse_spot_found"));
		Controller->FocusCameraOnLocation(M_PhaseSpot);
		M_PhaseMarkSeconds = 0.f;
		++M_PhaseStep;
		return;
	case 2:
		{
			if (GetIsCursorOwnedByVisibleWindow())
			{
				PlaceMousePhasePitWithoutCursor(*Controller, Now);
				return;
			}
			// The camera glides to the spot for a few frames; the cursor is kept on the spot until the preview,
			// which follows the viewport cursor in its own tick, stands there.
			if (M_PhaseMarkSeconds <= 0.f)
			{
				M_PhaseMarkSeconds = Now;
			}
			const bool bOnScreen = MoveViewportCursorTo(M_PhaseSpot);
			const FVector PreviewLocation = Controller->CPPConstructionPreviewRef->GetFieldDefensePlacementLocation();
			const bool bPreviewOnSpot = FVector::Dist2D(PreviewLocation, M_PhaseSpot) <= CursorToPreviewTolerance
				&& not Controller->CPPConstructionPreviewRef->GetIsBuildingPreviewBlocked();
			if (not bPreviewOnSpot && Now - M_PhaseMarkSeconds < CursorSettleSeconds)
			{
				return;
			}
			Check(bOnScreen, TEXT("mouse_spot_on_screen"), M_CursorAbsolutePosition.ToString());
			// The camera must not move while the cursor aims at the screen.
			constexpr bool bFocusCamera = false;
			ScheduleCapture(TEXT("5_preview_under_cursor_with_menu"), M_PhaseSpot, 0.f, bFocusCamera);
			Check(bPreviewOnSpot, TEXT("preview_follows_viewport_cursor"),
			      FString::Printf(TEXT("preview=%s spot=%s after=%.2fs"), *PreviewLocation.ToString(),
			                      *M_PhaseSpot.ToString(), Now - M_PhaseMarkSeconds));
			// Press now and release on the next frame, as a player's click reaches the input stack.
			SendViewportLeftButton(true);
			M_PhaseStep = 3;
			return;
		}
	case 3:
		// The cursor stays where it was pressed, as a player's does; a moved cursor makes the release a box selection.
		SendViewportLeftButton(false);
		M_PhaseMarkSeconds = Now;
		++M_PhaseStep;
		return;
	case 4:
		{
			const bool bQueued = GetQueuedCount(Pak40PitOption) == M_QueuedBeforeMouse + 1;
			if (not bQueued && Now - M_PhaseMarkSeconds < ClickSettleSeconds)
			{
				return;
			}
			Check(bQueued, TEXT("viewport_click_places_pit"));
			Check(not Controller->MapTest_GetIsFieldDefensePreviewActive(), TEXT("placement_ends_after_click"));
			M_SecondPhaseSpot = Controller->CPPConstructionPreviewRef->GetFieldDefensePlacementLocation();
			Check(IsValid(FindPreviewNear(M_SecondPhaseSpot)) || IsValid(FindPitNear(M_SecondPhaseSpot)),
			      TEXT("clicked_pit_at_cursor_spot"), M_SecondPhaseSpot.ToString());
			++M_PhaseStep;
			return;
		}
	case MousePhaseRightClickStep:
		{
			// Frames after the click: a release counted as a box selection would have picked the new pit by now.
			if (Now - M_PhaseMarkSeconds < ClickSettleSeconds)
			{
				return;
			}
			const AActor* PrimaryAfterClick = Controller->GetPrimarySelectedUnit();
			Check(PrimaryAfterClick == M_HQ.Get(), TEXT("hq_stays_selected_after_placement"),
			      IsValid(PrimaryAfterClick) ? PrimaryAfterClick->GetName() : TEXT("none"));
		}
		// The first right click on the item pauses the queue, as in the training menu.
		Check(ClickTrainingItemWidget(Pak40PitOption, false), TEXT("menu_item_right_clicked"));
		Check(Trainer->GetIsPaused(), TEXT("menu_right_click_pauses_queue"));
		++M_PhaseStep;
		return;
	case 6:
		// The second right click removes the last pit of that type.
		Check(ClickTrainingItemWidget(Pak40PitOption, false), TEXT("menu_item_right_clicked_again"));
		Check(GetQueuedCount(Pak40PitOption) == M_QueuedBeforeMouse, TEXT("menu_right_click_cancels_pit"));
		Trainer->SetPauseQueue(false);
		++M_PhaseStep;
		return;
	default:
		Check(not IsValid(FindPreviewNear(M_SecondPhaseSpot))
		      && not RTSFunctionLibrary::RTSIsValid(FindPitNear(M_SecondPhaseSpot)),
		      TEXT("menu_cancel_removes_preview_or_pit"));
		SetSyntheticCursorMovesEnabled(true);
		EnterPhase(EFieldDefenseTestPhase::PlanningAhead, UnitTrainingTimeoutSeconds);
		return;
	}
}

bool UFieldDefenseTestSubsystem::GetIsCursorOwnedByVisibleWindow()
{
	const TSharedPtr<SViewport> ViewportWidget = GEngine != nullptr && IsValid(GEngine->GameViewport)
		                                             ? GEngine->GameViewport->GetGameViewportWidget()
		                                             : nullptr;
	return ViewportWidget.IsValid() && ViewportWidget->HasMouseCapture();
}

void UFieldDefenseTestSubsystem::PlaceMousePhasePitWithoutCursor(ACPPController& Controller, const float Now)
{
	// Driving the viewport here would move the player's own cursor; the offscreen run checks the click itself.
	UE_LOG(LogRTSFieldDefenseTest, Display,
	       TEXT("RTS_FIELD_DEFENSE_TEST SKIP viewport_click_placement reason=visible window owns the cursor"
		       " (run with -RenderOffscreen to check it)"));
	float InclineDegrees = 0.f;
	const bool bAccepted = Controller.MapTest_PlaceFieldDefenseAt(M_PhaseSpot, InclineDegrees);
	Check(bAccepted && GetQueuedCount(FieldDefenseTestPrivate::Pak40PitOption) == M_QueuedBeforeMouse + 1,
	      TEXT("menu_started_placement_places_pit"));
	M_SecondPhaseSpot = Controller.CPPConstructionPreviewRef->GetFieldDefensePlacementLocation();
	constexpr bool bFocusCamera = false;
	constexpr float PlannedPitCaptureDelaySeconds = 0.5f;
	ScheduleCapture(TEXT("5_planned_pit_with_menu"), M_SecondPhaseSpot, PlannedPitCaptureDelaySeconds, bFocusCamera);
	// On to the selection and right click checks, which do not need the cursor.
	M_PhaseMarkSeconds = Now;
	M_PhaseStep = FieldDefenseTestPrivate::MousePhaseRightClickStep;
}

UW_TrainingItem* UFieldDefenseTestSubsystem::FindTrainingItemWidget(const FTrainingOption& Option) const
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UW_TrainingItem::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		UW_TrainingItem* TrainingItem = Cast<UW_TrainingItem>(Widget);
		if (IsValid(TrainingItem) && TrainingItem->GetTrainingItemState().ItemID == Option)
		{
			return TrainingItem;
		}
	}
	return nullptr;
}

bool UFieldDefenseTestSubsystem::ClickTrainingItemWidget(const FTrainingOption& Option, const bool bIsLeftClick) const
{
	UW_TrainingItem* TrainingItem = FindTrainingItemWidget(Option);
	UFunction* ClickFunction = IsValid(TrainingItem)
		                           ? TrainingItem->FindFunction(TEXT("OnClickedTrainingItem"))
		                           : nullptr;
	if (ClickFunction == nullptr)
	{
		return false;
	}
	// What the item's button calls when the player clicks it.
	struct FClickParameters
	{
		bool bIsLeftClick;
	};
	FClickParameters Parameters{bIsLeftClick};
	TrainingItem->ProcessEvent(ClickFunction, &Parameters);
	return true;
}

bool UFieldDefenseTestSubsystem::MoveViewportCursorTo(const FVector& WorldLocation)
{
	const ACPPController* Controller = GetController();
	FSceneViewport* SceneViewport = GEngine != nullptr && IsValid(GEngine->GameViewport)
		                                ? GEngine->GameViewport->GetGameViewport()
		                                : nullptr;
	const TSharedPtr<SViewport> ViewportWidget = GEngine != nullptr && IsValid(GEngine->GameViewport)
		                                             ? GEngine->GameViewport->GetGameViewportWidget()
		                                             : nullptr;
	if (not IsValid(Controller) || SceneViewport == nullptr || not ViewportWidget.IsValid())
	{
		return false;
	}
	// A viewport that holds the mouse would put the machine's cursor back on every move; never touch it then.
	if (ViewportWidget->HasMouseCapture())
	{
		UE_LOG(LogRTSFieldDefenseTest, Display, TEXT("RTS_FIELD_DEFENSE_TEST viewport has mouse capture; no cursor"));
		return false;
	}
	FVector2D ViewportPosition;
	if (not Controller->ProjectWorldLocationToScreen(WorldLocation, ViewportPosition, false))
	{
		return false;
	}
	// The events go straight to the game's viewport, which keeps its own cursor; the machine's cursor stays put.
	const FGeometry& ViewportGeometry = SceneViewport->GetCachedGeometry();
	M_CursorAbsolutePosition = ViewportGeometry.LocalToAbsolute(ViewportPosition / ViewportGeometry.Scale);
	const FPointerEvent MouseMove(
		FSlateApplication::Get().GetUserIndexForMouse(), FSlateApplication::CursorPointerIndex,
		M_CursorAbsolutePosition, M_CursorAbsolutePosition, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState());
	(void)SceneViewport->OnMouseMove(ViewportGeometry, MouseMove);
	FIntPoint ViewportCursor;
	SceneViewport->GetMousePos(ViewportCursor);
	UE_LOG(LogRTSFieldDefenseTest, Display,
	       TEXT("RTS_FIELD_DEFENSE_TEST cursor pixel=%s viewport_cursor=%s size=%s scale=%.2f"),
	       *ViewportPosition.ToString(), *ViewportCursor.ToString(), *ViewportGeometry.GetLocalSize().ToString(),
	       ViewportGeometry.Scale);
	return true;
}

void UFieldDefenseTestSubsystem::SetSyntheticCursorMovesEnabled(const bool bEnabled)
{
	IConsoleVariable* SyntheticMoves = IConsoleManager::Get().FindConsoleVariable(
		FieldDefenseTestPrivate::SyntheticCursorMovesCVar);
	if (SyntheticMoves != nullptr)
	{
		SyntheticMoves->Set(bEnabled);
	}
}

void UFieldDefenseTestSubsystem::SendViewportLeftButton(const bool bIsPressed) const
{
	FSceneViewport* SceneViewport = GEngine != nullptr && IsValid(GEngine->GameViewport)
		                                ? GEngine->GameViewport->GetGameViewport()
		                                : nullptr;
	if (SceneViewport == nullptr || not FSlateApplication::IsInitialized())
	{
		return;
	}
	const FGeometry& ViewportGeometry = SceneViewport->GetCachedGeometry();
	const TSet<FKey> PressedButtons = bIsPressed ? TSet<FKey>({EKeys::LeftMouseButton}) : TSet<FKey>();
	const FPointerEvent ButtonEvent(FSlateApplication::Get().GetUserIndexForMouse(),
	                                FSlateApplication::CursorPointerIndex, M_CursorAbsolutePosition,
	                                M_CursorAbsolutePosition, PressedButtons, EKeys::LeftMouseButton, 0,
	                                FModifierKeysState());
	if (bIsPressed)
	{
		(void)SceneViewport->OnMouseButtonDown(ViewportGeometry, ButtonEvent);
		return;
	}
	(void)SceneViewport->OnMouseButtonUp(ViewportGeometry, ButtonEvent);
}

void UFieldDefenseTestSubsystem::TickPlanningAhead()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer))
	{
		Check(false, TEXT("trainer_valid"));
		Finish();
		return;
	}
	if (M_PhaseStep == 0)
	{
		// Another unit holds the pits back so that they all stay planned.
		Trainer->AddToQueue(MakeQueueItem(HarvesterOption));
		++M_PhaseStep;
		return;
	}
	if (M_PhaseStep <= PitsToPlanAhead)
	{
		FVector Spot;
		const bool bPlaced = PlacePitAtValidSpot(Spot, Pak40PitOption);
		Check(bPlaced, TEXT("planned_pit_placed"), FString::Printf(TEXT("index=%d"), M_PhaseStep));
		M_PlannedSpots.Add(Spot);
		M_PlannedPreviews.Add(FindPreviewNear(Spot));
		++M_PhaseStep;
		return;
	}
	if (not M_PlannedSpots.IsEmpty())
	{
		ScheduleCapture(TEXT("4_eight_pits_planned"), M_PlannedSpots[0], 1.f);
	}
	Check(Trainer->GetPlannedFieldDefenseCount() == PitsToPlanAhead, TEXT("eight_pits_planned"),
	      FString::Printf(TEXT("planned=%d"), Trainer->GetPlannedFieldDefenseCount()));
	// The ninth is refused before its preview starts.
	Check(not StartPlacing(Pak40PitOption), TEXT("ninth_pit_refused"));
	Check(Trainer->GetPlannedFieldDefenseCount() == PitsToPlanAhead, TEXT("refusal_keeps_eight_planned"));
	EnterPhase(EFieldDefenseTestPhase::WaitingForNextConstruction, UnitTrainingTimeoutSeconds);
}

void UFieldDefenseTestSubsystem::TickWaitingForNextConstruction()
{
	using namespace FieldDefenseTestPrivate;
	UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer))
	{
		Check(false, TEXT("trainer_valid"));
		Finish();
		return;
	}
	if (M_PhaseStep == 0)
	{
		if (GetQueuedCount(HarvesterOption) > 0 || M_PlannedSpots.IsEmpty())
		{
			return;
		}
		ABuildingExpansion* NextPit = FindPitNear(M_PlannedSpots[0]);
		if (not IsValid(NextPit))
		{
			return;
		}
		M_PitUnderConstructionAtDeath = NextPit;
		Check(Trainer->GetPlannedFieldDefenseCount() == PitsToPlanAhead - 1, TEXT("next_pit_left_planned_count"));
		++M_PhaseStep;
		return;
	}
	if (M_PhaseStep == 1)
	{
		// The pit under construction no longer counts: one more may be planned.
		FVector Spot;
		Check(PlacePitAtValidSpot(Spot, Pak40PitOption), TEXT("pit_planned_while_one_builds"));
		M_PlannedPreviews.Add(FindPreviewNear(Spot));
		++M_PhaseStep;
		return;
	}
	Check(not StartPlacing(Pak40PitOption), TEXT("limit_counts_only_planned_pits"));
	ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(HQ))
	{
		Check(false, TEXT("hq_alive_before_kill"));
		Finish();
		return;
	}
	if (not M_PlannedSpots.IsEmpty())
	{
		ScheduleCapture(TEXT("6_hq_destroyed_previews_gone"), M_PlannedSpots[0], DeathCheckDelaySeconds - 0.5f);
	}
	HQ->TakeFatalDamage();
	EnterPhase(EFieldDefenseTestPhase::CheckingOwnerDeath, ConstructionTimeoutSeconds);
}

void UFieldDefenseTestSubsystem::TickCheckingOwnerDeath()
{
	using namespace FieldDefenseTestPrivate;
	if (GetPhaseSeconds() < DeathCheckDelaySeconds)
	{
		return;
	}
	int32 PreviewsLeft = 0;
	for (const TWeakObjectPtr<AStaticPreviewMesh>& Preview : M_PlannedPreviews)
	{
		PreviewsLeft += Preview.IsValid() ? 1 : 0;
	}
	// The pits planned ahead plus the one planned while the first of them was being built.
	Check(M_PlannedPreviews.Num() == PitsToPlanAhead + 1, TEXT("previews_tracked"),
	      FString::Printf(TEXT("tracked=%d"), M_PlannedPreviews.Num()));
	Check(PreviewsLeft == 0, TEXT("previews_die_with_hq"), FString::Printf(TEXT("left=%d"), PreviewsLeft));
	Check(not RTSFunctionLibrary::RTSIsValid(M_PitUnderConstructionAtDeath.Get()),
	      TEXT("unfinished_pit_dies_with_hq"));
	const ABuildingExpansion* FirstPit = M_FirstPit.Get();
	Check(RTSFunctionLibrary::RTSIsValid(M_FirstPit.Get()) && FirstPit->GetBuildingExpansionStatus() ==
	      EBuildingExpansionStatus::BXS_Built, TEXT("finished_pit_outlives_hq"));
	Finish();
}

bool UFieldDefenseTestSubsystem::FindHQ()
{
	for (TActorIterator<ANomadicVehicle> NomadicIterator(GetWorld()); NomadicIterator; ++NomadicIterator)
	{
		ANomadicVehicle* Nomadic = *NomadicIterator;
		const URTSComponent* RTSComponent = IsValid(Nomadic) ? Nomadic->GetRTSComponent() : nullptr;
		const bool bIsPlayerHQ = IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == 1
			&& RTSComponent->GetSubtypeAsNomadicSubtype() == ENomadicSubtype::Nomadic_GerHq;
		if (bIsPlayerHQ)
		{
			M_HQ = Nomadic;
			return true;
		}
	}
	return false;
}

void UFieldDefenseTestSubsystem::GivePlayerResources() const
{
	const ACPPController* Controller = GetController();
	UPlayerResourceManager* ResourceManager = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	if (not IsValid(ResourceManager))
	{
		return;
	}
	for (const ERTSResourceType Resource : {
		     ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal,
		     ERTSResourceType::Resource_VehicleParts, ERTSResourceType::Resource_Fuel
	     })
	{
		ResourceManager->CheatAddStorage(Resource, FieldDefenseTestPrivate::TestResourceAmount);
	}
}

void UFieldDefenseTestSubsystem::KeepHQAnimating() const
{
	// Unattended runs never draw the HQ; its expansion montage would not play otherwise.
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(HQ))
	{
		return;
	}
	TArray<USkeletalMeshComponent*> SkeletalMeshes;
	HQ->GetComponents(SkeletalMeshes);
	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
	{
		SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

ACPPController* UFieldDefenseTestSubsystem::GetController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

URTSRequirement* UFieldDefenseTestSubsystem::GetRequirementTemplate(const FTrainingOption& Option) const
{
	const ACPPController* Controller = GetController();
	const UMainGameUI* MainGameUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
	const UTrainingMenuManager* MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	if (not IsValid(MenuManager))
	{
		return nullptr;
	}
	return MenuManager->GetTrainingOptionState(Option).RTSRequirement;
}

FTrainingQueueItem UFieldDefenseTestSubsystem::MakeQueueItem(const FTrainingOption& Option) const
{
	// Built as the training menu builds it for a click on the option.
	FTrainingQueueItem QueueItem;
	QueueItem.ItemID = Option;
	const ACPPController* Controller = GetController();
	const UMainGameUI* MainGameUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
	const UTrainingMenuManager* MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	if (IsValid(MenuManager))
	{
		QueueItem.TotalTrainingTime = MenuManager->GetTrainingOptionState(Option).TrainingTime;
		QueueItem.RemainingTrainingTime = QueueItem.TotalTrainingTime;
	}
	if (const UTrainerComponent* Trainer = M_Trainer.Get())
	{
		QueueItem.WidgetIndex = Trainer->GetTrainingOptions().IndexOfByKey(Option);
	}
	bool bIsValidCosts = false;
	QueueItem.TotalResourceCosts = FRTS_Statics::GetResourceCostsOfTrainingOption(Option, bIsValidCosts, GetWorld());
	QueueItem.RemainingResourceCosts = QueueItem.TotalResourceCosts;
	return QueueItem;
}

bool UFieldDefenseTestSubsystem::StartPlacing(const FTrainingOption& Option)
{
	ACPPController* Controller = GetController();
	UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Controller) || not IsValid(Trainer))
	{
		return false;
	}
	Controller->StartFieldDefensePlacement(Trainer, MakeQueueItem(Option), GetRequirementTemplate(Option));
	return Controller->MapTest_GetIsFieldDefensePreviewActive();
}

bool UFieldDefenseTestSubsystem::FindValidSpot(FVector& OutSpot) const
{
	using namespace FieldDefenseTestPrivate;
	const ACPPController* Controller = GetController();
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(Controller) || not IsValid(HQ) || not IsValid(Controller->CPPConstructionPreviewRef))
	{
		return false;
	}
	const FVector2D Center(HQ->GetActorLocation());
	constexpr float FullCircleDegrees = 360.f;
	const int32 AnglesPerRing = FMath::RoundToInt32(FullCircleDegrees / SpotSearchAngleStep);
	const int32 RingCount = FMath::FloorToInt32((SpotSearchMaxRadius - SpotSearchMinRadius) / SpotSearchRadiusStep) + 1;
	// Rings from the inside out, a full circle of candidates on each.
	for (int32 CandidateIndex = 0; CandidateIndex < RingCount * AnglesPerRing; ++CandidateIndex)
	{
		const float Radius = SpotSearchMinRadius + (CandidateIndex / AnglesPerRing) * SpotSearchRadiusStep;
		const float AngleRadians = FMath::DegreesToRadians((CandidateIndex % AnglesPerRing) * SpotSearchAngleStep);
		const FVector2D Candidate = Center + FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)) * Radius;
		FVector Ground;
		float InclineDegrees = 0.f;
		if (GetGroundAt(Candidate, Ground)
			&& Controller->CPPConstructionPreviewRef->MapTest_EvaluateFieldDefensePlacement(Ground, InclineDegrees))
		{
			OutSpot = Ground;
			return true;
		}
	}
	return false;
}

bool UFieldDefenseTestSubsystem::GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const
{
	const UWorld* World = GetWorld();
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(World) || not IsValid(HQ))
	{
		return false;
	}
	const float Height = HQ->GetActorLocation().Z;
	FHitResult GroundHit;
	const FVector TraceStart(Location2D.X, Location2D.Y, Height + FieldDefenseTestPrivate::GroundTraceHalfHeight);
	const FVector TraceEnd(Location2D.X, Location2D.Y, Height - FieldDefenseTestPrivate::GroundTraceHalfHeight);
	if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE))
	{
		return false;
	}
	OutGround = GroundHit.ImpactPoint;
	return true;
}

bool UFieldDefenseTestSubsystem::PlacePitAtValidSpot(FVector& OutSpot, const FTrainingOption& Option)
{
	ACPPController* Controller = GetController();
	if (not IsValid(Controller) || not StartPlacing(Option))
	{
		return false;
	}
	if (not FindValidSpot(OutSpot))
	{
		return false;
	}
	float InclineDegrees = 0.f;
	const int32 QueuedBefore = GetQueuedCount(Option);
	const bool bAccepted = Controller->MapTest_PlaceFieldDefenseAt(OutSpot, InclineDegrees);
	// The pit is planned at the grid snapped spot the preview checked, not at the clicked ground.
	OutSpot = Controller->CPPConstructionPreviewRef->GetFieldDefensePlacementLocation();
	return bAccepted && GetQueuedCount(Option) == QueuedBefore + 1;
}

bool UFieldDefenseTestSubsystem::CancelLastQueuedPit(const FTrainingOption& Option) const
{
	UTrainerComponent* Trainer = M_Trainer.Get();
	ACPPController* Controller = GetController();
	if (not IsValid(Trainer) || not IsValid(Controller))
	{
		return false;
	}
	// Cancelling is only reachable from the HQ's own training menu, so the HQ is the selected unit.
	Controller->SelectSingleUnitFromUI(M_HQ.Get());
	// A right click only removes an item from a paused queue; the first right click pauses it.
	Trainer->SetPauseQueue(true);
	const bool bRemoved = Trainer->RemoveLastInstanceOfTypeFromQueue(Option);
	Trainer->SetPauseQueue(false);
	return bRemoved;
}

int32 UFieldDefenseTestSubsystem::GetResourceAmount(const ERTSResourceType Resource) const
{
	const ACPPController* Controller = GetController();
	const UPlayerResourceManager* ResourceManager = IsValid(Controller)
		                                                ? Controller->GetPlayerResourceManager()
		                                                : nullptr;
	return IsValid(ResourceManager) ? ResourceManager->GetResourceAmount(Resource) : INDEX_NONE;
}

int32 UFieldDefenseTestSubsystem::GetQueuedCount(const FTrainingOption& Option) const
{
	const UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Trainer))
	{
		return 0;
	}
	int32 ActiveIndex = INDEX_NONE;
	int32 TimeRemaining = 0;
	bool bIsPausedByResources = false;
	return Trainer->GetCurrentTrainingQueueState(ActiveIndex, TimeRemaining, bIsPausedByResources).FindRef(Option);
}

ABuildingExpansion* UFieldDefenseTestSubsystem::FindPitNear(const FVector& Spot) const
{
	for (TActorIterator<ABuildingExpansion> ExpansionIterator(GetWorld()); ExpansionIterator; ++ExpansionIterator)
	{
		ABuildingExpansion* Expansion = *ExpansionIterator;
		if (IsValid(Expansion) && Expansion->GetIsFieldDefense()
			&& FVector::Dist2D(Expansion->GetActorLocation(), Spot) < FieldDefenseTestPrivate::SpotMatchDistance)
		{
			return Expansion;
		}
	}
	return nullptr;
}

AStaticPreviewMesh* UFieldDefenseTestSubsystem::FindPreviewNear(const FVector& Spot) const
{
	for (TActorIterator<AStaticPreviewMesh> PreviewIterator(GetWorld()); PreviewIterator; ++PreviewIterator)
	{
		AStaticPreviewMesh* Preview = *PreviewIterator;
		if (IsValid(Preview)
			&& FVector::Dist2D(Preview->GetActorLocation(), Spot) < FieldDefenseTestPrivate::SpotMatchDistance)
		{
			return Preview;
		}
	}
	return nullptr;
}

#else

bool UFieldDefenseTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UFieldDefenseTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UFieldDefenseTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldDefenseTestSubsystem, STATGROUP_Tickables);
}

#endif
