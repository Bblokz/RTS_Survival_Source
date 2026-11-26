// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "WorldUnknownMissionMarkerComponent.generated.h"

class UMaterialInstanceDynamic;

/**
 * @brief The plain grey puck a mission shows while the fog of war hides it. A mission object creates one at runtime,
 * calls SetupUnknownMarker once and then SetIsMarkerShown as the fog changes. Looks the same on every mission and
 * only answers the cursor trace while shown.
 */
UCLASS()
class RTS_SURVIVAL_API UWorldUnknownMissionMarkerComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UWorldUnknownMissionMarkerComponent();

	/**
	 * @brief Gives the marker its mesh, colour and size from UWorldFogHintSettings and puts it above the fog.
	 * @param MapLocation Where the mission stands; only X and Y are used.
	 * @param FogTopZ World Z of the top of the fog cloud the marker has to lie on.
	 * @return false when the marker mesh of the settings is missing; the marker then cannot be shown.
	 */
	bool SetupUnknownMarker(const FVector& MapLocation, float FogTopZ);

	/** Shows or hides the marker; a hidden marker is not hit by the cursor trace. */
	void SetIsMarkerShown(bool bIsShown);

	bool GetIsMarkerShown() const { return bM_IsMarkerShown; }

private:
	void SetupMarkerMaterial();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_MarkerMaterial;

	bool bM_IsMarkerShown = false;
};
