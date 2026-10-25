// Copyright (C) Bas Blokzijl - All rights reserved.

// The complete translation unit is absent from Shipping builds.
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Units/Squads/SoloInfantryUnit/SoloInfantryUnit.h"
#include "UObject/StrongObjectPtr.h"

/** @brief Exposes the standalone defaults and isolated cargo bookkeeping without adding production test hooks. */
struct FSoloInfantryUnitTestAccess
{
	static bool GetHidesInNearestFreeCover(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.bM_HideInNearestFreeCover;
	}

	static bool GetHasConfiguredCargo(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.M_EnterCargoActor.Get() != nullptr;
	}

	static bool GetCanStartAutomaticCover(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.GetCanUseAutomaticCover();
	}

	static bool GetMayKeepCover(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.GetMayKeepAutomaticCover();
	}

	static bool GetMayRelocateForStress(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.GetMayRelocateFromCoverForStress();
	}

	static bool GetIsHoldingPosition(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.GetIsSquadOnHoldPosition();
	}

	static bool GetHasSquadController(const ASoloInfantryUnit& SoloUnit)
	{
		return SoloUnit.GetIsValidSquadController();
	}

	static void ConfigureSingleSeatCargo(UCargo& Cargo, UPrimitiveComponent& CargoMesh)
	{
		Cargo.bM_IsEnabled = true;
		Cargo.M_CargoMesh = &CargoMesh;
		Cargo.M_VacancyState.M_MaxSquadsSupported = 1;
		Cargo.M_SocketOccupancy.M_AllCargoSockets = {TEXT("cargo_solo_test")};
	}

	static int32 GetEmptySeatCount(const UCargo& Cargo)
	{
		return Cargo.M_SocketOccupancy.GetEmptySockets().Num();
	}
};

namespace SoloInfantryUnitTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSoloInfantryUnitDefaultsAndCargoTest,
	"RTS.Squads.SoloInfantry.DefaultHoldAndCargoSeat",
	SoloInfantryUnitTestsPrivate::TestFlags)

bool FSoloInfantryUnitDefaultsAndCargoTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	ASoloInfantryUnit* SoloUnit = GetMutableDefault<ASoloInfantryUnit>();
	TestNotNull(TEXT("The solo infantry class has a default object"), SoloUnit);
	if (not IsValid(SoloUnit))
	{
		return false;
	}

	TestTrue(TEXT("Solo infantry remains a squad-unit subtype"), SoloUnit->IsA<ASquadUnit>());
	TestTrue(TEXT("Solo infantry updates its shared animation Blueprint without a squad tick"),
		SoloUnit->PrimaryActorTick.bCanEverTick);
	TestTrue(TEXT("Solo infantry is idle without a squad controller"), SoloUnit->GetIsUnitIdle());
	TestFalse(TEXT("A squad controller is intentionally absent"),
		FSoloInfantryUnitTestAccess::GetHasSquadController(*SoloUnit));
	TestTrue(TEXT("Solo infantry always holds its assigned position"),
		FSoloInfantryUnitTestAccess::GetIsHoldingPosition(*SoloUnit));
	TestFalse(TEXT("Solo infantry never starts recurring automatic cover searches"),
		FSoloInfantryUnitTestAccess::GetCanStartAutomaticCover(*SoloUnit));
	TestTrue(TEXT("Solo infantry keeps the one cover point it was assigned"),
		FSoloInfantryUnitTestAccess::GetMayKeepCover(*SoloUnit));
	TestFalse(TEXT("Stress never causes a second cover query"),
		FSoloInfantryUnitTestAccess::GetMayRelocateForStress(*SoloUnit));
	TestFalse(TEXT("Nearest-cover startup is opt-in"),
		FSoloInfantryUnitTestAccess::GetHidesInNearestFreeCover(*SoloUnit));
	TestFalse(TEXT("Cargo startup is opt-in"),
		FSoloInfantryUnitTestAccess::GetHasConfiguredCargo(*SoloUnit));

	TStrongObjectPtr<UCargo> Cargo(NewObject<UCargo>());
	UStaticMeshComponent* CargoMesh = NewObject<UStaticMeshComponent>(Cargo.Get());
	FSoloInfantryUnitTestAccess::ConfigureSingleSeatCargo(*Cargo, *CargoMesh);

	FName AssignedSocket = NAME_None;
	TestTrue(TEXT("A solo infantry unit can reserve one cargo seat"),
		Cargo->TryAssignSoloUnitSeat(SoloUnit, AssignedSocket));
	TestEqual(TEXT("The reserved seat is the configured cargo socket"),
		AssignedSocket,
		FName(TEXT("cargo_solo_test")));
	TestEqual(TEXT("The reserved solo seat is unavailable to later occupants"),
		FSoloInfantryUnitTestAccess::GetEmptySeatCount(*Cargo),
		0);

	Cargo->ReleaseSoloUnitSeat(SoloUnit);
	TestEqual(TEXT("Releasing the solo unit makes its seat available again"),
		FSoloInfantryUnitTestAccess::GetEmptySeatCount(*Cargo),
		1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
