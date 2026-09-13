#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconDataAsset.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconSettings.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWidgetPoolManager.h"

namespace AnimatedIconPoolTests
{
	/** @brief Isolates a tiny pool from the project's gameplay subsystems and restores settings after the test. */
	struct FScopedPool
	{
		TStrongObjectPtr<UAnimatedIconWidgetPoolManager> Manager{NewObject<UAnimatedIconWidgetPoolManager>()};
		TStrongObjectPtr<UAnimatedIconDataAsset> Catalog{NewObject<UAnimatedIconDataAsset>()};
		TSoftObjectPtr<UAnimatedIconDataAsset> OriginalCatalog = GetDefault<UAnimatedIconSettings>()->IconDataAsset;
		TWeakObjectPtr<UWorld> World;

		~FScopedPool()
		{
			Manager->Shutdown();
			GetMutableDefault<UAnimatedIconSettings>()->IconDataAsset = OriginalCatalog;
			if (World.IsValid())
			{
				GEngine->DestroyWorldContext(World.Get());
				World->DestroyWorld(false);
			}
		}

		bool Initialize()
		{
			// Initialize as preview to avoid starting unrelated game subsystems, then enable game widget creation.
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (not World.IsValid())
			{
				return false;
			}
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			Catalog->PoolSize = 2;
			constexpr int32 TextureExtent = 4;
			UTexture2D* Texture = UTexture2D::CreateTransient(TextureExtent, TextureExtent);
			FRTSVerticalAnimatedIconDefinition Definition;
			Definition.Texture = Texture;
			Catalog->IconDefinitions.Add(ERTSVerticalAnimatedIcon::Healing, Definition);
			GetMutableDefault<UAnimatedIconSettings>()->IconDataAsset = Catalog.Get();
			return Manager->Init(World.Get());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimatedIconPoolReuseTest, "RTS.UI.AnimatedIcons.PoolReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimatedIconPoolReuseTest::RunTest(const FString& Parameters)
{
	AnimatedIconPoolTests::FScopedPool Pool;
	if (not TestTrue(TEXT("Native widgets prewarm without a Blueprint or local player"), Pool.Initialize()))
	{
		return false;
	}
	TestEqual(TEXT("Honors catalog capacity"), Pool.Manager->GetPoolSize(), 2);
	TestFalse(TEXT("None is a no-op"), Pool.Manager->ShowAnimatedIcon(ERTSVerticalAnimatedIcon::None, FVector::ZeroVector));
	TestEqual(TEXT("None leaves slots free"), Pool.Manager->GetActiveIconCount(), 0);
	for (int32 RequestIndex = 0; RequestIndex < 5; ++RequestIndex)
	{
		TestTrue(TEXT("Requests reuse the bounded pool"), Pool.Manager->ShowAnimatedIcon(
			ERTSVerticalAnimatedIcon::Healing, FVector(RequestIndex, 0.0, 0.0)));
	}
	TestEqual(TEXT("Exhaustion does not duplicate active indices"), Pool.Manager->GetActiveIconCount(), 2);
	TestEqual(TEXT("Exhaustion does not grow slot storage"), Pool.Manager->GetPoolSize(), 2);
	Pool.Manager->Shutdown();
	TestEqual(TEXT("Shutdown removes all slots"), Pool.Manager->GetPoolSize(), 0);
	TestFalse(TEXT("Shutdown stops readiness"), Pool.Manager->IsReady());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimatedIconAttachmentTest, "RTS.UI.AnimatedIcons.Attachment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimatedIconAttachmentTest::RunTest(const FString& Parameters)
{
	AnimatedIconPoolTests::FScopedPool Pool;
	if (not TestTrue(TEXT("Pool initializes"), Pool.Initialize()))
	{
		return false;
	}
	AActor* Subject = Pool.World->SpawnActor<AActor>();
	if (not TestNotNull(TEXT("Subject created"), Subject))
	{
		return false;
	}
	TestFalse(TEXT("Missing root is rejected before allocation"), Pool.Manager->ShowAnimatedIconAttachedToActor(
		ERTSVerticalAnimatedIcon::Healing, Subject, FVector::ZeroVector));
	TestEqual(TEXT("Failed attachment keeps capacity free"), Pool.Manager->GetActiveIconCount(), 0);
	USceneComponent* Root = NewObject<USceneComponent>(Subject);
	Subject->AddInstanceComponent(Root);
	Subject->SetRootComponent(Root);
	Root->RegisterComponent();
	TestTrue(TEXT("Valid subject activates"), Pool.Manager->ShowAnimatedIconAttachedToActor(
		ERTSVerticalAnimatedIcon::Healing, Subject, FVector::ZeroVector));
	Subject->Destroy();
	Pool.Manager->Tick();
	TestEqual(TEXT("Destroyed subject releases its slot"), Pool.Manager->GetActiveIconCount(), 0);
	TestTrue(TEXT("Formerly attached slot can display at a fixed anchor"), Pool.Manager->ShowAnimatedIcon(
		ERTSVerticalAnimatedIcon::Healing, FVector::ZeroVector));
	return true;
}

#endif
