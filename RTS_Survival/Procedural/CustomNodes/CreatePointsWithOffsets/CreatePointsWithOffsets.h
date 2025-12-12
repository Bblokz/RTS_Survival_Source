#pragma once
#include "PCGSettings.h"

#include "PCGData.h"
#include "CreatePointsWithOffsets.generated.h"

USTRUCT(BlueprintType)
struct FPCGPointOffsetToCreate
{
	GENERATED_BODY()
	// Offset from the original point
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (ClampMin = "0"))
	float Offset = 0.f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	float Angle = 0.f;

	// Added yaw to the original point rotation.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	float PointAddedYaw = 0.f;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	FVector Bounds = FVector(100.f, 100.f, 100.f);
};

UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreatePointsWithOffetsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// Each element in this array denotes a new point to create out of the provided point(s).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TArray<FPCGPointOffsetToCreate> PointsToCreate;

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif
	virtual bool UseSeed() const override { return true; }
	virtual bool HasDynamicPins() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface
};

class FPCGCreatePointsWithOffsetsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
