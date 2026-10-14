// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "W_BehaviourDescription.generated.h"

enum class EBuffDebuffType : uint8;
class UBorder;
enum class EBehaviourLifeTime : uint8;
class URichTextBlock;
class UMaterialInterface;
class UTexture2D;

USTRUCT(Blueprintable, BlueprintType)
struct FBehaviourDescriptionStyle

{
	GENERATED_BODY()
	// Used when no panel material is set; drawn stretched over the whole description.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> PanelTexture = nullptr;

	// Slice panel (custom_parts.py), preferred over PanelTexture: drawn tiled at PanelBrushSize so its rim and
	// corners keep their size however large the description grows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UMaterialInterface> PanelMaterial = nullptr;

	// Must match the BrushSize the panel material instance was made for (its MI9_<name>_<W>x<H> name).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D PanelBrushSize = FVector2D(64.0, 64.0);
};
/**
 * @brief Hover description widget used by the behaviour container to show details for the active behaviour icon.
 */
UCLASS()
class RTS_SURVIVAL_API UW_BehaviourDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupDescription(const FBehaviourUIData& InBehaviourUIData);

protected:
	// Style for different types of behaviours.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TMap<EBuffDebuffType, FBehaviourDescriptionStyle> BehaviourDescriptionStyles;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> TitleBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> DescriptionBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> TypeDescriptionBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> BehaviourBorder;

private:
	UPROPERTY()
	TObjectPtr<UTexture2D> M_AppliedPanelTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_AppliedPanelMaterial = nullptr;

	void SetupLifeTimeDescription(const FBehaviourUIData& InBehaviourUIData) const;
	void SetupStyle(const FBehaviourUIData& InBehaviourUIData);
	void ClearBehaviourPanelBrush();
	void ApplyPanelMaterial(const FBehaviourDescriptionStyle& Style);
};
