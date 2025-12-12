// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/RichTextBlockDecorator.h"
#include "ArmorOffsetDecorator.generated.h"

/**
 * 
// TODO THIS DOES NOT WORK; parsing problem with arguments...
 */
UCLASS()
class RTS_SURVIVAL_API UArmorOffsetDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

protected:
	/** Override this function to handle custom tags like <armor> */
	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;
};
