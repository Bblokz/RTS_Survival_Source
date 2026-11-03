// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTSCorpsePoseLibrary.generated.h"

class USkeletalMesh;
class UStaticMesh;

/** One baked corpse: the mesh and how its pelvis lay, so a settled ragdoll can find the pose closest to its own. */
USTRUCT()
struct FRTSCorpsePose
{
	GENERATED_BODY()

	// Its origin is the pelvis over the ground; its axes are the world axes of the bake. Material slots match the
	// soldier mesh's slots one to one, so a soldier's materials apply as they are.
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
 * @brief The baked corpse poses of one soldier mesh, written by the -RagdollPoseBake harness into the corpse folder,
 * where URTSCorpseSubsystem finds it; dying soldiers and thrown crew of that mesh are left as these poses.
 */
UCLASS()
class RTS_SURVIVAL_API URTSCorpsePoseLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	// Soft, so finding the libraries does not load every soldier mesh of the game.
	UPROPERTY(EditAnywhere, Category="Corpse")
	TSoftObjectPtr<USkeletalMesh> SourceMesh;

	UPROPERTY(EditAnywhere, Category="Corpse")
	TArray<FRTSCorpsePose> Poses;
};
