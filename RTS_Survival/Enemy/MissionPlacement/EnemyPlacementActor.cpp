// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyPlacementActor.h"

#include "Components/BillboardComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Texture2D.h"
#include "EnemyPlacementShapesComponent.h"
#include "EnemyPlacementTypes.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace EnemyPlacementActorConstants
{
	constexpr float MinReadyDelaySeconds = 0.2f;
	constexpr float ReadyRetryIntervalSeconds = 1.f;
	constexpr int32 MaxReadyRetries = 30;

	constexpr float LabelWorldSize = 260.f;
	constexpr float LabelHeight = 700.f;
	// Lies flat and faces up so it reads from the top-down camera.
	const FRotator LabelRotation(90.f, 180.f, 0.f);
}

AEnemyPlacementActor::AEnemyPlacementActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = M_Root;

#if WITH_EDITORONLY_DATA
	using namespace EnemyPlacementActorConstants;
	M_EditorSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (M_EditorSprite)
	{
		M_EditorSprite->SetupAttachment(M_Root);
		M_EditorSprite->bIsScreenSizeScaled = true;
	}

	M_EditorLabel = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (M_EditorLabel)
	{
		M_EditorLabel->SetupAttachment(M_Root);
		M_EditorLabel->SetRelativeLocation(FVector(0.f, 0.f, LabelHeight));
		M_EditorLabel->SetRelativeRotation(LabelRotation);
		M_EditorLabel->SetHorizontalAlignment(EHTA_Center);
		M_EditorLabel->SetVerticalAlignment(EVRTA_TextCenter);
		M_EditorLabel->SetWorldSize(LabelWorldSize);
		M_EditorLabel->SetHiddenInGame(true);
		M_EditorLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_EditorLabel->SetCastShadow(false);
	}

	M_SummaryShapes = CreateEditorOnlyDefaultSubobject<UEnemyPlacementShapesComponent>(TEXT("SummaryShapes"));
	if (M_SummaryShapes)
	{
		M_SummaryShapes->SetupAttachment(M_Root);
		M_SummaryShapes->SetLayer(EEnemyPlacementShapesLayer::Summary);
	}

	M_DetailShapes = CreateEditorOnlyDefaultSubobject<UEnemyPlacementShapesComponent>(TEXT("DetailShapes"));
	if (M_DetailShapes)
	{
		M_DetailShapes->SetupAttachment(M_Root);
		M_DetailShapes->SetLayer(EEnemyPlacementShapesLayer::Detail);
	}
#endif
}

FColor AEnemyPlacementActor::GetPlacementColor() const
{
	return FColor::White;
}

FString AEnemyPlacementActor::GetConfigurationProblem() const
{
	return FString();
}

void AEnemyPlacementActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshEditorVisuals();
}

void AEnemyPlacementActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelWaitForEnemyAI();
	Super::EndPlay(EndPlayReason);
}

FString AEnemyPlacementActor::GetPlacementKindName() const
{
	return TEXT("ENEMY");
}

FName AEnemyPlacementActor::GetPlacementId() const
{
	return NAME_None;
}

UTexture2D* AEnemyPlacementActor::GetEditorSpriteTexture() const
{
	return nullptr;
}

void AEnemyPlacementActor::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
}

void AEnemyPlacementActor::RefreshEditorVisuals()
{
#if WITH_EDITORONLY_DATA
	if (M_EditorSprite)
	{
		M_EditorSprite->SetSprite(GetEditorSpriteTexture());
	}
	if (M_EditorLabel)
	{
		const bool bHasProblem = not GetConfigurationProblem().IsEmpty();
		M_EditorLabel->SetText(FText::FromString(GetEditorLabelText()));
		M_EditorLabel->SetTextRenderColor(bHasProblem ? EnemyPlacementColors::Problem : GetPlacementColor());
	}
	if (not M_SummaryShapes || not M_DetailShapes)
	{
		return;
	}
	FEnemyPlacementShapes Summary;
	FEnemyPlacementShapes Detail;
	BuildEditorShapes(Summary, Detail);
	M_SummaryShapes->SetShapes(MoveTemp(Summary));
	M_DetailShapes->SetShapes(MoveTemp(Detail));
#endif
}

FString AEnemyPlacementActor::GetEditorLabelText() const
{
	const FName Id = GetPlacementId();
	const FString Name = Id.IsNone() ? FString(TEXT("(no id)")) : Id.ToString();
	FString Label = GetPlacementKindName() + TEXT(" ") + Name;
	const FString Problem = GetConfigurationProblem();
	if (not Problem.IsEmpty())
	{
		Label += TEXT("\n! ") + Problem;
	}
	return Label;
}

TArray<FVector> AEnemyPlacementActor::GetWorldPoints(const TArray<FVector>& LocalPoints) const
{
	TArray<FVector> WorldPoints;
	WorldPoints.Reserve(LocalPoints.Num());
	const FTransform& ActorTransform = GetActorTransform();
	for (const FVector& LocalPoint : LocalPoints)
	{
		WorldPoints.Add(ActorTransform.TransformPosition(LocalPoint));
	}
	return WorldPoints;
}

// ------------------------------------------------------------------
// Waiting for the enemy AI
// ------------------------------------------------------------------

void AEnemyPlacementActor::WaitForEnemyAI(const float DelaySeconds)
{
	M_EnemyAIReadyRetryCount = 0;
	ScheduleEnemyAIReadyCheck(FMath::Max(DelaySeconds, EnemyPlacementActorConstants::MinReadyDelaySeconds));
}

void AEnemyPlacementActor::CancelWaitForEnemyAI()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_EnemyAIReadyTimerHandle);
	}
}

bool AEnemyPlacementActor::GetIsWaitingForEnemyAI() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) && World->GetTimerManager().IsTimerActive(M_EnemyAIReadyTimerHandle);
}

void AEnemyPlacementActor::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
}

void AEnemyPlacementActor::OnEnemyAINeverReady()
{
}

void AEnemyPlacementActor::ScheduleEnemyAIReadyCheck(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyPlacementActor> WeakThis(this);
	World->GetTimerManager().SetTimer(M_EnemyAIReadyTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CheckEnemyAIReady();
		}
	}), DelaySeconds, false);
}

void AEnemyPlacementActor::CheckEnemyAIReady()
{
	using namespace EnemyPlacementActorConstants;
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	const bool bHasDifficulty = EnemyPlacement::TryGetGameDifficulty(this, Difficulty);
	const bool bHasController = IsValid(EnemyController) || not GetNeedsEnemyController();
	if (bHasController && bHasDifficulty)
	{
		OnEnemyAIReady(EnemyController, Difficulty);
		return;
	}
	M_EnemyAIReadyRetryCount++;
	if (M_EnemyAIReadyRetryCount <= MaxReadyRetries)
	{
		ScheduleEnemyAIReadyCheck(ReadyRetryIntervalSeconds);
		return;
	}
	if (not bHasController)
	{
		RTSFunctionLibrary::ReportError(GetPlacementKindName() + TEXT(" ") + GetName() +
			TEXT(" found no enemy controller; it does nothing."));
		OnEnemyAINeverReady();
		return;
	}
	RTSFunctionLibrary::ReportError(GetPlacementKindName() + TEXT(" ") + GetName() +
		TEXT(" found no game difficulty; it acts as on Normal."));
	OnEnemyAIReady(EnemyController, ERTSGameDifficulty::Normal);
}
