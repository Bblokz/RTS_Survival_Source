// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ResourceStorageOwner.generated.h"

enum class ERTSResourceType : uint8;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UResourceStorageOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * This interface is purely for both resources like ACPPResourceMaster, DropOffs and harvesters to update
 * their visuals depending on the amount of resources stored.
 */
class RTS_SURVIVAL_API IResourceStorageOwner
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType) =0;

	virtual void OnResourceStorageEmpty() = 0;
};
