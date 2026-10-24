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

		// Empty selection; the arrow's front is drawn whether or not any unit takes a slot.
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
	"RTS.Formation.MovePreview.ArrowFrontIsDrawnForAHiddenOwner",
	FormationMovePreviewTestsPrivate::TestFlags)

bool FFormationMovePreviewRendersDragTest::RunTest(const FString& Parameters)
{
	using namespace FormationMovePreviewTestsPrivate;
	constexpr float LocationTolerance = 0.5f;
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
	TestFalse(TEXT("A drag without a rotation arrow is a quick click"), Preview.GetIsArrowFormationReady());

	Preview.UpdatePreview(Fixture.MakeInput(FVector(0.0f, 100.0f, 0.0f)));
	TestFalse(TEXT("Until the arrow appears the click keeps the regular rectangle"),
		Preview.GetIsArrowFormationReady());
	TestEqual(TEXT("A quick click draws no front"), Preview.GetPreviewLineCount(), 0);

	FFormationMovePreviewInput ArrowInput = Fixture.MakeInput(FVector(300.0f, 0.0f, 0.0f));
	ArrowInput.bArrowFacingChosen = true;
	ArrowInput.ArrowRotation = FRotator::ZeroRotator;
	Preview.UpdatePreview(ArrowInput);
	TestTrue(TEXT("Once the arrow appears it shapes the formation"), Preview.GetIsArrowFormationReady());
	TestEqual(TEXT("The arrow is as long as the cursor is far from where the button went down"),
		Preview.GetArrowDrag().ArrowLength, 300.0f, LocationTolerance);
	TestEqual(TEXT("The front of the arrow formation is drawn at once"), Preview.GetPreviewLineCount(), 1);
	const TArray<FVector>& FrontPoints = Preview.GetFrontLine().GetPoints();
	if (TestEqual(TEXT("The front is one straight line"), FrontPoints.Num(), 2))
	{
		TestTrue(TEXT("The front reaches as far to the left as the arrow is long"),
			FrontPoints[0].Equals(FVector(0.0f, -300.0f, 0.0f), LocationTolerance));
		TestTrue(TEXT("The front reaches as far to the right as the arrow is long"),
			FrontPoints[1].Equals(FVector(0.0f, 300.0f, 0.0f), LocationTolerance));
	}

	// Updates faster than the rebuild interval still move the arrow the order is given with.
	ArrowInput.CursorLocation = FVector(500.0f, 0.0f, 0.0f);
	Preview.UpdatePreview(ArrowInput);
	TestEqual(TEXT("The order uses the latest arrow, also between rebuilds"),
		Preview.GetArrowDrag().ArrowLength, 500.0f, LocationTolerance);

	Preview.EndDrag();
	TestFalse(TEXT("Releasing the button ends the drag"), Preview.GetIsDragActive());
	TestFalse(TEXT("Releasing the button drops the arrow formation"), Preview.GetIsArrowFormationReady());
	TestEqual(TEXT("The front disappears with the drag"), Preview.GetPreviewLineCount(), 0);

	// Losing the move context in the middle of a drag drops the arrow formation as well.
	Preview.BeginDrag(FVector::ZeroVector);
	Preview.UpdatePreview(ArrowInput);
	FFormationMovePreviewInput NoContextInput = ArrowInput;
	NoContextInput.bFormationMoveContext = false;
	Preview.UpdatePreview(NoContextInput);
	TestFalse(TEXT("Without a formation selection there is no drag"), Preview.GetIsDragActive());
	TestEqual(TEXT("Without a formation selection nothing is drawn"), Preview.GetPreviewLineCount(), 0);
	return true;
}

#endif
