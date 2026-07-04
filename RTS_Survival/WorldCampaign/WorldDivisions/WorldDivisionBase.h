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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Campaign|World Divisions|Debug")
	FVector AdvanceTurnTargetLocation = FVector::ZeroVector;

	virtual void PostInitializeComponents() override;

	void InitializeWorldDivision(const FGuid& DivisionKey,
	                             EWorldFieldDivisions DivisionType,
	                             int32 OwningPlayer,
	                             int32 MaxStrengthPercentage,
	                             const FText& StrengthReasonText);

	void RestoreWorldDivisionSaveData(const FWorldDivisionSaveData& DivisionSaveData);
	FWorldDivisionSaveData BuildWorldDivisionSaveData() const;

	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool IssueMoveOrderToPoint(const FVector& TargetWorldPoint);

	UFUNCTION(CallInEditor, Exec, Category="World Campaign|World Divisions|Debug")
	void AdvanceTurn();

	bool MoveForTurnDistance(float DistanceBudgetOverride = -1.f);

	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount);

	void SetOwningPlayer(int32 OwningPlayer);
	void SetDivisionType(EWorldFieldDivisions DivisionType);
	void SetCurrentStrengthPercentage(int32 StrengthPercentage);
	void SetMaxStrengthPercentage(int32 StrengthPercentage);
	void SetStrengthReasonText(const FText& StrengthReasonText);

	FGuid GetDivisionKey() const { return M_DivisionKey; }
	EWorldFieldDivisions GetDivisionType() const { return M_DivisionType; }
	int32 GetOwningPlayer() const { return M_OwningPlayer; }
	bool GetHasPendingMoveOrder() const { return bM_HasPendingMoveOrder; }
	bool GetIsMoving() const;
	int32 GetCurrentStrengthPercentage() const { return M_CurrentStrengthPercentage; }
	int32 GetMaxStrengthPercentage() const { return M_MaxStrengthPercentage; }
	const FText& GetStrengthReasonText() const { return M_StrengthReasonText; }
	float GetInfluenceAreaRadius() const { return M_InfluenceAreaRadius; }
	ERTSRadiusType GetHoverRadiusType() const { return M_HoverRadiusType; }
	UWorldDivisionMovementComponent* GetWorldDivisionMovementComponent() const { return M_MovementComponent; }
	FWorldDivisionStrengthChangedDelegate& OnDivisionStrengthChanged() { return M_OnDivisionStrengthChanged; }

protected:
	void RecalculateCurrentStrengthFromEntries(int32 RemainingEntryCount, int32 StartingEntryCount);
	int32 EstimateStartingEntryCountFromCurrentStrength(int32 RemainingEntryCount) const;
	void BroadcastDivisionStrengthChanged();
	virtual void FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const;
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

	bool GetIsValidMovementComponent() const;
	bool ConsumeMovementBudget(float DistanceBudget, TArray<FVector>& OutMovementPathPoints);
	bool GetIsAdvanceTurnTargetReached() const;
	TArray<FVector> BuildPathToTargetPoint(const FVector& TargetWorldPoint) const;
	void CachePendingMoveOrder(const FVector& TargetWorldPoint, const TArray<FVector>& PathPoints);
};
