// Copyright (C) Your Company - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGData.h"
#include "PCGAssignRandomPresetZRotation.generated.h"

/**
 * @brief Settings for the AssignRandomPresetZRotation node.
 *
 * This node assigns a random preset Z rotation (in degrees) to each input point.
 * The preset rotations are provided as an array of floats. For every point, one rotation
 * is randomly picked using the PCG context’s seed.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGAssignRandomPresetZRotationSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	/**
	 * @brief Gets the default node name.
	 * @return The default node name.
	 */
	virtual FName GetDefaultNodeName() const override;

	/**
	 * @brief Gets the default node title.
	 * @return The default node title.
	 */
	virtual FText GetDefaultNodeTitle() const override;

	/**
	 * @brief Gets the node tooltip text.
	 * @return The tooltip text for the node.
	 */
	virtual FText GetNodeTooltipText() const override;

	/**
	 * @brief Gets the settings type.
	 * @return The settings type.
	 */
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }
#endif

	/**
	 * @brief Indicates that this node uses a seed for random generation.
	 * @return true.
	 */
	virtual bool UseSeed() const override { return true; }

	/**
	 * @brief Indicates that this node does not have dynamic pins.
	 * @return false.
	 */
	virtual bool HasDynamicPins() const override { return false; }

protected:
	/**
	 * @brief Defines the input pin properties.
	 * @return An array with a single input pin expecting point data.
	 */
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	/**
	 * @brief Defines the output pin properties.
	 * @return An array with a single output pin producing point data.
	 */
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	/**
	 * @brief Creates the execution element for this node.
	 * @return A shared pointer to the execution element.
	 */
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/**
	 * @brief Array of preset Z rotations (in degrees) to assign.
	 *
	 * For each point, one of these values is chosen at random and assigned as the point's Z rotation.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<float> PresetZRotations;
};

/**
 * @brief Execution element for the AssignRandomPresetZRotation node.
 *
 * This element iterates over input point data, and for each point randomly assigns
 * one of the preset Z rotations (using the context seed) to the point’s transform.
 */
class FPCGAssignRandomPresetZRotationElement : public IPCGElement
{
protected:
	/**
	 * @brief Executes the node's logic.
	 *
	 * For each input point, a random preset Z rotation is picked and applied to the point’s transform,
	 * modifying the yaw while leaving pitch and roll unchanged.
	 *
	 * @param Context The PCG context containing input and output data.
	 * @return true if execution succeeds; false otherwise.
	 */
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
