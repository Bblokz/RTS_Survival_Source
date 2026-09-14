#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconDataAsset.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconSettings.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWidgetPoolManager.h"

/** @brief Exercises feedback without changing Blueprint class defaults or exposing runtime test APIs. */
struct FBehaviourAnimatedFeedbackTestAccess
{
	static UBehaviour* Add(UBehaviourComp& Component, const FRepeatedBehaviourIconSettings& IconSettings,
		const bool bEnableText)
	{
		UBehaviour* Behaviour = NewObject<UBehaviour>(&Component);
		Behaviour->AnimatedIconSettings = IconSettings;
		Behaviour->AnimatedTextSettings.TextSettings.bUseText = bEnableText;
		Component.AddInitialisedBehaviour(Behaviour);
		return Behaviour;
	}

	static void UsePool(UBehaviourComp& Component, UAnimatedIconWidgetPoolManager* Manager)
	{
		Component.M_AnimatedIconWidgetPoolManager = Manager;
	}

	static bool HasTextManager(const UBehaviourComp& Component)
	{
		return Component.M_AnimatedTextWidgetPoolManager.IsValid();
	}

	static bool HasIconManager(const UBehaviourComp& Component)
	{
		return Component.M_AnimatedIconWidgetPoolManager.IsValid();
	}

	static int32 RepeatCount(const UBehaviourComp& Component)
	{
		return Component.M_BehaviourAnimatedFeedbackStates.Num();
	}

	static void Advance(UBehaviourComp& Component, const double NowSeconds)
	{
		TGuardValue<bool> TickGuard(Component.bM_IsTickingBehaviours, true);
		for (int32 StateIndex = Component.M_BehaviourAnimatedFeedbackStates.Num() - 1; StateIndex >= 0; --StateIndex)
		{
			Component.AdvanceAnimatedFeedbackState(StateIndex, NowSeconds);
		}
		Component.UpdateComponentTickEnabled();
	}

	static void DisableIconSetting(UBehaviour& Behaviour)
	{
		Behaviour.AnimatedIconSettings.IconSettings.bUseIcons = false;
	}

	static void Remove(UBehaviourComp& Component, UBehaviour* Behaviour)
	{
		Component.RemoveBehaviourInstance(Behaviour);
		Component.UpdateComponentTickEnabled();
	}

	static void Clear(UBehaviourComp& Component)
	{
		Component.ClearAllBehaviours();
		Component.UpdateComponentTickEnabled();
	}
};

namespace BehaviourAnimatedFeedbackTests
{
	constexpr float RepeatInterval = 4.0f;
	constexpr int32 PoolCapacity = 16;

	/** @brief Owns an isolated world and restores catalog configuration when a test completes. */
	struct FScopedFeedbackWorld
	{
		TStrongObjectPtr<UAnimatedIconWidgetPoolManager> Manager{NewObject<UAnimatedIconWidgetPoolManager>()};
		TStrongObjectPtr<UAnimatedIconDataAsset> Catalog{NewObject<UAnimatedIconDataAsset>()};
		TSoftObjectPtr<UAnimatedIconDataAsset> OriginalCatalog = GetDefault<UAnimatedIconSettings>()->IconDataAsset;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UBehaviourComp> Component;

		~FScopedFeedbackWorld()
		{
			UBehaviourComp* BehaviourComponent = Component.Get();
			if (IsValid(BehaviourComponent))
			{
				FBehaviourAnimatedFeedbackTestAccess::Clear(*BehaviourComponent);
			}
			Manager->Shutdown();
			GetMutableDefault<UAnimatedIconSettings>()->IconDataAsset = OriginalCatalog;
			UWorld* TestWorld = World.Get();
			if (IsValid(TestWorld))
			{
				GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool Initialize()
		{
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (not World.IsValid())
			{
				return false;
			}
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			AActor* Owner = World->SpawnActor<AActor>();
			if (not IsValid(Owner))
			{
				return false;
			}
			USceneComponent* Root = NewObject<USceneComponent>(Owner);
			Owner->SetRootComponent(Root);
			Owner->AddInstanceComponent(Root);
			Root->RegisterComponent();
			Component = NewObject<UBehaviourComp>(Owner);
			Owner->AddInstanceComponent(Component.Get());
			Component->RegisterComponent();
			return true;
		}

		bool InitializeIconPool()
		{
			Catalog->PoolSize = PoolCapacity;
			constexpr int32 TextureExtent = 4;
			FRTSVerticalAnimatedIconDefinition Definition;
			Definition.Texture = UTexture2D::CreateTransient(TextureExtent, TextureExtent);
			Catalog->IconDefinitions.Add(ERTSVerticalAnimatedIcon::Healing, Definition);
			GetMutableDefault<UAnimatedIconSettings>()->IconDataAsset = Catalog.Get();
			if (not Manager->Init(World.Get()))
			{
				return false;
			}
			FBehaviourAnimatedFeedbackTestAccess::UsePool(*Component, Manager.Get());
			return true;
		}
	};

	FRepeatedBehaviourIconSettings MakeIconSettings()
	{
		FRepeatedBehaviourIconSettings Settings;
		Settings.IconSettings.bUseIcons = true;
		Settings.IconSettings.IconType = ERTSVerticalAnimatedIcon::Healing;
		Settings.RepeatInterval = RepeatInterval;
		return Settings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBehaviourFeedbackSelectionTest, "RTS.Behaviours.AnimatedFeedback.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBehaviourFeedbackSelectionTest::RunTest(const FString& Parameters)
{
	BehaviourAnimatedFeedbackTests::FScopedFeedbackWorld Fixture;
	if (not TestTrue(TEXT("World initializes"), Fixture.Initialize()))
	{
		return false;
	}
	FRepeatedBehaviourIconSettings Settings;
	TestFalse(TEXT("Icons default to disabled"), Settings.IconSettings.bUseIcons);
	FBehaviourAnimatedFeedbackTestAccess::Add(*Fixture.Component, Settings, false);
	TestEqual(TEXT("Neither creates no repeat state"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 0);
	TestFalse(TEXT("Neither does not enable ticking"), Fixture.Component->IsComponentTickEnabled());
	Settings.IconSettings.bUseIcons = true;
	Settings.RepeatStrategy = EBehaviourRepeatedVerticalIconStrategy::InfiniteRepeats;
	FBehaviourAnimatedFeedbackTestAccess::Add(*Fixture.Component, Settings, true);
	TestFalse(TEXT("Icon None never falls back to text"), FBehaviourAnimatedFeedbackTestAccess::HasTextManager(*Fixture.Component));
	TestFalse(TEXT("None requires no icon manager lookup"), FBehaviourAnimatedFeedbackTestAccess::HasIconManager(*Fixture.Component));
	TestEqual(TEXT("None never schedules repeats"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBehaviourFeedbackFiniteRepeatTest, "RTS.Behaviours.AnimatedFeedback.FiniteRepeats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBehaviourFeedbackFiniteRepeatTest::RunTest(const FString& Parameters)
{
	BehaviourAnimatedFeedbackTests::FScopedFeedbackWorld Fixture;
	if (not TestTrue(TEXT("Pool initializes"), Fixture.Initialize() && Fixture.InitializeIconPool()))
	{
		return false;
	}
	FRepeatedBehaviourIconSettings Settings = BehaviourAnimatedFeedbackTests::MakeIconSettings();
	Settings.AmountRepeats = 3;
	UBehaviour* Behaviour = FBehaviourAnimatedFeedbackTestAccess::Add(*Fixture.Component, Settings, true);
	TestEqual(TEXT("Initial icon appears immediately"), Fixture.Manager->GetActiveIconCount(), 1);
	TestEqual(TEXT("Exactly one repeat state"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 1);
	TestTrue(TEXT("Repeats enable component ticking"), Fixture.Component->IsComponentTickEnabled());
	TestFalse(TEXT("Gameplay OnTick remains disabled"), Behaviour->UsesTick());
	TestFalse(TEXT("Enabled text is never resolved for icon behaviour"), FBehaviourAnimatedFeedbackTestAccess::HasTextManager(*Fixture.Component));
	const double StartTime = Fixture.World->GetTimeSeconds();
	FBehaviourAnimatedFeedbackTestAccess::Advance(*Fixture.Component, StartTime);
	TestEqual(TEXT("Not due performs no display"), Fixture.Manager->GetActiveIconCount(), 1);
	FBehaviourAnimatedFeedbackTestAccess::DisableIconSetting(*Behaviour);
	FBehaviourAnimatedFeedbackTestAccess::Advance(*Fixture.Component, StartTime + Settings.RepeatInterval);
	TestEqual(TEXT("Cached icon selection is retained"), Fixture.Manager->GetActiveIconCount(), 2);
	constexpr double LongFrameDelay = 100.0;
	FBehaviourAnimatedFeedbackTestAccess::Advance(*Fixture.Component, StartTime + LongFrameDelay);
	TestEqual(TEXT("Count includes initial display and no catch-up burst"), Fixture.Manager->GetActiveIconCount(), 3);
	TestEqual(TEXT("Final repeat removes scheduling state"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 0);
	TestFalse(TEXT("Completed repeats no longer require ticking"), Fixture.Component->IsComponentTickEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBehaviourFeedbackCleanupTest, "RTS.Behaviours.AnimatedFeedback.Cleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBehaviourFeedbackCleanupTest::RunTest(const FString& Parameters)
{
	BehaviourAnimatedFeedbackTests::FScopedFeedbackWorld Fixture;
	if (not TestTrue(TEXT("Pool initializes"), Fixture.Initialize() && Fixture.InitializeIconPool()))
	{
		return false;
	}
	FRepeatedBehaviourIconSettings Settings = BehaviourAnimatedFeedbackTests::MakeIconSettings();
	Settings.RepeatStrategy = EBehaviourRepeatedVerticalIconStrategy::InfiniteRepeats;
	UBehaviour* Behaviour = FBehaviourAnimatedFeedbackTestAccess::Add(*Fixture.Component, Settings, false);
	const double StartTime = Fixture.World->GetTimeSeconds();
	FBehaviourAnimatedFeedbackTestAccess::Advance(*Fixture.Component, StartTime + Settings.RepeatInterval);
	TestEqual(TEXT("Infinite state remains scheduled"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 1);
	FBehaviourAnimatedFeedbackTestAccess::Remove(*Fixture.Component, Behaviour);
	TestEqual(TEXT("Removal stops infinite repeats"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 0);
	TestFalse(TEXT("Removal permits sleeping"), Fixture.Component->IsComponentTickEnabled());
	FBehaviourAnimatedFeedbackTestAccess::Add(*Fixture.Component, Settings, false);
	FBehaviourAnimatedFeedbackTestAccess::Clear(*Fixture.Component);
	TestEqual(TEXT("Clear stops all repeats"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 0);
	Settings.RepeatInterval = 0.0f;
	const int32 PreviousDisplays = Fixture.Manager->GetActiveIconCount();
	FBehaviourAnimatedFeedbackTestAccess::Add(*Fixture.Component, Settings, false);
	TestEqual(TEXT("Zero interval permits initial display"), Fixture.Manager->GetActiveIconCount(), PreviousDisplays + 1);
	TestEqual(TEXT("Zero interval cannot create a repeat loop"), FBehaviourAnimatedFeedbackTestAccess::RepeatCount(*Fixture.Component), 0);
	return true;
}

#endif
