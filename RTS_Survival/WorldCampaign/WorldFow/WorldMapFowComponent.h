// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldMapFowTypes.h"
#include "WorldMapFowComponent.generated.h"

/**
 * @brief Add this component to campaign actors in Blueprint to let the world FOW manager store and query their reveal state.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent, DisplayName="WorldMapFow"))
class RTS_SURVIVAL_API UWorldMapFowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldMapFowComponent();

	EWorldMapFowState GetCurrentFowState() const { return M_CurrentFowState; }
	void SetCurrentFowState(EWorldMapFowState NewState);

	bool GetCanPrimaryClickInteractForCurrentState() const;
	bool GetCanBeOutlinedForCurrentState() const;
	bool GetWritesVisibleMaskForCurrentState() const;
	bool GetWritesExplorableMaskForCurrentState() const;
	bool GetWritesPOIMaskForCurrentState() const;
	float GetRevealRadiusForCurrentState() const;
	float GetRevealFalloffForCurrentState() const;
	float GetConnectionCorridorWidthForCurrentState() const;
	bool GetCanBecomePOIVisible() const { return bM_CanBecomePOIVisible; }
	float GetPOIRevealRadius() const { return M_POIRevealRadius; }
	float GetPOIRevealFalloff() const { return M_POIRevealFalloff; }
	FVector GetPOIRevealOrigin() const;
	bool GetPOIVisibleRevealsOwningAnchor() const { return bM_POIVisibleRevealsOwningAnchor; }

private:
	const FWorldMapFowStateSettings& GetSettingsForState(EWorldMapFowState State) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW", meta = (AllowPrivateAccess = "true"))
	EWorldMapFowState M_CurrentFowState = EWorldMapFowState::NotVisible;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW", meta = (AllowPrivateAccess = "true"))
	FWorldMapFowStateSettings M_NotVisibleSettings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW", meta = (AllowPrivateAccess = "true"))
	FWorldMapFowStateSettings M_ExplorableSettings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW", meta = (AllowPrivateAccess = "true"))
	FWorldMapFowStateSettings M_VisibleSettings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW", meta = (AllowPrivateAccess = "true"))
	FWorldMapFowStateSettings M_POIVisibleSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW|POI", meta = (AllowPrivateAccess = "true"))
	bool bM_CanBecomePOIVisible = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW|POI", meta = (AllowPrivateAccess = "true"))
	float M_POIRevealRadius = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW|POI", meta = (AllowPrivateAccess = "true"))
	float M_POIRevealFalloff = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW|POI", meta = (AllowPrivateAccess = "true"))
	FVector M_POIRevealOriginOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW|POI", meta = (AllowPrivateAccess = "true"))
	bool bM_POIVisibleRevealsOwningAnchor = false;
};
