// Source/RTS_Survival/RTSGamePlayTags/FRTSGamePlayTags.h
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Central definition of RTS gameplay tags (native).
 * Declare your tags here with UE_DECLARE_GAMEPLAY_TAG_EXTERN at global scope.
 */

// Components
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Component_ScavengeMesh);

/**
 * Convenience accessors for C++ call sites that prefer a type wrapper.
 * Note: No static data here to avoid init-order issues; we return the native tag on demand.
 */
struct FRTSGamePlayTags final
{
	/** @return Gameplay tag that marks a mesh component used for scavenge socket discovery. */
	static FORCEINLINE FGameplayTag GetScavengeMeshComponentTag()
	{
		return TAG_Component_ScavengeMesh; // implicitly convertible to FGameplayTag
	}

private:
	FRTSGamePlayTags() = delete;
};
