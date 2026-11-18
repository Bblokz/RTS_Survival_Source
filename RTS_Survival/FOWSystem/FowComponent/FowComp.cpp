// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "FowComp.h"

#include "FowType.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


UFowComp::UFowComp(): VisionRadius(0), M_FowManager(nullptr), M_FowParticpationHandle(),
                      M_FowBehaviour(EFowBehaviour::Fow_Passive)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFowComp::SetVisionRadius(const float NewVisionRadius)
{
	if (NewVisionRadius <= 0)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NoOwner";
		RTSFunctionLibrary::ReportError("Vision radius set to 0 or less! for component: " + GetName() +
			"With owner: " + OwnerName);
		return;
	}
	VisionRadius = NewVisionRadius;
}


void UFowComp::StartFow(const EFowBehaviour FowComponentBehaviour, const bool bStartImmediately)
{
	M_FowBehaviour = FowComponentBehaviour;
	bM_HasStartedFow = true;

	if (bStartImmediately)
	{
		OnFowStartParticipation();
		return;
	}

	// As there is a delay between when the unit gets loaded and when it is actually selectable for the player we wait
	// a bit before we start participating in the FOW system.
	constexpr float TimeTillActivateFow = DeveloperSettings::GamePlay::Training::TimeRemainingStartAsyncLoading + 2.0f;
	StartFowParticipationInSeconds(TimeTillActivateFow);
}

void UFowComp::OnFowVisibilityUpdated(const float Visibility)
{
	using DeveloperSettings::GamePlay::FoW::TagUnitHiddenByFog;
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	using DeveloperSettings::GamePlay::FoW::VisibleEnoughToRevealEnemy;
	using DeveloperSettings::GamePlay::FoW::VisibleEnoughToRevealPlayer;
	AActor* FowOwner = GetOwner();
	if (not IsValid(FowOwner))
	{
		return;
	}
	if (M_FowBehaviour == EFowBehaviour::Fow_Active)
	{
		const FName VisionTag = DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
		SetShouldDrawMiniMapIcon(not FowOwner->IsHidden());
		if (Visibility >= VisibleEnoughToRevealPlayer)
		{
			if (not FowOwner->Tags.Contains(TagUnitInEnemyVision))
			{
				FowOwner->Tags.Add(TagUnitInEnemyVision);
			}
		}
		else
		{
			FowOwner->Tags.Remove(TagUnitInEnemyVision);
		}
		if constexpr (DeveloperSettings::Debugging::GFowComponents_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = FowOwner->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation, FString::Printf(TEXT("PlayerVis:: %f"), Visibility),
			                nullptr,
			                FColor::White, 2.0f, false);
		}
		return;
	}

	SetShouldDrawMiniMapIcon(Visibility >= VisibleEnoughToRevealEnemy);
	if (Visibility >= VisibleEnoughToRevealEnemy)
	{
		bM_OwnerHiddenByFog = false;
		FowOwner->Tags.Remove(TagUnitHiddenByFog);
		FowOwner->SetActorHiddenInGame(false);
		if constexpr (DeveloperSettings::Debugging::GFowComponents_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = FowOwner->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation, FString::Printf(TEXT("Visibility > .15! %f"), Visibility),
			                nullptr, FColor::Red, 1.0f, false);
		}
		return;
	}
	// Marked only when the fog is what hides the unit, so the other AI players keep seeing it. A unit that was
	// hidden already when the fog came is hidden by something else (still spawning, parked) and stays unmarked.
	bM_OwnerHiddenByFog = bM_OwnerHiddenByFog || not FowOwner->IsHidden();
	// Everything else that hides a unit also turns its collision off: while it is off the fog is not the only
	// reason, once it is back on the fog is again.
	if (bM_OwnerHiddenByFog && FowOwner->GetActorEnableCollision())
	{
		FowOwner->Tags.AddUnique(TagUnitHiddenByFog);
	}
	else
	{
		FowOwner->Tags.Remove(TagUnitHiddenByFog);
	}
	FowOwner->SetActorHiddenInGame(true);
	if constexpr (DeveloperSettings::Debugging::GFowComponents_Compile_DebugSymbols)
	{
		FVector DrawTextLocation = FowOwner->GetActorLocation() + FVector(0, 0, 300);
		DrawDebugString(GetWorld(), DrawTextLocation, FString::Printf(TEXT("Visibility: %f"), Visibility), nullptr,
		                FColor::White, 2.0f, false);
	}
}

void UFowComp::StartFowParticipationInSeconds(const float Seconds)
{
	if (not GetWorld())
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(M_FowParticpationHandle, this, &UFowComp::OnFowStartParticipation, Seconds,
	                                       false);
}

void UFowComp::StopFowParticipation()
{
	// A pending delayed start would otherwise join the fog after this stop, and the fog would hide the owner again.
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(M_FowParticpationHandle);
	}
	if (GetIsValidFowManager())
	{
		M_FowManager->RemoveFowParticipant(this);
	}
	// Out of the fog nothing keeps the mark up to date; whoever stops the fog decides whether the unit shows.
	bM_OwnerHiddenByFog = false;
	if (AActor* FowOwner = GetOwner())
	{
		FowOwner->Tags.Remove(DeveloperSettings::GamePlay::FoW::TagUnitHiddenByFog);
	}
}

void UFowComp::OnOwnerOwningPlayerChanged(const uint8 OldOwner, const uint8 NewOwner)
{
	// The minimap colour follows the owner.
	UpdateMiniMapIconSettingsFromOwnerRTSComponent();
	// Not started yet: whoever starts the fog picks the behaviour from the owner the unit has by then.
	if (not bM_HasStartedFow)
	{
		return;
	}
	const bool bGivesHumanVision = M_FowBehaviour == EFowBehaviour::Fow_Active;
	if (bGivesHumanVision == FRTSPlayers::IsHumanPlayer(NewOwner))
	{
		return;
	}
	SwitchFowBehaviour(bGivesHumanVision ? EFowBehaviour::Fow_PassiveEnemyVision : EFowBehaviour::Fow_Active);
}

void UFowComp::SwitchFowBehaviour(const EFowBehaviour NewBehaviour)
{
	// A start that is still pending joins the fog later, with the new behaviour.
	const bool bWasParticipating = GetIsValidFowManager() && M_FowManager->GetIsFowParticipant(this);
	if (bWasParticipating)
	{
		M_FowManager->RemoveFowParticipant(this);
	}
	ClearFogStateOnOwner();
	bM_OwnerHiddenByFog = false;
	M_FowBehaviour = NewBehaviour;
	if (bWasParticipating)
	{
		OnFowStartParticipation();
	}
}

void UFowComp::ClearFogStateOnOwner() const
{
	using DeveloperSettings::GamePlay::FoW::TagUnitHiddenByFog;
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	AActor* FowOwner = GetOwner();
	if (not IsValid(FowOwner))
	{
		return;
	}
	FowOwner->Tags.Remove(TagUnitInEnemyVision);
	// Only what the fog hid is shown again; a unit hidden for another reason stays hidden.
	const bool bWasHiddenByFog = FowOwner->Tags.Remove(TagUnitHiddenByFog) > 0;
	if (bWasHiddenByFog && FowOwner->IsHidden())
	{
		FowOwner->SetActorHiddenInGame(false);
	}
}


void UFowComp::BeginPlay()
{
	Super::BeginPlay();

	M_FowManager = FRTS_Statics::GetFowManager(this);
	BeginPlay_InitMiniMapIconSettings();
}

void UFowComp::CacheMiniMapWorldLocation(const FVector& WorldLocation)
{
	M_MiniMapRuntimeState.M_WorldLocation = WorldLocation;
	M_MiniMapRuntimeState.bM_HasCachedWorldLocation = true;
}

void UFowComp::SetShouldDrawMiniMapIcon(const bool bShouldDrawMiniMapIcon)
{
	M_MiniMapRuntimeState.bM_ShouldDrawMiniMapIcon = bShouldDrawMiniMapIcon;
}

void UFowComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	StopFowParticipation();
}


bool UFowComp::GetIsValidFowManager()
{
	if (M_FowManager.IsValid())
	{
		return true;
	}
	M_FowManager = FRTS_Statics::GetFowManager(this);
	return M_FowManager.IsValid();
}

void UFowComp::UpdateMiniMapIconSizeFromFowManager()
{
	if (not GetIsValidFowManager())
	{
		return;
	}

	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	if (bM_ShouldUseTinyMiniMapIcon)
	{
		M_MinimapIconSettings.M_IconSizePixels = M_FowManager->GetMiniMapTinyIconSizePixels();
		return;
	}

	if (M_UnitType == EAllUnitType::UNType_None)
	{
		return;
	}

	M_MinimapIconSettings.M_IconSizePixels = FRTSMinimapIconHelpers::GetMiniMapIconSizePixels(
		*M_FowManager.Get(),
		OwnerActor,
		M_UnitType,
		M_UnitSubtype);
}

void UFowComp::BeginPlay_InitMiniMapIconSettings()
{
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	bM_ShouldUseTinyMiniMapIcon = OwnerActor->IsA(ASquadUnit::StaticClass());
	URTSComponent* const RTSComponent = OwnerActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		UpdateMiniMapIconSizeFromFowManager();
		return;
	}

	UpdateMiniMapIconSettingsFromOwnerRTSComponent();
	RTSComponent->OnSubTypeInitialized.AddUObject(this, &UFowComp::UpdateMiniMapIconSettingsFromOwnerRTSComponent);
	RTSComponent->OnOwningPlayerChanged.AddUObject(this, &UFowComp::OnOwnerOwningPlayerChanged);
}

void UFowComp::UpdateMiniMapIconSettingsFromOwnerRTSComponent()
{
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	URTSComponent* const RTSComponent = OwnerActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	M_UnitType = RTSComponent->GetUnitType();
	M_UnitSubtype = RTSComponent->GetUnitSubType();
	M_MinimapIconSettings.M_IconColor = FRTSMinimapIconHelpers::GetMiniMapIconColor(
		*RTSComponent,
		M_UnitType,
		M_UnitSubtype);
	UpdateMiniMapIconSizeFromFowManager();
}

bool UFowComp::GetShouldDrawMiniMapIcon() const
{
	if (M_MinimapIconSettings.M_IconSizePixels <= 0.0f)
	{
		return false;
	}

	if (not M_MiniMapRuntimeState.bM_HasCachedWorldLocation)
	{
		return false;
	}

	return M_MiniMapRuntimeState.bM_ShouldDrawMiniMapIcon;
}

void UFowComp::OnFowStartParticipation()
{
	if (not IsActive())
	{
		Activate();
	}
	if (GetIsValidFowManager())
	{
		UpdateMiniMapIconSizeFromFowManager();
		AActor* const OwnerActor = GetOwner();
		if (IsValid(OwnerActor))
		{
			CacheMiniMapWorldLocation(OwnerActor->GetActorLocation());
			SetShouldDrawMiniMapIcon(M_FowBehaviour == EFowBehaviour::Fow_Active && not OwnerActor->IsHidden());
		}
		M_FowManager->AddFowParticipant(this);
	}
}
