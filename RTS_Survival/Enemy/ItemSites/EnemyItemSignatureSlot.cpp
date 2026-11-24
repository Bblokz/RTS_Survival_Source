// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyItemSignatureSlot.h"

#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace EnemyItemSignatureSlotConstants
{
	const FColor SlotColor(255, 150, 30);
	constexpr float GroundLiftZ = 20.f;
	constexpr float FacingArrowLength = 600.f;
	constexpr float OutlineBrightness = 0.8f;
}

AEnemyItemSignatureSlot::AEnemyItemSignatureSlot()
{
	FillerContent.bIsSignatureTarget = false;
}

bool AEnemyItemSignatureSlot::TryFillForItem(const EMapEnemyItem ItemType,
                                             AActor*& OutSpawnedContent,
                                             bool& bOutIsSignatureTarget)
{
	OutSpawnedContent = nullptr;
	bOutIsSignatureTarget = false;
	if (bM_IsFilled)
	{
		return false;
	}

	// Marked first: a failed spawn is reported once and the slot counts as used.
	bM_IsFilled = true;
	FEnemyItemSlotContent Content;
	if (not EnemyItemSiteRules::TryResolveSlotContent(ItemType, ContentByItem, FillerContent, ContentSet, Content))
	{
		return false;
	}

	AActor* SpawnedContent = SpawnContent(Content);
	if (not IsValid(SpawnedContent))
	{
		return false;
	}

	M_SpawnedContent = SpawnedContent;
	OutSpawnedContent = SpawnedContent;
	bOutIsSignatureTarget = Content.bIsSignatureTarget;
	return true;
}

AActor* AEnemyItemSignatureSlot::SpawnContent(const FEnemyItemSlotContent& Content) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	UClass* LoadedClass = Content.ActorClass.LoadSynchronous();
	if (not IsValid(LoadedClass))
	{
		RTSFunctionLibrary::ReportError("Signature slot " + GetName() + " could not load "
			+ Content.ActorClass.ToString());
		return nullptr;
	}

	AActor* SpawnedContent = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, LoadedClass, GetActorTransform(), static_cast<uint8>(GetEnemyPlayer()));
	if (not IsValid(SpawnedContent))
	{
		RTSFunctionLibrary::ReportError("Signature slot " + GetName() + " could not spawn " + LoadedClass->GetName());
		return nullptr;
	}

	return SpawnedContent;
}

FColor AEnemyItemSignatureSlot::GetPlacementColor() const
{
	return EnemyItemSignatureSlotConstants::SlotColor;
}

FString AEnemyItemSignatureSlot::GetConfigurationProblem() const
{
	const bool bHasSetContent = IsValid(ContentSet)
		&& (not ContentSet->ContentByItem.IsEmpty() || ContentSet->FillerContent.GetHasActorClass());
	if (ContentByItem.IsEmpty() && not FillerContent.GetHasActorClass() && not bHasSetContent)
	{
		return TEXT("no content for any item");
	}

	return FString();
}

void AEnemyItemSignatureSlot::BuildEditorShapes(FEnemyPlacementShapes& OutSummary,
                                                FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyItemSignatureSlotConstants;
	const FVector Ground = GetActorLocation() + FVector(0.f, 0.f, GroundLiftZ);
	const float Yaw = GetActorRotation().Yaw;
	const FVector HalfExtent(FootprintHalfExtent.X, FootprintHalfExtent.Y, 0.f);
	// Outline only: the content is not there until the director fills the slot.
	OutSummary.AddFlatBox(Ground, HalfExtent, Yaw, EnemyPlacementColors::GetDimmed(SlotColor, OutlineBrightness), false);
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	OutDetail.ArrowLines.Emplace(Ground, Ground + Forward * FacingArrowLength, EnemyPlacementColors::Facing);
}
