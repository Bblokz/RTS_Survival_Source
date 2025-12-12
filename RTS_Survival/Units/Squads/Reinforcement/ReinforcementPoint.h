// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReinforcementPoint.generated.h"

class UMeshComponent;

/**
 * @brief Defines a socket-based reinforcement location on a mesh.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UReinforcementPoint : public UActorComponent
{
        GENERATED_BODY()

public:
        UReinforcementPoint();

        /**
         * @brief Initialize the reinforcement mesh and socket.
         * @param InMeshComponent Mesh component that owns the socket.
         * @param InSocketName Socket name on the mesh to use for reinforcements.
         */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
        void InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName);

        /**
         * @brief Resolve the world-space location of the configured reinforcement socket.
         * @param bOutHasValidLocation Set true when a valid mesh and socket were found.
         * @return Socket world location or ZeroVector when invalid.
         */
        FVector GetReinforcementLocation(bool& bOutHasValidLocation) const;

private:
        bool GetIsValidReinforcementMesh() const;

        UPROPERTY()
        TWeakObjectPtr<UMeshComponent> M_ReinforcementMeshComponent;

        UPROPERTY()
        FName M_ReinforcementSocketName;
};
