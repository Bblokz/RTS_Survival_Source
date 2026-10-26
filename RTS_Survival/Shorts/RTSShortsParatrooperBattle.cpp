// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace RTSShortsParatrooperCameraPrivate
{
	/** @brief Keeps the position, aim and lens together while evaluating a camera route. */
	struct FCameraView
	{
		FVector Location = FVector::ZeroVector;
		FVector LookAt = FVector::ZeroVector;
		float FocalLength = 0.f;
	};

	/** @brief Retains one active route leg after elapsed time has passed earlier viewpoints. */
	struct FCameraLeg
	{
		FCameraView Start;
		FCameraView End;
		float ElapsedSeconds = 0.f;
		float DurationSeconds = 0.f;
		int32 Index = 0;
	};

	FCameraView MakeStageView(const FTransform& StageTransform, const FVector& LocalMidpoint,
		const FVector& Offset, const FVector& LookOffset, const float FocalLength)
	{
		FCameraView View;
		View.Location = StageTransform.TransformPosition(LocalMidpoint + Offset);
		View.LookAt = StageTransform.TransformPosition(LocalMidpoint + LookOffset);
		View.FocalLength = FocalLength;
		return View;
	}

	FCameraLeg FindActiveLeg(const FRTSShortsParatrooperCameraMove& Move, const FTransform& StageTransform,
		const FVector& LocalMidpoint, const FCameraView& InitialView, const float ElapsedSeconds)
	{
		FCameraLeg Leg;
		Leg.Start = InitialView;
		Leg.End = MakeStageView(StageTransform, LocalMidpoint, Move.EndOffset, Move.EndLookOffset, Move.EndFocalLength);
		Leg.ElapsedSeconds = FMath::Max(0.f, ElapsedSeconds);
		Leg.DurationSeconds = Move.DurationSeconds;
		for (const FRTSShortsParatrooperCameraWaypoint& Waypoint : Move.ContinuationPoints)
		{
			if (Waypoint.DurationSeconds <= 0.f)
			{
				continue;
			}
			if (Leg.ElapsedSeconds <= Leg.DurationSeconds)
			{
				return Leg;
			}
			Leg.ElapsedSeconds -= Leg.DurationSeconds;
			Leg.Start = Leg.End;
			Leg.End = MakeStageView(StageTransform, LocalMidpoint, Waypoint.Offset, Waypoint.LookOffset, Waypoint.FocalLength);
			Leg.DurationSeconds = Waypoint.DurationSeconds;
			++Leg.Index;
		}
		return Leg;
	}

	FCameraView InterpolateLeg(const FCameraLeg& Leg)
	{
		const float Alpha = FMath::Clamp(Leg.ElapsedSeconds / Leg.DurationSeconds, 0.f, 1.f);
		const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);
		FCameraView View;
		View.Location = FMath::Lerp(Leg.Start.Location, Leg.End.Location, Eased);
		View.LookAt = FMath::Lerp(Leg.Start.LookAt, Leg.End.LookAt, Eased);
		View.FocalLength = FMath::Lerp(Leg.Start.FocalLength, Leg.End.FocalLength, Eased);
		return View;
	}
}

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
	M_ParatrooperAbility->InitGlobalAbility(GetParatrooperOwningPlayer(), AbilityManager, M_PlayerController);
	M_ParatrooperAbility->SetCinematicLandingTrailLength(Segment.ParatrooperFormation.LandingTrailLength);
	M_ParatrooperAbility->SetCinematicJumpInterval(Segment.ParatrooperFormation.JumpIntervalSeconds);
	M_ParatrooperBattleCamera = M_Camera->GetActorTransform();
	bM_ParatrooperCameraCut = false;
	M_Orbit.bIsActive = false;
	if (Segment.ParatrooperFormation.Passes.IsEmpty())
	{
		constexpr float BattleMidpointFraction = 0.5f;
		FRTSShortsParatrooperPass DefaultPass;
		DefaultPass.DropOffset = Segment.TargetOffset * BattleMidpointFraction;
		DefaultPass.AircraftStartOffset = Segment.ParatrooperAircraftStartOffset;
		StartParatrooperPass(DefaultPass, 0);
	}
	for (int32 PassIndex = 0; PassIndex < Segment.ParatrooperFormation.Passes.Num(); ++PassIndex)
	{
		StartParatrooperPass(Segment.ParatrooperFormation.Passes[PassIndex], PassIndex);
	}
	TickParatrooperBattle(0);
}

void ARTSShortsDirector::StartParatrooperPass(const FRTSShortsParatrooperPass& Pass, const int32 PassIndex) const
{
	if (not GetIsValidStage() || not GetIsValidParatrooperAbility())
	{
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector DropLocation = StageTransform.TransformPosition(Pass.DropOffset);
	const FVector StartLocation = DropLocation + StageTransform.TransformVectorNoScale(Pass.AircraftStartOffset);
	M_ParatrooperAbility->ExecuteCinematicDrop(DropLocation, StartLocation);
	UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_BEGIN ability=%d drop=%s start=%s intro=%.2f pass=%d owner=%d"),
		static_cast<int32>(Segment.ParatrooperAbility), *DropLocation.ToString(), *StartLocation.ToString(),
		Segment.ParatrooperIntroSeconds, PassIndex, GetParatrooperOwningPlayer());
}

int32 ARTSShortsDirector::GetParatrooperOwningPlayer() const
{
	if (not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return M_Job.UnitOwningPlayer;
	}
	const int32 OverridePlayer = M_Job.Segments[M_CurrentSegment].ParatrooperFormation.OwningPlayer;
	return OverridePlayer > 0 ? OverridePlayer : M_Job.UnitOwningPlayer;
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
	KeepParatrooperAircraftVisible();
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
	UpdateParatrooperAircraftCamera(LocalFrame);
}

void ARTSShortsDirector::UpdateParatrooperAircraftCamera(const int32 LocalFrame) const
{
	if (not GetIsValidParatrooperAbility() || not GetIsValidCamera())
	{
		return;
	}
	const AAircraftMaster* LeadAircraft = M_ParatrooperAbility->GetActiveDropAircraft();
	if (not IsValid(LeadAircraft))
	{
		return;
	}
	const TArray<TWeakObjectPtr<AAircraftMaster>> AircraftFormation = M_ParatrooperAbility->GetActiveDropAircrafts();
	FBox FormationBounds(ForceInit);
	for (const TWeakObjectPtr<AAircraftMaster>& FormationAircraft : AircraftFormation)
	{
		if (const AAircraftMaster* Aircraft = FormationAircraft.Get())
		{
			FormationBounds += Aircraft->GetActorLocation();
		}
	}
	const FVector AircraftLocation = FormationBounds.IsValid ? FormationBounds.GetCenter() : LeadAircraft->GetActorLocation();
	const bool bHasFormation = AircraftFormation.Num() > 1;
	const FVector FormationCameraOffset(2200.f, -2400.f, 1100.f);
	const FVector SingleAircraftCameraOffset(-2200.f, -2400.f, 850.f);
	const FVector CameraOffset = bHasFormation ? FormationCameraOffset : SingleAircraftCameraOffset;
	const FRotator FramingRotation = bHasFormation
		? FRotator(0.f, LeadAircraft->GetActorRotation().Yaw, 0.f) : LeadAircraft->GetActorRotation();
	const FVector CameraLocation = AircraftLocation + FramingRotation.RotateVector(CameraOffset);
	M_Camera->SetActorLocationAndRotation(CameraLocation, (AircraftLocation - CameraLocation).Rotation());
	constexpr float AircraftFocalLengthMm = 18.f;
	constexpr float FormationFocalLengthMm = 9.5f;
	M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(bHasFormation ? FormationFocalLengthMm : AircraftFocalLengthMm);
	const int32 LogIntervalFrames = FMath::Max(1, M_Job.Fps / 2);
	if (LocalFrame % LogIntervalFrames == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_AIRCRAFT count=%d focus=%s camera=%s aircraftRotation=%s hidden=%d"),
			AircraftFormation.Num(), *AircraftLocation.ToString(), *CameraLocation.ToString(),
			*LeadAircraft->GetActorRotation().ToString(), LeadAircraft->IsHidden() ? 1 : 0);
	}
}

void ARTSShortsDirector::KeepParatrooperAircraftVisible() const
{
	if (not GetIsValidParatrooperAbility())
	{
		return;
	}
	const TArray<TWeakObjectPtr<AAircraftMaster>> AircraftFormation = M_ParatrooperAbility->GetActiveDropAircrafts();
	for (const TWeakObjectPtr<AAircraftMaster>& Aircraft : AircraftFormation)
	{
		PrepareParatrooperAircraftForCapture(Aircraft.Get());
	}
}

void ARTSShortsDirector::PrepareParatrooperAircraftForCapture(AAircraftMaster* Aircraft) const
{
	if (not IsValid(Aircraft))
	{
		return;
	}
	if (UFowComp* AircraftFog = Aircraft->FindComponentByClass<UFowComp>())
	{
		AircraftFog->StopFowParticipation();
	}
	Aircraft->SetActorHiddenInGame(false);
	const FName PreparedTag(TEXT("ShortsAircraftCapturePrepared"));
	const bool bLogPreparation = not Aircraft->Tags.Contains(PreparedTag);
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Aircraft);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		PrepareParatrooperAircraftMesh(Mesh, bLogPreparation);
	}
	Aircraft->Tags.AddUnique(PreparedTag);
}

void ARTSShortsDirector::PrepareParatrooperAircraftMesh(USkeletalMeshComponent* Mesh, const bool bFirstPreparation) const
{
	if (not IsValid(Mesh))
	{
		return;
	}
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->bEnableUpdateRateOptimizations = false;
	Mesh->SetComponentTickEnabled(true);
	Mesh->SetVisibility(true);
	Mesh->SetHiddenInGame(false);
	if (not bFirstPreparation)
	{
		return;
	}
	RepairParatrooperAircraftPropellerMaterial(Mesh);
}

void ARTSShortsDirector::RepairParatrooperAircraftPropellerMaterial(USkeletalMeshComponent* Mesh) const
{
	if (not IsValid(Mesh))
	{
		return;
	}
	UMaterialInterface* PropellerMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/FW_Recon/M_FWRecon_PropBlade.M_FWRecon_PropBlade"));
	if (not IsValid(PropellerMaterial))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cinematic aircraft need the German propeller blade material."));
		return;
	}
	const FName PlaceholderMaterialName(TEXT("WorldGridMaterial"));
	for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
	{
		const UMaterialInterface* CurrentMaterial = Mesh->GetMaterial(MaterialIndex);
		const UMaterial* BaseMaterial = IsValid(CurrentMaterial) ? CurrentMaterial->GetMaterial() : nullptr;
		if (not IsValid(BaseMaterial) || BaseMaterial->GetFName() != PlaceholderMaterialName)
		{
			continue;
		}
		Mesh->SetMaterial(MaterialIndex, PropellerMaterial);
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_AIRCRAFT_MATERIAL_REPAIRED aircraft=%s slot=%d material=%s"),
			*GetNameSafe(Mesh->GetOwner()), MaterialIndex, *PropellerMaterial->GetName());
	}
}

void ARTSShortsDirector::UpdateParatrooperBattleCamera(const int32 BattleLocalFrame) const
{
	using namespace RTSShortsParatrooperCameraPrivate;
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
	constexpr float MidpointFraction = 0.5f;
	const FVector LocalMidpoint = Segment.TargetOffset * MidpointFraction;
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector InitialFocus = StageTransform.TransformPosition(LocalMidpoint + FVector(0.f, 0.f, Segment.Camera.LookAtHeight));
	FCameraView InitialView;
	InitialView.Location = M_ParatrooperBattleCamera.GetLocation();
	const float InitialFocusDistance = FVector::Distance(InitialView.Location, InitialFocus);
	InitialView.LookAt = InitialView.Location + M_ParatrooperBattleCamera.GetRotation().GetForwardVector() * InitialFocusDistance;
	InitialView.FocalLength = Segment.Camera.FocalLength;
	const float ElapsedSeconds = static_cast<float>(BattleLocalFrame) / M_Job.Fps;
	const FCameraLeg Leg = FindActiveLeg(Move, StageTransform, LocalMidpoint, InitialView, ElapsedSeconds);
	const FCameraView View = InterpolateLeg(Leg);
	const FRotator Rotation = (View.LookAt - View.Location).Rotation();
	M_Camera->SetActorLocationAndRotation(View.Location, Rotation);
	M_Camera->GetCineCameraComponent()->SetCurrentFocalLength(View.FocalLength);
	constexpr int32 LogIntervalSeconds = 10;
	if (BattleLocalFrame % FMath::Max(1, M_Job.Fps * LogIntervalSeconds) == 0)
	{
		const float Alpha = FMath::Clamp(Leg.ElapsedSeconds / Leg.DurationSeconds, 0.f, 1.f);
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_CAMERA_MOVE local=%d alpha=%.3f location=%s pitch=%.2f focal=%.2f leg=%d"),
			BattleLocalFrame, Alpha, *View.Location.ToString(), Rotation.Pitch, View.FocalLength, Leg.Index);
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
	const int32 DropOwner = GetParatrooperOwningPlayer();
	TArray<TWeakObjectPtr<AActor>>& DropSideSquads = DropOwner == M_Job.UnitOwningPlayer
		? M_Battle.PlayerSquads : M_Battle.EnemySquads;
	for (const TWeakObjectPtr<ASquadController>& DropSquad : DropSquads)
	{
		ASquadController* Squad = DropSquad.Get();
		if (not IsValid(Squad) || DropSideSquads.Contains(Squad))
		{
			continue;
		}
		DropSideSquads.Add(Squad);
		if (URTSComponent* SquadRTS = Squad->FindComponentByClass<URTSComponent>())
		{
			SquadRTS->SetOwningPlayerRuntime(static_cast<uint8>(DropOwner));
		}
		UE_LOG(LogTemp, Display, TEXT("RTS_SHORTS_PARATROOPER_SQUAD squad=%s owner=%d"),
			*Squad->GetName(), DropOwner);
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
