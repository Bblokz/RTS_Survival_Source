#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
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
	// Round 3 (2026-11-05), appended for the same reason.
	LightBringer UMETA(DisplayName = "Light Bringer"),
	Feuersturm UMETA(DisplayName = "Feuersturm"),
	Stahllaeufer UMETA(DisplayName = "Stahlläufer"),
	Festungsbaumeister UMETA(DisplayName = "Festungsbaumeister"),
	Sturmkommandant UMETA(DisplayName = "Sturmkommandant"),
	// Round 3d (2026-11-05), appended for the same reason.
	SkyDropQuartermaster UMETA(DisplayName = "Sky-Drop Quartermaster"),
};

namespace RTSCommanderConstants
{
	// Slot plan (Docs/Abilities/Commanders.md): 1 intel (Tier I), 2 small reinforcements (Tier I), 3 economy (Tier I),
	// 4 reinforcements (Tier II), 5 air strike (Tier III), 6 signature (Tier III, or Tier IV for a super-weapon),
	// 7 super-heavy drop (Tier IV).
	constexpr int32 AbilitySlotsPerCommander = 7;

	// Lowest and highest tier an ability may have in each slot, by slot index (0 = slot 1).
	constexpr EGlobalAbilityTier SlotMinTiers[AbilitySlotsPerCommander] = {
		EGlobalAbilityTier::TierI, EGlobalAbilityTier::TierI, EGlobalAbilityTier::TierI, EGlobalAbilityTier::TierII,
		EGlobalAbilityTier::TierIII, EGlobalAbilityTier::TierIII, EGlobalAbilityTier::TierIV
	};
	constexpr EGlobalAbilityTier SlotMaxTiers[AbilitySlotsPerCommander] = {
		EGlobalAbilityTier::TierI, EGlobalAbilityTier::TierI, EGlobalAbilityTier::TierI, EGlobalAbilityTier::TierII,
		EGlobalAbilityTier::TierIII, EGlobalAbilityTier::TierIV, EGlobalAbilityTier::TierIV
	};
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