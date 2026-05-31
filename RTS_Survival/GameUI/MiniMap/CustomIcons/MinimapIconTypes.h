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
	DefendTarget UMETA(DisplayName = "Defend Target"),
	MoveTarget UMETA(DisplayName = "Move Target"),
	ResourceTarget UMETA(DisplayName = "Resource Target"),
	Warning UMETA(DisplayName = "Warning"),
};

USTRUCT(BlueprintType)
struct FMinimapIcon
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniMap", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float M_SizeXY = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniMap")
	TObjectPtr<UTexture2D> M_Texture = nullptr;
};

struct FRTSMinimapCustomIconDrawData
{
	FVector2D M_UV = FVector2D::ZeroVector;
	float M_IconSizePixels = 0.0f;
	float M_RotationDegrees = 0.0f;
	EMinimapIconType M_IconType = EMinimapIconType::None;
};

struct FRTSMinimapIconBrushData
{
	EMinimapIconType M_IconType = EMinimapIconType::None;
	TWeakObjectPtr<UTexture2D> M_Texture = nullptr;
};
