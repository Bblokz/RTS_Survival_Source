#pragma once

#include "CoreMinimal.h"

#include "AttachmentAnimationState.generated.h"

// What types of animations to play on this attachment.
UENUM(Blueprintable)
enum class EAttachmentAnimationState :uint8
{
	MontageAndAO,
	AOOnly,
	MontageOnly
};
