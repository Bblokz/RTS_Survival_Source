// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowParticipant.h"
#include "Connection.generated.h"

class AAnchorPoint;

/**
 * @brief Connection actors are spawned by the campaign generator to link anchor points on the world map.
 */
UCLASS()
class RTS_SURVIVAL_API AConnection : public AActor, public IWorldFowParticipant
{
	GENERATED_BODY()

public:
	AConnection();

	virtual void PostInitializeComponents() override;

	void InitializeConnection(AAnchorPoint* AnchorA, AAnchorPoint* AnchorB);
	bool TryAddThirdAnchor(AAnchorPoint* AnchorPoint);

	bool GetIsThreeWayConnection() const { return bM_IsThreeWayConnection; }
	const TArray<TObjectPtr<AAnchorPoint>>& GetConnectedAnchors() const { return M_ConnectedAnchors; }
	FVector GetJunctionLocation() const { return M_JunctionLocation; }

	void DebugDrawBaseConnection(const FColor& Color, float Duration) const;
	void DebugDrawThirdConnection(const FColor& Color, float Duration) const;
	virtual UWorldMapFowComponent* GetFowComponent() const override { return M_WorldMapFowComponent.Get(); }

private:
	void SetConnectedAnchors(AAnchorPoint* AnchorA, AAnchorPoint* AnchorB);
	FVector GetAnchorLocationSafe(const AAnchorPoint* AnchorPoint) const;
	FVector GetProjectedPointOnBaseSegment(const FVector& AnchorLocation) const;

	UPROPERTY()
	TWeakObjectPtr<UWorldMapFowComponent> M_WorldMapFowComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AAnchorPoint>> M_ConnectedAnchors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection", meta = (AllowPrivateAccess = "true"))
	bool bM_IsThreeWayConnection = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection", meta = (AllowPrivateAccess = "true"))
	FVector M_JunctionLocation = FVector::ZeroVector;
};
