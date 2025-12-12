#pragma once

#include "CoreMinimal.h"
#include "FormationTypes.h"
#include "FormationPositionEffects/PlayerFormationPositionEffects.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "FormationMovement.generated.h"

class AFormationEffectActor;
class RTS_SURVIVAL_API ASelectablePawnMaster;
class RTS_SURVIVAL_API ASquadController;
class RTS_SURVIVAL_API ASelectableActorObjectsMaster;
class RTS_SURVIVAL_API URTSComponent;
enum class EAllUnitType : uint8;

/**
 * @brief Holds unit type, subtype, and formation radius for calculations.
 */
struct FUnitData
{
	EAllUnitType UnitType;
	int32 UnitSubType;
	float FormationRadius;

	/** @note World‐space location at start of formation, used to minimize crossing. */
	FVector OriginalLocation;
};


/**
 * @brief Stores circle radius and generated positions for a specific subtype.
 */
struct FSubtypeFormationData
{
	float Radius = 0.0f;
	TArray<FVector> Positions;
	TArray<FRotator> Rotations;
};

/**
 * @brief Controller component that computes and displays unit formations when issuing move commands.
 *        Always call InitiateMovement before UseFormationPosition.
 */
UCLASS()
class RTS_SURVIVAL_API UFormationController : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	UFormationController();

	/**
	 * @brief Initializes with the shared effects struct.
	 * @param InFormationEffectsStruct Shared pointer to player formation effects settings.
	 */
	void InitFormationController(const TSharedPtr<FPlayerFormationPositionEffects>& InFormationEffectsStruct);
	bool IsPlayerRotationOverrideActive() const;

	/**
	 * @brief Computes formation positions and spawns visual effects.
	 * @param MoveLocation World-space target location.
	 * @param SelectedSquads Array of selected squads.
	 * @param SelectedPawns Array of selected pawns.
	 * @param SelectedActorMasters Array of other selectable actors.
	 */
	void InitiateMovement(
		const FVector& MoveLocation,
		const TArray<ASquadController*>* SelectedSquads,
		const TArray<ASelectablePawnMaster*>* SelectedPawns,
		const TArray<ASelectableActorObjectsMaster*>* SelectedActorMasters
	);

	/**
	 * @brief Activates the formation-picker UI at the given screen position.
	 * @param MousePosition Screen-space mouse coordinates.
	 * @return true if activated successfully.
	 */
	bool ActivateFormationPicker(const FVector2D& MousePosition) const;

	/** Projects a primary click into the formation-picker, updating the shape. */
	void PrimaryClickPickFormation(const FVector2D& ClickedPosition);

/**
 * @brief Retrieves the next formation slot for a unit, or returns the actor's current location
 *        if that actor does not implement ICommands or lacks the movement ability.
 * @param RTSComponent   RTS component of the querying unit (used to resolve type/subtype).
 * @param OutMovementRotation  Returned movement rotation for this slot (unchanged when falling back).
 * @param OwningActorForCommands The actor we attempt to query for ICommands & movement ability. If
 *                               absent/invalid/non-movable, we return its current location instead
 *                               of popping from the formation, preserving formation integrity.
 * @return World-space target location for the unit; either a formation position or the actor’s current location.
 */
FVector UseFormationPosition(const URTSComponent* RTSComponent,
                             FRotator& OutMovementRotation,
                             AActor* OwningActorForCommands);

	void OnMovementFinished();

	/** Overrides automatic formation rotation with a player-specified value. */
	void SetPlayerRotationOverride(const FRotator& InRotation);

protected:
	virtual void BeginPlay() override;

private:

	/**
     * @brief Build “movable” views of the selection arrays without mutating the originals.
     * @param SelectedSquads        All selected squads.
     * @param SelectedPawns         All selected pawns.
     * @param SelectedActorMasters  All selected other actors.
     * @param OutMovableSquads      Receives only squads that can execute a move command.
     * @param OutMovablePawns       Receives only pawns that can execute a move command.
     * @param OutMovableActors      Receives only actors that can execute a move command.
     */
    void BuildMovableSelections(const TArray<ASquadController*>& SelectedSquads,
                                const TArray<ASelectablePawnMaster*>& SelectedPawns,
                                const TArray<ASelectableActorObjectsMaster*>& SelectedActorMasters,
                                TArray<ASquadController*>& OutMovableSquads,
                                TArray<ASelectablePawnMaster*>& OutMovablePawns,
                                TArray<ASelectableActorObjectsMaster*>& OutMovableActors) const;

	    /** @brief True if Actor implements ICommands and has the movement ability. */
        bool GetActorHasMovementAbility(AActor* Actor) const;
	
	/** Shared configuration for spawnable formation effects. */
	TSharedPtr<FPlayerFormationPositionEffects> M_FormationEffectsStructReference = nullptr;

	/** Pooled actors used to display formation positions. */
	UPROPERTY()
	TArray<TObjectPtr<AFormationEffectActor>> M_FormationEffectActors;

	/** Widget used to pick formation shapes. */
	UPROPERTY()
	TObjectPtr<UW_FormationPicker> M_FormationPickerWidget;

	/** Timer handle to hide effects after duration. */
	FTimerHandle M_FormationEffectDurationTimer;

	/** Ratio of rows-to-columns when building rectangular formations. */
	int32 M_RowsToColumnsRatio = 1;

	/** Current formation shape enum. */
	EFormation M_CurrentFormation = EFormation::RectangleFormation;

	/** Original world-space click location. */
	FVector M_OriginalLocation;

	/** Computed rotation for the formation. */
	FRotator M_FormationRotation;

	/** If true, use override rotation instead of computed. */
	bool bM_PlayerRotationOverride = false;

	/** Player-specified override rotation. */
	FRotator M_PlayerRotationOverride;

	/** Vertical offset applied to effect actors. */
	UPROPERTY()
	FVector M_PositionEffectOffset = FVector(0.f, 0.f, 100.f);

	/** Maps each unit type to priority (front-to-back). */
	static TMap<EAllUnitType, int32> M_TUnitTypePriority;

	/**
	 * @brief Container mapping unit types & subtypes to formation positions and radii.
	 * @note First key: umbrella type, second key: subtype.
	 */
	TMap<EAllUnitType, TMap<int32, FSubtypeFormationData>> M_TPositionsPerUnitType;

	EFormationEffect GetFormationEffectFromFormation(EFormation Formation) const;
	// Draws the positions in different ways depending on whether the formation rotation arrow was used to orient this
	// formation: draw arrow. Or if this was a regular formation: draw position marker.
	void DrawFormationPositionEffects();

	/** Validates and reports error if effects struct ptr is null. */
	bool GetIsValidFormationEffectsStruct() const;

	/** Validates and reports error if picker widget ptr is invalid. */
	bool GetIsValidFormationPickerWidget() const;

	/** Creates the formation-picker widget during BeginPlay. */
	void BeginPlay_CreateFormationWidget();

	/** Spawns and caches the pooled effect actors. */
	void BeginPlay_CreateEffectPool();

	/** Handles rectangle formation generation. */
	void CreateRectangleFormation(
		const TArray<ASquadController*>& Squads,
		const TArray<ASelectablePawnMaster*>& Pawns,
		const TArray<ASelectableActorObjectsMaster*>& Actors
	);

	/** Gathers unit data, sorts by priority, computes average location. */
	void GatherAndSortUnits(
		const TArray<ASquadController*>& Squads,
		const TArray<ASelectablePawnMaster*>& Pawns,
		const TArray<ASelectableActorObjectsMaster*>& Actors,
		TArray<FUnitData>& OutUnits,
		FVector& OutAverageSelectedUnitsLocation
	) const;

	/** Builds positions for a rectangular formation. */
	void BuildRectangleFormationPositions(const TArray<FUnitData>& Units, const FVector& AverageUnitsLocation);

	/** Flattens all stored positions into a single array. */
	TArray<FVector> GatherAllFormationPositions(TArray<FRotator>& OutAllRotations) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ Rectangle Formation Helpers ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	/** Determines number of columns & rows for rectangle formation. */
	void DetermineGridDimensions(int32 TotalUnits, int32& OutColumns, int32& OutRows) const;

	/** Groups flat unit list into rows of given column count. */
	TArray<TArray<FUnitData>> GroupUnitsIntoRows(const TArray<FUnitData>& Units, int32 Columns) const;

	/** Computes formation axes (forward & right) based on click-to-centroid. */
	void ComputeFormationAxes(const FVector& AverageUnitsLocation, FVector& OutForward, FVector& OutRight);

	/**
	 * @brief Calculates per-row horizontal offsets and records each row's maximum unit radius.
	 *
	 * @param AllRows         Array of rows, each containing unit data for that row.
	 * @param RightVector     Formation’s right-axis vector used for horizontal placement.
	 * @param OutLocalPos     Receives relative positions along each row for every unit.
	 * @param OutMaxRadius    Receives the largest formation radius found in each row.
	 */
	void ComputeLocalRowPositions(
		const TArray<TArray<FUnitData>>& AllRows,
		const FVector& RightVector,
		TArray<TArray<FVector>>& OutLocalPos,
		TArray<float>& OutMaxRadius
	) const;

	/**
	 * @brief Computes world-space origins for each row to ensure rows do not overlap vertically.
	 *
	 * @param MaxRadiusPerRow    Maximum unit radius per row, for spacing calculation.
	 * @param ForwardVector      Formation’s forward-axis vector used for vertical placement.
	 * @param OutGlobalOffsets   Receives world-space origin location for each row.
	 */
	void ComputeRowGlobalOffsets(
		const TArray<float>& MaxRadiusPerRow,
		const FVector& ForwardVector,
		TArray<FVector>& OutGlobalOffsets
	) const;

	/**
	 * @brief Combines per-row origins and local offsets to generate final world-space positions.
	 *
	 * @param AllRows         Array of rows with their unit data.
	 * @param LocalPos        Local offsets per unit within each row (relative to row origin).
	 * @param GlobalOffsets   World-space row origins computed to avoid overlap.
	 * @post Each computed position is added to the formation data map via AddPositionForUnitToFormation().
	 */
	void StoreRectangleFormationPositions(
		const TArray<TArray<FUnitData>>& AllRows,
		const TArray<TArray<FVector>>& LocalPos,
		const TArray<FVector>& GlobalOffsets
	);


	/**
	 * @brief Sorts a row’s units so that those originally on the same lateral side remain on that side.
	 * @param RowRows                The row’s unit data to sort in-place.
	 * @param RightVector            The formation’s right-axis vector.
	 * @param AverageUnitsLocation   Centroid of all units—used to determine movement direction.
	 */
	void SortRowUnitsByOriginalPosition(
		TArray<FUnitData>& RowRows,
		const FVector& RightVector,
		const FVector& AverageUnitsLocation
	) const;


	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ END Rectangle Formation Helpers ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	/** Moves pooled actors to Positions and shows them. */
	void ShowEffectsAtPositions(const TArray<FVector>& Positions, const EFormationEffect EffectType, const TArray<FRotator>& Rotations);

	/** Hides any pooled actors from StartIndex onward. */
	void HideRemainingEffects(int32 StartIndex);

	/**
	 * @brief Adds a single position+rotation for a given type & subtype.
	 * @param UnitType   Umbrella unit type.
	 * @param UnitSubType Sub‐type index.
	 * @param Position   World‐space slot location.
	 * @param Radius     Unit’s formation radius (for future spacing).
	 * @param Rotation   Movement‐rotation to apply when the unit moves there.
	 */
	void AddPositionForUnitToFormation(EAllUnitType UnitType, int32 UnitSubType, const FVector& Position, float Radius,
	                                   const FRotator& Rotation);

	/** Reports an error for missing data and returns zero vector. */
	FVector ReportAndReturnError(EAllUnitType UnitType, int32 UnitSubType, const FString& ErrorMessage) const;


	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ Effect Hiding Helpers ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	 * @brief Schedules hiding of pooled formation‐effect actors after the configured duration.
	 *        Clears any existing timer and then sets a new one if the duration is valid.
	 */
	void ScheduleFormationEffectHiding();

	/**
	 * @brief Retrieves and validates the effect‐hiding duration.
	 * @param OutDuration Receives the duration in seconds if valid.
	 * @return true if Duration > 0 and struct is valid; false otherwise.
	 */
	bool TryGetEffectHideDuration(float& OutDuration) const;

	/**
	 * @brief Constructs a timer delegate that hides all pooled effect actors when fired.
	 * @return Configured FTimerDelegate.
	 */
	FTimerDelegate MakeHideEffectsDelegate();

	/**
	 * @brief Iterates over all pooled formation‐effect actors and hides them, reporting errors on invalid pointers.
	 */
	void HideAllPooledEffectActors();

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ END Effect Hiding Helpers ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ Spear Formation Helpers ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	 * @brief Generates a spear (wedge) formation of the given thickness.
	 * @param Squads         Selected squad controllers.
	 * @param Pawns          Selected pawn masters.
	 * @param Actors         Selected other actor masters.
	 * @param SpearThickness Desired max width of each spear row; minimum is 3.
	 */
	void CreateSpearFormation(
		const TArray<ASquadController*>& Squads,
		const TArray<ASelectablePawnMaster*>& Pawns,
		const TArray<ASelectableActorObjectsMaster*>& Actors,
		int32 SpearThickness = 3
	);

	/**
	 * @brief Ensures thickness is at least 3, clamping and notifying the player if reduced.
	 * @param Thickness In/out thickness to validate.
	 */
	void ValidateSpearThickness(int32& Thickness) const;

	/**
	 * @brief Computes spacing between two successive spear rows so their circles don't overlap.
	 * @param PrevRow  The units in the previous row.
	 * @param CurRow   The units in the current row.
	 * @return         Distance between those two row centers.
	 */
	float ComputeRowSpacing(const TArray<FUnitData>& PrevRow, const TArray<FUnitData>& CurRow) const;

	/**
	 * @brief Computes the horizontal spacing between units in the same row.
	 * @param Row      The units in that row.
	 * @return         Distance between adjacent unit centers.
	 */
	float ComputeIntraRowSpacing(const TArray<FUnitData>& Row) const;

	/**
	 * @brief Builds the world-space positions for all units in a spear formation.
	 * @param Units       Sorted unit data (in priority order).
	 * @param Thickness   Max units per row.
	 * @param ForwardAxis Formation forward axis.
	 * @param RightAxis   Formation right axis.
	 * @return            Array of positions matching Units order.
	 */
	TArray<FVector> GenerateSpearFormationPositions(
		const TArray<FUnitData>& Units,
		int32 Thickness,
		const FVector& ForwardAxis,
		const FVector& RightAxis
	) const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ END Spear Formation Helpers ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // ++++++++++++++++++++++++ Semi-Circle Formation Helpers ++++++++++++++++++++++++++++++++
    ////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
     * @brief Generates a semi-circle (fan) formation in one or more arcs.
     * @param Squads         Selected squad controllers.
     * @param Pawns          Selected pawn masters.
     * @param Actors         Selected other actor masters.
     * @param UnitsPerSlice  Base number of units in the first slice.
     * @param CurvatureAngle Angle in degrees from center-line out to the edges of each half-arc.
     */
    void CreateSemiCircleFormation(
        const TArray<ASquadController*>& Squads,
        const TArray<ASelectablePawnMaster*>& Pawns,
        const TArray<ASelectableActorObjectsMaster*>& Actors,
        int32 UnitsPerSlice = 4,
        float CurvatureAngle = 20.f
    );


    /**
     * @brief Builds world-space positions AND rotations for a multi-slice semi-circle.
     * @param Units           Sorted unit data (in priority order).
     * @param UnitsPerSlice   Base count in the first slice.
     * @param CurvatureAngle  Degrees out to the edge of each half-arc.
     * @param ForwardAxis     Formation forward vector.
     * @param RightAxis       Formation right (lateral) vector.
     * @param OutPositions    Receives one FVector per unit.
     * @param OutRotations    Receives one FRotator per unit.
     */
    void GenerateSemiCirclePositions(
        const TArray<FUnitData>& Units,
        int32 UnitsPerSlice,
        float CurvatureAngle,
        const FVector& ForwardAxis,
        const FVector& RightAxis,
        TArray<FVector>& OutPositions,
        TArray<FRotator>& OutRotations
    ) const;


	
    /** Splits Units into successive slices of size UnitsPerSlice, then +2 each time. */
    TArray<TArray<FUnitData>> PartitionUnitsIntoSlices(
        const TArray<FUnitData>& Units,
        int32                    UnitsPerSlice
    ) const;

    /** Computes world-space center for each slice, stacked back by row-spacing. */
    TArray<FVector> ComputeSemiCircleCenters(
        const TArray<TArray<FUnitData>>& Slices,
        const FVector&                   Origin,
        const FVector&                   ForwardAxis
    ) const;

    /**
     * Places a single slice of units along a half-arc around Center.
     * Appends the resulting world positions into Out.
     */
    void PlaceUnitsInHalfArc(
	    const TArray<FUnitData>& Slice,
	    float CurvatureAngle,
	    const FVector& Center,
	    const FVector& ForwardAxis,
	    const FVector& RightAxis,
	    TArray<FVector>& OutPositions, TArray<FRotator>& OutRotations
    ) const;

	  /**
         * @brief Given a unit’s index within a slice, computes its lateral and yaw offsets along the half-arc.
         */
        void CalculateUnitArcOffsets(
            int32    UnitIndex,
            int32    SliceSize,
            float    InterUnitSpacing,
            float    CurvatureAngle,
            float&   OutLateralOffset,
            float&   OutYawOffsetDegrees
        ) const;
    
        /**
         * @brief Even-count slice: computes offsets for an “even” number of units in the slice.
         */
        void ComputeEvenSliceOffsets(
            int32    UnitIndex,
            int32    MiddleIndex,
            float    InterUnitSpacing,
            float    CurvatureAngle,
            float&   OutLateralOffset,
            float&   OutYawOffsetDegrees
        ) const;
    
        /**
         * @brief Odd-count slice: computes offsets for an “odd” number of units in the slice.
         */
        void ComputeOddSliceOffsets(
            int32    UnitIndex,
            int32    MiddleIndex,
            float    InterUnitSpacing,
            float    CurvatureAngle,
            float&   OutLateralOffset,
            float&   OutYawOffsetDegrees
        ) const;
    
        /**
         * @brief Applies the small “sag” (depth offset) based on yaw deviation.
         */
        FVector ApplyDepthSag(
            const FVector& BasePosition,
            float          YawOffsetDegrees,
            float          InterUnitSpacing,
            const FVector& ForwardAxis
        ) const;

	float M_SemiCircleDepthScale = 3.f;

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // ++++++++++++++++++++++++ END Semi-Circle Formation Helpers ++++++++++++++++++++++++++++++++
    ////////////////////////////////////////////////////////////////////////////////////////////////////
	/**
	 * @brief Draws debug strings and lines for every formation position, labeling units with their display names
	 *        and radii, and connecting each group’s points to its first point with distance labels.
	 *
	 * @note Works for any formation stored in M_TPositionsPerUnitType.
	 * @note Also draws a directional arrow from the original move location showing formation forward.
	 */
	void DebugFormation() const;
};
