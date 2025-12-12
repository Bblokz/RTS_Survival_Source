// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "PlayerTechManager.h"

#include "Engine/AssetManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UPlayerTechManager::OnTechResearched(ETechnology Tech)
{
	M_ResearchedTechs.Add(Tech);
	if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		const FString TechName = UEnum::GetValueAsString(Tech);
		RTSFunctionLibrary::PrintString("Technology researched: " + TechName);
	}

	// Find the soft reference to the effect class
	if (const TSoftClassPtr<UTechnologyEffect>* EffectClassPtr = M_TechnologyEffectsMap.Find(Tech))
	{
		if (EffectClassPtr->IsValid())
		{
			// Class asset is already loaded
			UClass* EffectClass = EffectClassPtr->Get();
			if (EffectClass)
			{
				// Create an instance of the class
				UTechnologyEffect* TechEffect = NewObject<UTechnologyEffect>(this, EffectClass);
				if (TechEffect)
				{
					TechEffect->ApplyTechnologyEffect(GetWorld());
				}
				else
				{
					RTSFunctionLibrary::ReportError(
						"Failed to create technology effect instance for " + UEnum::GetValueAsString(Tech));
				}
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					"Failed to get technology effect class for " + UEnum::GetValueAsString(Tech));
			}
		}
		else
		{
			// Class asset is not loaded, load it asynchronously
			FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
			// Ensure that the Tech parameter is captured correctly
			FSoftObjectPath AssetPath = EffectClassPtr->ToSoftObjectPath();
			Streamable.RequestAsyncLoad(
				AssetPath,
				FStreamableDelegate::CreateUObject(this, &UPlayerTechManager::OnTechnologyEffectLoaded, Tech));
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"The technology effect for " + UEnum::GetValueAsString(Tech) +
			" is not found in the TechnologyEffectsMap.\nAdd the effect to the map in the PlayerTechManager actor component.");
	}
}

void UPlayerTechManager::InitTechsInManager(ACPPController* Controller)
{
	if (IsValid(Controller))
	{
		M_TechnologyEffectsMap = Controller->GetTechnologyEffectsMap();
		if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
		{
			for (auto EachTech : M_TechnologyEffectsMap)
			{
				FString TechName = UEnum::GetValueAsString(EachTech.Key);
				FString TechEffect = EachTech.Value.IsValid() ? EachTech.Value->GetName() : "INVALID";
				RTSFunctionLibrary::PrintString("Technology: " + TechName + " Effect: " + TechEffect);
			}
		}

		return;
	}
	RTSFunctionLibrary::ReportNullErrorInitialisation(this, "Controller", "InitTechsInManager");
}

void UPlayerTechManager::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerTechManager::OnTechnologyEffectLoaded(ETechnology Tech)
{
	// Find the soft reference to the effect class
	if (const TSoftClassPtr<UTechnologyEffect>* EffectClassPtr = M_TechnologyEffectsMap.Find(Tech))
	{
		if (EffectClassPtr->IsValid())
		{
			UClass* EffectClass = EffectClassPtr->Get();
			if (EffectClass)
			{
				// Create an instance of the class
				UTechnologyEffect* TechEffect = NewObject<UTechnologyEffect>(this, EffectClass);
				if (TechEffect)
				{
					TechEffect->ApplyTechnologyEffect(GetWorld());
					return;
				}
				RTSFunctionLibrary::ReportError(
					"Failed to create technology effect instance for " + UEnum::GetValueAsString(Tech));
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					"Failed to get technology effect class after loading for " + UEnum::GetValueAsString(Tech));
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError(
				"Technology effect class for " + UEnum::GetValueAsString(Tech) +
				" is still not loaded after async load.");
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"The technology effect for " + UEnum::GetValueAsString(Tech) +
			" is not found in the TechnologyEffectsMap.");
	}
}
