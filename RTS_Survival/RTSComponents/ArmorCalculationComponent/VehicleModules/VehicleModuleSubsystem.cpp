// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleSubsystem.h"

#include "Engine/Texture2D.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UVehicleModuleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeModuleAssets();
}

void UVehicleModuleSubsystem::Deinitialize()
{
	M_OnModuleAssetsReady.Clear();
	M_ModuleDataAsset = nullptr;
	bM_AreModuleAssetsReady = false;
	Super::Deinitialize();
}

void UVehicleModuleSubsystem::InitializeModuleAssets()
{
	const UVehicleModuleSettings* Settings = UVehicleModuleSettings::Get();
	if (Settings == nullptr || Settings->ModuleDataAsset.IsNull())
	{
		// Module state and repair still work; without the asset there are no icons and no behaviour effects.
		RTSFunctionLibrary::ReportError(TEXT("Vehicle Modules project settings have no ModuleDataAsset assigned;")
			TEXT(" module icons and module behaviours are disabled."));
		bM_AreModuleAssetsReady = true;
		return;
	}

	M_ModuleDataAsset = Settings->ModuleDataAsset.LoadSynchronous();
	if (not IsValid(M_ModuleDataAsset))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to load the vehicle module data asset: ")
			+ Settings->ModuleDataAsset.ToString());
		bM_AreModuleAssetsReady = true;
		return;
	}

	BuildModuleAssetCache();
	ValidateAndCacheImageSize();
	bM_AreModuleAssetsReady = true;
	M_OnModuleAssetsReady.Broadcast();
}

void UVehicleModuleSubsystem::BuildModuleAssetCache()
{
	for (int32 TypeIndex = 1; TypeIndex < VehicleModuleBalance::ModuleTypeCount; ++TypeIndex)
	{
		const EVehicleModuleTypes Type = static_cast<EVehicleModuleTypes>(TypeIndex);
		const FVehicleModuleStateAssets* StateAssets = M_ModuleDataAsset->ModulesByType.Find(Type);
		if (StateAssets == nullptr)
		{
			RTSFunctionLibrary::ReportError(TEXT("Vehicle module data asset has no entry for ")
				+ UEnum::GetValueAsString(Type));
			continue;
		}
		// Unassigned behaviour classes are valid; only missing textures are configuration errors.
		if (not IsValid(StateAssets->DamagedTexture) || not IsValid(StateAssets->DestroyedTexture))
		{
			RTSFunctionLibrary::ReportError(TEXT("Vehicle module data asset misses a damaged or destroyed texture for ")
				+ UEnum::GetValueAsString(Type));
		}
		M_CachedAssetsByType[TypeIndex] = *StateAssets;
	}
}

void UVehicleModuleSubsystem::ValidateAndCacheImageSize()
{
	const FVector2D ConfiguredSize = M_ModuleDataAsset->ImageSize;
	const bool bIsValidSize = FMath::IsFinite(ConfiguredSize.X) && FMath::IsFinite(ConfiguredSize.Y)
		&& ConfiguredSize.X > 0.f && ConfiguredSize.Y > 0.f;
	if (bIsValidSize)
	{
		M_ImageSize = ConfiguredSize;
		return;
	}
	RTSFunctionLibrary::ReportError(TEXT("Vehicle module icon ImageSize must be finite and positive;")
		TEXT(" using the default size."));
}

TSubclassOf<UVehicleModuleBehaviour> UVehicleModuleSubsystem::GetModuleBehaviourClass(
	const EVehicleModuleTypes Type,
	const EVehicleModuleState State) const
{
	if (State == EVehicleModuleState::Healthy || not VehicleModuleBalance::GetIsInstallableModuleType(Type))
	{
		return nullptr;
	}
	const FVehicleModuleStateAssets& StateAssets = M_CachedAssetsByType[VehicleModuleBalance::GetModuleTypeIndex(Type)];
	return State == EVehicleModuleState::Destroyed
		       ? StateAssets.DestroyedBehaviourClass
		       : StateAssets.DamagedBehaviourClass;
}

bool UVehicleModuleSubsystem::GetModuleIconStyle(const EVehicleModuleTypes Type, const EVehicleModuleState State,
                                                 UTexture2D*& OutTexture, FVector2D& OutImageSize) const
{
	OutTexture = nullptr;
	OutImageSize = M_ImageSize;
	if (State == EVehicleModuleState::Healthy || not VehicleModuleBalance::GetIsInstallableModuleType(Type))
	{
		return false;
	}
	const FVehicleModuleStateAssets& StateAssets = M_CachedAssetsByType[VehicleModuleBalance::GetModuleTypeIndex(Type)];
	OutTexture = State == EVehicleModuleState::Destroyed ? StateAssets.DestroyedTexture : StateAssets.DamagedTexture;
	return IsValid(OutTexture);
}
