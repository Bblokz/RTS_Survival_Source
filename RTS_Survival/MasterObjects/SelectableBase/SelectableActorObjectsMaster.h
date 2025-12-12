// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
// For Unit type
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"

#include "SelectableActorObjectsMaster.generated.h"


class UFowComp;
class ACPPController;
class RTS_SURVIVAL_API USelectionComponent;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API ASelectableActorObjectsMaster : public AHPActorObjectsMaster, public ICommands
{
	GENERATED_BODY()

public:
	ASelectableActorObjectsMaster(const FObjectInitializer& ObjectInitializer);

	
	// Updates selection logic on the component which also updates the selection decal.
	void SetUnitSelected(const bool bIsSelected) const;

	// Contains references to selectionBox, decals and associated flags.
	UFUNCTION(BlueprintCallable)
	inline USelectionComponent* GetSelectionComponent() const { return SelectionComponent; };

	// If unit is rendered or not
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsHidden;


	// ICommand interface helpers.
	virtual FVector GetOwnerLocation() const override final;
	virtual FString GetOwnerName() const override final;
	virtual AActor* GetOwnerActor() override final;

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	virtual void UnitDies(const ERTSDeathType DeathType) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Contains references to selectionBox, decals and associated flags. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ReferenceCasts")
	USelectionComponent* SelectionComponent;
	


	bool GetIsValidSelectionComponent() const;


	virtual void PostInitializeComponents() override;

	UPROPERTY(BlueprintReadOnly, Category="Reference")
	ACPPController* PlayerController;


	// Creates the vision bubble for either the player or the AI.
	// Note that unlike with pawns we only define this for also selectable actor objects as
	// Hp actors also have environment derived classes that do not have vision.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFowComp* FowComponent;

	/*--------------------------------- Abilities ---------------------------------*/
	/** @copydoc ICommands::ResetUnitSpecificLogic */
	virtual void SetUnitToIdleSpecificLogic() override;


	/** @copydoc ICommands::GetCommandData()*/
	virtual UCommandData* GetIsValidCommandData() override final;

	/** @copydoc ICommands::DoneExecutingCommand */
	UFUNCTION(BlueprintCallable, Category = "Commands", meta = (BlueprintProtected = "true"))
	virtual void DoneExecutingCommand(EAbilityID AbilityFinished) override final;

	/** @copydoc ICommands::StopBehaviourTree */
	virtual void StopBehaviourTree() override;

	virtual void StopMovement() override;


	// Contains the data for the commands that this unit can execute using the ICommands interface.
	// Needs to be accessible by derived classes to get data like current command.
	UPROPERTY()
	UCommandData* UnitCommandData;

private:
	// Setup the behaviour of the fow component depending on the owner as registered in the RTS Component.
	void PostInitComp_InitFowBehaviour(URTSComponent* MyRTSComponent);
};
