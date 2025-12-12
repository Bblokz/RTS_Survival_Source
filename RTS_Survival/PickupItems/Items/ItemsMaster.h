// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"

#include "ItemsMaster.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API AItemsMaster : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	AItemsMaster(const FObjectInitializer& ObjectInitializer);

	inline void SetItemDisabled()
	{
		bM_IsItemEnabled = false;
	}

	inline bool GetIsItemEnabled() const {return bM_IsItemEnabled;}

private:
	bool bM_IsItemEnabled;
};
