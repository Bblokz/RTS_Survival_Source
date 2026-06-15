// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GlobalAbility.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UGlobalAbility : public UObject
{
	GENERATED_BODY()
	
	public:
	UGlobalAbility();
	
	void InitGlobalAbility(const int32 OwningPlayer);
	
	virtual void ActivateAbility();
	
	virtual void OnClickedAbilityLocation(const FVector& TargetLocation);
	
private:
	UPROPERTY()
	int32 M_OwningPlayer = INDEX_NONE; 
};
