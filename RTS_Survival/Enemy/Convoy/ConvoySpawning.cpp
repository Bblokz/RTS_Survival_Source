// Copyright (C) Bas Blokzijl - All rights reserved.

// Members of AConvoy that build the column from the definition and spawn its units.

#include "Convoy.h"

#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace ConvoySpawningConstants
{
	constexpr float KmhToCms = 100000.f / 3600.f;
	// Squads report their soldiers a few frames after the controller spawns.
	constexpr double SquadSoldierWaitSeconds = 25.0;
}

namespace
{
	ARTSAsyncSpawner* Convoy_FindAsyncSpawner(const UObject* WorldContextObject)
	{
		if (ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(WorldContextObject))
		{
			return Spawner;
		}
		UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
		if (World == nullptr)
		{
			return nullptr;
		}
		for (TActorIterator<ARTSAsyncSpawner> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}
}

void AConvoy::BuildMembersFromDefinition()
{
	M_Members.Reset();
	const TArray<FConvoyTruckSpec> TruckColumn = M_Definition->GetTruckColumn(M_Params.GuardTierIndex);
	for (const FConvoyTruckSpec& Truck : TruckColumn)
	{
		AddMember(EConvoyUnitRole::Truck, EConvoyStation::Lead, Truck.TrainingOption.GetDisplayName());
	}
	const FConvoyGuardTier* Tier = M_Definition->GetGuardTier(M_Params.GuardTierIndex);
	if (Tier == nullptr)
	{
		return;
	}
	for (const FConvoyEscortSpec& Escort : Tier->Escorts)
	{
		const EConvoyUnitRole EscortRole = Escort.Unit.UnitType == EAllUnitType::UNType_Squad
			                                   ? EConvoyUnitRole::EscortSquad
			                                   : EConvoyUnitRole::EscortVehicle;
		for (int32 Copy = 0; Copy < Escort.Count; ++Copy)
		{
			AddMember(EscortRole, Escort.Station, Escort.Unit.GetDisplayName());
		}
	}
}

void AConvoy::AddMember(const EConvoyUnitRole MemberRole, const EConvoyStation Station, const FString& UnitName)
{
	FConvoyMember& Member = M_Members.AddDefaulted_GetRef();
	Member.MemberId = M_Members.Num() - 1;
	Member.Role = MemberRole;
	Member.Station = Station;
	Member.UnitName = UnitName;
}

float AConvoy::GetTruckColumnLength() const
{
	int32 TruckCount = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		const bool bCounts = Member.Role == EConvoyUnitRole::Truck
			and (Member.State == EConvoyMemberState::Spawning or Member.GetIsAlive());
		TruckCount += bCounts ? 1 : 0;
	}
	return FMath::Max(0, TruckCount - 1) * M_Definition->Spacing.TruckGap;
}

void AConvoy::AssignSlots()
{
	const FConvoySpacingSettings& Spacing = M_Definition->Spacing;
	// Trucks keep their order along the route; the front one is slot 0.
	TArray<FConvoyMember*> Trucks;
	for (FConvoyMember& Member : M_Members)
	{
		const bool bActive = Member.State == EConvoyMemberState::Spawning or Member.GetIsAlive();
		if (Member.Role == EConvoyUnitRole::Truck and bActive)
		{
			Trucks.Add(&Member);
		}
	}
	Trucks.StableSort([](const FConvoyMember& A, const FConvoyMember& B)
	{
		return A.RouteDistance > B.RouteDistance;
	});
	for (int32 Index = 0; Index < Trucks.Num(); ++Index)
	{
		Trucks[Index]->SlotAlongOffset = -Index * Spacing.TruckGap;
		Trucks[Index]->SlotLateralOffset = 0.f;
		Trucks[Index]->SlotName = FString::Printf(TEXT("truck_%d"), Index);
	}
	const float TruckColumnLength = GetTruckColumnLength();
	AssignStationSlots(EConvoyStation::Lead, Spacing.LeadDistance, 1.f);
	AssignStationSlots(EConvoyStation::Rear, -TruckColumnLength - Spacing.RearDistance, -1.f);
	AssignStationSlots(EConvoyStation::LeftFlank, 0.f, -1.f);
	AssignStationSlots(EConvoyStation::RightFlank, 0.f, -1.f);
}

void AConvoy::AssignStationSlots(const EConvoyStation Station, const float RowStart, const float RowDirection)
{
	const FConvoySpacingSettings& Spacing = M_Definition->Spacing;
	TArray<FConvoyMember*> Escorts;
	for (FConvoyMember& Member : M_Members)
	{
		const bool bActive = Member.State == EConvoyMemberState::Spawning or Member.GetIsAlive();
		if (Member.Role != EConvoyUnitRole::Truck and Member.Station == Station and bActive)
		{
			Escorts.Add(&Member);
		}
	}
	const bool bIsFlank = Station == EConvoyStation::LeftFlank or Station == EConvoyStation::RightFlank;
	const float FlankSign = Station == EConvoyStation::LeftFlank ? -1.f : 1.f;
	const float TruckColumnLength = GetTruckColumnLength();
	for (int32 Index = 0; Index < Escorts.Num(); ++Index)
	{
		FConvoyMember& Escort = *Escorts[Index];
		if (bIsFlank)
		{
			// Spread along the trucks: a single flank escort walks beside the middle of the column.
			const float Fraction = (Index + 0.5f) / Escorts.Num();
			Escort.SlotAlongOffset = -TruckColumnLength * Fraction;
			Escort.SlotLateralOffset = FlankSign * (Escort.GetIsVehicle() ? Spacing.FlankVehicleOffset
				                                        : Spacing.FlankSquadOffset);
		}
		else
		{
			// Rows of two side by side; a lone escort drives on the centre line.
			const int32 Row = Index / 2;
			const bool bAloneInRow = Index == Escorts.Num() - 1 and Index % 2 == 0;
			const float Side = Index % 2 == 0 ? -1.f : 1.f;
			Escort.SlotAlongOffset = RowStart + RowDirection * Row * Spacing.StationRowSpacing;
			Escort.SlotLateralOffset = bAloneInRow ? 0.f : Side * Spacing.StationColumnSpacing * 0.5f;
		}
		Escort.SlotName = FString::Printf(TEXT("%s_%d"), *ConvoyNames::GetStationName(Station).ToLower(), Index);
	}
}

void AConvoy::SpawnMembers()
{
	const TArray<FConvoyTruckSpec> TruckColumn = M_Definition->GetTruckColumn(M_Params.GuardTierIndex);
	const FConvoyGuardTier* Tier = M_Definition->GetGuardTier(M_Params.GuardTierIndex);
	int32 MemberIndex = 0;
	M_PendingSpawns = M_Members.Num();
	for (const FConvoyTruckSpec& Truck : TruckColumn)
	{
		SpawnMember(MemberIndex++, Truck.TrainingOption, Truck.TruckClass);
	}
	if (Tier == nullptr)
	{
		return;
	}
	for (const FConvoyEscortSpec& Escort : Tier->Escorts)
	{
		for (int32 Copy = 0; Copy < Escort.Count; ++Copy)
		{
			SpawnMember(MemberIndex++, Escort.Unit, Escort.ClassOverride);
		}
	}
}

void AConvoy::SpawnMember(const int32 MemberIndex, const FTrainingOption& Option, const TSoftClassPtr<AActor>& ExactClass)
{
	if (not M_Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	FConvoyMember& Member = M_Members[MemberIndex];
	Member.SpawnRequestTime = GetNow();
	const int32 MemberId = Member.MemberId;
	ARTSAsyncSpawner* Spawner = Convoy_FindAsyncSpawner(this);
	FVector SpawnLocation = GetSlotLocation(Member);
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), SpawnLocation, SpawnLocation);
	const FRotator SpawnRotation = M_Polyline.GetDirectionAtDistance(M_ColumnReference + Member.SlotAlongOffset).Rotation();
	TWeakObjectPtr<AConvoy> WeakThis(this);
	auto OnSpawned = [WeakThis, MemberId](const FTrainingOption&, AActor* SpawnedActor, const int32)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnMemberSpawned(MemberId, SpawnedActor);
		}
	};
	bool bRequested = false;
	if (IsValid(Spawner))
	{
		bRequested = ExactClass.IsNull()
			             ? Spawner->AsyncSpawnOptionAtLocation(Option, SpawnLocation, this, MemberId, OnSpawned,
			                                                   SpawnRotation)
			             : Spawner->AsyncSpawnClassAtLocation(ExactClass, Option, SpawnLocation, this, MemberId,
			                                                  OnSpawned, SpawnRotation);
	}
	if (not bRequested)
	{
		Broadcast(EConvoyEventType::SpawnFailed, &M_Members[MemberIndex],
		          FString::Printf(TEXT("request failed for %s"), *Option.GetDisplayName()), SpawnLocation);
		M_Members[MemberIndex].State = EConvoyMemberState::Dead;
		--M_PendingSpawns;
	}
}

void AConvoy::OnMemberSpawned(const int32 MemberId, AActor* SpawnedActor)
{
	FConvoyMember* Member = FindMember(MemberId);
	if (Member == nullptr or Member->State != EConvoyMemberState::Spawning or M_Outcome != EConvoyOutcome::None)
	{
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->Destroy();
		}
		return;
	}
	if (not IsValid(SpawnedActor))
	{
		Broadcast(EConvoyEventType::SpawnFailed, Member, TEXT("spawner returned no actor"));
		Member->State = EConvoyMemberState::Dead;
		--M_PendingSpawns;
		return;
	}
	SetupSpawnedMember(*Member, SpawnedActor);
}

void AConvoy::SetupSpawnedMember(FConvoyMember& Member, AActor* SpawnedActor)
{
	Member.Actor = SpawnedActor;
	Member.Tank = Cast<ATankMaster>(SpawnedActor);
	Member.Squad = Cast<ASquadController>(SpawnedActor);
	Member.Location = SpawnedActor->GetActorLocation();
	Member.PreviousLocation = Member.Location;
	Member.RouteDistance = M_ColumnReference + Member.SlotAlongOffset;
	if (const URTSComponent* RTSComponent = SpawnedActor->FindComponentByClass<URTSComponent>())
	{
		if (RTSComponent->GetOwningPlayer() != M_Params.OwningPlayer)
		{
			UE_LOG(LogConvoy, Warning, TEXT("CONVOY %d %s spawned for player %d, convoy owner is %d"), M_ConvoyId,
			       *SpawnedActor->GetName(), RTSComponent->GetOwningPlayer(), M_Params.OwningPlayer);
		}
	}
	// Squads wait for their soldiers (UpdateMemberSamples promotes them); vehicles are ready now.
	if (Member.Squad.IsValid() and Member.Squad->GetSquadUnitsChecked().IsEmpty())
	{
		return;
	}
	Member.State = EConvoyMemberState::Station;
	Member.NominalSpeed = ReadNominalSpeed(Member);
	Member.CalibratedSpeed = Member.NominalSpeed;
	const TArray<FConvoyTruckSpec> TruckColumn = M_Definition->GetTruckColumn(M_Params.GuardTierIndex);
	// Trucks are the first members, in column order.
	if (Member.Role == EConvoyUnitRole::Truck and TruckColumn.IsValidIndex(Member.MemberId)
		and TruckColumn[Member.MemberId].CalibratedCruiseSpeedCms > 0.f)
	{
		Member.CalibratedSpeed = TruckColumn[Member.MemberId].CalibratedCruiseSpeedCms;
	}
	BindMemberDeath(Member);
	Broadcast(EConvoyEventType::UnitSpawned, &Member,
	          FString::Printf(TEXT("%s role=%s slot=%s nominal_speed=%.0f calibrated_speed=%.0f"),
	                          *SpawnedActor->GetName(), *ConvoyNames::GetRoleName(Member.Role), *Member.SlotName,
	                          Member.NominalSpeed, Member.CalibratedSpeed));
	--M_PendingSpawns;
	if (M_PendingSpawns <= 0)
	{
		OnSpawningComplete();
	}
}

float AConvoy::ReadNominalSpeed(const FConvoyMember& Member) const
{
	if (Member.Tank.IsValid())
	{
		const URTSComponent* RTSComponent = Member.Tank->FindComponentByClass<URTSComponent>();
		if (RTSComponent == nullptr)
		{
			return 0.f;
		}
		bool bIsValidData = false;
		const FTankData TankData = FRTS_Statics::BP_GetTankDataOfPlayer(
			RTSComponent->GetOwningPlayer(), RTSComponent->GetSubtypeAsTankSubtype(), Member.Tank.Get(), bIsValidData);
		return TankData.VehicleMaxSpeedKmh * ConvoySpawningConstants::KmhToCms;
	}
	if (not Member.Squad.IsValid())
	{
		return 0.f;
	}
	// The slowest soldier's own march speed (walk speed × march multiplier).
	float Slowest = TNumericLimits<float>::Max();
	for (const ASquadUnit* Soldier : Member.Squad->GetSquadUnitsChecked())
	{
		const UCharacterMovementComponent* Movement = IsValid(Soldier) ? Soldier->GetCharacterMovement() : nullptr;
		if (Movement != nullptr)
		{
			Slowest = FMath::Min(Slowest, Movement->MaxWalkSpeed * Soldier->GetMarchSpeedMultiplier());
		}
	}
	return Slowest == TNumericLimits<float>::Max() ? 0.f : Slowest;
}

void AConvoy::BindMemberDeath(FConvoyMember& Member)
{
	if (not Member.Tank.IsValid())
	{
		return;
	}
	TWeakObjectPtr<AConvoy> WeakThis(this);
	const int32 MemberId = Member.MemberId;
	Member.DeathHandle = Member.Tank->OnUnitDies.AddLambda([WeakThis, MemberId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnMemberDied(MemberId);
		}
	});
}

void AConvoy::OnSpawningComplete()
{
	if (M_State != EConvoyState::Spawning)
	{
		return;
	}
	// The pace: every truck at TruckSpeedFraction of its own top speed, the slowest one sets it.
	float Pace = TNumericLimits<float>::Max();
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.Role == EConvoyUnitRole::Truck and Member.GetIsAlive() and Member.NominalSpeed > 0.f)
		{
			Pace = FMath::Min(Pace, Member.CalibratedSpeed * M_Definition->Speed.TruckSpeedFraction);
		}
	}
	M_PaceSpeed = Pace == TNumericLimits<float>::Max() ? 0.f : Pace;
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.Role == EConvoyUnitRole::EscortVehicle and Member.GetIsAlive() and Member.NominalSpeed < M_PaceSpeed)
		{
			UE_LOG(LogConvoy, Warning, TEXT("CONVOY %d escort %s top speed %.0f is below the truck pace %.0f"),
			       M_ConvoyId, *Member.UnitName, Member.NominalSpeed, M_PaceSpeed);
		}
	}
	SetState(EConvoyState::Forming, FString::Printf(TEXT("pace=%.0f"), M_PaceSpeed));
}

void AConvoy::UpdateWaitingSquads(const double Now)
{
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.State != EConvoyMemberState::Spawning or not Member.Actor.IsValid())
		{
			continue;
		}
		if (Member.Squad.IsValid() and not Member.Squad->GetSquadUnitsChecked().IsEmpty())
		{
			SetupSpawnedMember(Member, Member.Actor.Get());
			continue;
		}
		if (Now - Member.SpawnRequestTime > ConvoySpawningConstants::SquadSoldierWaitSeconds)
		{
			Broadcast(EConvoyEventType::SpawnFailed, &Member, TEXT("squad never reported its soldiers"));
			Member.State = EConvoyMemberState::Dead;
			--M_PendingSpawns;
			if (M_PendingSpawns <= 0)
			{
				OnSpawningComplete();
			}
		}
	}
}
