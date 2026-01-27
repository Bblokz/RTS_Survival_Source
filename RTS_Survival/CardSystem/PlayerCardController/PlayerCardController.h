// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerCardController.generated.h"

class UW_RTSCard;
class UW_CardMenu;
class URTSCardManager;
/**
 * @brief Serves as the player controller that coordinates card menu UI interactions.
 */
UCLASS()
class RTS_SURVIVAL_API APlayerCardController : public APlayerController
{
	GENERATED_BODY()

public:
	APlayerCardController(const FObjectInitializer& ObjectInitializer);


protected:
	UPROPERTY(BlueprintReadWrite, Category="UI")
	TObjectPtr<UW_CardMenu> CardMenu;

private:



	bool GetIsValidCardMenu() const;
};
