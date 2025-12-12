// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGData.h"
#include "RandomChoiceDistance.generated.h"

// Constants for output pin labels.
namespace PCGRandomChoiceDistanceConstants
{
	const FName ChosenEntriesLabel = TEXT("Chosen");
	const FName DiscardedEntriesLabel = TEXT("Discarded");
}

/**
 * Settings for the RandomChoiceDistance node.
 * This node randomly selects a subset of point entries from the input data,
 * subject to a minimum distance constraint: each new candidate must be at least
 * PreferredMinimalDistance away from all previously chosen points.
 *
 * The node supports two selection modes:
 * - Fixed Mode: keep exactly FixedNumber of points.
 * - Ratio Mode: keep a fixed Ratio of the input points.
 *
 * Optionally, the node can output the discarded entries as well.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGRandomChoiceDistanceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif
	virtual bool UseSeed() const override { return true; }
	virtual bool HasDynamicPins() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/** If true, a fixed number of entries is chosen; otherwise, a ratio of the input is chosen. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFixedMode = true;

	// If we cannot find enough points with the current constrains; pick any other point?
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bIfNotSatisfiedPickAny = false;


	/** When in Fixed Mode, the exact number of points to choose. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (PCG_Overridable, EditCondition = "bFixedMode", ClampMin = "0"))
	int FixedNumber = 1;

	/** When not in Fixed Mode, the ratio (0–1) of points to choose from the input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (PCG_Overridable, EditCondition = "!bFixedMode", ClampMin = "0", ClampMax = "1"))
	float Ratio = 0.5f;

	/** The preferred minimal distance between any two chosen points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "0"))
	float PreferredMinimalDistance = 100.f;

	/** If true, the node also outputs the discarded entries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOutputDiscardedEntries = true;
};

/**
 * The execution element for RandomChoiceDistance.
 * This element applies the minimal distance criterion when choosing points.
 */
class FPCGRandomChoiceDistanceElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override
	{
		return EPCGElementExecutionLoopMode::SinglePrimaryPin;
	}
};
