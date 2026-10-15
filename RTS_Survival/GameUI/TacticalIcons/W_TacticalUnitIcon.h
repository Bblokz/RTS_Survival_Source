// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TacticalUnitIconSettings.h"

#include "W_TacticalUnitIcon.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USizeBox;

/**
 * @brief One tactical icon behind lit, scratched glass (M_UI_TacticalIconGlass). Builds its own widget tree, so it
 * can be placed in a designer or constructed from C++; UW_TacticalUnitIconCorners places two on a unit portrait.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TacticalUnitIcon : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_TacticalUnitIcon(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;

	void SetupTacticalIcon(const FTacticalUnitIconSettings& Settings);
	void SetIconSize(const float IconSize);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "TacticalIcon")
	FTacticalUnitIconSettings M_Settings;

	// Edge length of the square plate in UI units.
	UPROPERTY(EditAnywhere, Category = "TacticalIcon", meta = (ClampMin = "8.0"))
	float M_IconSize = 32.0f;

	UPROPERTY(EditAnywhere, Category = "TacticalIcon")
	TSoftObjectPtr<UMaterialInterface> M_GlassMaterial;

private:
	UPROPERTY()
	TObjectPtr<USizeBox> M_SizeBox;

	UPROPERTY()
	TObjectPtr<UImage> M_GlassImage;

	// Created on first use from M_GlassMaterial; carries the icon texture and light colour of M_Settings.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_GlassMaterialInstance;

	void BuildWidgetTree();
	void ApplySettings();
	void ApplyIconSize() const;
	bool EnsureGlassMaterialInstance();

	bool GetIsValidGlassImage() const;
	bool GetIsValidSizeBox() const;
};
