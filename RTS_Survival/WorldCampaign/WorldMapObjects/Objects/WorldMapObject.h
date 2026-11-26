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

	/**
	 * @brief Marks the mission as won or the enemy item as destroyed. A cleared object can no longer be clicked or
	 * launched; by default it is hidden, and Blueprint can show a wreck or a flag instead.
	 */
	void SetIsClearedByPlayer();

	UFUNCTION(BlueprintPure, Category = "World Campaign|World Objects")
	bool GetIsClearedByPlayer() const { return bM_IsClearedByPlayer; }

protected:
	/** C++ side of BP_OnClearedByPlayer, called just before it. */
	virtual void OnClearedByPlayer();

	/** Called once when the player cleared this object, also after loading a campaign in which it was cleared. */
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|World Objects")
	void BP_OnClearedByPlayer();

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

	// Off = the Blueprint draws its own cleared look in BP_OnClearedByPlayer and the actor stays visible.
	UPROPERTY(EditDefaultsOnly, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	bool bM_HideWhenClearedByPlayer = true;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	bool bM_IsClearedByPlayer = false;
};
