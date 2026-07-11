// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"

#include "PCGGetReinforcementPathActorData.generated.h"

/**
 * @brief Configures a source node that emits the transforms of all reinforcement-path actors in the current world.
 * Actors derived from AReinforcementPathActor are included automatically.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGGetReinforcementPathActorDataSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::InputOutput; }
	virtual void GetStaticTrackedKeys(
		FPCGSelectionKeyToSettingsMap& OutKeysToSettings,
		TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief Finds every reinforcement-path marker in the executing world and emits one point per actor transform.
 */
class FPCGGetReinforcementPathActorDataElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
