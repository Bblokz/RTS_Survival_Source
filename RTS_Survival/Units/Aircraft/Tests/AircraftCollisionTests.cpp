// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "UObject/StrongObjectPtr.h"

namespace AircraftCollisionTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float BoxHalfExtent = 50.f;
	constexpr float ObstacleSpacing = 400.f;
	constexpr float TraceHalfLength = 200.f;

	/** @brief Keeps collision queries isolated from the loaded map and its actors. */
	struct FCollisionWorldFixture
	{
		TStrongObjectPtr<UWorld> World{UWorld::CreateWorld(EWorldType::EditorPreview, false)};

		~FCollisionWorldFixture()
		{
			if (not World.IsValid())
			{
				return;
			}
			World->DestroyWorld(false);
		}

		UBoxComponent* SpawnBox(const FVector& Location, const ECollisionChannel ObjectChannel) const
		{
			AActor* Actor = World->SpawnActor<AActor>();
			if (not IsValid(Actor))
			{
				return nullptr;
			}
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(BoxHalfExtent));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ObjectChannel);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			Actor->SetActorLocation(Location);
			return Box;
		}

		bool TraceHits(const FVector& Center, const ECollisionChannel TraceChannel) const
		{
			const FVector TraceOffset(TraceHalfLength, 0.f, 0.f);
			FHitResult Hit;
			return World->LineTraceSingleByChannel(Hit, Center - TraceOffset, Center + TraceOffset, TraceChannel);
		}

		/** A shot of this player: its weapon channel and the ignore mask that lets it pass its own and allied units. */
		bool WeaponTraceHits(const FVector& Center, const int32 ShootingPlayer) const
		{
			const FVector TraceOffset(TraceHalfLength, 0.f, 0.f);
			FCollisionQueryParams QueryParams;
			FRTSPlayerCollision::ApplyWeaponIgnoreMask(QueryParams, ShootingPlayer);
			FHitResult Hit;
			return World->LineTraceSingleByChannel(Hit, Center - TraceOffset, Center + TraceOffset,
			                                       FRTSPlayerCollision::GetWeaponTraceChannel(ShootingPlayer),
			                                       QueryParams);
		}

		bool WeaponSweepHits(const FVector& Center, const int32 ShootingPlayer) const
		{
			const FVector TraceOffset(TraceHalfLength, 0.f, 0.f);
			FCollisionQueryParams QueryParams;
			FRTSPlayerCollision::ApplyWeaponIgnoreMask(QueryParams, ShootingPlayer);
			FHitResult Hit;
			return World->SweepSingleByChannel(Hit, Center - TraceOffset, Center + TraceOffset, FQuat::Identity,
			                                   FRTSPlayerCollision::GetWeaponTraceChannel(ShootingPlayer),
			                                   FCollisionShape::MakeSphere(BoxHalfExtent), QueryParams);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftEnemyWeaponCollisionTest,
	"RTS.Aircraft.Collision.OnlyOpposingWeaponsHit",
	AircraftCollisionTestsPrivate::TestFlags)

bool FAircraftEnemyWeaponCollisionTest::RunTest(const FString& Parameters)
{
	using namespace AircraftCollisionTestsPrivate;
	FCollisionWorldFixture Fixture;
	if (not TestTrue(TEXT("Collision world exists"), Fixture.World.IsValid()))
	{
		return false;
	}
	for (const int32 OwningPlayer : {1, 2})
	{
		UBoxComponent* AircraftBox = Fixture.SpawnBox(FVector::ZeroVector, ECC_Pawn);
		if (not TestNotNull(TEXT("Aircraft hit geometry exists"), AircraftBox))
		{
			return false;
		}
		FRTS_CollisionSetup::SetupAircraftComponentCollision(AircraftBox, OwningPlayer, true);
		// Shots are tested the way weapons fire them: on the shooter's channel with its ignore mask.
		const int32 EnemyPlayer = FRTSPlayers::IsHumanPlayer(OwningPlayer)
			                          ? RTSPlayers::FirstAIPlayer
			                          : RTSPlayers::HumanPlayer;
		TestTrue(TEXT("Enemy weapon line trace hits"), Fixture.WeaponTraceHits(FVector::ZeroVector, EnemyPlayer));
		TestFalse(TEXT("Friendly weapon line trace passes through"),
			Fixture.WeaponTraceHits(FVector::ZeroVector, OwningPlayer));
		TestFalse(TEXT("Shared projectile object channel cannot bypass team filtering"),
			Fixture.TraceHits(FVector::ZeroVector, COLLISION_OBJ_PROJECTILE));
		TestTrue(TEXT("Mouse selection still hits"), Fixture.TraceHits(FVector::ZeroVector, ECC_Visibility));
		TestTrue(TEXT("Enemy projectile sweep hits"), Fixture.WeaponSweepHits(FVector::ZeroVector, EnemyPlayer));
		TestFalse(TEXT("Friendly projectile sweep passes through"),
			Fixture.WeaponSweepHits(FVector::ZeroVector, OwningPlayer));
		FRTS_CollisionSetup::SetupAircraftComponentCollision(AircraftBox, OwningPlayer, false);
		TestFalse(TEXT("Non-hit primitives cannot intercept enemy weapons"),
			Fixture.WeaponTraceHits(FVector::ZeroVector, EnemyPlayer));
		TestTrue(TEXT("Non-hit selection geometry still works"), Fixture.TraceHits(FVector::ZeroVector, ECC_Visibility));
		AircraftBox->GetOwner()->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftUnobstructedFlightTest,
	"RTS.Aircraft.Collision.FliesThroughAircraftAndBoundaries",
	AircraftCollisionTestsPrivate::TestFlags)

bool FAircraftUnobstructedFlightTest::RunTest(const FString& Parameters)
{
	using namespace AircraftCollisionTestsPrivate;
	FCollisionWorldFixture Fixture;
	if (not TestTrue(TEXT("Collision world exists"), Fixture.World.IsValid()))
	{
		return false;
	}
	int32 ObstacleIndex = 0;
	for (const ECollisionChannel ObjectChannel : {ECC_WorldStatic, ECC_WorldDynamic, COLLISION_OBJ_PLAYER, COLLISION_OBJ_ENEMY})
	{
		const FVector ObstacleLocation(++ObstacleIndex * ObstacleSpacing, 0.f, 0.f);
		if (not TestNotNull(TEXT("Blocking boundary or unit exists"), Fixture.SpawnBox(ObstacleLocation, ObjectChannel)))
		{
			return false;
		}
	}
	const FVector Destination((ObstacleIndex + 1) * ObstacleSpacing, 0.f, 0.f);
	for (const int32 OwningPlayer : {1, 2})
	{
		UBoxComponent* AircraftBox = Fixture.SpawnBox(FVector::ZeroVector, ECC_Pawn);
		if (not TestNotNull(TEXT("Aircraft movement primitive exists"), AircraftBox))
		{
			return false;
		}
		FRTS_CollisionSetup::SetupAircraftComponentCollision(AircraftBox, OwningPlayer, true);
		FHitResult MovementHit;
		AircraftBox->GetOwner()->SetActorLocation(Destination, true, &MovementHit);
		TestFalse(TEXT("Aircraft movement has no blocking hit"), MovementHit.bBlockingHit);
		TestTrue(TEXT("Aircraft reaches the drop-off position beyond every blocker"),
			AircraftBox->GetComponentLocation().Equals(Destination));
		AircraftBox->GetOwner()->Destroy();
	}
	return true;
}

#endif
