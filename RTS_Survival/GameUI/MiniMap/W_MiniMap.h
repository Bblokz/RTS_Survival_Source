// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "RTS_Survival/GameUI/MiniMap/RTSMinimapIconHelpers.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "W_MiniMap.generated.h"

struct FRTSMinimapCustomIconDrawData;
class AFowManager;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture;
class UTexture2D;

/** Delegate broadcast whenever the mini‐map is clicked, giving UV in [0,1]. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiniMapClicked, FVector2D, LocalClickUV);

/**
 * @brief Look of the glass over the minimap edges: a refracting band, a lit rim, a reflection streak, frost and
 * post-apocalyptic scratches. Edited on W_BP_MiniMap (Class Defaults, MiniMap|Glass) with a live designer preview.
 */
USTRUCT(BlueprintType)
struct FMiniMapGlassSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass")
	bool bShowGlass = true;

	/** Width of the glass edge band as a fraction of the minimap's shortest side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float GlassWidth = 0.09f;

	/** Rounding of the glass corners as a fraction of the shortest side (0 = sharp square corners). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float CornerRadius = 0.02f;

	/** How strongly the map bends under the glass band (lens magnification towards the edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Refraction = 0.3f;

	/** Falloff of the edge tint from the border inwards; higher keeps the tint closer to the border. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float FresnelPower = 2.2f;

	/** Colour of the glass towards the border; alpha is its strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass")
	FLinearColor EdgeTint = FLinearColor(0.7f, 0.86f, 1.0f, 0.22f);

	/** Brightness of the thin highlight just inside the border (lit from the top left). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Rim", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float RimIntensity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Rim", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float RimWidthPx = 1.5f;

	/** Dark line where the glass meets the frame, giving the pane its thickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Rim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InnerShadow = 0.35f;

	/** Diagonal reflection band across the glass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Reflection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StreakIntensity = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Reflection", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float StreakAngleDegrees = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Reflection", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float StreakWidth = 0.1f;

	/** Moves the streak across the glass (-0.5 .. 0.5 of the minimap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Reflection", meta = (ClampMin = "-0.75", ClampMax = "0.75"))
	float StreakOffset = -0.18f;

	/** Cloudy frost on the glass band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Frost", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FrostIntensity = 0.1f;

	/** Frost blobs across the minimap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Frost", meta = (ClampMin = "1.0", ClampMax = "64.0"))
	float FrostScale = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScratchIntensity = 0.3f;

	/** Scratch cells across the minimap; more cells give more, shorter scratches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches", meta = (ClampMin = "1.0", ClampMax = "64.0"))
	float ScratchDensity = 11.0f;

	/** Share of cells that hold a scratch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScratchCoverage = 0.45f;

	/** Scratch length relative to its cell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float ScratchLength = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float ScratchWidthPx = 0.8f;

	/** 1 keeps scratches on the glass band only, 0 spreads them over the whole minimap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScratchEdgeBias = 0.75f;

	/** Changes the scratch pattern. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glass|Scratches")
	float ScratchSeed = 3.0f;
};

/**
 * @brief Widget displaying the world mini‐map and handling click input.
 * @note NativeOnMouseButtonDown: catches clicks on the widget and fires OnMiniMapClicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_MiniMap : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @return the UImage used to render the mini‐map, or nullptr (and logs) if invalid */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UImage* GetIsValidMiniMapImg() const;

	AFowManager* GetIsValidFowManager() const;

	/**
	 * @brief Keeps the static terrain capture and live FOW textures in one minimap material.
	 * @param Active Live player-vision render target.
	 * @param Passive Live explored-area render target.
	 * @param Terrain One-shot playable-area capture owned by the game state.
	 * @param FowManager Supplies icon and coordinate data used by the widget.
	 */
	void InitMiniMapRTs(const TObjectPtr<UTexture>& Active,
	                    const TObjectPtr<UTexture>& Passive,
	                    const TObjectPtr<UTexture>& Terrain,
	                    const TObjectPtr<AFowManager>& FowManager);

	/** Applies new glass settings at runtime (both the map refraction and the overlay). */
	UFUNCTION(BlueprintCallable, Category = "MiniMap|Glass")
	void SetGlassSettings(const FMiniMapGlassSettings& NewGlassSettings);

	const FMiniMapGlassSettings& GetGlassSettings() const { return M_GlassSettings; }

	/** Fired when the user clicks the mini-map; UV coordinates in [0,1]. */
	UPROPERTY(BlueprintAssignable, Category="MiniMap")
	FOnMiniMapClicked OnMiniMapClicked;

protected:
	/** Bound widget in the UMG designer. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> MiniMapImg;

	virtual void NativePreConstruct() override;

	virtual void NativeConstruct() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Glass over the minimap edges; tweak in the Class Defaults of W_BP_MiniMap (previews in the designer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MiniMap|Glass", meta = (DisplayName = "Glass Settings"))
	FMiniMapGlassSettings M_GlassSettings;

	// Drawn last, over the map, the unit icons and the camera frustum, so they all sit under the glass.
	UPROPERTY(EditAnywhere, Category = "MiniMap|Glass", meta = (DisplayName = "Glass Overlay Material"))
	TSoftObjectPtr<UMaterialInterface> M_GlassOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Blueprints/GameUI/MiniMap/M_UI_MiniMapGlass.M_UI_MiniMapGlass")));


	virtual int32 NativePaint(const FPaintArgs& Args,
	                          const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect,
	                          FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId,
	                          const FWidgetStyle& InWidgetStyle,
	                          bool bParentEnabled) const override;

	/** Override to detect left‐mouse clicks on the mini‐map. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
	                                       const FPointerEvent& InMouseEvent) override;

private:
	int32 DrawMiniMapUnitColorIcons(const FGeometry& AllottedGeometry,
	                                FSlateWindowElementList& OutDrawElements,
	                                const int32 LayerId) const;

	int32 DrawCustomMiniMapTextureIcons(const FGeometry& AllottedGeometry,
	                                   FSlateWindowElementList& OutDrawElements,
	                                   const int32 LayerId) const;

	int32 DrawCustomMiniMapText(const FGeometry& AllottedGeometry,
	                          FSlateWindowElementList& OutDrawElements,
	                          const int32 LayerId) const;

	void DrawCustomMiniMapTextEntry(const FRTSMinimapTextDrawData& TextDrawData,
	                                const FGeometry& MiniMapGeometry,
	                                const FGeometry& AllottedGeometry,
	                                const FVector2D& MiniMapSize,
	                                FSlateWindowElementList& OutDrawElements,
	                                const int32 LayerId) const;

	void DrawCustomMiniMapTextureIcon(const FRTSMinimapCustomIconDrawData& IconDrawData,
	                                  const FGeometry& MiniMapGeometry,
	                                  const FGeometry& AllottedGeometry,
	                                  const FVector2D& MiniMapSize,
	                                  FSlateWindowElementList& OutDrawElements,
	                                  const int32 LayerId) const;

	void RefreshCustomMiniMapIconBrushes();

	const FSlateBrush* GetCustomMiniMapIconBrush(const EMinimapIconType IconType) const;

	UPROPERTY()
	TObjectPtr<AFowManager> M_FowManager = nullptr;

	mutable bool bM_HasReportedMissingFowManager = false;

	FSlateRoundedBoxBrush M_MinimapIconBrush = FSlateRoundedBoxBrush(FLinearColor::White);

	UPROPERTY()
	TMap<EMinimapIconType, FSlateBrush> M_CustomMinimapIconBrushes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_GlassOverlayMaterialInstance = nullptr;

	FSlateBrush M_GlassOverlayBrush;

	// Render angle the glass light was last aligned to; the start direction rotates the whole widget.
	float M_GlassAppliedAngle = 0.0f;

	void EnsureGlassOverlayMaterial();
	void ApplyGlassSettings();

	/**
	 * @brief Writes the glass settings into one minimap material instance (map refraction or overlay).
	 * @param MaterialInstance The map image's or the overlay's dynamic material.
	 */
	void ApplyGlassParameters(UMaterialInstanceDynamic& MaterialInstance) const;

	int32 DrawGlassOverlay(const FGeometry& AllottedGeometry,
	                       FSlateWindowElementList& OutDrawElements,
	                       const int32 LayerId) const;
};
