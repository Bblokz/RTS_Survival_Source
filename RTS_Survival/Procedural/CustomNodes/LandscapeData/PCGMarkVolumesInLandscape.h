// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataTypes.h"

#include "PCGMarkVolumesInLandscape.generated.h"

/**
 * @brief Configures a PCG side-effect node that paints volume density into the Landscape data texture.
 * Input volumes pass through unchanged so designers can compose marking with their existing graph flow.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGMarkVolumesInLandscapeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	virtual bool UseSeed() const override;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override;
	virtual FString GetAdditionalTitleInformation() const override;
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Landscape Data", meta = (PCG_Overridable))
	ERTSLandscapeDataChannel Channel = ERTSLandscapeDataChannel::Scorched;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief Borrows volume inputs only for a synchronous density rasterization on the game thread.
 */
class FPCGMarkVolumesInLandscapeElement final : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
};
