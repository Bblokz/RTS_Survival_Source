#include "SquadMovePreviewTestScenario.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverTestScenario.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "UnrealClient.h"
#include "Engine/StaticMesh.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePreviewComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSquadPreviewTest, Log, All);

namespace SquadMovePreviewTestPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr float MaximumScanWaitSeconds = 240.0f;
	// Lets the squads finish walking into their idle cover before plans are compared against the map.
	constexpr float SettleSecondsAfterScan = 12.0f;
	constexpr float PlannedMoveSeconds = 35.0f;
	constexpr float MinimumPlannedSpacing = 74.0f;
	constexpr float RegularArrivalTolerance = 140.0f;
	constexpr float OpenGroundClearance = 1100.0f;
	constexpr float OpenGroundSearchStep = 700.0f;
	constexpr int32 OpenGroundSearchRings = 6;
	constexpr int32 PlanningCostIterations = 200;
	constexpr double MaximumAveragePlanningMilliseconds = 1.0;

	int32 CountPositionsOfType(const FSquadMovePlan& Plan, const ESquadPlannedPositionType Type)
	{
		int32 Count = 0;
		for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
		{
			Count += Position.Type == Type ? 1 : 0;
		}
		return Count;
	}

	bool GetPlanUsesCoverPoint(const FSquadMovePlan& Plan, const int64 PointId)
	{
		for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
		{
			if (Position.GetIsCover() && Position.CoverPoint.PointId == PointId)
			{
				return true;
			}
		}
		return false;
	}

	float FindSmallestSpacing(const FSquadMovePlan& Plan)
	{
		float SmallestSpacing = TNumericLimits<float>::Max();
		for (int32 FirstIndex = 0; FirstIndex < Plan.UnitPositions.Num(); ++FirstIndex)
		{
			for (int32 SecondIndex = FirstIndex + 1; SecondIndex < Plan.UnitPositions.Num(); ++SecondIndex)
			{
				SmallestSpacing = FMath::Min(SmallestSpacing, FVector::Dist2D(
					Plan.UnitPositions[FirstIndex].Location,
					Plan.UnitPositions[SecondIndex].Location));
			}
		}
		return SmallestSpacing;
	}

	bool GetHasDuplicateCoverPoints(const FSquadMovePlan& Plan)
	{
		TSet<int64> SeenPointIds;
		for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
		{
			bool bAlreadySeen = false;
			if (Position.GetIsCover())
			{
				SeenPointIds.Add(Position.CoverPoint.PointId, &bAlreadySeen);
			}
			if (bAlreadySeen)
			{
				return true;
			}
		}
		return false;
	}

	TArray<ASquadController*> GatherPlayerSquads(UWorld& World)
	{
		TArray<ASquadController*> PlayerSquads;
		for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator; ++SquadIterator)
		{
			ASquadController* SquadController = *SquadIterator;
			const URTSComponent* SquadRTSComponent = IsValid(SquadController)
				? SquadController->GetRTSComponent()
				: nullptr;
			if (IsValid(SquadRTSComponent) && SquadRTSComponent->GetOwningPlayer() == PlayerOwnedTeam)
			{
				PlayerSquads.Add(SquadController);
			}
		}
		return PlayerSquads;
	}

	/** Nearest published point of the wanted kind that no soldier outside the test squad holds. */
	constexpr float WatchHeight = 2600.0f;
	constexpr float WatchBackOffset = 900.0f;
	constexpr int32 StanceCaptureFramesPerLocation = 40;
	constexpr float StanceCaptureCameraHeight = 750.0f;
	constexpr float StanceCaptureCameraBackOffset = 900.0f;

	void WatchLocationFromAbove(UWorld& World, const FVector& Location, const float Height, const float BackOffset)
	{
		ACameraActor* Camera = World.SpawnActor<ACameraActor>();
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
		if (not IsValid(Camera) || not IsValid(PlayerController))
		{
			return;
		}
		if (UCameraComponent* CameraComponent = Camera->GetCameraComponent())
		{
			CameraComponent->bConstrainAspectRatio = false;
		}
		const FVector CameraLocation = Location + FVector(-BackOffset, 0.0f, Height);
		Camera->SetActorLocationAndRotation(CameraLocation, (Location - CameraLocation).Rotation());
		PlayerController->SetViewTarget(Camera);
	}

	bool FindNearestFreeCoverPoint(
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const ASquadController& TestSquad,
		const bool bWantCrouchCover,
		FRTSCoverPoint& OutCoverPoint)
	{
		const FVector SquadLocation = TestSquad.GetActorLocation();
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
		{
			const bool bIsCrouchCover = CoverPoint.CoverType == ERTSCoverType::Crouch;
			const bool bIsStandingCover = CoverPoint.CoverType == ERTSCoverType::StandingLeft ||
				CoverPoint.CoverType == ERTSCoverType::StandingRight;
			const ASquadUnit* ReservingUnit = CoverSubsystem.GetCoverReservationOwner(CoverPoint.PointId);
			const bool bHeldByOtherSquad = IsValid(ReservingUnit) &&
				ReservingUnit->GetSquadControllerChecked() != &TestSquad;
			const float DistanceSquared = FVector::DistSquared2D(SquadLocation, CoverPoint.Location);
			// Prone cover and trench steps are neither of the two kinds this test plans onto.
			const bool bIsWantedKind = bWantCrouchCover ? bIsCrouchCover : bIsStandingCover;
			if (not bIsWantedKind || bHeldByOtherSquad || DistanceSquared >= NearestDistanceSquared)
			{
				continue;
			}
			NearestDistanceSquared = DistanceSquared;
			OutCoverPoint = CoverPoint;
		}
		return NearestDistanceSquared < TNumericLimits<float>::Max();
	}

	/** Walks outward from the squad until it finds navigable ground with no cover anywhere near it. */
	bool FindOpenGroundLocation(
		UWorld& World,
		const URTSCoverFinderWorldSubsystem& CoverSubsystem,
		const FVector& SearchOrigin,
		FVector& OutLocation)
	{
		const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(&World);
		if (not IsValid(NavigationSystem))
		{
			return false;
		}
		constexpr int32 DirectionsPerRing = 12;
		for (int32 Ring = 1; Ring <= OpenGroundSearchRings; ++Ring)
		{
			for (int32 DirectionIndex = 0; DirectionIndex < DirectionsPerRing; ++DirectionIndex)
			{
				const float AngleRadians = UE_TWO_PI * static_cast<float>(DirectionIndex) /
					static_cast<float>(DirectionsPerRing);
				const FVector Candidate = SearchOrigin + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f)
					* (static_cast<float>(Ring) * OpenGroundSearchStep);
				FNavLocation ProjectedCandidate;
				if (not NavigationSystem->ProjectPointToNavigation(
					Candidate,
					ProjectedCandidate,
					FVector(100.0f, 100.0f, 500.0f)))
				{
					continue;
				}
				if (CoverSubsystem.FindCoverPointsInRadius(ProjectedCandidate.Location, OpenGroundClearance).IsEmpty())
				{
					OutLocation = ProjectedCandidate.Location;
					return true;
				}
			}
		}
		return false;
	}
}

void FSquadMovePreviewTestScenario::Start()
{
	M_Expectations.Reset();
	M_FailedCheckCount = 0;
	EnterPhase(ESquadMovePreviewTestPhase::WaitingForCoverScan);
	UE_LOG(LogRTSSquadPreviewTest, Display, TEXT("RTS_SQUAD_PREVIEW_TEST scheduled"));
}

void FSquadMovePreviewTestScenario::Tick(USquadMovePreviewComponent& PreviewComponent, const float DeltaTime)
{
	UWorld* World = PreviewComponent.GetWorld();
	if (M_Phase == ESquadMovePreviewTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	ResumeWorldIfPaused(*World);
	TickStanceCapture(PreviewComponent, *World);
	FCoverTestScenario::GiveSoldiersCommandLineApproachMoves(*World);
	M_PhaseElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	const URTSCoverFinderWorldSubsystem* CoverSubsystem = World->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	if (not IsValid(CoverSubsystem))
	{
		return;
	}

	if (M_Phase == ESquadMovePreviewTestPhase::WaitingForPlannedMove)
	{
		if (M_PhaseElapsedSeconds >= SquadMovePreviewTestPrivate::PlannedMoveSeconds)
		{
			FinishPlannedMoveCheck(*World);
		}
		return;
	}

	if (M_Phase == ESquadMovePreviewTestPhase::SettlingAfterCoverScan)
	{
		if (M_PhaseElapsedSeconds >= SquadMovePreviewTestPrivate::SettleSecondsAfterScan)
		{
			RunPlanChecksAndIssueMove(PreviewComponent, *World, *CoverSubsystem);
		}
		return;
	}

	if (CoverSubsystem->GetHasCompletedFullScan())
	{
		EnterPhase(ESquadMovePreviewTestPhase::SettlingAfterCoverScan);
		return;
	}
	if (M_PhaseElapsedSeconds >= SquadMovePreviewTestPrivate::MaximumScanWaitSeconds)
	{
		Check(false, TEXT("cover scan completed"));
		FinishPlannedMoveCheck(*World);
	}
}

void FSquadMovePreviewTestScenario::ResumeWorldIfPaused(UWorld& World) const
{
	// Same start path the cover scenario uses: the map boots paused behind the start-game widget.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (UGameplayStatics::IsGamePaused(&World))
	{
		UGameplayStatics::SetGamePaused(&World, false);
	}
}

void FSquadMovePreviewTestScenario::Check(const bool bPassed, const TCHAR* CheckName, const FString& Details)
{
	M_FailedCheckCount += bPassed ? 0 : 1;
	UE_LOG(
		LogRTSSquadPreviewTest,
		Display,
		TEXT("RTS_SQUAD_PREVIEW_TEST check=%s %s %s"),
		bPassed ? TEXT("PASS") : TEXT("FAIL"),
		CheckName,
		*Details);
}

void FSquadMovePreviewTestScenario::RunPlanChecksAndIssueMove(
	USquadMovePreviewComponent& PreviewComponent,
	UWorld& World,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	const TArray<ASquadController*> PlayerSquads = SquadMovePreviewTestPrivate::GatherPlayerSquads(World);
	Check(not PlayerSquads.IsEmpty(), TEXT("player squads found"), FString::FromInt(PlayerSquads.Num()));
	Check(
		USquadMovePreviewComponent::GetCanPlanForSquads(PlayerSquads),
		TEXT("player squads can be planned"));
	if (PlayerSquads.IsEmpty())
	{
		FinishPlannedMoveCheck(World);
		return;
	}
	const TArray<ASquadController*> TestSquad = {PlayerSquads[0]};
	FRTSCoverPoint CrouchPoint;
	FRTSCoverPoint StandingPoint;
	const bool bFoundCrouchPoint = SquadMovePreviewTestPrivate::FindNearestFreeCoverPoint(
		CoverSubsystem,
		*TestSquad[0],
		true,
		CrouchPoint);
	const bool bFoundStandingPoint = SquadMovePreviewTestPrivate::FindNearestFreeCoverPoint(
		CoverSubsystem,
		*TestSquad[0],
		false,
		StandingPoint);
	Check(bFoundCrouchPoint, TEXT("map has free crouch cover"));
	Check(bFoundStandingPoint, TEXT("map has free standing cover"));
	if (not bFoundCrouchPoint || not bFoundStandingPoint)
	{
		FinishPlannedMoveCheck(World);
		return;
	}

	CheckCoverPlans(PreviewComponent, TestSquad, CrouchPoint, StandingPoint);
	CheckPronePlan(PreviewComponent, CoverSubsystem, TestSquad);
	CheckStanceMeshes(PreviewComponent, TestSquad, CrouchPoint.Location);
	CheckStanceMeshes(PreviewComponent, TestSquad, StandingPoint.Location);
	if (FParse::Param(FCommandLine::Get(), TEXT("SquadMovePreviewCaptureStances")))
	{
		M_StanceCaptureSquads.Reset();
		M_StanceCaptureSquads.Append(TestSquad);
		M_StanceCaptureLocations[0] = CrouchPoint.Location;
		M_StanceCaptureLocations[1] = StandingPoint.Location;
		M_StanceCaptureFramesLeft = SquadMovePreviewTestPrivate::StanceCaptureFramesPerLocation * 2;
	}
	CheckChosenFacing(PreviewComponent, TestSquad, CrouchPoint);
	FVector OpenGroundLocation = FVector::ZeroVector;
	const bool bFoundOpenGround = SquadMovePreviewTestPrivate::FindOpenGroundLocation(
		World,
		CoverSubsystem,
		TestSquad[0]->GetActorLocation(),
		OpenGroundLocation);
	Check(bFoundOpenGround, TEXT("map has open ground"));
	if (bFoundOpenGround)
	{
		CheckOpenGroundPlan(PreviewComponent, TestSquad, OpenGroundLocation);
	}
	CheckSeveralSquads(PreviewComponent, PlayerSquads, StandingPoint.Location);
	CheckPlanningCost(PreviewComponent, PlayerSquads, StandingPoint.Location);
	// -SquadMovePreviewWatch: a rendered run sends the squad to low cover and looks at it, so what only happens
	// on screen and at crouch or prone cover, a slide or a roll into it, happens in this test as well.
	const bool bWatchLowCover = FParse::Param(FCommandLine::Get(), TEXT("SquadMovePreviewWatch"));
	if (bWatchLowCover)
	{
		SquadMovePreviewTestPrivate::WatchLocationFromAbove(
			World,
			CrouchPoint.Location,
			SquadMovePreviewTestPrivate::WatchHeight,
			SquadMovePreviewTestPrivate::WatchBackOffset);
	}
	IssuePlannedMove(PreviewComponent, TestSquad, bWatchLowCover ? CrouchPoint.Location : StandingPoint.Location);
}

void FSquadMovePreviewTestScenario::CheckStanceMeshes(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& TestSquad,
	const FVector& CursorLocation)
{
	constexpr float LocationTolerance = 1.0f;
	constexpr float YawToleranceDegrees = 1.0f;
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const float YawOffsetDegrees = IsValid(CoverSettings) ? CoverSettings->M_PreviewStanceYawOffsetDegrees : 0.0f;
	FSquadMovePreviewInput Input;
	Input.SelectedSquads = &TestSquad;
	Input.bSquadsOnlyMoveContext = true;
	Input.bCursorOnMoveGround = true;
	Input.CursorLocation = CursorLocation;
	PreviewComponent.UpdatePreview(Input);
	const FSquadMovePlan& ShownPlan = PreviewComponent.GetCurrentPlan();
	const FSquadMovePreviewStances& Stances = PreviewComponent.GetStances();
	Check(
		ShownPlan.UnitPositions.Num() > 0 && Stances.GetShownInstanceCount() == ShownPlan.UnitPositions.Num(),
		TEXT("one stance mesh per planned soldier"),
		FString::Printf(
			TEXT("meshes=%d positions=%d no_cover=%d crouch=%d high=%d prone=%d trench=%d"),
			Stances.GetShownInstanceCount(),
			ShownPlan.UnitPositions.Num(),
			Stances.GetShownInstanceCount(ESquadPreviewStance::NoCover),
			Stances.GetShownInstanceCount(ESquadPreviewStance::CrouchCover),
			Stances.GetShownInstanceCount(ESquadPreviewStance::HighCover),
			Stances.GetShownInstanceCount(ESquadPreviewStance::ProneCover),
			Stances.GetShownInstanceCount(ESquadPreviewStance::TrenchCover)));
	int32 MatchedPositionCount = 0;
	for (const FSquadUnitPlannedPosition& Position : ShownPlan.UnitPositions)
	{
		const ESquadPreviewStance Stance = FSquadMovePreviewStances::GetStanceForPosition(Position);
		const float WantedYaw = Position.Facing.Rotation().Yaw + YawOffsetDegrees;
		FTransform InstanceTransform;
		bool bHasMatchingMesh = false;
		for (int32 ShownIndex = 0; Stances.TryGetShownInstanceTransform(Stance, ShownIndex, InstanceTransform); ++ShownIndex)
		{
			const float YawError = FMath::Abs(FRotator::NormalizeAxis(InstanceTransform.Rotator().Yaw - WantedYaw));
			bHasMatchingMesh = bHasMatchingMesh ||
				(InstanceTransform.GetLocation().Equals(Position.Location, LocationTolerance) &&
					YawError <= YawToleranceDegrees && InstanceTransform.GetScale3D().Equals(FVector::OneVector));
		}
		MatchedPositionCount += bHasMatchingMesh ? 1 : 0;
	}
	Check(
		MatchedPositionCount == ShownPlan.UnitPositions.Num(),
		TEXT("every stance mesh stands on its position, in its stance, looking along its facing"),
		FString::Printf(TEXT("matched=%d of %d"), MatchedPositionCount, ShownPlan.UnitPositions.Num()));

	Input.bSquadsOnlyMoveContext = false;
	PreviewComponent.UpdatePreview(Input);
	Check(Stances.GetShownInstanceCount() == 0, TEXT("the stance meshes go away with the preview"));
}

void FSquadMovePreviewTestScenario::TickStanceCapture(USquadMovePreviewComponent& PreviewComponent, UWorld& World)
{
	using namespace SquadMovePreviewTestPrivate;
	if (M_StanceCaptureFramesLeft <= 0)
	{
		return;
	}
	--M_StanceCaptureFramesLeft;
	const int32 LocationIndex = M_StanceCaptureFramesLeft >= StanceCaptureFramesPerLocation ? 0 : 1;
	const int32 FramesLeftAtLocation = M_StanceCaptureFramesLeft % StanceCaptureFramesPerLocation;
	TArray<ASquadController*> CaptureSquads;
	for (const TWeakObjectPtr<ASquadController>& CaptureSquad : M_StanceCaptureSquads)
	{
		if (CaptureSquad.IsValid())
		{
			CaptureSquads.Add(CaptureSquad.Get());
		}
	}
	// The player controller's own updates are held off for as long as frames are left.
	FSquadMovePreviewInput Input;
	Input.SelectedSquads = &CaptureSquads;
	Input.bSquadsOnlyMoveContext = true;
	Input.bCursorOnMoveGround = true;
	Input.CursorLocation = M_StanceCaptureLocations[LocationIndex];
	PreviewComponent.ApplyPreviewInput(Input);
	if (FramesLeftAtLocation == StanceCaptureFramesPerLocation - 1)
	{
		WatchLocationFromAbove(World, Input.CursorLocation, StanceCaptureCameraHeight, StanceCaptureCameraBackOffset);
	}
	if (FramesLeftAtLocation == StanceCaptureFramesPerLocation / 2)
	{
		UE_LOG(
			LogRTSSquadPreviewTest,
			Display,
			TEXT("RTS_SQUAD_PREVIEW_TEST stance_capture location=%d shown=%d no_cover=%d crouch=%d high=%d prone=%d trench=%d"),
			LocationIndex,
			PreviewComponent.GetStances().GetShownInstanceCount(),
			PreviewComponent.GetStances().GetShownInstanceCount(ESquadPreviewStance::NoCover),
			PreviewComponent.GetStances().GetShownInstanceCount(ESquadPreviewStance::CrouchCover),
			PreviewComponent.GetStances().GetShownInstanceCount(ESquadPreviewStance::HighCover),
			PreviewComponent.GetStances().GetShownInstanceCount(ESquadPreviewStance::ProneCover),
			PreviewComponent.GetStances().GetShownInstanceCount(ESquadPreviewStance::TrenchCover));
		FScreenshotRequest::RequestScreenshot(
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CoverFinderDebug"),
				FString::Printf(TEXT("SquadPreviewStances_%d.png"), LocationIndex)),
			false,
			false);
	}
	if (M_StanceCaptureFramesLeft == 0)
	{
		Input.bSquadsOnlyMoveContext = false;
		PreviewComponent.ApplyPreviewInput(Input);
	}
}

void FSquadMovePreviewTestScenario::CheckPronePlan(
	USquadMovePreviewComponent& PreviewComponent,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const TArray<ASquadController*>& TestSquad)
{
	// A map without prone cover, or with prone cover switched off, has nothing to plan onto.
	const FVector SquadLocation = TestSquad[0]->GetActorLocation();
	const FRTSCoverPoint* NearestPronePoint = nullptr;
	for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
	{
		const bool bIsFreePronePoint = CoverPoint.CoverType == ERTSCoverType::Prone &&
			not IsValid(CoverSubsystem.GetCoverReservationOwner(CoverPoint.PointId));
		const bool bIsNearer = NearestPronePoint == nullptr ||
			FVector::DistSquared2D(SquadLocation, CoverPoint.Location) <
			FVector::DistSquared2D(SquadLocation, NearestPronePoint->Location);
		NearestPronePoint = bIsFreePronePoint && bIsNearer ? &CoverPoint : NearestPronePoint;
	}
	if (NearestPronePoint == nullptr)
	{
		return;
	}
	TArray<FSquadMovePreviewUnit> Units;
	FSquadMovePlan PronePlan;
	PreviewComponent.BuildPlanForSquads(TestSquad, NearestPronePoint->Location, false, FVector::ZeroVector, Units, PronePlan);
	Check(
		SquadMovePreviewTestPrivate::CountPositionsOfType(PronePlan, ESquadPlannedPositionType::ProneCover) > 0,
		TEXT("cursor on prone cover plans prone cover"));
	Check(
		SquadMovePreviewTestPrivate::GetPlanUsesCoverPoint(PronePlan, NearestPronePoint->PointId),
		TEXT("the prone point under the cursor is used"));
}

void FSquadMovePreviewTestScenario::CheckCoverPlans(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& TestSquad,
	const FRTSCoverPoint& CrouchPoint,
	const FRTSCoverPoint& StandingPoint)
{
	TArray<FSquadMovePreviewUnit> Units;
	FSquadMovePlan CrouchPlan;
	const bool bCrouchPlanned = PreviewComponent.BuildPlanForSquads(
		TestSquad,
		CrouchPoint.Location,
		false,
		FVector::ZeroVector,
		Units,
		CrouchPlan);
	Check(bCrouchPlanned && CrouchPlan.UnitPositions.Num() == Units.Num(), TEXT("one position per soldier"));
	Check(
		SquadMovePreviewTestPrivate::CountPositionsOfType(CrouchPlan, ESquadPlannedPositionType::CrouchCover) > 0,
		TEXT("cursor on crouch cover plans crouch cover"));
	Check(
		SquadMovePreviewTestPrivate::GetPlanUsesCoverPoint(CrouchPlan, CrouchPoint.PointId),
		TEXT("the cover point under the cursor is used"));
	Check(
		SquadMovePreviewTestPrivate::FindSmallestSpacing(CrouchPlan) >=
		SquadMovePreviewTestPrivate::MinimumPlannedSpacing || CrouchPlan.UnitPositions.Num() < 2,
		TEXT("planned positions keep their spacing"),
		FString::Printf(TEXT("smallest=%.0f"), SquadMovePreviewTestPrivate::FindSmallestSpacing(CrouchPlan)));
	Check(
		not SquadMovePreviewTestPrivate::GetHasDuplicateCoverPoints(CrouchPlan),
		TEXT("no cover point is planned twice"));

	FSquadMovePlan StandingPlan;
	PreviewComponent.BuildPlanForSquads(TestSquad, StandingPoint.Location, false, FVector::ZeroVector, Units, StandingPlan);
	Check(
		SquadMovePreviewTestPrivate::CountPositionsOfType(StandingPlan, ESquadPlannedPositionType::StandingCover) > 0,
		TEXT("cursor on standing cover plans standing cover"));
	const bool bPlansDiffer = CrouchPlan.UnitPositions.Num() != StandingPlan.UnitPositions.Num() ||
		(CrouchPlan.UnitPositions.Num() > 0 && not CrouchPlan.UnitPositions[0].Location.Equals(
			StandingPlan.UnitPositions[0].Location,
			1.0f));
	Check(
		bPlansDiffer || FVector::Dist2D(CrouchPoint.Location, StandingPoint.Location) < 100.0f,
		TEXT("moving the cursor changes the plan"));
}

void FSquadMovePreviewTestScenario::CheckOpenGroundPlan(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& TestSquad,
	const FVector& OpenGroundLocation)
{
	TArray<FSquadMovePreviewUnit> Units;
	FSquadMovePlan OpenPlan;
	PreviewComponent.BuildPlanForSquads(TestSquad, OpenGroundLocation, false, FVector::ZeroVector, Units, OpenPlan);
	const int32 RegularCount = SquadMovePreviewTestPrivate::CountPositionsOfType(
		OpenPlan,
		ESquadPlannedPositionType::RegularStanding);
	Check(
		RegularCount == OpenPlan.UnitPositions.Num() && RegularCount > 0,
		TEXT("cursor on open ground plans only regular positions"),
		FString::Printf(TEXT("regular=%d of %d"), RegularCount, OpenPlan.UnitPositions.Num()));
	float FurthestFromCursor = 0.0f;
	for (const FSquadUnitPlannedPosition& Position : OpenPlan.UnitPositions)
	{
		FurthestFromCursor = FMath::Max(FurthestFromCursor, FVector::Dist2D(Position.Location, OpenGroundLocation));
	}
	constexpr float MaximumFormationRadius = 600.0f;
	Check(
		FurthestFromCursor <= MaximumFormationRadius,
		TEXT("regular positions gather around the cursor"),
		FString::Printf(TEXT("furthest=%.0f"), FurthestFromCursor));
}

void FSquadMovePreviewTestScenario::CheckChosenFacing(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& TestSquad,
	const FRTSCoverPoint& CoverPoint)
{
	TArray<FSquadMovePreviewUnit> Units;
	// Facing against the cover is facing the threat it protects from; facing along its normal is facing away.
	FSquadMovePlan FacingCoverPlan;
	PreviewComponent.BuildPlanForSquads(
		TestSquad,
		CoverPoint.Location,
		true,
		-CoverPoint.CoverNormal,
		Units,
		FacingCoverPlan);
	Check(
		SquadMovePreviewTestPrivate::GetPlanUsesCoverPoint(FacingCoverPlan, CoverPoint.PointId),
		TEXT("arrow pointing at the cover keeps that cover"));
	FSquadMovePlan FacingAwayPlan;
	PreviewComponent.BuildPlanForSquads(
		TestSquad,
		CoverPoint.Location,
		true,
		CoverPoint.CoverNormal,
		Units,
		FacingAwayPlan);
	Check(
		not SquadMovePreviewTestPrivate::GetPlanUsesCoverPoint(FacingAwayPlan, CoverPoint.PointId),
		TEXT("arrow pointing away from the cover drops that cover"));
	bool bRegularFacingMatchesArrow = true;
	for (const FSquadUnitPlannedPosition& Position : FacingAwayPlan.UnitPositions)
	{
		if (Position.Type == ESquadPlannedPositionType::RegularStanding && FacingAwayPlan.UnitPositions.Num() > 0)
		{
			bRegularFacingMatchesArrow &= FVector::DotProduct(
				Position.Facing,
				CoverPoint.CoverNormal.GetSafeNormal2D()) > -0.01f;
		}
	}
	Check(bRegularFacingMatchesArrow, TEXT("soldiers in the open never face against the arrow"));
}

void FSquadMovePreviewTestScenario::CheckSeveralSquads(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& PlayerSquads,
	const FVector& Anchor)
{
	TArray<FSquadMovePreviewUnit> Units;
	FSquadMovePlan Plan;
	PreviewComponent.BuildPlanForSquads(PlayerSquads, Anchor, false, FVector::ZeroVector, Units, Plan);
	Check(
		Plan.UnitPositions.Num() == Units.Num() && Plan.SquadAnchors.Num() == PlayerSquads.Num(),
		TEXT("several squads: every soldier and squad is planned"),
		FString::Printf(TEXT("soldiers=%d squads=%d"), Units.Num(), PlayerSquads.Num()));
	Check(
		not SquadMovePreviewTestPrivate::GetHasDuplicateCoverPoints(Plan),
		TEXT("several squads: no cover point is shared"));
	Check(
		SquadMovePreviewTestPrivate::FindSmallestSpacing(Plan) >= SquadMovePreviewTestPrivate::MinimumPlannedSpacing,
		TEXT("several squads: positions keep their spacing"),
		FString::Printf(TEXT("smallest=%.0f"), SquadMovePreviewTestPrivate::FindSmallestSpacing(Plan)));
}

void FSquadMovePreviewTestScenario::CheckPlanningCost(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& PlayerSquads,
	const FVector& Anchor)
{
	TArray<FSquadMovePreviewUnit> Units;
	FSquadMovePlan Plan;
	const double StartSeconds = FPlatformTime::Seconds();
	for (int32 Iteration = 0; Iteration < SquadMovePreviewTestPrivate::PlanningCostIterations; ++Iteration)
	{
		// The cursor keeps moving, as it does while the player sweeps the mouse over the map.
		const FVector MovingAnchor = Anchor + FVector(static_cast<float>(Iteration) * 10.0f, 0.0f, 0.0f);
		PreviewComponent.BuildPlanForSquads(PlayerSquads, MovingAnchor, false, FVector::ZeroVector, Units, Plan);
	}
	const double AverageMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0
		/ static_cast<double>(SquadMovePreviewTestPrivate::PlanningCostIterations);
	Check(
		AverageMilliseconds <= SquadMovePreviewTestPrivate::MaximumAveragePlanningMilliseconds,
		TEXT("replanning stays cheap"),
		FString::Printf(
			TEXT("average_ms=%.4f soldiers=%d squads=%d"),
			AverageMilliseconds,
			Units.Num(),
			PlayerSquads.Num()));
}

void FSquadMovePreviewTestScenario::IssuePlannedMove(
	USquadMovePreviewComponent& PreviewComponent,
	const TArray<ASquadController*>& TestSquad,
	const FVector& Anchor)
{
	// The expectation is the plan the order itself builds: same squads, anchor and facing.
	TArray<FSquadMovePreviewUnit> Units;
	FSquadMovePlan ExpectedPlan;
	PreviewComponent.BuildPlanForSquads(TestSquad, Anchor, false, FVector::ZeroVector, Units, ExpectedPlan);
	uint32 IssuedCommandCount = 0;
	const bool bIssued = PreviewComponent.TryIssuePlannedMove(
		TestSquad,
		Anchor,
		false,
		FRotator::ZeroRotator,
		false,
		IssuedCommandCount);
	Check(bIssued && IssuedCommandCount == 1, TEXT("planned move order accepted"));
	for (int32 UnitIndex = 0; UnitIndex < Units.Num(); ++UnitIndex)
	{
		FSquadMovePreviewTestExpectation& Expectation = M_Expectations.AddDefaulted_GetRef();
		Expectation.SquadUnit = Units[UnitIndex].SquadUnit;
		Expectation.Position = ExpectedPlan.UnitPositions[UnitIndex];
	}
	UE_LOG(
		LogRTSSquadPreviewTest,
		Display,
		TEXT("RTS_SQUAD_PREVIEW_TEST issued soldiers=%d standing_cover=%d crouch_cover=%d prone_cover=%d regular=%d"),
		Units.Num(),
		SquadMovePreviewTestPrivate::CountPositionsOfType(ExpectedPlan, ESquadPlannedPositionType::StandingCover),
		SquadMovePreviewTestPrivate::CountPositionsOfType(ExpectedPlan, ESquadPlannedPositionType::CrouchCover),
		SquadMovePreviewTestPrivate::CountPositionsOfType(ExpectedPlan, ESquadPlannedPositionType::ProneCover),
		SquadMovePreviewTestPrivate::CountPositionsOfType(ExpectedPlan, ESquadPlannedPositionType::RegularStanding));
	EnterPhase(ESquadMovePreviewTestPhase::WaitingForPlannedMove);
}

bool FSquadMovePreviewTestScenario::GetIsExpectationMet(
	const FSquadMovePreviewTestExpectation& Expectation,
	FString& OutDetails) const
{
	const ASquadUnit* SquadUnit = Expectation.SquadUnit.Get();
	if (not IsValid(SquadUnit))
	{
		OutDetails = TEXT("soldier no longer exists");
		return false;
	}
	const FSquadUnitCoverRuntimeState& CoverState = SquadUnit->GetCoverRuntimeState();
	const float DistanceToPlan = FVector::Dist2D(SquadUnit->GetActorLocation(), Expectation.Position.Location);
	ASquadController* SquadController = SquadUnit->GetSquadControllerChecked();
	OutDetails = FString::Printf(
		TEXT("soldier=%s planned_type=%d distance_cm=%.0f in_cover=%d cover_state=%s unit_command=%s squad_command=%s path_following=%d speed=%.0f"),
		*SquadUnit->GetName(),
		static_cast<int32>(Expectation.Position.Type),
		DistanceToPlan,
		SquadUnit->GetIsOccupyingCover() ? 1 : 0,
		*UEnum::GetValueAsString(CoverState.State),
		*UEnum::GetValueAsString(SquadUnit->GetActiveCommand()),
		IsValid(SquadController) ? *UEnum::GetValueAsString(SquadController->GetActiveCommandID()) : TEXT("none"),
		SquadUnit->GetIsPathFollowingActive() ? 1 : 0,
		SquadUnit->GetVelocity().Size2D());
	if (Expectation.Position.GetIsCover())
	{
		return SquadUnit->GetIsOccupyingCover() &&
			CoverState.AssignedCoverPoint.PointId == Expectation.Position.CoverPoint.PointId;
	}
	return not SquadUnit->GetHasCoverAssignment() &&
		DistanceToPlan <= SquadMovePreviewTestPrivate::RegularArrivalTolerance;
}

void FSquadMovePreviewTestScenario::FinishPlannedMoveCheck(UWorld& World)
{
	int32 MetExpectations = 0;
	for (const FSquadMovePreviewTestExpectation& Expectation : M_Expectations)
	{
		FString Details;
		const bool bMet = GetIsExpectationMet(Expectation, Details);
		MetExpectations += bMet ? 1 : 0;
		Check(bMet, TEXT("soldier ended on its previewed position"), Details);
	}
	Check(
		not M_Expectations.IsEmpty(),
		TEXT("a planned move was issued and followed"),
		FString::Printf(TEXT("met=%d of %d"), MetExpectations, M_Expectations.Num()));
	if (M_FailedCheckCount == 0)
	{
		UE_LOG(LogRTSSquadPreviewTest, Display, TEXT("RTS_SQUAD_PREVIEW_TEST RESULT PASS map=%s"), *World.GetMapName());
	}
	else
	{
		UE_LOG(
			LogRTSSquadPreviewTest,
			Error,
			TEXT("RTS_SQUAD_PREVIEW_TEST RESULT FAIL failed_checks=%d map=%s"),
			M_FailedCheckCount,
			*World.GetMapName());
	}
	EnterPhase(ESquadMovePreviewTestPhase::Inactive);
}

void FSquadMovePreviewTestScenario::EnterPhase(const ESquadMovePreviewTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseElapsedSeconds = 0.0f;
}
