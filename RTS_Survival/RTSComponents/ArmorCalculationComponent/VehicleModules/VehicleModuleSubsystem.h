// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VehicleModuleDataAsset.h"

#include "VehicleModuleSubsystem.generated.h"

/**
 * @brief Loads the shared vehicle module asset once per game instance (dedicated servers included) and
 * compiles it into an enum-indexed cache, so tanks never load assets or search maps on impact.
 */
UCLASS()
class RTS_SURVIVAL_API UVehicleModuleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** @return The optional behaviour for the state; Healthy and unassigned states return null without error. */
	TSubclassOf<UVehicleModuleBehaviour> GetModuleBehaviourClass(EVehicleModuleTypes Type,
	                                                             EVehicleModuleState State) const;

	/**
	 * @brief Resolves the healthbar icon of a non-healthy module type.
	 * @param Type Installable module type.
	 * @param State Damaged or Destroyed.
	 * @param OutTexture [out] Texture to show; null when not configured.
	 * @param OutImageSize [out] Validated icon size.
	 * @return True if a texture is configured for the type and state.
	 */
	bool GetModuleIconStyle(EVehicleModuleTypes Type, EVehicleModuleState State, UTexture2D*& OutTexture,
	                        FVector2D& OutImageSize) const;

	bool GetAreModuleAssetsReady() const { return bM_AreModuleAssetsReady; }

	FSimpleMulticastDelegate& GetOnModuleAssetsReady() { return M_OnModuleAssetsReady; }

private:
	void InitializeModuleAssets();
	void BuildModuleAssetCache();
	void ValidateAndCacheImageSize();

	// Retained so textures and behaviour classes stay loaded for the game instance's lifetime.
	UPROPERTY()
	TObjectPtr<UVehicleModuleDataAsset> M_ModuleDataAsset = nullptr;

	// Enum-indexed copy of the asset map; entries of unconfigured types stay empty.
	UPROPERTY()
	FVehicleModuleStateAssets M_CachedAssetsByType[VehicleModuleBalance::ModuleTypeCount];

	FVector2D M_ImageSize = FVector2D(VehicleModuleBalance::DefaultModuleIconWidth,
	                                  VehicleModuleBalance::DefaultModuleIconHeight);

	bool bM_AreModuleAssetsReady = false;

	FSimpleMulticastDelegate M_OnModuleAssetsReady;
};
