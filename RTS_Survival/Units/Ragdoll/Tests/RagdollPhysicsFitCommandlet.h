// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RagdollPhysicsFitCommandlet.generated.h"

/**
 * @brief Refits the capsules of a physics asset to one skeletal mesh, keeping its bodies and joints, so a ragdoll
 * copied from a similar soldier stops sinking into the ground. Vertices of bones without a body count for the nearest
 * parent body; vertices on the root are carried by no body and are left out. Body pairs that overlap in the rest
 * pose stop colliding with each other, as they would otherwise keep the ragdoll twitching on the ground.
 * Run with UnrealEditor-Cmd -run=RagdollPhysicsFit -Mesh=<object path> -PhysicsAsset=<object path>.
 */
UCLASS()
class RTS_SURVIVAL_API URagdollPhysicsFitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
