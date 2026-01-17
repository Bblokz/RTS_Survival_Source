// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "PlayerAimAbility.generated.h"

UCLASS()
class RTS_SURVIVAL_API APlayerAimAbility : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APlayerAimAbility();

	void ShowAimRadius(const float Radius, const EPlayerAimAbilityTypes AimType);
	void HideRadius();
	bool IsPlayerAimActive() const { return bM_IsPlayerAimActive; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly)
	UStaticMeshComponent* AimMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TMap<EPlayerAimAbilityTypes, UMaterialInterface*> MaterialPerAbilityAim;

	// How 1 unit of scale increase translates to world units.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float UnitsPerScale = 100.0f;

private:
	bool GetIsValidAimMeshComponent() const;
	bool GetIsValidMaterialForAimType(const EPlayerAimAbilityTypes AimType, UMaterialInterface*& OutMaterial);
	UPROPERTY()
	bool bM_IsPlayerAimActive = false;
};
