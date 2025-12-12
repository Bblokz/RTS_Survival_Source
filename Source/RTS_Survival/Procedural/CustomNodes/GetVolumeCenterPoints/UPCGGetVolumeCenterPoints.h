// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGData.h"
#include "UPCGGetVolumeCenterPoints.generated.h"

/**
 * @brief Settings for the PCGGetVolumeCenterPoints node.
 *
 * This node takes spatial volume data as input and returns the center points of each volume.
 * The computed center points are clamped within the user-specified bounds.
 *
 * Parameters:
 * - CenterBoundsMin: The minimum bounds for the computed center points.
 * - CenterBoundsMax: The maximum bounds for the computed center points.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGGetVolumeCenterPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * @brief Retrieves the default node name.
	 * @return The default node name.
	 */
	virtual FName GetDefaultNodeName() const override;

	/**
	 * @brief Retrieves the default node title.
	 * @return The default node title.
	 */
	virtual FText GetDefaultNodeTitle() const override;

	/**
	 * @brief Retrieves the node tooltip text.
	 * @return The node tooltip text.
	 */
	virtual FText GetNodeTooltipText() const override;

	/**
	 * @brief Retrieves the type of the settings.
	 * @return The settings type.
	 */
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	/**
	 * @brief Indicates whether the node has dynamic pins.
	 * @return false, as this node does not have dynamic pins.
	 */
	virtual bool HasDynamicPins() const override { return false; }

	/**
	 * @brief Creates the execution element for this node.
	 * @return A shared pointer to the new execution element.
	 */
	virtual FPCGElementPtr CreateElement() const override;

protected:
	/**
	 * @brief Defines the input pin properties.
	 * @return An array of input pin properties. The default input expects Spatial (volume) data.
	 */
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	/**
	 * @brief Defines the output pin properties.
	 * @return An array of output pin properties. The default output produces Point data.
	 */
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

public:
	/** 
	 * @brief The minimum bounds for clamping the center points.
	 * Default is (-10000, -10000, -10000).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FVector CenterBoundsMin = FVector(-500.f, -500.f, -500.f);

	/** 
	 * @brief The maximum bounds for clamping the center points.
	 * Default is (10000, 10000, 10000).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FVector CenterBoundsMax = FVector(-500.f, -500.f, -500.f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float ZValueOfCenter = 115.f;

};

/**
 * @brief Execution element for PCGGetVolumeCenterPoints.
 *
 * Processes input spatial volume data by computing the center point of each volume,
 * clamping the result within the specified bounds, and outputting point data.
 */
class FPCGGetVolumeCenterPointsElement : public IPCGElement
{
protected:
	/**
	 * @brief Executes the node's logic.
	 *
	 * Processes each input spatial volume by computing its center,
	 * clamping it within the specified bounds, and converting the result into point data.
	 *
	 * @param Context The PCG context containing input and output data.
	 * @return true if the execution succeeds; false otherwise.
	 */
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
