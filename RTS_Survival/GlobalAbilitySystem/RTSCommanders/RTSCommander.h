#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "RTSCommander.generated.h"

class UTexture2D;

// Commanders are not faction-locked: every German faction can pick any of them.
UENUM(blueprintType)
enum class ERTSCommander : uint8
{
	NoCommander,
	BalancedCommander UMETA(DisplayName = "Field Commander"),
	// Appended 2026-11; keep the order, the values are saved in profiles and config.
	LuftwaffeLiaison UMETA(DisplayName = "Luftwaffe Liaison"),
	BarrageMarshal UMETA(DisplayName = "Barrage Marshal"),
	RadixiteHerald UMETA(DisplayName = "Radixite Herald"),
	ScrapWarlord UMETA(DisplayName = "Scrap Warlord"),
	RailEngineer UMETA(DisplayName = "Rail Engineer"),
};

namespace RTSCommanderConstants
{
	// Slot plan: 1 intel (Tier I), 2 economy (Tier I), 3 reinforcement drop (Tier II), 4 air strike (Tier III),
	// 5 signature (Tier III, or Tier IV for a super-weapon), 6 super-heavy drop (Tier IV).
	constexpr int32 AbilitySlotsPerCommander = 6;
}

USTRUCT(BlueprintType)
struct FRTSCommanderSettings
{
	GENERATED_BODY()

	FRTSCommanderSettings();

	// The commander's kit in slot order (see RTSCommanderConstants::AbilitySlotsPerCommander).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EGlobalAbility> GlobalAbilities;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	// Rendered outside Unreal (CommanderPortraits/). The full portrait, plus its character (alpha) and background
	// layers for UIs that move them apart.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Portrait;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> PortraitCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> PortraitBackground;
};