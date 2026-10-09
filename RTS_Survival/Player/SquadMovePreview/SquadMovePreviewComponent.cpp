#include "SquadMovePreviewComponent.h"

#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"

namespace SquadMovePreviewPrivate
{
	// The plan is rebuilt when the cursor moved this far or the facing turned this much.
	constexpr float ReplanAnchorDistance = 25.0f;
	constexpr float ReplanFacingDot = 0.9994f;
	// Upper bound on replans per second while the cursor keeps moving.
	constexpr double MinimumReplanIntervalSeconds = 0.04;
	// A standing cursor still replans at this pace, because cover and reservations change underneath it.
	constexpr double IdleReplanIntervalSeconds = 0.5;
	// Closer than this to the squads' centre, the walking direction is too noisy to derive a facing from.
	constexpr float MinimumDistanceForAutomaticFacing = 150.0f;

	const FVector NavigationProjectionExtent(150.0f, 150.0f, 400.0f);

	bool GetCanPlanForSquad(const ASquadController* SquadController)
	{
		if (not IsValid(SquadController) || SquadController->GetSquadUnitsCount() <= 0)
		{
			return false;
		}
		// Team weapon crews are positioned by their weapon, and squads inside cargo cannot walk.
		if (IsValid(Cast<ATeamWeaponController>(SquadController)))
		{
			return false;
		}
		const UCargoSquad* CargoSquad = SquadController->FindComponentByClass<UCargoSquad>();
		return not IsValid(CargoSquad) || not CargoSquad->GetIsInsideCargo();
	}
}

USquadMovePreviewComponent::USquadMovePreviewComponent()
{
	// Driven by the player controller's tick; the component only ticks itself while its map test runs.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// The map test has to get the game past its paused start screen by itself.
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void USquadMovePreviewComponent::BeginPlay()
{
	Super::BeginPlay();
	// A dedicated server has nobody to show a preview to.
	UWorld* World = GetWorld();
	if (IsValid(World) && not IsRunningDedicatedServer())
	{
		M_Stances.Setup(*World);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("SquadMovePreviewValidate")))
	{
		M_TestScenario.Start();
		SetComponentTickEnabled(true);
	}
}

void USquadMovePreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	M_Stances.Destroy();
	Super::EndPlay(EndPlayReason);
}

void USquadMovePreviewComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	M_TestScenario.Tick(*this, DeltaTime);
	if (not M_TestScenario.GetIsRunning())
	{
		SetComponentTickEnabled(false);
	}
}

bool USquadMovePreviewComponent::GetCanPlanForSquads(const TArray<ASquadController*>& Squads)
{
	if (Squads.IsEmpty())
	{
		return false;
	}
	for (const ASquadController* SquadController : Squads)
	{
		if (not SquadMovePreviewPrivate::GetCanPlanForSquad(SquadController))
		{
			return false;
		}
	}
	return true;
}

void USquadMovePreviewComponent::SetFormationShape(const EFormation FormationShape)
{
	if (FormationShape == M_FormationShape)
	{
		return;
	}
	M_FormationShape = FormationShape;
	// The plan on screen was built for the previous shape.
	M_RefreshState.bHasPlan = false;
}

void USquadMovePreviewComponent::UpdatePreview(const FSquadMovePreviewInput& Input)
{
	// The player controller calls this every tick; with nothing selected it would wipe what the map test shows.
	if (M_TestScenario.GetOwnsPreview())
	{
		return;
	}
	ApplyPreviewInput(Input);
}

void USquadMovePreviewComponent::ApplyPreviewInput(const FSquadMovePreviewInput& Input)
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	const bool bPreviewEnabled = IsValid(CoverSettings) && CoverSettings->bM_EnableSquadMovePreview;
	const bool bHasMoveTarget = Input.bFacingChosenByPlayer || Input.bCursorOnMoveGround;
	const UWorld* World = GetWorld();
	if (not bPreviewEnabled || not Input.bSquadsOnlyMoveContext || not bHasMoveTarget ||
		Input.SelectedSquads == nullptr || not IsValid(World) || not GetCanPlanForSquads(*Input.SelectedSquads))
	{
		HidePreview();
		return;
	}

	// While the arrow is dragged the squads stay where the drag began and only their facing follows the cursor.
	const FVector Anchor = Input.bFacingChosenByPlayer ? Input.ChosenAnchorLocation : Input.CursorLocation;
	const FVector ChosenFacing = Input.ChosenRotation.Vector().GetSafeNormal2D();
	const uint32 SelectionHash = BuildSelectionHash(*Input.SelectedSquads);
	const double WorldSeconds = World->GetTimeSeconds();
	if (GetShouldReplan(Input, Anchor, ChosenFacing, SelectionHash, WorldSeconds))
	{
		M_RefreshState.bHasPlan = BuildPlanForSquads(
			*Input.SelectedSquads,
			Anchor,
			Input.bFacingChosenByPlayer,
			ChosenFacing,
			M_CurrentPlanUnits,
			M_CurrentPlan);
		M_RefreshState.Anchor = Anchor;
		M_RefreshState.Facing = ChosenFacing;
		M_RefreshState.bFacingChosenByPlayer = Input.bFacingChosenByPlayer;
		M_RefreshState.SelectionHash = SelectionHash;
		M_RefreshState.PlannedAtWorldSeconds = WorldSeconds;
		// The meshes only move when the plan did; between replans they simply stay where they stand.
		if (M_RefreshState.bHasPlan)
		{
			M_Stances.ShowPlan(M_CurrentPlan);
		}
		else
		{
			M_Stances.Hide();
		}
	}
}

bool USquadMovePreviewComponent::GetShouldReplan(
	const FSquadMovePreviewInput& Input,
	const FVector& Anchor,
	const FVector& Facing,
	const uint32 SelectionHash,
	const double WorldSeconds) const
{
	if (not M_RefreshState.bHasPlan || SelectionHash != M_RefreshState.SelectionHash ||
		Input.bFacingChosenByPlayer != M_RefreshState.bFacingChosenByPlayer)
	{
		return true;
	}
	const double SecondsSincePlan = WorldSeconds - M_RefreshState.PlannedAtWorldSeconds;
	if (SecondsSincePlan < SquadMovePreviewPrivate::MinimumReplanIntervalSeconds)
	{
		return false;
	}
	const bool bAnchorMoved = FVector::DistSquared2D(Anchor, M_RefreshState.Anchor) >
		FMath::Square(SquadMovePreviewPrivate::ReplanAnchorDistance);
	const bool bFacingTurned = Input.bFacingChosenByPlayer &&
		FVector::DotProduct(Facing, M_RefreshState.Facing) < SquadMovePreviewPrivate::ReplanFacingDot;
	return bAnchorMoved || bFacingTurned ||
		SecondsSincePlan >= SquadMovePreviewPrivate::IdleReplanIntervalSeconds;
}

bool USquadMovePreviewComponent::BuildPlanForSquads(
	const TArray<ASquadController*>& Squads,
	const FVector& Anchor,
	const bool bFacingChosenByPlayer,
	const FVector& Facing,
	TArray<FSquadMovePreviewUnit>& OutUnits,
	FSquadMovePlan& OutPlan)
{
	// Taken before the plan is overwritten, so cover that is already on screen keeps a small advantage.
	TSet<int64> PreviousCoverPointIds;
	for (const FSquadUnitPlannedPosition& PreviousPosition : M_CurrentPlan.UnitPositions)
	{
		if (PreviousPosition.GetIsCover())
		{
			PreviousCoverPointIds.Add(PreviousPosition.CoverPoint.PointId);
		}
	}

	OutUnits.Reset();
	OutPlan.Reset();
	if (not GetCanPlanForSquads(Squads))
	{
		return false;
	}
	FSquadMovePlanRequest Request;
	Request.Settings = M_PlannerSettings;
	Request.Anchor = Anchor;
	Request.bFacingChosenByPlayer = bFacingChosenByPlayer;
	Request.Formation = M_FormationShape;
	Request.PreviousCoverPointIds = MoveTemp(PreviousCoverPointIds);
	const FVector UnitsCentre = GatherPlannerUnits(Squads, OutUnits, Request);
	if (OutUnits.IsEmpty())
	{
		return false;
	}
	Request.Facing = ResolveFacing(bFacingChosenByPlayer, Facing, UnitsCentre, Anchor);
	GatherAvailableCoverPoints(OutUnits, Request);
	FSquadMovePlanner::BuildPlan(Request, OutPlan);
	ProjectRegularPositionsToNavigation(OutPlan);
	return true;
}

FVector USquadMovePreviewComponent::GatherPlannerUnits(
	const TArray<ASquadController*>& Squads,
	TArray<FSquadMovePreviewUnit>& OutUnits,
	FSquadMovePlanRequest& OutRequest) const
{
	FVector LocationSum = FVector::ZeroVector;
	OutRequest.SquadCount = Squads.Num();
	for (int32 SquadIndex = 0; SquadIndex < Squads.Num(); ++SquadIndex)
	{
		const TArray<ASquadUnit*> SquadUnits = Squads[SquadIndex]->GetSquadUnitsChecked();
		for (ASquadUnit* SquadUnit : SquadUnits)
		{
			if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive())
			{
				continue;
			}
			FSquadMovePreviewUnit& PreviewUnit = OutUnits.AddDefaulted_GetRef();
			PreviewUnit.SquadUnit = SquadUnit;
			PreviewUnit.SquadIndex = SquadIndex;
			FSquadMovePlannerUnit& PlannerUnit = OutRequest.Units.AddDefaulted_GetRef();
			PlannerUnit.SquadIndex = SquadIndex;
			PlannerUnit.Location = SquadUnit->GetActorLocation();
			LocationSum += PlannerUnit.Location;
		}
	}
	return OutUnits.IsEmpty() ? FVector::ZeroVector : LocationSum / static_cast<float>(OutUnits.Num());
}

void USquadMovePreviewComponent::GatherAvailableCoverPoints(
	const TArray<FSquadMovePreviewUnit>& Units,
	FSquadMovePlanRequest& InOutRequest) const
{
	const UWorld* World = GetWorld();
	const URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
		? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		: nullptr;
	if (not IsValid(CoverSubsystem))
	{
		return;
	}
	TSet<const ASquadUnit*> PlannedUnits;
	PlannedUnits.Reserve(Units.Num());
	for (const FSquadMovePreviewUnit& PreviewUnit : Units)
	{
		PlannedUnits.Add(PreviewUnit.SquadUnit.Get());
	}
	// One spatial-grid query around the cursor; the planner ranks the handful of points it returns.
	InOutRequest.CoverPoints = CoverSubsystem->FindCoverPointsInRadius(
		InOutRequest.Anchor,
		FSquadMovePlanner::GetCoverQueryRadius(InOutRequest));
	InOutRequest.CoverPoints.RemoveAll([CoverSubsystem, &PlannedUnits](const FRTSCoverPoint& CoverPoint)
	{
		const ASquadUnit* ReservingUnit = CoverSubsystem->GetCoverReservationOwner(CoverPoint.PointId);
		return IsValid(ReservingUnit) && not PlannedUnits.Contains(ReservingUnit);
	});
	// Soldiers outside the selection already use up room around thin obstacles; the planner gets what is left.
	for (int32 CoverPointIndex = InOutRequest.CoverPoints.Num() - 1; CoverPointIndex >= 0; --CoverPointIndex)
	{
		FRTSCoverPoint& CoverPoint = InOutRequest.CoverPoints[CoverPointIndex];
		if (CoverPoint.ThinObstacleId == 0)
		{
			continue;
		}
		const int32 RoomLeft = static_cast<int32>(CoverPoint.ThinObstacleCapacity) -
			CoverSubsystem->GetThinObstacleReservationCount(CoverPoint.ThinObstacleId, nullptr, &PlannedUnits);
		if (RoomLeft <= 0)
		{
			InOutRequest.CoverPoints.RemoveAt(CoverPointIndex);
			continue;
		}
		CoverPoint.ThinObstacleCapacity = static_cast<uint8>(RoomLeft);
	}
}

FVector USquadMovePreviewComponent::ResolveFacing(
	const bool bFacingChosenByPlayer,
	const FVector& Facing,
	const FVector& UnitsCentre,
	const FVector& Anchor)
{
	const FVector ChosenFacing = Facing.GetSafeNormal2D();
	if (bFacingChosenByPlayer && not ChosenFacing.IsNearlyZero())
	{
		return ChosenFacing;
	}
	// Without an arrow the squads face the way they walk, which is also where the player is looking.
	const FVector WalkDirection = Anchor - UnitsCentre;
	if (WalkDirection.SizeSquared2D() >
		FMath::Square(SquadMovePreviewPrivate::MinimumDistanceForAutomaticFacing))
	{
		M_LastAutomaticFacing = WalkDirection.GetSafeNormal2D();
	}
	return M_LastAutomaticFacing;
}

void USquadMovePreviewComponent::ProjectRegularPositionsToNavigation(FSquadMovePlan& InOutPlan) const
{
	for (FSquadUnitPlannedPosition& Position : InOutPlan.UnitPositions)
	{
		// Cover points already are navigation points; only the formation slots were computed on a flat plane.
		if (Position.Type == ESquadPlannedPositionType::RegularStanding)
		{
			Position.Location = ProjectToNavigation(Position.Location);
		}
	}
}

FVector USquadMovePreviewComponent::ProjectToNavigation(const FVector& Location) const
{
	const UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation ProjectedLocation;
	if (IsValid(NavigationSystem) && NavigationSystem->ProjectPointToNavigation(
		Location,
		ProjectedLocation,
		SquadMovePreviewPrivate::NavigationProjectionExtent))
	{
		return ProjectedLocation.Location;
	}
	return Location;
}

uint32 USquadMovePreviewComponent::BuildSelectionHash(const TArray<ASquadController*>& Squads) const
{
	uint32 SelectionHash = GetTypeHash(Squads.Num());
	for (const ASquadController* SquadController : Squads)
	{
		SelectionHash = HashCombine(SelectionHash, GetTypeHash(SquadController));
		SelectionHash = HashCombine(
			SelectionHash,
			GetTypeHash(IsValid(SquadController) ? SquadController->GetSquadUnitsCount() : 0));
	}
	return SelectionHash;
}

void USquadMovePreviewComponent::HidePreview()
{
	M_RefreshState.bHasPlan = false;
	M_CurrentPlan.Reset();
	M_CurrentPlanUnits.Reset();
	M_Stances.Hide();
}

bool USquadMovePreviewComponent::TryIssuePlannedMove(
	const TArray<ASquadController*>& SelectedSquads,
	const FVector& MoveLocation,
	const bool bFacingChosenByPlayer,
	const FRotator& ChosenRotation,
	const bool bQueueCommand,
	uint32& OutIssuedCommandCount)
{
	OutIssuedCommandCount = 0;
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	if (not IsValid(CoverSettings) || not CoverSettings->bM_EnableSquadMovePreview)
	{
		return false;
	}
	// Planned again for the exact order location, so the order never depends on how recent the preview was.
	TArray<FSquadMovePreviewUnit> PlannedUnits;
	FSquadMovePlan Plan;
	if (not BuildPlanForSquads(
		SelectedSquads,
		MoveLocation,
		bFacingChosenByPlayer,
		ChosenRotation.Vector(),
		PlannedUnits,
		Plan))
	{
		return false;
	}

	TArray<TArray<FSquadUnitPlannedDestination>> DestinationsPerSquad;
	DestinationsPerSquad.SetNum(SelectedSquads.Num());
	for (int32 UnitIndex = 0; UnitIndex < PlannedUnits.Num(); ++UnitIndex)
	{
		FSquadUnitPlannedDestination& Destination =
			DestinationsPerSquad[PlannedUnits[UnitIndex].SquadIndex].AddDefaulted_GetRef();
		Destination.SquadUnit = PlannedUnits[UnitIndex].SquadUnit;
		Destination.Position = Plan.UnitPositions[UnitIndex];
	}

	for (int32 SquadIndex = 0; SquadIndex < SelectedSquads.Num(); ++SquadIndex)
	{
		ASquadController* SquadController = SelectedSquads[SquadIndex];
		const FRotator FacingRotation = Plan.SquadFacings[SquadIndex].Rotation();
		const FVector SquadMoveLocation = ProjectToNavigation(Plan.SquadAnchors[SquadIndex]);
		SquadController->SetPlannedMoveDestinations(SquadMoveLocation, MoveTemp(DestinationsPerSquad[SquadIndex]));
		const ECommandQueueError MoveResult = SquadController->MoveToLocation(
			SquadMoveLocation,
			not bQueueCommand,
			FacingRotation,
			bFacingChosenByPlayer);
		OutIssuedCommandCount += MoveResult == ECommandQueueError::NoError ? 1 : 0;
	}
	return true;
}
