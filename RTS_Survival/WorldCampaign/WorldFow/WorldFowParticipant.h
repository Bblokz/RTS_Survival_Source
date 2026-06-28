// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WorldFowParticipant.generated.h"

class UWorldMapFowComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class UWorldFowParticipant : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief Implement on campaign actors so click and outline systems can query cached FOW behavior without knowing actor types.
 */
class RTS_SURVIVAL_API IWorldFowParticipant
{
	GENERATED_BODY()

public:
	virtual UWorldMapFowComponent* GetFowComponent() const = 0;
	virtual bool GetCanBeOutlined() const;
	virtual bool GetCanPrimaryClickInteract() const;
};
