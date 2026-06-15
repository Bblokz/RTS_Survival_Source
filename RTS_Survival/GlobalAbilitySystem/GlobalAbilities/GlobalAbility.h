// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GlobalAbility.generated.h"

/**
 * UPROPERTY(Instanced) tells Unreal to duplicate/own a unique subobject for that property 
 * and implies inline editing/export behavior,
 * while UCLASS(EditInlineNew) allows the object to be created from the Details panel
 * instead of only referencing an existing asset.
 * DefaultToInstanced makes instances of that class considered instanced by default.
 */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGlobalAbility : public UObject
{
	GENERATED_BODY()
	
	public:
	UGlobalAbility();
	
	void InitGlobalAbility(const int32 OwningPlayer);
	
	virtual void ActivateAbility();
	
	virtual void OnClickedAbilityLocation(const FVector& TargetLocation);
	
private:
	UPROPERTY(Transient)
	int32 M_OwningPlayer = INDEX_NONE; 
};
