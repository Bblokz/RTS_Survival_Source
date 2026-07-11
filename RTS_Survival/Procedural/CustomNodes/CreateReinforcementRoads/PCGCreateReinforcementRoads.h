// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"

#include "PCGCreateReinforcementRoads.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * @brief Configures managed reinforcement-road splines generated between compatible endpoints.
 * The node pairs each endpoint at most once and falls back to backup ends when no natural primary route exists.
 * @note Generated spline actors and their road meshes are owned by the executing PCG component.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateReinforcementRoadsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Strength of the decorative, snake-like displacement. Required connection turns may be sharper. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road", meta = (ClampMin = "0", PCG_Overridable))
	float Curvature = 0.5f;

	/** Approximate number of decorative bends added per 1000 Unreal units of road. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road", meta = (ClampMin = "0", PCG_Overridable))
	float AmountCurvesPer1000Units = 0.35f;

	/** Mesh deformed along every generated spline. Its local X extent determines mesh segment length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road")
	TSoftObjectPtr<UStaticMesh> RoadMesh;

	/** Optional material applied to slot zero of every generated spline mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Road")
	TSoftObjectPtr<UMaterialInterface> OverrideMaterial;
};

/**
 * @brief Selects one-use road endpoints, plans exclusion-safe natural routes, follows terrain,
 * spawns managed spline meshes, and emits the final sampled paths for downstream PCG logic.
 */
class FPCGCreateReinforcementRoadsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
