// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BarbedWireLineTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavModifierComponent.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "RTS_Survival/Navigation/NavQueryFilterSettings/RTSNavQueryFilterSettings.h"
#include "NavigationSystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Buildings/BarbedWireLine/RTSBarbedWireLine.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/W_TrainingItem.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/ConstructionPreview/BarbedWirePlacement/BarbedWirePlacementComponent.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionActor/FieldConstruction.h"
#include "RTS_Survival/RTSComponents/NavCollision/NavCollisionEnvObstacle/RTSNavCollisionEnvObstacle.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRTSBarbedWireTest, Log, All);

#if RTS_WITH_MAP_TESTS
namespace BarbedWireTestPrivate
{
	const TCHAR* const TestSwitch = TEXT("BarbedWireValidate");
	constexpr float StartDelaySeconds = 3.0f;
	constexpr float HQWaitSeconds = 30.0f;
	constexpr float ConversionFastForwardSeconds = 30.0f;
	constexpr float TestTimeoutSeconds = 900.0f;
	constexpr int32 TestResourceAmount = 1000000;
	constexpr float GroundTraceHalfHeight = 5000.0f;
	// The HQ layout's fourth tab.
	constexpr int32 BarriersTabIndex = 3;
	const FTrainingOption WireOption(EAllUnitType::UNType_BuildingExpansion,
	                                 static_cast<uint8>(EBuildingExpansionType::BTX_GerBarbedWireLine));
	// The first line: away from the HQ, then a turn of 45 degrees to the right; inside the HQ's build radius.
	constexpr float StartDistanceFromHQ = 1000.0f;
	constexpr float FirstLegLength = 1600.0f;
	constexpr float SecondLegLength = 600.0f;
	constexpr float TurnDegrees = 45.0f;
	constexpr float CursorStep = 100.0f;
	constexpr int32 MinimumFirstLinePieces = 5;
	constexpr float MinimumBendDegrees = 30.0f;
	// The piece destroyed by damage; its neighbours stay.
	constexpr int32 GapPieceIndex = 1;
	// The snapped line runs to the left of the HQ for this long; the tank crushes its piece CrushPieceIndex.
	constexpr float SnappedLineLength = 1600.0f;
	constexpr int32 CrushPieceIndex = 2;
	// Where the cursor is beside an open end when the click should snap to it.
	const FVector SnapCursorOffset(60.0f, 40.0f, 0.0f);
	constexpr float SnapTolerance = 1.0f;
	// The infantry path is probed this far to either side of a piece.
	constexpr float NavProbeHalfWidth = 500.0f;
	constexpr float NavProjectionExtent = 300.0f;
	// Navigation tiles rebuild a moment after a modifier changes; the checks wait at least this long, then measure
	// every tick until the path changes or the longest wait is over (rendered runs rebuild slower).
	constexpr float NavSettleSeconds = 1.0f;
	constexpr float NavMaximumWaitSeconds = 20.0f;
	// The cancel step's line starts this far in front of the HQ and beside the first line, outside its snap radius,
	// and is drawn into the HQ: uncut it would be BlockedLinePieces long.
	constexpr float BlockedLineStartDistance = 2200.0f;
	constexpr float BlockedLineSideOffset = 500.0f;
	constexpr int32 BlockedLinePieces = 6;
	// A blocked line sends infantry round its end: much longer than straight across.
	constexpr float BlockedDetourFactor = 1.6f;
	constexpr float OpenPathFactor = 1.3f;
	const TCHAR* const VehicleClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H."
		"Ger_Vehicle_PanzerIV_H_C");
	constexpr float VehicleSpawnSideDistance = 1500.0f;
	constexpr float VehicleSettleSeconds = 2.0f;
	constexpr float VehicleDropHeight = 60.0f;
	constexpr float CrushWaitSeconds = 3.0f;
	const TCHAR* const WireClassName = TEXT("Ger_BarbedWire_FieldConstruction");
	const TCHAR* const ScavengerWireClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_BarbedWire_FieldConstruction."
		"Ger_BarbedWire_FieldConstruction_C");
	const TCHAR* const EnemyVehicleClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476."
		"Rus_Vehicle_T3476_C");
	// The scavenger wire stands to the left of the HQ, clear of the lines.
	constexpr float ScavengerWireForward = 1500.0f;
	constexpr float ScavengerWireLeft = 1800.0f;
	constexpr float EnemySpawnDistance = 1500.0f;
	// A shell's blast on one piece: smaller than the distance to the neighbouring pieces.
	constexpr float BlastRadius = 100.0f;
	constexpr float BlastHeight = 30.0f;
	constexpr float BlastDamage = 1.0e6f;
	// Shots at the wire from beside it, every few centimetres up to its top.
	constexpr float WeaponTraceSideDistance = 400.0f;
	constexpr float WeaponTraceStepHeight = 10.0f;
	constexpr float WeaponTraceTopHeight = 120.0f;
	constexpr int32 FramesToWriteLastCapture = 5;
	// Camera arm length for the screenshots: close enough to see the coils.
	constexpr float CaptureZoomLevel = 2400.0f;
	// The lines go the first way from the HQ, in this order, whose area holds no building (the other trucks of the
	// map expand into buildings when it is rendered). The area runs from TestAreaNear to TestAreaFar out of the HQ and
	// TestAreaHalfWidth to either side of TestAreaSideCentre (the snapped line runs to the left).
	constexpr float TestYawOffsets[] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f};
	constexpr float TestAreaNear = 600.0f;
	constexpr float TestAreaFar = 4600.0f;
	constexpr float TestAreaHalfWidth = 1900.0f;
	constexpr float TestAreaSideCentre = -400.0f;
	constexpr float TestAreaHalfHeight = 400.0f;
	// How far an expanded nomadic building reaches from where its truck stood.
	constexpr float NomadicBuildingRadius = 1500.0f;
	const TCHAR* const BlockCharAreaName = TEXT("BlockChar");
}
#endif

bool UBarbedWireLineTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld() &&
		FParse::Param(FCommandLine::Get(), BarbedWireTestPrivate::TestSwitch);
#else
	return false;
#endif
}

TStatId UBarbedWireLineTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBarbedWireLineTestSubsystem, STATGROUP_Tickables);
}

void UBarbedWireLineTestSubsystem::Tick(const float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	const UWorld* World = GetWorld();
	if (bM_IsExitPending)
	{
		TickExitPending();
		return;
	}
	if (bM_IsFinished || not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	TakeDueCapture();
	if (M_TestStartSeconds < 0.0f)
	{
		FParse::Value(FCommandLine::Get(), TEXT("BarbedWireCapture="), M_CaptureDirectory);
		M_TestStartSeconds = World->GetTimeSeconds();
		M_StepStartSeconds = M_TestStartSeconds;
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST started map=%s"), *World->GetName());
	}
	if (World->GetTimeSeconds() - M_TestStartSeconds > BarbedWireTestPrivate::TestTimeoutSeconds)
	{
		Check(false, TEXT("test_timeout"), FString::Printf(TEXT("step=%s"), *UEnum::GetValueAsString(M_Step)));
		Finish();
		return;
	}
	KeepHQAnimating();
	// Edge scrolling reads the machine's cursor even offscreen and would move the camera off the captured spots.
	if (ACPPController* Controller = GetController(); IsValid(Controller) && not M_CaptureDirectory.IsEmpty() &&
		M_Step != EBarbedWireTestStep::WaitingForStart)
	{
		Controller->SetCameraMovementDisabled(true);
	}
	TickStep();
#endif
}

#if RTS_WITH_MAP_TESTS
void UBarbedWireLineTestSubsystem::TickStep()
{
	switch (M_Step)
	{
	case EBarbedWireTestStep::WaitingForStart: TickWaitingForStart(); return;
	case EBarbedWireTestStep::ExpandingHQ: TickExpandingHQ(); return;
	case EBarbedWireTestStep::OpeningBarriersTab: TickOpeningBarriersTab(); return;
	case EBarbedWireTestStep::PickingWire: TickPickingWire(); return;
	case EBarbedWireTestStep::Drawing: TickDrawing(); return;
	case EBarbedWireTestStep::Confirming: TickConfirming(); return;
	case EBarbedWireTestStep::WaitingForBuild: TickWaitingForBuild(); return;
	case EBarbedWireTestStep::CheckingNavBlocked: TickCheckingNavBlocked(); return;
	case EBarbedWireTestStep::CheckingDamagedPiece: TickCheckingDamagedPiece(); return;
	case EBarbedWireTestStep::CheckingNavOpen: TickCheckingNavOpen(); return;
	case EBarbedWireTestStep::StartingSnappedLine: TickStartingSnappedLine(); return;
	case EBarbedWireTestStep::ConfirmingSnappedLine: TickConfirmingSnappedLine(); return;
	case EBarbedWireTestStep::StartingPatch: TickStartingPatch(); return;
	case EBarbedWireTestStep::ConfirmingPatch: TickConfirmingPatch(); return;
	case EBarbedWireTestStep::WaitingForPatch: TickWaitingForPatch(); return;
	case EBarbedWireTestStep::SpawningVehicle: TickSpawningVehicle(); return;
	case EBarbedWireTestStep::DrivingOntoWire: TickDrivingOntoWire(); return;
	case EBarbedWireTestStep::CheckingCrush: TickCheckingCrush(); return;
	case EBarbedWireTestStep::SpawningScavengerWire: TickSpawningScavengerWire(); return;
	case EBarbedWireTestStep::DrivingEnemyOntoScavengerWire: TickDrivingEnemyOntoScavengerWire(); return;
	case EBarbedWireTestStep::CheckingScavengerWireCrush: TickCheckingScavengerWireCrush(); return;
	case EBarbedWireTestStep::StartingCancel: TickStartingCancel(); return;
	case EBarbedWireTestStep::CheckingCancel: TickCheckingCancel(); return;
	case EBarbedWireTestStep::Done:
	default:
		Finish();
		return;
	}
}

void UBarbedWireLineTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), *Description, *Details);
}

void UBarbedWireLineTestSubsystem::EnterStep(const EBarbedWireTestStep NewStep)
{
	M_Step = NewStep;
	M_StepStartSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}

float UBarbedWireLineTestSubsystem::GetStepSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_StepStartSeconds : 0.0f;
}

void UBarbedWireLineTestSubsystem::Finish()
{
	if (bM_IsFinished)
	{
		return;
	}
	bM_IsFinished = true;
	UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	// A screenshot is written a few frames after it is requested.
	bM_IsExitPending = true;
	M_ExitFramesLeft = BarbedWireTestPrivate::FramesToWriteLastCapture;
}

void UBarbedWireLineTestSubsystem::TickExitPending()
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
	FPlatformMisc::RequestExit(false, TEXT("BarbedWireValidate"));
}

void UBarbedWireLineTestSubsystem::ScheduleCapture(const FString& MomentName, const FVector& FocusLocation,
                                                   const float DelaySeconds)
{
	if (M_CaptureDirectory.IsEmpty() || not IsValid(GetWorld()))
	{
		return;
	}
	if (ACPPController* Controller = GetController())
	{
		// The camera is held still between captures; it zooms in only while unlocked.
		Controller->SetCameraMovementDisabled(false);
		if (UPlayerCameraController* CameraController = Controller->GetValidPlayerCameraController())
		{
			CameraController->SetCustomCameraZoomLevel(BarbedWireTestPrivate::CaptureZoomLevel);
		}
		Controller->FocusCameraOnLocation(FocusLocation);
	}
	M_PendingCaptures.Add(TPair<float, FString>(GetWorld()->GetTimeSeconds() + DelaySeconds, MomentName));
}

void UBarbedWireLineTestSubsystem::TakeDueCapture()
{
	if (M_PendingCaptures.IsEmpty() || not IsValid(GetWorld()))
	{
		return;
	}
	// One request per frame: the engine keeps only the last screenshot request of a frame.
	const float Now = GetWorld()->GetTimeSeconds();
	const int32 DueIndex = M_PendingCaptures.IndexOfByPredicate([Now](const TPair<float, FString>& PendingCapture)
	{
		return Now >= PendingCapture.Key;
	});
	if (DueIndex == INDEX_NONE)
	{
		return;
	}
	const FString CapturePath = FPaths::Combine(M_CaptureDirectory, M_PendingCaptures[DueIndex].Value + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
	UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST capture %s"), *CapturePath);
	M_PendingCaptures.RemoveAt(DueIndex);
}

void UBarbedWireLineTestSubsystem::CheckWeaponTraceHitsPiece(const FBarbedWirePieceRuntime& Piece)
{
	using namespace BarbedWireTestPrivate;
	const AActor* WireActor = Piece.WireActor.Get();
	const FVector Side = Piece.GroundTransform.GetRotation().GetRightVector() * WeaponTraceSideDistance;
	// Shots from beside the wire at every height up to its top, on the channel the AI's weapons trace on to hit what
	// the player owns; the coil's hull is thin, so only some heights meet it, as for the scavengers' wire.
	TArray<FString> HitHeights;
	for (float ProbeHeight = WeaponTraceStepHeight; ProbeHeight <= WeaponTraceTopHeight; ProbeHeight += WeaponTraceStepHeight)
	{
		const FVector ProbeCentre = Piece.GroundTransform.GetLocation() +
			Piece.GroundTransform.GetRotation().GetUpVector() * ProbeHeight;
		FHitResult WeaponHit;
		const bool bWeaponHit = GetWorld()->LineTraceSingleByChannel(
			WeaponHit, ProbeCentre + Side, ProbeCentre - Side, COLLISION_TRACE_AI_WEAPONS);
		if (bWeaponHit && IsValid(WireActor) && WeaponHit.GetActor() == WireActor)
		{
			HitHeights.Add(FString::Printf(TEXT("%.0f"), ProbeHeight));
		}
	}
	Check(not HitHeights.IsEmpty(), TEXT("hostile_shot_hits_the_wire_piece"),
	      FString::Printf(TEXT("hit_heights_cm=%s"), *FString::Join(HitHeights, TEXT(","))));
}

void UBarbedWireLineTestSubsystem::TickWaitingForStart()
{
	using namespace BarbedWireTestPrivate;
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
	if (GetStepSeconds() < StartDelaySeconds)
	{
		return;
	}
	if (not FindHQ())
	{
		if (GetStepSeconds() < StartDelaySeconds + HQWaitSeconds)
		{
			return;
		}
		Check(false, TEXT("hq_found"));
		Finish();
		return;
	}
	GivePlayerResources();
	ANomadicVehicle* HQ = M_HQ.Get();
	if (HQ->GetIsInTruckMode())
	{
		const ECommandQueueError Error = HQ->CreateBuildingAtLocation(
			HQ->GetActorLocation(), HQ->GetActorRotation(), true);
		Check(Error == ECommandQueueError::NoError, TEXT("hq_expand_ordered"));
	}
	EnterStep(EBarbedWireTestStep::ExpandingHQ);
}

void UBarbedWireLineTestSubsystem::TickExpandingHQ()
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
		// Headless runs never draw the truck, so its expansion montage may never finish on its own.
		if (GetStepSeconds() >= BarbedWireTestPrivate::ConversionFastForwardSeconds)
		{
			(void)HQ->ShippingTest_ForceFinishConversionToBuilding();
		}
		return;
	}
	M_Trainer = Trainer;
	UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST hq_expanded after=%.1fs"), GetStepSeconds());
	EnterStep(EBarbedWireTestStep::OpeningBarriersTab);
}

void UBarbedWireLineTestSubsystem::TickOpeningBarriersTab()
{
	using namespace BarbedWireTestPrivate;
	ACPPController* Controller = GetController();
	const UTrainerComponent* Trainer = M_Trainer.Get();
	if (not IsValid(Controller) || not IsValid(Trainer))
	{
		Check(false, TEXT("wire_setup"), TEXT("controller or trainer missing"));
		Finish();
		return;
	}
	const FTrainingSubSectionState* BarriersSection = Trainer->GetTrainingSubSections().FindByPredicate(
		[](const FTrainingSubSectionState& SubSection) { return SubSection.Options.Contains(WireOption); });
	Check(BarriersSection != nullptr, TEXT("wire_in_hq_barriers_tab"),
	      BarriersSection != nullptr ? BarriersSection->DisplayName.ToString() : FString(TEXT("no sub section has it")));
	ChooseClearTestYaw();
	Controller->SelectSingleUnitFromUI(M_HQ.Get());
	const UMainGameUI* MainGameUI = Controller->GetMainMenuUI();
	UTrainingMenuManager* MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	if (IsValid(MenuManager))
	{
		MenuManager->OnSubSectionTabClicked(BarriersTabIndex);
	}
	EnterStep(EBarbedWireTestStep::PickingWire);
}

void UBarbedWireLineTestSubsystem::TickPickingWire()
{
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	if (not IsValid(Placement))
	{
		Check(false, TEXT("wire_placement_component"));
		Finish();
		return;
	}
	MakeFirstPath();
	M_CursorDistance = 0.0f;
	// The mouse is on the start before the wire is picked, so the hover piece shows from the start.
	Placement->MapTest_SetCursorOverride(M_Path[0]);
	const bool bClicked = ClickWireMenuItem();
	Check(bClicked && Placement->GetIsPlacing(), TEXT("wire_menu_click_starts_placement"),
	      FString::Printf(TEXT("item_widget_found=%d placing=%d"), bClicked ? 1 : 0, Placement->GetIsPlacing() ? 1 : 0));
	if (not Placement->GetIsPlacing())
	{
		// Without the widget the rest is still tested through the controller.
		Controller->StartBarbedWirePlacement(M_Trainer.Get(), GetCostPerMetre());
	}
	Check(not Placement->GetIsDrawing() && Placement->MapTest_GetPlannedSpans().Num() == 1 &&
	      IsValid(Placement->MapTest_GetPreviewLine()), TEXT("wire_hover_shows_one_piece"),
	      FString::Printf(TEXT("spans=%d"), Placement->MapTest_GetPlannedSpans().Num()));
	Check(not Controller->MapTest_ClickBarbedWireAt(M_Path[0]) && Placement->GetIsDrawing(),
	      TEXT("wire_first_click_starts_drawing"), M_Path[0].ToCompactString());
	EnterStep(EBarbedWireTestStep::Drawing);
}

void UBarbedWireLineTestSubsystem::TickDrawing()
{
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	float PathLength = 0.0f;
	for (int32 PointIndex = 1; PointIndex < M_Path.Num(); ++PointIndex)
	{
		PathLength += FVector::Dist2D(M_Path[PointIndex - 1], M_Path[PointIndex]);
	}
	M_CursorDistance = FMath::Min(M_CursorDistance + BarbedWireTestPrivate::CursorStep, PathLength);
	float Walked = 0.0f;
	FVector Cursor = M_Path.Last();
	for (int32 PointIndex = 1; PointIndex < M_Path.Num(); ++PointIndex)
	{
		const float SegmentLength = FVector::Dist2D(M_Path[PointIndex - 1], M_Path[PointIndex]);
		if (Walked + SegmentLength >= M_CursorDistance)
		{
			Cursor = FMath::Lerp(M_Path[PointIndex - 1], M_Path[PointIndex], (M_CursorDistance - Walked) / SegmentLength);
			break;
		}
		Walked += SegmentLength;
	}
	if (IsValid(Placement))
	{
		Placement->MapTest_SetCursorOverride(Cursor);
	}
	if (M_CursorDistance >= PathLength)
	{
		EnterStep(EBarbedWireTestStep::Confirming);
	}
}

void UBarbedWireLineTestSubsystem::TickConfirming()
{
	using namespace BarbedWireTestPrivate;
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	const TArray<FBarbedWireSpan> Plan = Placement->MapTest_GetPlannedSpans();
	const float BendDegrees = Plan.Num() >= 2
		                          ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(
			                          (Plan[0].End - Plan[0].Start).GetSafeNormal(),
			                          (Plan.Last().End - Plan.Last().Start).GetSafeNormal()), -1.0f, 1.0f)))
		                          : 0.0f;
	Check(Plan.Num() >= MinimumFirstLinePieces && BendDegrees >= MinimumBendDegrees,
	      TEXT("wire_follows_the_cursor_round_the_bend"),
	      FString::Printf(TEXT("pieces=%d bend=%.1f"), Plan.Num(), BendDegrees));
	const float PieceLength = GetDefault<ARTSBarbedWireLine>()->GetPieceLength();
	M_ExpectedRadixite = Plan.Num() * ARTSBarbedWireLine::GetCostOfLength(GetCostPerMetre(), PieceLength)
		.FindRef(ERTSResourceType::Resource_Radixite);
	M_RadixiteBefore = GetRadixite();
	const bool bConfirmed = Controller->MapTest_ClickBarbedWireAt(M_Path.Last());
	M_FirstLine = Placement->MapTest_GetLastConfirmedLine();
	Check(bConfirmed && M_FirstLine.IsValid() && not Placement->GetIsPlacing(), TEXT("wire_second_click_confirms"));
	ScheduleCapture(TEXT("BarbedWire_1_Planned"), M_Path[1], 0.5f);
	Placement->MapTest_SetCursorOverride(TOptional<FVector>());
	if (not M_FirstLine.IsValid())
	{
		Finish();
		return;
	}
	EnterStep(EBarbedWireTestStep::WaitingForBuild);
}

void UBarbedWireLineTestSubsystem::TickWaitingForBuild()
{
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	if (not GetIsLineDone(Line))
	{
		if (IsValid(Line) && not bM_TookStringingCapture && Line->GetBuiltPieceCount() >= 2)
		{
			bM_TookStringingCapture = true;
			ScheduleCapture(TEXT("BarbedWire_2_Stringing"), M_Path[1], 0.5f);
		}
		return;
	}
	ScheduleCapture(TEXT("BarbedWire_3_Built"), M_Path[1], 0.5f);
	const int32 Spent = M_RadixiteBefore - GetRadixite();
	Check(Spent == M_ExpectedRadixite && Line->GetBuiltPieceCount() == Line->GetPieceCount(),
	      TEXT("wire_strung_piece_by_piece_and_paid_per_metre"),
	      FString::Printf(TEXT("pieces=%d built=%d spent=%d expected=%d after=%.1fs"), Line->GetPieceCount(),
	                      Line->GetBuiltPieceCount(), Spent, M_ExpectedRadixite, GetStepSeconds()));
	CheckBuiltPieces(*Line);
	SetNavProbeOnPiece(*Line, BarbedWireTestPrivate::GapPieceIndex);
	EnterStep(EBarbedWireTestStep::CheckingNavBlocked);
}

void UBarbedWireLineTestSubsystem::CheckBuiltPieces(const ARTSBarbedWireLine& Line)
{
	using namespace BarbedWireTestPrivate;
	int32 WireActors = 0;
	int32 BlockCharModifiers = 0;
	int32 CrushableMeshes = 0;
	for (const FBarbedWirePieceRuntime& Piece : Line.GetPieces())
	{
		const AFieldConstruction* WireActor = Piece.WireActor.Get();
		if (not IsValid(WireActor) || not WireActor->GetClass()->GetName().Contains(WireClassName))
		{
			continue;
		}
		++WireActors;
		const URTSNavCollisionEnvObstacle* NavModifier = WireActor->FindComponentByClass<URTSNavCollisionEnvObstacle>();
		// The area is protected on the engine's nav modifier; read it as the details panel does.
		const FClassProperty* AreaProperty = FindFProperty<FClassProperty>(
			UNavModifierComponent::StaticClass(), TEXT("AreaClass"));
		const UClass* AreaClass = IsValid(NavModifier) && AreaProperty != nullptr
			                          ? Cast<UClass>(AreaProperty->GetObjectPropertyValue_InContainer(NavModifier))
			                          : nullptr;
		BlockCharModifiers += IsValid(AreaClass) && AreaClass->GetName().Contains(BlockCharAreaName) ? 1 : 0;
		// The wire mesh's convex hull is what vehicles overlap: the Blueprint binds the crush to it.
		const UStaticMeshComponent* WireMesh = WireActor->FindComponentByClass<UStaticMeshComponent>();
		const UBodySetup* BodySetup = IsValid(WireMesh) && IsValid(WireMesh->GetStaticMesh())
			                              ? WireMesh->GetStaticMesh()->GetBodySetup()
			                              : nullptr;
		CrushableMeshes += IsValid(BodySetup) && BodySetup->AggGeom.GetElementCount() > 0 &&
		                   WireMesh->GetGenerateOverlapEvents() &&
		                   WireMesh->GetCollisionResponseToChannel(COLLISION_OBJ_PLAYER) == ECR_Overlap &&
		                   WireMesh->GetCollisionResponseToChannel(COLLISION_OBJ_ENEMY) == ECR_Overlap
			                   ? 1
			                   : 0;
	}
	const int32 PieceCount = Line.GetPieceCount();
	Check(WireActors == PieceCount, TEXT("every_piece_is_a_scavenger_wire_actor"),
	      FString::Printf(TEXT("wire_actors=%d pieces=%d"), WireActors, PieceCount));
	Check(BlockCharModifiers == PieceCount, TEXT("every_piece_blocks_infantry_navigation"),
	      FString::Printf(TEXT("block_char_modifiers=%d pieces=%d"), BlockCharModifiers, PieceCount));
	Check(CrushableMeshes == PieceCount, TEXT("every_piece_overlaps_vehicles"),
	      FString::Printf(TEXT("crushable_meshes=%d pieces=%d first_piece: %s"), CrushableMeshes, PieceCount,
	                      *DescribeCrushSetup(PieceCount > 0 ? Line.GetPieces()[0].WireActor.Get() : nullptr)));
}

void UBarbedWireLineTestSubsystem::TickCheckingNavBlocked()
{
	using namespace BarbedWireTestPrivate;
	if (GetStepSeconds() < NavSettleSeconds)
	{
		return;
	}
	M_BlockedPathLength = MeasureInfantryPathAcrossProbe();
	if (not GetIsInfantryPathBlocked(M_BlockedPathLength) && GetStepSeconds() < NavMaximumWaitSeconds)
	{
		return;
	}
	Check(GetIsInfantryPathBlocked(M_BlockedPathLength), TEXT("standing_wire_blocks_the_infantry_path"),
	      FString::Printf(TEXT("path_cm=%.0f straight_cm=%.0f after=%.1fs"), M_BlockedPathLength,
	                      NavProbeHalfWidth * 2.0f, GetStepSeconds()));
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	AActor* GapWire = IsValid(Line) && Line->GetPieces().IsValidIndex(GapPieceIndex)
		                  ? Line->GetPieces()[GapPieceIndex].WireActor.Get()
		                  : nullptr;
	Check(IsValid(GapWire), TEXT("gap_piece_wire_actor"));
	if (IsValid(GapWire))
	{
		const FBarbedWirePieceRuntime& GapPiece = Line->GetPieces()[GapPieceIndex];
		CheckWeaponTraceHitsPiece(GapPiece);
		// A shell bursting on the piece: radial damage, which reaches the piece through its mesh's hull.
		const FVector BlastOrigin = GapPiece.GroundTransform.GetLocation() + FVector(0.0f, 0.0f, BlastHeight);
		const bool bDamaged = UGameplayStatics::ApplyRadialDamage(
			GetWorld(), BlastDamage, BlastOrigin, BlastRadius, UDamageType::StaticClass(), TArray<AActor*>(), nullptr,
			nullptr, true, ECC_Visibility);
		Check(bDamaged, TEXT("shell_blast_reaches_the_piece"));
		ScheduleCapture(TEXT("BarbedWire_4_Gap"), BlastOrigin, 1.5f);
	}
	EnterStep(EBarbedWireTestStep::CheckingDamagedPiece);
}

void UBarbedWireLineTestSubsystem::TickCheckingDamagedPiece()
{
	using namespace BarbedWireTestPrivate;
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	if (not IsValid(Line))
	{
		Check(false, TEXT("first_line_survives_one_lost_piece"));
		Finish();
		return;
	}
	const TArray<FBarbedWirePieceRuntime>& Pieces = Line->GetPieces();
	auto GetIsStandingWire = [&Pieces](const int32 PieceIndex)
	{
		const AFieldConstruction* WireActor = Pieces.IsValidIndex(PieceIndex)
			                                      ? Pieces[PieceIndex].WireActor.Get()
			                                      : nullptr;
		return Pieces.IsValidIndex(PieceIndex) && Pieces[PieceIndex].State == EBarbedWirePieceState::Built &&
			IsValid(WireActor) && WireActor->GetCurrentHealth() > 0.0f;
	};
	const bool bGapDestroyed = Pieces[GapPieceIndex].State == EBarbedWirePieceState::Destroyed;
	Check(bGapDestroyed && GetIsStandingWire(GapPieceIndex - 1) && GetIsStandingWire(GapPieceIndex + 1),
	      TEXT("damage_destroys_only_the_hit_piece"));
	FVector OpenEnd;
	const bool bGapStartOpen = ARTSBarbedWireLine::FindOpenEndNear(
		GetWorld(), Pieces[GapPieceIndex - 1].Span.End, SnapTolerance * 10.0f, 1, nullptr, OpenEnd);
	const bool bGapEndOpen = ARTSBarbedWireLine::FindOpenEndNear(
		GetWorld(), Pieces[GapPieceIndex + 1].Span.Start, SnapTolerance * 10.0f, 1, nullptr, OpenEnd);
	Check(bGapStartOpen && bGapEndOpen, TEXT("gap_leaves_two_open_ends"));
	EnterStep(EBarbedWireTestStep::CheckingNavOpen);
}

void UBarbedWireLineTestSubsystem::TickCheckingNavOpen()
{
	using namespace BarbedWireTestPrivate;
	if (GetStepSeconds() < NavSettleSeconds)
	{
		return;
	}
	const float OpenPathLength = MeasureInfantryPathAcrossProbe();
	const float StraightLength = NavProbeHalfWidth * 2.0f;
	const bool bIsOpen = OpenPathLength > 0.0f && OpenPathLength < StraightLength * OpenPathFactor;
	if (not bIsOpen && GetStepSeconds() < NavMaximumWaitSeconds)
	{
		return;
	}
	if (not bIsOpen)
	{
		(void)MeasureInfantryPathAcrossProbe(true);
	}
	Check(bIsOpen, TEXT("infantry_walks_through_the_gap"),
	      FString::Printf(TEXT("path_cm=%.0f blocked_path_cm=%.0f straight_cm=%.0f after=%.1fs"), OpenPathLength,
	                      M_BlockedPathLength, StraightLength, GetStepSeconds()));
	EnterStep(EBarbedWireTestStep::StartingSnappedLine);
}

void UBarbedWireLineTestSubsystem::TickStartingSnappedLine()
{
	using namespace BarbedWireTestPrivate;
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	if (not IsValid(Line) || not IsValid(Placement))
	{
		Check(false, TEXT("snap_setup"));
		Finish();
		return;
	}
	const FVector2D LineEnd = Line->GetPieces().Last().Span.End;
	FVector Ground;
	const FVector Cursor = GetGroundAt(LineEnd + FVector2D(SnapCursorOffset), Ground) ? Ground : M_Path.Last();
	Placement->MapTest_SetCursorOverride(Cursor);
	Controller->StartBarbedWirePlacement(M_Trainer.Get(), GetCostPerMetre());
	Check(Placement->MapTest_GetIsStartSnapped(), TEXT("hover_snaps_to_the_open_end"));
	(void)Controller->MapTest_ClickBarbedWireAt(Cursor);
	const FVector2D Left = FVector2D(-GetHQRight());
	M_SnapCursor = GetGroundAt(LineEnd + Left * SnappedLineLength, Ground) ? Ground : FVector(LineEnd, Cursor.Z);
	Placement->MapTest_SetCursorOverride(M_SnapCursor);
	EnterStep(EBarbedWireTestStep::ConfirmingSnappedLine);
}

void UBarbedWireLineTestSubsystem::TickConfirmingSnappedLine()
{
	using namespace BarbedWireTestPrivate;
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	const bool bConfirmed = Controller->MapTest_ClickBarbedWireAt(M_SnapCursor);
	M_SnappedLine = Placement->MapTest_GetLastConfirmedLine();
	const ARTSBarbedWireLine* FirstLine = M_FirstLine.Get();
	const ARTSBarbedWireLine* SnappedLine = M_SnappedLine.Get();
	const bool bHasLines = IsValid(FirstLine) && IsValid(SnappedLine) && SnappedLine != FirstLine &&
		SnappedLine->GetPieceCount() > 0;
	const float JoinError = bHasLines
		                        ? FVector2D::Distance(SnappedLine->GetPieces()[0].Span.Start,
		                                              FirstLine->GetPieces().Last().Span.End)
		                        : -1.0f;
	Check(bConfirmed && bHasLines && JoinError >= 0.0f && JoinError <= SnapTolerance,
	      TEXT("new_line_starts_exactly_on_the_open_end"),
	      FString::Printf(TEXT("pieces=%d join_error_cm=%.2f"), bHasLines ? SnappedLine->GetPieceCount() : -1, JoinError));
	Placement->MapTest_SetCursorOverride(TOptional<FVector>());
	FVector UnusedEnd;
	Check(bHasLines && not ARTSBarbedWireLine::FindOpenEndNear(
		      GetWorld(), FirstLine->GetPieces().Last().Span.End, SnapTolerance * 10.0f, 1, nullptr, UnusedEnd),
	      TEXT("joined_end_is_no_longer_open"));
	EnterStep(EBarbedWireTestStep::StartingPatch);
}

void UBarbedWireLineTestSubsystem::TickStartingPatch()
{
	using namespace BarbedWireTestPrivate;
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	if (not IsValid(Line))
	{
		Check(false, TEXT("patch_setup"));
		Finish();
		return;
	}
	const FVector2D GapStart = Line->GetPieces()[GapPieceIndex - 1].Span.End;
	const FVector2D GapEnd = Line->GetPieces()[GapPieceIndex + 1].Span.Start;
	FVector Ground;
	const FVector StartCursor = GetGroundAt(GapStart + FVector2D(SnapCursorOffset) * 0.5f, Ground)
		                            ? Ground
		                            : FVector(GapStart, M_Path[0].Z);
	Placement->MapTest_SetCursorOverride(StartCursor);
	Controller->StartBarbedWirePlacement(M_Trainer.Get(), GetCostPerMetre());
	(void)Controller->MapTest_ClickBarbedWireAt(StartCursor);
	Check(Placement->GetIsDrawing() && Placement->MapTest_GetIsStartSnapped(), TEXT("patch_starts_on_the_gap"));
	M_SnapCursor = GetGroundAt(GapEnd - FVector2D(SnapCursorOffset) * 0.5f, Ground) ? Ground : FVector(GapEnd, 0.0f);
	Placement->MapTest_SetCursorOverride(M_SnapCursor);
	EnterStep(EBarbedWireTestStep::ConfirmingPatch);
}

void UBarbedWireLineTestSubsystem::TickConfirmingPatch()
{
	using namespace BarbedWireTestPrivate;
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	const FVector2D GapEnd = Line->GetPieces()[GapPieceIndex + 1].Span.Start;
	const TArray<FBarbedWireSpan> Plan = Placement->MapTest_GetPlannedSpans();
	const float CloseError = Plan.IsEmpty() ? -1.0f : FVector2D::Distance(Plan.Last().End, GapEnd);
	Check(Placement->MapTest_GetIsEndSnapped() && Plan.Num() == 1 && CloseError >= 0.0f && CloseError <= SnapTolerance,
	      TEXT("patch_end_snaps_onto_the_far_side_of_the_gap"),
	      FString::Printf(TEXT("pieces=%d close_error_cm=%.2f"), Plan.Num(), CloseError));
	const bool bConfirmed = Controller->MapTest_ClickBarbedWireAt(M_SnapCursor);
	M_PatchLine = Placement->MapTest_GetLastConfirmedLine();
	Check(bConfirmed && M_PatchLine.IsValid() && M_PatchLine != M_SnappedLine, TEXT("patch_confirmed"));
	Placement->MapTest_SetCursorOverride(TOptional<FVector>());
	EnterStep(EBarbedWireTestStep::WaitingForPatch);
}

void UBarbedWireLineTestSubsystem::TickWaitingForPatch()
{
	using namespace BarbedWireTestPrivate;
	if (not GetIsLineDone(M_PatchLine.Get()) || not GetIsLineDone(M_SnappedLine.Get()))
	{
		return;
	}
	if (M_PatchDoneSeconds < 0.0f)
	{
		M_PatchDoneSeconds = GetStepSeconds();
	}
	const float SecondsSincePatch = GetStepSeconds() - M_PatchDoneSeconds;
	if (SecondsSincePatch < NavSettleSeconds)
	{
		return;
	}
	const float PatchedPathLength = MeasureInfantryPathAcrossProbe();
	if (not GetIsInfantryPathBlocked(PatchedPathLength) && SecondsSincePatch < NavMaximumWaitSeconds)
	{
		return;
	}
	Check(GetIsInfantryPathBlocked(PatchedPathLength), TEXT("patched_gap_blocks_infantry_again"),
	      FString::Printf(TEXT("path_cm=%.0f after=%.1fs"), PatchedPathLength, SecondsSincePatch));
	const ARTSBarbedWireLine* Line = M_FirstLine.Get();
	FVector UnusedEnd;
	const bool bGapStillOpen = IsValid(Line) && ARTSBarbedWireLine::FindOpenEndNear(
		GetWorld(), Line->GetPieces()[GapPieceIndex - 1].Span.End, SnapTolerance * 10.0f, 1, nullptr, UnusedEnd);
	Check(not bGapStillOpen, TEXT("patch_closes_the_open_ends"));
	EnterStep(EBarbedWireTestStep::SpawningVehicle);
}

void UBarbedWireLineTestSubsystem::TickSpawningVehicle()
{
	using namespace BarbedWireTestPrivate;
	const ARTSBarbedWireLine* SnappedLine = M_SnappedLine.Get();
	UClass* VehicleClass = LoadClass<AActor>(nullptr, VehicleClassPath);
	if (not IsValid(SnappedLine) || not SnappedLine->GetPieces().IsValidIndex(CrushPieceIndex + 1) ||
		not IsValid(VehicleClass))
	{
		Check(false, TEXT("crush_setup"), FString::Printf(TEXT("vehicle_class=%d"), IsValid(VehicleClass) ? 1 : 0));
		EnterStep(EBarbedWireTestStep::StartingCancel);
		return;
	}
	const FVector PieceCentre = SnappedLine->GetPieces()[CrushPieceIndex].GroundTransform.GetLocation();
	FVector Ground;
	const FVector2D SpawnSpot = FVector2D(PieceCentre) - FVector2D(GetHQForward()) * VehicleSpawnSideDistance;
	const FVector SpawnLocation = GetGroundAt(SpawnSpot, Ground) ? Ground : PieceCentre;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Vehicle = GetWorld()->SpawnActor<AActor>(VehicleClass, SpawnLocation + FVector(0.0f, 0.0f, VehicleDropHeight),
	                                           GetHQForward().Rotation(), SpawnParameters);
	Check(M_Vehicle.IsValid(), TEXT("crush_vehicle_spawned"));
	EnterStep(EBarbedWireTestStep::DrivingOntoWire);
}

void UBarbedWireLineTestSubsystem::TickDrivingOntoWire()
{
	using namespace BarbedWireTestPrivate;
	if (GetStepSeconds() < VehicleSettleSeconds)
	{
		return;
	}
	AActor* Vehicle = M_Vehicle.Get();
	const ARTSBarbedWireLine* SnappedLine = M_SnappedLine.Get();
	if (not IsValid(Vehicle) || not IsValid(SnappedLine))
	{
		Check(false, TEXT("crush_vehicle_alive"));
		EnterStep(EBarbedWireTestStep::StartingCancel);
		return;
	}
	// Across the wire, as a tank drives over it; only the piece under its hull may go.
	const FVector PieceCentre = SnappedLine->GetPieces()[CrushPieceIndex].GroundTransform.GetLocation();
	Vehicle->SetActorLocationAndRotation(PieceCentre + FVector(0.0f, 0.0f, VehicleDropHeight),
	                                     GetHQForward().Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
	EnterStep(EBarbedWireTestStep::CheckingCrush);
}

void UBarbedWireLineTestSubsystem::TickCheckingCrush()
{
	using namespace BarbedWireTestPrivate;
	const ARTSBarbedWireLine* SnappedLine = M_SnappedLine.Get();
	const bool bIsCrushed = IsValid(SnappedLine) &&
		SnappedLine->GetPieces()[CrushPieceIndex].State == EBarbedWirePieceState::Destroyed;
	if (not bIsCrushed && GetStepSeconds() < CrushWaitSeconds)
	{
		return;
	}
	int32 StandingNeighbours = 0;
	if (IsValid(SnappedLine))
	{
		for (const int32 NeighbourIndex : {CrushPieceIndex - 1, CrushPieceIndex + 1})
		{
			StandingNeighbours += SnappedLine->GetPieces()[NeighbourIndex].State == EBarbedWirePieceState::Built ? 1 : 0;
		}
	}
	Check(bIsCrushed, TEXT("vehicle_crushes_the_piece_it_drives_over"),
	      FString::Printf(TEXT("after=%.1fs"), GetStepSeconds()));
	Check(StandingNeighbours == 2, TEXT("crush_leaves_the_neighbouring_pieces"),
	      FString::Printf(TEXT("standing_neighbours=%d"), StandingNeighbours));
	if (IsValid(SnappedLine))
	{
		ScheduleCapture(TEXT("BarbedWire_5_PatchedAndCrushed"),
		                SnappedLine->GetPieces()[0].GroundTransform.GetLocation(), 1.0f);
	}
	EnterStep(EBarbedWireTestStep::SpawningScavengerWire);
}

FString UBarbedWireLineTestSubsystem::DescribeCrushSetup(const AFieldConstruction* WireActor)
{
	if (not IsValid(WireActor))
	{
		return TEXT("no wire actor");
	}
	FString Description;
	TArray<UPrimitiveComponent*> Primitives;
	WireActor->GetComponents<UPrimitiveComponent>(Primitives);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Primitive);
		const UBodySetup* BodySetup = IsValid(MeshComponent) && IsValid(MeshComponent->GetStaticMesh())
			                              ? MeshComponent->GetStaticMesh()->GetBodySetup()
			                              : nullptr;
		if (IsValid(BodySetup))
		{
			const FKAggregateGeom& Geometry = BodySetup->AggGeom;
			Description += FString::Printf(
				TEXT("{boxes=%d spheres=%d capsules=%d convex=%d levelsets=%d aabb=%s} "),
				Geometry.BoxElems.Num(), Geometry.SphereElems.Num(), Geometry.SphylElems.Num(),
				Geometry.ConvexElems.Num(), Geometry.LevelSetElems.Num(),
				*Geometry.CalcAABB(FTransform::Identity).ToString());
		}
		Description += FString::Printf(
			TEXT("[%s %s enabled=%d obj=%d ai_weapon=%d player_obj=%d overlaps=%d shapes=%d flag=%d] "),
			*Primitive->GetName(), *Primitive->GetClass()->GetName(), static_cast<int32>(Primitive->GetCollisionEnabled()),
			static_cast<int32>(Primitive->GetCollisionObjectType()),
			static_cast<int32>(Primitive->GetCollisionResponseToChannel(COLLISION_TRACE_AI_WEAPONS)),
			static_cast<int32>(Primitive->GetCollisionResponseToChannel(COLLISION_OBJ_PLAYER)),
			Primitive->GetGenerateOverlapEvents() ? 1 : 0,
			IsValid(BodySetup) ? BodySetup->AggGeom.GetElementCount() : -1,
			IsValid(BodySetup) ? static_cast<int32>(BodySetup->GetCollisionTraceFlag()) : -1);
	}
	return Description;
}

FVector UBarbedWireLineTestSubsystem::GetScavengerWireSpot() const
{
	using namespace BarbedWireTestPrivate;
	const ANomadicVehicle* HQ = M_HQ.Get();
	const FVector2D Spot = FVector2D(IsValid(HQ) ? HQ->GetActorLocation() : FVector::ZeroVector) +
		FVector2D(GetHQForward()) * ScavengerWireForward - FVector2D(GetHQRight()) * ScavengerWireLeft;
	FVector Ground;
	return GetGroundAt(Spot, Ground) ? Ground : FVector(Spot, IsValid(HQ) ? HQ->GetActorLocation().Z : 0.0f);
}

void UBarbedWireLineTestSubsystem::TickSpawningScavengerWire()
{
	using namespace BarbedWireTestPrivate;
	UClass* WireClass = LoadClass<AFieldConstruction>(nullptr, ScavengerWireClassPath);
	UClass* EnemyClass = LoadClass<AActor>(nullptr, EnemyVehicleClassPath);
	if (not IsValid(WireClass) || not IsValid(EnemyClass))
	{
		Check(false, TEXT("scavenger_wire_setup"), FString::Printf(TEXT("wire_class=%d enemy_class=%d"),
		                                                           IsValid(WireClass) ? 1 : 0,
		                                                           IsValid(EnemyClass) ? 1 : 0));
		EnterStep(EBarbedWireTestStep::StartingCancel);
		return;
	}
	const FVector Spot = GetScavengerWireSpot();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// As the scavengers' field construction ability spawns it: the Blueprint on its own, no line around it.
	AFieldConstruction* ScavengerWire = GetWorld()->SpawnActor<AFieldConstruction>(
		WireClass, Spot, GetHQForward().Rotation(), SpawnParameters);
	M_ScavengerWire = ScavengerWire;
	Check(IsValid(ScavengerWire), TEXT("scavenger_wire_spawned"), DescribeCrushSetup(ScavengerWire));
	if (IsValid(ScavengerWire))
	{
		ScavengerWire->OnDestructableEnvActorDied.AddWeakLambda(this, [this]() { bM_ScavengerWireDied = true; });
	}
	const FVector EnemySpot = Spot - GetHQRight() * EnemySpawnDistance;
	M_EnemyVehicle = GetWorld()->SpawnActor<AActor>(EnemyClass, EnemySpot + FVector(0.0f, 0.0f, VehicleDropHeight),
	                                                GetHQRight().Rotation(), SpawnParameters);
	const int32 EnemyOwner = FRTSPlayers::GetOwningPlayerOfActor(M_EnemyVehicle.Get());
	Check(M_EnemyVehicle.IsValid() && EnemyOwner != 1, TEXT("enemy_vehicle_spawned"),
	      FString::Printf(TEXT("owner=%d"), EnemyOwner));
	EnterStep(EBarbedWireTestStep::DrivingEnemyOntoScavengerWire);
}

void UBarbedWireLineTestSubsystem::TickDrivingEnemyOntoScavengerWire()
{
	using namespace BarbedWireTestPrivate;
	if (GetStepSeconds() < VehicleSettleSeconds)
	{
		return;
	}
	AActor* EnemyVehicle = M_EnemyVehicle.Get();
	const AActor* ScavengerWire = M_ScavengerWire.Get();
	if (not IsValid(EnemyVehicle) || not IsValid(ScavengerWire))
	{
		Check(false, TEXT("scavenger_wire_and_enemy_alive"));
		EnterStep(EBarbedWireTestStep::StartingCancel);
		return;
	}
	// Across the wire, as the enemy drives over it.
	EnemyVehicle->SetActorLocationAndRotation(
		ScavengerWire->GetActorLocation() + FVector(0.0f, 0.0f, VehicleDropHeight), GetHQRight().Rotation(), false,
		nullptr, ETeleportType::TeleportPhysics);
	EnterStep(EBarbedWireTestStep::CheckingScavengerWireCrush);
}

void UBarbedWireLineTestSubsystem::TickCheckingScavengerWireCrush()
{
	using namespace BarbedWireTestPrivate;
	if (not bM_ScavengerWireDied && M_ScavengerWire.IsValid() && GetStepSeconds() < CrushWaitSeconds)
	{
		return;
	}
	Check(bM_ScavengerWireDied || not M_ScavengerWire.IsValid(), TEXT("enemy_vehicle_crushes_the_scavenger_wire"),
	      FString::Printf(TEXT("after=%.1fs"), GetStepSeconds()));
	ScheduleCapture(TEXT("BarbedWire_6_ScavengerWireCrushed"), GetScavengerWireSpot(), 1.0f);
	EnterStep(EBarbedWireTestStep::StartingCancel);
}

void UBarbedWireLineTestSubsystem::TickStartingCancel()
{
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(Placement) || not IsValid(HQ))
	{
		Check(false, TEXT("cancel_setup"));
		Finish();
		return;
	}
	using namespace BarbedWireTestPrivate;
	M_LineCountBeforeCancel = CountLines();
	// Beside where the first line starts, in front of the HQ, then straight into the HQ: the line must stop before it.
	FVector Start;
	const FVector2D StartSpot = FVector2D(HQ->GetActorLocation()) +
		FVector2D(GetHQForward()) * BlockedLineStartDistance + FVector2D(GetHQRight()) * BlockedLineSideOffset;
	if (not GetGroundAt(StartSpot, Start))
	{
		Start = FVector(StartSpot, HQ->GetActorLocation().Z);
	}
	FVector IntoHQ;
	const FVector2D IntoHQSpot = FVector2D(HQ->GetActorLocation()) + FVector2D(GetHQRight()) * BlockedLineSideOffset;
	if (not GetGroundAt(IntoHQSpot, IntoHQ))
	{
		IntoHQ = FVector(IntoHQSpot, HQ->GetActorLocation().Z);
	}
	Placement->MapTest_SetCursorOverride(Start);
	Controller->StartBarbedWirePlacement(M_Trainer.Get(), GetCostPerMetre());
	(void)Controller->MapTest_ClickBarbedWireAt(Start);
	Placement->MapTest_SetCursorOverride(IntoHQ);
	EnterStep(EBarbedWireTestStep::CheckingCancel);
}

void UBarbedWireLineTestSubsystem::TickCheckingCancel()
{
	ACPPController* Controller = GetController();
	UBarbedWirePlacementComponent* Placement = GetPlacement();
	if (not bM_HasStoppedCancelLine)
	{
		bM_HasStoppedCancelLine = true;
		Check(CountLines() == M_LineCountBeforeCancel + 1, TEXT("wire_preview_while_drawing"));
		CheckLineStopsBeforeHQ(*Placement);
		Controller->StopBarbedWirePlacement();
		Placement->MapTest_SetCursorOverride(TOptional<FVector>());
		return;
	}
	Check(CountLines() == M_LineCountBeforeCancel && not Placement->GetIsPlacing(),
	      TEXT("cancel_removes_only_the_unconfirmed_line"),
	      FString::Printf(TEXT("lines=%d expected=%d"), CountLines(), M_LineCountBeforeCancel));
	EnterStep(EBarbedWireTestStep::Done);
}

bool UBarbedWireLineTestSubsystem::GetIsInfantryPathBlocked(const float PathLength)
{
	using namespace BarbedWireTestPrivate;
	// No full path at all, or one round the end of the line, much longer than straight across.
	return PathLength < 0.0f || PathLength > NavProbeHalfWidth * 2.0f * BlockedDetourFactor;
}

void UBarbedWireLineTestSubsystem::CheckLineStopsBeforeHQ(const UBarbedWirePlacementComponent& Placement)
{
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(HQ))
	{
		Check(false, TEXT("line_stops_before_the_hq"), TEXT("no HQ"));
		return;
	}
	// The parts of the HQ buildings may not be placed on (its building mesh), on the ground plan.
	FBox HQBounds(ForceInit);
	TArray<UPrimitiveComponent*> HQParts;
	HQ->GetComponents<UPrimitiveComponent>(HQParts);
	for (const UPrimitiveComponent* Part : HQParts)
	{
		if (IsValid(Part) && Part->GetCollisionEnabled() != ECollisionEnabled::NoCollision &&
			Part->GetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT) != ECR_Ignore)
		{
			HQBounds += Part->Bounds.GetBox();
		}
	}
	const TArray<FBarbedWireSpan>& Plan = Placement.MapTest_GetPlannedSpans();
	int32 PiecesInHQ = 0;
	for (const FBarbedWireSpan& Span : Plan)
	{
		PiecesInHQ += HQBounds.IsInsideXY(FVector(Span.GetCentre(), HQBounds.GetCenter().Z)) ? 1 : 0;
	}
	Check(HQBounds.IsValid && not Plan.IsEmpty() && Plan.Num() < BarbedWireTestPrivate::BlockedLinePieces &&
	      PiecesInHQ == 0, TEXT("line_stops_before_the_hq"),
	      FString::Printf(TEXT("pieces=%d uncut=%d pieces_in_hq=%d hq_building_bounds=%s"), Plan.Num(),
	                      BarbedWireTestPrivate::BlockedLinePieces, PiecesInHQ, *HQBounds.ToString()));
	// What answers the building placement channel: the HQ's parts, and what each planned piece's spot meets.
	TArray<UPrimitiveComponent*> HQPrimitives;
	HQ->GetComponents<UPrimitiveComponent>(HQPrimitives);
	for (const UPrimitiveComponent* Primitive : HQPrimitives)
	{
		if (not IsValid(Primitive) || Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST hq_part %s (%s) placement=%d bounds=%s"),
		       *Primitive->GetName(), *Primitive->GetClass()->GetName(),
		       static_cast<int32>(Primitive->GetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT)),
		       *Primitive->Bounds.GetBox().ToString());
	}
	const ARTSBarbedWireLine* PreviewLine = Placement.MapTest_GetPreviewLine();
	if (not IsValid(PreviewLine))
	{
		return;
	}
	for (const FBarbedWirePieceRuntime& Piece : PreviewLine->GetPieces())
	{
		TArray<FOverlapResult> Overlaps;
		GetWorld()->OverlapMultiByChannel(Overlaps, Piece.GroundTransform.GetLocation() + FVector(0.0f, 0.0f, 100.0f),
		                                  Piece.GroundTransform.GetRotation(), COLLISION_OBJ_BUILDING_PLACEMENT,
		                                  FCollisionShape::MakeBox(FVector(200.0f, 45.0f, 75.0f)));
		TArray<FString> Names;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			Names.Add(FString::Printf(TEXT("%s.%s"), IsValid(Overlap.GetActor()) ? *Overlap.GetActor()->GetName() : TEXT("-"),
			                          IsValid(Overlap.GetComponent()) ? *Overlap.GetComponent()->GetName() : TEXT("-")));
		}
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST hq_piece at=%s state=%d obstacle=%d meets=[%s]"),
		       *Piece.GroundTransform.GetLocation().ToCompactString(), static_cast<int32>(Piece.State),
		       PreviewLine->GetIsPieceOnObstacle(Piece) ? 1 : 0, *FString::Join(Names, TEXT(", ")));
	}
}

ACPPController* UBarbedWireLineTestSubsystem::GetController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

UBarbedWirePlacementComponent* UBarbedWireLineTestSubsystem::GetPlacement() const
{
	const ACPPController* Controller = GetController();
	return IsValid(Controller) ? Controller->MapTest_GetBarbedWirePlacement() : nullptr;
}

bool UBarbedWireLineTestSubsystem::FindHQ()
{
	for (TActorIterator<ANomadicVehicle> NomadicIterator(GetWorld()); NomadicIterator; ++NomadicIterator)
	{
		ANomadicVehicle* Nomadic = *NomadicIterator;
		const URTSComponent* RTSComponent = IsValid(Nomadic) ? Nomadic->GetRTSComponent() : nullptr;
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == 1 &&
			RTSComponent->GetSubtypeAsNomadicSubtype() == ENomadicSubtype::Nomadic_GerHq)
		{
			M_HQ = Nomadic;
			return true;
		}
	}
	return false;
}

void UBarbedWireLineTestSubsystem::GivePlayerResources() const
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
		ResourceManager->CheatAddStorage(Resource, BarbedWireTestPrivate::TestResourceAmount);
	}
}

void UBarbedWireLineTestSubsystem::KeepHQAnimating() const
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

TMap<ERTSResourceType, int32> UBarbedWireLineTestSubsystem::GetCostPerMetre() const
{
	return {{
		ERTSResourceType::Resource_Radixite,
		DeveloperSettings::GameBalance::UnitCosts::Bxps::GerBarbedWireLine_RadixitePerMetre
	}};
}

int32 UBarbedWireLineTestSubsystem::GetRadixite() const
{
	const ACPPController* Controller = GetController();
	const UPlayerResourceManager* ResourceManager = IsValid(Controller)
		                                                ? Controller->GetPlayerResourceManager()
		                                                : nullptr;
	return IsValid(ResourceManager) ? ResourceManager->GetResourceAmount(ERTSResourceType::Resource_Radixite) : 0;
}

bool UBarbedWireLineTestSubsystem::GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const
{
	const UWorld* World = GetWorld();
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(World) || not IsValid(HQ))
	{
		return false;
	}
	const float Height = HQ->GetActorLocation().Z;
	FHitResult GroundHit;
	const FVector TraceStart(Location2D, Height + BarbedWireTestPrivate::GroundTraceHalfHeight);
	const FVector TraceEnd(Location2D, Height - BarbedWireTestPrivate::GroundTraceHalfHeight);
	if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE))
	{
		return false;
	}
	OutGround = GroundHit.ImpactPoint;
	return true;
}

FVector UBarbedWireLineTestSubsystem::GetHQForward() const
{
	return FRotator(0.0f, M_TestYaw, 0.0f).Vector();
}

FVector UBarbedWireLineTestSubsystem::GetHQRight() const
{
	return FRotator(0.0f, M_TestYaw + 90.0f, 0.0f).Vector();
}

void UBarbedWireLineTestSubsystem::ChooseClearTestYaw()
{
	using namespace BarbedWireTestPrivate;
	const ANomadicVehicle* HQ = M_HQ.Get();
	if (not IsValid(HQ))
	{
		return;
	}
	const float HQYaw = HQ->GetActorRotation().Yaw;
	M_TestYaw = HQYaw;
	for (const float YawOffset : TestYawOffsets)
	{
		const float CandidateYaw = HQYaw + YawOffset;
		const FVector Forward = FRotator(0.0f, CandidateYaw, 0.0f).Vector();
		const FVector Right = FRotator(0.0f, CandidateYaw + 90.0f, 0.0f).Vector();
		const FVector AreaCentre = HQ->GetActorLocation() + Forward * ((TestAreaNear + TestAreaFar) * 0.5f) +
			Right * TestAreaSideCentre + FVector(0.0f, 0.0f, TestAreaHalfHeight);
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BarbedWireTestArea), false, HQ);
		GetWorld()->OverlapMultiByChannel(
			Overlaps, AreaCentre, FRotator(0.0f, CandidateYaw, 0.0f).Quaternion(), COLLISION_OBJ_BUILDING_PLACEMENT,
			FCollisionShape::MakeBox(FVector((TestAreaFar - TestAreaNear) * 0.5f, TestAreaHalfWidth,
			                                 TestAreaHalfHeight)), QueryParams);
		int32 Obstacles = Overlaps.FilterByPredicate([](const FOverlapResult& Overlap)
		{
			return IsValid(Overlap.GetActor()) && not Overlap.GetActor()->IsA<ACharacter>();
		}).Num();
		// Trucks still on their wheels expand into buildings where they stand, later in a rendered run.
		const FTransform AreaFrame(FRotator(0.0f, CandidateYaw, 0.0f), AreaCentre);
		for (TActorIterator<ANomadicVehicle> NomadicIterator(GetWorld()); NomadicIterator; ++NomadicIterator)
		{
			const ANomadicVehicle* Nomadic = *NomadicIterator;
			if (not IsValid(Nomadic) || Nomadic == HQ)
			{
				continue;
			}
			const FVector Local = AreaFrame.InverseTransformPosition(Nomadic->GetActorLocation());
			const bool bIsInArea = FMath::Abs(Local.X) <= (TestAreaFar - TestAreaNear) * 0.5f + NomadicBuildingRadius &&
				FMath::Abs(Local.Y) <= TestAreaHalfWidth + NomadicBuildingRadius;
			Obstacles += bIsInArea ? 1 : 0;
		}
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST test_area yaw_offset=%.0f obstacles=%d"),
		       YawOffset, Obstacles);
		if (Obstacles == 0)
		{
			M_TestYaw = CandidateYaw;
			return;
		}
	}
	UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST test_area none clear; using the HQ's forward"));
}

void UBarbedWireLineTestSubsystem::MakeFirstPath()
{
	using namespace BarbedWireTestPrivate;
	const ANomadicVehicle* HQ = M_HQ.Get();
	const FVector2D Forward(GetHQForward());
	const FVector2D Turned(FRotator(0.0f, M_TestYaw + TurnDegrees, 0.0f).Vector());
	const FVector2D Start = FVector2D(HQ->GetActorLocation()) + Forward * StartDistanceFromHQ;
	const FVector2D Corner = Start + Forward * FirstLegLength;
	M_Path.Reset();
	for (const FVector2D& Point : {Start, Corner, Corner + Turned * SecondLegLength})
	{
		FVector Ground;
		M_Path.Add(GetGroundAt(Point, Ground) ? Ground : FVector(Point, HQ->GetActorLocation().Z));
	}
}

bool UBarbedWireLineTestSubsystem::ClickWireMenuItem() const
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UW_TrainingItem::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		UW_TrainingItem* TrainingItem = Cast<UW_TrainingItem>(Widget);
		if (not IsValid(TrainingItem) || not (TrainingItem->GetTrainingItemState().ItemID ==
			BarbedWireTestPrivate::WireOption))
		{
			continue;
		}
		UFunction* ClickFunction = TrainingItem->FindFunction(TEXT("OnClickedTrainingItem"));
		if (ClickFunction == nullptr)
		{
			return false;
		}
		// What the item's button calls when the player clicks it.
		struct FClickParameters
		{
			bool bIsLeftClick;
		};
		FClickParameters Parameters{true};
		TrainingItem->ProcessEvent(ClickFunction, &Parameters);
		return true;
	}
	return false;
}

int32 UBarbedWireLineTestSubsystem::CountLines() const
{
	int32 LineCount = 0;
	for (TActorIterator<ARTSBarbedWireLine> LineIterator(GetWorld()); LineIterator; ++LineIterator)
	{
		LineCount += IsValid(*LineIterator) ? 1 : 0;
	}
	return LineCount;
}

bool UBarbedWireLineTestSubsystem::GetIsLineDone(const ARTSBarbedWireLine* Line)
{
	if (not IsValid(Line) || Line->GetPieceCount() == 0)
	{
		return false;
	}
	return not Line->GetPieces().ContainsByPredicate([](const FBarbedWirePieceRuntime& Piece)
	{
		return Piece.State == EBarbedWirePieceState::Planned || Piece.State == EBarbedWirePieceState::Building;
	});
}

float UBarbedWireLineTestSubsystem::MeasureInfantryPathAcrossProbe(const bool bLogPathPoints) const
{
	using namespace BarbedWireTestPrivate;
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (not IsValid(NavigationSystem))
	{
		return -1.0f;
	}
	ANavigationData* CharacterNavData = nullptr;
	const FString AgentName = Global_GetRTSNavAgentName(ERTSNavAgents::Character);
	for (ANavigationData* NavData : NavigationSystem->NavDataSet)
	{
		if (IsValid(NavData) && NavData->GetConfig().Name.ToString() == AgentName)
		{
			CharacterNavData = NavData;
		}
	}
	FNavLocation ProjectedStart;
	FNavLocation ProjectedEnd;
	const FVector Extent(NavProjectionExtent);
	if (not IsValid(CharacterNavData) ||
		not NavigationSystem->ProjectPointToNavigation(M_NavProbeCentre + M_NavProbeSide * NavProbeHalfWidth,
		                                               ProjectedStart, Extent, CharacterNavData) ||
		not NavigationSystem->ProjectPointToNavigation(M_NavProbeCentre - M_NavProbeSide * NavProbeHalfWidth,
		                                               ProjectedEnd, Extent, CharacterNavData))
	{
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST nav_probe no navdata or projection"));
		return -1.0f;
	}
	// Infantry plans with the RTS character filter, which excludes the wire's BlockChar area; unfiltered queries
	// (convoy routes, planners) treat it as plain ground on purpose.
	const URTSNavQueryFilterSettings* FilterSettings = URTSNavQueryFilterSettings::Get();
	const TSubclassOf<UNavigationQueryFilter> CharacterFilterClass = IsValid(FilterSettings)
		                                                                 ? FilterSettings->ResolveFilterClassForAgent(
			                                                                 ERTSNavAgents::Character)
		                                                                 : nullptr;
	if (not CharacterFilterClass)
	{
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST nav_probe no character query filter"));
		return -1.0f;
	}
	FPathFindingQuery Query(nullptr, *CharacterNavData, ProjectedStart.Location, ProjectedEnd.Location,
	                        UNavigationQueryFilter::GetQueryFilter(*CharacterNavData, nullptr, CharacterFilterClass));
	const FPathFindingResult Result = NavigationSystem->FindPathSync(Query);
	if (not Result.IsSuccessful() || not Result.Path.IsValid() || Result.IsPartial())
	{
		return -1.0f;
	}
	if (bLogPathPoints)
	{
		TArray<FString> Points;
		for (const FNavPathPoint& PathPoint : Result.Path->GetPathPoints())
		{
			Points.Add(PathPoint.Location.ToCompactString());
		}
		UE_LOG(LogRTSBarbedWireTest, Display, TEXT("RTS_BARBED_WIRE_TEST nav_path probe=%s side=%s points=[%s]"),
		       *M_NavProbeCentre.ToCompactString(), *M_NavProbeSide.ToCompactString(),
		       *FString::Join(Points, TEXT(" -> ")));
	}
	return Result.Path->GetLength();
}

void UBarbedWireLineTestSubsystem::SetNavProbeOnPiece(const ARTSBarbedWireLine& Line, const int32 PieceIndex)
{
	if (not Line.GetPieces().IsValidIndex(PieceIndex))
	{
		return;
	}
	const FBarbedWirePieceRuntime& Piece = Line.GetPieces()[PieceIndex];
	M_NavProbeCentre = Piece.GroundTransform.GetLocation();
	const FVector2D Along = (Piece.Span.End - Piece.Span.Start).GetSafeNormal();
	M_NavProbeSide = FVector(-Along.Y, Along.X, 0.0f);
}
#endif
