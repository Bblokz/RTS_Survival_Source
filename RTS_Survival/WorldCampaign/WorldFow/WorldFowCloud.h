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

	/** @return World Z of the top of the cloud mesh; what must show under fog is drawn just above it. */
	float GetFogTopZ() const;

	/**
	 * @brief Finds the fog cloud of a campaign map and reads the top of it.
	 * @param World World of the campaign map.
	 * @param OutFogTopZ Receives the world Z of the top of the cloud.
	 * @return false when the map has no fog cloud, so nothing is hidden.
	 */
	static bool TryGetFogTopZ(const UWorld* World, float& OutFogTopZ);
	UMeshComponent* GetCloudMeshComponent() const { return M_CloudMeshComponent; }
	
	virtual void PostInitializeComponents() override;
	
	protected:
	UPROPERTY( BlueprintReadWrite)
	TObjectPtr<UMeshComponent> M_CloudMeshComponent = nullptr;
};
