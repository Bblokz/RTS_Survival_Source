#include "FormationMovement.h"
#include "RTS_Survival/GameUI/FormationUI/W_FormationPicker.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "FormationEffectPooledActor/FormationEffectActor.h"

// Static priority initialization
TMap<EAllUnitType, int32> UFormationController::M_TUnitTypePriority = {
	{EAllUnitType::UNType_Harvester, 0},
	{EAllUnitType::UNType_Nomadic, 1},
	{EAllUnitType::UNType_Squad, 2},
	{EAllUnitType::UNType_Tank, 3},
	{EAllUnitType::UNType_Aircraft, 0},
};

UFormationController::UFormationController()
	: M_OriginalLocation()
	  , M_FormationRotation()
	  , M_PlayerRotationOverride()
{
	M_CurrentFormation = EFormation::RectangleFormation;
}

void UFormationController::InitFormationController(
	const TSharedPtr<FPlayerFormationPositionEffects>& InFormationEffectsStruct)
{
	if (not InFormationEffectsStruct.IsValid())
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this, "InFormationEffectsStruct", "InitFormationController");
		return;
	}

	M_FormationEffectsStructReference = InFormationEffectsStruct;
	M_PositionEffectOffset = InFormationEffectsStruct->PositionEffectsOffset;
}

bool UFormationController::IsPlayerRotationOverrideActive() const
{
	return bM_PlayerRotationOverride;
}

void UFormationController::InitiateMovement(
	const FVector& MoveLocation,
	const TArray<ASquadController*>* SelectedSquads,
	const TArray<ASelectablePawnMaster*>* SelectedPawns,
	const TArray<ASelectableActorObjectsMaster*>* SelectedActorMasters)
{
	M_OriginalLocation = MoveLocation;
	M_TPositionsPerUnitType.Empty();
	M_AssignedFormationSlots.Empty();

	if (not SelectedSquads || not SelectedPawns || not SelectedActorMasters)
	{
		RTSFunctionLibrary::ReportError("Null arrays passed to InitiateMovement!");
		return;
	}

	// Build *non-mutating* filtered views with only units that can actually move.
	TArray<ASquadController*> MovableSquads;
	TArray<ASelectablePawnMaster*> MovablePawns;
	TArray<ASelectableActorObjectsMaster*> MovableActors;
	BuildMovableSelections(*SelectedSquads, *SelectedPawns, *SelectedActorMasters,
	                       MovableSquads, MovablePawns, MovableActors);

	switch (M_CurrentFormation)
	{
	case EFormation::RectangleFormation:
		CreateRectangleFormation(MovableSquads, MovablePawns, MovableActors);
		break;
	case EFormation::SpearFormation:
		CreateSpearFormation(MovableSquads, MovablePawns, MovableActors, 5);
		break;
	case EFormation::ThinSpearFormation:
		CreateSpearFormation(MovableSquads, MovablePawns, MovableActors, 3);
		break;
	case EFormation::SemiCircleFormation:
		CreateSemiCircleFormation(MovableSquads, MovablePawns, MovableActors, 3, 20.f);
		break;
	default:
		break;
	}


	// Draws the positions in different ways depending on whether the formation rotation arrow was used to orient this
	// formation: draw arrow. Or if this was a regular formation: draw position marker.
	DrawFormationPositionEffects();

	if (DeveloperSettings::Debugging::GFormations_Compile_DebugSymbols)
	{
		DebugFormation();
	}
}

bool UFormationController::ActivateFormationPicker(const FVector2D& MousePosition) const
{
	if (not GetIsValidFormationPickerWidget())
	{
		return false;
	}
	return M_FormationPickerWidget->Activate(MousePosition);
}

void UFormationController::PrimaryClickPickFormation(const FVector2D& ClickedPosition)
{
	if (not GetIsValidFormationPickerWidget())
	{
		return;
	}
	M_CurrentFormation = M_FormationPickerWidget->GetFormationFromPrimaryClick(ClickedPosition);
}

FVector UFormationController::UseFormationPosition(
	const URTSComponent* RTSComponent,
	FRotator& OutMovementRotation,
	AActor* OwningActorForCommands)
{
	// When the supplied actor cannot move (no interface / lacks move ability), we do NOT
	// consume a formation slot. Return the actor’s own location to keep the formation intact.
	if (IsValid(OwningActorForCommands) && not GetActorHasMovementAbility(OwningActorForCommands))
	{
		// Keep rotation unchanged (caller may ignore it); do not pop any slot.
		return OwningActorForCommands->GetActorLocation();
	}

	if (not IsValid(RTSComponent))
	{
		// No component to resolve type/subtype; if we can, fall back to actor location.
		return IsValid(OwningActorForCommands) ? OwningActorForCommands->GetActorLocation() : FVector::ZeroVector;
	}

	const EAllUnitType UnitType = RTSComponent->GetUnitType();
	const int32 SubType = static_cast<int32>(RTSComponent->GetUnitSubType());

	const TWeakObjectPtr<AActor> OwningActorWeak = OwningActorForCommands;
	if (OwningActorWeak.IsValid())
	{
		if (FFormationAssignment Assignment; M_AssignedFormationSlots.RemoveAndCopyValue(OwningActorWeak, Assignment))
		{
			if (Assignment.UnitType == UnitType && Assignment.UnitSubType == SubType)
			{
				OutMovementRotation = Assignment.Rotation;
				return Assignment.Position;
			}

			// If types diverge, fall back to the type-based pool to keep priority ordering intact.
			M_AssignedFormationSlots.Add(OwningActorWeak, Assignment);
		}
	}

	if (auto* InnerMap = M_TPositionsPerUnitType.Find(UnitType))
	{
		if (auto* Data = InnerMap->Find(SubType))
		{
			if (Data->Positions.Num() > 0)
			{
				OutMovementRotation = Data->Rotations.Pop();
				return Data->Positions.Pop();
			}
			return ReportAndReturnError(UnitType, SubType, TEXT("No positions left for this subtype:"));
		}
		return ReportAndReturnError(UnitType, SubType, TEXT("No formation data for this subtype:"));
	}
	return ReportAndReturnError(UnitType, SubType, TEXT("No positions for this type:"));
}

void UFormationController::OnMovementFinished()
{
	bM_PlayerRotationOverride = false;
}


void UFormationController::SetPlayerRotationOverride(const FRotator& InRotation)
{
	bM_PlayerRotationOverride = true;
	M_PlayerRotationOverride = InRotation;
}

void UFormationController::ConsoleDebugFormations() const
{
	DebugFormation();
}

void UFormationController::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_CreateFormationWidget();

	if (not GetIsValidFormationEffectsStruct() ||
		not M_FormationEffectsStructReference->FormationPositionEffectClass)
	{
		RTSFunctionLibrary::ReportError("Formation controller cannot setup effect pool: invalid struct or class.");
		return;
	}

	BeginPlay_CreateEffectPool();
}

void UFormationController::BuildMovableSelections(
	const TArray<ASquadController*>& SelectedSquads,
	const TArray<ASelectablePawnMaster*>& SelectedPawns,
	const TArray<ASelectableActorObjectsMaster*>& SelectedActorMasters,
	TArray<ASquadController*>& OutMovableSquads,
	TArray<ASelectablePawnMaster*>& OutMovablePawns,
	TArray<ASelectableActorObjectsMaster*>& OutMovableActors) const
{
	OutMovableSquads.Reset();
	OutMovablePawns.Reset();
	OutMovableActors.Reset();

	for (ASquadController* Squad : SelectedSquads)
	{
		if (IsValid(Squad) && GetActorHasMovementAbility(Squad))
		{
			OutMovableSquads.Add(Squad);
		}
	}
	for (ASelectablePawnMaster* Pawn : SelectedPawns)
	{
		if (IsValid(Pawn) && GetActorHasMovementAbility(Pawn))
		{
			OutMovablePawns.Add(Pawn);
		}
	}
	for (ASelectableActorObjectsMaster* Actor : SelectedActorMasters)
	{
		if (IsValid(Actor) && GetActorHasMovementAbility(Actor))
		{
			OutMovableActors.Add(Actor);
		}
	}
}

bool UFormationController::GetActorHasMovementAbility(AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return false;
	}

	if (ICommands* Commands = Cast<ICommands>(Actor))
	{
		return Commands->HasAbility(EAbilityID::IdMove);
	}

	// No interface -> treat as non-movable (do not log; this is a soft gate used frequently).
	return false;
}

EFormationEffect UFormationController::GetFormationEffectFromFormation(const EFormation Formation) const
{
	switch (Formation)
	{
	case EFormation::RectangleFormation:
		return EFormationEffect::Arrow;
	case EFormation::SpearFormation:
		return EFormationEffect::ArrowSpearFormation;
	case EFormation::ThinSpearFormation:
		return EFormationEffect::ArrowThinSpearFormation;
	case EFormation::SemiCircleFormation:
		return EFormationEffect::ArrowCircleFormation;
	}
	return EFormationEffect::Arrow;
}

void UFormationController::DrawFormationPositionEffects()
{
	EFormationEffect FormationEffect = EFormationEffect::PositionMarker;
	if (bM_PlayerRotationOverride)
	{
		FormationEffect = GetFormationEffectFromFormation(M_CurrentFormation);
	}
	TArray<FRotator> Rotations;
	const TArray<FVector> Positions = GatherAllFormationPositions(Rotations);
	ShowEffectsAtPositions(Positions, FormationEffect, Rotations);

	// Sets a timer to hide the effects after the duration set on the struct.
	ScheduleFormationEffectHiding();
}

bool UFormationController::GetIsValidFormationEffectsStruct() const
{
	if (M_FormationEffectsStructReference.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Formation controller missing valid effects struct reference.");
	return false;
}

bool UFormationController::GetIsValidFormationPickerWidget() const
{
	if (IsValid(M_FormationPickerWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Formation controller cannot use the formation widget as it is not valid!");
	return false;
}

void UFormationController::BeginPlay_CreateFormationWidget()
{
	if (not GetIsValidFormationEffectsStruct() ||
		not M_FormationEffectsStructReference->FormationPickerWidgetClass)
	{
		RTSFunctionLibrary::ReportError(
			"Formation controller failed to create formation widget: struct or class invalid.");
		return;
	}

	M_FormationPickerWidget = CreateWidget<UW_FormationPicker>(
		GetWorld(),
		M_FormationEffectsStructReference->FormationPickerWidgetClass
	);
}

void UFormationController::BeginPlay_CreateEffectPool()
{
	if (not GetWorld())
	{
		RTSFunctionLibrary::ReportError("Cannot create effect pool: invalid world.");
		return;
	}

	constexpr int32 PoolSize = DeveloperSettings::GamePlay::Formation::MaxFormationEffects;
	M_FormationEffectActors.Reserve(PoolSize);

	for (int32 i = 0; i < PoolSize; ++i)
	{
		if (AFormationEffectActor* NewActor = GetWorld()->SpawnActor<AFormationEffectActor>(
			M_FormationEffectsStructReference->FormationPositionEffectClass,
			FTransform::Identity))
		{
			NewActor->SetActorEnableCollision(false);
			NewActor->SetActorHiddenInGame(true);
			M_FormationEffectActors.Add(NewActor);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Failed to spawn FormationPositionEffectClass actor.");
		}
	}
}

void UFormationController::CreateRectangleFormation(
	const TArray<ASquadController*>& Squads,
	const TArray<ASelectablePawnMaster*>& Pawns,
	const TArray<ASelectableActorObjectsMaster*>& Actors)
{
	TArray<FUnitData> Units;
	FVector AvgLoc;
	GatherAndSortUnits(Squads, Pawns, Actors, Units, AvgLoc);
	BuildRectangleFormationPositions(Units, AvgLoc);
}

void UFormationController::GatherAndSortUnits(
	const TArray<ASquadController*>& Squads,
	const TArray<ASelectablePawnMaster*>& Pawns,
	const TArray<ASelectableActorObjectsMaster*>& Actors,
	TArray<FUnitData>& OutUnits,
	FVector& OutAverageSelectedUnitsLocation) const
{
	// Clear any previous data
	OutUnits.Empty();
	FVector TotalLoc = FVector::ZeroVector;
	int32 Count = 0;

	/* 
	 * Lambda to extract FUnitData from a given RTSComponent:
	 * - Grabs the umbrella unit type and subtype.
	 * - Retrieves the formation radius.
	 * - Appends the constructed FUnitData to OutUnits.
	 * - Stores the original location of the unit.
	 */
	auto Capture = [&](const URTSComponent* Comp, AActor* Actor, const FVector& ActorLoc)
	{
		FUnitData D;
		D.UnitType = Comp->GetUnitType();
		D.UnitSubType = static_cast<int32>(Comp->GetUnitSubType());
		D.FormationRadius = Comp->GetFormationUnitInnerRadius();
		D.OriginalLocation = ActorLoc;
		D.SourceActor = Actor;
		OutUnits.Add(D);
	};

	for (ASquadController* EachSquadSelected : Squads)
	{
		if (EachSquadSelected && EachSquadSelected->GetRTSComponent())
		{
			Capture(EachSquadSelected->GetRTSComponent(), EachSquadSelected, EachSquadSelected->GetSquadLocation());
			TotalLoc += EachSquadSelected->GetSquadLocation();
			++Count;
		}
	}

	for (ASelectablePawnMaster* PawnMaster : Pawns)
	{
		if (PawnMaster && PawnMaster->GetRTSComponent())
		{
			Capture(PawnMaster->GetRTSComponent(), PawnMaster, PawnMaster->GetActorLocation());
			TotalLoc += PawnMaster->GetActorLocation();
			++Count;
		}
	}

	for (ASelectableActorObjectsMaster* EachSelectedActor : Actors)
	{
		if (EachSelectedActor && EachSelectedActor->GetRTSComponent())
		{
			Capture(EachSelectedActor->GetRTSComponent(), EachSelectedActor, EachSelectedActor->GetActorLocation());
			TotalLoc += EachSelectedActor->GetActorLocation();
			++Count;
		}
	}

	const FVector AverageLocation = (Count > 0)
		                                ? TotalLoc / static_cast<float>(Count)
		                                : FVector::ZeroVector;

	FVector SortForward = FVector::ForwardVector;
	FVector SortRight = FVector::RightVector;
	{
		FVector Dir = M_OriginalLocation - AverageLocation;
		Dir.Z = 0.f;
		if (Dir.Normalize())
		{
			const FRotator SortRotation = bM_PlayerRotationOverride
				                              ? M_PlayerRotationOverride
				                              : Dir.Rotation();
			SortForward = SortRotation.Vector();
			SortRight = SortRotation.RotateVector(FVector::RightVector);
		}
	}

	/*
	 * Sort the collected units:
	 *    - First by umbrella unit-type priority (front-to-back).
	 *    - Then, for equal priorities, by higher subtype value first.
	 *    - Finally, for identical priority/subtype, by their original distance to the target (closer units to the front)
	 *      and then by lateral offset (units already near the correct side remain there).
	 */
	OutUnits.Sort([&](const FUnitData& A, const FUnitData& B)
	{
		const int32 Pa = M_TUnitTypePriority.Contains(A.UnitType) ? M_TUnitTypePriority[A.UnitType] : INT32_MAX;
		const int32 Pb = M_TUnitTypePriority.Contains(B.UnitType) ? M_TUnitTypePriority[B.UnitType] : INT32_MAX;
		if (Pa != Pb)
		{
			return Pa < Pb;
		}

		if (A.UnitSubType != B.UnitSubType)
		{
			return A.UnitSubType > B.UnitSubType;
		}

		const float AForward = FVector::DotProduct(M_OriginalLocation - A.OriginalLocation, SortForward);
		const float BForward = FVector::DotProduct(M_OriginalLocation - B.OriginalLocation, SortForward);
		if (not FMath::IsNearlyEqual(AForward, BForward))
		{
			return AForward < BForward;
		}

		const float ALateral = FVector::DotProduct(A.OriginalLocation - M_OriginalLocation, SortRight);
		const float BLateral = FVector::DotProduct(B.OriginalLocation - M_OriginalLocation, SortRight);
		const float AAbsLateral = FMath::Abs(ALateral);
		const float BAbsLateral = FMath::Abs(BLateral);
		if (not FMath::IsNearlyEqual(AAbsLateral, BAbsLateral))
		{
			return AAbsLateral < BAbsLateral;
		}

		return ALateral < BLateral;
	});

	/*
	 * Compute the average position of all selected entities:
	 *    - Divide the accumulated TotalLoc by Count, if any units were found.
	 *    - Otherwise, default to the zero vector.
	 */
	OutAverageSelectedUnitsLocation = AverageLocation;
}


void UFormationController::BuildRectangleFormationPositions(
	const TArray<FUnitData>& Units,
	const FVector& AverageUnitsLocation)
{
	// Determine how many columns & rows we need based on total units and our ratio.
	int32 Columns = 0, Rows = 0;
	DetermineGridDimensions(Units.Num(), Columns, Rows);

	// Slice the flat unit list into row‐arrays of up to Columns each.
	TArray<TArray<FUnitData>> AllRows = GroupUnitsIntoRows(Units, Columns);

	// Compute the formation's forward/right axes from click‐to‐centroid.
	FVector Forward, Right;
	ComputeFormationAxes(AverageUnitsLocation, Forward, Right);

	// Sort each row by original lateral offset
	for (auto& Row : AllRows)
	{
		SortRowUnitsByOriginalPosition(Row, Right, AverageUnitsLocation);
	}

	// For each row, compute the local horizontal offsets and track each row’s max radius.
	TArray<TArray<FVector>> LocalPositions;
	TArray<float> MaxRadiusPerRow;
	ComputeLocalRowPositions(AllRows, Right, LocalPositions, MaxRadiusPerRow);

	// Compute each row’s world‐space origin so circles don’t overlap vertically.
	TArray<FVector> GlobalOffsets;
	ComputeRowGlobalOffsets(MaxRadiusPerRow, Forward, GlobalOffsets);

	// Combine per‐row origins & local offsets, then store into our formation map.
	StoreRectangleFormationPositions(AllRows, LocalPositions, GlobalOffsets);
}


TArray<FVector> UFormationController::GatherAllFormationPositions(TArray<FRotator>& OutAllRotations) const
{
	TArray<FVector> Out;
	for (const auto& TypePair : M_TPositionsPerUnitType)
	{
		for (const auto& SubPair : TypePair.Value)
		{
			Out.Append(SubPair.Value.Positions);
			OutAllRotations.Append(SubPair.Value.Rotations);
		}
	}
	return Out;
}

void UFormationController::DetermineGridDimensions(
	const int32 TotalUnits, int32& OutColumns, int32& OutRows) const
{
	OutColumns = FMath::CeilToInt(FMath::Sqrt(TotalUnits / static_cast<float>(M_RowsToColumnsRatio)));
	OutRows = M_RowsToColumnsRatio * OutColumns;
}

TArray<TArray<FUnitData>> UFormationController::GroupUnitsIntoRows(
	const TArray<FUnitData>& Units, const int32 Columns) const
{
	TArray<TArray<FUnitData>> Rows;
	Rows.Reserve(FMath::CeilToInt(Units.Num() / static_cast<float>(Columns)));
	for (int32 i = 0; i < Units.Num();)
	{
		TArray<FUnitData> Row;
		for (int32 c = 0; c < Columns && i < Units.Num(); ++c, ++i)
		{
			Row.Add(Units[i]);
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

void UFormationController::ComputeFormationAxes(
	const FVector& AverageUnitsLocation,
	FVector& OutForward, FVector& OutRight)
{
	FVector Dir = M_OriginalLocation - AverageUnitsLocation;
	Dir.Z = 0.f;
	if (not Dir.Normalize()) Dir = FVector::ForwardVector;

	const FRotator AutoRot = Dir.Rotation();
	M_FormationRotation = bM_PlayerRotationOverride
		                      ? M_PlayerRotationOverride
		                      : AutoRot;

	OutForward = M_FormationRotation.Vector();
	OutRight = M_FormationRotation.RotateVector(FVector::RightVector);
}

void UFormationController::ComputeLocalRowPositions(
	const TArray<TArray<FUnitData>>& AllRows,
	const FVector& RightVector,
	TArray<TArray<FVector>>& OutLocalPos,
	TArray<float>& OutMaxRadius) const
{
	const int32 NumRows = AllRows.Num();
	OutLocalPos.SetNum(NumRows);
	OutMaxRadius.SetNum(NumRows);

	for (int32 r = 0; r < NumRows; ++r)
	{
		const auto& RowData = AllRows[r];
		if (RowData.Num() == 0)
		{
			OutMaxRadius[r] = 0.f;
			continue;
		}

		OutLocalPos[r].SetNum(RowData.Num());
		OutLocalPos[r][0] = FVector::ZeroVector;
		float RowMax = RowData[0].FormationRadius;

		for (int32 c = 1; c < RowData.Num(); ++c)
		{
			const float Dist = FMath::Max(
				RowData[c - 1].FormationRadius,
				RowData[c].FormationRadius
			);
			OutLocalPos[r][c] = OutLocalPos[r][c - 1] + RightVector * Dist;
			RowMax = FMath::Max(RowMax, RowData[c].FormationRadius);
		}
		OutMaxRadius[r] = RowMax;
	}
}

void UFormationController::ComputeRowGlobalOffsets(
	const TArray<float>& MaxRadiusPerRow,
	const FVector& ForwardVector,
	TArray<FVector>& OutGlobalOffsets) const
{
	const int32 NumRows = MaxRadiusPerRow.Num();
	OutGlobalOffsets.SetNum(NumRows);
	if (NumRows > 0)
	{
		OutGlobalOffsets[0] = M_OriginalLocation;
	}

	for (int32 r = 1; r < NumRows; ++r)
	{
		const float Dist = (MaxRadiusPerRow[r - 1] + MaxRadiusPerRow[r]) / 1.5f;
		OutGlobalOffsets[r] = OutGlobalOffsets[r - 1] - ForwardVector * Dist;
	}
}

void UFormationController::StoreRectangleFormationPositions(
	const TArray<TArray<FUnitData>>& AllRows,
	const TArray<TArray<FVector>>& LocalPos,
	const TArray<FVector>& GlobalOffsets)
{
	int32 TotalUnits = 0;
	for (const TArray<FUnitData>& Row : AllRows)
	{
		TotalUnits += Row.Num();
	}

	TArray<FUnitData> OrderedUnits;
	TArray<FVector> OrderedPositions;
	TArray<FRotator> OrderedRotations;
	OrderedUnits.Reserve(TotalUnits);
	OrderedPositions.Reserve(TotalUnits);
	OrderedRotations.Reserve(TotalUnits);

	for (int32 r = 0; r < AllRows.Num(); ++r)
	{
		for (int32 c = 0; c < AllRows[r].Num(); ++c)
		{
			OrderedUnits.Add(AllRows[r][c]);
			OrderedPositions.Add(GlobalOffsets[r] + LocalPos[r][c]);
			OrderedRotations.Add(M_FormationRotation);
		}
	}

	BuildUnitAssignments(OrderedUnits, OrderedPositions, OrderedRotations);
}

void UFormationController::SortRowUnitsByOriginalPosition(
	TArray<FUnitData>& RowRows,
	const FVector& RightVector,
	const FVector& AverageUnitsLocation) const
{
	// Determine if movement is to the "right" (dot >= 0) or to the "left" (dot < 0)
	const float MoveSide = FVector::DotProduct(
		(AverageUnitsLocation - M_OriginalLocation),
		RightVector
	);

	RowRows.Sort([&](const FUnitData& A, const FUnitData& B)
	{
		const float AProj = FVector::DotProduct(A.OriginalLocation - M_OriginalLocation, RightVector);
		const float BProj = FVector::DotProduct(B.OriginalLocation - M_OriginalLocation, RightVector);

		// When MoveSide >= 0, smaller projection = more left → ascending sort
		// When MoveSide <  0, we want the opposite ordering to prevent crossing
		return (MoveSide >= 0.f)
			       ? (AProj < BProj)
			       : (AProj > BProj);
	});
}

void UFormationController::BuildUnitAssignments(
	const TArray<FUnitData>& Units,
	const TArray<FVector>& Positions,
	const TArray<FRotator>& Rotations)
{
	if (Units.Num() != Positions.Num() || Positions.Num() != Rotations.Num())
	{
		RTSFunctionLibrary::ReportError("Formation assignment inputs are mismatched.");
		return;
	}

	const FVector ForwardVector = M_FormationRotation.Vector();
	const FVector RightVector = M_FormationRotation.RotateVector(FVector::RightVector);

	struct FUnitPriority
	{
		int32 UnitIndex = INDEX_NONE;
		float ForwardDistance = 0.f;
		float AbsLateralOffset = 0.f;
		float LateralOffset = 0.f;
	};

	struct FSlotPriority
	{
		FVector Position;
		FRotator Rotation;
		float FrontPriority = 0.f;
		float AbsLateralOffset = 0.f;
		float LateralOffset = 0.f;
	};

	TMap<EAllUnitType, TMap<int32, TArray<FUnitPriority>>> UnitsByType;
	TMap<EAllUnitType, TMap<int32, TArray<FSlotPriority>>> SlotsByType;

	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		const FUnitData& Unit = Units[Index];

		FUnitPriority& UnitPriority = UnitsByType.FindOrAdd(Unit.UnitType).FindOrAdd(Unit.UnitSubType).AddDefaulted_GetRef();
		UnitPriority.UnitIndex = Index;
		UnitPriority.ForwardDistance = FVector::DotProduct(M_OriginalLocation - Unit.OriginalLocation, ForwardVector);
		UnitPriority.LateralOffset = FVector::DotProduct(Unit.OriginalLocation - M_OriginalLocation, RightVector);
		UnitPriority.AbsLateralOffset = FMath::Abs(UnitPriority.LateralOffset);

		FSlotPriority& SlotPriority = SlotsByType.FindOrAdd(Unit.UnitType).FindOrAdd(Unit.UnitSubType).AddDefaulted_GetRef();
		SlotPriority.Position = Positions[Index];
		SlotPriority.Rotation = Rotations[Index];
		const FVector OffsetFromOrigin = Positions[Index] - M_OriginalLocation;
		SlotPriority.FrontPriority = FVector::DotProduct(OffsetFromOrigin, ForwardVector);
		SlotPriority.LateralOffset = FVector::DotProduct(OffsetFromOrigin, RightVector);
		SlotPriority.AbsLateralOffset = FMath::Abs(SlotPriority.LateralOffset);
	}

	auto UnitSortPredicate = [](const FUnitPriority& A, const FUnitPriority& B)
	{
		if (not FMath::IsNearlyEqual(A.ForwardDistance, B.ForwardDistance))
		{
			return A.ForwardDistance < B.ForwardDistance;
		}
		if (not FMath::IsNearlyEqual(A.AbsLateralOffset, B.AbsLateralOffset))
		{
			return A.AbsLateralOffset < B.AbsLateralOffset;
		}
		return A.LateralOffset < B.LateralOffset;
	};

	auto SlotSortPredicate = [](const FSlotPriority& A, const FSlotPriority& B)
	{
		if (not FMath::IsNearlyEqual(A.FrontPriority, B.FrontPriority))
		{
			return A.FrontPriority > B.FrontPriority;
		}
		if (not FMath::IsNearlyEqual(A.AbsLateralOffset, B.AbsLateralOffset))
		{
			return A.AbsLateralOffset < B.AbsLateralOffset;
		}
		return A.LateralOffset < B.LateralOffset;
	};

	for (auto& TypePair : UnitsByType)
	{
		for (auto& SubPair : TypePair.Value)
		{
			TMap<int32, TArray<FSlotPriority>>* SlotsForType = SlotsByType.Find(TypePair.Key);
			if (not SlotsForType)
			{
				continue;
			}

			TArray<FSlotPriority>* SlotsForSubtype = SlotsForType->Find(SubPair.Key);
			if (not SlotsForSubtype)
			{
				continue;
			}

			SubPair.Value.Sort(UnitSortPredicate);
			SlotsForSubtype->Sort(SlotSortPredicate);

			const int32 PairCount = FMath::Min(SubPair.Value.Num(), SlotsForSubtype->Num());
			for (int32 PairIndex = 0; PairIndex < PairCount; ++PairIndex)
			{
				const FUnitPriority& UnitPriority = SubPair.Value[PairIndex];
				const FSlotPriority& SlotPriority = (*SlotsForSubtype)[PairIndex];
				const FUnitData& UnitData = Units[UnitPriority.UnitIndex];

				if (UnitData.SourceActor.IsValid())
				{
					M_AssignedFormationSlots.Add(
						UnitData.SourceActor,
						{SlotPriority.Position, SlotPriority.Rotation, UnitData.UnitType, UnitData.UnitSubType});
				}

				AddPositionForUnitToFormation(
					UnitData.UnitType,
					UnitData.UnitSubType,
					SlotPriority.Position,
					UnitData.FormationRadius,
					SlotPriority.Rotation
				);
			}
		}
	}
}


void UFormationController::ShowEffectsAtPositions(const TArray<FVector>& Positions,
                                                  const EFormationEffect EffectType, const TArray<FRotator>& Rotations)
{
	int32 Index = 0;
	for (const FVector& Pos : Positions)
	{
		FRotator Rotation = M_FormationRotation;
		if (Rotations.IsValidIndex(Index))
		{
			Rotation = Rotations[Index];
		}
		if (Index >= M_FormationEffectActors.Num()) return;
		if (AFormationEffectActor* FX = M_FormationEffectActors[Index]; IsValid(FX))
		{
			FX->SetActorLocation(Pos + M_PositionEffectOffset);
			FX->SetActorRotation(Rotation);
			FX->SetFormationEffectActivated(true, EffectType);
		}
		++Index;
	}
	HideRemainingEffects(Index);
}

void UFormationController::HideRemainingEffects(const int32 StartIndex)
{
	for (int32 i = StartIndex; i < M_FormationEffectActors.Num(); ++i)
	{
		if (AFormationEffectActor* FX = M_FormationEffectActors[i]; IsValid(FX))
		{
			FX->SetFormationEffectActivated(false, EFormationEffect::PositionMarker);
		}
	}
}

void UFormationController::AddPositionForUnitToFormation(
	const EAllUnitType UnitType,
	const int32 UnitSubType,
	const FVector& Position,
	const float Radius,
	const FRotator& Rotation)
{
	if (not M_TPositionsPerUnitType.Contains(UnitType))
		M_TPositionsPerUnitType.Add(UnitType, {});
	auto& SubMap = M_TPositionsPerUnitType[UnitType];

	if (not SubMap.Contains(UnitSubType))
	{
		FSubtypeFormationData Data;
		Data.Radius = Radius;
		SubMap.Add(UnitSubType, MoveTemp(Data));
	}
	auto& Bucket = SubMap[UnitSubType];
	Bucket.Positions.Add(Position);
	Bucket.Rotations.Add(Rotation);
}


FVector UFormationController::ReportAndReturnError(
	const EAllUnitType UnitType,
	const int32 UnitSubType,
	const FString& ErrorMessage) const
{
	const FTrainingOption TrainingOption(UnitType, UnitSubType);
	RTSFunctionLibrary::ReportError(ErrorMessage + TrainingOption.GetTrainingName());
	return FVector::ZeroVector;
}


void UFormationController::ScheduleFormationEffectHiding()
{
	if (not GetIsValidFormationEffectsStruct() || not GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(M_FormationEffectDurationTimer);
	float Duration;
	if (not TryGetEffectHideDuration(Duration))
	{
		RTSFunctionLibrary::ReportError("Formation effect duration is not set.");
		HideAllPooledEffectActors();
		return;
	}

	const FTimerDelegate Delegate = MakeHideEffectsDelegate();
	GetWorld()->GetTimerManager().SetTimer(
		M_FormationEffectDurationTimer,
		Delegate,
		Duration,
		false
	);
}

bool UFormationController::TryGetEffectHideDuration(float& OutDuration) const
{
	if (not M_FormationEffectsStructReference.IsValid())
	{
		return false;
	}
	OutDuration = M_FormationEffectsStructReference->PositionEffectsDuration;
	return OutDuration > 0.f;
}

FTimerDelegate UFormationController::MakeHideEffectsDelegate()
{
	return FTimerDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UFormationController>(this)]()
	{
		if (UFormationController* This = WeakThis.Get())
		{
			This->HideAllPooledEffectActors();
		}
	});
}

void UFormationController::HideAllPooledEffectActors()
{
	for (AFormationEffectActor* FX : M_FormationEffectActors)
	{
		if (IsValid(FX))
		{
			FX->SetFormationEffectActivated(false, EFormationEffect::PositionMarker);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Formation effect actor invalid when hiding.");
		}
	}
}


void UFormationController::CreateSpearFormation(
	const TArray<ASquadController*>& Squads,
	const TArray<ASelectablePawnMaster*>& Pawns,
	const TArray<ASelectableActorObjectsMaster*>& Actors,
	int32 SpearThickness)
{
	ValidateSpearThickness(SpearThickness);
	TArray<FUnitData> Units;
	FVector AvgLoc;
	GatherAndSortUnits(Squads, Pawns, Actors, Units, AvgLoc);

	// Compute formation axes
	FVector Forward, Right;
	ComputeFormationAxes(AvgLoc, Forward, Right);

	// Generate world-space positions in spear formation
	TArray<FVector> Positions = GenerateSpearFormationPositions(
		Units,
		SpearThickness,
		Forward,
		Right
	);

	TArray<FRotator> Rotations;
	Rotations.Init(M_FormationRotation, Positions.Num());
	BuildUnitAssignments(Units, Positions, Rotations);
}


void UFormationController::ValidateSpearThickness(int32& Thickness) const
{
	if (Thickness < 3)
	{
		Thickness = 3;
		RTSFunctionLibrary::DisplayNotification(
			FText::FromString(
				TEXT("Spear formation thickness too low; clamped to 3.")));
	}
}

float UFormationController::ComputeRowSpacing(
	const TArray<FUnitData>& PrevRow,
	const TArray<FUnitData>& CurRow) const
{
	float MaxPrev = 0.f, MaxCur = 0.f;
	for (const auto& D : PrevRow)
		MaxPrev = FMath::Max(MaxPrev, D.FormationRadius);
	for (const auto& D : CurRow)
		MaxCur = FMath::Max(MaxCur, D.FormationRadius);

	// center-to-center distance so circles of each row just touch
	return MaxPrev + MaxCur;
}

float UFormationController::ComputeIntraRowSpacing(
	const TArray<FUnitData>& Row) const
{
	if (Row.Num() < 2)
		return 0.f;

	float MaxSpacing = 0.f;
	for (int32 i = 1; i < Row.Num(); ++i)
	{
		const float PairDist = Row[i - 1].FormationRadius + Row[i].FormationRadius / 1.5;
		MaxSpacing = FMath::Max(MaxSpacing, PairDist);
	}
	return MaxSpacing;
}

TArray<FVector> UFormationController::GenerateSpearFormationPositions(
	const TArray<FUnitData>& Units,
	const int32 Thickness,
	const FVector& ForwardAxis,
	const FVector& RightAxis) const
{
	TArray<FVector> Out;
	if (Units.Num() == 0)
	{
		return Out;
	}

	// 1) Build row‐lists up to Thickness:
	//    row 0 → 1 unit
	//    row 1 → 2 units
	//    ...
	//    row m-1 → m units
	//    row ≥ m → m units
	TArray<TArray<FUnitData>> Rows;
	Rows.Reserve((Units.Num() + Thickness - 1) / Thickness + 1);

	int32 Placed = 0;
	while (Placed < Units.Num())
	{
		const int32 RowIndex = Rows.Num(); // 0,1,2,...
		int32 DesiredCount = (RowIndex < Thickness) ? (RowIndex + 1) : Thickness;
		DesiredCount = FMath::Min(DesiredCount, Units.Num() - Placed);

		TArray<FUnitData> ThisRow;
		ThisRow.Reserve(DesiredCount);
		for (int32 i = 0; i < DesiredCount; ++i)
		{
			ThisRow.Add(Units[Placed++]);
		}
		Rows.Add(MoveTemp(ThisRow));
	}

	// 2) Compute each row’s center cumulatively:
	TArray<FVector> Centers;
	Centers.SetNum(Rows.Num());
	Centers[0] = M_OriginalLocation;

	for (int32 r = 1; r < Rows.Num(); ++r)
	{
		const float Sp = ComputeRowSpacing(Rows[r - 1], Rows[r]);
		// half-spacing for rows before we reach full thickness
		const float Mul = (r < Thickness ? 0.5f : 1.0f);
		Centers[r] = Centers[r - 1] - ForwardAxis * (Sp * Mul);
	}

	// 3) Spread each row’s units horizontally using the max intra-row radius
	for (int32 r = 0; r < Rows.Num(); ++r)
	{
		const TArray<FUnitData>& Row = Rows[r];
		const float Intra = ComputeIntraRowSpacing(Row);
		const int32 N = Row.Num();
		// center the group on the row center
		const float HalfSpan = (N - 1) * 0.5f * Intra;

		for (int32 c = 0; c < N; ++c)
		{
			const float Offset = c * Intra - HalfSpan;
			Out.Add(Centers[r] + RightAxis * Offset);
		}
	}

	return Out;
}

void UFormationController::CreateSemiCircleFormation(
	const TArray<ASquadController*>& Squads,
	const TArray<ASelectablePawnMaster*>& Pawns,
	const TArray<ASelectableActorObjectsMaster*>& Actors,
	int32 UnitsPerSlice,
	float CurvatureAngle)
{
	// 1) collect & sort
	TArray<FUnitData> Units;
	FVector AvgLoc;
	GatherAndSortUnits(Squads, Pawns, Actors, Units, AvgLoc);

	// 2) compute axes
	FVector Forward, Right;
	ComputeFormationAxes(AvgLoc, Forward, Right);

	// 3) generate positions + rotations
	TArray<FVector> Positions;
	TArray<FRotator> Rotations;
	GenerateSemiCirclePositions(
		Units, UnitsPerSlice, CurvatureAngle,
		Forward, Right,
		Positions, Rotations
	);

	BuildUnitAssignments(Units, Positions, Rotations);
}


void UFormationController::GenerateSemiCirclePositions(
	const TArray<FUnitData>& Units,
	int32 UnitsPerSlice,
	float CurvatureAngle,
	const FVector& ForwardAxis,
	const FVector& RightAxis,
	TArray<FVector>& OutPositions,
	TArray<FRotator>& OutRotations
) const
{
	if (Units.IsEmpty()) return;

	if (Units.Num() == 1)
	{
		// just put it at the origin click, facing the formation rotation
		OutPositions.Add(M_OriginalLocation);
		OutRotations.Add(M_FormationRotation);
		return;
	}

	// Partition units into slices that grow by +2 each time
	auto Slices = PartitionUnitsIntoSlices(Units, UnitsPerSlice);
	// Compute the center point of each slice (stacked back by row-spacing)
	auto Centers = ComputeSemiCircleCenters(Slices, M_OriginalLocation, ForwardAxis);

	OutPositions.Reserve(Units.Num());
	OutRotations.Reserve(Units.Num());

	// For each slice, place its units along a half-arc
	for (int32 s = 0; s < Slices.Num(); ++s)
	{
		PlaceUnitsInHalfArc(
			Slices[s],
			CurvatureAngle,
			Centers[s],
			ForwardAxis,
			RightAxis,
			OutPositions,
			OutRotations
		);
	}
}


TArray<TArray<FUnitData>> UFormationController::PartitionUnitsIntoSlices(
	const TArray<FUnitData>& Units,
	const int32 UnitsPerSlice
) const
{
	TArray<TArray<FUnitData>> Slices;
	// reserve enough slices to avoid reallocations
	Slices.Reserve((Units.Num() + UnitsPerSlice - 1) / UnitsPerSlice);

	int32 UnitsRemaining = Units.Num();
	int32 CurrentSliceCapacity = UnitsPerSlice;
	int32 UnitIndex = 0;

	while (UnitsRemaining > 0)
	{
		const int32 UnitsThisSlice = FMath::Min(CurrentSliceCapacity, UnitsRemaining);
		TArray<FUnitData> ThisSlice;
		ThisSlice.Reserve(UnitsThisSlice);

		for (int32 SliceUnit = 0; SliceUnit < UnitsThisSlice; ++SliceUnit)
		{
			ThisSlice.Add(Units[UnitIndex++]);
		}

		Slices.Add(MoveTemp(ThisSlice));
		UnitsRemaining -= UnitsThisSlice;
		CurrentSliceCapacity += 2;
	}

	return Slices;
}


TArray<FVector> UFormationController::ComputeSemiCircleCenters(
	const TArray<TArray<FUnitData>>& Slices,
	const FVector& Origin,
	const FVector& ForwardAxis
) const
{
	const int32 Num = Slices.Num();
	TArray<FVector> Centers;
	Centers.SetNum(Num);
	if (Num == 0) return Centers;

	Centers[0] = Origin;
	for (int32 s = 1; s < Num; ++s)
	{
		const float Sp = ComputeRowSpacing(Slices[s - 1], Slices[s]);
		Centers[s] = Centers[s - 1] - ForwardAxis * Sp;
	}
	return Centers;
}

void UFormationController::PlaceUnitsInHalfArc(
	const TArray<FUnitData>& Slice,
	const float CurvatureAngle,
	const FVector& Center,
	const FVector& ForwardAxis,
	const FVector& RightAxis,
	TArray<FVector>& OutPositions,
	TArray<FRotator>& OutRotations
) const
{
	int32 UnitCount = Slice.Num();
	if (UnitCount == 0) return;

	float InterUnitSpacing = ComputeIntraRowSpacing(Slice);

	for (int32 UnitIdx = 0; UnitIdx < UnitCount; ++UnitIdx)
	{
		float LateralOffset = 0.f, YawOffsetDeg = 0.f;

		// Compute each unit’s lateral + yaw offset
		CalculateUnitArcOffsets(
			UnitIdx,
			UnitCount,
			InterUnitSpacing,
			CurvatureAngle,
			LateralOffset,
			YawOffsetDeg
		);

		// Position: apply lateral offset then depth sag
		FVector Pos = Center + RightAxis * LateralOffset;
		Pos = ApplyDepthSag(Pos, YawOffsetDeg, InterUnitSpacing, ForwardAxis);
		OutPositions.Add(Pos);

		// Rotation: base formation rotation plus outward yaw
		FRotator UnitRot = M_FormationRotation;
		UnitRot.Yaw += YawOffsetDeg;
		OutRotations.Add(UnitRot);
	}
}

void UFormationController::CalculateUnitArcOffsets(
	const int32 UnitIndex,
	const int32 SliceSize,
	const float InterUnitSpacing,
	const float CurvatureAngle,
	float& OutLateralOffset,
	float& OutYawOffsetDegrees
) const
{
	const bool bIsEvenCount = (SliceSize % 2 == 0);
	const int32 MiddleIndex = SliceSize / 2;

	if (bIsEvenCount)
	{
		ComputeEvenSliceOffsets(
			UnitIndex, MiddleIndex,
			InterUnitSpacing, CurvatureAngle,
			OutLateralOffset, OutYawOffsetDegrees
		);
	}
	else
	{
		ComputeOddSliceOffsets(
			UnitIndex, MiddleIndex,
			InterUnitSpacing, CurvatureAngle,
			OutLateralOffset, OutYawOffsetDegrees
		);
	}
}

auto UFormationController::ComputeEvenSliceOffsets(
	const int32 UnitIndex,
	const int32 MiddleIndex,
	const float InterUnitSpacing,
	const float CurvatureAngle,
	float& OutLateralOffset,
	float& OutYawOffsetDegrees
) const -> void
{
	// two “center” slots:
	if (UnitIndex == MiddleIndex - 1)
	{
		OutLateralOffset = -0.5f * InterUnitSpacing;
		OutYawOffsetDegrees = -CurvatureAngle;
	}
	else if (UnitIndex == MiddleIndex)
	{
		OutLateralOffset = +0.5f * InterUnitSpacing;
		OutYawOffsetDegrees = +CurvatureAngle;
	}
	else
	{
		// outward layers beyond the center pair
		const int32 LayerDepth = (UnitIndex < MiddleIndex - 1)
			                         ? (MiddleIndex - 1 - UnitIndex)
			                         : (UnitIndex - MiddleIndex);

		const float DirectionSign = (UnitIndex < MiddleIndex - 1) ? -1.f : +1.f;
		OutLateralOffset = DirectionSign * (0.5f * InterUnitSpacing + LayerDepth * InterUnitSpacing);
		OutYawOffsetDegrees = DirectionSign * (CurvatureAngle * (LayerDepth + 1));
	}
}

auto UFormationController::ComputeOddSliceOffsets(
	const int32 UnitIndex,
	const int32 MiddleIndex,
	const float InterUnitSpacing,
	const float CurvatureAngle,
	float& OutLateralOffset,
	float& OutYawOffsetDegrees
) const -> void
{
	if (UnitIndex == MiddleIndex)
	{
		// exact center: no offset
		OutLateralOffset = 0.f;
		OutYawOffsetDegrees = 0.f;
	}
	else
	{
		const int32 LayerDepth = FMath::Abs(UnitIndex - MiddleIndex);
		const float DirectionSign = (UnitIndex < MiddleIndex) ? -1.f : +1.f;

		OutLateralOffset = DirectionSign * LayerDepth * InterUnitSpacing;
		OutYawOffsetDegrees = DirectionSign * (CurvatureAngle * LayerDepth);
	}
}

FVector UFormationController::ApplyDepthSag(
	const FVector& BasePosition,
	const float YawOffsetDegrees,
	const float InterUnitSpacing,
	const FVector& ForwardAxis
) const
{
	// sag proportionally to how far the yaw deviates
	const float DepthFactor = FMath::Abs(YawOffsetDegrees) / /*max*/ InterUnitSpacing * M_SemiCircleDepthScale;
	return BasePosition - ForwardAxis * (DepthFactor * (InterUnitSpacing * 0.5f));
}


void UFormationController::DebugFormation() const
{
	constexpr float DebugDuration = 8.f;
	if (const UWorld* World = GetWorld())
	{
		// For each umbrella unit type
		for (const auto& TypePair : M_TPositionsPerUnitType)
		{
			bool bHaveFirst = false;
			FVector FirstPos = FVector::ZeroVector;

			// Iterate all subtypes & positions
			for (const auto& SubPair : TypePair.Value)
			{
				const float Radius = SubPair.Value.Radius;
				for (const FVector& Pos : SubPair.Value.Positions)
				{
					if (not bHaveFirst)
					{
						FirstPos = Pos;
						bHaveFirst = true;
					}

					// Draw unit name + radius
					FTrainingOption Option(TypePair.Key, SubPair.Key);
					const FString Label = Option.GetDisplayName()
						+ TEXT(" -- r ")
						+ FString::SanitizeFloat(Radius);
					DrawDebugString(World, Pos + FVector(0, 0, 100), Label, nullptr, FColor::White, DebugDuration);

					// Draw line back to first point in this group
					if (Pos != FirstPos)
					{
						DrawDebugLine(World, Pos, FirstPos, FColor::White, false, DebugDuration);

						// Label the distance at midpoint
						const FVector Mid = (Pos + FirstPos) * 0.5f;
						const float Dist = FVector::Distance(Pos, FirstPos);
						DrawDebugString(
							World,
							Mid + FVector(0, 0, 100),
							FString::SanitizeFloat(Dist),
							nullptr,
							FColor::White,
							DebugDuration
						);
					}
				}
			}
		}

		// Draw formation forward arrow
		const FVector ArrowStart = M_OriginalLocation + FVector(0, 0, 50);
		const FVector ArrowEnd = ArrowStart + M_FormationRotation.Vector() * 100.f;
		DrawDebugDirectionalArrow(
			World,
			ArrowStart,
			ArrowEnd,
			3.f,
			FColor::Red,
			false,
			DebugDuration
		);
	}
}
