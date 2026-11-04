// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "WreckDebrisAssetTools.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/**
 * @brief Editor-only setup of NS_WreckDebrisBurst's mesh renderer, which the editor tooling cannot reach: the debris
 * meshes with their own scales and the material override bound to a User material parameter.
 * Called from Source/Tools/RegularDeath/ue_build_wreck_debris.py in a headless editor; never at runtime.
 */
UCLASS()
class RTS_SURVIVAL_API UWreckDebrisAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * @brief Puts the meshes on every mesh renderer of the system and binds its material override to a User parameter.
	 * @param System The debris system; recompiled and marked dirty, the caller saves it.
	 * @param Meshes Meshes of the renderer, picked per particle by the mesh index.
	 * @param MeshScales Uniform scale per mesh (same order); missing entries use 1.
	 * @param UserMaterialParameter User parameter name without the "User." prefix, declared on the system if missing.
	 * @param DefaultMaterial Default value of the parameter and the renderer's explicit material.
	 * @return False when the system has no mesh renderer or an input is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Editor")
	static bool SetupDebrisMeshRenderer(UNiagaraSystem* System, const TArray<UStaticMesh*>& Meshes,
	                                    const TArray<float>& MeshScales, FName UserMaterialParameter,
	                                    UMaterialInterface* DefaultMaterial);
#endif
};
