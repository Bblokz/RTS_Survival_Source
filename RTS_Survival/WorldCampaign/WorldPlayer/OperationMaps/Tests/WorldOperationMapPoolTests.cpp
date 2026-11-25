// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/OperationMaps/WorldOperationMapPool.h"

namespace WorldOperationMapPoolTestData
{
	// Paths only; the tests never load a level.
	TSoftObjectPtr<UWorld> MakeMap(const TCHAR* MapName)
	{
		return TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("/Game/PoolTest/%s.%s"), MapName,
		                                                              MapName)));
	}

	TArray<TSoftObjectPtr<UWorld>> MakeThreeMaps()
	{
		return {MakeMap(TEXT("MapA")), MakeMap(TEXT("MapB")), MakeMap(TEXT("MapC"))};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldOperationMapPoolRetryTest, "RTS.WorldCampaign.OperationMaps.RetryKeepsMap",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldOperationMapPoolRetryTest::RunTest(const FString& Parameters)
{
	using namespace WorldOperationMapPoolTestData;
	const TArray<TSoftObjectPtr<UWorld>> Maps = MakeThreeMaps();
	FWorldOperationMapPoolState PoolState;
	const FGuid Airfield = FGuid::NewGuid();
	const FGuid Factory = FGuid::NewGuid();

	TSoftObjectPtr<UWorld> FirstAttack;
	TestTrue(TEXT("First attack picks a map"),
	         WorldOperationMapPool::TryPickMapForTarget(PoolState, Airfield, Maps, FirstAttack));
	TestEqual(TEXT("First attack takes the first shuffled map"), FirstAttack, Maps[0]);
	TestEqual(TEXT("Cursor moved on"), PoolState.NextMapIndex, 1);

	TSoftObjectPtr<UWorld> Retry;
	WorldOperationMapPool::TryPickMapForTarget(PoolState, Airfield, Maps, Retry);
	TestEqual(TEXT("A retry of the same item gets the same map"), Retry, FirstAttack);
	TestEqual(TEXT("A retry does not move the cursor"), PoolState.NextMapIndex, 1);

	TSoftObjectPtr<UWorld> OtherItem;
	WorldOperationMapPool::TryPickMapForTarget(PoolState, Factory, Maps, OtherItem);
	TestEqual(TEXT("A new item takes the next map"), OtherItem, Maps[1]);

	TSoftObjectPtr<UWorld> RetryAfterOther;
	WorldOperationMapPool::TryPickMapForTarget(PoolState, Airfield, Maps, RetryAfterOther);
	TestEqual(TEXT("The first item still keeps its map after another launch"), RetryAfterOther, Maps[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldOperationMapPoolCursorTest, "RTS.WorldCampaign.OperationMaps.CursorWraps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldOperationMapPoolCursorTest::RunTest(const FString& Parameters)
{
	using namespace WorldOperationMapPoolTestData;
	const TArray<TSoftObjectPtr<UWorld>> Maps = MakeThreeMaps();
	FWorldOperationMapPoolState PoolState;
	TArray<TSoftObjectPtr<UWorld>> Picked;
	for (int32 LaunchIndex = 0; LaunchIndex < 4; ++LaunchIndex)
	{
		TSoftObjectPtr<UWorld> PickedMap;
		WorldOperationMapPool::TryPickMapForTarget(PoolState, FGuid::NewGuid(), Maps, PickedMap);
		Picked.Add(PickedMap);
	}

	TestEqual(TEXT("Second new item"), Picked[1], Maps[1]);
	TestEqual(TEXT("Third new item"), Picked[2], Maps[2]);
	TestEqual(TEXT("Fourth new item wraps to the start"), Picked[3], Maps[0]);

	TSoftObjectPtr<UWorld> NoKeyMap;
	WorldOperationMapPool::TryPickMapForTarget(PoolState, FGuid(), Maps, NoKeyMap);
	TestEqual(TEXT("No key picks from the cursor"), NoKeyMap, Maps[1]);
	TestEqual(TEXT("No key remembers nothing"), PoolState.MapByTargetKey.Num(), 4);

	PoolState.NextMapIndex = 17;
	TSoftObjectPtr<UWorld> AfterBadCursor;
	WorldOperationMapPool::TryPickMapForTarget(PoolState, FGuid::NewGuid(), Maps, AfterBadCursor);
	TestEqual(TEXT("A cursor past the end starts over"), AfterBadCursor, Maps[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldOperationMapPoolEdgeTest, "RTS.WorldCampaign.OperationMaps.UnsetAndRemovedMaps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldOperationMapPoolEdgeTest::RunTest(const FString& Parameters)
{
	using namespace WorldOperationMapPoolTestData;
	FWorldOperationMapPoolState PoolState;
	TSoftObjectPtr<UWorld> PickedMap;
	TestFalse(TEXT("An empty pool picks nothing"),
	          WorldOperationMapPool::TryPickMapForTarget(PoolState, FGuid::NewGuid(), {}, PickedMap));

	const TArray<TSoftObjectPtr<UWorld>> OnlyUnset = {TSoftObjectPtr<UWorld>(), TSoftObjectPtr<UWorld>()};
	TestFalse(TEXT("A pool of unset entries picks nothing"),
	          WorldOperationMapPool::TryPickMapForTarget(PoolState, FGuid::NewGuid(), OnlyUnset, PickedMap));
	TestEqual(TEXT("A failed pick leaves the cursor"), PoolState.NextMapIndex, 0);
	TestEqual(TEXT("A failed pick remembers nothing"), PoolState.MapByTargetKey.Num(), 0);

	const TArray<TSoftObjectPtr<UWorld>> WithGap = {TSoftObjectPtr<UWorld>(), MakeMap(TEXT("MapB"))};
	WorldOperationMapPool::TryPickMapForTarget(PoolState, FGuid::NewGuid(), WithGap, PickedMap);
	TestEqual(TEXT("Unset entries are skipped"), PickedMap, WithGap[1]);
	TestEqual(TEXT("Cursor wraps after the skipped pick"), PoolState.NextMapIndex, 0);

	const FGuid Barracks = FGuid::NewGuid();
	const TArray<TSoftObjectPtr<UWorld>> Maps = MakeThreeMaps();
	FWorldOperationMapPoolState FreshPool;
	WorldOperationMapPool::TryPickMapForTarget(FreshPool, Barracks, Maps, PickedMap);
	const TArray<TSoftObjectPtr<UWorld>> MapsWithoutA = {Maps[1], Maps[2]};
	TSoftObjectPtr<UWorld> RetryAfterRemoval;
	WorldOperationMapPool::TryPickMapForTarget(FreshPool, Barracks, MapsWithoutA, RetryAfterRemoval);
	TestEqual(TEXT("A target whose map left the pool gets a new one"), RetryAfterRemoval, Maps[2]);
	TestEqual(TEXT("The new map is remembered"),
	          FreshPool.MapByTargetKey.FindRef(Barracks), Maps[2].ToSoftObjectPath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldOperationMapCursorSnapshotTest, "RTS.WorldCampaign.OperationMaps.FailedLaunchRestores",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldOperationMapCursorSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace WorldOperationMapPoolTestData;
	UWorldStateAndSaveManager* SaveManager = NewObject<UWorldStateAndSaveManager>();
	const TArray<TSoftObjectPtr<UWorld>> Maps = MakeThreeMaps();
	const FGuid Depot = FGuid::NewGuid();

	const FWorldOperationMapCursorSnapshot BeforeLaunch = SaveManager->CaptureOperationMapCursors();
	TSoftObjectPtr<UWorld> PickedMap;
	WorldOperationMapPool::TryPickMapForTarget(SaveManager->GetHandmadeEnemyItemMapPool(), Depot, Maps, PickedMap);
	WorldOperationMapPool::TryPickMapForTarget(SaveManager->GetHandmadeFieldMapPool(), Depot, Maps, PickedMap);
	SaveManager->AdvanceEnemyObjectProceduralMapIndex(Maps.Num());
	SaveManager->RestoreOperationMapCursors(BeforeLaunch);

	const FWorldCampaignState& State = SaveManager->GetCachedWorldCampaignState();
	TestEqual(TEXT("Item cursor restored"), State.HandmadeEnemyItemMapPool.NextMapIndex, 0);
	TestEqual(TEXT("Item memory restored"), State.HandmadeEnemyItemMapPool.MapByTargetKey.Num(), 0);
	TestEqual(TEXT("Field cursor restored"), State.HandmadeFieldMapPool.NextMapIndex, 0);
	TestEqual(TEXT("Procedural cursor restored"), State.EnemyObjectProceduralMapIndex, 0);

	WorldOperationMapPool::TryPickMapForTarget(SaveManager->GetHandmadeEnemyItemMapPool(), Depot, Maps, PickedMap);
	SaveManager->ResetHandmadeOperationMapPools();
	TestEqual(TEXT("A new campaign forgets item maps"), State.HandmadeEnemyItemMapPool.MapByTargetKey.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldOperationMapStoryRouteTest, "RTS.WorldCampaign.OperationMaps.StoryItemRoute",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldOperationMapStoryRouteTest::RunTest(const FString& Parameters)
{
	using namespace WorldOperationMapPool;
	constexpr EWorldEnemyOperationMapSource Handmade = EWorldEnemyOperationMapSource::HandmadeMaps;
	constexpr EWorldEnemyOperationMapSource Procedural = EWorldEnemyOperationMapSource::ProceduralMaps;
	TestTrue(TEXT("HQ with its level goes there"),
	         GetEnemyItemMapRoute(EMapEnemyItem::EnemyHQ, true, Handmade) == EWorldEnemyItemMapRoute::StoryMap);
	TestTrue(TEXT("Wall without a level is refused on handmade maps"),
	         GetEnemyItemMapRoute(EMapEnemyItem::EnemyWall, false, Handmade) == EWorldEnemyItemMapRoute::Refuse);
	TestTrue(TEXT("HQ without a level keeps the procedural pool"),
	         GetEnemyItemMapRoute(EMapEnemyItem::EnemyHQ, false, Procedural) == EWorldEnemyItemMapRoute::MapPool);
	TestTrue(TEXT("Airfield uses the handmade pool"),
	         GetEnemyItemMapRoute(EMapEnemyItem::Airfield, false, Handmade) == EWorldEnemyItemMapRoute::MapPool);
	TestTrue(TEXT("A story level set for an ordinary item wins too"),
	         GetEnemyItemMapRoute(EMapEnemyItem::Factory, true, Handmade) == EWorldEnemyItemMapRoute::StoryMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldOperationMapPoolSaveRoundTripTest,
                                 "RTS.WorldCampaign.OperationMaps.SurvivesSaveAndLoad",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldOperationMapPoolSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace WorldOperationMapPoolTestData;
	// A retry always crosses a campaign save and load; this uses the same serializer as SaveGameToSlot, in memory.
	const TArray<TSoftObjectPtr<UWorld>> Maps = MakeThreeMaps();
	const FGuid Airfield = FGuid::NewGuid();
	const FGuid Convoy = FGuid::NewGuid();
	USaveWorldCampaign* SavePayload = NewObject<USaveWorldCampaign>();
	TSoftObjectPtr<UWorld> PickedMap;
	WorldOperationMapPool::TryPickMapForTarget(SavePayload->WorldCampaignState.HandmadeEnemyItemMapPool, FGuid::NewGuid(),
	                                           Maps, PickedMap);
	WorldOperationMapPool::TryPickMapForTarget(SavePayload->WorldCampaignState.HandmadeEnemyItemMapPool, Airfield,
	                                           Maps, PickedMap);
	WorldOperationMapPool::TryPickMapForTarget(SavePayload->WorldCampaignState.HandmadeFieldMapPool, Convoy, Maps,
	                                           PickedMap);

	TArray<uint8> SaveBytes;
	if (not TestTrue(TEXT("Saved to memory"), UGameplayStatics::SaveGameToMemory(SavePayload, SaveBytes)))
	{
		return false;
	}

	const USaveWorldCampaign* LoadedPayload = Cast<USaveWorldCampaign>(UGameplayStatics::LoadGameFromMemory(SaveBytes));
	if (not TestNotNull(TEXT("Loaded from memory"), LoadedPayload))
	{
		return false;
	}

	FWorldOperationMapPoolState LoadedItemPool = LoadedPayload->WorldCampaignState.HandmadeEnemyItemMapPool;
	TestEqual(TEXT("Item cursor survives"), LoadedItemPool.NextMapIndex, 2);
	TestEqual(TEXT("Item memory survives"), LoadedItemPool.MapByTargetKey.FindRef(Airfield), Maps[1].ToSoftObjectPath());
	TestEqual(TEXT("Field memory survives"),
	          LoadedPayload->WorldCampaignState.HandmadeFieldMapPool.MapByTargetKey.FindRef(Convoy),
	          Maps[0].ToSoftObjectPath());

	TSoftObjectPtr<UWorld> RetryAfterLoad;
	WorldOperationMapPool::TryPickMapForTarget(LoadedItemPool, Airfield, Maps, RetryAfterLoad);
	TestEqual(TEXT("A retry after a load gets the same map"), RetryAfterLoad, Maps[1]);
	return true;
}

#endif
