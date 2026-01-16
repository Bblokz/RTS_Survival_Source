#include "FNomadicPreviewAttachmentState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"

bool FNomadicPreviewAttachmentState::GetIsValidRadiusSubsystem() const
{
	if (M_RadiusSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("FNomadicPreviewAttachmentState: Radius subsystem is not valid."));
	return false;
}

void FNomadicPreviewAttachmentState::AttachBuildRadiusToPreview(
	AActor* PreviewActor,
	const float Radius,
	const FVector& AttachmentOffset)
{
	// Validate inputs early.
	if (not IsValid(PreviewActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("AttachBuildRadiusToPreview: PreviewActor is invalid."));
		return;
	}
	if (Radius <= 0.0f || FMath::IsNearlyZero(Radius))
	{
		return;
	}

	// Ensure any existing attachment is cleaned up first.
	if (M_RadiusID >= 0 && M_RadiusSubsystem.IsValid())
	{
		M_RadiusSubsystem->HideRTSRadiusById(M_RadiusID);
		M_RadiusID = -1;
	}

	// Resolve subsystem from this actor's world.
	UWorld* World = PreviewActor->GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("AttachBuildRadiusToPreview: World is invalid."));
		return;
	}
	URTSRadiusPoolSubsystem* Subsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>();
	if (not IsValid(Subsystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("AttachBuildRadiusToPreview: URTSRadiusPoolSubsystem not found."));
		return;
	}

	// Create a buildng radius type radius at the actor's current world location (no lifetime for preview).
	const FVector SpawnLocation = PreviewActor->GetActorLocation();
	constexpr ERTSRadiusType RadiusType = ERTSRadiusType::PostConstructionBuildingRangeBorderOnly;

	const int32 NewId = Subsystem->CreateRTSRadius(SpawnLocation, Radius, RadiusType, /*LifeTime*/ 0.0f);
	if (NewId < 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("AttachBuildRadiusToPreview: Failed to create pooled radius."));
		return;
	}

	// Attach to the preview actor with the desired relative offset.
	Subsystem->AttachRTSRadiusToActor(NewId, PreviewActor, AttachmentOffset);

	// Cache state for later removal.
	M_RadiusID = NewId;
	M_RadiusSubsystem = Subsystem;
}

void FNomadicPreviewAttachmentState::RemoveAttachmentRadius()
{
	if (M_RadiusID < 0)
	{
		return; // Nothing to do
	}
	if (not GetIsValidRadiusSubsystem())
	{
		M_RadiusID = -1; // Prevent repeated error spam on subsequent calls.
		return;
	}
	M_RadiusSubsystem->HideRTSRadiusById(M_RadiusID);
	M_RadiusID = -1;
}
