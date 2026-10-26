// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void ARTSShortsDirector::BeginParatrooperBattle()
{
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	if (Segment.ParatrooperAbility == EGlobalAbility::GA_None
		|| not GetIsValidPlayerController() || not GetIsValidStage() || not GetIsValidCamera())
	{
		return;
	}
	UGlobalAbilitySetDataAsset* AbilitySet = LoadObject<UGlobalAbilitySetDataAsset>(nullptr,
		TEXT("/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem.DA_GlobalAbilitySystem"));
	UGlobalAbilitiesManager* AbilityManager = M_PlayerController->GetGlobalAbilitiesManager();
	if (not IsValid(AbilitySet) || not IsValid(AbilityManager))
	{
		RTSFunctionLibrary::ReportError(TEXT("Shorts paratrooper battle needs the global ability set and manager."));
		return;
	}
	M_ParatrooperAbility = Cast<UGA_Paratroopers>(AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Segment.ParatrooperAbility));
	if (not GetIsValidParatrooperAbility())
	{
		return;
	}
	M_ParatrooperAbility->InitGlobalAbility(M_Job.UnitOwningPlayer, AbilityManager, M_PlayerController);
	M_ParatrooperBattleCamera = M_Camera->GetActorTransform();
	bM_ParatrooperCameraCut = false;
	M_Orbit.bIsActive = false;
	constexpr float BattleMidpointFraction = 0.5f;
	const FVector DropLocation = M_Stage->GetActorTransform().TransformPosition(Segment.TargetOffset * BattleMidpointFraction);
	const FVector StartLocation = DropLocation + M_Stage->GetActorTransform().TransformVectorNoScale(Segment.ParatrooperAircraftStartOffset);
	M_ParatrooperAbility->ExecuteCinematicDrop(DropLocation, StartLocation);
	TickParatrooperBattle(0);
	UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_BEGIN ability=%d drop=%s start=%s intro=%.2f"),
		static_cast<int32>(Segment.ParatrooperAbility), *DropLocation.ToString(), *StartLocation.ToString(), Segment.ParatrooperIntroSeconds);
}

bool ARTSShortsDirector::GetIsValidParatrooperAbility() const
{
	if (IsValid(M_ParatrooperAbility))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ParatrooperAbility", "GetIsValidParatrooperAbility", this);
	return false;
}

void ARTSShortsDirector::TickParatrooperBattle(const int32 LocalFrame)
{
	if (not M_Job.Segments.IsValidIndex(M_CurrentSegment)
		|| M_Job.Segments[M_CurrentSegment].ParatrooperAbility == EGlobalAbility::GA_None
		|| not GetIsValidCamera() || not GetIsValidParatrooperAbility())
	{
		return;
	}
	const int32 IntroFrames = FMath::RoundToInt(M_Job.Segments[M_CurrentSegment].ParatrooperIntroSeconds * M_Job.Fps);
	if (LocalFrame >= IntroFrames)
	{
		if (not bM_ParatrooperCameraCut)
		{
			bM_ParatrooperCameraCut = true;
			M_Camera->SetActorTransform(M_ParatrooperBattleCamera);
			M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(M_Job.Segments[M_CurrentSegment].Camera.FocalLength);
			ApplyCameraCut(M_ParatrooperBattleCamera.GetLocation());
			UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_CUT local=%d introFrames=%d"), LocalFrame, IntroFrames);
		}
		UpdateParatrooperBattleCamera(LocalFrame - IntroFrames);
		return;
	}
	const AAircraftMaster* Aircraft = M_ParatrooperAbility->GetActiveDropAircraft();
	if (not IsValid(Aircraft))
	{
		return;
	}
	const FVector AircraftLocation = Aircraft->GetActorLocation();
	const FVector CameraOffset(-2200.f, -2400.f, 850.f);
	const FVector CameraLocation = AircraftLocation + Aircraft->GetActorRotation().RotateVector(CameraOffset);
	M_Camera->SetActorLocationAndRotation(CameraLocation, (AircraftLocation - CameraLocation).Rotation());
	constexpr float AircraftFocalLengthMm = 18.f;
	M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(AircraftFocalLengthMm);
}

void ARTSShortsDirector::UpdateParatrooperBattleCamera(const int32 BattleLocalFrame) const
{
	if (not M_Job.Segments.IsValidIndex(M_CurrentSegment) || not GetIsValidStage() || not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	const FRTSShortsParatrooperCameraMove& Move = Segment.ParatrooperCameraMove;
	if (Move.DurationSeconds <= 0.f)
	{
		return;
	}
	const float Alpha = FMath::Clamp(static_cast<float>(BattleLocalFrame) / (M_Job.Fps * Move.DurationSeconds), 0.f, 1.f);
	const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);
	constexpr float MidpointFraction = 0.5f;
	const FVector LocalMidpoint = Segment.TargetOffset * MidpointFraction;
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector InitialFocus = StageTransform.TransformPosition(LocalMidpoint + FVector(0.f, 0.f, Segment.Camera.LookAtHeight));
	const FVector StartLocation = M_ParatrooperBattleCamera.GetLocation();
	const float InitialFocusDistance = FVector::Distance(StartLocation, InitialFocus);
	const FVector StartLookAt = StartLocation + M_ParatrooperBattleCamera.GetRotation().GetForwardVector() * InitialFocusDistance;
	const FVector EndLocation = StageTransform.TransformPosition(LocalMidpoint + Move.EndOffset);
	const FVector EndLookAt = StageTransform.TransformPosition(LocalMidpoint + Move.EndLookOffset);
	const FVector Location = FMath::Lerp(StartLocation, EndLocation, Eased);
	const FVector LookAt = FMath::Lerp(StartLookAt, EndLookAt, Eased);
	const FRotator Rotation = (LookAt - Location).Rotation();
	const float FocalLength = FMath::Lerp(Segment.Camera.FocalLength, Move.EndFocalLength, Eased);
	M_Camera->SetActorLocationAndRotation(Location, Rotation);
	M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(FocalLength);
	constexpr int32 LogIntervalSeconds = 10;
	if (BattleLocalFrame % FMath::Max(1, M_Job.Fps * LogIntervalSeconds) == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_CAMERA_MOVE local=%d alpha=%.3f location=%s pitch=%.2f focal=%.2f"),
			BattleLocalFrame, Alpha, *Location.ToString(), Rotation.Pitch, FocalLength);
	}
}

void ARTSShortsDirector::TrackParatrooperSquads()
{
	if (not M_Job.Segments.IsValidIndex(M_CurrentSegment)
		|| M_Job.Segments[M_CurrentSegment].ParatrooperAbility == EGlobalAbility::GA_None
		|| not GetIsValidParatrooperAbility())
	{
		return;
	}
	const TArray<TWeakObjectPtr<ASquadController>> DropSquads = M_ParatrooperAbility->GetActiveDropSquads();
	for (const TWeakObjectPtr<ASquadController>& DropSquad : DropSquads)
	{
		ASquadController* Squad = DropSquad.Get();
		if (not IsValid(Squad) || M_Battle.PlayerSquads.Contains(Squad))
		{
			continue;
		}
		M_Battle.PlayerSquads.Add(Squad);
		if (URTSComponent* SquadRTS = Squad->FindComponentByClass<URTSComponent>())
		{
			SquadRTS->SetOwningPlayerRuntime(static_cast<uint8>(M_Job.UnitOwningPlayer));
		}
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_SQUAD squad=%s owner=%d"),
			*Squad->GetName(), M_Job.UnitOwningPlayer);
	}
}

void ARTSShortsDirector::UpdateParatrooperRangeBehaviour(const bool bRemove) const
{
	if (M_Job.StageName != TEXT("ParatrooperMeadow")
		|| not M_Job.Segments.IsValidIndex(M_CurrentSegment)
		|| M_Job.Segments[M_CurrentSegment].ParatrooperAbility == EGlobalAbility::GA_None)
	{
		return;
	}
	// Snapshot the groups: behaviour callbacks must never invalidate the battle's arrays during iteration.
	TArray<TWeakObjectPtr<AActor>> Participants = M_Battle.PlayerUnits;
	Participants.Append(M_Battle.EnemyUnits);
	Participants.Append(M_Battle.PlayerSquads);
	Participants.Append(M_Battle.EnemySquads);
	for (const TWeakObjectPtr<AActor>& Participant : Participants)
	{
		UpdateParatrooperUnitRangeBehaviour(Participant.Get(), bRemove);
	}
}

void ARTSShortsDirector::UpdateParatrooperUnitRangeBehaviour(AActor* Unit, const bool bRemove) const
{
	if (not IsValid(Unit) || Cast<ASquadUnit>(Unit) != nullptr)
	{
		return;
	}
	UBehaviourComp* BehaviourComponent = Unit->FindComponentByClass<UBehaviourComp>();
	const TSubclassOf<UBehaviour> RangeBehaviour = LoadClass<UBehaviour>(nullptr,
		TEXT("/Game/RTS_Survival/Shorts/BP_ParatrooperMeadowDoubleRange.BP_ParatrooperMeadowDoubleRange_C"));
	if (not IsValid(BehaviourComponent) || RangeBehaviour == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Paratrooper meadow needs its range behaviour and unit behaviour component."));
		return;
	}
	if (bRemove)
	{
		BehaviourComponent->RemoveBehaviour(RangeBehaviour);
		Unit->Tags.Remove(FName(TEXT("ShortsMeadowRangeAudited")));
		return;
	}
	if (BehaviourComponent->GetBehaviourByClass(RangeBehaviour) != nullptr)
	{
		LogParatrooperUnitRanges(Unit);
		return;
	}
	BehaviourComponent->AddBehaviour(RangeBehaviour);
	UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_DOUBLE_RANGE unit=%s multiplier=2"), *Unit->GetName());
}

void ARTSShortsDirector::LogParatrooperUnitRanges(AActor* Unit) const
{
	const FName AuditedTag(TEXT("ShortsMeadowRangeAudited"));
	if (not IsValid(Unit) || Unit->Tags.Contains(AuditedTag))
	{
		return;
	}
	ASquadController* Squad = Cast<ASquadController>(Unit);
	const ATankMaster* Tank = Cast<ATankMaster>(Unit);
	const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(Unit);
	if (Squad == nullptr && Tank == nullptr && Expansion == nullptr)
	{
		return;
	}
	if (Squad != nullptr && not Squad->GetIsSquadFullyLoadedAndInitialized())
	{
		return;
	}
	const TArray<UWeaponState*> Weapons = Squad != nullptr ? Squad->GetWeaponsOfSquad()
		: (Expansion != nullptr ? Expansion->GetAllWeapons() : FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank));
	if (Weapons.IsEmpty())
	{
		return;
	}
	const URTSComponent* UnitRTS = Unit->FindComponentByClass<URTSComponent>();
	const int32 ParticipantOwner = IsValid(UnitRTS) ? UnitRTS->GetOwningPlayer() : INDEX_NONE;
	if (Expansion != nullptr)
	{
		ForceAnimationTicking(Unit);
	}
	for (const UWeaponState* Weapon : Weapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_RANGE_READY unit=%s owner=%d weapon=%s range=%.1f"),
			*Unit->GetName(), ParticipantOwner, *Weapon->GetName(), Weapon->GetRange());
	}
	if (Squad != nullptr)
	{
		const TArray<ASquadUnit*> Soldiers = Squad->GetSquadUnitsChecked();
		for (const ASquadUnit* Soldier : Soldiers)
		{
			const URTSComponent* SoldierRTS = IsValid(Soldier) ? Soldier->FindComponentByClass<URTSComponent>() : nullptr;
			UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_OWNER_READY squad=%s soldier=%s owner=%d expected=%d"),
				*Squad->GetName(), *GetNameSafe(Soldier), IsValid(SoldierRTS) ? SoldierRTS->GetOwningPlayer() : INDEX_NONE, ParticipantOwner);
		}
	}
	Unit->Tags.Add(AuditedTag);
}

void ARTSShortsDirector::EndParatrooperBattle()
{
	// The ability is optional on ordinary shorts; absence here is expected.
	if (M_ParatrooperAbility == nullptr)
	{
		return;
	}
	if (GetIsValidParatrooperAbility())
	{
		TrackParatrooperSquads();
		M_ParatrooperAbility->CancelActiveDrops();
	}
	M_ParatrooperAbility = nullptr;
}
