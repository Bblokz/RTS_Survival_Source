// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTSShortsLandscapeLibrary.generated.h"

class ULandscapeLayerInfoObject;
class UMaterialInterface;

/**
 * @brief Editor-only helpers the shorts level scripts call from Python: editor Python cannot create a landscape,
 * so the battle stages build theirs through the same ALandscape::Import path the landscape editor's
 * "New Landscape" button uses, with heights and painted layer weights read from raw files.
 */
UCLASS()
class RTS_SURVIVAL_API URTSShortsLandscapeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief New cinematic terrain must answer the custom ground traces used by tracked vehicle physics.
	 * @param LandscapeActor Isolated stage landscape whose collision may be rebuilt.
	 * @return True when its collision was configured and rebuilt in the editor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor")
	static bool ConfigureVehicleGroundLandscape(AActor* LandscapeActor);

	/**
	 * @brief Lets isolated stage builders bake vehicle and crew paths after terrain and dressing are placed.
	 * @param WorldContextObject Editor world containing the new stage's navigation bounds.
	 * @return True when a valid editor navigation system accepted the rebuild.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor", meta = (WorldContext = "WorldContextObject"))
	static bool RebuildStageNavigation(UObject* WorldContextObject);

	/**
	 * @brief Lets stage dressing scripts reshape an existing isolated landscape without replacing its actor.
	 * @param LandscapeActor Existing landscape with edit layers disabled.
	 * @param HeightFile Raw uint16 heights matching the landscape's complete vertex grid.
	 * @return True when the grid was validated and the editor height/collision update completed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor")
	static bool ImportStageHeights(AActor* LandscapeActor, const FString& HeightFile);

	/**
	 * Spawns a landscape with one section per component and imports its heights and layer weights.
	 * @param Location World location of the landscape's corner (vertex 0, 0).
	 * @param Scale Actor scale: X/Y are centimetres per quad, Z 100 maps the uint16 range to +-256 m.
	 * @param HeightFile Raw little-endian uint16 heights, (ComponentsX * QuadsPerSection + 1) x
	 * (ComponentsY * QuadsPerSection + 1), row by row along X.
	 * @param Layers Layer info objects of the landscape material's paint layers.
	 * @param LayerFiles One raw uint8 weight file per layer in the same grid; weights sum to 255 per vertex.
	 * @return The landscape, or nullptr (logged) outside the editor or when a file does not match the grid.
	 */
	/**
	 * @brief Reads a rectangle of an existing landscape so a map script can reshape part of it offline.
	 * Vertex coordinates are landscape grid indices (the same space as GetLandscapeExtent). Vertices of missing
	 * components read as 0.
	 * @param HeightFile Written as raw uint16, (MaxX - MinX + 1) x (MaxY - MinY + 1), row by row along X.
	 * @param Layers Paint layers to read; one raw uint8 file per layer in LayerFiles, same grid.
	 * @return True when every file was written.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor")
	static bool ExportLandscapeRegion(AActor* LandscapeActor, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY,
	                                  const FString& HeightFile, const TArray<ULandscapeLayerInfoObject*>& Layers,
	                                  const TArray<FString>& LayerFiles);

	/**
	 * @brief Writes a rectangle of heights and paint weights back (files laid out as ExportLandscapeRegion writes
	 * them). Pass an empty HeightFile to keep the heights. Every listed layer is written as-is (no renormalising), so
	 * pass all painted layers with weights that sum to 255. Requires a landscape without edit layers.
	 * @return True when the files matched the rectangle and the landscape was updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor")
	static bool ImportLandscapeRegion(AActor* LandscapeActor, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY,
	                                  const FString& HeightFile, const TArray<ULandscapeLayerInfoObject*>& Layers,
	                                  const TArray<FString>& LayerFiles);

	/**
	 * @brief Adds the missing components in a component-index rectangle (the landscape editor's Add Component tool,
	 * without a viewport). New components start flat at height 0 (landscape z -256 m): write their heights and
	 * weights with ImportLandscapeRegion right after.
	 * @return Number of components added, or -1 on error.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor")
	static int32 AddLandscapeComponents(AActor* LandscapeActor, int32 MinComponentX, int32 MinComponentY,
	                                    int32 MaxComponentX, int32 MaxComponentY);

	/** @return "MinX MinY MaxX MaxY ComponentSizeQuads" of the landscape grid, or an empty string. */
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor")
	static FString GetLandscapeGridInfo(AActor* LandscapeActor);

	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* CreatePaintedLandscape(UObject* WorldContextObject, FVector Location, FVector Scale,
	                                      int32 ComponentsX, int32 ComponentsY, int32 QuadsPerSection,
	                                      UMaterialInterface* Material, const FString& HeightFile,
	                                      const TArray<ULandscapeLayerInfoObject*>& Layers,
	                                      const TArray<FString>& LayerFiles, const FString& Label);
};
