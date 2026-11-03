// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSRagdollSettings.h"

#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "RTSCorpsePoseLibrary.h"

namespace RTSRagdollDefaults
{
	const TCHAR* const GermanCrewMeshPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Mesh/SK_PostApoSoldier.SK_PostApoSoldier");
	const TCHAR* const SovietCrewMeshPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier.SK_RussianSoldier");
	// The looks of the Scavenger squad soldiers.
	const TCHAR* const GermanCrewMaterialPaths[] = {
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Material/M_ScavengerNoCamo.M_ScavengerNoCamo"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/ScavengerVariations/MI_Inf_Ger_ScavengerVar1.MI_Inf_Ger_ScavengerVar1"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/ScavengerVariations/MI_Inf_Ger_ScavengerVeteran.MI_Inf_Ger_ScavengerVeteran"),
	};
	// Written by the -RagdollPoseBake harness.
	const TCHAR* const GermanCorpsePosesPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/CorpsePoses/DA_CorpsePoses_Scav.DA_CorpsePoses_Scav");
	const TCHAR* const SovietCorpsePosesPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/CorpsePoses/DA_CorpsePoses_Mosin.DA_CorpsePoses_Mosin");
}

URTSRagdollSettings::URTSRagdollSettings()
{
	using namespace RTSRagdollDefaults;
	CategoryName = TEXT("Game");
	GermanCrew.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(GermanCrewMeshPath));
	for (const TCHAR* const MaterialPath : GermanCrewMaterialPaths)
	{
		GermanCrew.Materials.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath)));
	}
	SovietCrew.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SovietCrewMeshPath));
	GermanCorpsePoses = TSoftObjectPtr<URTSCorpsePoseLibrary>(FSoftObjectPath(GermanCorpsePosesPath));
	SovietCorpsePoses = TSoftObjectPtr<URTSCorpsePoseLibrary>(FSoftObjectPath(SovietCorpsePosesPath));

	CookOffDeath.MinBodies = 2;
	CookOffDeath.MaxBodies = 3;
	CookOffDeath.OutwardSpeed = FVector2D(250.0, 500.0);
	CookOffDeath.UpwardSpeed = FVector2D(600.0, 900.0);
	CookOffDeath.MaxSpinDegrees = 360.f;
}
