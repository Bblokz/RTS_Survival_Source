// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Units/Ragdoll/RTSPooledRagdoll.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSubsystem.h"

namespace RagdollTestWorld
{
	constexpr float FrameSeconds = 1.f / 60.f;
	// Long enough for the staggered prewarm to fill both pools.
	constexpr float PrewarmSeconds = 1.5f;
	constexpr float GroundHalfExtentCm = 5000.f;
	constexpr float GroundHalfHeightCm = 50.f;

	/**
	 * @brief A game world with a flat ground box (its top at Z = 0) and the ragdoll and corpse subsystems begun,
	 * ticked by hand so the ragdoll tests run under -nullrhi.
	 */
	struct FRagdollTestWorld
	{
		UWorld* World = nullptr;

		FRagdollTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World == nullptr)
			{
				return;
			}
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			SpawnGround();
			World->BeginPlay();
		}

		~FRagdollTestWorld()
		{
			if (World == nullptr)
			{
				return;
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		URTSRagdollSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<URTSRagdollSubsystem>() : nullptr;
		}

		/** Ticks frame by frame; the timer manager ticks once per GFrameCounter, so every frame advances it. */
		void TickFor(const float Seconds, const TFunctionRef<void()> AfterEachFrame = [] {}) const
		{
			const int32 Frames = FMath::CeilToInt(Seconds / FrameSeconds);
			for (int32 FrameIndex = 0; FrameIndex < Frames; ++FrameIndex)
			{
				++GFrameCounter;
				World->Tick(LEVELTICK_All, FrameSeconds);
				AfterEachFrame();
			}
		}

		TArray<ARTSPooledRagdoll*> GetAllBodies() const
		{
			TArray<ARTSPooledRagdoll*> Bodies;
			for (TActorIterator<ARTSPooledRagdoll> It(World); It; ++It)
			{
				Bodies.Add(*It);
			}
			return Bodies;
		}

		/** Woken bodies are the visible ones; parked bodies are hidden. */
		TArray<ARTSPooledRagdoll*> GetWokenBodies() const
		{
			TArray<ARTSPooledRagdoll*> Bodies = GetAllBodies();
			Bodies.RemoveAll([](const ARTSPooledRagdoll* Body) { return Body->IsHidden(); });
			return Bodies;
		}

	private:
		void SpawnGround() const
		{
			AActor* Ground = World->SpawnActor<AActor>();
			if (Ground == nullptr)
			{
				return;
			}
			UBoxComponent* GroundBox = NewObject<UBoxComponent>(Ground);
			Ground->AddInstanceComponent(GroundBox);
			Ground->SetRootComponent(GroundBox);
			GroundBox->SetBoxExtent(FVector(GroundHalfExtentCm, GroundHalfExtentCm, GroundHalfHeightCm));
			GroundBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			GroundBox->SetCollisionObjectType(ECC_WorldStatic);
			GroundBox->SetCollisionResponseToAllChannels(ECR_Block);
			GroundBox->RegisterComponent();
			Ground->SetActorLocation(FVector(0.f, 0.f, -GroundHalfHeightCm));
		}
	};
}

#endif
