#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicBuildingLayoutData/NomadicBuildingLayoutData.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "USavePlayerProfile.generated.h"

enum class ERTSCard : uint8;

class AActor;
enum class ETechnology : uint8;
struct FUnitCost;

USTRUCT(BlueprintType)
struct FCardSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ECardType CardType = ECardType::StartingUnit;

	// For Unit cards
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FTrainingOption UnitToCreate;

	// For Technology cards
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ETechnology Technology = static_cast<ETechnology>(0);

	// For Resource cards
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FUnitCost Resources;

	// For Training Option Cards in nomadic building layouts.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FTrainingOption TrainingOption;
};


UCLASS()
class RTS_SURVIVAL_API USavePlayerProfile : public USaveGame
{
	GENERATED_BODY()

public:
	USavePlayerProfile(const FObjectInitializer& ObjectInitializer);

	void Initialize(const TArray<ERTSCard>& PlayerAvailableCards,
	                const TArray<FCardSaveData>& SelectedCards,
	                int32 MaxCardsInUnits,
	                int32 MaxCardsInTechResources,
	                const TArray<FNomadicBuildingLayoutData>& TNomadicBuildingLayouts, const TArray<ERTSCard>& NewCardsInUnits, const
	                TArray<ERTSCard>& NewCardsInTechResources);

	UPROPERTY(SaveGame)
	TArray<ERTSCard> M_PLayerAvailableCards;

	UPROPERTY(SaveGame)
	TArray<FCardSaveData> M_SelectedCards;

	UPROPERTY(SaveGame)
	TArray<ERTSCard> M_CardsInUnits;

	UPROPERTY(SaveGame)
	TArray<ERTSCard> M_CardsInTechResources;

	UPROPERTY(SaveGame)
	int32 M_MaxCardsInUnits = 0;

	UPROPERTY(SaveGame)
	int32 M_MaxCardsInTechResources = 0;

	UPROPERTY(SaveGame)
	TArray<FNomadicBuildingLayoutData> M_TNomadicBuildingLayouts;
};
