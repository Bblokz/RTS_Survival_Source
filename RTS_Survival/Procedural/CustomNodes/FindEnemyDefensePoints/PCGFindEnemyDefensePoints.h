// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGFindEnemyDefensePoints.generated.h"

UENUM(BlueprintType)
enum class EDefensePointStrategy : uint8
{
	RectangularSymmetrical UMETA(DisplayName = "Rectangular Symmetrical")
};

/**
 * @brief Configures the selection of a collision-free defensive formation from candidate points.
 * The formation is centered on the EnemyBase input while unused, unobstructed candidates remain available downstream.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGFindEnemyDefensePointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	virtual bool HasDynamicPins() const override { return false; }
	virtual bool UseSeed() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Maximum number of points to place while preserving the selected formation strategy. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (ClampMin = "0", PCG_Overridable, DisplayName = "Amount Of Defense Points To Aim For"))
	int32 AmountOfDefensePointsToAimFor = 8;

	/** Formation rule used to select points around the EnemyBase bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EDefensePointStrategy DefenseStrategy = EDefensePointStrategy::RectangularSymmetrical;

	/** XY collision radius assigned to defense points and used by all overlap tests. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (ClampMin = "0", Units = "cm", PCG_Overridable))
	float DefensePointRadius = 200.0f;
};

/**
 * @brief Selects symmetric defense points and separates remaining candidates that are clear of exclusions and the formation.
 */
class FPCGFindEnemyDefensePointsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
