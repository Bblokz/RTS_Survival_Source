// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowParticipant.h"
#include "WorldMapObject.generated.h"

class AAnchorPoint;

/**
 * @brief Base actor spawned to represent a promoted anchor during campaign generation.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldMapObject : public AActor, public IWorldFowParticipant
{
	GENERATED_BODY()

public:
	AWorldMapObject();

	virtual void PostInitializeComponents() override;

	virtual void InitializeForAnchor(AAnchorPoint* AnchorPoint);
	virtual FString GetObjectDebugName() const;

	AAnchorPoint* GetOwningAnchor() const;
	FGuid GetAnchorKey() const;
	virtual UWorldMapFowComponent* GetFowComponent() const override { return M_WorldMapFowComponent.Get(); }

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AAnchorPoint> M_OwningAnchor;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	FGuid M_AnchorKey;
};
