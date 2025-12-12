#include "RTSNavCollision.h"
#include "AI/NavigationSystemBase.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool FRTSNavCollisionSettings::IsValidNavFilters()
{
	if (IsValid(EnemyNavigationFilter) && IsValid(PlayerNavigationFilter))
	{
		return true;
	}
	return false;
}

URTSNavCollision::URTSNavCollision()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	AreaClass = UNavArea_Null::StaticClass();
}

void URTSNavCollision::EnableAffectNavmesh(bool bEnable)
{
	SetNavigationRelevancy(bEnable);

	// Force refresh of modifiers so the navmesh updates immediately
	RefreshNavigationModifiers();

	// Extra safety: request nav system to rebuild octree for owner
	RequestNavMeshRefresh();
}

void URTSNavCollision::BeginPlay()
{
	Super::BeginPlay();
	// Depending on owning player set the right navigation query filter.
	BeginPlay_SetupNavigationFilters();
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "OwnerActor", "RequestNavMeshRefresh");
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("World invalid in RequestNavMeshRefresh\nComponent: " + GetName());
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (not IsValid(NavSys))
	{
		RTSFunctionLibrary::ReportError("NavigationSystem invalid in RequestNavMeshRefresh\nComponent: " + GetName());
		return;
	}
	M_NavSystem = NavSys;
}

void URTSNavCollision::RequestNavMeshRefresh()
{
	if (not EnsureIsValidNavSystem())
	{
		return;
	}
	// If no existing handle, we can mark the area as dirty so nav rebuild includes it
	const FBox NavBounds = GetNavigationBounds();
	M_NavSystem->AddDirtyArea(Bounds, ENavigationDirtyFlag::All);
}

bool URTSNavCollision::EnsureIsValidNavSystem() const
{
	if (M_NavSystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_NavSystem"),
	                                                      TEXT("EnsureIsValidNavSystem"));
	return false;
}

void URTSNavCollision::BeginPlay_SetupNavigationFilters()
{
	if (not GetOwner() || not NavCollisionSettings.IsValidNavFilters())
	{
		return;
	}
	// Get rts component by class from owner actor
	URTSComponent* RTSComp = Cast<URTSComponent>(GetOwner()->GetComponentByClass(URTSComponent::StaticClass()));
	if (not IsValid(RTSComp))
	{
		return;
	}
	if (RTSComp->GetOwningPlayer() == 1)
	{
		AreaClass = NavCollisionSettings.PlayerNavigationFilter;
	}
	else
	{
		AreaClass = NavCollisionSettings.EnemyNavigationFilter;
	}
}
