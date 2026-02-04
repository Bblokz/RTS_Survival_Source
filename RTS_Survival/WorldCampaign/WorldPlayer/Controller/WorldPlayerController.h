// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "WorldPlayerController.generated.h"

class AGeneratorWorldCampaign;
class UWorldCameraController;

/**
 * @brief Controller used by the world campaign Blueprint to route Enhanced Input
 * actions into the C++ camera component.
 * @note SetIsWorldCameraMovementDisabled: call in blueprint to enable or block camera input.
 * @note WorldCamera_ZoomIn: call in blueprint to route zoom input.
 * @note WorldCamera_ZoomOut: call in blueprint to route zoom input.
 * @note WorldCamera_ForwardMovement: call in blueprint to route forward/backward axis input.
 * @note WorldCamera_RightMovement: call in blueprint to route right/left axis input.
 * @note WorldCamera_MoveTo: call in blueprint to move the camera to campaign locations.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void SetIsWorldCameraMovementDisabled(bool bIsDisabled);

	UFUNCTION(BlueprintCallable)
	bool GetIsWorldCameraMovementDisabled() const;

	UFUNCTION(BlueprintCallable)
	void WorldCamera_ZoomIn();

	UFUNCTION(BlueprintCallable)
	void WorldCamera_ZoomOut();

	UFUNCTION(BlueprintCallable)
	void WorldCamera_ForwardMovement(float AxisValue);

	UFUNCTION(BlueprintCallable)
	void WorldCamera_RightMovement(float AxisValue);

	/**
	 * @brief Provides a Blueprint routing point for move-to requests during the campaign map.
	 * @param MoveRequest Move data passed through to the camera controller.
	 */
	UFUNCTION(BlueprintCallable)
	void WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest);

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	



private:
	bool GetIsValidWorldCameraController() const;

	void Beginplay_HandleWorldGeneration();

	UPROPERTY()
	TWeakObjectPtr<UWorldCameraController> M_WorldCameraController;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;
	bool GetIsValidWorldGenerator() const;
	FWorldgene
};
