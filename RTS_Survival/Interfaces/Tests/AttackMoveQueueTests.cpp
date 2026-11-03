// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"

namespace AttackMoveQueueTestConstants
{
	// More fights than the queue has entries: a march that fights this often must not run the queue full.
	constexpr int32 FightsOnTheWay = 40;
	const FVector FirstDestination(1000.f, 0.f, 0.f);
	const FVector SecondDestination(2000.f, 0.f, 0.f);
}

/** @brief Records what the queue asks of a unit, without a world or any of the real movement behind it. */
class FAttackMoveTestUnit final : public ICommands
{
public:
	explicit FAttackMoveTestUnit(const bool bCanAttack)
	{
		M_CommandData.Reset(NewObject<UCommandData>());
		M_CommandData->InitCommandData(this);
		M_CommandData->SetPlayerResourceManger(NewObject<UPlayerResourceManager>());
		FUnitAbilityEntry MoveEntry;
		MoveEntry.AbilityId = EAbilityID::IdMove;
		FUnitAbilityEntry AttackEntry;
		AttackEntry.AbilityId = bCanAttack ? EAbilityID::IdAttack : EAbilityID::IdNoAbility;
		M_CommandData->SetAbilities({MoveEntry, AttackEntry, FUnitAbilityEntry()});
	}

	virtual FVector GetOwnerLocation() const override { return FVector::ZeroVector; }
	virtual AActor* GetOwnerActor() override { return nullptr; }
	virtual FString GetOwnerName() const override { return TEXT("AttackMoveTestUnit"); }
	virtual UCommandData* GetIsValidCommandData() override { return M_CommandData.Get(); }

	int32 MoveExecutions = 0;
	int32 AttackExecutions = 0;
	FVector LastMoveLocation = FVector::ZeroVector;
	// Whether the last terminated move would have kept rolling because it expected a hand-off to the next move.
	bool bLastMoveTerminationKeptVelocity = false;

protected:
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override
	{
		++MoveExecutions;
		LastMoveLocation = MoveToLocation;
	}

	virtual void TerminateMoveCommand() override
	{
		bLastMoveTerminationKeptVelocity = M_CommandData->GetHasQueuedMovementCommandAfterActive();
	}

	virtual void ExecuteAttackCommand(AActor* TargetActor) override
	{
		++AttackExecutions;
	}

	virtual void SetUnitToIdleSpecificLogic() override
	{
	}

	virtual void StopBehaviourTree() override
	{
	}

	virtual void StopMovement() override
	{
	}

private:
	TStrongObjectPtr<UCommandData> M_CommandData;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackMoveResumesAfterFightsTest, "RTS.Commands.AttackMove.ResumesAfterFights",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackMoveResumesAfterFightsTest::RunTest(const FString& Parameters)
{
	using namespace AttackMoveQueueTestConstants;
	FAttackMoveTestUnit Unit(true);
	UCommandData* CommandData = Unit.GetIsValidCommandData();

	TestEqual(TEXT("The order is accepted"),
	          Unit.AttackMoveToLocation(FirstDestination, true, FRotator::ZeroRotator),
	          ECommandQueueError::NoError);
	TestEqual(TEXT("A queued move sits behind the attack move"),
	          Unit.MoveToLocation(SecondDestination, false, FRotator::ZeroRotator),
	          ECommandQueueError::NoError);
	TestEqual(TEXT("An attack move reads as a move"), Unit.GetActiveCommandID(), EAbilityID::IdMove);
	TestTrue(TEXT("The active command is marked as an attack move"), CommandData->GetIsActiveCommandAttackMove());

	for (int32 FightIndex = 0; FightIndex < FightsOnTheWay; ++FightIndex)
	{
		const FString FightName = FString::Printf(TEXT("Fight %d"), FightIndex);
		TestEqual(*(FightName + TEXT(" is accepted")), Unit.AttackActorBeforeActiveCommand(nullptr),
		          ECommandQueueError::NoError);
		TestEqual(*(FightName + TEXT(" is the running command")), Unit.GetActiveCommandID(), EAbilityID::IdAttack);
		TestFalse(*(FightName + TEXT(" stops the unit despite the move queued behind it")),
		          Unit.bLastMoveTerminationKeptVelocity);

		Unit.DoneExecutingCommand(EAbilityID::IdAttack);
		TestTrue(*(FightName + TEXT(" hands back to the attack move")), CommandData->GetIsActiveCommandAttackMove());
		TestEqual(*(FightName + TEXT(" resumes towards the same location")), Unit.LastMoveLocation, FirstDestination);
	}
	TestEqual(TEXT("Every fight ran"), Unit.AttackExecutions, FightsOnTheWay);
	TestEqual(TEXT("The move ran once plus once per fight"), Unit.MoveExecutions, FightsOnTheWay + 1);

	Unit.DoneExecutingCommand(EAbilityID::IdMove);
	TestEqual(TEXT("Arrival starts the queued move"), Unit.GetActiveCommandID(), EAbilityID::IdMove);
	TestEqual(TEXT("The queued move has its own location"), Unit.LastMoveLocation, SecondDestination);
	TestFalse(TEXT("The queued move is a regular move"), CommandData->GetIsActiveCommandAttackMove());

	Unit.DoneExecutingCommand(EAbilityID::IdMove);
	TestEqual(TEXT("The unit ends idle"), Unit.GetActiveCommandID(), EAbilityID::IdIdle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackMoveUnarmedUnitTest, "RTS.Commands.AttackMove.UnarmedUnitWalks",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAttackMoveUnarmedUnitTest::RunTest(const FString& Parameters)
{
	using namespace AttackMoveQueueTestConstants;
	FAttackMoveTestUnit Unit(false);

	TestEqual(TEXT("The order is accepted"),
	          Unit.AttackMoveToLocation(FirstDestination, true, FRotator::ZeroRotator),
	          ECommandQueueError::NoError);
	TestEqual(TEXT("The unit moves"), Unit.GetActiveCommandID(), EAbilityID::IdMove);
	TestFalse(TEXT("A unit that cannot attack makes a regular move"),
	          Unit.GetIsValidCommandData()->GetIsActiveCommandAttackMove());
	return true;
}

#endif
