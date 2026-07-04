// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldDivisionBase.generated.h"

class UWorldDivisionInfluenceComponent;
class UWorldDivisionMovementComponent;
class AWorldDivisionBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FWorldDivisionStrengthChangedDelegate, AWorldDivisionBase*);

/**
 * @brief Runtime world-map division actor used by turn systems to move and project field strength.
 */
UCLASS(Abstract, Blueprintable)
class RTS_SURVIVAL_API AWorldDivisionBase : public AActor
{
	GENERATED_BODY()

public:
	AWorldDivisionBase();

	/**
	 * @brief Editor-only convenience target for manually advancing a division during map testing.
	 * @note AdvanceTurn uses this as a point target and still obeys DistanceTravelledPerTurn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Campaign|World Divisions|Debug")
	FVector AdvanceTurnTargetLocation = FVector::ZeroVector;

	virtual void PostInitializeComponents() override;

	/**
	 * @brief Seeds runtime identity, owner, and data-driven strength after spawn or restore.
	 * @param DivisionKey Stable save/load key; invalid keys are replaced with a new guid.
	 * @param DivisionType Field-division enum used for data lookup and class identity.
	 * @param OwningPlayer Runtime owner id used by turn movement and influence rules.
	 * @param MaxStrengthPercentage Full-composition strength loaded from WorldData.
	 * @param StrengthReasonText Player-facing reason text used by strength estimation UI.
	 */
	void InitializeWorldDivision(const FGuid& DivisionKey,
	                             EWorldFieldDivisions DivisionType,
	                             int32 OwningPlayer,
	                             int32 MaxStrengthPercentage,
	                             const FText& StrengthReasonText);

	/**
	 * @brief Restores location, pending path, owner, strength, and subclass composition from save data.
	 * @param DivisionSaveData Persistent division state captured in FWorldCampaignState.
	 */
	void RestoreWorldDivisionSaveData(const FWorldDivisionSaveData& DivisionSaveData);

	/**
	 * @brief Builds save data for current movement, strength, owner, and subclass composition.
	 * @return Serializable division state for FWorldCampaignState.
	 */
	FWorldDivisionSaveData BuildWorldDivisionSaveData() const;

	/**
	 * @brief Builds and stores a point-targeted path that can be consumed over future turns.
	 * @param TargetWorldPoint Arbitrary world-space point, not an anchor.
	 * @return true when a usable path with at least one segment was cached.
	 */
	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool IssueMoveOrderToPoint(const FVector& TargetWorldPoint);

	/**
	 * @brief Editor/debug command that consumes one turn of movement toward AdvanceTurnTargetLocation.
	 * @note Applies movement immediately so it works from CallInEditor without relying on ticking.
	 */
	UFUNCTION(CallInEditor, Exec, Category="World Campaign|World Divisions|Debug")
	void AdvanceTurn();

	/**
	 * @brief Draws the generated division path so campaign pathing can be inspected in the editor.
	 * @note Uses AdvanceTurnTargetLocation when set; otherwise draws the remaining cached move order.
	 */
	UFUNCTION(CallInEditor, Exec, Category="World Campaign|World Divisions|Debug")
	void DebugPath();

	/**
	 * @brief Consumes a single turn movement budget from the cached pending path and starts interpolation.
	 * @param DistanceBudgetOverride Optional distance for tests; negative uses DistanceTravelledPerTurn.
	 * @return true when an interpolation path was started.
	 */
	bool MoveForTurnDistance(float DistanceBudgetOverride = -1.f);

	/**
	 * @brief Applies abstract division damage; derived classes remove whole composition entries.
	 * @param DamageEntryCount Number of tank/squad entries that should be removed by the subclass.
	 */
	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount);

	/** @brief Sets runtime owner used by movement turns and influence team rules. */
	void SetOwningPlayer(int32 OwningPlayer);

	/** @brief Sets field-division enum used for identity and WorldData strength lookup. */
	void SetDivisionType(EWorldFieldDivisions DivisionType);

	/** @brief Sets current effective strength after damage or restore. */
	void SetCurrentStrengthPercentage(int32 StrengthPercentage);

	/** @brief Sets full-composition strength loaded from WorldData. */
	void SetMaxStrengthPercentage(int32 StrengthPercentage);

	/** @brief Sets player-facing strength reason text used when applying influence. */
	void SetStrengthReasonText(const FText& StrengthReasonText);

	FGuid GetDivisionKey() const { return M_DivisionKey; }
	EWorldFieldDivisions GetDivisionType() const { return M_DivisionType; }
	int32 GetOwningPlayer() const { return M_OwningPlayer; }
	bool GetHasPendingMoveOrder() const { return bM_HasPendingMoveOrder; }

	/**
	 * @brief Checks whether this division is currently playing turn movement interpolation.
	 * @return true while the movement component is consuming its visual path.
	 */
	bool GetIsMoving() const;
	int32 GetCurrentStrengthPercentage() const { return M_CurrentStrengthPercentage; }
	int32 GetMaxStrengthPercentage() const { return M_MaxStrengthPercentage; }
	const FText& GetStrengthReasonText() const { return M_StrengthReasonText; }
	float GetInfluenceAreaRadius() const { return M_InfluenceAreaRadius; }
	ERTSRadiusType GetHoverRadiusType() const { return M_HoverRadiusType; }
	UWorldDivisionMovementComponent* GetWorldDivisionMovementComponent() const { return M_MovementComponent; }
	FWorldDivisionStrengthChangedDelegate& OnDivisionStrengthChanged() { return M_OnDivisionStrengthChanged; }

protected:
	/**
	 * @brief Recomputes current strength from remaining whole entries so damage scales influence.
	 * @param RemainingEntryCount Number of tanks/squads still in the division.
	 * @param StartingEntryCount Composition count before damage started.
	 */
	void RecalculateCurrentStrengthFromEntries(int32 RemainingEntryCount, int32 StartingEntryCount);

	/**
	 * @brief Reconstructs likely starting composition count when loading an already-damaged division.
	 * @param RemainingEntryCount Number of entries restored from save data.
	 * @return Starting count estimate that preserves current/max strength ratio.
	 */
	int32 EstimateStartingEntryCountFromCurrentStrength(int32 RemainingEntryCount) const;

	/**
	 * @brief Notifies the manager that damage changed this division's field strength contribution.
	 */
	void BroadcastDivisionStrengthChanged();

	/**
	 * @brief Lets subclasses add their composition arrays to generic division save data.
	 * @param OutDivisionSaveData Save data being assembled by the base class.
	 */
	virtual void FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const;

	/**
	 * @brief Lets subclasses restore their composition arrays from generic division save data.
	 * @param DivisionSaveData Save data being restored by the base class.
	 */
	virtual void RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWorldDivisionMovementComponent> M_MovementComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWorldDivisionInfluenceComponent> M_InfluenceComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	ERTSRadiusType M_HoverRadiusType = ERTSRadiusType::FullCircle_DifficultyRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	float M_InfluenceAreaRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	float M_Speed = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	float M_DistanceTravelledPerTurn = 5000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	FGuid M_DivisionKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	EWorldFieldDivisions M_DivisionType = EWorldFieldDivisions::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	int32 M_OwningPlayer = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	int32 M_MaxStrengthPercentage = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	int32 M_CurrentStrengthPercentage = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	FText M_StrengthReasonText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	FVector M_PendingTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TArray<FVector> M_PathPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	int32 M_CurrentPathPointIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	bool bM_HasPendingMoveOrder = false;

	FWorldDivisionStrengthChangedDelegate M_OnDivisionStrengthChanged;

	/**
	 * @brief Validates the owned movement component before starting turn movement.
	 * @return true when the component exists.
	 */
	bool GetIsValidMovementComponent() const;

	/**
	 * @brief Advances along the cached path without deciding whether movement should be visual or instant.
	 * @param DistanceBudget Maximum XY distance to consume this turn.
	 * @param OutMovementPathPoints Path segment consumed during this pass, including current location.
	 * @return true when at least one movement segment was consumed.
	 */
	bool ConsumeMovementBudget(float DistanceBudget, TArray<FVector>& OutMovementPathPoints);

	/**
	 * @brief Tests the debug target in XY so editor AdvanceTurn can stop at the requested point.
	 * @return true when the actor is already effectively at AdvanceTurnTargetLocation.
	 */
	bool GetIsAdvanceTurnTargetReached() const;

	/**
	 * @brief Builds a world-space Unreal path to a point, then repairs it against campaign constraints.
	 * @param TargetWorldPoint Arbitrary world point requested by code, Blueprint, or editor testing.
	 * @return Path points clamped inside the boundary and routed around anchors where possible.
	 */
	TArray<FVector> BuildPathToTargetPoint(const FVector& TargetWorldPoint) const;

	/**
	 * @brief Renders world-space path segments with fixed editor debug styling.
	 * @param PathPoints Ordered points forming the path that should be visualized.
	 */
	void DrawDebugPathPoints(const TArray<FVector>& PathPoints) const;

	/**
	 * @brief Stores a validated path as the authoritative pending movement order.
	 * @param TargetWorldPoint Final repaired target point.
	 * @param PathPoints World-space path points to consume over future turns.
	 */
	void CachePendingMoveOrder(const FVector& TargetWorldPoint, const TArray<FVector>& PathPoints);
};
