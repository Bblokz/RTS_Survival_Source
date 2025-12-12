// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_PlayerResource.generated.h"

class UW_ResourceDescription;
class URichTextBlock;
class UPlayerResourceManager;
enum class ERTSResourceType : uint8;
class UButton;
class UImage;

UCLASS()
class RTS_SURVIVAL_API UW_PlayerResource : public UUserWidget
{
	GENERATED_BODY()

public:
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

private:
	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> PlayerResourceManager;

		/** Timer handle for updating Info widget of this resource. */
    	FTimerHandle ResourceUpdateTimerHandle;

	void SetTextOnDescriptionWidget() const;



};
