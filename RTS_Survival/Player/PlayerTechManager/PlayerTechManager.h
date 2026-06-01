// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "UObject/Object.h"
#include "PlayerTechManager.generated.h"

class ACPPController;
class UTechnologyEffect;
enum class ETechnology : uint8;
enum class ERTSResourceType : uint8;



/**
 * @brief Owned by the player controller to track researched technologies and apply their effects.
 *
 * Gameplay systems query this manager before allowing tech-gated actions or displaying tech requirements.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerTechManager : public UActorComponent
{
	GENERATED_BODY()

public:
	inline bool HasTechResearched(const ETechnology Tech) const { return M_ResearchedTechs.Contains(Tech); }
	TArray<ETechnology> GetMissingRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const;
	bool HasAllRequiredTechnologies(const TArray<ETechnology>& RequiredTechnologies) const;
	void OnTechResearched(const ETechnology Tech);

	void InitTechsInManager(ACPPController* Controller);

protected:

	virtual void BeginPlay() override;


private:
	TSet<ETechnology> M_ResearchedTechs;

	TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>> M_TechnologyEffectsMap;

	/** Callback when an effect asset is loaded */
	void OnTechnologyEffectLoaded(ETechnology Tech);
};
