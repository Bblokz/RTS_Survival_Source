// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DigInType/DigInType.h"
#include "DigInWall/DigInWall.h"
#include "RTS_Survival/UnitData/DigInData.h"
#include "DigInComponent.generated.h"


class IDigInUnit;

UENUM()
enum class EDigInStatus
{
	None,
	Movable,
	BuildingWall,
	CompletedBuildingWall,
};
class ADigInWall;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UDigInComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UDigInComponent();

	void OnWallCompletedBuilding(const ADigInWall* WallCompleted);
	void OnWallDestroyed(const ADigInWall* WallDestroyed);
	void SetupOwner(const TScriptInterface<IDigInUnit>& NewOwner);
	/** @return Whether the digin wall state is so that the unit can move. */
	bool GetIsMovable() const { return M_DigInStatus == EDigInStatus::Movable; }

	void ExecuteDigInCommand();
	void TerminateDigInCommand();
	void OnOwningUnitDeath();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<ADigInWall> WallActorClass;

	// Defines what data to load for this dig in component using the game state.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EDigInType DigInType = EDigInType::None;

	// Offset used in direction of the forward vector of the owner to place the dig in wall.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DigInOffset = 100.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector DigInWallScaling = FVector(1.0f, 1.0f, 1.0f);

	
private:
	bool EnsureIsValidWallActorClass() const;

	FVector GetWallLocation() const;
	bool SpawnDigInWallActor(const FVector& SpawnLocation);

	// The wall actor spawned by this component.
	TWeakObjectPtr<ADigInWall> M_DigInWallActor;
	bool GetIsValidDigInWallActor() const;

	FDigInData M_DigInData;

	EDigInStatus M_DigInStatus = EDigInStatus::None;

	UPROPERTY()
	TScriptInterface<IDigInUnit> M_OwningDigInUnit;
	bool GetIsValidOwningDigInUnit() const;


	bool GetOwningPlayerFromOwner(int32& OutOwningPlayer) const;

	bool EnsureCallingWallIsOwnedByComponent(const ADigInWall* CallingWall) const;

	
	bool EnsureScalingVectorIsValid(const FVector& InVector) const;
	
};
