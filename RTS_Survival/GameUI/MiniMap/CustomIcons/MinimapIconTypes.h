// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "MinimapIconTypes.generated.h"


UENUM(BlueprintType)
enum class EMinimapIconType : uint8
{
	None UMETA(DisplayName = "None"),
	Objective UMETA(DisplayName = "Objective"),
	AttackTarget UMETA(DisplayName = "Attack Target"),
	AttackArrow UMETA(DisplayName = "Attack Arrow"),
	EnemyBoss UMETA(DisplayName = "Enemy Boss"),
	Warning UMETA(DisplayName = "Warning"),
	Scavenge,
	Train UMETA(DisplayName = "Train"),
	// Survival waves: a chevron drawn pointing along +X that rotates with the wave's heading, and an upright badge.
	IncomingWave UMETA(DisplayName = "Incoming Wave"),
	IncomingWaveBadge UMETA(DisplayName = "Incoming Wave Badge"),
	// Supply runs: an upright side view of the supply truck, and one upright flag per faction that can hold a
	// supply point (the point shows the flag of its holder).
	SupplyTruck UMETA(DisplayName = "Supply Truck"),
	SupplyFlag_DirectorOfShield UMETA(DisplayName = "Supply Flag Director of Shield"),
	SupplyFlag_DirectorOfForge UMETA(DisplayName = "Supply Flag Director of Forge"),
	SupplyFlag_DirectorOfHarvest UMETA(DisplayName = "Supply Flag Director of Harvest"),
	SupplyFlag_GerBreakthrough UMETA(DisplayName = "Supply Flag Ger Breakthrough"),
	SupplyFlag_GerStrikeDivision UMETA(DisplayName = "Supply Flag Ger Strike Division"),
	SupplyFlag_GerItalian UMETA(DisplayName = "Supply Flag Ger Italian"),
	// Direction-finder stations (Oder-Warthe): the station's loop aerial with the flag of its holder, upright.
	DFStation_DirectorOfShield UMETA(DisplayName = "DF Station Director of Shield"),
	DFStation_DirectorOfForge UMETA(DisplayName = "DF Station Director of Forge"),
	DFStation_GerBreakthrough UMETA(DisplayName = "DF Station Ger Breakthrough"),
	DFStation_GerStrikeDivision UMETA(DisplayName = "DF Station Ger Strike Division"),
};

/** @return True for icons drawn as upright pictures (side views, flags): they must not turn with the minimap. */
inline bool Global_GetIsUprightMinimapIcon(const EMinimapIconType IconType)
{
	switch (IconType)
	{
	case EMinimapIconType::SupplyTruck:
	case EMinimapIconType::SupplyFlag_DirectorOfShield:
	case EMinimapIconType::SupplyFlag_DirectorOfForge:
	case EMinimapIconType::SupplyFlag_DirectorOfHarvest:
	case EMinimapIconType::SupplyFlag_GerBreakthrough:
	case EMinimapIconType::SupplyFlag_GerStrikeDivision:
	case EMinimapIconType::SupplyFlag_GerItalian:
	case EMinimapIconType::DFStation_DirectorOfShield:
	case EMinimapIconType::DFStation_DirectorOfForge:
	case EMinimapIconType::DFStation_GerBreakthrough:
	case EMinimapIconType::DFStation_GerStrikeDivision:
		return true;
	default:
		return false;
	}
}

USTRUCT(BlueprintType)
struct FMinimapIcon
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniMap", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float M_SizeXY = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniMap")
	TObjectPtr<UTexture2D> M_Texture = nullptr;
};

USTRUCT(BlueprintType)
struct FMinimapIconTextPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap|Text")
	bool bAlsoWriteText = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap|Text")
	FText Text = NSLOCTEXT("MinimapIconTextPayload", "DefaultText", "Attack");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap|Text", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float TextSizePixels = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MiniMap|Text")
	FLinearColor TextColor = FLinearColor::Red;
};

struct FRTSMinimapCustomIconDrawData
{
	FVector2D M_UV = FVector2D::ZeroVector;
	float M_IconSizePixels = 0.0f;
	float M_RotationDegrees = 0.0f;
	EMinimapIconType M_IconType = EMinimapIconType::None;
};

struct FRTSMinimapTextDrawData
{
	FVector2D M_UV = FVector2D::ZeroVector;
	FText M_Text = FText::GetEmpty();
	float M_TextSizePixels = 0.0f;
	FLinearColor M_TextColor = FLinearColor::White;
};

struct FRTSMinimapIconBrushData
{
	EMinimapIconType M_IconType = EMinimapIconType::None;
	TWeakObjectPtr<UTexture2D> M_Texture = nullptr;
};
