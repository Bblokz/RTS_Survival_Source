// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTSMaterialGraphLibrary.generated.h"

class UMaterial;
class UMaterialExpressionComment;

/**
 * @brief Editor scripting helpers for material graphs that Python cannot reach otherwise. The landscape builders
 * (Source/Docs/Landscape/ls_*_build.py) call them as unreal.RTSMaterialGraphLibrary.<function>.
 */
UCLASS()
class RTS_SURVIVAL_API URTSMaterialGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief A comment's size is internal editor state with no editable property, so scripts could only place
	 * default-sized comments instead of frames around the nodes that belong together.
	 * @param Comment The comment expression to resize.
	 * @param SizeX Width in graph units.
	 * @param SizeY Height in graph units.
	 * @return False when the comment is missing or this build has no editor data.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Material Graph")
	static bool SetCommentSize(UMaterialExpressionComment* Comment, const int32 SizeX, const int32 SizeY);

	/**
	 * @brief The material's output node has no editable position either, so scripts could not place it next to the
	 * nodes that feed it.
	 * @param Material The material whose output node moves.
	 * @param PositionX Graph X of the output node.
	 * @param PositionY Graph Y of the output node.
	 * @return False when the material is missing or this build has no editor data.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Material Graph")
	static bool SetMaterialOutputPosition(UMaterial* Material, const int32 PositionX, const int32 PositionY);
};
