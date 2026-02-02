// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "WorldPlayerController.generated.h"

class UWorldCameraController;

/**
 * @brief Controller used by the world campaign Blueprint to route Enhanced Input
 * actions into the C++ camera component.
 * @note SetIsWorldCameraMovementDisabled: call in blueprint to enable or block camera input.
 * @note WorldCamera_ZoomIn: call in blueprint to route zoom input.
 * @note WorldCamera_ZoomOut: call in blueprint to route zoom input.
 * @note WorldCamera_ForwardRightMovement: call in blueprint to route movement axes.
 * @note WorldCamera_MoveTo: call in blueprint to move the camera to campaign locations.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

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
	void WorldCamera_ForwardRightMovement(bool bOnForward, float AxisX, bool bOnRight, float AxisY);

	/**
	 * @brief Provides a Blueprint routing point for move-to requests during the campaign map.
	 * @param MoveRequest Move data passed through to the camera controller.
	 */
	UFUNCTION(BlueprintCallable)
	void WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest);

private:
	bool GetIsValidWorldCameraController() const;

	UPROPERTY()
	TWeakObjectPtr<UWorldCameraController> M_WorldCameraController;
};
