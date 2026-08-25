#pragma once

#include "CoreMinimal.h"
#include "NavModifierComponent.h"
#include "RTSSplineNav.generated.h"

class USplineComponent;

/**
 * @brief Applies a navigation area through closely fitted boxes along the owner's first spline.
 * Add it beside a fully configured spline; runtime nav export discovers the spline automatically.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSSplineNav : public UNavModifierComponent
{
	GENERATED_BODY()

public:
	URTSSplineNav();

	/** Local half extent around the spline; X adds longitudinal overlap between sampled boxes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation|Spline", meta=(ClampMin="0.0", UIMin="0.0"))
	FVector SplineHalfExtent = FVector(50.0, 100.0, 100.0);

	/**
	 * Per-axis scale applied in the spline's local frame. A Z value of 2 doubles vertical reach.
	 * Spline component scale and spline point scale are applied independently as well.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation|Spline", meta=(ClampMin="0.01", UIMin="0.01"))
	FVector ModifierScale = FVector::OneVector;

	/** Maximum world-space length of a box before it is subdivided. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation|Spline|Fidelity",
		meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float MaxSegmentLength = 100.0f;

	/** Maximum allowed spline-to-chord deviation before a box is subdivided. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation|Spline|Fidelity",
		meta=(ClampMin="0.1", UIMin="0.1", Units="cm"))
	float MaxChordError = 5.0f;

	/** Maximum tangent or up-vector change across a box before it is subdivided. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation|Spline|Fidelity",
		meta=(ClampMin="0.1", UIMin="0.1", ClampMax="90.0", UIMax="90.0", Units="deg"))
	float MaxSegmentAngle = 5.0f;

	/** Safety cap preventing malformed or extremely long splines from creating unbounded nav data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation|Spline|Fidelity", AdvancedDisplay,
		meta=(ClampMin="1", UIMin="1", ClampMax="32768", UIMax="32768"))
	int32 MaxModifierBoxes = 8192;

	/** Rebuilds this component's navigation data after a spline is changed at runtime. */
	UFUNCTION(BlueprintCallable, Category="Navigation|Spline")
	void RefreshSplineNavigation();

protected:
	virtual void CalculateBounds() const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	const USplineComponent* FindFirstSplineComponent() const;
	void BuildSplineModifierBounds(const USplineComponent& SplineComponent) const;

	/**
	 * @brief Keeps each cached nav box aligned with the spline while covering its sampled chord.
	 * @param SplineComponent Source spline owned by this component's actor.
	 * @param StartDistance Start distance of the sampled spline range.
	 * @param EndDistance End distance of the sampled spline range.
	 */
	void AddSplineSegmentModifier(
		const USplineComponent& SplineComponent,
		float StartDistance,
		float EndDistance) const;
	void AddSplinePointModifier(const USplineComponent& SplineComponent) const;

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* ChangedObject, FPropertyChangedEvent&);

	FDelegateHandle M_ObjectPropertyChangedHandle;
#endif
};
