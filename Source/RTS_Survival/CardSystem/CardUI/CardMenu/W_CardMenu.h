// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "LayoutProfileWidgets.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/GameUI/Popup/PopupCaller/PopupCaller.h"
#include "W_CardMenu.generated.h"

enum class ERTSPopup : uint8;
enum class ENomadicLayoutBuildingType : uint8;
class UW_RTSPopup;
enum class ECardSound : uint8;
enum class ELayoutProfileWidgets : uint8;
struct FNomadicBuildingLayoutData;
class UW_NomadicLayoutBuilding;
class UW_NomadicLayout;
class URichTextBlock;
class UW_CardViewer;
class UW_CardScrollBox;
class UScrollBox;
class UW_CardHolder;
class UWidgetSwitcher;

UENUM()
enum class ECardViewer
{
	Left,
	Right,
	None
};

/**
 * @brief Manages the card menu UI, including initialization, card interactions, layout switching, and sound playback.
 *
 * The UW_CardMenu class is responsible for handling the user interface of the card menu in the game.
 * It initializes and manages various UI components such as the card picker (scroll box), unit card holder, tech resource card holder, nomadic layouts, card viewers, and popups.
 * It also handles loading the player profile, registering selected cards, checking for empty slots, and playing associated sounds.
 *
 * Key functionalities include:
 * - Initializing UI components with player data.
 * - Handling card hovering and viewing.
 * - Switching between different layout profiles.
 * - Playing sounds associated with card interactions.
 * - Managing popup messages for incomplete selections.
 *
 * @note This class implements the IPopupCaller interface to handle popup responses.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CardMenu : public UUserWidget, public IPopupCaller
{
	GENERATED_BODY()

public:
	/**
	 * @brief Handles card hover events to display the card in the appropriate card viewer.
	 *
	 * When a card is hovered over, this function determines which card viewer (left or right) to display the card in, based on whether the card is on the left side or not.
	 *
	 * @param bIsHover Indicates whether the card is being hovered (true) or the hover has ended (false).
	 * @param Card The ERTSCard enum value representing the card that is hovered.
	 * @param bIsLeftSide Indicates whether the card is on the left side (true) or not (false), used to determine which card viewer to use.
	 * @note This function ensures that only valid cards are displayed, and hides the card viewers when the hover ends.
	 */
	void OnCardHovered(const bool bIsHover, ERTSCard Card, const bool bIsLeftSide);

	/**
	 * @brief Handles mouse enter and leave events for the card scroll box to adjust its size dynamically.
	 *
	 * When the mouse enters or leaves the card scroll box, this function adjusts the size of the scroll box to provide a better user experience.
	 *
	 * @param bIsEnterScrollBox True if the mouse has entered the scroll box, false if it has left.
	 * @note This function interacts with the scroll box to resize it based on the mouse interaction.
	 */
	void OnMouseInScrollBox(const bool bIsEnterScrollBox);

	/**
	 * @brief Switches the current layout profile to display different UI components.
	 *
	 * This function changes the active widget in the layout switcher based on the provided layout profile, allowing the user to switch between different layouts (e.g., units, tech/resources, nomadic layouts).
	 *
	 * @param NewLayoutProfile The new layout profile to switch to, of type ELayoutProfileWidgets.
	 * @note This function plays a sound when the layout is switched, and only switches if the new layout is different from the current one.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SwitchLayout(const ELayoutProfileWidgets NewLayoutProfile);

	/**
	 * @brief Plays a sound associated with a specific card interaction.
	 *
	 * This function plays a 2D sound cue corresponding to the provided ECardSound enum value, such as hover sounds, drag sounds, or layout switch sounds.
	 *
	 * @param Sound The ECardSound enum value indicating which sound to play.
	 * @note Hover sounds have a cooldown to prevent spamming, managed internally.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void PlayRTSCardSound(const ECardSound Sound);

	/**
	 * @brief Retrieves the card scroll box widget used in the card menu.
	 *
	 * @return A pointer to the UW_CardScrollBox instance used in the card menu.
	 * @note This function provides access to the scroll box for external use if needed.
	 */
	UW_CardScrollBox* GetCardScrollBox() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnInitialized() override;

	/**
	 * @brief Called when the OK button is clicked on a popup.
	 *
	 * Handles the logic to perform when the user confirms a popup message.
	 *
	 * @note This function should be implemented to define specific behaviors on popup confirmation.
	 */
	virtual void OnPopupOK() override;

	/**
	 * @brief Called when the Back button is clicked on a popup.
	 *
	 * Handles the logic to perform when the user cancels or goes back from a popup message.
	 *
	 * @note This function should be implemented to define specific behaviors on popup cancellation.
	 */
	virtual void OnPopupBack() override;

	/**
	 * @brief Initializes the card menu with provided UI components and settings.
	 *
	 * Sets up the card menu by assigning references to UI components, initializing card viewers, loading the player profile, and preparing the UI for interaction.
	 *
	 * @param NewTechCardHolder Pointer to the UW_CardHolder for technology and resource cards.
	 * @param NewUnitCardHolder Pointer to the UW_CardHolder for unit cards.
	 * @param NewCardScrollBox Pointer to the UW_CardScrollBox that contains the available cards.
	 * @param InCardClass The TSubclassOf<UW_RTSCard> representing the blueprint class for card widgets.
	 * @param LeftCardViewer Pointer to the UW_CardViewer for displaying cards on the left side.
	 * @param RightCardViewer Pointer to the UW_CardViewer for displaying cards on the right side.
	 * @param ScrollBoxHeightMlt A float multiplier to adjust the height of the card scroll box on mouse over.
	 * @param LayoutSwitcher Pointer to the UWidgetSwitcher used to switch between different layout profiles.
	 * @param NomadicLayoutWidget Pointer to the UW_NomadicLayout widget managing nomadic layouts.
	 * @param NomadicBuildingLayoutClass The TSubclassOf<UW_NomadicLayoutBuilding> representing the blueprint class for nomadic building layouts.
	 * @param NewSoundMap A TMap mapping ECardSound enum values to USoundCue pointers for sound playback.
	 * @param NewPopupWidget Pointer to the UW_RTSPopup widget for displaying popups.
	 * @note This function must be called to properly initialize the card menu before use.
	 */
	UFUNCTION(BlueprintCallable, Category = "CardMenu")
	void InitCardMenu(
		UW_CardHolder* NewTechCardHolder,
		UW_CardHolder* NewUnitCardHolder,
		UW_CardScrollBox* NewCardScrollBox,
		TSubclassOf<UW_RTSCard> InCardClass,
		UW_CardViewer* LeftCardViewer,
		UW_CardViewer* RightCardViewer,
		const float ScrollBoxHeightMlt,
		UWidgetSwitcher* LayoutSwitcher,
		UW_NomadicLayout* NomadicLayoutWidget,
		TSubclassOf<UW_NomadicLayoutBuilding> NomadicBuildingLayoutClass, TMap<ECardSound, USoundCue*>
		NewSoundMap, UW_RTSPopup* NewPopupWidget);

	// Specific text of button that displays the Extra units and technologies in the widget switcher.
	UPROPERTY(meta = (MultiLine = true, BindWidget))
	URichTextBlock* BonusUnitText;

	/**
	 * @brief Handles the event when the "Start Game" button is clicked.
	 *
	 * Validates selected cards and checks for empty slots. If any required slots are not filled, it displays a popup. Otherwise, proceeds to start the game.
	 *
	 * @note This function interacts with the player profile and may trigger UI popups for incomplete selections.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickStartGame();

private:
	/**
	 * @brief Loads the player profile and initializes the card menu with the player's saved data.
	 *
	 * This function retrieves the player's saved cards, layouts, and settings from the player profile and initializes the card picker, unit card holder, tech resource card holder, and nomadic layout accordingly.
	 *
	 * @note If no profile is found, default test data is used to initialize the card menu.
	 */
	void LoadPlayerProfile();

	/**
	 * @brief Initializes the card picker (scroll box) with available cards.
	 *
	 * Sets up the card scroll box with the provided list of available cards and assigns the card class for creating card widgets.
	 *
	 * @param AvailableCards An array of ERTSCard enums representing the cards available to the player.
	 * @param InCardClass The TSubclassOf<UW_RTSCard> representing the blueprint class for card widgets.
	 * @note The card scroll box must be valid before calling this function.
	 */
	void InitializeCardPicker(const TArray<ERTSCard>& AvailableCards, TSubclassOf<UW_RTSCard> InCardClass);

	/**
	 * @brief Initializes the unit card holder with selected unit cards and settings.
	 *
	 * Sets up the unit card holder with the provided list of unit cards, the maximum number of cards it can hold, and the card class for creating card widgets.
	 *
	 * @param AvailableCards An array of ERTSCard enums representing the unit cards selected by the player.
	 * @param InCardClass The TSubclassOf<UW_RTSCard> representing the blueprint class for card widgets.
	 * @param MaxCards The maximum number of cards the unit card holder can hold.
	 * @note The unit card holder must be valid before calling this function.
	 */
	void InitializeUnitCardHolder(const TArray<ERTSCard>& AvailableCards, TSubclassOf<UW_RTSCard> InCardClass,
	                              const int32 MaxCards);

	/**
	 * @brief Initializes the tech/resource card holder with selected cards and settings.
	 *
	 * Sets up the tech/resource card holder with the provided list of tech and resource cards, the maximum number of cards it can hold, and the card class for creating card widgets.
	 *
	 * @param AvailableCards An array of ERTSCard enums representing the tech/resource cards selected by the player.
	 * @param InCardClass The TSubclassOf<UW_RTSCard> representing the blueprint class for card widgets.
	 * @param MaxCards The maximum number of cards the tech/resource card holder can hold.
	 * @note The tech/resource card holder must be valid before calling this function.
	 */
	void InitializeTechCardHolder(const TArray<ERTSCard>& AvailableCards, TSubclassOf<UW_RTSCard> InCardClass,
	                              const int32 MaxCards);

	/**
	 * @brief Initializes the nomadic layout with building data and settings.
	 *
	 * Sets up the nomadic layout with the provided building layout data, building layout class, and card class for creating card widgets.
	 *
	 * @param BuildingsData An array of FNomadicBuildingLayoutData structs containing the data for each building in the layout.
	 * @param BuildingLayoutClass The TSubclassOf<UW_NomadicLayoutBuilding> representing the blueprint class for nomadic building layouts.
	 * @param InCardClass The TSubclassOf<UW_RTSCard> representing the blueprint class for card widgets.
	 * @note The nomadic layout must be valid before calling this function.
	 */
	void InitializeNomadicLayout(const TArray<FNomadicBuildingLayoutData>& BuildingsData,
	                             TSubclassOf<UW_NomadicLayoutBuilding> BuildingLayoutClass,
	                             TSubclassOf<UW_RTSCard> InCardClass);

	UPROPERTY(EditDefaultsOnly)
	const TSoftObjectPtr<UWorld> LevelToLoad;

	/** References to card holders */
	UPROPERTY()
	TObjectPtr<UW_CardHolder> M_UnitCardHolder;

	UPROPERTY()
	TObjectPtr<UW_CardHolder> M_TechResourceCardHolder;

	UPROPERTY()
	TObjectPtr<UW_CardScrollBox> M_CardScrollBox;

	/** The Blueprint class for the card widgets */
	UPROPERTY()
	TSubclassOf<UW_RTSCard> M_BlueprintCardClass;

	UPROPERTY()
	TSubclassOf<UW_NomadicLayoutBuilding> M_NomadicBuildingLayoutClass;

	UPROPERTY()
	TObjectPtr<UW_CardViewer> M_LeftCardViewer;

	UPROPERTY()
	TObjectPtr<UW_NomadicLayout> M_NomadicLayout;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> M_LayoutSwitcher;

	UPROPERTY()
	TArray<TObjectPtr<UW_RTSCard>> M_SelectedCards;

	// Initialized as if viewing the extra units, tech, and buildings.
	UPROPERTY()
	ELayoutProfileWidgets M_CurrentLayoutProfile;

	UPROPERTY()
	TObjectPtr<UW_CardViewer> M_RightCardViewer;

	UPROPERTY()
	TMap<ECardSound, USoundCue*> M_MapSoundToCue;

	UPROPERTY()
	UW_RTSPopup* M_PopupWidget;

	FTimerHandle HoverSoundCooldownTimer;
	bool bCanPlayHoverSound = true;

	/**
	 * @brief Plays a 2D sound cue associated with a specific card sound type.
	 *
	 * @param Sound The ECardSound enum value indicating which sound cue to play.
	 * @note This function is called internally by PlayRTSCardSound to handle actual sound playback.
	 */
	void Play2DSound(ECardSound Sound);

	/**
	 * @brief Resets the cooldown for playing hover sounds, allowing them to be played again.
	 *
	 * @note This function is called by a timer to prevent hover sounds from playing too frequently.
	 */
	void ResetHoverSoundCooldown();

	ECardViewer M_CurrentCardViewer = ECardViewer::None;

	/**
	 * @brief Checks if the provided card viewer is valid.
	 *
	 * @param CardViewer Pointer to the UW_CardViewer to check.
	 * @return True if the card viewer is valid; false otherwise.
	 * @note Reports an error if the card viewer is invalid.
	 */
	bool GetIsCardViewerValid(UW_CardViewer* CardViewer);

	/**
	 * @brief Checks if the provided card widget is valid.
	 *
	 * @param Card Pointer to the UW_RTSCard to check.
	 * @return True if the card widget is valid; false otherwise.
	 * @note Reports an error if the card is invalid.
	 */
	bool GetIsCardValid(UW_RTSCard* Card);

	/**
	 * @brief Checks if the card scroll box is valid.
	 *
	 * @return True if the card scroll box is valid; false otherwise.
	 * @note Reports an error if the scroll box is invalid.
	 */
	bool GetIsValidCardScrollBox() const;

	/**
	 * @brief Checks if the unit card holder is valid.
	 *
	 * @return True if the unit card holder is valid; false otherwise.
	 * @note Reports an error if the unit card holder is invalid.
	 */
	bool GetIsValidUnitCardHolder() const;

	/**
	 * @brief Checks if the tech/resource card holder is valid.
	 *
	 * @return True if the tech/resource card holder is valid; false otherwise.
	 * @note Reports an error if the tech/resource card holder is invalid.
	 */
	bool GetIsValidTechCardHolder() const;

	/**
	 * @brief Checks if the nomadic layout widget is valid.
	 *
	 * @return True if the nomadic layout widget is valid; false otherwise.
	 * @note Reports an error if the nomadic layout widget is invalid.
	 */
	bool GetIsValidNomadicLayout() const;

	/**
	 * @brief Checks if the layout switcher widget is valid.
	 *
	 * @return True if the layout switcher widget is valid; false otherwise.
	 * @note Reports an error if the layout switcher widget is invalid.
	 */
	bool GetIsValidLayoutSwitcher() const;

	/**
	 * @brief Checks if the popup widget is valid.
	 *
	 * @return True if the popup widget is valid; false otherwise.
	 * @note Reports an error if the popup widget is invalid.
	 */
	bool GetIsValidPopupWidget() const;

	/**
	 * @brief Registers the selected cards and checks for any empty slots in the card holders and layouts.
	 *
	 * Collects the selected cards from the unit card holder, tech/resource card holder, and nomadic layouts. Checks if there are empty slots that need to be filled and whether there are available cards to fill them. If empty slots exist and cards are available, a popup is created to notify the user.
	 *
	 * @return True if a popup was created due to empty slots; false otherwise.
	 * @note This function is called when starting the game to ensure all required slots are filled.
	 */
	bool RegisterSelectedCardsAndCheckSlots();

	/**
	 * @brief Checks if all unit card slots are filled and collects selected unit cards.
	 *
	 * @param OutEmptySlots Output parameter that will contain the number of empty unit slots.
	 * @param OutAvailableCard Output parameter that will contain an available unit card from the scroll box, if any.
	 * @param OutSelectedCards Output array that will contain the selected unit cards.
	 * @return True if all unit slots are filled; false otherwise.
	 * @note Requires valid unit card holder and card scroll box.
	 */
	bool CheckUnitCardsFilled(uint32& OutEmptySlots, ERTSCard& OutAvailableCard,
	                          TArray<UW_RTSCard*>& OutSelectedCards) const;

	/**
	 * @brief Checks if all tech/resource card slots are filled and collects selected cards.
	 *
	 * @param OutEmptySlots Output parameter that will contain the number of empty tech/resource slots.
	 * @param OutAvailableCardResource Output parameter that will contain an available resource card from the scroll box, if any.
	 * @param OutAvailableCardTech Output parameter that will contain an available tech card from the scroll box, if any.
	 * @param OutSelectedCards Output array that will contain the selected tech/resource cards.
	 * @return True if all tech/resource slots are filled; false otherwise.
	 * @note Requires valid tech/resource card holder and card scroll box.
	 */
	bool CheckTechResourceCardsFilled(uint32& OutEmptySlots, ERTSCard& OutAvailableCardResource,
	                                  ERTSCard& OutAvailableCardTech, TArray<UW_RTSCard*>&
	                                  OutSelectedCards) const;

	/**
	 * @brief Checks if all nomadic layout slots are filled and collects selected cards.
	 *
	 * @param OutEmptySlots Output parameter that will contain the number of empty layout slots.
	 * @param UnfilledLayouts Output array containing the types of layouts that are unfilled.
	 * @param OutSelectedCards Output array that will contain the selected cards for the layouts.
	 * @return True if all nomadic layout slots are filled; false otherwise.
	 * @note Requires valid nomadic layout and card scroll box.
	 */
	bool CheckNomadicLayoutsFilled(uint32& OutEmptySlots, TArray<ECardType>& UnfilledLayouts,
	                               TArray<UW_RTSCard*>& OutSelectedCards) const;

	/**
	 * @brief Displays the specified card in the appropriate card viewer.
	 *
	 * Sets the visibility of the left and right card viewers and displays the specified card in the selected viewer.
	 *
	 * @param Card The ERTSCard enum value representing the card to display.
	 * @param Viewer The ECardViewer enum indicating which card viewer to use (Left, Right, or None).
	 * @note Hides both card viewers if Viewer is None or the card is invalid.
	 */
	void ViewCard(const ERTSCard Card, const ECardViewer Viewer);

	/**
	 * @brief Converts card pairs from a card holder to an array of card enums.
	 *
	 * Extracts the ERTSCard enum values from the card holder's card pairs.
	 *
	 * @param Cardholder Pointer to the ICardHolder interface implementation.
	 * @return An array of ERTSCard enums representing the cards held.
	 * @note Reports an error if the card holder is invalid.
	 */
	TArray<ERTSCard> ConvertCardPairsToCardOnly(ICardHolder* Cardholder) const;

	void SaveProfileLoadMap();


	float M_OriginalCardScrollBoxHeight = 0.0f;
	float M_ScrollBoxHeightMlt = 3.0f;

	/**
	 * @brief Creates a popup message indicating that not all card slots are filled.
	 *
	 * Constructs a detailed message based on the number of empty slots and available cards, and displays a popup to notify the user.
	 *
	 * @param EmptyUnitSlots The number of empty unit card slots.
	 * @param EmptyTechResourceSlots The number of empty tech/resource card slots.
	 * @param EmptyLayoutslots The number of empty nomadic layout slots.
	 * @param UnfilledLayouts An array of ECardType enums representing the unfilled layouts.
	 * @param AvailableUnitCard An available unit card from the scroll box, if any.
	 * @param AvailableTechResourceCard An available tech/resource card from the scroll box, if any.
	 * @note This function is called internally when empty slots are detected.
	 */
	void CreateCardsNotFilledPopup(const uint32& EmptyUnitSlots, const uint32 EmptyTechResourceSlots,
	                               const uint32 EmptyLayoutslots,
	                               const TArray<ECardType>& UnfilledLayouts, const ERTSCard& AvailableUnitCard,
	                               const ERTSCard& AvailableTechResourceCard);

	/**
	 * @brief Enables and configures the popup widget with the specified type, title, and message.
	 *
	 * @param Popup The ERTSPopup enum value indicating the type of popup to display.
	 * @param Title The FText title of the popup.
	 * @param Message The FText message to display in the popup.
	 * @note The popup widget must be valid before calling this function.
	 */
	void EnablePopupWidget(const ERTSPopup Popup, const FText& Title, const FText& Message);


	void DebugSelectedCards();

	static TMap<ELayoutProfileWidgets, int32> M_TMapLayoutToIndex;

	// Test data for initialization if no player profile is found.
	TArray<ERTSCard> M_TestCards = {
		// [List of test cards...]
	};

	TArray<ERTSCard> M_TestCards_UnitHolder = {ERTSCard::Card_Ger_Pz38T};
	TArray<ERTSCard> M_TestCards_TechHolder = {ERTSCard::Card_Resource_Metal, ERTSCard::Card_Ger_Tech_PzJager};
	int32 TestCard_MaxUnitCards = 2;
	int32 TestCard_MaxTechCards = 8;
};
