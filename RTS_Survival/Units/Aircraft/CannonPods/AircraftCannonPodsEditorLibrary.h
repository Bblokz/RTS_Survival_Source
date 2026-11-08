// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AircraftCannonPodsEditorLibrary.generated.h"

class USkeletalMesh;

/**
 * @brief Editor scripting helper for gun-pod aircraft variants: Python cannot set a skeletal mesh socket's name,
 * so Source/Tools/Ju87G/author_ju87g_blueprint.py adds the pod and muzzle sockets through this library.
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftCannonPodsEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Adds the socket to the mesh (not its skeleton, so other meshes stay untouched) or moves it if it exists.
	 * @param Mesh Skeletal mesh asset to edit; the caller saves it.
	 * @param SocketName Name of the socket.
	 * @param BoneName Bone the socket follows.
	 * @param RelativeLocation Location in the bone's space (bone scale applies).
	 * @param RelativeRotation Rotation in the bone's space.
	 * @return True when the mesh holds the socket afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category="Cannon Pods|Editor")
	static bool SetSkeletalMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName,
	                                  FVector RelativeLocation, FRotator RelativeRotation);
};
