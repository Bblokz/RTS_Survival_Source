// Copyright ...
#pragma once

#include "CoreMinimal.h"
#include "WeaponDescriptionType.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "W_WeaponDescription.generated.h"

enum class ERTSRichText : uint8;
class UWeaponState;
class UWeaponStateLaser;
class UWeaponStateFlameThrower;
class UBombComponent;
class USizeBox;
class UWidgetSwitcher;
enum class EWeaponShellType : uint8;
enum class EWeaponName : uint8;
struct FWeaponData;

/**
 * @brief Lightweight, data-driven weapon/bomb description widget.
 * Renders consistent rich-text lines from FWeaponDescription using small
 * helpers (number+unit, no stray spaces, consistent styles).
 * @note OnWeaponDataUpdated: Blueprint can react after text is refreshed.
 */
USTRUCT(BlueprintType, Blueprintable)
struct FWeaponDescription
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EWeaponDescriptionType WeaponDescriptionType = EWeaponDescriptionType::Regular;

	UPROPERTY(BlueprintReadOnly)
	ERTSDamageType DamageType = ERTSDamageType::None;

	UPROPERTY(BlueprintReadOnly)
	EWeaponName WeaponName = EWeaponName::DEFAULT_WEAPON;

	UPROPERTY(BlueprintReadOnly)
	EWeaponShellType ShellType = EWeaponShellType::Shell_AP;

	UPROPERTY(BlueprintReadOnly)
	TArray<EWeaponShellType> ShellTypes = {EWeaponShellType::Shell_AP};

	// Gun diameter in mm.
	UPROPERTY(BlueprintReadOnly)
	float WeaponCalibre = 0.f;

	// Base damage dealt by one bullet.
	UPROPERTY(BlueprintReadOnly)
	float BaseDamage = 0.0f;

	// Effective range of the weapon in centimeters.
	UPROPERTY(BlueprintReadOnly)
	float Range = 0.0f;

	// Factor used for armor penetration calculations before damage application.
	UPROPERTY(BlueprintReadOnly)
	float ArmorPen = 0.0f;

	// Factor used for armor penetration calculations before damage application at maximum range.
	UPROPERTY(BlueprintReadOnly)
	float ArmorPenAtMaxRange = 0.0f;

	// Maximum number of bullets in the magazine before requiring a reload.
	UPROPERTY(BlueprintReadOnly)
	int32 MagCapacity = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentMagCapacity = 0;

	// Accuracy rating of the weapon, ranging from 1 to 100 (100 indicating perfect accuracy).
	UPROPERTY(BlueprintReadOnly)
	int32 Accuracy = 0;

	// Time taken to reload the entire magazine, measured in seconds.
	UPROPERTY(BlueprintReadOnly)
	float ReloadSpeed = 0.0f;

	// Base cooldown time between individual shots, measured in seconds.
	UPROPERTY(BlueprintReadOnly)
	float BaseCooldown = 0.0f;

	// Range of the Area of Effect (AOE) explosion in centimeters, applicable for AOE-enabled projectiles.
	UPROPERTY(BlueprintReadOnly)
	float ShrapnelRange = 0.0f;

	// Damage dealt by each projectile in an AOE attack, relevant for AOE-enabled projectiles.
	UPROPERTY(BlueprintReadOnly)
	float ShrapnelDamage = 0.0f;

	// Number of shrapnel particles generated in an AOE attack, applicable for AOE-enabled projectiles.
	UPROPERTY(BlueprintReadOnly)
	int32 ShrapnelParticles = 0;

	// Factor used for armor penetration calculations before damage application in AOE attacks.
	UPROPERTY()
	float ShrapnelPen = 0.0f;

	// Explosive payload tnt equivalent in grams.
	UPROPERTY(BlueprintReadOnly)
	float TNTExplosiveGrams = 0.f;

	// Count of amount of sockets fired from; this is one for regular weapons but multi trac and multi projectile weapons
	// can have more than one.
	UPROPERTY(BlueprintReadOnly)
	int32 Count = 0;

	// Maximum number of bombs in bomb component.
	UPROPERTY(BlueprintReadOnly)
	int32 MaxBombs = 0;

	// Current amount of bombs left in bomb component
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentBombs = 0;

	// How long between bombs.
	UPROPERTY(BlueprintReadOnly)
	float BombInterval = 0.f;

	// The angle cone of the flame weapon in degrees.
	UPROPERTY(BlueprintReadOnly)
	float FlameAngle = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 FlameRays = 0;

	// Used by flame and laser weapons for damage per burst (one full iteration).
	UPROPERTY(BlueprintReadOnly)
	int32 DamageTicks = 0;
};

UCLASS()
class RTS_SURVIVAL_API UW_WeaponDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Update view for weapon state under hover.
	 *  @param WeaponState weapon data source (can be flame/laser/multi*).
	 *  @return Effective description type that was set. */
	EWeaponDescriptionType OnHoverWeaponItem(UWeaponState* WeaponState);

	/** @brief Update view for bomb component hover. */
	void OnHoverBombItem(UBombComponent* BombComponent);

	void OnUnHover();

protected:
	// Contains the data of the current weapon assigned to this widget, initialised in cpp.
	UPROPERTY(BlueprintReadOnly)
	FWeaponDescription WeaponDescription;

	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponDataUpdated();

	void UpdateWeaponDescriptionTexts();
	void UpdateBombDescriptionTexts();

	/** @brief Formats a float to text with a specified number of decimal places. */
	UFUNCTION(BlueprintCallable, Category = "WeaponDescription")
	FText FormatFloatToText(float Value, int32 DecimalPlaces) const;

	UPROPERTY(meta = (BindWidget))
	USizeBox* SizeBoxAmmoIcon;

	UPROPERTY(meta = (BindWidget))
	USizeBox* SizeBoxAmmoText;

	UPROPERTY(meta = (BindWidget))
	URichTextBlock* LeftDescriptionBox;

	UPROPERTY(meta = (BindWidget))
	URichTextBlock* RightDescriptionBox;

	UPROPERTY(meta = (BindWidget))
	URichTextBlock* DamageTypeBox;

private:
	// ----- Validators (per-member, logs via RTSFunctionLibrary::ReportError... ) -----
	bool GetIsValidLeftDescriptionBox() const;
	bool GetIsValidRightDescriptionBox() const;
	bool GetIsValidDamageTypeBox() const;
	bool GetIsValidSizeBoxAmmoIcon() const;
	bool GetIsValidSizeBoxAmmoText() const;

	// ----- Small rich-text helpers (no stray spaces between styled segments) -----
	/** @brief Wrap FRTSRichTextConverter::MakeStringRTSRichText for clarity. */
	FString GetRTSRich(const FString& InText, ERTSRichText RichType) const;

	/** @brief Number -> rich with optional fixed decimals. */
	FString GetNumberRich(float Value, int32 Decimals, ERTSRichText RichType) const;

	/** @brief Number with unit (concatenated without extra spaces). */
	FString GetNumberWithUnitRich(float Value, int32 Decimals, ERTSRichText NumberType,
	                              const TCHAR* Unit, ERTSRichText UnitType) const;

	/** @brief Armor pen number rich (no units). */
	FString GetArmorPenText(float ArmorPen) const;

	// ----- Line builders (reuse across Regular/Flame/Laser/Bomb) -----
	FString Line_CalibreMM() const;
	FString Line_AverageDamage() const;
	FString Line_ArmorPenAt90() const;
	FString Line_ArmorPenAtMaxRange() const;
	FString Line_ReloadSeconds(const TCHAR* Label) const;
	FString Line_TNTGrams(const TCHAR* Label = TEXT("TNT Explosive: ")) const;
	FString Line_Accuracy() const;
	FString Line_RangeMeters() const;
	FString Line_MagCapacity() const;
	FString Line_AverageCooldownIfMulti() const;
	FString Lines_AOEIfAny() const;
	FString Line_WeaponsCountIfMulti() const;
	FString Line_Ticks() const;
	FString Line_ConeAngleDeg() const;
	FString Line_BombCapacity() const;
	FString Line_BombInterval() const;

	// ----- Text assembly entry points -----
	void UpdateLeftWeaponDescriptionText(EWeaponDescriptionType HoverType) const;
	FString LeftRegularText() const;
	FString LeftFlameText() const;
	FString LeftLaserText() const;

	void UpdateRightWeaponDescriptionText(EWeaponDescriptionType HoverType) const;
	FString RightRegularText() const;
	FString RightFlameText() const;
	FString RightLaserText() const;

	void UpdateDamageTypeText() const;
	void UpdateLeftBombDescriptionText() const;
	void UpdateRightBombDescriptionText() const;

	// ----- Data plumbing / view toggles -----
	void SetDescriptionWithData(const FWeaponData& WeaponData, const UWeaponState* WeaponState);
	void SetAmmoIconVisibility(bool bVisible) const;

	FString GetCanonicalNameOfDmgType(ERTSDamageType DamageType) const;
	void SetToHoverRegularOrMultiWeapon(UWeaponState* WeaponState);
	void SetToHoverFlameWeapon(const UWeaponStateFlameThrower* FlameThrower);
	void SetToHoverLaserWeapon(const UWeaponStateLaser* LaserWeapon);
};
