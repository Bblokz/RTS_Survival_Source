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

USTRUCT(Blueprintable, BlueprintType)
struct FBehaviourDescriptionStyle

{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* PanelTexture = nullptr;
	
};
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_BehaviourDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupDescription(const FBehaviourUIData& InBehaviourUIData ) const;

protected:
	// Style for different types of behaviours.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TMap<EBuffDebuffType, FBehaviourDescriptionStyle> BehaviourDescriptionStyles;
	UPROPERTY(meta = (BindWidget))
	URichTextBlock* TitleBox;
	UPROPERTY(meta = (BindWidget))
	URichTextBlock* DescriptionBox;
	UPROPERTY(meta = (BindWidget))
	URichTextBlock* TypeDescriptionBox;

	UPROPERTY(meta = (BindWidget))
	UBorder* BehaviourBorder;

private:
	void SetupLifeTimeDescription(const FBehaviourUIData& InBehaviourUIData) const;
	void SetupStyle(const FBehaviourUIData& InBehaviourUIData) const;
};
