#include "W_CardMenu.h"

#include "LayoutProfileWidgets.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/CardSystem/CardUI/CardHolderWidget/W_CardHolder.h"
#include "RTS_Survival/CardSystem/CardUI/CardScollBox/W_CardScrollBox.h"
#include "RTS_Survival/CardSystem/CardUI/CardSounds/CardSounds.h"
#include "RTS_Survival/CardSystem/CardUI/CardView/W_CardViewer.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/W_NomadicLayout.h"
#include "RTS_Survival/CardSystem/PlayerProfile/PlayerProfileLoader/FRTS_Profile.h"

#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/GameUI/Popup/RTSPopupTypes.h"
#include "RTS_Survival/GameUI/Popup/W_RTSPopup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundCue.h"

// Initialize the static map outside of any functions, in the .cpp file
TMap<ELayoutProfileWidgets, int32> UW_CardMenu::M_TMapLayoutToIndex = {
	{ELayoutProfileWidgets::Widgets_UnitTechResources, 0},
	{ELayoutProfileWidgets::Widgets_NomadicLayouts, 1}
};


void UW_CardMenu::NativeConstruct()
{
	Super::NativeConstruct();
	M_CurrentLayoutProfile = ELayoutProfileWidgets::Widgets_UnitTechResources;
}

void UW_CardMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (GetIsValidCardScrollBox())
	{
		// set the card scroll box height
		UCanvasPanelSlot* ScrollSlot = Cast<UCanvasPanelSlot>(M_CardScrollBox->Slot);
		if (IsValid(ScrollSlot))
		{
			M_OriginalCardScrollBoxHeight = ScrollSlot->GetSize().Y;
		}
	}
}

void UW_CardMenu::OnPopupOK()
{
	// Saves the profile.
	SaveProfileLoadMap();
}

void UW_CardMenu::OnPopupBack()
{
	if (GetIsValidPopupWidget())
	{
		M_PopupWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UW_CardMenu::InitializeCardPicker(const TArray<ERTSCard>& AvailableCards,
                                       const TSubclassOf<UW_RTSCard> InCardClass)
{
	if (not IsValid(M_CardScrollBox))
	{
		RTSFunctionLibrary::ReportError("not able to initialize card picker in CardMenu as the scroll box widget"
			"is not valid");
		return;
	}
	M_CardScrollBox->InitCardScrollBox(AvailableCards, InCardClass, this, M_ScrollBoxHeightMlt);
}

void UW_CardMenu::InitializeUnitCardHolder(const TArray<ERTSCard>& AvailableCards,
                                           const TSubclassOf<UW_RTSCard> InCardClass, const int32 MaxCards)
{
	if (not IsValid(M_UnitCardHolder))
	{
		RTSFunctionLibrary::ReportError("No valid unit card holder found in UW_CardMenu::InitializeUnitCardHolder");
		return;
	}
	M_UnitCardHolder->SetupCardHolder(AvailableCards, InCardClass, MaxCards,
	                                  {ECardType::StartingUnit, ECardType::Empty}, this);
}

void UW_CardMenu::InitializeTechCardHolder(const TArray<ERTSCard>& AvailableCards,
                                           const TSubclassOf<UW_RTSCard> InCardClass, const int32 MaxCards)
{
	if (not IsValid(M_TechResourceCardHolder))
	{
		RTSFunctionLibrary::ReportError("No valid tech card holder found in UW_CardMenu::InitializeTechCardHolder");
		return;
	}
	M_TechResourceCardHolder->SetupCardHolder(AvailableCards, InCardClass, MaxCards,
	                                          {ECardType::Technology, ECardType::Resource, ECardType::Empty}, this);
}

void UW_CardMenu::InitializeNomadicLayout(const TArray<FNomadicBuildingLayoutData>& BuildingsData,
                                          TSubclassOf<UW_NomadicLayoutBuilding> BuildingLayoutClass,
                                          TSubclassOf<UW_RTSCard> InCardClass)
{
	if (not GetIsValidNomadicLayout())
	{
		return;
	}
	M_NomadicLayout->InitNomadicLayout(BuildingsData, BuildingLayoutClass, InCardClass, this);
}

bool UW_CardMenu::GetIsCardViewerValid(UW_CardViewer* CardViewer)
{
	if (IsValid(CardViewer))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("The provided card viewer is not valid in UW_CardMenu::GetIsCardViewerValid.");
	return false;
}

bool UW_CardMenu::GetIsCardValid(UW_RTSCard* Card)
{
	if (IsValid(Card))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("The provided card is not valid in UW_CardMenu::GetIsCardValid.");
	return false;
}

bool UW_CardMenu::GetIsValidCardScrollBox() const
{
	if (IsValid(M_CardScrollBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid card scroll box found in UW_CardMenu::GetIsValidCardScrollBox");
	return false;
}

bool UW_CardMenu::GetIsValidUnitCardHolder() const
{
	if (IsValid(M_UnitCardHolder))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(" Card Menu has invalid Unit Card Holder.");
	return false;
}

bool UW_CardMenu::GetIsValidTechCardHolder() const
{
	if (IsValid(M_TechResourceCardHolder))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(" Card Menu has invalid Tech Card Holder.");
	return false;
}

bool UW_CardMenu::GetIsValidNomadicLayout() const
{
	if (IsValid(M_NomadicLayout))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid nomadic layout found in UW_CardMenu::GetIsValidNomadicLayout");
	return false;
}

bool UW_CardMenu::GetIsValidLayoutSwitcher() const
{
	if (IsValid(M_LayoutSwitcher))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid widget switcher found in UW_CardMenu::GetIsValidLayoutSwitcher");
	return false;
}

bool UW_CardMenu::GetIsValidPopupWidget() const
{
	if (IsValid(M_PopupWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid popup widget found in UW_CardMenu::GetIsValidPopupWidget");
	return false;
}

bool UW_CardMenu::RegisterSelectedCardsAndCheckSlots()
{
	bool bCreatePopup = false;
	M_SelectedCards.Empty();
	TArray<UW_RTSCard*> SelectedCardsOfContainer;
	uint32 EmptyUnitSlots = 0, EmptyTechResourceSlots = 0, EmptyLayoutSlots = 0;
	TArray<ECardType> UnfilledLayouts = {};
	ERTSCard AvailableUnitCard = ERTSCard::Card_Empty, AvailableTechCard = ERTSCard::Card_Empty;
	if (not CheckUnitCardsFilled(EmptyUnitSlots, AvailableUnitCard, SelectedCardsOfContainer))
	{
		bCreatePopup = true;
	}
	M_SelectedCards = SelectedCardsOfContainer;
	SelectedCardsOfContainer.Empty();
	if (not CheckTechResourceCardsFilled(EmptyTechResourceSlots, AvailableUnitCard, AvailableTechCard,
	                                     SelectedCardsOfContainer))
	{
		bCreatePopup = true;
	}
	for (auto EachCard : SelectedCardsOfContainer)
	{
		M_SelectedCards.Add(EachCard);
	}
	SelectedCardsOfContainer.Empty();
	if (not CheckNomadicLayoutsFilled(EmptyLayoutSlots, UnfilledLayouts, SelectedCardsOfContainer))
	{
		bCreatePopup = true;
	}
	for (auto EachCard : SelectedCardsOfContainer)
	{
		M_SelectedCards.Add(EachCard);
	}
	DebugSelectedCards();
	if (bCreatePopup)
	{
		CreateCardsNotFilledPopup(EmptyUnitSlots, EmptyTechResourceSlots, EmptyLayoutSlots, UnfilledLayouts,
		                          AvailableUnitCard, AvailableTechCard);
		return true;
	}
	return false;
}

bool UW_CardMenu::CheckUnitCardsFilled(uint32& OutEmptySlots, ERTSCard& OutAvailableCard,
                                       TArray<UW_RTSCard*>& OutSelectedCards) const
{
	if (not GetIsValidUnitCardHolder() || not GetIsValidCardScrollBox())
	{
		return false;
	}
	const TArray<ERTSCard> SelectedCardEnums = ConvertCardPairsToCardOnly(M_UnitCardHolder);
	const int32 MaxUnitCards = M_UnitCardHolder->GetMaxCardsToHold();
	OutSelectedCards = M_UnitCardHolder->GetSelectedCards();
	if (MaxUnitCards <= SelectedCardEnums.Num())
	{
		OutEmptySlots = 0;
		return true;
	}
	OutEmptySlots = MaxUnitCards - SelectedCardEnums.Num();
	const TArray<ERTSCard> UnitCardsInScrollBox = M_CardScrollBox->GetCardsOfType(ECardType::StartingUnit);
	if (UnitCardsInScrollBox.Num() > 0)
	{
		OutAvailableCard = UnitCardsInScrollBox[0];
		return false;
	}
	return true;
}

bool UW_CardMenu::CheckTechResourceCardsFilled(uint32& OutEmptySlots, ERTSCard& OutAvailableCardResource,
                                               ERTSCard& OutAvailableCardTech,
                                               TArray<UW_RTSCard*>& OutSelectedCards) const
{
	if (not GetIsValidTechCardHolder() || not GetIsValidCardScrollBox())
	{
		return false;
	}

	const TArray<ERTSCard> SelectedCardEnums = ConvertCardPairsToCardOnly(M_TechResourceCardHolder);
	const int32 MaxTechResourceCards = M_TechResourceCardHolder->GetMaxCardsToHold();
	OutSelectedCards = M_TechResourceCardHolder->GetSelectedCards();
	if (MaxTechResourceCards <= SelectedCardEnums.Num())
	{
		OutEmptySlots = 0;
		return true;
	}
	OutEmptySlots = MaxTechResourceCards - SelectedCardEnums.Num();
	TArray<ERTSCard> TechResourceCardsInScollBox = M_CardScrollBox->GetCardsOfType(ECardType::Technology);
	TArray<ERTSCard> ResourceCards = M_CardScrollBox->GetCardsOfType(ECardType::Resource);
	bool bHasCardsLeftOver = false;
	if (ResourceCards.Num() > 0)
	{
		OutAvailableCardResource = ResourceCards[0];
		bHasCardsLeftOver = true;
	}
	if (TechResourceCardsInScollBox.Num() > 0)
	{
		OutAvailableCardTech = TechResourceCardsInScollBox[0];
		bHasCardsLeftOver = true;
	}

	return !bHasCardsLeftOver;
}

bool UW_CardMenu::CheckNomadicLayoutsFilled(uint32& OutEmptySlots,
                                            TArray<ECardType>& UnfilledLayouts,
                                            TArray<UW_RTSCard*>& OutSelectedCards) const
{
	if (not GetIsValidNomadicLayout() || not GetIsValidCardScrollBox())
	{
		return false;
	}
	TMap<ECardType, TArray<ERTSCard>> CardsLeftPerLayout;
	// Get the card type of each layout in the nomad layout.
	for (const auto& EachLayout : M_NomadicLayout->GetAllLayouts())
	{
		CardsLeftPerLayout.Add(EachLayout, M_CardScrollBox->GetCardsOfType(EachLayout));
	}
	// For each layout, count the empty slots, see if there are cards left in the scrollbox that could be placed,
	// and obtain the cards of the current selection.
	return M_NomadicLayout->CheckLayoutsFilled(OutEmptySlots, UnfilledLayouts, OutSelectedCards, CardsLeftPerLayout);
}

void UW_CardMenu::ViewCard(const ERTSCard Card, const ECardViewer Viewer)
{
	if (not GetIsCardViewerValid(M_LeftCardViewer) || not GetIsCardViewerValid(M_RightCardViewer))
	{
		return;
	}
	M_LeftCardViewer->SetVisibility(ESlateVisibility::Hidden);
	M_RightCardViewer->SetVisibility(ESlateVisibility::Hidden);
	switch (Viewer)
	{
	case ECardViewer::Left:
		M_LeftCardViewer->SetVisibility(ESlateVisibility::Visible);
		M_LeftCardViewer->ViewCard(Card);
		break;
	case ECardViewer::Right:
		M_RightCardViewer->SetVisibility(ESlateVisibility::Visible);
		M_RightCardViewer->ViewCard(Card);
		break;
	case ECardViewer::None:
		break;
	}
}

TArray<ERTSCard> UW_CardMenu::ConvertCardPairsToCardOnly(ICardHolder* Cardholder) const
{
	if (not Cardholder)
	{
		RTSFunctionLibrary::ReportError(
			"invalid interface ptr of ICardHolder in UW_CardMenu::ConvertCardPairsToCardOnly");
		return {};
	}
	auto CardPairs = Cardholder->GetNonEmptyCardsHeld();
	TArray<ERTSCard> Cards;
	for (const auto& EachPair : CardPairs)
	{
		Cards.Add(EachPair.Key);
	}
	return Cards;
}

void UW_CardMenu::SaveProfileLoadMap()
{
	if (not GetIsValidCardScrollBox() || not GetIsValidUnitCardHolder() || not GetIsValidTechCardHolder() || not
		GetIsValidNomadicLayout())
	{
		return;
	}
	TArray<ERTSCard> PlayerAvailableCards;
	auto CardsScrollBox = M_CardScrollBox->GetNonEmptyCardsHeld();
	for (auto EachCardInBox : CardsScrollBox)
	{
		PlayerAvailableCards.Add(EachCardInBox.Key);
	}
	TArray<FCardSaveData> SelectedCardData;

	for (auto EachCard : M_SelectedCards)
	{
		FCardSaveData CardData;
		CardData.CardType = EachCard->GetCardType();
		CardData.Resources = EachCard->GetResources();
		CardData.Technology = EachCard->GetTechnology();
		CardData.TrainingOption = EachCard->GetTrainngOption();
		CardData.UnitToCreate = EachCard->GetUnitToCreate();
		SelectedCardData.Add(CardData);
	}
	TArray<ERTSCard> CardsInUnits = ConvertCardPairsToCardOnly(M_UnitCardHolder);
	TArray<ERTSCard> CardsInTechResources = ConvertCardPairsToCardOnly(M_TechResourceCardHolder);
	const int32 MaxUnitCards = M_UnitCardHolder->GetMaxCardsToHold();
	const int32 MaxTechCards = M_TechResourceCardHolder->GetMaxCardsToHold();
	TArray<FNomadicBuildingLayoutData> NomadicLayouts = M_NomadicLayout->GetBuildingLayoutData();
	// for (auto EachSavedData : M_NomadicLayout->GetNomadicLayoutCardSaveData())
	// {
	// 	SelectedCardData.Add(EachSavedData);
	// }


	if (FRTS_Profile::SaveNewPlayerProfile(PlayerAvailableCards, SelectedCardData, MaxUnitCards, MaxTechCards,
	                                       NomadicLayouts,
	                                       CardsInUnits, CardsInTechResources))
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, LevelToLoad);
	}
}

void UW_CardMenu::CreateCardsNotFilledPopup(const uint32& EmptyUnitSlots, const uint32 EmptyTechResourceSlots,
                                            const uint32 EmptyLayoutslots, const TArray<ECardType>& UnfilledLayouts,
                                            const ERTSCard& AvailableUnitCard,
                                            const ERTSCard& AvailableTechResourceCard)
{
	FText Title = FText::FromString("Profile cards are not filled");
	FText Message = FText::FromString(
		"Some card slots are still empty while there are cards available. Are you sure you want to start?");
	FText UnitTechResourcesDetails;

	if (AvailableUnitCard != ERTSCard::Card_Empty)
	{
		UnitTechResourcesDetails = FText::Format(
			NSLOCTEXT("CardMenu", "AvailableUnitSlots",
			          "There are cards available for the start unit slots, for which you have {0} empty slots."),
			FText::AsNumber(EmptyUnitSlots));
	}
	else if (AvailableTechResourceCard != ERTSCard::Card_Empty)
	{
		UnitTechResourcesDetails = FText::Format(
			NSLOCTEXT("CardMenu", "AvailableTechSlots",
			          "There are cards available for the tech resource slots, for which you have {0} empty slots."),
			FText::AsNumber(EmptyTechResourceSlots));
	}

	// Format Layout details dynamically based on UnfilledLayouts array
	FText NomadicLayoutDetails;
	if (!UnfilledLayouts.IsEmpty())
	{
		FString LayoutDetails = "There are cards available for the following layouts:";
		for (const auto& EachLayout : UnfilledLayouts)
		{
			switch (EachLayout)
			{
			case ECardType::BarracksTrain:
				LayoutDetails += " Barracks";
				break;
			case ECardType::ForgeTrain:
				LayoutDetails += " Forge";
				break;
			case ECardType::MechanicalDepotTrain:
				LayoutDetails += " Mechanical Depot";
				break;
			default:
				break;
			}
		}

		// Add empty layout slots detail
		NomadicLayoutDetails = FText::Format(
			NSLOCTEXT("CardMenu", "NomadicLayoutDetails",
			          "{0}\nThe nomadic layouts have in total {1} empty slots."),
			FText::FromString(LayoutDetails),
			FText::AsNumber(EmptyLayoutslots)
		);
	}


	// Combine all parts into a final formatted message
	FText FinalMessage = FText::Format(
		NSLOCTEXT("CardMenu", "CombinedPopupText", "{0}\n\n{1}\n\n{2}"),
		Message,
		UnitTechResourcesDetails,
		NomadicLayoutDetails);

	EnablePopupWidget(ERTSPopup::OKBack, Title, FinalMessage);
}

void UW_CardMenu::EnablePopupWidget(const ERTSPopup Popup, const FText& Title, const FText& Message)
{
	if (!GetIsValidPopupWidget())
	{
		return;
	}
	M_PopupWidget->SetVisibility(ESlateVisibility::Visible);
	M_PopupWidget->SetPopupType(Popup, Title, Message);
}

void UW_CardMenu::DebugSelectedCards()
{
	RTSFunctionLibrary::PrintToLog("Selected cards: ", false);
	for (auto EachCard : M_SelectedCards)
	{
		RTSFunctionLibrary::PrintToLog("Selected card: " + Global_GetCardAsString(EachCard->GetCard()), false);
	}
}


void UW_CardMenu::InitCardMenu(
	UW_CardHolder* NewTechCardHolder,
	UW_CardHolder* NewUnitCardHolder,
	UW_CardScrollBox* NewCardScrollBox,
	const TSubclassOf<UW_RTSCard> InCardClass,
	UW_CardViewer* LeftCardViewer, UW_CardViewer*
	RightCardViewer, const float ScrollBoxHeightMlt,
	UWidgetSwitcher* LayoutSwitcher,
	UW_NomadicLayout* NomadicLayoutWidget,
	const TSubclassOf<UW_NomadicLayoutBuilding> NomadicBuildingLayoutClass,
	TMap<ECardSound, USoundCue*>
	NewSoundMap, UW_RTSPopup* NewPopupWidget)
{
	M_UnitCardHolder = NewUnitCardHolder;
	M_TechResourceCardHolder = NewTechCardHolder;
	M_CardScrollBox = NewCardScrollBox;
	M_BlueprintCardClass = InCardClass;
	M_NomadicBuildingLayoutClass = NomadicBuildingLayoutClass;
	M_NomadicLayout = NomadicLayoutWidget;
	M_LeftCardViewer = LeftCardViewer;
	M_RightCardViewer = RightCardViewer;
	M_ScrollBoxHeightMlt = ScrollBoxHeightMlt;
	M_LayoutSwitcher = LayoutSwitcher;
	M_MapSoundToCue = NewSoundMap;
	M_PopupWidget = NewPopupWidget;
	if (GetIsValidPopupWidget())
	{
		M_PopupWidget->SetVisibility(ESlateVisibility::Hidden);
		M_PopupWidget->InitPopupWidget(this);
	}
	// Set card viewers' Z-order to render above other widgets if they are in a CanvasPanel
	if (GetIsCardViewerValid(M_LeftCardViewer))
	{
		M_LeftCardViewer->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(M_LeftCardViewer->Slot))
		{
			CanvasSlot->SetZOrder(999); // Use a high value to ensure it renders on top
		}
	}

	if (GetIsCardViewerValid(M_RightCardViewer))
	{
		M_RightCardViewer->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(M_RightCardViewer->Slot))
		{
			CanvasSlot->SetZOrder(999); // Similarly, set a high Z-order value
		}
	}

	LoadPlayerProfile();
}

void UW_CardMenu::OnCardHovered(const bool bIsHover, ERTSCard Card, const bool bIsLeftSide)
{
	if (Card == ERTSCard::Card_Empty || Card == ERTSCard::Card_INVALID)
	{
		return;
	}
	if (bIsHover)
	{
		// For cards on the left side, use the right card viewer
		ViewCard(Card, bIsLeftSide ? ECardViewer::Right : ECardViewer::Left);
		return;
	}
	ViewCard(ERTSCard::Card_INVALID, ECardViewer::None);
}

void UW_CardMenu::OnMouseInScrollBox(const bool bIsEnterScrollBox)
{
	if (not GetIsValidCardScrollBox())
	{
		return;
	}
	// get the canvas slot of the scroll box
	UCanvasPanelSlot* ScrollSlot = Cast<UCanvasPanelSlot>(M_CardScrollBox->Slot);
	if (IsValid(ScrollSlot))
	{
		// set the size of the scroll box
		ScrollSlot->SetSize(FVector2D(ScrollSlot->GetSize().X,
		                              bIsEnterScrollBox
			                              ? M_OriginalCardScrollBoxHeight * M_ScrollBoxHeightMlt
			                              : M_OriginalCardScrollBoxHeight));
	}
}

void UW_CardMenu::SwitchLayout(const ELayoutProfileWidgets NewLayoutProfile)
{
	if (not GetIsValidLayoutSwitcher())
	{
		return;
	}
	if (M_CurrentLayoutProfile == NewLayoutProfile)
	{
		return;
	}
	if (M_TMapLayoutToIndex.Contains(NewLayoutProfile))
	{
		M_LayoutSwitcher->SetActiveWidgetIndex(M_TMapLayoutToIndex[NewLayoutProfile]);
		M_CurrentLayoutProfile = NewLayoutProfile;
		PlayRTSCardSound(ECardSound::Sound_SwitchLayout);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"The provided layout profile is not set in the TMapLayoutToIndex in UW_CardMenu::SwitchLayout");
	}
}

void UW_CardMenu::PlayRTSCardSound(const ECardSound Sound)
{
	// Check if the sound type is in the map and is valid
	if (!M_MapSoundToCue.Contains(Sound) || !IsValid(M_MapSoundToCue[Sound]))
	{
		return;
	}

	// Handle each sound type specifically
	switch (Sound)
	{
	case ECardSound::Sound_Hover:
		if (bCanPlayHoverSound)
		{
			Play2DSound(Sound);
			bCanPlayHoverSound = false;

			// Set a timer to re-enable hover sound after 0.5 seconds
			GetWorld()->GetTimerManager().SetTimer(
				HoverSoundCooldownTimer, this, &UW_CardMenu::ResetHoverSoundCooldown, 0.8f, false);
		}
		break;

	case ECardSound::Sound_StartDrag:
	case ECardSound::Sound_Drop:
	case ECardSound::Sound_DropFailed:
	case ECardSound::Sound_SwitchLayout:
		// Play other sounds immediately
		Play2DSound(Sound);
		break;

	default:
		break;
	}
}

UW_CardScrollBox* UW_CardMenu::GetCardScrollBox() const
{
	return M_CardScrollBox;
}

void UW_CardMenu::Play2DSound(const ECardSound Sound)
{
	// Plays the sound associated with the provided type
	if (IsValid(M_MapSoundToCue[Sound]))
	{
		UGameplayStatics::PlaySound2D(this, M_MapSoundToCue[Sound]);
	}
}

void UW_CardMenu::ResetHoverSoundCooldown()
{
	// Resets the hover sound cooldown to allow playing again
	bCanPlayHoverSound = true;
}


void UW_CardMenu::OnClickStartGame()
{
	if (RegisterSelectedCardsAndCheckSlots())
	{
		return;
	}
	SaveProfileLoadMap();
}

void UW_CardMenu::LoadPlayerProfile()
{
	TArray<ERTSCard> AvailableCards, CardsInUnits, CardsInTechResources;
	int32 MaxUnitCards, MaxTechCards;
	TArray<FNomadicBuildingLayoutData> BuildingLayouts;


	const USavePlayerProfile* LoadedProfile = FRTS_Profile::LoadPlayerProfile();
	if (!LoadedProfile)
	{
		AvailableCards = M_TestCards;
		MaxUnitCards = TestCard_MaxUnitCards;
		MaxTechCards = TestCard_MaxTechCards;
		BuildingLayouts = {};
		CardsInUnits = M_TestCards_UnitHolder;
		CardsInTechResources = M_TestCards_TechHolder;
	}
	else
	{
		// Get the available cards
		AvailableCards = LoadedProfile->M_PLayerAvailableCards;
		MaxUnitCards = LoadedProfile->M_MaxCardsInUnits;
		MaxTechCards = LoadedProfile->M_MaxCardsInTechResources;
		BuildingLayouts = LoadedProfile->M_TNomadicBuildingLayouts;
		CardsInUnits = LoadedProfile->M_CardsInUnits;
		CardsInTechResources = LoadedProfile->M_CardsInTechResources;
	}

	// Initialize the card picker with these cards
	InitializeCardPicker(AvailableCards, M_BlueprintCardClass);
	InitializeUnitCardHolder(CardsInUnits, M_BlueprintCardClass, MaxUnitCards);
	InitializeTechCardHolder(CardsInTechResources, M_BlueprintCardClass, MaxTechCards);
	InitializeNomadicLayout(BuildingLayouts, M_NomadicBuildingLayoutClass, M_BlueprintCardClass);
}
