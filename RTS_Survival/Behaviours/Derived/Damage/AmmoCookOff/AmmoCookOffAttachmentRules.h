#pragma once

#include "CoreMinimal.h"
#include "AmmoCookOffAttachmentRules.generated.h"

UENUM(BlueprintType)
enum class EAmmoCookOffAttachmentMode : uint8
{
	TurretSocket UMETA(DisplayName="First Turret Socket"),
	TankPivotOffset UMETA(DisplayName="Tank Pivot Offset")
};

/** Selects a socket on the first mounted turret or an offset measured from the tank actor's pivot. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FAmmoCookOffAttachmentRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo Cook Off Attachment")
	EAmmoCookOffAttachmentMode Mode = EAmmoCookOffAttachmentMode::TankPivotOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo Cook Off Attachment",
		meta=(EditCondition="Mode == EAmmoCookOffAttachmentMode::TurretSocket", EditConditionHides))
	FName TurretSocketName = NAME_None;

	// Tank-local offset from the actor pivot, converted to hull-relative coordinates when attached.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo Cook Off Attachment",
		meta=(EditCondition="Mode == EAmmoCookOffAttachmentMode::TankPivotOffset", EditConditionHides))
	FVector TankPivotOffset = FVector::ZeroVector;
};
