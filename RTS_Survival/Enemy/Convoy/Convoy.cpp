// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Convoy.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

int32 AConvoy::M_NextFreeConvoyId = 0;

namespace ConvoyCoreConstants
{
	constexpr float TickIntervalSeconds = 0.25f;
}

bool FConvoyMember::GetIsAlive() const
{
	if (State == EConvoyMemberState::Dead or State == EConvoyMemberState::Released
		or State == EConvoyMemberState::Spawning)
	{
		return false;
	}
	if (Squad.IsValid())
	{
		return not Squad->GetSquadUnitsChecked().IsEmpty();
	}
	return Actor.IsValid() and RTSFunctionLibrary::RTSIsValid(Actor.Get());
}

ICommands* FConvoyMember::GetCommands() const
{
	return Actor.IsValid() ? Cast<ICommands>(Actor.Get()) : nullptr;
}

AConvoy::AConvoy()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
}

AConvoy* AConvoy::StartConvoy(const UObject* WorldContextObject, UConvoyDefinition* Definition,
                              const FConvoyRoute& Route, const FConvoyStartParams& Params)
{
	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("AConvoy::StartConvoy called without a world."));
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Start = Route.Points.IsEmpty() ? FVector::ZeroVector : Route.Points[0];
	AConvoy* Convoy = World->SpawnActor<AConvoy>(AConvoy::StaticClass(), Start, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Convoy))
	{
		RTSFunctionLibrary::ReportError(TEXT("AConvoy::StartConvoy could not spawn the convoy actor."));
		return nullptr;
	}
	if (not Convoy->InitConvoy(Definition, Route, Params))
	{
		Convoy->Destroy();
		return nullptr;
	}
	return Convoy;
}

bool AConvoy::InitConvoy(UConvoyDefinition* Definition, const FConvoyRoute& Route, const FConvoyStartParams& Params)
{
	M_Definition = Definition;
	if (not GetIsValidDefinition())
	{
		return false;
	}
	const TArray<FString> Problems = Definition->Validate();
	M_Polyline.Build(Route.Points);
	if (not Problems.IsEmpty() or not M_Polyline.GetIsValid())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("Convoy %s cannot start: %s%s"), *Definition->GetName(),
		                                                *FString::Join(Problems, TEXT(", ")),
		                                                M_Polyline.GetIsValid() ? TEXT("") : TEXT(" route has no length")));
		return false;
	}
	M_Route = Route;
	M_Params = Params;
	M_ConvoyId = M_NextFreeConvoyId++;
	M_TotalCargo = Definition->GetTotalCargo(Params.GuardTierIndex, Params.CargoMultiplier);
	M_CargoPerTruck = Definition->GetCargoPerTruck(Params.GuardTierIndex, Params.CargoMultiplier);
	M_StateStartTime = GetNow();
	BuildMembersFromDefinition();
	AssignSlots();
	// The column starts with its rear on the route start.
	M_ColumnReference = GetTruckColumnLength() + Definition->Spacing.RearDistance
		+ Definition->Spacing.StationRowSpacing;
	M_ColumnReference = FMath::Min(M_ColumnReference, M_Polyline.GetLength());
	Broadcast(EConvoyEventType::Started, nullptr,
	          FString::Printf(TEXT("definition=%s route=%s length=%.0f tier=%d members=%d cargo_total=%d label=%s"),
	                          *Definition->GetName(), *Route.RouteId.ToString(), M_Polyline.GetLength(),
	                          Params.GuardTierIndex, M_Members.Num(), M_TotalCargo.GetTotal(), *Params.DebugLabel),
	          M_Polyline.GetPointAtDistance(0.f));
	SpawnMembers();
	GetWorld()->GetTimerManager().SetTimer(M_TickTimer, this, &AConvoy::TickConvoy,
	                                       ConvoyCoreConstants::TickIntervalSeconds, true);
	return true;
}

bool AConvoy::GetIsValidDefinition() const
{
	if (IsValid(M_Definition))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Definition", "GetIsValidDefinition", this);
	return false;
}

void AConvoy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TickTimer);
		World->GetTimerManager().ClearTimer(M_SelfDestroyTimer);
		for (FConvoyCargoCache& Cache : M_Caches)
		{
			World->GetTimerManager().ClearTimer(Cache.ExpiryTimer);
		}
	}
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.Tank.IsValid() and Member.DeathHandle.IsValid())
		{
			Member.Tank->OnUnitDies.Remove(Member.DeathHandle);
		}
		Member.DeathHandle.Reset();
		if (M_Outcome == EConvoyOutcome::None and Member.GetIsAlive())
		{
			ReleaseMember(Member, false);
		}
	}
	for (FConvoyCargoCache& Cache : M_Caches)
	{
		if (Cache.Cache.IsValid())
		{
			Cache.Cache->OnScavengedOrDestroyed.RemoveAll(this);
			Cache.Cache->OnScavenged.RemoveAll(this);
		}
	}
	RemoveAllMinimapIcons();
	OnConvoyEventNative.Clear();
	Super::EndPlay(EndPlayReason);
}

void AConvoy::EndConvoy(const bool bDespawnUnits)
{
	FinishConvoy(EConvoyOutcome::Aborted, bDespawnUnits);
}

void AConvoy::SetState(const EConvoyState NewState, const FString& Reason)
{
	if (NewState == M_State)
	{
		return;
	}
	const EConvoyState OldState = M_State;
	M_State = NewState;
	M_StateStartTime = GetNow();
	Broadcast(EConvoyEventType::StateChanged, nullptr,
	          FString::Printf(TEXT("%s->%s %s"), *ConvoyNames::GetStateName(OldState),
	                          *ConvoyNames::GetStateName(NewState), *Reason), GetColumnCentre());
}

void AConvoy::Broadcast(const EConvoyEventType Type, const FConvoyMember* Member, const FString& Detail,
                        const FVector& Location)
{
	FConvoyEvent Event;
	Event.Type = Type;
	Event.ConvoyId = M_ConvoyId;
	Event.MemberId = Member != nullptr ? Member->MemberId : INDEX_NONE;
	Event.Actor = Member != nullptr ? Member->Actor : nullptr;
	Event.Location = Member != nullptr and Location.IsZero() ? Member->Location : Location;
	Event.Detail = Detail;
	Event.WorldSeconds = GetNow();
	UE_LOG(LogConvoy, Log, TEXT("CONVOY %d t=%.2f %s member=%d %s"), M_ConvoyId, Event.WorldSeconds,
	       *ConvoyNames::GetEventName(Type), Event.MemberId, *Detail);
	OnConvoyEventNative.Broadcast(Event);
	OnConvoyEvent.Broadcast(Event);
}

double AConvoy::GetNow() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0;
}

int32 AConvoy::GetAliveTruckCount() const
{
	int32 Count = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		Count += Member.Role == EConvoyUnitRole::Truck and Member.GetIsAlive() ? 1 : 0;
	}
	return Count;
}

FVector AConvoy::GetColumnCentre() const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.Role == EConvoyUnitRole::Truck and Member.GetIsAlive())
		{
			Sum += Member.Location;
			++Count;
		}
	}
	if (Count > 0)
	{
		return Sum / Count;
	}
	return M_Polyline.GetPointAtDistance(M_ColumnReference);
}

int32 AConvoy::GetOpenCacheCount() const
{
	int32 Count = 0;
	for (const FConvoyCargoCache& Cache : M_Caches)
	{
		Count += not Cache.bScavenged and not Cache.bExpired and not Cache.bSpawnFailed ? 1 : 0;
	}
	return Count;
}

void AConvoy::GetMemberSnapshots(TArray<FConvoyMemberSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset();
	for (const FConvoyMember& Member : M_Members)
	{
		FConvoyMemberSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
		Snapshot.MemberId = Member.MemberId;
		Snapshot.Role = Member.Role;
		Snapshot.Station = Member.Station;
		Snapshot.State = Member.State;
		Snapshot.SlotName = Member.SlotName;
		Snapshot.Actor = Member.Actor;
		Snapshot.Location = Member.Location;
		Snapshot.SlotLocation = GetSlotLocation(Member);
		Snapshot.RouteDistance = Member.RouteDistance;
		Snapshot.SlotRouteDistance = M_ColumnReference + Member.SlotAlongOffset;
		Snapshot.NominalSpeed = Member.NominalSpeed;
		Snapshot.AppliedSpeedFraction = Member.GetIsVehicle() ? Member.AppliedSpeedFraction : Member.AppliedMarchSpeed;
		Snapshot.WantedSpeed = Member.WantedSpeed;
		Snapshot.bIsStuck = Member.bIsStuck;
		Snapshot.bHasPartialPath = Member.bHasPartialPath;
	}
}

TArray<AActor*> AConvoy::GetAliveUnits() const
{
	TArray<AActor*> Units;
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.GetIsAlive())
		{
			Units.Add(Member.Actor.Get());
		}
	}
	return Units;
}

FConvoyMember* AConvoy::FindMember(const int32 MemberId)
{
	return M_Members.FindByPredicate([MemberId](const FConvoyMember& Member)
	{
		return Member.MemberId == MemberId;
	});
}

FConvoyCargoCache* AConvoy::FindCache(const int32 CacheId)
{
	return M_Caches.FindByPredicate([CacheId](const FConvoyCargoCache& Cache)
	{
		return Cache.CacheId == CacheId;
	});
}
