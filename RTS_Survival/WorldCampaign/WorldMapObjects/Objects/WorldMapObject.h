// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldMapObject.generated.h"

class AAnchorPoint;

/**
 * @brief Base actor spawned to represent a promoted anchor during campaign generation.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldMapObject : public AActor
{
	GENERATED_BODY()

public:
	AWorldMapObject();

	virtual void InitializeForAnchor(AAnchorPoint* AnchorPoint);
	virtual FString GetObjectDebugName() const;

	AAnchorPoint* GetOwningAnchor() const;
	FGuid GetAnchorKey() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AAnchorPoint> M_OwningAnchor;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	FGuid M_AnchorKey;
};
