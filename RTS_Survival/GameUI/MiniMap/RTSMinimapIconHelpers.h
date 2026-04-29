// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTSMinimapIconHelpers.generated.h"

class AActor;
class AFowManager;
class URTSComponent;

UENUM(BlueprintType)
enum class ERTSMinimapIconColor : uint8
{
	DefaultEnemy UMETA(DisplayName = "Default Enemy"),
	BossEnemy UMETA(DisplayName = "Boss Enemy"),
	DefaultPlayerUnit UMETA(DisplayName = "Default Player Unit"),
	PlayerBuilding UMETA(DisplayName = "Player Building"),
};

USTRUCT(BlueprintType)
struct FMinimapIconSettings
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap")
	float M_IconSizePixels = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MiniMap")
	ERTSMinimapIconColor M_IconColor = ERTSMinimapIconColor::DefaultPlayerUnit;
};

struct FRTSMinimapIconDrawData
{
	FVector2D M_UV = FVector2D::ZeroVector;
	float M_IconSizePixels = 0.0f;
	FLinearColor M_IconColor = FLinearColor::White;
};

namespace FRTSMinimapIconHelpers
{
	/**
	 * @brief Resolves the icon size from the unit classification and manager defaults.
	 *
	 * Centralising these rules keeps minimap sizing consistent between icon setup and icon drawing.
	 *
	 * @param FowManager Manager containing the configured minimap icon sizes.
	 * @param OwnerActor Actor owning the Fog of War component.
	 * @param UnitType Cached unit type from the RTS component.
	 * @param UnitSubtype Cached raw unit subtype value reserved for future icon rules.
	 * @return Icon size in Slate logical pixels.
	 */
	float GetMiniMapIconSizePixels(const AFowManager& FowManager,
	                               const AActor* OwnerActor,
	                               const EAllUnitType UnitType,
	                               const uint8 UnitSubtype);

	/**
	 * @brief Resolves the icon colour bucket from ownership and unit classification.
	 *
	 * This keeps colour decisions in one place so the widget only needs to map enum buckets to actual colours.
	 *
	 * @param RTSComponent Owner RTS component containing the owning player.
	 * @param UnitType Cached unit type from the RTS component.
	 * @param UnitSubtype Cached raw unit subtype value reserved for future icon rules.
	 * @return The configured minimap icon colour bucket.
	 */
	ERTSMinimapIconColor GetMiniMapIconColor(const URTSComponent& RTSComponent,
	                                         const EAllUnitType UnitType,
	                                         const uint8 UnitSubtype);
}
