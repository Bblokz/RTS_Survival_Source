// Copyright Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "NomadicDeployEditorLibrary.generated.h"

class UMaterial;
class UMaterialExpression;

/**
 * @brief Editor-only material helpers for the nomadic deploy pipeline (Source/Tools/NomadicDeploy, editor python).
 * Python has no MP_WorldPositionOffset, and wiring the pin through other tools left old masters compiling without it;
 * these go through UMaterialEditingLibrary with the real property. No-ops outside the editor.
 */
UCLASS()
class RTS_SURVIVAL_API UNomadicDeployEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Connects an expression output to the material's World Position Offset.
	 * @param FromExpression Expression already added to the material.
	 * @param FromOutputName Output pin name; empty for the first output.
	 * @return Whether the connection was made.
	 */
	UFUNCTION(BlueprintCallable, Category = "NomadicDeploy|Editor")
	static bool ConnectWorldPositionOffset(UMaterialExpression* FromExpression, const FString& FromOutputName);

	// The expression currently feeding World Position Offset, or null.
	UFUNCTION(BlueprintCallable, Category = "NomadicDeploy|Editor")
	static UMaterialExpression* GetWorldPositionOffsetInput(UMaterial* Material, FString& OutOutputName);

	// Diagnostic: editor input connection vs what the compiler's cached expression data says.
	UFUNCTION(BlueprintCallable, Category = "NomadicDeploy|Editor")
	static FString DescribeWorldPositionOffset(UMaterial* Material);

	/**
	 * @brief Finds the static switch parameter SwitchName and makes it feed World Position Offset (directly, or as B of
	 * the Add that sums it with an existing offset). Repairs masters whose pin was wired to the wrong node by index.
	 * @return What the pin was fed by before and after, for the log.
	 */
	UFUNCTION(BlueprintCallable, Category = "NomadicDeploy|Editor")
	static FString WireSwitchToWorldPositionOffset(UMaterial* Material, const FName SwitchName);

	// Diagnostic: what the rendered resource of a material / instance compiled to (WPO use, permutation, errors).
	UFUNCTION(BlueprintCallable, Category = "NomadicDeploy|Editor")
	static FString DescribeCompiledResource(UMaterialInterface* MaterialInterface);

	// Rebuilds the cached expression data and recompiles (after editing a master from script).
	UFUNCTION(BlueprintCallable, Category = "NomadicDeploy|Editor")
	static void RefreshMaterial(UMaterial* Material);
};
