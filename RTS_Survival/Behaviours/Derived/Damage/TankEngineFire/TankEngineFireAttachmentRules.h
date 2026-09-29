#pragma once

#include "CoreMinimal.h"
#include "TankEngineFireAttachmentRules.generated.h"

UENUM(BlueprintType)
enum class ETankEngineFireAttachmentMode : uint8
{
	HullSocket UMETA(DisplayName="Hull Socket"),
	TankPivotOffset UMETA(DisplayName="Tank Pivot Offset")
};

/** Selects a hull socket or an offset measured from the tank actor's pivot. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FTankEngineFireAttachmentRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Attachment")
	ETankEngineFireAttachmentMode Mode = ETankEngineFireAttachmentMode::TankPivotOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Attachment", meta=(EditCondition="Mode == ETankEngineFireAttachmentMode::HullSocket", EditConditionHides))
	FName HullSocketName = NAME_None;

	// Tank-local offset from the actor pivot, converted to hull-relative coordinates when attached.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Attachment", meta=(EditCondition="Mode == ETankEngineFireAttachmentMode::TankPivotOffset", EditConditionHides))
	FVector TankPivotOffset = FVector::ZeroVector;
};
