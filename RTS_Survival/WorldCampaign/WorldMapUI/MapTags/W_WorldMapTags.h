// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_WorldMapTags.generated.h"

class AWorldDivisionBase;
class AWorldMissionObject;
class AWorldPlayerController;
class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UW_WorldMapArrowLayer;
class UWidget;
class UWorldCountryOccupationRegulator;
struct FWorldCampaignUILook;
struct FWorldMapArrowPaint;
struct FWorldMapTagPlacement;

/** The name and liberation bar of one country, kept so the tick only moves it. */
USTRUCT()
struct FWorldMapCountryLabel
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UWidget> Root = nullptr;

	// Slot of the fill inside the bar's trough; its width is the liberation.
	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> FillSlot = nullptr;

	FVector LabelPoint = FVector::ZeroVector;
};

/** The tag plate of one enemy division or convoy and what its arrow points at. */
USTRUCT()
struct FWorldMapPieceTag
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AWorldDivisionBase> Piece = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> Plate = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeadlineText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> TurnsText = nullptr;

	bool bHasArrow = false;

	FVector ArrowTarget = FVector::ZeroVector;
};

/** The name plate under one named mission piece. */
USTRUCT()
struct FWorldMapMissionTag
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AWorldMissionObject> MissionObject = nullptr;

	UPROPERTY()
	TObjectPtr<UBorder> Plate = nullptr;
};

/**
 * @brief Full-screen, click-through overlay of the world map (UI sketch sheet 6 B): each country's name with a thin
 * liberation bar, a name plate under every named mission, and a tag plus a dashed arrow on every enemy division and
 * convoy that has a target. The campaign HUD
 * creates it, calls InitMapTags before adding it under its other widgets, and hides it while a full-screen page is
 * open. Builds its own widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldMapTags : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the overlay; countries and pieces appear on their own once the campaign map is ready.
	 * @param WorldPlayerController Controller that owns the campaign managers and the map camera.
	 * @param Look Shared art of the campaign screens; owned by the HUD and must outlive this widget.
	 */
	void InitMapTags(AWorldPlayerController* WorldPlayerController, const FWorldCampaignUILook* Look);

	/** Hide while a full-screen page (deck builder, tech wheel) covers the map. */
	void SetTagsVisible(const bool bVisible);

	bool GetAreTagsVisible() const { return bM_TagsVisible; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();

	/** @return The regulator on the world generator; found once and bound to its liberation changes. */
	UWorldCountryOccupationRegulator* GetOrBindCountryRegulator();
	void UnbindCountryRegulator();

	/** Creates the country labels once the regulator has mapped anchors; until then it is retried on each refresh. */
	void TryBuildCountryLabels();

	/**
	 * @brief Groups the generated anchors by the country the regulator mapped them to.
	 * @param CountryRegulator Regulator that knows each anchor's country.
	 * @param OutAnchorLocationsByCountry Receives the anchor locations per country id.
	 */
	void GatherCountryAnchorLocations(const UWorldCountryOccupationRegulator& CountryRegulator,
	                                  TMap<FName, TArray<FVector>>& OutAnchorLocationsByCountry) const;

	void CreateCountryLabel(FName CountryId, const FVector& LabelPoint, float Liberation);
	void SetCountryLiberationFill(const FWorldMapCountryLabel& CountryLabel, float Liberation) const;

	UFUNCTION()
	void HandleCountryLiberationChanged(FName CountryId, float Liberation);

	/** Adds tags for new pieces, drops tags of pieces that left, and refreshes texts and targets. */
	void RefreshPieceTags();

	/** @return Division keys of the enemy pieces that should carry a tag now. */
	TArray<FGuid> GatherTaggedPieceKeys() const;

	void CreatePieceTag(AWorldDivisionBase& Piece);
	void RemovePieceTag(const FGuid& PieceKey);
	void RefreshPieceTagContent(FWorldMapPieceTag& PieceTag) const;
	void RefreshPieceTurnsText(FWorldMapPieceTag& PieceTag, const AWorldDivisionBase& Piece) const;

	/** Reads where the player HQ stands; it is only known once the campaign map has been generated or restored. */
	void RefreshPlayerHQLocation();

	/** Adds a name plate for every named mission piece on the map and drops the plates of pieces that are gone. */
	void RefreshMissionTags();

	void CreateMissionTag(AWorldMissionObject& MissionObject, const FText& MissionName);

	/** @return false for a mission the player won, or one the fog of war still hides. */
	bool GetShowsMissionTag(const AWorldMissionObject& MissionObject) const;

	/** Projects every name, and hides what the top bar or another name would cut. Mission names win over countries. */
	void RepositionNameLabels(const FGeometry& MyGeometry);

	/**
	 * @brief Collects the mission name plates that can be drawn now, before they are weighed against other names.
	 * @param MyGeometry Geometry of this overlay.
	 * @param OutPlacements Receives one placement per plate.
	 * @param OutLabelRoots Receives the plate of each placement, same order.
	 */
	void AddMissionTagPlacements(const FGeometry& MyGeometry, TArray<FWorldMapTagPlacement>& OutPlacements,
	                             TArray<UWidget*>& OutLabelRoots);

	/**
	 * @brief Collects the country labels that can be drawn now, lifted off the player HQ and off the mission pieces
	 * with their name plates, so a country name next to them moves up instead of being hidden.
	 * @param MyGeometry Geometry of this overlay.
	 * @param OutPlacements Holds the mission plates on entry; receives one placement per label after them.
	 * @param OutLabelRoots Receives the widget of each placement, same order.
	 */
	void AddCountryLabelPlacements(const FGeometry& MyGeometry, TArray<FWorldMapTagPlacement>& OutPlacements,
	                               TArray<UWidget*>& OutLabelRoots);

	/**
	 * @brief Moves a country label up until it is clear of every mission piece and its name plate.
	 * @param LabelCenter Middle of the label where its country and the HQ put it.
	 * @param LabelSize Size of the label.
	 * @param MissionPlates Placements of the mission name plates.
	 * @return Middle of the label to use.
	 */
	FVector2D GetLabelCenterClearOfMissions(const FVector2D& LabelCenter, const FVector2D& LabelSize,
	                                        TConstArrayView<FWorldMapTagPlacement> MissionPlates) const;

	/**
	 * @brief Where one name wants to be before the names are weighed against each other.
	 * @param LabelRoot Widget to project; hidden here when it cannot be placed yet.
	 * @param WorldPoint Point on the map the name belongs to.
	 * @param OffsetFromPoint Shift of the name's middle from the projected point, in local units.
	 * @param MyGeometry Geometry of this overlay.
	 * @param OutPlacement Receives the name's middle and size in local units.
	 * @return false when the point is behind the camera or the widget has not been laid out yet.
	 */
	bool TryGetLabelPlacement(UWidget* LabelRoot, const FVector& WorldPoint, const FVector2D& OffsetFromPoint,
	                          const FGeometry& MyGeometry, FWorldMapTagPlacement& OutPlacement);

	void RepositionPieceTags(const FGeometry& MyGeometry);

	/**
	 * @brief Turns one piece-to-target line into dashes, clipped to the overlay so far targets stay cheap.
	 * @param PieceTag Tag whose piece and target are projected.
	 * @param PiecePosition Piece in local units.
	 * @param MyGeometry Geometry of this overlay.
	 * @param OutArrows Receives the arrow when any of it is on screen.
	 */
	void AddPieceArrow(const FWorldMapPieceTag& PieceTag, const FVector2D& PiecePosition,
	                   const FGeometry& MyGeometry, TArray<FWorldMapArrowPaint>& OutArrows);

	bool GetIsValidWorldPlayerController() const;
	bool GetIsValidArrowLayer() const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	// Owned by the HUD that created this widget.
	const FWorldCampaignUILook* M_Look = nullptr;

	// Lives on the world generator; resolved lazily because the HUD can exist before the map is generated.
	UPROPERTY()
	TWeakObjectPtr<UWorldCountryOccupationRegulator> M_CountryRegulator;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_RootCanvas;

	UPROPERTY()
	TObjectPtr<UW_WorldMapArrowLayer> M_ArrowLayer;

	UPROPERTY()
	TMap<FName, FWorldMapCountryLabel> M_CountryLabels;

	// Country ids in the order their labels were made: the fixed priority when two labels want the same spot, so
	// the same one wins every frame.
	TArray<FName> M_CountryLabelOrder;

	UPROPERTY()
	TMap<FGuid, FWorldMapPieceTag> M_PieceTags;

	// Keyed by the mission's anchor key.
	UPROPERTY()
	TMap<FGuid, FWorldMapMissionTag> M_MissionTags;

	// Anchor keys in the order the plates were made: the fixed priority between two mission names.
	TArray<FGuid> M_MissionTagOrder;

	// The player HQ's world location; country labels are lifted off its model.
	FVector M_PlayerHQLocation = FVector::ZeroVector;

	bool bM_HasPlayerHQLocation = false;

	bool bM_HasBuiltCountryLabels = false;

	bool bM_TagsVisible = true;

	float M_SecondsUntilDataRefresh = 0.f;
};
