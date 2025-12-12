#include "USavePlayerProfile.h"

USavePlayerProfile::USavePlayerProfile(const FObjectInitializer& ObjectInitializer):
Super(ObjectInitializer), M_MaxCardsInTechResources(0)
{
	
}

void USavePlayerProfile::Initialize(const TArray<ERTSCard>& PlayerAvailableCards,
                                    const TArray<FCardSaveData>& SelectedCards,
                                    const int32 MaxCardsInUnits,
                                    const int32 MaxCardsInTechResources,
                                    const TArray<FNomadicBuildingLayoutData>& TNomadicBuildingLayouts, const TArray<ERTSCard>& NewCardsInUnits, const
                                    TArray<ERTSCard>& NewCardsInTechResources)
{
    M_PLayerAvailableCards = PlayerAvailableCards;
    M_SelectedCards = SelectedCards;
    M_MaxCardsInUnits = MaxCardsInUnits;
    M_MaxCardsInTechResources = MaxCardsInTechResources;
    M_TNomadicBuildingLayouts = TNomadicBuildingLayouts;
	M_CardsInUnits = NewCardsInUnits;
	M_CardsInTechResources = NewCardsInTechResources;
}
