// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshSocket.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BXPConstructionRules/BXPConstructionRules.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#include "BxpConstructionData.generated.h"

/**
 * Runtime construction data used by the preview/placement flow.
 * Now carries the full FBxpConstructionRules (including optional explicit footprint).
 */
USTRUCT()
struct FBxpConstructionData
{
	GENERATED_BODY()

public:
	FBxpConstructionData() = default;

	FBxpConstructionData(const FBxpConstructionData& Other)
		: M_OwnerAsActor(Other.M_OwnerAsActor)
		  , M_OwnerAsBxpOwner(Other.M_OwnerAsBxpOwner)
		  , M_Rules(Other.M_Rules)
		  , M_ConstructionType(Other.M_ConstructionType)
		  , M_AttachToMeshComp(Other.M_AttachToMeshComp)
		  , bM_UseCollision(Other.bM_UseCollision)
		  , M_SocketList(Other.M_SocketList)
		  , M_BxpBuildRadius(Other.M_BxpBuildRadius)
	{
	}

	FBxpConstructionData(FBxpConstructionData&& Other) noexcept
		: M_OwnerAsActor(Other.M_OwnerAsActor)
		  , M_OwnerAsBxpOwner(Other.M_OwnerAsBxpOwner)
		  , M_Rules(MoveTemp(Other.M_Rules))
		  , M_ConstructionType(Other.M_ConstructionType)
		  , M_AttachToMeshComp(Other.M_AttachToMeshComp)
		  , bM_UseCollision(Other.bM_UseCollision)
		  , M_SocketList(MoveTemp(Other.M_SocketList))
		  , M_BxpBuildRadius(Other.M_BxpBuildRadius)
	{
	}


	FBxpConstructionData& operator=(const FBxpConstructionData& Other)
	{
		if (this != &Other)
		{
			M_Rules = Other.M_Rules;
			M_ConstructionType = Other.M_ConstructionType;
			M_AttachToMeshComp = Other.M_AttachToMeshComp;
			bM_UseCollision = Other.bM_UseCollision;
			M_SocketList = Other.M_SocketList;
			M_BxpBuildRadius = Other.M_BxpBuildRadius;
			M_OwnerAsActor = Other.M_OwnerAsActor;
			M_OwnerAsBxpOwner = Other.M_OwnerAsBxpOwner;
		}
		return *this;
	}

	FBxpConstructionData& operator=(FBxpConstructionData&& Other) noexcept
	{
		if (this != &Other)
		{
			M_Rules = MoveTemp(Other.M_Rules);
			M_ConstructionType = Other.M_ConstructionType;
			M_AttachToMeshComp = Other.M_AttachToMeshComp;
			bM_UseCollision = Other.bM_UseCollision;
			M_SocketList = MoveTemp(Other.M_SocketList);
			M_BxpBuildRadius = Other.M_BxpBuildRadius;
			M_OwnerAsActor = Other.M_OwnerAsActor;
			M_OwnerAsBxpOwner = Other.M_OwnerAsBxpOwner;
		}
		return *this;
	}

	/**
	 * NEW: Initialize from full Bxp rules (includes optional explicit footprint).
	 * - Will never fail due to missing footprint; it logs and falls back to projection.
	 * - Performs the same per-type validations as the legacy Init.
	 */
	bool Init(
		const FBxpConstructionRules& Rules,
		UStaticMeshComponent* AttachToMeshComp = nullptr,
		const float NewBxpBuildRadius = 0.0f,
		const TScriptInterface<IBuildingExpansionOwner>& OwnerAsBxpOwner = nullptr,
		const int32 ExpansionSlotIndex = 0)
	{
		M_Rules = Rules;
		M_ConstructionType = Rules.ConstructionType;
		M_AttachToMeshComp = AttachToMeshComp;
		bM_UseCollision = Rules.bUseCollision; // uses the flag inside rules
		M_BxpBuildRadius = NewBxpBuildRadius;
		M_OwnerAsBxpOwner = OwnerAsBxpOwner;

		if (M_AttachToMeshComp)
		{
			M_OwnerAsActor = M_AttachToMeshComp->GetOwner();
		}

		// Footprint: NEVER fail for unset; just inform and use projection in preview.
		if (!Rules.Footprint.IsSet())
		{
			RTSFunctionLibrary::PrintString(
				TEXT("BXP rules have no explicit footprint set; preview will use mesh projection."));
		}

		// Common validation follows legacy behavior.
		if (M_ConstructionType == EBxpConstructionType::None)
		{
			RTSFunctionLibrary::ReportError(TEXT("Attempted to Init FBxpConstructionData with ConstructionType=None!"));
			return false;
		}

		if (!IsValid(M_OwnerAsActor) || !M_OwnerAsBxpOwner)
		{
			const FString OwnerAsActorName = IsValid(M_OwnerAsActor) ? M_OwnerAsActor->GetName() : TEXT("nullptr");
			const FString OwnerAsBxpOwnerName = M_OwnerAsBxpOwner ? M_OwnerAsBxpOwner->GetOwnerName() : TEXT("nullptr");
			const FString MeshComponentName = IsValid(M_AttachToMeshComp)
				                                  ? M_AttachToMeshComp->GetName()
				                                  : TEXT("nullptr");
			RTSFunctionLibrary::ReportError(
				TEXT("Attempted to init FBxpConstructionData without a valid owner actor or bxp owner!")
				TEXT("\n OwnerAsActor: ") + OwnerAsActorName +
				TEXT("\n OwnerAsBxpOwner: ") + OwnerAsBxpOwnerName +
				TEXT("\n Note: OwnerAsActor is taken from the AttachToMesh's Owner.")
				TEXT("\n MeshComponent: ") + MeshComponentName);
		}

		switch (M_ConstructionType)
		{
		case EBxpConstructionType::Socket:
			{
				if (!IsValid(M_AttachToMeshComp))
				{
					RTSFunctionLibrary::ReportError(TEXT("Socket placement requires a valid AttachToMesh component."));
					return false;
				}
				if (bM_UseCollision && !IsValid(M_OwnerAsActor))
				{
					RTSFunctionLibrary::ReportError(
						TEXT(
							"Socket placement with collision requires a valid owning actor to be excluded from overlaps."));
					return false;
				}
				M_SocketList = M_OwnerAsBxpOwner
					               ? M_OwnerAsBxpOwner->GetFreeSocketList(ExpansionSlotIndex)
					               : TArray<UStaticMeshSocket*>();
				if (M_SocketList.Num() == 0)
				{
					RTSFunctionLibrary::ReportError(
						TEXT("Socket placement requested but no free sockets found on the attach mesh!")
						TEXT("\n Socket needs to contain: ") +
						DeveloperSettings::GamePlay::Construction::BxpSocketTrigger);
					return false;
				}
				break;
			}

		case EBxpConstructionType::AtBuildingOrigin:
			{
				if (!IsValid(M_AttachToMeshComp))
				{
					RTSFunctionLibrary::ReportError(TEXT("Origin placement requires a valid AttachToMesh component."));
					return false;
				}
				if (bM_UseCollision)
				{
					RTSFunctionLibrary::ReportError(
						TEXT("Origin placement ignores collision; rules.bUseCollision should be false for this type."));
				}
				break;
			}

		case EBxpConstructionType::Free:
			{
				if (NewBxpBuildRadius <= 0.0f)
				{
					RTSFunctionLibrary::ReportError(
						TEXT("Free placement requires a positive BuildRadius (NewBxpBuildRadius > 0)."));
					return false;
				}
				if (!bM_UseCollision)
				{
					RTSFunctionLibrary::ReportError(
						TEXT(
							"WARNING: Free placement without collision means the BXP can be placed anywhere in the build radius."));
				}
				break;
			}

		default:
			break;
		}

		return true;
	}

	/**
	 * Legacy Init (kept for backward compatibility).
	 * If you use this overload, no explicit footprint is provided; we log that projection will be used.
	 */
	bool Init(
		const EBxpConstructionType ConstructionType,
		UStaticMeshComponent* AttachToMeshComp = nullptr,
		const bool bUseCollision = true,
		const float NewBxpBuildRadius = 0.0f,
		const TScriptInterface<IBuildingExpansionOwner>& OwnerAsBxpOwner = nullptr,
		const int32 ExpansionSlotIndex = 0)
	{
		// Synthesize rules from the legacy params; no explicit footprint here.
		M_Rules.ConstructionType = ConstructionType;
		M_Rules.bUseCollision = bUseCollision;
		M_Rules.Footprint = FBuildingFootprint{}; // (0,0) => not set

		// Inform (do NOT fail) that we'll use mesh projection.
		RTSFunctionLibrary::PrintString(
			TEXT("Legacy BXP Init used without explicit footprint; preview will use mesh projection."));

		M_ConstructionType = ConstructionType;
		M_AttachToMeshComp = AttachToMeshComp;
		bM_UseCollision = bUseCollision;
		M_BxpBuildRadius = NewBxpBuildRadius;
		M_OwnerAsBxpOwner = OwnerAsBxpOwner;

		if (M_AttachToMeshComp)
		{
			M_OwnerAsActor = M_AttachToMeshComp->GetOwner();
		}

		if (ConstructionType == EBxpConstructionType::None)
		{
			RTSFunctionLibrary::ReportError(TEXT("Attempted to Init FBxpConstructionData with ConstructionType=None!"));
			return false;
		}

		if (!IsValid(M_OwnerAsActor) || !M_OwnerAsBxpOwner)
		{
			const FString OwnerAsActorName = IsValid(M_OwnerAsActor) ? M_OwnerAsActor->GetName() : TEXT("nullptr");
			const FString OwnerAsBxpOwnerName = M_OwnerAsBxpOwner ? M_OwnerAsBxpOwner->GetOwnerName() : TEXT("nullptr");
			const FString MeshComponentName = IsValid(M_AttachToMeshComp)
				                                  ? M_AttachToMeshComp->GetName()
				                                  : TEXT("nullptr");
			RTSFunctionLibrary::ReportError(
				TEXT("Attempted to init FBxpConstructionData without a valid owner actor or bxp owner!")
				TEXT("\n OwnerAsActor: ") + OwnerAsActorName +
				TEXT("\n OwnerAsBxpOwner: ") + OwnerAsBxpOwnerName +
				TEXT("\n Note: OwnerAsActor is taken from the AttachToMesh's Owner.")
				TEXT("\n MeshComponent: ") + MeshComponentName);
		}

		if (ConstructionType == EBxpConstructionType::Socket)
		{
			if (!IsValid(AttachToMeshComp))
			{
				RTSFunctionLibrary::ReportError(TEXT("Socket placement requires a valid AttachToMesh component."));
				return false;
			}
			if (bUseCollision && !IsValid(M_OwnerAsActor))
			{
				RTSFunctionLibrary::ReportError(
					TEXT(
						"Socket placement with collision requires a valid owning actor to be excluded from overlaps."));
				return false;
			}
			M_SocketList = OwnerAsBxpOwner
				               ? OwnerAsBxpOwner->GetFreeSocketList(ExpansionSlotIndex)
				               : TArray<UStaticMeshSocket*>();
			if (M_SocketList.Num() == 0)
			{
				RTSFunctionLibrary::ReportError(
					TEXT("Socket placement requested but no free sockets found on the attach mesh!")
					TEXT("\n Socket needs to contain: ") + DeveloperSettings::GamePlay::Construction::BxpSocketTrigger);
				return false;
			}
			return true;
		}

		if (ConstructionType == EBxpConstructionType::AtBuildingOrigin)
		{
			if (!IsValid(AttachToMeshComp))
			{
				RTSFunctionLibrary::ReportError(TEXT("Origin placement requires a valid AttachToMesh component."));
				return false;
			}
			if (bUseCollision)
			{
				RTSFunctionLibrary::ReportError(
					TEXT("Origin placement ignores collision; bUseCollision should be false for this type."));
			}
		}

		if (ConstructionType == EBxpConstructionType::Free)
		{
			if (NewBxpBuildRadius <= 0.0f)
			{
				RTSFunctionLibrary::ReportError(
					TEXT("Free placement requires a positive BuildRadius (NewBxpBuildRadius > 0)."));
				return false;
			}
			if (!bUseCollision)
			{
				RTSFunctionLibrary::ReportError(
					TEXT(
						"WARNING: Free placement without collision means the BXP can be placed anywhere in the build radius."));
			}
		}

		return true;
	}

	// ------------ Accessors ------------

	EBxpConstructionType GetConstructionType() const { return M_ConstructionType; }
	UStaticMeshComponent* GetAttachToMesh() const { return M_AttachToMeshComp; }
	bool GetUseCollision() const { return bM_UseCollision; }
	TArray<UStaticMeshSocket*> GetSocketList() const { return M_SocketList; }
	float GetBxpBuildRadius() const { return M_BxpBuildRadius; }
	AActor* GetBxpOwnerActor() const { return M_OwnerAsActor; }

	//  full rules (includes optional explicit footprint)
	const FBxpConstructionRules& GetConstructionRules() const { return M_Rules; }

	//  convenience footprint access
	const FBuildingFootprint& GetFootprint() const { return M_Rules.Footprint; }
	bool HasExplicitFootprint() const { return M_Rules.Footprint.IsSet(); }

private:
	// The owning actor (used to exclude from overlaps when placing with collision).
	UPROPERTY()
	AActor* M_OwnerAsActor = nullptr;

	// The BXP owner interface (provides sockets, etc.)
	TScriptInterface<IBuildingExpansionOwner> M_OwnerAsBxpOwner = nullptr;

	//  the full construction rules (construction type, collision, optional footprint, etc.)
	UPROPERTY()
	FBxpConstructionRules M_Rules;

	// Cached type for convenience (mirrors M_Rules.ConstructionType).
	UPROPERTY()
	EBxpConstructionType M_ConstructionType = EBxpConstructionType::None;

	// For Bxps that use sockets or attach directly at the origin location.
	UPROPERTY()
	UStaticMeshComponent* M_AttachToMeshComp = nullptr;

	// Cached collision flag (mirrors M_Rules.bUseCollision).
	UPROPERTY()
	bool bM_UseCollision = true;

	// Sockets available for socket-placed Bxps.
	UPROPERTY()
	TArray<UStaticMeshSocket*> M_SocketList = {};

	// Free-placement radius (if applicable).
	UPROPERTY()
	float M_BxpBuildRadius = 0.0f;
};
