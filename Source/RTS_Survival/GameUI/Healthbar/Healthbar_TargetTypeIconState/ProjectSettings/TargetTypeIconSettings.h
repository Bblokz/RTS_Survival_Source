#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Slate/SlateBrushAsset.h"
#include "TargetTypeIconSettings.generated.h"

enum class ETargetTypeIcon : uint8;

USTRUCT(BlueprintType)
struct FTargetTypeIconBrushes_Soft
{
	GENERATED_BODY()

	/** Player brush (soft ref so it can live in config). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Icons")
	TSoftObjectPtr<USlateBrushAsset> PlayerBrush;

	/** Enemy brush (soft ref so it can live in config). */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Icons")
	TSoftObjectPtr<USlateBrushAsset> EnemyBrush;
};

/**
 * @brief Project settings for mapping target type -> (player/enemy) brushes.
 * Appears under Project Settings as: UI ► Health Bar Icons (configurable per project).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Health Bar Icons"))
class RTS_SURVIVAL_API UTargetTypeIconSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTargetTypeIconSettings();

	/** Configure target-type icon brushes globally for the project. */
	UPROPERTY(EditAnywhere, Config, Category="Icons", meta=(ForceInlineRow))
	TMap<ETargetTypeIcon, FTargetTypeIconBrushes_Soft> TypeToBrush;


	/** Convenience accessor. */
	static const UTargetTypeIconSettings* Get()
	{
		return GetDefault<UTargetTypeIconSettings>();
	}

	/**
	 * Resolve soft references to hard pointers for runtime use.
	 * Loads assets synchronously the first time (they’ll stay in memory afterwards).
	 */
	void ResolveTypeToBrushMap(TMap<ETargetTypeIcon, struct FTargetTypeIconBrushes>& OutMap) const;
};
