// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
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

	/**
	 * @brief Gets the authoritative strength estimation cache shared by enemy and mission map objects.
	 * @return Strength estimation component owned by this world map object.
	 */
	UWorldStrengthEstimationComponent* GetWorldStrengthEstimationComponent() const
	{
		return M_WorldStrengthEstimationComponent;
	}
	virtual UWorldMapFowComponent* GetFowComponent() const override { return M_WorldMapFowComponent.Get(); }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strength Estimation",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldStrengthEstimationComponent> M_WorldStrengthEstimationComponent;

	UPROPERTY()
	TWeakObjectPtr<UWorldMapFowComponent> M_WorldMapFowComponent;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AAnchorPoint> M_OwningAnchor;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	FGuid M_AnchorKey;
};
