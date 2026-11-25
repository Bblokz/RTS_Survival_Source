// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Info.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemSignatureSlot.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemSiteDirector.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemSiteTypes.h"
#include "RTS_Survival/Enemy/ItemSites/EnemyItemStrengthSlot.h"
#include "UObject/UnrealType.h"
#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionContext.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EnemyItemSiteTestData
{
	FEnemyItemSlotContent MakeContent(UClass* ActorClass, const bool bIsSignatureTarget)
	{
		FEnemyItemSlotContent Content;
		Content.ActorClass = TSoftClassPtr<AActor>(ActorClass);
		Content.bIsSignatureTarget = bIsSignatureTarget;
		return Content;
	}

	FWorldStrengthContribution MakeFortification(const EWorldFortificationStrength Fortification,
	                                             const int32 Percentage)
	{
		FWorldStrengthContribution Contribution;
		Contribution.StrengthType = EWorldStrengthTypes::FortificationStrength;
		Contribution.FortificationModification = Fortification;
		Contribution.StrengthPercentage = Percentage;
		return Contribution;
	}

	// DefenceClass is protected on the dormant defence slot; the test sets it the way the editor does.
	bool SetDefenceClass(ADormantDefenceSlot& Slot, UClass* DefenceClass)
	{
		const FSoftClassProperty* DefenceClassProperty = FindFProperty<FSoftClassProperty>(
			ADormantDefenceSlot::StaticClass(), TEXT("DefenceClass"));
		if (DefenceClassProperty == nullptr)
		{
			return false;
		}

		TSoftClassPtr<AActor>* DefenceClassValue = DefenceClassProperty->ContainerPtrToValuePtr<TSoftClassPtr<AActor>>(
			&Slot);
		*DefenceClassValue = TSoftClassPtr<AActor>(DefenceClass);
		return true;
	}

	/** A bare game world for spawning slots and a director; destroyed with the scope. */
	struct FScopedTestWorld
	{
		TWeakObjectPtr<UWorld> World;

		bool Initialize()
		{
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (not World.IsValid())
			{
				return false;
			}

			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			return true;
		}

		~FScopedTestWorld()
		{
			if (World.IsValid())
			{
				GEngine->DestroyWorldContext(World.Get());
				World->DestroyWorld(false);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyItemSiteSlotContentTest, "RTS.EnemyItemSites.SlotContentOrder",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyItemSiteSlotContentTest::RunTest(const FString& Parameters)
{
	using namespace EnemyItemSiteTestData;
	UEnemyItemSlotContentSet* ContentSet = NewObject<UEnemyItemSlotContentSet>();
	ContentSet->ContentByItem.Add(EMapEnemyItem::Airfield, MakeContent(AActor::StaticClass(), true));
	ContentSet->ContentByItem.Add(EMapEnemyItem::Factory, MakeContent(APawn::StaticClass(), true));
	ContentSet->FillerContent = MakeContent(AInfo::StaticClass(), false);

	TMap<EMapEnemyItem, FEnemyItemSlotContent> SlotEntries;
	SlotEntries.Add(EMapEnemyItem::Factory, MakeContent(AController::StaticClass(), false));
	// A row with no class must not hide the content set's entry.
	SlotEntries.Add(EMapEnemyItem::Airfield, FEnemyItemSlotContent());
	const FEnemyItemSlotContent NoFiller;

	FEnemyItemSlotContent Resolved;
	TestTrue(TEXT("Slot entry wins"), EnemyItemSiteRules::TryResolveSlotContent(
		         EMapEnemyItem::Factory, SlotEntries, NoFiller, ContentSet, Resolved));
	TestTrue(TEXT("Slot entry class"), Resolved.ActorClass.Get() == AController::StaticClass());
	TestFalse(TEXT("Slot entry flag"), Resolved.bIsSignatureTarget);

	EnemyItemSiteRules::TryResolveSlotContent(EMapEnemyItem::Airfield, SlotEntries, NoFiller, ContentSet, Resolved);
	TestTrue(TEXT("Empty slot row falls back to the content set"), Resolved.ActorClass.Get() == AActor::StaticClass());

	EnemyItemSiteRules::TryResolveSlotContent(EMapEnemyItem::Barracks, SlotEntries, NoFiller, ContentSet, Resolved);
	TestTrue(TEXT("Unlisted item takes the content set filler"), Resolved.ActorClass.Get() == AInfo::StaticClass());

	const FEnemyItemSlotContent SlotFiller = MakeContent(ACameraActor::StaticClass(), false);
	EnemyItemSiteRules::TryResolveSlotContent(EMapEnemyItem::Barracks, SlotEntries, SlotFiller, ContentSet, Resolved);
	TestTrue(TEXT("Slot filler wins over the set filler"), Resolved.ActorClass.Get() == ACameraActor::StaticClass());

	TestFalse(TEXT("Item None spawns nothing"), EnemyItemSiteRules::TryResolveSlotContent(
		          EMapEnemyItem::None, SlotEntries, SlotFiller, ContentSet, Resolved));
	TestFalse(TEXT("Nothing anywhere spawns nothing"), EnemyItemSiteRules::TryResolveSlotContent(
		          EMapEnemyItem::Barracks, {}, NoFiller, nullptr, Resolved));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyItemSiteStrengthSlotTest, "RTS.EnemyItemSites.StrengthSlotCondition",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyItemSiteStrengthSlotTest::RunTest(const FString& Parameters)
{
	using namespace EnemyItemSiteTestData;
	FEnemyItemSiteContext Site;
	Site.ItemType = EMapEnemyItem::Airfield;
	Site.StrengthContributions.Add(MakeFortification(EWorldFortificationStrength::None, 100));
	Site.StrengthContributions.Add(MakeFortification(EWorldFortificationStrength::AABattery, 15));

	FEnemyItemStrengthSlotCondition Always;
	TestTrue(TEXT("No condition = always on"), EnemyItemSiteRules::GetShouldActivateStrengthSlot(Always, Site));

	FEnemyItemStrengthSlotCondition NeedsAA;
	NeedsAA.RequiredFortification = EWorldFortificationStrength::AABattery;
	TestTrue(TEXT("AA battery present"), EnemyItemSiteRules::GetShouldActivateStrengthSlot(NeedsAA, Site));

	FEnemyItemStrengthSlotCondition NeedsMines;
	NeedsMines.RequiredFortification = EWorldFortificationStrength::Minefields;
	TestFalse(TEXT("No minefields"), EnemyItemSiteRules::GetShouldActivateStrengthSlot(NeedsMines, Site));

	FEnemyItemStrengthSlotCondition NeedsStrength;
	NeedsStrength.MinimumTotalStrengthPercentage = 115;
	TestTrue(TEXT("115 reached"), EnemyItemSiteRules::GetShouldActivateStrengthSlot(NeedsStrength, Site));
	NeedsStrength.MinimumTotalStrengthPercentage = 116;
	TestFalse(TEXT("116 not reached"), EnemyItemSiteRules::GetShouldActivateStrengthSlot(NeedsStrength, Site));

	FEnemyItemStrengthSlotCondition FactoryOnly;
	FactoryOnly.OnlyForItems = {EMapEnemyItem::Factory};
	TestFalse(TEXT("Other item type"), EnemyItemSiteRules::GetShouldActivateStrengthSlot(FactoryOnly, Site));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyItemSiteContextTest, "RTS.EnemyItemSites.SiteContextSource",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyItemSiteContextTest::RunTest(const FString& Parameters)
{
	FEnemyItemSiteContext TestValues;
	TestValues.ItemType = EMapEnemyItem::Barracks;
	TestValues.OperationSeed = 7;

	const FEnemyItemSiteContext WithoutWorldMap = EnemyItemSiteRules::BuildSiteContext(FWorldMissionContext(),
	                                                                                    TestValues);
	TestEqual(TEXT("No context = test item"), WithoutWorldMap.ItemType, EMapEnemyItem::Barracks);
	TestFalse(TEXT("Marked as test values"), WithoutWorldMap.bFromMissionContext);

	FWorldMissionContext ItemOperation;
	ItemOperation.bIsInitialized = true;
	ItemOperation.OperationType = EMapItemType::EnemyItem;
	ItemOperation.EnemyObjectType = EMapEnemyItem::Airfield;
	ItemOperation.OperationSeed = 42;
	TestEqual(TEXT("PIE startup settings (not launched from the world map) do not override the test item"),
	          EnemyItemSiteRules::BuildSiteContext(ItemOperation, TestValues).ItemType, EMapEnemyItem::Barracks);

	ItemOperation.bLaunchedFromWorldMap = true;
	const FEnemyItemSiteContext FromItem = EnemyItemSiteRules::BuildSiteContext(ItemOperation, TestValues);
	TestEqual(TEXT("World map item used"), FromItem.ItemType, EMapEnemyItem::Airfield);
	TestEqual(TEXT("World map seed used"), FromItem.OperationSeed, 42);
	TestTrue(TEXT("Marked as world map values"), FromItem.bFromMissionContext);
	TestFalse(TEXT("Not a division"), FromItem.bIsDivisionOperation);

	FWorldMissionContext DivisionOperation = ItemOperation;
	DivisionOperation.EnemyObjectType = EMapEnemyItem::Factory;
	DivisionOperation.SourceDivisionKey = FGuid::NewGuid();
	DivisionOperation.EnemyArmy.DivisionType = EWorldFieldDivisions::SovietInfantryDivision;
	DivisionOperation.EnemyArmy.Squads = {ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Mosin};
	const FEnemyItemSiteContext FromDivision = EnemyItemSiteRules::BuildSiteContext(DivisionOperation, TestValues);
	TestTrue(TEXT("Division recognised by its key"), FromDivision.bIsDivisionOperation);
	TestEqual(TEXT("The stand-in item type is dropped"), FromDivision.ItemType, EMapEnemyItem::None);
	TestEqual(TEXT("The division's roster is carried"), FromDivision.Army.Squads.Num(), 2);
	TestEqual(TEXT("The division type is carried"), FromDivision.Army.DivisionType,
	          EWorldFieldDivisions::SovietInfantryDivision);

	FWorldMissionContext StoryMission;
	StoryMission.bIsInitialized = true;
	StoryMission.bLaunchedFromWorldMap = true;
	StoryMission.OperationType = EMapItemType::Mission;
	StoryMission.MissionType = EMapMission::Berlin;
	TestEqual(TEXT("A story mission context is ignored"),
	          EnemyItemSiteRules::BuildSiteContext(StoryMission, TestValues).ItemType, EMapEnemyItem::Barracks);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyItemSiteObjectiveStateTest, "RTS.EnemyItemSites.ObjectiveState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyItemSiteObjectiveStateTest::RunTest(const FString& Parameters)
{
	using namespace EnemyItemSiteRules;
	TestEqual(TEXT("Nothing before the site is built"), GetObjectiveState(false, 0, 0),
	          EEnemyItemSiteObjectiveState::WaitingForSite);
	TestEqual(TEXT("A built site without targets is never a win"), GetObjectiveState(true, 0, 0),
	          EEnemyItemSiteObjectiveState::NoTargets);
	TestEqual(TEXT("Targets left"), GetObjectiveState(true, 3, 1), EEnemyItemSiteObjectiveState::InProgress);
	TestEqual(TEXT("All targets dead"), GetObjectiveState(true, 3, 0),
	          EEnemyItemSiteObjectiveState::AllTargetsDestroyed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyItemSiteDirectorBuildTest, "RTS.EnemyItemSites.DirectorFillsSlots",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyItemSiteDirectorBuildTest::RunTest(const FString& Parameters)
{
	using namespace EnemyItemSiteTestData;
	FScopedTestWorld TestWorld;
	if (not TestTrue(TEXT("Test world created"), TestWorld.Initialize()))
	{
		return false;
	}

	UWorld* World = TestWorld.World.Get();
	AEnemyItemSignatureSlot* HangarSlot = World->SpawnActor<AEnemyItemSignatureSlot>();
	HangarSlot->ContentByItem.Add(EMapEnemyItem::Airfield, MakeContent(AActor::StaticClass(), true));
	AEnemyItemSignatureSlot* DressingSlot = World->SpawnActor<AEnemyItemSignatureSlot>();
	DressingSlot->FillerContent = MakeContent(AActor::StaticClass(), false);
	AEnemyItemSignatureSlot* FactoryOnlySlot = World->SpawnActor<AEnemyItemSignatureSlot>();
	FactoryOnlySlot->ContentByItem.Add(EMapEnemyItem::Factory, MakeContent(AActor::StaticClass(), true));
	AEnemyItemStrengthSlot* AASlot = World->SpawnActor<AEnemyItemStrengthSlot>();
	AASlot->Condition.RequiredFortification = EWorldFortificationStrength::AABattery;
	AEnemyItemStrengthSlot* MinesSlot = World->SpawnActor<AEnemyItemStrengthSlot>();
	MinesSlot->Condition.RequiredFortification = EWorldFortificationStrength::Minefields;
	if (not TestTrue(TEXT("Defence classes set"), SetDefenceClass(*AASlot, AActor::StaticClass())
		             && SetDefenceClass(*MinesSlot, AActor::StaticClass())))
	{
		return false;
	}

	AEnemyItemSiteDirector* Director = World->SpawnActor<AEnemyItemSiteDirector>();

	TestFalse(TEXT("Not built before BuildSite"), Director->GetIsSiteBuilt());
	FEnemyItemSiteContext Airfield;
	Airfield.ItemType = EMapEnemyItem::Airfield;
	Airfield.StrengthContributions.Add(MakeFortification(EWorldFortificationStrength::AABattery, 15));
	TestTrue(TEXT("Site builds"), Director->BuildSite(Airfield));
	TestTrue(TEXT("Built afterwards"), Director->GetIsSiteBuilt());
	TestEqual(TEXT("One victory target"), Director->GetSpawnedSignatureTargetCount(), 1);
	TestEqual(TEXT("Only the AA strength slot switched on"), Director->GetActivatedStrengthSlotCount(), 1);
	TestTrue(TEXT("AA defence spawned"), IsValid(AASlot->GetSpawnedDefence()));
	TestFalse(TEXT("Minefield slot stays dormant"), MinesSlot->GetIsActivated());
	TestEqual(TEXT("Living targets"), Director->GetSignatureTargets().Num(), 1);
	TestTrue(TEXT("Hangar spawned"), IsValid(HangarSlot->GetSpawnedContent()));
	TestTrue(TEXT("Filler spawned"), IsValid(DressingSlot->GetSpawnedContent()));
	TestFalse(TEXT("Factory slot left empty"), IsValid(FactoryOnlySlot->GetSpawnedContent()));
	TestTrue(TEXT("The hangar is the target"), Director->GetSignatureTargets()[0] == HangarSlot->GetSpawnedContent());
	TestFalse(TEXT("A second build does nothing"), Director->BuildSite(Airfield));

	HangarSlot->GetSpawnedContent()->Destroy();
	TestEqual(TEXT("A destroyed target drops out"), Director->GetSignatureTargets().Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyDivisionArmySpawnListTest, "RTS.EnemyItemSites.ArmySpawnList",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyDivisionArmySpawnListTest::RunTest(const FString& Parameters)
{
	FWorldMissionEnemyArmy Army;
	Army.Vehicles = {ETankSubtype::Tank_ZilFuel, ETankSubtype::Tank_None};
	Army.Squads = {ESquadSubtype::Squad_Rus_Mosin};
	Army.ConvoyTruckCount = 2;
	const FEnemyPlacementUnit Truck = FEnemyPlacementUnit::MakeVehicle(ETankSubtype::Tank_ZilAmmo);

	TArray<FEnemyPlacementUnit> Units;
	TestEqual(TEXT("The None vehicle is skipped"), EnemyItemSiteRules::BuildArmySpawnList(Army, Truck, Units), 1);
	if (not TestEqual(TEXT("Vehicle, squad and two trucks"), Units.Num(), 4))
	{
		return false;
	}

	TestEqual(TEXT("Vehicles first"), Units[0].VehicleSubtype, ETankSubtype::Tank_ZilFuel);
	TestEqual(TEXT("Then squads"), Units[1].SquadSubtype, ESquadSubtype::Squad_Rus_Mosin);
	TestEqual(TEXT("Then the trucks"), Units[3].VehicleSubtype, ETankSubtype::Tank_ZilAmmo);

	TArray<FEnemyPlacementUnit> WithoutTruck;
	TestEqual(TEXT("Trucks are skipped while no truck is set"),
	          EnemyItemSiteRules::BuildArmySpawnList(Army, FEnemyPlacementUnit(), WithoutTruck), 3);
	TestEqual(TEXT("Only the vehicle and the squad"), WithoutTruck.Num(), 2);
	TestEqual(TEXT("Unit count of the roster"), Army.GetUnitCount(), 5);

	FEnemyDivisionArmySettings Settings;
	TestEqual(TEXT("The default truck is set"), Settings.ConvoyTruck.VehicleSubtype, ETankSubtype::Tank_ZilAmmo);
	Settings.OrdersByDivision.Add(EWorldFieldDivisions::SovietInfantryDivision, EEnemyDivisionArmyOrders::HoldStart);
	TestEqual(TEXT("A division with an entry uses it"),
	          Settings.GetOrdersForDivision(EWorldFieldDivisions::SovietInfantryDivision),
	          EEnemyDivisionArmyOrders::HoldStart);
	TestEqual(TEXT("A convoy holds its start by default"),
	          Settings.GetOrdersForDivision(EWorldFieldDivisions::SovietSupplyConvoy),
	          EEnemyDivisionArmyOrders::HoldStart);
	TestEqual(TEXT("Others use the default orders"),
	          Settings.GetOrdersForDivision(EWorldFieldDivisions::SovietHeavyArmorDivision),
	          EEnemyDivisionArmyOrders::EnemyAIControl);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyDivisionArmySiteTest, "RTS.EnemyItemSites.DivisionArmySite",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyDivisionArmySiteTest::RunTest(const FString& Parameters)
{
	using namespace EnemyItemSiteTestData;
	FScopedTestWorld TestWorld;
	if (not TestTrue(TEXT("Test world created"), TestWorld.Initialize()))
	{
		return false;
	}

	UWorld* World = TestWorld.World.Get();
	AEnemyItemSignatureSlot* HangarSlot = World->SpawnActor<AEnemyItemSignatureSlot>();
	HangarSlot->ContentByItem.Add(EMapEnemyItem::Airfield, MakeContent(AActor::StaticClass(), true));

	// The empty roster is reported; the editor error dialog is turned into a log line for the test.
	RTSFunctionLibrary::PushModalDialogSuppression();
	AddExpectedError(TEXT("has no units in its roster"), EAutomationExpectedErrorFlags::Contains, 1);
	AEnemyItemSiteDirector* EmptyArmyDirector = World->SpawnActor<AEnemyItemSiteDirector>();
	FEnemyItemSiteContext EmptyDivision;
	EmptyDivision.bIsDivisionOperation = true;
	EmptyDivision.Army.DivisionType = EWorldFieldDivisions::SovietLightArmorDivision;
	EmptyArmyDirector->BuildSite(EmptyDivision);
	RTSFunctionLibrary::PopModalDialogSuppression();
	TestTrue(TEXT("An empty army is built at once"), EmptyArmyDirector->GetIsSiteBuilt());
	TestEqual(TEXT("It has no victory target"), EmptyArmyDirector->GetSpawnedVictoryTargetCount(), 0);
	TestEqual(TEXT("So it is never a win"),
	          EnemyItemSiteRules::GetObjectiveState(EmptyArmyDirector->GetIsSiteBuilt(),
	                                                EmptyArmyDirector->GetSpawnedVictoryTargetCount(),
	                                                EmptyArmyDirector->GetVictoryTargets().Num()),
	          EEnemyItemSiteObjectiveState::NoTargets);
	TestFalse(TEXT("A field map fills no signature slot"), IsValid(HangarSlot->GetSpawnedContent()));

	// No async spawner in a test world: the units stay pending, and the site must not count as built meanwhile.
	AEnemyItemSiteDirector* ArmyDirector = World->SpawnActor<AEnemyItemSiteDirector>();
	FEnemyItemSiteContext TankDivision;
	TankDivision.bIsDivisionOperation = true;
	TankDivision.Army.DivisionType = EWorldFieldDivisions::SovietMediumArmorDivision;
	TankDivision.Army.Vehicles = {ETankSubtype::Tank_ZilFuel, ETankSubtype::Tank_ZilFuel};
	TestTrue(TEXT("The army starts"), ArmyDirector->BuildSite(TankDivision));
	TestFalse(TEXT("Not built while units are pending"), ArmyDirector->GetIsSiteBuilt());
	TestEqual(TEXT("Waiting, not a win"),
	          EnemyItemSiteRules::GetObjectiveState(ArmyDirector->GetIsSiteBuilt(),
	                                                ArmyDirector->GetSpawnedVictoryTargetCount(),
	                                                ArmyDirector->GetVictoryTargets().Num()),
	          EEnemyItemSiteObjectiveState::WaitingForSite);
	TestFalse(TEXT("A second build does nothing"), ArmyDirector->BuildSite(TankDivision));
	return true;
}

#endif
