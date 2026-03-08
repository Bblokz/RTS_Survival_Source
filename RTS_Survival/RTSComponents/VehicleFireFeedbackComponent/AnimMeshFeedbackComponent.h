#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/VehicleFireFeedbackComponent/VehicleFireFeedbackComponent.h"
#include "AnimMeshFeedbackComponent.generated.h"

class ACPPTurretsMaster;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EAnimMeshRecoilSignedAxis : uint8
{
	ForwardX UMETA(DisplayName="Forward (+X)"),
	BackwardX UMETA(DisplayName="Backward (-X)"),
	RightY UMETA(DisplayName="Right (+Y)"),
	LeftY UMETA(DisplayName="Left (-Y)"),
	UpZ UMETA(DisplayName="Up (+Z)"),
	DownZ UMETA(DisplayName="Down (-Z)")
};

UENUM(BlueprintType)
enum class EAnimMeshOptionalRollChannel : uint8
{
	Roll UMETA(DisplayName="Roll Channel"),
	Pitch UMETA(DisplayName="Pitch Channel")
};

USTRUCT(BlueprintType)
struct FAnimMeshRecoilAxesSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	EAnimMeshRecoilSignedAxis M_BackwardRecoilAxis = EAnimMeshRecoilSignedAxis::BackwardX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	EAnimMeshRecoilSignedAxis M_SideSwayAxis = EAnimMeshRecoilSignedAxis::RightY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	EAnimMeshOptionalRollChannel M_OptionalRollChannel = EAnimMeshOptionalRollChannel::Roll;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	float M_MaxSideSwayCm = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	float M_MaxOptionalRollDeg = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	bool bM_EnableSideSway = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	bool bM_EnableOptionalRoll = true;
};

/**
 * @brief Allows turrets to drive recoil feedback on a single skeletal mesh using the same weapon-tracking flow.
 * Use this when no separate hull static mesh exists and recoil must be additive on top of AO/aiming animation.
 * @note InitializeAnimMeshFeedbackComponent: call in blueprint with animated skeletal mesh + turret master.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAnimMeshFeedbackComponent : public UVehicleFireFeedbackComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="VehicleFireFeedback")
	void InitializeAnimMeshFeedbackComponent(
		USkeletalMeshComponent* InAnimatedMesh,
		ACPPTurretsMaster* InTurretMaster);

protected:
	virtual bool GetIsValidHullMesh() const override;
	virtual void CacheHullBaseTransform() override;
	virtual void ApplyFeedbackKick(float NormalisedMuzzleEnergy, float TurretWorldYawDegrees) override;
	virtual void ApplyHullFeedbackTransform() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VehicleFireFeedback|AnimMesh")
	FAnimMeshRecoilAxesSettings M_AnimMeshRecoilAxesSettings;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_AnimatedMesh = nullptr;

	// Tracks the recoil location offset that was last written so external animation can keep owning base movement.
	mutable FVector M_LastAppliedRecoilLocationOffsetCm = FVector::ZeroVector;

	// Tracks the recoil pitch that was last written to the mesh so we can preserve external yaw/roll drivers like AO.
	mutable float M_LastAppliedRecoilPitchDeg = 0.0f;

	// Tracks optional recoil roll contribution so we only remove/apply the part this component owns.
	mutable float M_LastAppliedRecoilRollDeg = 0.0f;

	bool GetIsValidAnimatedMesh() const;
	FVector GetSignedAxisVector(EAnimMeshRecoilSignedAxis Axis) const;
	void ApplyPreservedRecoilLocation() const;
	void ApplyPreservedRecoilRotation() const;
	void GetCurrentRecoilPitchAndRoll(float& OutRecoilPitchDeg, float& OutRecoilRollDeg) const;
};
