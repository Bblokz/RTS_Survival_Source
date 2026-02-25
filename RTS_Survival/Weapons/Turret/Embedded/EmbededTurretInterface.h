#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "EmbededTurretInterface.generated.h"

/**
 * @brief Used by an embedded turret to communicate with the owning tank.
 * Uses GetCurrentTurretAngle from tank and sets new target angle and target pitch.
 */
UINTERFACE(Blueprintable)
class RTS_SURVIVAL_API UEmbeddedTurretInterface : public UInterface
{
	GENERATED_BODY()
};

class RTS_SURVIVAL_API IEmbeddedTurretInterface
{
	GENERATED_BODY()

public:
	/** @return The angle that the turret is facing. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Turret|Angle")
	float GetCurrentTurretAngle() const;
	virtual float GetCurrentTurretAngle_Implementation() const;

	/**
	 * @brief Sets the angle that the turret is facing.
	 * @param NewAngle The new angle that the turret is facing.
	 * @note Sets instantly.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Turret|Angle")
	void SetTurretAngle(float NewAngle);
	virtual void SetTurretAngle_Implementation(float NewAngle);

	/**
	 * @brief Update the animation of the weapon pitch.
	 * @param NewPitch The new pitch of the weapon.
	 * @note Sets instantly.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Turret|Angle")
	void UpdateTargetPitch(float NewPitch);
	virtual void UpdateTargetPitch_Implementation(float NewPitch);

	/**
	 * @brief Instructs the owning tank to turn the base to face the target.
	 * @param Degrees The amount of degrees needed to turn to face the target.
	 * @return Whether the turning process is successful, false if the owner needs control of turning for movement.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Turret|Control")
	bool TurnBase(float Degrees);
	virtual bool TurnBase_Implementation(float Degrees);

	/**
	 * @brief Notifies the owning Tank that the turret has fired and the animation should be played.
	 * @param WeaponIndex The index of the weapon in the turret.
	 * @note We use these fire functions to notify the owner that implements the embedded turret and hence the skeleton
	 * that an animation needs to be played. We cannot simply use the PlayAnimation function from the weapon owner (this turret)
	 * as not all embedded turrets have access to their skeleton.
	 * @note Therefore, in the BP embedded turrets master we call these interface functions on the BP Play weapon animation
	 * which is called by the implemented weapons.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Turret|Animation")
	void PlaySingleFireAnimation(int32 WeaponIndex);
	virtual void PlaySingleFireAnimation_Implementation(int32 WeaponIndex);

	/**
	 * @brief Notifies the owning Tank that the turret has fired and the animation should be played.
	 * @param WeaponIndex The index of the weapon in the turret.
	 * @note We use these fire functions to notify the owner that implements the embedded turret and hence the skeleton
	 * that an animation needs to be played. We cannot simply use the PlayAnimation function from the weapon owner (this turret)
	 * as not all embedded turrets have access to their skeleton.
	 * @note Therefore, in the BP embedded turrets master we call these interface functions on the BP Play weapon animation
	 * which is called by the implemented weapons.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Turret|Animation")
	void PlayBurstFireAnimation(int32 WeaponIndex);
	virtual void PlayBurstFireAnimation_Implementation(int32 WeaponIndex);
};
