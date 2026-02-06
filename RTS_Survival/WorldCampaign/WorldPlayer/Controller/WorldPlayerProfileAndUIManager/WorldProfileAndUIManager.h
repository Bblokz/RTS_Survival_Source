// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "WorldProfileAndUIManager.generated.h"


class AWorldPlayerController;
class UW_WorldMenu;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldProfileAndUIManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UWorldProfileAndUIManager();

	void SetupWorldMenu(AWorldPlayerController* PlayerController);
	void OnSetupUIForNewCampaign(ERTSFaction PlayerFaction);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UW_WorldMenu> WorldMenuClass;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_WorldMenu> M_WorldMenu;
	bool GetIsValidWorldMenu() const;
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_PlayerController;
	bool GetIsValidPlayerController() const;
 
};
