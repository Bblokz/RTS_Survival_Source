// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_LightRadixiteArmor.generated.h"

enum class EArmorPlate : uint8;
class ATankMaster;

UCLASS()
class RTS_SURVIVAL_API UTE_LightRadixiteArmor : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	virtual void ApplyTechnologyEffect(const UObject* WorldContextObject) override;

protected:
	/** Override to provide the target actor class */
	virtual TSet<TSubclassOf<AActor>> GetTargetActorClasses() const override;

	/** Override to apply changes to the found actors */
	virtual void OnApplyEffectToActor(AActor* ValidActor) override;

	/** The class of tanks to search for */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AActor> Panzer38TClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AActor> PanzerIIClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<AActor> Sdkfz140Class;

	/** The new mesh to apply */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* Panzer38TRadixiteMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* PanzerIIRadixiteMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMesh* Sdkfz140RadixiteMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ArmorValueMlt = 1.5;

private:
	/** @return The radixite mesh depending on the class of the actor.*/
	UStaticMesh* GetMeshToApply(AActor* ValidActor) const;

	void ImproveArmor(ATankMaster* ValidTank);

	bool IsArmorTypeToAdjust(EArmorPlate ArmorPlate);
};
