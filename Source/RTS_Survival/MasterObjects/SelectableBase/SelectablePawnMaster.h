// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "SelectablePawnMaster.generated.h"


// Forward declaration.
enum class EAbilityID : uint8;
class RTS_SURVIVAL_API ACPPController;
class RTS_SURVIVAL_API USelectionComponent;
struct FCommandData;


//todo refactor.
UENUM()
enum OrderTypeVehicle
{
	OTV_Armed,
	OTV_Cargo
};

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API ASelectablePawnMaster : public AHpPawnMaster, public ICommands
{
	GENERATED_BODY()
public:

	ASelectablePawnMaster(const FObjectInitializer& ObjectInitializer);

	// Contains references to selectionBox, decals and associated flags.
	UFUNCTION(BlueprintCallable)
	inline USelectionComponent* GetSelectionComponent() const { return SelectionComponent; };

	// Updates selection logic on the component which also updates the selection decal.
	void SetUnitSelected(const bool bIsSelected) const;

	inline OrderTypeVehicle GetVehicleType() const { return VehicleType; };

	inline bool GetIsHidden() const { return bIsHidden; }

	// ICommand interface helpers.
	virtual FVector GetOwnerLocation() const override final;
	virtual FString GetOwnerName() const override final;
	virtual AActor* GetOwnerActor()  override final;
	virtual float GetUnitRepairRadius() override;
	

protected:
	virtual void BeginPlay() override;

	virtual void PostInitProperties() override;
	
	UPROPERTY(BlueprintReadOnly, Category="Reference")
	ACPPController* PlayerController;

	// Contains references to selectionBox, decals and associated flags. 
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	USelectionComponent* SelectionComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFowComp* FowComponent;
	
	virtual void PostInitializeComponents() override;
	
	/** @copydoc ICommands::ResetUnitSpecificLogic */
	virtual void SetUnitToIdleSpecificLogic() override;


	/** @copydoc ICommands::GetCommandData()*/
	virtual UCommandData* GetIsValidCommandData() override final ;

	/** @copydoc ICommands::DoneExecutingCommand */
	UFUNCTION(BlueprintCallable, Category = "Commands", meta = (BlueprintProtected = "true"))
	virtual void DoneExecutingCommand(EAbilityID AbilityFinished) override final;

	/** @copydoc ICommands::StopBehaviourTree */
	virtual void StopBehaviourTree() override;

	virtual void StopMovement() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<OrderTypeVehicle> VehicleType;

	// If unit is rendered or not.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsHidden;

	// Contains the data for the commands that this unit can execute using the ICommands interface.
	// Needs to be accessible by derived classes to get data like current command.
	UPROPERTY()
	UCommandData* UnitCommandData;

	virtual void UnitDies(const ERTSDeathType DeathType) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool GetIsValidPlayerControler()const;
	bool GetIsValidSelectionComponent() const;
	bool GetIsSelected() const;

private:

	// Setup the behaviour of the fow component depending on the owner as registered in the RTS Component.
	void PostInitComp_InitFowBehaviour(URTSComponent* MyRTSComponent);


};
