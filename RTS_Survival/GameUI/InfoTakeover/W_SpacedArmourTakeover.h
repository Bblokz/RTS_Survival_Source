// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_SpacedArmourTakeover.generated.h"

class UTexture2D;
class UVerticalBox;
class UWidget;

/** One state of a side's spaced armour, as the right column of the takeover explains it. */
USTRUCT()
struct FSpacedArmourTakeoverState
{
	GENERATED_BODY()

	// "INTACT", drawn in the state's colour.
	UPROPERTY()
	FText StateLabel;

	UPROPERTY()
	FLinearColor StateLabelColor = FLinearColor::White;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;
};

/**
 * @brief The field manual on spaced armour: a real render of a Panzer II Luchs losing a side plate to a hit, next to
 * what intact, damaged and stripped plates are worth, and that even shots that bounce off knock plates loose.
 * Opened by UInfoTakeoverSubsystem::QueueSpacedArmourTakeover from the Show Spaced Armour Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_SpacedArmourTakeover copies the vehicle takeover).
 */
UCLASS()
class RTS_SURVIVAL_API UW_SpacedArmourTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_SpacedArmourTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupSpacedArmour();

	static FText GetSpacedArmourTitle();

	/** @return What spaced armour does and how it is lost, in plain paragraphs, for the archive entry. */
	static FText GetSpacedArmourSummary();

	/** @return True when the render of the plate being shot off loaded, for the headless validation. */
	bool GetHasLoadedRender() const { return IsValid(M_LoadedRender); }

protected:
	// In-game render of a Panzer II Luchs (spaced armour) with a side plate flying off after a hit.
	UPROPERTY(EditDefaultsOnly, Category = "Spaced Armour")
	TSoftObjectPtr<UTexture2D> M_PlateShotOffRender;

private:
	// Kept loaded while the widget lives.
	UPROPERTY()
	TObjectPtr<UTexture2D> M_LoadedRender;

	TArray<FSpacedArmourTakeoverState> MakeStates() const;
	void BuildBody();
	UWidget* BuildRenderCard();
	UWidget* BuildStatesCard();
	UWidget* WrapInGlassPane(UWidget* Content);
	void AddStateText(UVerticalBox& StatesColumn, const FSpacedArmourTakeoverState& State, const bool bIsFirst) const;
};
