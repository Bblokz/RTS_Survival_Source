#include "RTSSplineNav.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#if WITH_EDITOR
#include "UObject/UObjectGlobals.h"
#endif

namespace RTSSplineNav::Private
{
	constexpr float MinimumBoxExtent = 1.0f;
	constexpr float MinimumChordError = 0.1f;
	constexpr float MinimumSegmentAngle = 0.1f;
	constexpr float MaximumSegmentAngle = 90.0f;
	constexpr int32 MaximumSubdivisionDepth = 12;
	constexpr int32 MaximumModifierBoxes = 32768;

	struct FSplineSample
	{
		FVector Location = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		FVector UpVector = FVector::UpVector;
	};

	FSplineSample SampleSpline(const USplineComponent& SplineComponent, const float Distance)
	{
		FSplineSample Sample;
		Sample.Location = SplineComponent.GetLocationAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::World);
		Sample.Direction = SplineComponent.GetDirectionAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::World).GetSafeNormal();
		Sample.UpVector = SplineComponent.GetUpVectorAtDistanceAlongSpline(
			Distance,
			ESplineCoordinateSpace::World).GetSafeNormal();
		return Sample;
	}

	float GetAngleDegrees(const FVector& FirstVector, const FVector& SecondVector)
	{
		const float DotProduct = FVector::DotProduct(FirstVector, SecondVector);
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));
	}

	bool GetNeedsSubdivision(
		const FSplineSample& StartSample,
		const FSplineSample& MiddleSample,
		const FSplineSample& EndSample,
		const float MaxChordError,
		const float MaxSegmentAngle)
	{
		const FVector ChordMiddle = (StartSample.Location + EndSample.Location) * 0.5;
		if (FVector::Dist(MiddleSample.Location, ChordMiddle) > MaxChordError)
		{
			return true;
		}

		const float DirectionAngle = FMath::Max3(
			GetAngleDegrees(StartSample.Direction, MiddleSample.Direction),
			GetAngleDegrees(MiddleSample.Direction, EndSample.Direction),
			GetAngleDegrees(StartSample.Direction, EndSample.Direction));
		const float UpVectorAngle = FMath::Max3(
			GetAngleDegrees(StartSample.UpVector, MiddleSample.UpVector),
			GetAngleDegrees(MiddleSample.UpVector, EndSample.UpVector),
			GetAngleDegrees(StartSample.UpVector, EndSample.UpVector));
		return DirectionAngle > MaxSegmentAngle || UpVectorAngle > MaxSegmentAngle;
	}

	void AppendAdaptiveDistanceRange(
		const USplineComponent& SplineComponent,
		const float StartDistance,
		const float EndDistance,
		const float MaxChordError,
		const float MaxSegmentAngle,
		const int32 SubdivisionDepth,
		int32& RemainingSubdivisions,
		TArray<float>& OutSampleDistances)
	{
		const float MiddleDistance = (StartDistance + EndDistance) * 0.5f;
		const FSplineSample StartSample = SampleSpline(SplineComponent, StartDistance);
		const FSplineSample MiddleSample = SampleSpline(SplineComponent, MiddleDistance);
		const FSplineSample EndSample = SampleSpline(SplineComponent, EndDistance);

		const bool bCanSubdivide = RemainingSubdivisions > 0
			&& SubdivisionDepth < MaximumSubdivisionDepth;
		if (bCanSubdivide && GetNeedsSubdivision(
			StartSample,
			MiddleSample,
			EndSample,
			MaxChordError,
			MaxSegmentAngle))
		{
			--RemainingSubdivisions;
			AppendAdaptiveDistanceRange(
				SplineComponent,
				StartDistance,
				MiddleDistance,
				MaxChordError,
				MaxSegmentAngle,
				SubdivisionDepth + 1,
				RemainingSubdivisions,
				OutSampleDistances);
			AppendAdaptiveDistanceRange(
				SplineComponent,
				MiddleDistance,
				EndDistance,
				MaxChordError,
				MaxSegmentAngle,
				SubdivisionDepth + 1,
				RemainingSubdivisions,
				OutSampleDistances);
			return;
		}

		OutSampleDistances.Add(EndDistance);
	}

	TArray<float> BuildSampleDistances(
		const USplineComponent& SplineComponent,
		const float SplineLength,
		const float MaxSegmentLength,
		const float MaxChordError,
		const float MaxSegmentAngle,
		const int32 MaxModifierBoxes)
	{
		const float MaximumComponentScale = SplineComponent.GetComponentScale().GetAbs().GetMax();
		const double EstimatedWorldLength = static_cast<double>(SplineLength)
			* FMath::Max(MaximumComponentScale, UE_SMALL_NUMBER);
		const double EstimatedSegmentCount = EstimatedWorldLength / MaxSegmentLength;
		const bool bRequiresMaximumSegmentCount = not FMath::IsFinite(EstimatedSegmentCount)
			|| EstimatedSegmentCount >= MaxModifierBoxes;
		const int32 BaseSegmentCount = bRequiresMaximumSegmentCount
			? MaxModifierBoxes
			: FMath::Clamp(FMath::CeilToInt(EstimatedSegmentCount), 1, MaxModifierBoxes);
		int32 RemainingSubdivisions = MaxModifierBoxes - BaseSegmentCount;

		TArray<float> SampleDistances;
		SampleDistances.Reserve(BaseSegmentCount + 1);
		SampleDistances.Add(0.0f);

		for (int32 SegmentIndex = 0; SegmentIndex < BaseSegmentCount; ++SegmentIndex)
		{
			const float StartDistance = SplineLength * SegmentIndex / BaseSegmentCount;
			const float EndDistance = SplineLength * (SegmentIndex + 1) / BaseSegmentCount;
			AppendAdaptiveDistanceRange(
				SplineComponent,
				StartDistance,
				EndDistance,
				MaxChordError,
				MaxSegmentAngle,
				0,
				RemainingSubdivisions,
				SampleDistances);
		}

		return SampleDistances;
	}

	FVector GetMaximumSplineScale(
		const USplineComponent& SplineComponent,
		const float StartDistance,
		const float EndDistance)
	{
		const float MiddleDistance = (StartDistance + EndDistance) * 0.5f;
		const FVector StartScale = SplineComponent.GetScaleAtDistanceAlongSpline(StartDistance).GetAbs();
		const FVector MiddleScale = SplineComponent.GetScaleAtDistanceAlongSpline(MiddleDistance).GetAbs();
		const FVector EndScale = SplineComponent.GetScaleAtDistanceAlongSpline(EndDistance).GetAbs();
		const FVector ComponentScale = SplineComponent.GetComponentScale().GetAbs();

		return FVector(
			FMath::Max3(StartScale.X, MiddleScale.X, EndScale.X),
			FMath::Max3(StartScale.Y, MiddleScale.Y, EndScale.Y),
			FMath::Max3(StartScale.Z, MiddleScale.Z, EndScale.Z)) * ComponentScale;
	}

	FQuat MakeSegmentRotation(
		const USplineComponent& SplineComponent,
		const FVector& StartLocation,
		const FVector& EndLocation,
		const float MiddleDistance)
	{
		FVector ForwardVector = (EndLocation - StartLocation).GetSafeNormal();
		if (ForwardVector.IsNearlyZero())
		{
			ForwardVector = SplineComponent.GetDirectionAtDistanceAlongSpline(
				MiddleDistance,
				ESplineCoordinateSpace::World).GetSafeNormal();
		}
		if (ForwardVector.IsNearlyZero())
		{
			ForwardVector = FVector::ForwardVector;
		}

		FVector SplineUpVector = SplineComponent.GetUpVectorAtDistanceAlongSpline(
			MiddleDistance,
			ESplineCoordinateSpace::World).GetSafeNormal();
		if (SplineUpVector.IsNearlyZero())
		{
			SplineUpVector = FVector::UpVector;
		}
		return FRotationMatrix::MakeFromXZ(ForwardVector, SplineUpVector).ToQuat();
	}
}

URTSSplineNav::URTSSplineNav()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSSplineNav::RefreshSplineNavigation()
{
	if (IsTemplate())
	{
		return;
	}

	UpdateNavigationBounds();
	RefreshNavigationModifiers();
}

void URTSSplineNav::CalculateBounds() const
{
	Bounds = FBox(ForceInit);
	ComponentBounds.Reset();

	const USplineComponent* SplineComponent = FindFirstSplineComponent();
	if (not IsValid(SplineComponent))
	{
		return;
	}

	BuildSplineModifierBounds(*SplineComponent);
}

void URTSSplineNav::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (GIsEditor && not HasAnyFlags(RF_ClassDefaultObject) && not M_ObjectPropertyChangedHandle.IsValid())
	{
		M_ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(
			this,
			&URTSSplineNav::HandleObjectPropertyChanged);
	}
#endif
}

void URTSSplineNav::OnUnregister()
{
#if WITH_EDITOR
	if (M_ObjectPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(M_ObjectPropertyChangedHandle);
		M_ObjectPropertyChangedHandle.Reset();
	}
#endif

	Super::OnUnregister();
}

const USplineComponent* URTSSplineNav::FindFirstSplineComponent() const
{
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("OwnerActor"),
			TEXT("FindFirstSplineComponent"),
			this);
		return nullptr;
	}

	const USplineComponent* SplineComponent = OwnerActor->FindComponentByClass<USplineComponent>();
	if (not IsValid(SplineComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("Owner's first SplineComponent"),
			TEXT("FindFirstSplineComponent"),
			this);
		return nullptr;
	}

	return SplineComponent;
}

void URTSSplineNav::BuildSplineModifierBounds(const USplineComponent& SplineComponent) const
{
	const float SplineLength = SplineComponent.GetSplineLength();
	if (SplineLength <= UE_SMALL_NUMBER)
	{
		AddSplinePointModifier(SplineComponent);
		return;
	}

	const float SafeMaxSegmentLength = FMath::Max(MaxSegmentLength, 1.0f);
	const float SafeMaxChordError = FMath::Max(MaxChordError, RTSSplineNav::Private::MinimumChordError);
	const float SafeMaxSegmentAngle = FMath::Clamp(
		MaxSegmentAngle,
		RTSSplineNav::Private::MinimumSegmentAngle,
		RTSSplineNav::Private::MaximumSegmentAngle);
	const int32 SafeMaxModifierBoxes = FMath::Clamp(
		MaxModifierBoxes,
		1,
		RTSSplineNav::Private::MaximumModifierBoxes);
	const TArray<float> SampleDistances = RTSSplineNav::Private::BuildSampleDistances(
		SplineComponent,
		SplineLength,
		SafeMaxSegmentLength,
		SafeMaxChordError,
		SafeMaxSegmentAngle,
		SafeMaxModifierBoxes);

	for (int32 SampleIndex = 1; SampleIndex < SampleDistances.Num(); ++SampleIndex)
	{
		AddSplineSegmentModifier(
			SplineComponent,
			SampleDistances[SampleIndex - 1],
			SampleDistances[SampleIndex]);
	}
}

void URTSSplineNav::AddSplineSegmentModifier(
	const USplineComponent& SplineComponent,
	const float StartDistance,
	const float EndDistance) const
{
	const FVector StartLocation = SplineComponent.GetLocationAtDistanceAlongSpline(
		StartDistance,
		ESplineCoordinateSpace::World);
	const FVector EndLocation = SplineComponent.GetLocationAtDistanceAlongSpline(
		EndDistance,
		ESplineCoordinateSpace::World);
	const FVector CenterLocation = (StartLocation + EndLocation) * 0.5;
	const float MiddleDistance = (StartDistance + EndDistance) * 0.5f;
	const FQuat SegmentRotation = RTSSplineNav::Private::MakeSegmentRotation(
		SplineComponent,
		StartLocation,
		EndLocation,
		MiddleDistance);

	const FVector MaximumSplineScale = RTSSplineNav::Private::GetMaximumSplineScale(
		SplineComponent,
		StartDistance,
		EndDistance);
	const FVector SafeModifierScale = ModifierScale.GetAbs().ComponentMax(FVector(UE_SMALL_NUMBER));
	FVector SegmentHalfExtent = SplineHalfExtent.GetAbs() * MaximumSplineScale * SafeModifierScale;
	SegmentHalfExtent.X += FVector::Dist(StartLocation, EndLocation) * 0.5f;
	SegmentHalfExtent = SegmentHalfExtent.ComponentMax(FVector(RTSSplineNav::Private::MinimumBoxExtent));

	const FTransform RotationOnlyTransform(SegmentRotation);
	const FVector CenterInRotationFrame = RotationOnlyTransform.InverseTransformPosition(CenterLocation);
	const FBox BoxInRotationFrame = FBox::BuildAABB(CenterInRotationFrame, SegmentHalfExtent);
	ComponentBounds.Add(FRotatedBox(BoxInRotationFrame, SegmentRotation));

	const FBox LocalCenteredBox = FBox::BuildAABB(FVector::ZeroVector, SegmentHalfExtent);
	Bounds += LocalCenteredBox.TransformBy(FTransform(SegmentRotation, CenterLocation));
}

void URTSSplineNav::AddSplinePointModifier(const USplineComponent& SplineComponent) const
{
	const FVector PointLocation = SplineComponent.GetLocationAtDistanceAlongSpline(
		0.0f,
		ESplineCoordinateSpace::World);
	const FQuat PointRotation = SplineComponent.GetQuaternionAtDistanceAlongSpline(
		0.0f,
		ESplineCoordinateSpace::World);
	const FVector SplineScale = RTSSplineNav::Private::GetMaximumSplineScale(
		SplineComponent,
		0.0f,
		0.0f);
	const FVector SafeModifierScale = ModifierScale.GetAbs().ComponentMax(FVector(UE_SMALL_NUMBER));
	const FVector PointHalfExtent = (SplineHalfExtent.GetAbs() * SplineScale * SafeModifierScale)
		.ComponentMax(FVector(RTSSplineNav::Private::MinimumBoxExtent));

	const FTransform RotationOnlyTransform(PointRotation);
	const FVector CenterInRotationFrame = RotationOnlyTransform.InverseTransformPosition(PointLocation);
	const FBox BoxInRotationFrame = FBox::BuildAABB(CenterInRotationFrame, PointHalfExtent);
	ComponentBounds.Add(FRotatedBox(BoxInRotationFrame, PointRotation));

	const FBox LocalCenteredBox = FBox::BuildAABB(FVector::ZeroVector, PointHalfExtent);
	Bounds += LocalCenteredBox.TransformBy(FTransform(PointRotation, PointLocation));
}

#if WITH_EDITOR
void URTSSplineNav::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshSplineNavigation();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void URTSSplineNav::HandleObjectPropertyChanged(
	UObject* ChangedObject,
	FPropertyChangedEvent&)
{
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor) || not IsValid(ChangedObject))
	{
		return;
	}

	const USplineComponent* ChangedSplineComponent = Cast<USplineComponent>(ChangedObject);
	const bool bOwnerChanged = ChangedObject == OwnerActor;
	const bool bOwnedSplineChanged = IsValid(ChangedSplineComponent)
		&& ChangedSplineComponent->GetOwner() == OwnerActor;
	if (not bOwnerChanged && not bOwnedSplineChanged)
	{
		return;
	}

	RefreshSplineNavigation();
}
#endif
