// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldTrenchAnchorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FieldTrenchAnchorSubsystem.h"
#include "TimerManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

const TCHAR* const UFieldTrenchAnchorComponent::SocketPrefix = TEXT("TrenchPort_");

UFieldTrenchAnchorComponent* FFieldTrenchAnchorHandle::Get() const
{
	UFieldTrenchAnchorComponent* AnchorComponent = Component.Get();
	return IsValid(AnchorComponent) && AnchorIndex >= 0 && AnchorIndex < AnchorComponent->GetAnchorCount()
		? AnchorComponent
		: nullptr;
}

UFieldTrenchAnchorComponent::UFieldTrenchAnchorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFieldTrenchAnchorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IHealthBarOwner* HealthOwner = Cast<IHealthBarOwner>(GetOwner()))
	{
		if (FOnUnitDies* OnUnitDies = HealthOwner->GetOnUnitDiesDelegate())
		{
			OnUnitDies->AddUObject(this, &UFieldTrenchAnchorComponent::OnOwnerDies);
		}
	}
	if (Cast<ABuildingExpansion>(GetOwner()))
	{
		BindToBuildingExpansion();
		return;
	}
	SetAnchorsAvailable(true);
}

void UFieldTrenchAnchorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromBuildingExpansion();
	if (IHealthBarOwner* HealthOwner = Cast<IHealthBarOwner>(GetOwner()))
	{
		if (FOnUnitDies* OnUnitDies = HealthOwner->GetOnUnitDiesDelegate())
		{
			OnUnitDies->RemoveAll(this);
		}
	}
	// Only an owner destroyed in play leaves trenches behind that need closing.
	RemoveAnchors(EndPlayReason == EEndPlayReason::Destroyed, true);
	Super::EndPlay(EndPlayReason);
}

void UFieldTrenchAnchorComponent::BindToBuildingExpansion()
{
	ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(Expansion))
	{
		return;
	}
	Expansion->OnBxpConstructed.AddWeakLambda(this, [this]() { SetAnchorsAvailable(true); });
	Expansion->OnBxpPackingUp.AddWeakLambda(this, [this]() { SetAnchorsAvailable(false); });
	Expansion->OnBxpCancelPackingUp.AddWeakLambda(this, [this]()
	{
		const ABuildingExpansion* PackingExpansion = Cast<ABuildingExpansion>(GetOwner());
		// A cancelled pack-up of an unfinished expansion goes back to construction, without sockets.
		SetAnchorsAvailable(IsValid(PackingExpansion) &&
			PackingExpansion->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_Built);
	});
	if (Expansion->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_Built)
	{
		SetAnchorsAvailable(true);
		return;
	}
	// Owners and the field defense queue take over the expansions they spawn in the frame they spawn them.
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UFieldTrenchAnchorComponent::CheckIfPlacedInLevel);
}

void UFieldTrenchAnchorComponent::CheckIfPlacedInLevel()
{
	const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(GetOwner());
	// Nothing took it over: a level designer placed it, built as it stands.
	if (IsValid(Expansion) && not Expansion->GetIsFieldDefense() &&
		Expansion->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_NotUnlocked &&
		not IsValid(Expansion->GetBuildingExpansionOwner().GetObject()))
	{
		SetAnchorsAvailable(true);
	}
}

void UFieldTrenchAnchorComponent::UnbindFromBuildingExpansion()
{
	ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(Expansion))
	{
		return;
	}
	Expansion->OnBxpConstructed.RemoveAll(this);
	Expansion->OnBxpPackingUp.RemoveAll(this);
	Expansion->OnBxpCancelPackingUp.RemoveAll(this);
}

void UFieldTrenchAnchorComponent::OnOwnerDies()
{
	// The neck and plug stay: they collapse with the rest of the owner.
	RemoveAnchors(true, true);
}

void UFieldTrenchAnchorComponent::SetAnchorsAvailable(const bool bAvailable)
{
	if (bAvailable == bM_AreAnchorsAvailable)
	{
		return;
	}
	if (bAvailable)
	{
		CreateAnchors();
		return;
	}
	RemoveAnchors(true, false);
}

bool UFieldTrenchAnchorComponent::GetIsAnchorFree(const int32 AnchorIndex) const
{
	return bM_AreAnchorsAvailable && M_Anchors.IsValidIndex(AnchorIndex) &&
		not M_Anchors[AnchorIndex].Occupant.IsValid();
}

const FFieldTrenchOpenPort& UFieldTrenchAnchorComponent::GetAnchorPort(const int32 AnchorIndex) const
{
	static const FFieldTrenchOpenPort NoPort;
	return M_Anchors.IsValidIndex(AnchorIndex) ? M_Anchors[AnchorIndex].Port : NoPort;
}

FVector UFieldTrenchAnchorComponent::GetAnchorLocation(const int32 AnchorIndex) const
{
	return M_Anchors.IsValidIndex(AnchorIndex)
		? FVector(M_Anchors[AnchorIndex].Port.Location, M_Anchors[AnchorIndex].Height)
		: FVector::ZeroVector;
}

FName UFieldTrenchAnchorComponent::GetAnchorSocketName(const int32 AnchorIndex) const
{
	return M_Anchors.IsValidIndex(AnchorIndex) ? M_Anchors[AnchorIndex].SocketName : NAME_None;
}

bool UFieldTrenchAnchorComponent::GetIsAnchorJoined(const int32 AnchorIndex) const
{
	return M_Anchors.IsValidIndex(AnchorIndex) && M_Anchors[AnchorIndex].bIsJoined;
}

bool UFieldTrenchAnchorComponent::TryClaimAnchor(const int32 AnchorIndex, ARTSFieldTrench& Trench)
{
	if (not M_Anchors.IsValidIndex(AnchorIndex))
	{
		return false;
	}
	FFieldTrenchAnchorRuntime& Anchor = M_Anchors[AnchorIndex];
	if (Anchor.Occupant.Get() == &Trench)
	{
		return true;
	}
	if (not GetIsAnchorFree(AnchorIndex))
	{
		return false;
	}
	Anchor.Occupant = &Trench;
	return true;
}

void UFieldTrenchAnchorComponent::ReleaseAnchor(const int32 AnchorIndex, const ARTSFieldTrench& Trench)
{
	if (not M_Anchors.IsValidIndex(AnchorIndex) || M_Anchors[AnchorIndex].Occupant.Get() != &Trench)
	{
		return;
	}
	FFieldTrenchAnchorRuntime& Anchor = M_Anchors[AnchorIndex];
	Anchor.Occupant.Reset();
	Anchor.bIsJoined = false;
	ShowAnchorState(Anchor);
}

void UFieldTrenchAnchorComponent::SetAnchorJoined(const int32 AnchorIndex, const bool bIsJoined)
{
	if (not M_Anchors.IsValidIndex(AnchorIndex))
	{
		return;
	}
	FFieldTrenchAnchorRuntime& Anchor = M_Anchors[AnchorIndex];
	Anchor.bIsJoined = bIsJoined;
	ShowAnchorState(Anchor);
}

void UFieldTrenchAnchorComponent::CreateAnchors()
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (not IsValid(Owner) || not IsValid(World))
	{
		return;
	}
	const URTSComponent* RTSComponent = Owner->FindComponentByClass<URTSComponent>();
	M_OwningPlayer = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : INDEX_NONE;

	TArray<UStaticMeshComponent*> MeshComponents;
	Owner->GetComponents(MeshComponents);
	M_Anchors.Reset();
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		// Necks and plugs of an earlier round are static meshes of the owner too, without sockets of their own.
		if (not IsValid(MeshComponent) || not IsValid(MeshComponent->GetStaticMesh()))
		{
			continue;
		}
		for (const FName& SocketName : MeshComponent->GetAllSocketNames())
		{
			if (not SocketName.ToString().StartsWith(SocketPrefix))
			{
				continue;
			}
			FFieldTrenchAnchorRuntime& Anchor = M_Anchors.AddDefaulted_GetRef();
			Anchor.SocketName = SocketName;
			Anchor.SocketMesh = MeshComponent;
			// The owner stands still while its anchors are available, so the port is read once.
			const FTransform SocketTransform = MeshComponent->GetSocketTransform(SocketName, RTS_World);
			const FVector Forward = SocketTransform.GetRotation().GetForwardVector();
			Anchor.Port.Location = FVector2D(SocketTransform.GetLocation());
			Anchor.Port.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
			Anchor.Height = SocketTransform.GetLocation().Z;
			if (const FFieldTrenchAnchorDefinition* Definition = FindDefinition(SocketName))
			{
				Anchor.NeckComponent = CreateSocketVisual(*MeshComponent, SocketName, Definition->NeckMesh);
				Anchor.PlugComponent = CreateSocketVisual(*MeshComponent, SocketName, Definition->PlugMesh);
			}
			ShowAnchorState(Anchor);
		}
	}
	for (const FFieldTrenchAnchorDefinition& Definition : Anchors)
	{
		if (not M_Anchors.ContainsByPredicate([&Definition](const FFieldTrenchAnchorRuntime& Anchor)
		{
			return Anchor.SocketName == Definition.SocketName;
		}))
		{
			RTSFunctionLibrary::ReportError("Trench anchors of " + Owner->GetName() + ": no mesh has the socket " +
				Definition.SocketName.ToString() + ".");
		}
	}
	bM_AreAnchorsAvailable = true;
	if (UFieldTrenchAnchorSubsystem* AnchorSubsystem = World->GetSubsystem<UFieldTrenchAnchorSubsystem>())
	{
		AnchorSubsystem->RegisterAnchors(*this);
	}
}

void UFieldTrenchAnchorComponent::RemoveAnchors(const bool bCapTrenches, const bool bKeepVisuals)
{
	if (not bM_AreAnchorsAvailable)
	{
		return;
	}
	bM_AreAnchorsAvailable = false;
	if (UWorld* World = GetWorld())
	{
		if (UFieldTrenchAnchorSubsystem* AnchorSubsystem = World->GetSubsystem<UFieldTrenchAnchorSubsystem>())
		{
			AnchorSubsystem->UnregisterAnchors(*this);
		}
	}
	for (int32 AnchorIndex = 0; AnchorIndex < M_Anchors.Num(); ++AnchorIndex)
	{
		FFieldTrenchAnchorRuntime& Anchor = M_Anchors[AnchorIndex];
		ARTSFieldTrench* Occupant = Anchor.Occupant.Get();
		Anchor.Occupant.Reset();
		if (bCapTrenches && IsValid(Occupant))
		{
			Occupant->OnStartAnchorLost(Anchor.Port, Anchor.Height);
		}
		if (bKeepVisuals)
		{
			continue;
		}
		for (UStaticMeshComponent* Visual : {Anchor.NeckComponent.Get(), Anchor.PlugComponent.Get()})
		{
			if (IsValid(Visual))
			{
				Visual->DestroyComponent();
			}
		}
	}
	if (not bKeepVisuals)
	{
		M_Anchors.Reset();
	}
}

UStaticMeshComponent* UFieldTrenchAnchorComponent::CreateSocketVisual(
	UStaticMeshComponent& SocketMesh,
	const FName SocketName,
	const TSoftObjectPtr<UStaticMesh>& Mesh) const
{
	if (Mesh.IsNull())
	{
		return nullptr;
	}
	UStaticMesh* LoadedMesh = Mesh.LoadSynchronous();
	if (not IsValid(LoadedMesh))
	{
		RTSFunctionLibrary::ReportError("Trench anchors of " + GetOwner()->GetName() + ": could not load " +
			Mesh.ToString() + " for socket " + SocketName.ToString() + ".");
		return nullptr;
	}
	UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(GetOwner());
	Visual->SetStaticMesh(LoadedMesh);
	Visual->SetMobility(SocketMesh.Mobility);
	// Part of the owner's walls: it blocks what they block, but leaves the navigation mesh alone.
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetCollisionObjectType(SocketMesh.GetCollisionObjectType());
	Visual->SetCollisionResponseToChannels(SocketMesh.GetCollisionResponseToChannels());
	Visual->SetGenerateOverlapEvents(false);
	// The meshes have their origin on the socket with +X pointing out, so no offset is needed.
	Visual->SetupAttachment(&SocketMesh, SocketName);
	Visual->RegisterComponent();
	GetOwner()->AddInstanceComponent(Visual);
	return Visual;
}

void UFieldTrenchAnchorComponent::ShowAnchorState(FFieldTrenchAnchorRuntime& Anchor) const
{
	const ECollisionEnabled::Type WallCollision = IsValid(Anchor.SocketMesh)
		? Anchor.SocketMesh->GetCollisionEnabled()
		: ECollisionEnabled::NoCollision;
	auto ShowVisual = [WallCollision](UStaticMeshComponent* Visual, const bool bShow)
	{
		if (not IsValid(Visual))
		{
			return;
		}
		Visual->SetVisibility(bShow);
		Visual->SetCollisionEnabled(bShow ? WallCollision : ECollisionEnabled::NoCollision);
	};
	ShowVisual(Anchor.NeckComponent, Anchor.bIsJoined);
	ShowVisual(Anchor.PlugComponent, not Anchor.bIsJoined);
}

const FFieldTrenchAnchorDefinition* UFieldTrenchAnchorComponent::FindDefinition(const FName SocketName) const
{
	return Anchors.FindByPredicate([SocketName](const FFieldTrenchAnchorDefinition& Definition)
	{
		return Definition.SocketName == SocketName;
	});
}
