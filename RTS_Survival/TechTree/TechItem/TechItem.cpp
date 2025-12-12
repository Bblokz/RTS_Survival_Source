// UTechItem.cpp

#include "TechItem.h"

#include "Components/ProgressBar.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "Engine/World.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "WTechItemDescription/W_TechItemDescription.h"

UTechItem::UTechItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , Technology(ETechnology::Tech_NONE)
	  , TechProgressBar(nullptr), TechItemDescription(nullptr), bM_HasBeenResearched(false), M_ResearchTime(0),
	  M_ElapsedResearchTime(0)
{
}

void UTechItem::NativeConstruct()
{
	Super::NativeConstruct();


	if (IsValid(TechProgressBar))
	{
		TechProgressBar->SetVisibility(ESlateVisibility::Hidden);
	}
}


#if WITH_EDITOR
void UTechItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// order tech costs; radixtie, metal, vehicle parts, fuel, ammo, weapon blueprint, vehicle blueprint,
	// construction blueprint, energy blueprint.
	TechCost.M_ResourceCostsMap.KeySort([](const ERTSResourceType& A, const ERTSResourceType& B)
	{
		return A < B;
	});

	OnChangeAllVisuals(false);
}


#endif
void UTechItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Initialize the player tech manager
	GetIsValidPlayerTechManager();
	// Initialize the player resource manager
	GetIsValidPlayerResourceManager();
}

void UTechItem::OnChangeAllVisuals(const bool bHasBeenResearched)
{
	OnChangeTechItemVisuals(Technology, bHasBeenResearched);
	if (IsValid(TechItemDescription))
	{
		TechItemDescription->InitTechItemDescription(Technology, TechCost.M_ResourceCostsMap, TechCost.M_TechTime,
		                                             bHasBeenResearched);
	}
}


bool UTechItem::GetIsValidPlayerResourceManager()
{
	if (IsValid(M_PlayerResourceManager))
	{
		return true;
	}

	if (UPlayerResourceManager* NewResourceManager = FRTS_Statics::GetPlayerResourceManager(this))
	{
		M_PlayerResourceManager = NewResourceManager;
		return true;
	}
	RTSFunctionLibrary::ReportError("Failed to get PlayerResourceManager in UTechItem");
	return false;
}

bool UTechItem::GetIsValidPlayerTechManager()
{
	if (IsValid(M_PlayerTechManager))
	{
		return true;
	}

	if (UPlayerTechManager* NewTechManager = FRTS_Statics::GetPlayerTechManager(this))
	{
		M_PlayerTechManager = NewTechManager;
		return true;
	}
	RTSFunctionLibrary::ReportError("Failed to get PlayerTechManager in UTechItem");
	return false;
}

void UTechItem::OnClickedTech()
{
	if (!GetIsValidPlayerTechManager() || bM_HasBeenResearched || M_ResearchTimerHandle.IsValid())
	{
		if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString(
				"Already researched or research in progress or invalid player tech manager");
		}
		return;
	}

	// Check if this tech has already been researched
	if (M_PlayerTechManager->HasTechResearched(Technology))
	{
		// Tech already researched
		RTSFunctionLibrary::ReportError("Technology already researched: " + UEnum::GetValueAsString(Technology));
		return;
	}

	// Check if all requirements have been researched
	for (UTechItem* RequiredTechItem : Requirements)
	{
		if (!IsValid(RequiredTechItem))
		{
			RTSFunctionLibrary::ReportError("Invalid tech item in requirements for " + GetName());
			return;
		}

		ETechnology RequiredTech = RequiredTechItem->Technology;
		if (!M_PlayerTechManager->HasTechResearched(RequiredTech))
		{
			// Required tech not researched
			if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString(
					"Required tech not researched: " + UEnum::GetValueAsString(RequiredTech), FColor::Red);
			}
			return;
		}
	}

	if (GetIsValidPlayerResourceManager())
	{
		if (M_PlayerResourceManager->PayForCosts(TechCost.M_ResourceCostsMap))
		{
			StartResearch(TechCost.M_TechTime);
		}
		else if (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Not enough resources to research " + UEnum::GetValueAsString(Technology),
			                                FColor::Red);
		}
	}
}

void UTechItem::StartResearch(const float TechResearchTime)
{
	M_ResearchTime = TechResearchTime;
	// Set research time from the tech cost
	M_ElapsedResearchTime = 0.0f;

	// Make the progress bar visible
	if (IsValid(TechProgressBar))
	{
		TechProgressBar->SetVisibility(ESlateVisibility::Visible);
		TechProgressBar->SetPercent(0.0f);
	}

	// Start a timer to update research progress every tick (e.g., every 0.1 seconds)
	const float UpdateInterval = 0.1f;
	GetWorld()->GetTimerManager().SetTimer(M_ResearchTimerHandle, this, &UTechItem::UpdateResearchProgress,
	                                       UpdateInterval, true);
}


void UTechItem::UpdateResearchProgress()
{
	// Increment elapsed time
	const float UpdateInterval = 0.1f; // Same as the timer interval
	M_ElapsedResearchTime += UpdateInterval;

	// Update progress bar
	if (IsValid(TechProgressBar))
	{
		float ProgressPercent = M_ElapsedResearchTime / M_ResearchTime;
		TechProgressBar->SetPercent(ProgressPercent);
	}

	// Check if research is complete
	if (M_ElapsedResearchTime >= M_ResearchTime)
	{
		// Stop the timer
		GetWorld()->GetTimerManager().ClearTimer(M_ResearchTimerHandle);

		// Handle research completion
		OnResearchCompleted();
	}
}

void UTechItem::OnResearchCompleted()
{
	// Mark the technology as researched
	if (IsValid(M_PlayerTechManager))
	{
		M_PlayerTechManager->OnTechResearched(Technology);
	}

	// Update the researched state
	bM_HasBeenResearched = true;

	// Update visuals
	OnChangeAllVisuals(true);


	// Hide or update the progress bar
	if (IsValid(TechProgressBar))
	{
		TechProgressBar->SetPercent(1.0f);
		TechProgressBar->SetVisibility(ESlateVisibility::Hidden);
	}
}
