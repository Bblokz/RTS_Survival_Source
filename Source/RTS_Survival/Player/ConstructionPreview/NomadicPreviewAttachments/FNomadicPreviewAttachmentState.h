#pragma once
#include "CoreMinimal.h"
#include "FNomadicPreviewAttachmentState.generated.h"

class URTSRadiusPoolSubsystem;

/**
 * @brief Lightweight helper that spawns a pooled radius, attaches it to a preview actor,
 *        and later hides it again via the pool subsystem.
 */
USTRUCT()
struct FNomadicPreviewAttachmentState
{
	GENERATED_BODY()

	/**
	 * @brief Creates (or reuses) a pooled radius and attaches it to the preview actor.
	 *        If a previous radius was active, it is hidden first.
	 * @param PreviewActor      Actor to attach the radius actor to.
	 * @param Radius            World radius to render.
	 * @param AttachmentOffset  Relative offset to apply while attached.
	 */
	void AttachBuildRadiusToPreview(AActor* PreviewActor, float Radius, const FVector& AttachmentOffset);

	/**
	 * @brief Remove the last created radius if the ID is valid, then reset the ID.
	 *        No-op if nothing is active.
	 */
	void RemoveAttachmentRadius();

private:
	/** Runtime id returned by the pool; -1 means inactive. */
	int32 M_RadiusID = -1;

	/** Cached subsystem used to hide the id later (set on attach). */
	UPROPERTY()
	TWeakObjectPtr<URTSRadiusPoolSubsystem> M_RadiusSubsystem;

	/** Helper per rule 0.5: validity + centralized error reporting. */
	bool GetIsValidRadiusSubsystem() const;
};
