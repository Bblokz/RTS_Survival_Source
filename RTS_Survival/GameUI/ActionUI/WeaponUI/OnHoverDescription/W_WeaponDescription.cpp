#include "W_WeaponDescription.h"

#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"

namespace WeaponDescriptionLayout
{
	ERTSRichText NormalText = ERTSRichText::Text_Armor;
	ERTSRichText BuffedText = ERTSRichText::Text_Exp;
	ERTSRichText DeBuffText = ERTSRichText::Text_Bad14;
	
}

EWeaponDescriptionType UW_WeaponDescription::OnHoverWeaponItem(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return EWeaponDescriptionType::Regular;
	}
	SetVisibility(ESlateVisibility::Visible);

	// Get the weapon data from the weapon state and adjust it for the current shell type.
	const FWeaponData WeaponData = WeaponState->GetWeaponDataAdjustedForShellType();
	SetDescriptionWithData(WeaponData, WeaponState);

	if (const UWeaponStateFlameThrower* FlameThrower = Cast<UWeaponStateFlameThrower>(WeaponState))
	{
		// Also hides the ammo icon
		SetToHoverFlameWeapon(FlameThrower);
	}
	else if (const UWeaponStateLaser* LaserWeapon = Cast<UWeaponStateLaser>(WeaponState))
	{
		// Also hides the ammo icon
		SetToHoverLaserWeapon(LaserWeapon);
	}
	else
	{
		// Shows ammo icon.
		SetToHoverRegularOrMultiWeapon(WeaponState);
	}

	UpdateWeaponDescriptionTexts();
	OnWeaponDataUpdated();
	return WeaponDescription.WeaponDescriptionType;
}

void UW_WeaponDescription::OnHoverBombItem(UBombComponent* BombComponent)
{
	if (not IsValid(BombComponent))
	{
		return;
	}
	SetVisibility(ESlateVisibility::Visible);

	WeaponDescription.WeaponDescriptionType = EWeaponDescriptionType::BombComponent;

	// Get the bomb data from the bomb component.
	const FWeaponData WeaponData = BombComponent->GetBombBayWeaponData();
	SetDescriptionWithData(WeaponData, nullptr);

	WeaponDescription.BombInterval = BombComponent->GetBombBayData().BombInterval;
	WeaponDescription.MaxBombs = BombComponent->GetBombBayData().GetMaxBombs();
	WeaponDescription.CurrentBombs = BombComponent->GetBombBayData().GetBombsRemaining();

	UpdateBombDescriptionTexts();
	// bombs don't use the ammo icon, so hide it.
	SetAmmoIconVisibility(false);
	OnWeaponDataUpdated();
}

void UW_WeaponDescription::OnUnHover()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

// ---------- Validators ----------

bool UW_WeaponDescription::GetIsValidLeftDescriptionBox() const
{
	if (IsValid(LeftDescriptionBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("LeftDescriptionBox"),
	                                                             TEXT("GetIsValidLeftDescriptionBox"), this);
	return false;
}

bool UW_WeaponDescription::GetIsValidRightDescriptionBox() const
{
	if (IsValid(RightDescriptionBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("RightDescriptionBox"),
	                                                             TEXT("GetIsValidRightDescriptionBox"), this);
	return false;
}

bool UW_WeaponDescription::GetIsValidDamageTypeBox() const
{
	if (IsValid(DamageTypeBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("DamageTypeBox"),
	                                                             TEXT("GetIsValidDamageTypeBox"), this);
	return false;
}

bool UW_WeaponDescription::GetIsValidSizeBoxAmmoIcon() const
{
	if (IsValid(SizeBoxAmmoIcon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("SizeBoxAmmoIcon"),
	                                                             TEXT("GetIsValidSizeBoxAmmoIcon"), this);
	return false;
}

bool UW_WeaponDescription::GetIsValidSizeBoxAmmoText() const
{
	if (IsValid(SizeBoxAmmoText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("SizeBoxAmmoText"),
	                                                             TEXT("GetIsValidSizeBoxAmmoText"), this);
	return false;
}

// ---------- Rich-text helpers (no stray spaces) ----------

FString UW_WeaponDescription::GetRTSRich(const FString& InText, const ERTSRichText RichType) const
{
	return FRTSRichTextConverter::MakeRTSRich(InText, RichType);
}

FString UW_WeaponDescription::GetNumberRich(const float Value, const int32 Decimals, const ERTSRichText RichType) const
{
	return GetRTSRich(FormatFloatToText(Value, Decimals).ToString(), RichType);
}

FString UW_WeaponDescription::GetNumberWithUnitRich(const float Value, const int32 Decimals,
                                                    const ERTSRichText NumberType,
                                                    const TCHAR* Unit,
                                                    const ERTSRichText UnitType) const
{
	// Intentionally no TEXT(" ") between segments to avoid oversized gaps between styles.
	return GetNumberRich(Value, Decimals, NumberType) + GetRTSRich(Unit, UnitType);
}

FString UW_WeaponDescription::GetArmorPenText(const float ArmorPen) const
{
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.ArmorPenetration> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.ArmorPenetration < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return GetNumberRich(ArmorPen, 0, TextTypeBuffAdjusted);
}

// ---------- Reusable line builders ----------

FString UW_WeaponDescription::Line_CalibreMM() const
{
	return TEXT("Calibre: ")
		+ GetNumberWithUnitRich(WeaponDescription.WeaponCalibre, 0, ERTSRichText::Text_Armor, TEXT("MM"),
		                        ERTSRichText::Text_Armor)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_AverageDamage() const
{
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.Damage > 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.Damage < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return TEXT("Average Damage: ")
		+ GetNumberRich(WeaponDescription.BaseDamage, 0, TextTypeBuffAdjusted)
		+ TEXT("\n");
	
}

FString UW_WeaponDescription::Line_ArmorPenAt90() const
{
	
	return TEXT("Armor Pen: ")
		+ GetArmorPenText(WeaponDescription.ArmorPen)
		+ GetRTSRich(TEXT(" @90 Deg"), ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_ArmorPenAtMaxRange() const
{
	return TEXT("At max Range: ")
		+ GetArmorPenText(WeaponDescription.ArmorPenAtMaxRange)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_ReloadSeconds(const TCHAR* const Label) const
{
	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	// Negative is faster reload; positive effect.
	if(WeaponDescription.BehaviourAttributes.ReloadSpeed < 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.ReloadSpeed > 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return FString(Label)
		+ GetNumberWithUnitRich(WeaponDescription.ReloadSpeed, 1, TextTypeBuffAdjusted, TEXT(" Sec"),
		                        ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_TNTGrams(const TCHAR* const Label) const
{
	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.TnTGrams> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.TnTGrams < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return FString(Label)
		+ GetNumberWithUnitRich(WeaponDescription.TNTExplosiveGrams, 0, TextTypeBuffAdjusted, TEXT(" Gr"),
		                        ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_Accuracy() const
{
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.Accuracy> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.Accuracy< 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return TEXT("Accuracy: ")
		+ GetNumberRich(static_cast<float>(WeaponDescription.Accuracy), 0, TextTypeBuffAdjusted);
}

FString UW_WeaponDescription::Line_RangeMeters() const
{
	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.Range> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.Range < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return TEXT("Range: ")
		+ GetNumberWithUnitRich(WeaponDescription.Range / 100.f, 0, TextTypeBuffAdjusted, TEXT(" meter"),
		                        ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_MagCapacity() const
{
	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.MagSize> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.MagSize < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	if (WeaponDescription.MagCapacity > 1)
	{
		return TEXT("Mag Capacity: ")
			+ GetRTSRich(FString::FromInt(WeaponDescription.CurrentMagCapacity), TextTypeBuffAdjusted)
			+ TEXT("/")
			+ GetRTSRich(FString::FromInt(WeaponDescription.MagCapacity), TextTypeBuffAdjusted)
			+ TEXT("\n");
	}
	return TEXT("Mag Capacity: ")
		+ GetRTSRich(FString::FromInt(WeaponDescription.MagCapacity), TextTypeBuffAdjusted)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_AverageCooldownIfMulti() const
{
	if (WeaponDescription.MagCapacity > 1)
	{
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	// Negative is faster reload; positive effect.
	if(WeaponDescription.BehaviourAttributes.BaseCooldown< 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.BaseCooldown > 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}

		return TEXT("Average Cooldown: ")
			+ GetNumberWithUnitRich(WeaponDescription.BaseCooldown, 2, ERTSRichText::Text_Armor, TEXT(" Sec"),
			                        ERTSRichText::Text_DescriptionHelper)
			+ TEXT("\n");
	}
	return FString();
}

FString UW_WeaponDescription::Lines_AOEIfAny() const
{
	if (WeaponDescription.ShrapnelDamage <= 0.f)
	{
		return FString();
	}

	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.ShrapnelDamage> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.ShrapnelDamage < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	FString Out;
	Out += TEXT("AOE Damage: ")
		+ GetNumberRich(WeaponDescription.ShrapnelDamage, 0, TextTypeBuffAdjusted)
		+ TEXT("\n");
	TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.ShrapnelRange> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.ShrapnelRange < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}

	Out += TEXT("AOE Range: ")
		+ GetNumberWithUnitRich(WeaponDescription.ShrapnelRange, 0, TextTypeBuffAdjusted, TEXT(" cm"),
		                        ERTSRichText::Text_DescriptionHelper);
	return Out;
}

FString UW_WeaponDescription::Line_WeaponsCountIfMulti() const
{
	if (WeaponDescription.Count > 1)
	{
		return TEXT("Weapons: ")
			+ GetRTSRich(FString::FromInt(WeaponDescription.Count), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}
	return FString();
}

FString UW_WeaponDescription::Line_Ticks() const
{
	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.DamageTicks> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.DamageTicks< 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	return TEXT("Ticks: ")
		+ GetRTSRich(FString::FromInt(WeaponDescription.DamageTicks), TextTypeBuffAdjusted)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_ConeAngleDeg() const
{
	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.FlameAngle> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.FlameAngle < 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}

	return TEXT("Cone Angle: ")
		+ GetNumberWithUnitRich(WeaponDescription.FlameAngle, 2, TextTypeBuffAdjusted, TEXT(" Deg"),
		                        ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_BombCapacity() const
{
	return TEXT("Bomb Capacity: ")
		+ GetRTSRich(FString::FromInt(WeaponDescription.CurrentBombs), ERTSRichText::Text_Armor)
		+ TEXT("/")
		+ GetRTSRich(FString::FromInt(WeaponDescription.MaxBombs), ERTSRichText::Text_Armor)
		+ TEXT("\n");
}

FString UW_WeaponDescription::Line_BombInterval() const
{
	return TEXT("Bomb Interval: ")
		+ GetNumberWithUnitRich(WeaponDescription.BombInterval, 2, ERTSRichText::Text_Armor, TEXT(" Sec"),
		                        ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
}

// ---------- Top-level update flows ----------

void UW_WeaponDescription::UpdateWeaponDescriptionTexts()
{
	if (not GetIsValidLeftDescriptionBox() || not GetIsValidRightDescriptionBox())
	{
		return;
	}

	UpdateLeftWeaponDescriptionText(WeaponDescription.WeaponDescriptionType);
	UpdateRightWeaponDescriptionText(WeaponDescription.WeaponDescriptionType);

	if (not GetIsValidDamageTypeBox())
	{
		return;
	}
	UpdateDamageTypeText();
}

void UW_WeaponDescription::UpdateBombDescriptionTexts()
{
	if (not GetIsValidLeftDescriptionBox() || not GetIsValidRightDescriptionBox())
	{
		return;
	}

	UpdateLeftBombDescriptionText();
	UpdateRightBombDescriptionText();

	if (not GetIsValidDamageTypeBox())
	{
		return;
	}
	UpdateDamageTypeText();
}

void UW_WeaponDescription::UpdateLeftWeaponDescriptionText(const EWeaponDescriptionType HoverType) const
{
	FString LeftText;
	switch (HoverType)
	{
	case EWeaponDescriptionType::Regular:
		LeftText = LeftRegularText();
		break;
	case EWeaponDescriptionType::BombComponent:
		RTSFunctionLibrary::ReportError(
			TEXT("Description of weapon set to HoverType: BombComponent but is setup for UWeaponState!"));
		LeftText = LeftRegularText();
		break;
	case EWeaponDescriptionType::FlameWeapon:
		LeftText = LeftFlameText();
		break;
	case EWeaponDescriptionType::LaserWeapon:
		LeftText = LeftLaserText();
		break;
	}
	LeftDescriptionBox->SetText(FText::FromString(LeftText));
}

FString UW_WeaponDescription::LeftRegularText() const
{
	FString LeftText;

	LeftText += Line_CalibreMM();
	LeftText += Line_AverageDamage();
	LeftText += Line_ArmorPenAt90();
	LeftText += Line_ArmorPenAtMaxRange();
	LeftText += Line_ReloadSeconds(TEXT("Reload Speed: "));
	LeftText += Line_TNTGrams();
	LeftText += Line_Accuracy();

	return LeftText;
}

FString UW_WeaponDescription::LeftFlameText() const
{
	FString LeftText;

	LeftText += Line_CalibreMM();

	// Full cone damage (sum of rays) per tick
	LeftText += TEXT("Full Cone Damage: ")
		+ GetNumberRich(WeaponDescription.BaseDamage * WeaponDescription.FlameRays, 0, ERTSRichText::Text_Armor)
		+ GetRTSRich(TEXT("/tick"), ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");

	// Partial (single ray) per tick
	LeftText += TEXT("Partial Damage: ")
		+ GetNumberRich(WeaponDescription.BaseDamage, 0, ERTSRichText::Text_Armor)
		+ GetRTSRich(TEXT("/tick"), ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");

	LeftText += Line_ReloadSeconds(TEXT("Reload Speed: "));

	return LeftText;
}

FString UW_WeaponDescription::LeftLaserText() const
{
	FString LeftText;

	LeftText += Line_CalibreMM();

	
	ERTSRichText TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.Damage > 0 || WeaponDescription.BehaviourAttributes.DamageTicks> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.Damage < 0 || WeaponDescription.BehaviourAttributes.DamageTicks  <0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}


	LeftText += TEXT("Full Laser Damage: ")
		+ GetNumberRich(WeaponDescription.BaseDamage * WeaponDescription.DamageTicks, 0, TextTypeBuffAdjusted)
		+ GetRTSRich(TEXT("/tick"), ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");

	TextTypeBuffAdjusted = WeaponDescriptionLayout::NormalText;
	if(WeaponDescription.BehaviourAttributes.Damage> 0)
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::BuffedText;
	}
	if(WeaponDescription.BehaviourAttributes.Damage< 0 )
	{
		TextTypeBuffAdjusted = WeaponDescriptionLayout::DeBuffText;
	}
	

	LeftText += TEXT("Damage: ")
		+ GetNumberRich(WeaponDescription.BaseDamage, 0, TextTypeBuffAdjusted)
		+ GetRTSRich(TEXT("/tick"), ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");

	LeftText += Line_ReloadSeconds(TEXT("Reload Speed: "));
	LeftText += Line_Accuracy();

	return LeftText;
}

void UW_WeaponDescription::UpdateRightWeaponDescriptionText(const EWeaponDescriptionType HoverType) const
{
	FString RightText;
	switch (HoverType)
	{
	case EWeaponDescriptionType::Regular:
		RightText = RightRegularText();
		break;
	case EWeaponDescriptionType::BombComponent:
		RTSFunctionLibrary::ReportError(
			TEXT("Description of weapon set to HoverType: BombComponent but is setup for UWeaponState!"));
		RightText = RightRegularText();
		break;
	case EWeaponDescriptionType::FlameWeapon:
		RightText = RightFlameText();
		break;
	case EWeaponDescriptionType::LaserWeapon:
		RightText = RightLaserText();
		break;
	}
	RightDescriptionBox->SetText(FText::FromString(RightText));
}

FString UW_WeaponDescription::RightRegularText() const
{
	FString RightText;

	RightText += Line_WeaponsCountIfMulti();
	RightText += Line_RangeMeters();
	RightText += Line_MagCapacity();
	RightText += Line_AverageCooldownIfMulti();
	RightText += Lines_AOEIfAny();

	return RightText;
}

FString UW_WeaponDescription::RightFlameText() const
{
	FString RightText;

	RightText += Line_RangeMeters();

	if (WeaponDescription.MagCapacity > 1)
	{
		// Bursts as in amount per mag (flame weapons are single fire weapons that deplete their mag every full fire iteration)
		RightText += TEXT("Bursts: ")
			+ GetRTSRich(FString::FromInt(WeaponDescription.MagCapacity), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}

	RightText += Line_AverageCooldownIfMulti();
	RightText += Line_Ticks();
	RightText += Line_ConeAngleDeg();

	return RightText;
}

FString UW_WeaponDescription::RightLaserText() const
{
	FString RightText;

	RightText += Line_RangeMeters();
	RightText += Line_MagCapacity();
	RightText += Line_AverageCooldownIfMulti();
	RightText += Line_Ticks();

	return RightText;
}

void UW_WeaponDescription::UpdateDamageTypeText() const
{
	const FString Start = TEXT("Damage Type: ");
	const FString TypeText = FRTSRichTextConverter::MakeDamageTypeString_ForWeaponDmgType(
		GetCanonicalNameOfDmgType(WeaponDescription.DamageType), WeaponDescription.DamageType);
	DamageTypeBox->SetText(FText::FromString(Start + TypeText));
}

void UW_WeaponDescription::UpdateLeftBombDescriptionText() const
{
	FString LeftText;

	LeftText += Line_CalibreMM();
	LeftText += Line_AverageDamage();
	LeftText += Line_ArmorPenAt90();
	LeftText += Line_ReloadSeconds(TEXT("Full bay reload time: "));
	LeftText += Line_TNTGrams();
	LeftText += Line_Accuracy();

	LeftDescriptionBox->SetText(FText::FromString(LeftText));
}

void UW_WeaponDescription::UpdateRightBombDescriptionText() const
{
	FString RightText;

	RightText += Line_BombCapacity();
	RightText += Line_BombInterval();
	RightText += Lines_AOEIfAny();

	RightDescriptionBox->SetText(FText::FromString(RightText));
}

// ---------- Data / view plumbing ----------

void UW_WeaponDescription::SetDescriptionWithData(const FWeaponData& WeaponData, const UWeaponState* WeaponState)
{
	// May be changed by multi weapons later in the Hover function.
	WeaponDescription.Count = 1;
	WeaponDescription.WeaponName = WeaponData.WeaponName;
	WeaponDescription.DamageType = WeaponData.DamageType;
	WeaponDescription.ShellType = WeaponData.ShellType;
	WeaponDescription.ShellTypes = WeaponData.ShellTypes;
	WeaponDescription.Range = WeaponData.Range;
	WeaponDescription.Accuracy = WeaponData.Accuracy;
	WeaponDescription.ArmorPen = WeaponData.ArmorPen;
	WeaponDescription.ArmorPenAtMaxRange = WeaponData.ArmorPenMaxRange;
	WeaponDescription.BaseCooldown = WeaponData.BaseCooldown;
	WeaponDescription.BaseDamage = WeaponData.BaseDamage;
	WeaponDescription.MagCapacity = WeaponData.MagCapacity;
	WeaponDescription.BehaviourAttributes = WeaponData.BehaviourAttributes;

	// Is null for bomb components.
	if (WeaponState)
	{
		WeaponDescription.CurrentMagCapacity = WeaponState->GetCurrentMagCapacity();
	}
	WeaponDescription.ReloadSpeed = WeaponData.ReloadSpeed;
	WeaponDescription.ShrapnelDamage = WeaponData.ShrapnelDamage;
	WeaponDescription.ShrapnelParticles = WeaponData.ShrapnelParticles;
	WeaponDescription.ShrapnelRange = WeaponData.ShrapnelRange;
	WeaponDescription.ShrapnelPen = WeaponData.ShrapnelPen;
	WeaponDescription.WeaponCalibre = WeaponData.WeaponCalibre;
	WeaponDescription.TNTExplosiveGrams = WeaponData.TNTExplosiveGrams;
}

void UW_WeaponDescription::SetAmmoIconVisibility(const bool bVisible) const
{
	// Early-out if either is invalid; log handled by validators.
	if (not GetIsValidSizeBoxAmmoIcon() || not GetIsValidSizeBoxAmmoText())
	{
		return;
	}
	SizeBoxAmmoIcon->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SizeBoxAmmoText->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

FText UW_WeaponDescription::FormatFloatToText(float Value, int32 DecimalPlaces) const
{
	// Check if the value has no fractional part
	if (FMath::FloorToFloat(Value) == Value)
	{
		DecimalPlaces = 0;
	}
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.MinimumFractionalDigits = DecimalPlaces;
	NumberFormatOptions.MaximumFractionalDigits = DecimalPlaces;

	// If DecimalPlaces is zero, we need to round the value to the nearest integer
	if (DecimalPlaces == 0)
	{
		Value = FMath::RoundToFloat(Value);
	}

	return FText::AsNumber(Value, &NumberFormatOptions);
}

FString UW_WeaponDescription::GetCanonicalNameOfDmgType(const ERTSDamageType DamageType) const
{
	switch (DamageType)
	{
	case ERTSDamageType::None:
		break;
	case ERTSDamageType::Kinetic:
		return TEXT("Kinetic");
	case ERTSDamageType::Fire:
		return TEXT("Fire");
	case ERTSDamageType::Laser:
		return TEXT("Laser");
	case ERTSDamageType::Radiation:
		return TEXT("Radiation");
	}
	return TEXT("Unknown");
}

void UW_WeaponDescription::SetToHoverRegularOrMultiWeapon(UWeaponState* WeaponState)
{
	if (const UWeaponStateMultiTrace* MultiTrace = Cast<UWeaponStateMultiTrace>(WeaponState))
	{
		WeaponDescription.Count = MultiTrace->GetTraceCount();
	}
	if (const UWeaponStateMultiProjectile* ProjectileWeapon = Cast<UWeaponStateMultiProjectile>(WeaponState))
	{
		WeaponDescription.Count = ProjectileWeapon->GetProjectileCount();
	}
	WeaponDescription.WeaponDescriptionType = EWeaponDescriptionType::Regular;
	// Regular weapons use the ammo icon to display their ammo type.
	SetAmmoIconVisibility(true);
}

void UW_WeaponDescription::SetToHoverFlameWeapon(const UWeaponStateFlameThrower* FlameThrower)
{
	using DeveloperSettings::GameBalance::Weapons::FlameWeapons::FlameConeAngleUnit;

	WeaponDescription.WeaponDescriptionType = EWeaponDescriptionType::FlameWeapon;
	const FFlameThrowerSettings FlameSettings = FlameThrower->GetFlameSettings();
	// The FlameConeAngleUnit is for + and - Yaw so we multiply by 2 to get the full angle.
	WeaponDescription.FlameAngle = FlameSettings.ConeAngleMlt * FlameConeAngleUnit * 2;

	constexpr int32 BaseFlameRay = 1;
	constexpr int32 RaysPerMlt = 2;
	// one straight one down for each computed ray.
	constexpr int32 DifferentPitchRaysMlt = 2;

	int32 Rays = 0;
	if (FlameSettings.ConeAngleMlt > 1)
	{
		Rays = (BaseFlameRay + (FlameSettings.ConeAngleMlt - 1) * RaysPerMlt) * DifferentPitchRaysMlt;
	}
	else
	{
		Rays = BaseFlameRay * DifferentPitchRaysMlt;
	}
	WeaponDescription.FlameRays = Rays;
	WeaponDescription.DamageTicks = FlameSettings.FlameIterations;

	// Flame weapons do not use the ammo icon.
	SetAmmoIconVisibility(false);
}

void UW_WeaponDescription::SetToHoverLaserWeapon(const UWeaponStateLaser* LaserWeapon)
{
	WeaponDescription.DamageTicks = LaserWeapon->GetLaserTicks();
	WeaponDescription.WeaponDescriptionType = EWeaponDescriptionType::LaserWeapon;
	// todo laser cartridges?
	SetAmmoIconVisibility(false);
}
