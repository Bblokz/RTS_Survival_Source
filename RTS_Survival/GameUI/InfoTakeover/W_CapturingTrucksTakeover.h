// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_ClipInfoTakeover.h"
#include "W_InfoTakeover.h"

#include "W_CapturingTrucksTakeover.generated.h"

class UCanvasPanel;
class UTexture2D;
class UVerticalBox;
class UW_SurrenderingTruckCountdown;
class UWidget;

/** The moving parts of the capture diagram, driven from NativeTick on real time. */
USTRUCT()
struct FCapturingTrucksDiagramParts
{
	GENERATED_BODY()

	// The real scuttle plate class the trucks show in the world, counting down in the diagram.
	UPROPERTY()
	TObjectPtr<UW_SurrenderingTruckCountdown> CountdownPlate = nullptr;

	// The squad icon that walks into the 20 m ring.
	UPROPERTY()
	TObjectPtr<UWidget> SquadIcon = nullptr;

	// Shown once the squad is inside the ring, in place of the countdown plate.
	UPROPERTY()
	TObjectPtr<UWidget> SurrenderedTag = nullptr;

	// Real seconds into the diagram loop; the takeover pauses the game, so world time would stand still.
	float LoopSeconds = 0.f;

	// The whole seconds the plate shows now; INDEX_NONE while the surrendered tag shows.
	int32 ShownCountdownSeconds = INDEX_NONE;
};

/**
 * @brief The field manual on capturing convoy trucks (Ore Mountains): a looping plain-UMG diagram of a squad walking
 * into the 20 m ring of a stopped truck while the real scuttle plate counts down, beside three steps: the 12 s
 * countdown, the squad and 60 % health rules, and capture versus destroy.
 * Opened by UInfoTakeoverSubsystem::QueueCapturingTrucksTakeover from the Show Capturing Trucks Takeover node.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_CapturingTrucksTakeover copies the spaced armour one).
 */
UCLASS()
class RTS_SURVIVAL_API UW_CapturingTrucksTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_CapturingTrucksTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds the body; call before OpenTakeover. */
	void SetupCapturingTrucks();

	static FText GetCapturingTrucksTitle();

	/** @return The rules in plain paragraphs, for the archive entry. */
	static FText GetCapturingTrucksSummary();

	/** @return True when the diagram has its countdown plate and squad icon, for the headless validation. */
	bool GetHasDiagram() const;

	/** @return The seconds the diagram's plate shows; INDEX_NONE while the truck shows as surrendered. */
	int32 GetShownCountdownSeconds() const { return M_DiagramParts.ShownCountdownSeconds; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// The squad walking into the ring; any square unit icon works.
	UPROPERTY(EditDefaultsOnly, Category = "Capturing Trucks")
	TSoftObjectPtr<UTexture2D> M_SquadIcon;

private:
	UPROPERTY()
	FCapturingTrucksDiagramParts M_DiagramParts;

	// Kept loaded while the widget lives.
	UPROPERTY()
	TObjectPtr<UTexture2D> M_LoadedSquadIcon;

	TArray<FClipTakeoverFact> MakeFacts() const;
	void BuildBody();
	UWidget* BuildDiagramCard();
	UWidget* BuildDiagram();
	UWidget* BuildFactsCard();
	UWidget* WrapInGlassPane(UWidget* Content);
	void AddFactText(UVerticalBox& FactsColumn, const FClipTakeoverFact& Fact, const bool bIsFirst) const;

	void AddRoadAndTruck(UCanvasPanel& Canvas) const;
	void AddCaptureRing(UCanvasPanel& Canvas) const;
	void AddHealthBar(UCanvasPanel& Canvas) const;
	void AddSquadIcon(UCanvasPanel& Canvas);
	void AddCountdownPlate(UCanvasPanel& Canvas);
	void AddSurrenderedTag(UCanvasPanel& Canvas);

	/**
	 * @brief Places a plain filled box on the diagram canvas.
	 * @param Canvas The diagram canvas.
	 * @param Position Top left in diagram units.
	 * @param Size Size in diagram units.
	 * @param Brush The box's brush (colour or rounded box).
	 * @return The placed border, for content.
	 */
	UWidget* AddBox(UCanvasPanel& Canvas, const FVector2D& Position, const FVector2D& Size,
	                const FSlateBrush& Brush) const;

	void AddDiagramText(UCanvasPanel& Canvas, const FText& Text, const FVector2D& Position, const int32 FontSize,
	                    const FLinearColor& Color) const;

	void AdvanceDiagram(const float RealDeltaSeconds);
};
