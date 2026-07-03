#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldFortificationModificationsComponent.generated.h"

/**
 * @brief Runtime campaign state describing fortification modifiers on a world map object.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWorldFortificationModificationsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldFortificationModificationsComponent();

	/**
	 * @brief Gets the enum modifiers currently applied to this object's fortification strength.
	 * @return Runtime/save-game list of fortification strength modifiers.
	 */
	const TArray<EWorldFortificationStrength>& GetFortificationStrengths() const
	{
		return M_FortificationStrengths;
	}

	/**
	 * @brief Replaces the full modifier list and strips EWorldFortificationStrength::None entries.
	 * @param FortificationStrengths New modifiers to store on this object.
	 */
	void SetFortificationStrengths(const TArray<EWorldFortificationStrength>& FortificationStrengths);

	/**
	 * @brief Adds a unique non-None fortification strength modifier.
	 * @param FortificationStrength Modifier enum to add.
	 */
	void AddFortificationStrength(EWorldFortificationStrength FortificationStrength);

	/**
	 * @brief Removes all instances of the supplied fortification strength modifier.
	 * @param FortificationStrength Modifier enum to remove.
	 */
	void RemoveFortificationStrength(EWorldFortificationStrength FortificationStrength);

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Fortification Modifications",
		meta = (AllowPrivateAccess = "true"))
	TArray<EWorldFortificationStrength> M_FortificationStrengths;
};
