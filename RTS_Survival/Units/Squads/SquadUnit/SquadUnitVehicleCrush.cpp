// Copyright (C) Bas Blokzijl - All rights reserved.

// The run-over part of ASquadUnit: a hostile vehicle that drives over a soldier kills him with a scream and crush blood,
// and a soldier standing in a vehicle's path may get out of the way. URTSVehicleCrushSubsystem decides who and when.

#include "SquadUnit.h"

#include "AISquadUnit/AISquadUnit.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundAttenuation.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Navigation/RTSNavigationHelpers/FRTSNavigationHelpers.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Units/Tanks/VehicleCrush/RTSVehicleCrushSettings.h"

namespace SquadUnitVehicleCrushStatics
{
	// The burst comes from the body, low: the hull is on top of him.
	constexpr float BurstHeightAboveGround = 35.f;
	constexpr float GroundSprayHeightAboveGround = 4.f;
	// How far up and down the dodge spot is searched for on the navmesh.
	const FVector DodgeProjectionExtent(150.f, 150.f, 250.f);
}

bool ASquadUnit::GetCanBeRunOver() const
{
	if (IsHidden() || IsValid(GetAttachParentActor()) || GetIsSquadInsideCargo())
	{
		// Riding on or inside a vehicle, or garrisoned out of sight.
		return false;
	}
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (not IsValid(Capsule) || not Capsule->IsCollisionEnabled())
	{
		return false;
	}
	if (M_CoverMoveGuard.bIsSnappedToCoverSocket)
	{
		// Snapped onto a building or wall socket, above or behind what a hull reaches.
		return false;
	}
	const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
	if (IsValid(Settings) && IsValid(RTSComponent) &&
		not Settings->GetIsCrushableSubtype(RTSComponent->GetSubtypeAsSquadSubtype()))
	{
		return false;
	}
	return true;
}

void ASquadUnit::OnRunOverByVehicle(AActor* Vehicle, const FVector& DriveDirection, const bool bMayScream)
{
	if (not IsUnitAlive() || not GetIsValidHealthComponent())
	{
		return;
	}
	bM_WasRunOver = true;
	// The scream is his death line; a selected soldier still reports the loss over the radio.
	if (not GetIsSelected())
	{
		bM_NoDeathVoiceLineOnDeath = true;
	}
	if (bMayScream)
	{
		RunOver_PlayScream();
	}
	RunOver_SpawnBlood(DriveDirection);

	if (M_RagdollSettings.bEnableRagdoll)
	{
		// The hull rolls over him; a full-damage hit would throw him like a shell does.
		const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
		M_DeathHit = FSquadUnitDeathHit();
		M_DeathHit.PushDirection = DriveDirection.GetSafeNormal2D();
		M_DeathHit.DamageFraction = IsValid(Settings) ? Settings->CorpsePushFraction : 0.f;
	}
	// Health first, so the squad's health bar loses him like any other death.
	HealthComponent->SetCurrentHealth(0.f);
	UnitDies(ERTSDeathType::Kinetic);
}

bool ASquadUnit::TryDodgeVehicle(const FVector& DodgeLocation)
{
	if (not RunOver_GetMayDodgeNow() || not GetIsValidAISquadUnit())
	{
		return false;
	}
	FVector DodgeGoal = DodgeLocation;
	if (const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (not NavigationSystem->ProjectPointToNavigation(
			DodgeLocation, Projected, SquadUnitVehicleCrushStatics::DodgeProjectionExtent))
		{
			return false;
		}
		DodgeGoal = Projected.Location;
	}
	FAIMoveRequest DodgeRequest(DodgeGoal);
	FRTSNavigationHelpers::ConfigureMoveRequestForPartialPathFinding(DodgeRequest);
	DodgeRequest.SetAcceptanceRadius(DeveloperSettings::GamePlay::Navigation::SquadUnitAcceptanceRadius);
	DodgeRequest.SetReachTestIncludesAgentRadius(true);
	DodgeRequest.SetCanStrafe(true);
	DodgeRequest.SetNavigationFilter(M_AISquadUnit->GetDefaultNavigationFilterClass());
	const EPathFollowingRequestResult::Type MoveResult = M_AISquadUnit->MoveTo(DodgeRequest);
	if (MoveResult != EPathFollowingRequestResult::RequestSuccessful)
	{
		return false;
	}
	M_VehicleDodgeRequestID = M_AISquadUnit->GetCurrentMoveRequestID();
	if (not M_AISquadUnit->ReceiveMoveCompleted.IsAlreadyBound(this, &ASquadUnit::OnMoveCompleted))
	{
		M_AISquadUnit->ReceiveMoveCompleted.AddDynamic(this, &ASquadUnit::OnMoveCompleted);
	}
	return M_VehicleDodgeRequestID.IsValid();
}

bool ASquadUnit::RunOver_GetMayDodgeNow() const
{
	// Starting a move while one runs aborts it, and the aborted move would end his squad's command early.
	if (GetIsPathFollowingActive() || M_VehicleDodgeRequestID.IsValid())
	{
		return false;
	}
	// Cover and stress keep their own moves and reservations; crews stay on their gun.
	if (GetHasCoverAssignment() || GetIsStressed() || M_CoverMoveGuard.bIsSnappedToCoverSocket)
	{
		return false;
	}
	if (IsValid(M_SquadController) && IsValid(Cast<ATeamWeaponController>(M_SquadController)))
	{
		return false;
	}
	switch (M_ActiveCommand)
	{
	case EAbilityID::IdIdle:
	case EAbilityID::IdNoAbility:
	case EAbilityID::IdAttack:
	case EAbilityID::IdAttackGround:
	case EAbilityID::IdMove:
	case EAbilityID::IdNoAbility_MoveToEvasionLocation:
		// Standing still with these: idle, firing, or arrived and waiting for the rest of his squad.
		return true;
	default:
		// Scavenging, repairing, healing, building, capturing: busy on the spot with a montage or a timer.
		return false;
	}
}

void ASquadUnit::RunOver_PlayScream() const
{
	const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
	if (not IsValid(Settings) || not IsValid(RTSComponent))
	{
		return;
	}
	USoundBase* const Scream = Settings->PickScream(RTSComponent->GetSubtypeAsSquadSubtype());
	if (not IsValid(Scream))
	{
		return;
	}
	const bool bIsHumanSoldier = FRTSPlayers::IsHumanPlayer(RTSComponent->GetOwningPlayer());
	USoundAttenuation* const Attenuation = bIsHumanSoldier
		                                       ? Settings->PlayerScreamAttenuation.LoadSynchronous()
		                                       : Settings->EnemyScreamAttenuation.LoadSynchronous();
	const float Pitch = FMath::FRandRange(Settings->ScreamPitch.X, Settings->ScreamPitch.Y);
	// Not through the spatial voice line player: that one only speaks for the human's units, and enemies scream too.
	UGameplayStatics::SpawnSoundAtLocation(
		this, Scream, GetActorLocation(), FRotator::ZeroRotator, 1.f, Pitch, 0.f, Attenuation);
}

void ASquadUnit::RunOver_SpawnBlood(const FVector& DriveDirection) const
{
	using namespace SquadUnitVehicleCrushStatics;
	const URTSVehicleCrushSettings* Settings = URTSVehicleCrushSettings::Get();
	UWorld* World = GetWorld();
	if (not IsValid(Settings) || not IsValid(World))
	{
		return;
	}
	const FVector Ground = GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	FVector Drive = DriveDirection.GetSafeNormal2D();
	if (Drive.IsNearlyZero())
	{
		Drive = GetActorForwardVector().GetSafeNormal2D();
	}
	const float DriveYaw = Drive.Rotation().Yaw;

	if (UNiagaraSystem* const Burst = Settings->CrushBloodBurst.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, Burst, Ground + FVector(0.f, 0.f, BurstHeightAboveGround), FRotator(0.f, DriveYaw, 0.f),
			FVector(Settings->CrushBloodBurstScale), true, true, ENCPoolMethod::AutoRelease);
	}
	if (UNiagaraSystem* const GroundSpray = Settings->CrushBloodGroundSpray.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, GroundSpray, Ground + FVector(0.f, 0.f, GroundSprayHeightAboveGround),
			FRotator(0.f, DriveYaw, 0.f), FVector(Settings->CrushBloodGroundSprayScale), true, true,
			ENCPoolMethod::AutoRelease);
	}
	if (UMaterialInterface* const DecalMaterial = Settings->CrushBloodDecal.LoadSynchronous())
	{
		const FVector HalfSize = Settings->CrushBloodDecalHalfSize;
		// Pitched down, the decal's X projects into the ground, its Z runs along the drive and its Y across it.
		UDecalComponent* const Decal = UGameplayStatics::SpawnDecalAtLocation(
			World, DecalMaterial, FVector(HalfSize.Z, HalfSize.Y, HalfSize.X), Ground,
			FRotator(-90.f, DriveYaw, FMath::FRandRange(-8.f, 8.f)));
		if (IsValid(Decal))
		{
			Decal->SetFadeOut(Settings->CrushBloodDecalLifetime, Settings->CrushBloodDecalFadeSeconds, false);
		}
	}
}
