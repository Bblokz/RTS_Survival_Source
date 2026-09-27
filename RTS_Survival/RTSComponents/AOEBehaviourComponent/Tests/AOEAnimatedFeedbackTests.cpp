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
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/AOEBehaviourComponent.h"

/** @brief Exercises AOE feedback selection without exposing runtime test APIs. */
struct FAOEAnimatedFeedbackTestAccess
{
	static void ConfigureIconAndText(UAOEBehaviourComponent& Component)
	{
		Component.AOEBehaviourSettings.IconSettings.bUseIcons = true;
		Component.AOEBehaviourSettings.IconSettings.IconType = ERTSVerticalAnimatedIcon::RangeBoost;
		Component.AOEBehaviourSettings.TextSettings.bUseText = true;
	}

	static void UseIconPool(UAOEBehaviourComponent& Component, UAnimatedIconWidgetPoolManager* Manager)
	{
		Component.M_AnimatedIconWidgetPoolManager = Manager;
	}

	static void Apply(UAOEBehaviourComponent& Component, UBehaviourComp* Target)
	{
		Component.ApplyAnimatedFeedbackForTargets({Target});
	}

	static void SelectNoIcon(UAOEBehaviourComponent& Component)
	{
		Component.AOEBehaviourSettings.IconSettings.IconType = ERTSVerticalAnimatedIcon::None;
	}
};

namespace AOEAnimatedFeedbackTests
{
	constexpr int32 PoolCapacity = 4;

	/** @brief Owns an isolated world and restores animated-icon configuration after the test. */
	struct FScopedFeedbackWorld
	{
		TStrongObjectPtr<UAnimatedIconWidgetPoolManager> Manager{NewObject<UAnimatedIconWidgetPoolManager>()};
		TStrongObjectPtr<UAnimatedIconDataAsset> Catalog{NewObject<UAnimatedIconDataAsset>()};
		TSoftObjectPtr<UAnimatedIconDataAsset> OriginalCatalog = GetDefault<UAnimatedIconSettings>()->IconDataAsset;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UAOEBehaviourComponent> AuraComponent;
		TWeakObjectPtr<UBehaviourComp> TargetComponent;

		~FScopedFeedbackWorld()
		{
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
			AActor* AuraOwner = SpawnActorWithRoot();
			AActor* TargetOwner = SpawnActorWithRoot();
			if (not IsValid(AuraOwner) || not IsValid(TargetOwner))
			{
				return false;
			}

			AuraComponent = NewObject<UAOEBehaviourComponent>(AuraOwner);
			TargetComponent = NewObject<UBehaviourComp>(TargetOwner);
			if (not AuraComponent.IsValid() || not TargetComponent.IsValid())
			{
				return false;
			}

			Catalog->PoolSize = PoolCapacity;
			constexpr int32 TextureExtent = 4;
			FRTSVerticalAnimatedIconDefinition Definition;
			Definition.Texture = UTexture2D::CreateTransient(TextureExtent, TextureExtent);
			Catalog->IconDefinitions.Add(ERTSVerticalAnimatedIcon::RangeBoost, Definition);
			GetMutableDefault<UAnimatedIconSettings>()->IconDataAsset = Catalog.Get();
			return Manager->Init(World.Get());
		}

	private:
		AActor* SpawnActorWithRoot() const
		{
			AActor* Actor = World->SpawnActor<AActor>();
			if (not IsValid(Actor))
			{
				return nullptr;
			}

			USceneComponent* Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Actor->AddInstanceComponent(Root);
			Root->RegisterComponent();
			return Actor;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAOEAnimatedFeedbackSelectionTest, "RTS.AOE.AnimatedFeedback.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAOEAnimatedFeedbackSelectionTest::RunTest(const FString& Parameters)
{
	AOEAnimatedFeedbackTests::FScopedFeedbackWorld Fixture;
	if (not TestTrue(TEXT("Fixture initializes"), Fixture.Initialize()))
	{
		return false;
	}

	FAOEAnimatedFeedbackTestAccess::ConfigureIconAndText(*Fixture.AuraComponent);
	FAOEAnimatedFeedbackTestAccess::UseIconPool(*Fixture.AuraComponent, Fixture.Manager.Get());
	FAOEAnimatedFeedbackTestAccess::Apply(*Fixture.AuraComponent, Fixture.TargetComponent.Get());
	TestEqual(TEXT("Icon wins when icon and text are enabled"), Fixture.Manager->GetActiveIconCount(), 1);

	FAOEAnimatedFeedbackTestAccess::SelectNoIcon(*Fixture.AuraComponent);
	FAOEAnimatedFeedbackTestAccess::Apply(*Fixture.AuraComponent, Fixture.TargetComponent.Get());
	TestEqual(TEXT("Selected None does not fall back to text"), Fixture.Manager->GetActiveIconCount(), 1);
	return true;
}

#endif
