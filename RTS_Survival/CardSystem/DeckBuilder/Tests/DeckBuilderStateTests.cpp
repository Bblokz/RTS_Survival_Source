// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderState.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

namespace DeckBuilderStateTestData
{
	constexpr int32 ForgeSlots = 2;
	constexpr int32 BarracksSlots = 3;
	constexpr int32 TechnologySlots = 2;
	constexpr int32 StartUnitSlots = 4;

	const FTrainingOption Pz38t(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Pz38t));
	const FTrainingOption Hetzer(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Hetzer));
	const FTrainingOption PzJager(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzJager));
	const FTrainingOption Scavengers(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_Scavengers));

	FDeckCardDefinition MakeTrainCard(const ERTSCard Card, const ECardType CardType, const FTrainingOption& Unit)
	{
		FDeckCardDefinition Definition;
		Definition.Card = Card;
		Definition.CardType = CardType;
		Definition.TrainingOption = Unit;
		Definition.VariantUnit = Unit;
		return Definition;
	}

	FDeckCardDefinition MakeCardOfType(const ERTSCard Card, const ECardType CardType)
	{
		FDeckCardDefinition Definition;
		Definition.Card = Card;
		Definition.CardType = CardType;
		return Definition;
	}

	TMap<ERTSCard, FDeckCardDefinition> MakeDefinitions()
	{
		TMap<ERTSCard, FDeckCardDefinition> Definitions;
		Definitions.Add(ERTSCard::Card_Train_Pz38T, MakeTrainCard(ERTSCard::Card_Train_Pz38T, ECardType::ForgeTrain, Pz38t));
		Definitions.Add(ERTSCard::Card_Train_Hetzer,
		                MakeTrainCard(ERTSCard::Card_Train_Hetzer, ECardType::ForgeTrain, Hetzer));
		Definitions.Add(ERTSCard::Card_Train_PzJager,
		                MakeTrainCard(ERTSCard::Card_Train_PzJager, ECardType::ForgeTrain, PzJager));
		Definitions.Add(ERTSCard::Card_Train_Scavengers,
		                MakeTrainCard(ERTSCard::Card_Train_Scavengers, ECardType::BarracksTrain, Scavengers));
		FDeckCardDefinition Technology = MakeCardOfType(ERTSCard::Card_Ger_Tech_PzJager, ECardType::Technology);
		Technology.Technology = ETechnology::Tech_PzJager;
		Definitions.Add(ERTSCard::Card_Ger_Tech_PzJager, Technology);
		Definitions.Add(ERTSCard::Card_Resource_Metal, MakeCardOfType(ERTSCard::Card_Resource_Metal, ECardType::Resource));
		Definitions.Add(ERTSCard::Card_Ger_Pz38T, MakeCardOfType(ERTSCard::Card_Ger_Pz38T, ECardType::StartingUnit));
		return Definitions;
	}

	FNomadicBuildingLayoutData MakeLayout(const ENomadicLayoutBuildingType BuildingType, const int32 Slots,
	                                      const TArray<ERTSCard>& Cards)
	{
		FNomadicBuildingLayoutData Layout;
		Layout.BuildingType = BuildingType;
		Layout.Slots = Slots;
		Layout.Cards = Cards;
		return Layout;
	}

	/** A profile as the old card menu left it: start units, a resource card in a tech slot, two building layouts. */
	FPlayerCardSaveData MakeSaveData()
	{
		FPlayerCardSaveData SaveData;
		SaveData.PlayerAvailableCards = {
			ERTSCard::Card_Train_Hetzer, ERTSCard::Card_Train_Hetzer, ERTSCard::Card_Train_PzJager,
			ERTSCard::Card_Train_Scavengers, ERTSCard::Card_Resource_Metal, ERTSCard::Card_Resource_Metal,
			ERTSCard::Card_Ger_Pz38T, ERTSCard::Card_EasterEgg
		};
		SaveData.CardsInUnits = {ERTSCard::Card_Ger_Pz38T};
		SaveData.MaxCardsInUnits = StartUnitSlots;
		SaveData.CardsInTechResources = {ERTSCard::Card_Resource_Metal, ERTSCard::Card_Ger_Tech_PzJager};
		SaveData.MaxCardsInTechResources = TechnologySlots;
		SaveData.NomadicBuildingLayouts = {
			MakeLayout(ENomadicLayoutBuildingType::Building_Barracks, BarracksSlots, {}),
			MakeLayout(ENomadicLayoutBuildingType::Building_Forge, ForgeSlots, {ERTSCard::Card_Train_Pz38T})
		};
		return SaveData;
	}

	FUnitVariantSelection MakeVariant(const FTrainingOption& Unit, const EUnitVariant Variant)
	{
		FUnitVariantSelection Selection;
		Selection.Unit = Unit;
		Selection.Variant = Variant;
		return Selection;
	}

	int32 CountCard(const TArray<ERTSCard>& Cards, const ERTSCard Card)
	{
		int32 Count = 0;
		for (const ERTSCard Each : Cards)
		{
			Count += Each == Card ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckBuilderInitSplitsCardsTest, "RTS.DeckBuilder.State.InitSplitsCards",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeckBuilderInitSplitsCardsTest::RunTest(const FString& Parameters)
{
	using namespace DeckBuilderStateTestData;
	FDeckBuilderState State;
	State.Init(MakeSaveData(), MakeDefinitions(), {});

	TestEqual(TEXT("Collection holds the three training cards once each"), State.GetCollection().Num(), 3);
	TestFalse(TEXT("Resource cards are not in the collection"),
	          State.GetCollection().Contains(ERTSCard::Card_Resource_Metal));
	TestFalse(TEXT("Start-unit cards are not in the collection"),
	          State.GetCollection().Contains(ERTSCard::Card_Ger_Pz38T));
	const FDeckSectionState& Technologies = State.GetSection(EDeckSection::Technologies);
	TestEqual(TEXT("The resource card left the technology slots"), Technologies.Cards.Num(), 1);
	TestEqual(TEXT("Technology slot count comes from the profile"), Technologies.SlotCount, TechnologySlots);
	TestEqual(TEXT("Forge keeps its slotted card"), State.GetSection(EDeckSection::Forge).Cards.Num(), 1);
	TestTrue(TEXT("Barracks is available"), State.GetSection(EDeckSection::Barracks).bIsAvailable);
	TestFalse(TEXT("A building without a layout is not available"),
	          State.GetSection(EDeckSection::Airbase).bIsAvailable);
	TestEqual(TEXT("Forge filter lists the two Forge cards in the collection"),
	          State.GetCollectionForSection(EDeckSection::Forge).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckBuilderSlotAndReturnTest, "RTS.DeckBuilder.State.SlotAndReturn",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeckBuilderSlotAndReturnTest::RunTest(const FString& Parameters)
{
	using namespace DeckBuilderStateTestData;
	FDeckBuilderState State;
	State.Init(MakeSaveData(), MakeDefinitions(), {});

	TestFalse(TEXT("A Barracks card does not go in the Forge"),
	          State.SlotCard(ERTSCard::Card_Train_Scavengers, EDeckSection::Forge));
	TestFalse(TEXT("A card does not go in a building without a layout"),
	          State.SlotCard(ERTSCard::Card_Train_Hetzer, EDeckSection::Airbase));
	TestTrue(TEXT("A Forge card goes in the free Forge slot"),
	         State.SlotCard(ERTSCard::Card_Train_Hetzer, EDeckSection::Forge));
	TestEqual(TEXT("Forge is full"), State.GetSection(EDeckSection::Forge).Cards.Num(), ForgeSlots);
	TestFalse(TEXT("A full section takes no further card on an empty slot"),
	          State.SlotCard(ERTSCard::Card_Train_PzJager, EDeckSection::Forge));
	TestTrue(TEXT("Dropping on a filled slot swaps"), State.SlotCard(ERTSCard::Card_Train_PzJager, EDeckSection::Forge, 0));
	TestTrue(TEXT("The replaced card is back in the collection"),
	         State.GetCollection().Contains(ERTSCard::Card_Train_Pz38T));
	TestEqual(TEXT("The dropped card took the slot"), State.GetSection(EDeckSection::Forge).Cards[0],
	          ERTSCard::Card_Train_PzJager);
	TestTrue(TEXT("A slotted card returns to the collection"), State.ReturnCardToCollection(EDeckSection::Forge, 1));
	TestTrue(TEXT("The returned card is in the collection"), State.GetCollection().Contains(ERTSCard::Card_Train_Hetzer));
	TestFalse(TEXT("An empty slot returns nothing"), State.ReturnCardToCollection(EDeckSection::Forge, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckBuilderVariantsTest, "RTS.DeckBuilder.State.Variants",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeckBuilderVariantsTest::RunTest(const FString& Parameters)
{
	using namespace DeckBuilderStateTestData;
	const TArray<FUnitVariantSelection> Owned = {
		MakeVariant(Pz38t, EUnitVariant::VariantB), MakeVariant(Pz38t, EUnitVariant::VariantA),
		MakeVariant(Scavengers, EUnitVariant::VariantA)
	};
	FPlayerCardSaveData SaveData = MakeSaveData();
	// A saved choice for a variant the player does not own must not come back attached.
	SaveData.UnitVariantSelections = {MakeVariant(Hetzer, EUnitVariant::VariantA)};
	FDeckBuilderState State;
	State.Init(SaveData, MakeDefinitions(), Owned);

	TestEqual(TEXT("An unowned saved choice is dropped"), State.GetAttachedVariant(Hetzer), EUnitVariant::Base);
	TestEqual(TEXT("Every owned variant starts loose"), State.GetLooseVariants().Num(), 3);
	TestEqual(TEXT("The Forge lists the two variants of its unit"),
	          State.GetLooseVariantsForSection(EDeckSection::Forge).Num(), 2);
	TestEqual(TEXT("Owned variants come back in variant order"), State.GetOwnedVariants(Pz38t)[0], EUnitVariant::VariantA);
	TestFalse(TEXT("An unowned variant does not attach"), State.AttachVariant(MakeVariant(Hetzer, EUnitVariant::VariantA)));
	TestFalse(TEXT("Base is not a variant card"), State.AttachVariant(MakeVariant(Pz38t, EUnitVariant::Base)));
	TestTrue(TEXT("An owned variant attaches"), State.AttachVariant(MakeVariant(Pz38t, EUnitVariant::VariantB)));
	TestEqual(TEXT("The unit runs the attached variant"), State.GetAttachedVariant(Pz38t), EUnitVariant::VariantB);
	TestEqual(TEXT("The unit's other variant stays loose while one is clipped on"),
	          State.GetLooseVariantsForSection(EDeckSection::Forge).Num(), 1);
	TestTrue(TEXT("Attaching the other variant swaps"), State.AttachVariant(MakeVariant(Pz38t, EUnitVariant::VariantA)));
	TestEqual(TEXT("After the swap the first variant is loose again"),
	          State.GetLooseVariantsForSection(EDeckSection::Forge)[0].Variant, EUnitVariant::VariantB);
	TestEqual(TEXT("The Forge counts its unit with a variant"), State.GetAttachedVariantCount(EDeckSection::Forge), 1);
	TestTrue(TEXT("A variant detaches"), State.DetachVariant(Pz38t));
	TestFalse(TEXT("Detaching twice changes nothing"), State.DetachVariant(Pz38t));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeckBuilderSaveRoundTripTest, "RTS.DeckBuilder.State.SaveRoundTrip",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeckBuilderSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace DeckBuilderStateTestData;
	const TArray<FUnitVariantSelection> Owned = {MakeVariant(Pz38t, EUnitVariant::VariantA)};
	FDeckBuilderState State;
	State.Init(MakeSaveData(), MakeDefinitions(), Owned);
	State.SlotCard(ERTSCard::Card_Train_Hetzer, EDeckSection::Forge);
	State.SlotCard(ERTSCard::Card_Train_Scavengers, EDeckSection::Barracks);
	State.AttachVariant(Owned[0]);
	const FPlayerCardSaveData Saved = State.BuildSaveData();

	TestEqual(TEXT("No start-unit card stays slotted"), Saved.CardsInUnits.Num(), 0);
	TestEqual(TEXT("The start-unit slot count is kept"), Saved.MaxCardsInUnits, StartUnitSlots);
	TestEqual(TEXT("Both start-unit cards are still owned"),
	          CountCard(Saved.PlayerAvailableCards, ERTSCard::Card_Ger_Pz38T), 2);
	TestEqual(TEXT("All three resource cards are still owned"),
	          CountCard(Saved.PlayerAvailableCards, ERTSCard::Card_Resource_Metal), 3);
	TestTrue(TEXT("A card without a table row is still owned"),
	         Saved.PlayerAvailableCards.Contains(ERTSCard::Card_EasterEgg));
	TestEqual(TEXT("Only the technology card is in the technology slots"), Saved.CardsInTechResources.Num(), 1);
	// One technology, two Forge units, one Barracks unit; no start unit and no resource card.
	TestEqual(TEXT("Selected cards hold only slotted technology and training cards"), Saved.SelectedCards.Num(), 4);
	TestEqual(TEXT("Both building layouts are written back"), Saved.NomadicBuildingLayouts.Num(), 2);
	TestEqual(TEXT("The attached variant is saved"), Saved.UnitVariantSelections.Num(), 1);
	TestEqual(TEXT("A granted variant is not saved as owned"), Saved.OwnedUnitVariants.Num(), 0);

	FDeckBuilderState Reloaded;
	Reloaded.Init(Saved, MakeDefinitions(), Owned);
	TestEqual(TEXT("Forge reloads full"), Reloaded.GetSection(EDeckSection::Forge).Cards.Num(), ForgeSlots);
	TestEqual(TEXT("Barracks reloads its card"), Reloaded.GetSection(EDeckSection::Barracks).Cards.Num(), 1);
	TestEqual(TEXT("The variant reloads attached"), Reloaded.GetAttachedVariant(Pz38t), EUnitVariant::VariantA);
	TestEqual(TEXT("The collection reloads with the one unslotted card"), Reloaded.GetCollection().Num(), 1);
	return true;
}

#endif
