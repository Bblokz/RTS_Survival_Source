// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"

#include "PCGCreatePowerLineInVolume.generated.h"

class UStaticMesh;
class AActor;

/**
 * @brief World-axis face of the bounds volume that anchors a power-line endpoint.
 * Automatic lets the node choose (start: random face; end: the face opposite the start).
 */
UENUM(BlueprintType)
enum class EPowerLineVolumeAnchorDirection : uint8
{
	/** Choose automatically. Start becomes a random face; end becomes the face opposite the start. */
	Automatic,
	/** Anchor the endpoint on the world -X (minimum-X) face of the bounds. */
	NegativeX,
	/** Anchor the endpoint on the world +X (maximum-X) face of the bounds. */
	PositiveX,
	/** Anchor the endpoint on the world -Y (minimum-Y) face of the bounds. */
	NegativeY,
	/** Anchor the endpoint on the world +Y (maximum-Y) face of the bounds. */
	PositiveY
};

/**
 * @brief One selectable pole option for a volume power line.
 * The pole Blueprint must own an initialized UDestructibleWire component so spans can be wired.
 */
USTRUCT(BlueprintType)
struct FPowerLineVolumePoleEntry
{
	GENERATED_BODY()

	/** Blueprint actor containing an initialized UDestructibleWire component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line")
	TSoftClassPtr<AActor> PoleActor;

	/** Wire count requested for every span of the line; clamped to the pole's physical socket count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line", meta = (ClampMin = "1"))
	int32 PreferredAmountWires = 1;

	/** Local horizontal axis aligned with the line direction so differently-authored poles face consistently. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line")
	FVector Orientation = FVector::ForwardVector;

	/** Relative selection chance among the configured poles; the whole line uses the single chosen entry. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line", meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/**
 * @brief Spawns a connected power line - poles plus drooping wires - across a bounds volume between two of its faces.
 * One pole type is chosen from a weighted list per line; poles inside excluded volumes are skipped and their
 * span is wired over when short enough, otherwise the run is broken. Optional start/end anchor directions steer the route.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreatePowerLineInVolumeSettings : public UPCGSettings
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
	/** Pole options; one entry is selected by Weight and used for the entire line. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Poles")
	TArray<FPowerLineVolumePoleEntry> Poles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Poles",
		meta = (ClampMin = "100", PCG_Overridable))
	float MinimumDistanceBetweenPoles = 1000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Poles",
		meta = (ClampMin = "100", PCG_Overridable))
	float MaximumDistanceBetweenPoles = 1400.0f;

	/** Vertical offset added to every grounded pole, e.g. to sink a base slightly into the terrain. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Poles", meta = (PCG_Overridable))
	float GroundOffset = 0.0f;

	/** Mesh used by every wire span created between adjacent poles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Wires")
	TSoftObjectPtr<UStaticMesh> WireMesh;

	/** Scale applied to the shared wire mesh along every generated span. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Wires")
	FVector WireMeshScale = FVector::OneVector;

	/** Bounds face that anchors the line start. Automatic picks a random face. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Direction", meta = (PCG_Overridable))
	EPowerLineVolumeAnchorDirection StartDirection = EPowerLineVolumeAnchorDirection::Automatic;

	/** Bounds face that anchors the line end. Automatic picks the face opposite the resolved start. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Direction", meta = (PCG_Overridable))
	EPowerLineVolumeAnchorDirection EndDirection = EPowerLineVolumeAnchorDirection::Automatic;

	/** Keeps endpoints away from the bounds corners, as a fraction of the anchored face length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Direction",
		meta = (ClampMin = "0", ClampMax = "0.5", PCG_Overridable))
	float EdgeCornerInset = 0.15f;

	/** Inward push of each endpoint from the exact bounds face so the first and last pole sit inside the volume. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Direction",
		meta = (ClampMin = "0", PCG_Overridable))
	float EdgeInset = 0.0f;

	/** Extra clearance used when testing candidate pole positions against excluded volumes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Exclusions",
		meta = (ClampMin = "0", PCG_Overridable))
	float ExclusionClearance = 100.0f;

	/**
	 * When a pole is skipped inside an excluded volume the wire spans the gap, but only while the span stays
	 * within this multiple of the maximum spacing; larger gaps break the run so no wire crosses the excluded area.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Power Line|Exclusions",
		meta = (ClampMin = "1", PCG_Overridable))
	float MaximumWireSpanSpacingMultiplier = 3.0f;
};

/**
 * @brief Picks two bounds-face endpoints per input volume, walks a straight route between them,
 * spawns exclusion-aware pole actors wired to their neighbours, and emits the placed poles as points.
 */
class FPCGCreatePowerLineInVolumeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
