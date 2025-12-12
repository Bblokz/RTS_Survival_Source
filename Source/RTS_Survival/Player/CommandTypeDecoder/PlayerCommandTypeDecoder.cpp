#include "PlayerCommandTypeDecoder.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Resources/Resource.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/CaptureMechanic/CaptureMechanicHelpers.h"
#include "RTS_Survival/CaptureMechanic/CaptureInterface/CaptureInterface.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/PickupItems/Items/ItemsMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace
{
	// Avoid magic numbers: ID of the local/ally player as used by these decoders.
	constexpr int32 kPlayerOneId = 1;
}

UPlayerCommandTypeDecoder::UPlayerCommandTypeDecoder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

ECommandType UPlayerCommandTypeDecoder::DecodeTargetedActor(AActor*& ClickedActor, FTargetUnion& OutTarget) const
{
	if (not IsValid(ClickedActor))
	{
		return ECommandType::Movement;
	}

	// Normalize turrets to their owning parent (if any) before decoding.
	Decode_TurretParentRedirect(ClickedActor);

	ECommandType OutType;

	if (Decode_CargoActor(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_Character(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	// 3) Capture actors / anything implementing the capture interface.
	//    Must happen BEFORE HP / destructible decoding so capture actors
	//    are not misclassified as destructible env actors or generic HP actors.
	if (Decode_CaptureActor(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_HpActor(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_HpPawn(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_Item(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_Scavengeable(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_Resource(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}
	if (Decode_DestructibleEnv(ClickedActor, OutTarget, OutType))
	{
		return OutType;
	}

	return ECommandType::Movement;
}

EPlacementEffect UPlayerCommandTypeDecoder::DecodeCommandTypeIntoEffect(const ECommandType CommandType) const
{
	switch (CommandType)
	{
	case ECommandType::Movement:
	case ECommandType::AlliedCharacter:
	case ECommandType::AlliedBuilding:
		return EPlacementEffect::Effect_Movement;
	case ECommandType::EnemyActor:
	case ECommandType::EnemyCharacter:
	case ECommandType::Attack:
	case ECommandType::AttackGround:
		return EPlacementEffect::Effect_Attack;
	case ECommandType::HarvestResource:
		return EPlacementEffect::Effect_HarvestResource;
	case ECommandType::PickupItem:
		return EPlacementEffect::Effect_PickUpItem;
	case ECommandType::RotateTowards:
		return EPlacementEffect::Effect_Rotate;
	case ECommandType::Repair:
		return EPlacementEffect::Effect_Repair;
	default:
		return EPlacementEffect::Effect_RegularClick;
	}
}

void UPlayerCommandTypeDecoder::InitCommandTypeDecoder(
	TMap<EPlacementEffect, UNiagaraSystem*> ClickSystems,
	TMap<EPlacementEffect, UNiagaraSystem*> PlaceSystems)
{
	M_TClickVFX = ClickSystems;
	M_TPlaceVFX = PlaceSystems;
}

void UPlayerCommandTypeDecoder::CreatePlacementEffectIfCommandExecuted(
	const bool bResetAllPlacementEffects,
	const uint32 AmountCommandsExe,
	const FVector& Location,
	const EPlacementEffect CommandType,
	const bool bAlsoCreateClickEffect)
{
	if (AmountCommandsExe > 0)
	{
		const FRotator Rotation = GetRotationFromClickedLocation(Location);
		if (bResetAllPlacementEffects)
		{
			ResetAllPlacementEffects();
		}
		if (M_TPlaceVFX.Contains(CommandType))
		{
			SpawnPlacementSystemAtLocation(GetWorld(), CommandType, Location, Rotation);
			if (bAlsoCreateClickEffect)
			{
				SpawnPlacementSystemAtLocation(GetWorld(), CommandType, Location, Rotation);
			}
		}
		else
		{
			// Spawn movement by default.
			if (bAlsoCreateClickEffect)
			{
				SpawnPlacementSystemAtLocation(GetWorld(), EPlacementEffect::Effect_RegularClick, Location, Rotation);
			}
		}
	}
}

void UPlayerCommandTypeDecoder::ResetAllPlacementEffects()
{
	for (const auto EachVfx : M_TSpawnedSystems)
	{
		if (EachVfx)
		{
			EachVfx->DestroyComponent();
		}
	}
	M_TSpawnedSystems.Empty();
}

void UPlayerCommandTypeDecoder::SpawnPlacementEffectsForPrimarySelected(
	EAllUnitType PrimaryUnitType,
	AActor* PrimarySelectedUnit)
{
	//todo
}

void UPlayerCommandTypeDecoder::SpawnPlacementSystemAtLocation(
	const UWorld* World,
	const EPlacementEffect Effect,
	const FVector& Location,
	const FRotator& Rotation)
{
	if (IsValid(World))
	{
		UNiagaraSystem* System;
		if (M_TPlaceVFX.Contains(Effect))
		{
			System = M_TPlaceVFX[Effect];
		}
		else
		{
			if (M_TPlaceVFX.Contains(EPlacementEffect::Effect_RegularClick))
			{
				System = M_TPlaceVFX[EPlacementEffect::Effect_RegularClick];
			}
			else
			{
				return;
			}
		}
		// Spawn the system at the transform's location, using its rotation
		UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, System, Location, Rotation,
			FVector(1, 1, 1), true, true, ENCPoolMethod::AutoRelease, true);
		M_TSpawnedSystems.Add(NiagaraComponent);
	}
}

void UPlayerCommandTypeDecoder::SpawnClickSystemAtLocation(
	const UWorld* World,
	const EPlacementEffect Effect,
	const FVector& Location,
	const FRotator& Rotation)
{
	if (IsValid(World))
	{
		UNiagaraSystem* System;
		if (M_TPlaceVFX.Contains(Effect))
		{
			System = M_TPlaceVFX[Effect];
		}
		else
		{
			if (M_TPlaceVFX.Contains(EPlacementEffect::Effect_RegularClick))
			{
				System = M_TPlaceVFX[EPlacementEffect::Effect_RegularClick];
			}
			else
			{
				return;
			}
		}
		// Spawn the system at the transform's location, using its rotation
		UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, System, Location, Rotation,
			FVector(1, 1, 1), true, true, ENCPoolMethod::AutoRelease, true);
	}
}

FRotator UPlayerCommandTypeDecoder::GetRotationFromClickedLocation(const FVector& Location) const
{
	FHitResult HitResult;
	FVector Start;
	FVector End;

	FCollisionQueryParams CollisionParams;

	const float TraceDistance = 10.f;
	// Distance between traces.
	const float Offset = 10.0f;

	float AverageSlopeAngle = 0;
	int32 NumHits = 0;

	// Perform multiple traces around the clicked location
	for (float X = -Offset; X <= Offset; X += Offset)
	{
		for (float Y = -Offset; Y <= Offset; Y += Offset)
		{
			Start = Location + FVector(X, Y, TraceDistance);
			End = Location + FVector(X, Y, -TraceDistance);

			if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, CollisionParams))
			{
				AverageSlopeAngle += FMath::RadiansToDegrees(
					acosf(FVector::DotProduct(HitResult.Normal, FVector::UpVector)));
				NumHits++;
			}
		}
	}

	if (NumHits > 0)
	{
		// Get the average normal
		AverageSlopeAngle /= NumHits;
		FRotator Rotation = FRotator(0, 0, 0);
		Rotation.Pitch = AverageSlopeAngle;
		// Keep the yaw unchanged (Z rotation)
		Rotation.Yaw = 0;
		return Rotation;
	}

	return FRotator::ZeroRotator;
}

// ------------------ Decode helpers ------------------

void UPlayerCommandTypeDecoder::Decode_TurretParentRedirect(AActor*& ClickedActor) const
{
	if (not IsValid(ClickedActor))
	{
		return;
	}

	if (not ClickedActor->IsA(ACPPTurretsMaster::StaticClass()))
	{
		return;
	}

	AActor* Parent = ClickedActor->GetParentActor();
	if (IsValid(Parent))
	{
		ClickedActor = Parent;
	}
}

bool UPlayerCommandTypeDecoder::Decode_Character(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(ACharacterObjectsMaster::StaticClass()))
	{
		return false;
	}

	ACharacterObjectsMaster* Character = Cast<ACharacterObjectsMaster>(ClickedActor);
	OutTarget.TargetCharacter = Character;

	const bool bIsAllied =
		IsValid(Character) &&
		IsValid(Character->GetRTSComponent()) &&
		Character->GetRTSComponent()->GetOwningPlayer() == kPlayerOneId;

	OutType = bIsAllied ? ECommandType::AlliedCharacter : ECommandType::EnemyCharacter;
	return true;
}

bool UPlayerCommandTypeDecoder::Decode_CaptureActor(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (ClickedActor == nullptr)
	{
		return false;
	}

	ICaptureInterface* CaptureInterface = FCaptureMechanicHelpers::GetValidCaptureInterface(ClickedActor);
	if (CaptureInterface == nullptr)
	{
		// Does not implement the capture interface (or otherwise invalid) – not a capture target.
		return false;
	}

	// At this point we know the actor is a valid capture target.
	OutTarget.TargetActor = ClickedActor;
	OutType = ECommandType::CaptureActor;
	return true;
}
bool UPlayerCommandTypeDecoder::Decode_HpActor(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(AHPActorObjectsMaster::StaticClass()))
	{
		return false;
	}

	AHPActorObjectsMaster* HpActor = Cast<AHPActorObjectsMaster>(ClickedActor);

	const bool bIsAllied =
		IsValid(HpActor) &&
		IsValid(HpActor->GetRTSComponent()) &&
		HpActor->GetRTSComponent()->GetOwningPlayer() == kPlayerOneId;

	if (bIsAllied)
	{
		if (HpActor->IsA(ABuildingExpansion::StaticClass()))
		{
			// Note: original code cast from OutTarget.TargetActor looked unintended.
			// Casting from HpActor ensures correct pointer.
			OutTarget.TargetBxp = Cast<ABuildingExpansion>(HpActor);
			OutType = ECommandType::AlliedBuilding;
			return true;
		}

		OutTarget.TargetActor = HpActor;
		OutType = ECommandType::AlliedActor;
		return true;
	}

	OutTarget.TargetActor = ClickedActor;
	OutType = ECommandType::EnemyActor;
	return true;
}

bool UPlayerCommandTypeDecoder::Decode_CargoActor(AActor* ClickedActor, FTargetUnion& OutTarget,
                                                  ECommandType& OutType)
{
	const UCargo* CargoComp = ClickedActor->FindComponentByClass<UCargo>();
	if (not IsValid(CargoComp) || not CargoComp->GetIsEnabledAndVacant())
	{
		return false;
	}
	const bool bIsAlliedToPlayer = GetIsActorAlliedToPlayer(ClickedActor);
	const bool bIsNeutral = GetIsActorNeutral(ClickedActor);
	if (bIsNeutral || bIsAlliedToPlayer)
	{
		OutTarget.TargetActor = ClickedActor;
		OutType = ECommandType::EnterCargo;
		return true;
	}
	RTSFunctionLibrary::PrintString("The clicked actor has a cargo component but is not allied to player;"
		"do not register as cargo actor target");
	return false;
}

bool UPlayerCommandTypeDecoder::Decode_HpPawn_IntoAircraftBuilding(AActor* ClickedActor, FTargetUnion& OutTarget,
                                                                   ECommandType& OutType) const
{
	if (not ClickedActor->IsA(ANomadicVehicle::StaticClass()))
	{
		return false;
	}
	ANomadicVehicle* Vehicle = Cast<ANomadicVehicle>(ClickedActor);
	if (UAircraftOwnerComp* AircraftOwner = Vehicle->GetAircraftOwnerComp())
	{
		OutTarget.TargetActor = ClickedActor;
		OutType = ECommandType::AlliedAirBase;
		return true;
	}
	return false;
}

bool UPlayerCommandTypeDecoder::Decode_HpPawn(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(AHpPawnMaster::StaticClass()))
	{
		return false;
	}

	AHpPawnMaster* HpPawn = Cast<AHpPawnMaster>(ClickedActor);
	OutTarget.TargetActor = ClickedActor;

	const bool bIsAllied =
		IsValid(HpPawn) &&
		IsValid(HpPawn->GetRTSComponent()) &&
		HpPawn->GetRTSComponent()->GetOwningPlayer() == kPlayerOneId;

	if (bIsAllied)
	{
		if (Decode_HpPawn_IntoAircraftBuilding(ClickedActor, OutTarget, OutType))
		{
			// Set target and OutType with the aircraft building decoder.
			return true;
		}
	}

	OutType = bIsAllied ? ECommandType::AlliedActor : ECommandType::EnemyActor;
	return true;
}

bool UPlayerCommandTypeDecoder::Decode_Item(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(AItemsMaster::StaticClass()))
	{
		return false;
	}

	OutTarget.PickupItem = Cast<AItemsMaster>(ClickedActor);
	OutType = ECommandType::PickupItem;
	return true;
}

bool UPlayerCommandTypeDecoder::Decode_Scavengeable(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(AScavengeableObject::StaticClass()))
	{
		return false;
	}

	OutTarget.ScavengeObject = Cast<AScavengeableObject>(ClickedActor);
	OutType = ECommandType::ScavengeObject;
	return true;
}

bool UPlayerCommandTypeDecoder::Decode_Resource(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(ACPPResourceMaster::StaticClass()))
	{
		return false;
	}

	OutTarget.TargetResource = Cast<ACPPResourceMaster>(ClickedActor);
	OutType = ECommandType::HarvestResource;
	return true;
}

bool UPlayerCommandTypeDecoder::Decode_DestructibleEnv(
	AActor* ClickedActor,
	FTargetUnion& OutTarget,
	ECommandType& OutType) const
{
	if (not ClickedActor->IsA(ADestructableEnvActor::StaticClass()))
	{
		return false;
	}

	OutTarget.TargetActor = ClickedActor;
	OutType = ECommandType::DestructableEnvActor;
	return true;
}

bool UPlayerCommandTypeDecoder::GetIsActorAlliedToPlayer(AActor* ClickedActor)
{
	return IsOfFaction(ClickedActor, 1);
}

bool UPlayerCommandTypeDecoder::GetIsActorNeutral(AActor* ClickedActor)
{
	return IsOfFaction(ClickedActor, 0);
}

bool UPlayerCommandTypeDecoder::IsOfFaction(AActor* ClickedActor, const uint8 OwningPlayer)
{
	// Are we checking for neutral units?
	if (OwningPlayer == 0)
	{
		// Then allow for any base class of unit.
		URTSComponent* RTSComp = ClickedActor->FindComponentByClass<URTSComponent>();
		if (not RTSComp)
		{
			return false;
		}
		return RTSComp->GetOwningPlayer() == 0;
	}
	bool bIsAllied = false;
	if (AHpPawnMaster* HpPawn = Cast<AHpPawnMaster>(ClickedActor))
	{
		bIsAllied =
			IsValid(HpPawn) &&
			IsValid(HpPawn->GetRTSComponent()) &&
			HpPawn->GetRTSComponent()->GetOwningPlayer() == kPlayerOneId;
		return bIsAllied;
	}
	if (AHPActorObjectsMaster* HpActor = Cast<AHPActorObjectsMaster>(ClickedActor))
	{
		bIsAllied =
			IsValid(HpActor) &&
			IsValid(HpActor->GetRTSComponent()) &&
			HpActor->GetRTSComponent()->GetOwningPlayer() == kPlayerOneId;
		return bIsAllied;
	}
	if (ASquadUnit* SquadUnit = Cast<ASquadUnit>(ClickedActor))
	{
		if (IsValid(SquadUnit->GetSquadControllerChecked()) &&
			IsValid(SquadUnit->GetSquadControllerChecked()->GetRTSComponent()))
		{
			bIsAllied = SquadUnit->GetSquadControllerChecked()->GetRTSComponent()->GetOwningPlayer() == kPlayerOneId;
			return bIsAllied;
		}
	}
	return bIsAllied;
}
