// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTSCorpsePoseLibrary.generated.h"

class UStaticMesh;

/** One baked corpse: the mesh and how its pelvis lay, so a settled ragdoll can find the pose closest to its own. */
USTRUCT()
struct FRTSCorpsePose
{
	GENERATED_BODY()

	// Its origin is the pelvis over the ground; its axes are the world axes of the bake.
	UPROPERTY(EditAnywhere, Category="Corpse")
	TObjectPtr<UStaticMesh> Mesh;

	// The pelvis bone's unit axes in the mesh's space.
	UPROPERTY(EditAnywhere, Category="Corpse")
	FVector PelvisAxisX = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, Category="Corpse")
	FVector PelvisAxisY = FVector::RightVector;

	UPROPERTY(EditAnywhere, Category="Corpse")
	FVector PelvisAxisZ = FVector::UpVector;
};

/**
 * @brief The baked corpse poses of one crew mesh, written by the -RagdollPoseBake harness and read by
 * URTSCorpseSubsystem through URTSRagdollSettings.
 */
UCLASS()
class RTS_SURVIVAL_API URTSCorpsePoseLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Corpse")
	TArray<FRTSCorpsePose> Poses;
};
