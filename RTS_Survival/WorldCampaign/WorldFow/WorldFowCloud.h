// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UMeshComponent;

#include "WorldFowCloud.generated.h"

/**
 * @brief Place one cloud actor on the campaign map to define the FOW mask center, XY bounds, and target mesh material.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldFowCloud : public AActor
{
	GENERATED_BODY()
public:
	AWorldFowCloud();
	FVector2D GetMapSizeXY() const;
	UMeshComponent* GetCloudMeshComponent() const { return M_CloudMeshComponent; }
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW Cloud", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMeshComponent> M_CloudMeshComponent = nullptr;
};
