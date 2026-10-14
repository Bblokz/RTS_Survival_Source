// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "W_PlayerResource.generated.h"

class UW_ResourceDescription;
class URichTextBlock;
class UPlayerResourceManager;
class UButton;
class UImage;
class UMaterialInterface;

/**
 * @brief One resource counter (icon + amount plate) in the minimap panel; MainGameUI inits it with its resource.
 * The blueprint's SetupResource picks the flat icon; a glass icon from M_GlassIconMaterials replaces it when set.
 */
UCLASS()
class RTS_SURVIVAL_API UW_PlayerResource : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_PlayerResource(const FObjectInitializer& ObjectInitializer);

	void InitUwPlayerResource(
		UPlayerResourceManager* NewPlayerResourceManager,
		ERTSResourceType NewResourceType);

protected:

	virtual void NativeConstruct() override;
	virtual void BeginDestroy() override;
	
	// The button of this widget that is underneath the size box in the hierarchy.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_ResourceButton;

	// The image of this widget that is a child of the button.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* M_ResourceImage;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_ResourceText;

	/**
	 * Initialise the image of the resource.
	 * @param NewResourceType What type of resource this widget represents.
	 * @note The resource type is also accessible in blueprints as UPROPERTY.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "PlayerResource")
	void SetupResource(ERTSResourceType NewResourceType);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ResourceText")
	FText GetResourceText() const;

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "ResourceType")
	ERTSResourceType ResourceType;

	UPROPERTY(BlueprintReadWrite, Category = "Description")
	TObjectPtr<UW_ResourceDescription> ResourceDescriptionWidget;

	// Glass-domed icon per resource (M_UI_GlassResourceIcon instances tinted with the resource's rich text colour).
	// Resources without an entry keep the icon the blueprint's SetupResource chose.
	UPROPERTY(EditDefaultsOnly, Category = "ResourceIcon", meta = (DisplayName = "Glass Icon Materials"))
	TMap<ERTSResourceType, TSoftObjectPtr<UMaterialInterface>> M_GlassIconMaterials;

private:
	void ApplyGlassResourceIcon(const ERTSResourceType NewResourceType) const;
	bool GetIsValidResourceImage() const;

	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> PlayerResourceManager;

		/** Timer handle for updating Info widget of this resource. */
    	FTimerHandle ResourceUpdateTimerHandle;

	void SetTextOnDescriptionWidget() const;



};
