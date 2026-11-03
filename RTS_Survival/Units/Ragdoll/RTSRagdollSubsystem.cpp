// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSRagdollSubsystem.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RTSPooledRagdoll.h"
#include "RTSRagdollSettings.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitRagdoll.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRagdoll, Log, All);

namespace RTSRagdollSubsystemStatics
{
	constexpr int32 SideCount = 2;
	// A few bodies at a time, so filling the pool never shows as one long frame.
	constexpr int32 BodiesPerPrewarmStep = 2;
	constexpr float PrewarmStepSeconds = 0.1f;
	constexpr float UpdateIntervalSeconds = 0.05f;
	// Bodies fresh out of the launch can read as asleep for a moment.
	constexpr float MinSecondsBeforeSleepCounts = 0.5f;
	constexpr float FullCircleDegrees = 360.f;
}

bool URTSRagdollSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void URTSRagdollSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	M_CorpseMaterial = FSquadUnitRagdoll::CreateCorpseMaterial(this);
	M_Pools.SetNum(RTSRagdollSubsystemStatics::SideCount);
	LoadPool(M_Pools[static_cast<int32>(ERTSCrewSide::German)], Settings->GermanCrew, Settings->PoolSizePerSide);
	LoadPool(M_Pools[static_cast<int32>(ERTSCrewSide::Soviet)], Settings->SovietCrew, Settings->PoolSizePerSide);
	InWorld.GetTimerManager().SetTimer(M_PrewarmTimer, this, &URTSRagdollSubsystem::PrewarmStep,
	                                   RTSRagdollSubsystemStatics::PrewarmStepSeconds, true);
}

void URTSRagdollSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_PrewarmTimer);
		World->GetTimerManager().ClearTimer(M_UpdateTimer);
	}
	Super::Deinitialize();
}

void URTSRagdollSubsystem::LoadPool(FRTSCrewPool& OutPool, const FRTSCrewBodyType& CrewType,
                                    const int32 PoolSize) const
{
	OutPool.Mesh = CrewType.Mesh.LoadSynchronous();
	if (not IsValid(OutPool.Mesh))
	{
		RTSFunctionLibrary::ReportError("Crew ragdoll mesh failed to load: " + CrewType.Mesh.ToString());
		return;
	}
	for (const TSoftObjectPtr<UMaterialInterface>& SoftMaterial : CrewType.Materials)
	{
		if (UMaterialInterface* Material = SoftMaterial.LoadSynchronous())
		{
			OutPool.Materials.Add(Material);
		}
	}
	OutPool.BodiesLeftToPrewarm = PoolSize;
}

void URTSRagdollSubsystem::PrewarmStep()
{
	using namespace RTSRagdollSubsystemStatics;
	bool bHasBodiesLeft = false;
	for (FRTSCrewPool& Pool : M_Pools)
	{
		const int32 BodiesThisStep = FMath::Min(BodiesPerPrewarmStep, Pool.BodiesLeftToPrewarm);
		for (int32 BodyIndex = 0; BodyIndex < BodiesThisStep; ++BodyIndex)
		{
			if (ARTSPooledRagdoll* Body = SpawnParkedBody(Pool))
			{
				Pool.ParkedBodies.Add(Body);
			}
		}
		Pool.BodiesLeftToPrewarm -= BodiesThisStep;
		bHasBodiesLeft |= Pool.BodiesLeftToPrewarm > 0;
	}
	if (not bHasBodiesLeft)
	{
		GetWorld()->GetTimerManager().ClearTimer(M_PrewarmTimer);
	}
}

ARTSPooledRagdoll* URTSRagdollSubsystem::SpawnParkedBody(FRTSCrewPool& Pool) const
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARTSPooledRagdoll* Body = GetWorld()->SpawnActor<ARTSPooledRagdoll>(ARTSPooledRagdoll::StaticClass(),
	                                                                    FTransform::Identity, SpawnParameters);
	if (not IsValid(Body))
	{
		return nullptr;
	}
	Body->InitializeForPool(Pool.Mesh, M_CorpseMaterial);
	return Body;
}

int32 URTSRagdollSubsystem::RequestCrewBodies(const FRTSCrewBodyRequest& Request)
{
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	const int32 PoolIndex = static_cast<int32>(Request.Side);
	if (not M_Pools.IsValidIndex(PoolIndex))
	{
		return 0;
	}
	const FRTSCrewThrow& Throw = Request.bIsCookOff ? Settings->CookOffDeath : Settings->OrdinaryDeath;
	const int32 BodiesWanted = FMath::RandRange(Throw.MinBodies, Throw.MaxBodies);
	const int32 BodiesAllowed = FMath::Min(BodiesWanted, GetBodyCountForRequest(Throw));
	int32 BodiesThrown = 0;
	while (BodiesThrown < BodiesAllowed && not M_Pools[PoolIndex].ParkedBodies.IsEmpty())
	{
		WakeBody(PoolIndex, Request, Throw);
		++BodiesThrown;
	}
	M_BodiesRefused += BodiesWanted - BodiesThrown;
	UE_LOG(LogRTSRagdoll, Display, TEXT("RTS_RGC request side=%d cookoff=%d wanted=%d thrown=%d simulating=%d parked=%d"),
	       PoolIndex, Request.bIsCookOff ? 1 : 0, BodiesWanted, BodiesThrown, GetSimulatingRagdollCount(),
	       M_Pools[PoolIndex].ParkedBodies.Num());
	if (BodiesThrown > 0 && not GetWorld()->GetTimerManager().IsTimerActive(M_UpdateTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(M_UpdateTimer, this, &URTSRagdollSubsystem::UpdateActiveBodies,
		                                       RTSRagdollSubsystemStatics::UpdateIntervalSeconds, true);
	}
	return BodiesThrown;
}

int32 URTSRagdollSubsystem::GetBodyCountForRequest(const FRTSCrewThrow& Throw) const
{
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	const int32 Simulating = GetSimulatingRagdollCount();
	if (Simulating >= Settings->MaxSimulatingRagdolls)
	{
		return 0;
	}
	if (Simulating >= Settings->PressureSimulatingRagdolls)
	{
		return 1;
	}
	return Settings->MaxSimulatingRagdolls - Simulating;
}

bool URTSRagdollSubsystem::GetCanStartRagdoll() const
{
	return GetSimulatingRagdollCount() < GetDefault<URTSRagdollSettings>()->MaxSimulatingRagdolls;
}

int32 URTSRagdollSubsystem::GetParkedBodyCount(const ERTSCrewSide Side) const
{
	const int32 PoolIndex = static_cast<int32>(Side);
	return M_Pools.IsValidIndex(PoolIndex) ? M_Pools[PoolIndex].ParkedBodies.Num() : 0;
}

void URTSRagdollSubsystem::WakeBody(const int32 PoolIndex, const FRTSCrewBodyRequest& Request,
                                    const FRTSCrewThrow& Throw)
{
	using namespace RTSRagdollSubsystemStatics;
	FRTSCrewPool& Pool = M_Pools[PoolIndex];
	ARTSPooledRagdoll* Body = Pool.ParkedBodies.Pop(EAllowShrinking::No);
	const FVector OutwardDirection = FRotator(0.f, FMath::FRandRange(0.f, FullCircleDegrees), 0.f).Vector();
	const FVector StartLocation = Request.TopCentre + OutwardDirection * FMath::FRandRange(0.f, Request.SpreadRadius);
	const FRotator StartRotation(0.f, FMath::FRandRange(0.f, FullCircleDegrees), 0.f);
	UMaterialInterface* Material = Pool.Materials.IsEmpty()
		                               ? nullptr
		                               : Pool.Materials[FMath::RandRange(0, Pool.Materials.Num() - 1)].Get();
	Body->Wake(StartLocation, StartRotation, Material);

	const FVector Velocity = OutwardDirection * FMath::FRandRange(Throw.OutwardSpeed.X, Throw.OutwardSpeed.Y)
		+ FVector::UpVector * FMath::FRandRange(Throw.UpwardSpeed.X, Throw.UpwardSpeed.Y);
	const FVector Spin = FMath::VRand() * Throw.MaxSpinDegrees;
	const TWeakObjectPtr<ARTSPooledRagdoll> WeakBody = Body;
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakBody, Velocity, Spin]()
	{
		if (WeakBody.IsValid())
		{
			WeakBody->Launch(Velocity, Spin);
		}
	}));

	FRTSActiveCrewBody ActiveBody;
	ActiveBody.Body = Body;
	ActiveBody.PoolIndex = PoolIndex;
	M_ActiveBodies.Add(ActiveBody);
	++M_SimulatingCrew;
}

void URTSRagdollSubsystem::UpdateActiveBodies()
{
	for (int32 BodyIndex = M_ActiveBodies.Num() - 1; BodyIndex >= 0; --BodyIndex)
	{
		FRTSActiveCrewBody& ActiveBody = M_ActiveBodies[BodyIndex];
		if (not IsValid(ActiveBody.Body))
		{
			M_ActiveBodies.RemoveAtSwap(BodyIndex);
			continue;
		}
		ActiveBody.SecondsAlive += RTSRagdollSubsystemStatics::UpdateIntervalSeconds;
		if (not ActiveBody.bIsSinking)
		{
			UpdateFlyingBody(ActiveBody);
			M_SimulatingCrew -= ActiveBody.bIsSinking ? 1 : 0;
			continue;
		}
		if (UpdateSinkingBody(ActiveBody))
		{
			M_ActiveBodies.RemoveAtSwap(BodyIndex);
		}
	}
	if (M_ActiveBodies.IsEmpty())
	{
		GetWorld()->GetTimerManager().ClearTimer(M_UpdateTimer);
	}
}

void URTSRagdollSubsystem::UpdateFlyingBody(FRTSActiveCrewBody& ActiveBody) const
{
	using namespace RTSRagdollSubsystemStatics;
	const bool bHasLanded = ActiveBody.SecondsAlive >= MinSecondsBeforeSleepCounts && ActiveBody.Body->GetIsAsleep();
	if (not bHasLanded && ActiveBody.SecondsAlive < GetDefault<URTSRagdollSettings>()->MaxSimulationSeconds)
	{
		return;
	}
	ActiveBody.Body->Freeze();
	ActiveBody.bIsSinking = true;
}

bool URTSRagdollSubsystem::UpdateSinkingBody(FRTSActiveCrewBody& ActiveBody)
{
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	constexpr float Interval = RTSRagdollSubsystemStatics::UpdateIntervalSeconds;
	ActiveBody.SecondsSinking += Interval;
	if (ActiveBody.SecondsSinking < Settings->SinkSeconds)
	{
		ActiveBody.Body->Sink(Settings->SinkDepthCm * Interval / Settings->SinkSeconds);
		return false;
	}
	ActiveBody.Body->Park();
	M_Pools[ActiveBody.PoolIndex].ParkedBodies.Add(ActiveBody.Body);
	return true;
}
