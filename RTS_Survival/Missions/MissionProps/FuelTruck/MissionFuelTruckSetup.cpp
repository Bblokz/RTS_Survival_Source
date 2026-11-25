// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionFuelTruckSetup.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionFuelTruckSetupPrivate
{
	// Name of the ZIL Blueprint's hull (armour) mesh component.
	const FName TruckHullComponentName(TEXT("Hull"));

	TSoftObjectPtr<UMaterialInterface> MakeDecalMaterial(const TCHAR* Path)
	{
		return TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Path));
	}
}

TSoftObjectPtr<UStaticMesh> FMissionFuelTruckSetup::GetDefaultZilHullHitboxMesh()
{
	return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/Meshes/"
		"SM_Hull_Zil_Hitbox.SM_Hull_Zil_Hitbox")));
}

FMissionPropSinkDeathSettings FMissionFuelTruckSetup::MakeDefaultZilWreckDeath()
{
	using MissionFuelTruckSetupPrivate::MakeDecalMaterial;
	FMissionPropSinkDeathSettings WreckDeath;
	WreckDeath.SmokeSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large."
		"NS_DestructionSmoke_Large")));
	// The small NS_DestructionSmoke is gone before the sink ends; the large one, scaled down, lasts.
	WreckDeath.SmokeScale = FVector(0.75f);
	WreckDeath.SinkSeconds = 8.f;
	// The ZIL wreck is under 3 m tall.
	WreckDeath.SinkDepth = 400.f;
	WreckDeath.MaxTiltDegrees = 5.f;

	FMissionPropDeathDecal Scorch;
	Scorch.Materials = {
		MakeDecalMaterial(TEXT("/Game/Crater_Decals/Decals/Scorched/MI_Decal_Scorch_03.MI_Decal_Scorch_03")),
		MakeDecalMaterial(TEXT("/Game/Crater_Decals/Decals/Scorched/MI_Decal_Scorch_04.MI_Decal_Scorch_04"))
	};
	Scorch.Count = 1;
	Scorch.SizeRange = FVector2D(600.f, 700.f);
	WreckDeath.Decals.Add(Scorch);

	// Small glowing ember ("lava") scorches on top of the big one.
	FMissionPropDeathDecal Embers;
	Embers.Materials = {
		MakeDecalMaterial(TEXT(
			"/Game/RTS_Survival/Materials/Decals/Embers/MI_Decal_Crater_Embers_01.MI_Decal_Crater_Embers_01")),
		MakeDecalMaterial(TEXT(
			"/Game/RTS_Survival/Materials/Decals/Embers/MI_Decal_Crater_Embers_02.MI_Decal_Crater_Embers_02")),
		MakeDecalMaterial(TEXT(
			"/Game/RTS_Survival/Materials/Decals/Embers/MI_Decal_Crater_Embers_03.MI_Decal_Crater_Embers_03"))
	};
	Embers.Count = 2;
	Embers.SizeRange = FVector2D(200.f, 280.f);
	Embers.Scatter = 160.f;
	WreckDeath.Decals.Add(Embers);
	return WreckDeath;
}

bool FMissionFuelTruckSetup::ApplyHitboxAndWreckDeath(AActor* Truck, const TSoftObjectPtr<UStaticMesh>& HullHitboxMesh,
                                                      const FMissionPropSinkDeathSettings& WreckDeath,
                                                      TArray<TObjectPtr<UObject>>& InOutPreloadedAssets,
                                                      const FString& ErrorContext)
{
	ATankMaster* const Tank = Cast<ATankMaster>(Truck);
	if (not IsValid(Tank))
	{
		RTSFunctionLibrary::ReportError(ErrorContext + ": the fuel truck is no ATankMaster; it keeps its own hit box "
			"and death.");
		return false;
	}
	if (not HullHitboxMesh.IsNull())
	{
		UStaticMesh* const HitboxMesh = HullHitboxMesh.LoadSynchronous();
		UStaticMeshComponent* Hull = nullptr;
		TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Tank);
		for (UStaticMeshComponent* const MeshComponent : MeshComponents)
		{
			if (IsValid(MeshComponent)
				&& MeshComponent->GetFName() == MissionFuelTruckSetupPrivate::TruckHullComponentName)
			{
				Hull = MeshComponent;
				break;
			}
		}
		if (HitboxMesh == nullptr || Hull == nullptr)
		{
			RTSFunctionLibrary::ReportError(ErrorContext + ": could not swap the fuel truck hull to "
				+ HullHitboxMesh.ToString() + " (mesh or Hull component missing).");
		}
		else
		{
			// Same render mesh and pivot; only the collision differs, so the armour plates still line up.
			Hull->SetStaticMesh(HitboxMesh);
		}
	}
	FMissionPropSinkDeath::PreloadAssets(WreckDeath, InOutPreloadedAssets);
	Tank->SetWreckSpawnedHook([WreckDeath](AActor* Wreck)
	{
		FMissionPropSinkDeath::Start(Wreck, WreckDeath);
	});
	return true;
}
