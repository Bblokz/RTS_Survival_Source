#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/Formation/FormationMovePreview/FormationMovePreviewComponent.h"
#include "RTS_Survival/Player/Formation/FormationMovement.h"
#include "RTS_Survival/Units/SquadController.h"
#include "UObject/StrongObjectPtr.h"

namespace FormationMovePreviewTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** A game world with the preview component on a hidden actor, which is what a player controller is. */
	struct FPreviewFixture
	{
		TStrongObjectPtr<UWorld> World;
		TObjectPtr<UFormationMovePreviewComponent> Preview = nullptr;

		// Empty selection; the drag history is drawn whether or not any unit takes a slot.
		TArray<ASquadController*> SelectedSquads;
		TArray<ASelectablePawnMaster*> SelectedPawns;
		TArray<ASelectableActorObjectsMaster*> SelectedActorMasters;

		~FPreviewFixture()
		{
			UWorld* TestWorld = World.Get();
			if (IsValid(TestWorld))
			{
				GEngine->DestroyWorldContext(TestWorld);
				TestWorld->DestroyWorld(false);
			}
		}

		bool Initialize()
		{
			World.Reset(UWorld::CreateWorld(EWorldType::EditorPreview, false));
			if (not World.IsValid())
			{
				return false;
			}
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			AActor* HiddenOwner = World->SpawnActor<AActor>();
			if (not IsValid(HiddenOwner))
			{
				return false;
			}
			HiddenOwner->SetActorHiddenInGame(true);
			UFormationController* FormationController = NewObject<UFormationController>(HiddenOwner);
			Preview = NewObject<UFormationMovePreviewComponent>(HiddenOwner);
			HiddenOwner->AddInstanceComponent(Preview);
			Preview->RegisterComponent();
			Preview->InitFormationMovePreview(FormationController);
			HiddenOwner->DispatchBeginPlay();
			return Preview->HasBegunPlay();
		}

		FFormationMovePreviewInput MakeInput(const FVector& CursorLocation)
		{
			FFormationMovePreviewInput Input;
			Input.SelectedSquads = &SelectedSquads;
			Input.SelectedPawns = &SelectedPawns;
			Input.SelectedActorMasters = &SelectedActorMasters;
			Input.bFormationMoveContext = true;
			Input.bCursorHit = true;
			Input.bCursorOnMoveGround = true;
			Input.CursorLocation = CursorLocation;
			return Input;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationMovePreviewRendersDragTest,
	"RTS.Formation.MovePreview.DragHistoryIsDrawnForAHiddenOwner",
	FormationMovePreviewTestsPrivate::TestFlags)

bool FFormationMovePreviewRendersDragTest::RunTest(const FString& Parameters)
{
	using namespace FormationMovePreviewTestsPrivate;
	FPreviewFixture Fixture;
	if (not TestTrue(TEXT("The preview fixture starts"), Fixture.Initialize()))
	{
		return false;
	}
	UFormationMovePreviewComponent& Preview = *Fixture.Preview;
	TestTrue(TEXT("The engine draws the preview although its owner is hidden, like a controller is"),
		Preview.GetCanPreviewRender());
	TestEqual(TEXT("Nothing is drawn before there is a preview"), Preview.GetPreviewLineCount(), 0);

	Preview.BeginDrag(FVector::ZeroVector);
	TestTrue(TEXT("Holding the secondary button starts the drag"), Preview.GetIsDragActive());
	TestFalse(TEXT("A drag that has not moved is not a line yet"), Preview.GetIsLineDragReady());

	Preview.UpdatePreview(Fixture.MakeInput(FVector(0.0f, 100.0f, 0.0f)));
	TestEqual(TEXT("The first stretch of the drag is drawn at once"), Preview.GetPreviewLineCount(), 1);
	TestFalse(TEXT("A short drag still is a regular move"), Preview.GetIsLineDragReady());

	// Every update extends the history, also the ones that come faster than the slots are rebuilt.
	Preview.UpdatePreview(Fixture.MakeInput(FVector(0.0f, 200.0f, 0.0f)));
	Preview.UpdatePreview(Fixture.MakeInput(FVector(0.0f, 300.0f, 0.0f)));
	TestEqual(TEXT("No part of the drag is lost between redraws"), Preview.GetDragPath().GetPoints().Num(), 4);
	TestTrue(TEXT("A long enough drag becomes a line to spread the units along"), Preview.GetIsLineDragReady());
	TestTrue(TEXT("The drag history stays on screen while dragging"), Preview.GetPreviewLineCount() >= 1);

	Preview.EndDrag();
	TestFalse(TEXT("Releasing the button ends the drag"), Preview.GetIsDragActive());
	TestEqual(TEXT("The drag history disappears with the drag"), Preview.GetPreviewLineCount(), 0);

	// Losing the move context in the middle of a drag drops the line as well.
	Preview.BeginDrag(FVector::ZeroVector);
	Preview.UpdatePreview(Fixture.MakeInput(FVector(0.0f, 100.0f, 0.0f)));
	FFormationMovePreviewInput NoContextInput = Fixture.MakeInput(FVector(0.0f, 200.0f, 0.0f));
	NoContextInput.bFormationMoveContext = false;
	Preview.UpdatePreview(NoContextInput);
	TestFalse(TEXT("Without a formation selection there is no drag"), Preview.GetIsDragActive());
	TestEqual(TEXT("Without a formation selection nothing is drawn"), Preview.GetPreviewLineCount(), 0);
	return true;
}

#endif
