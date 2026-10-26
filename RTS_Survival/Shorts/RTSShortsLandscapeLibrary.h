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
	UFUNCTION(BlueprintCallable, Category = "Shorts|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* CreatePaintedLandscape(UObject* WorldContextObject, FVector Location, FVector Scale,
	                                      int32 ComponentsX, int32 ComponentsY, int32 QuadsPerSection,
	                                      UMaterialInterface* Material, const FString& HeightFile,
	                                      const TArray<ULandscapeLayerInfoObject*>& Layers,
	                                      const TArray<FString>& LayerFiles, const FString& Label);
};
