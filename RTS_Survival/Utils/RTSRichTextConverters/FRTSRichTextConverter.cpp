#include "FRTSRichTextConverter.h"

#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

ERTSResourceRichText FRTSRichTextConverter::ConvertResourceToRichTextType(const ERTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case ERTSResourceType::Resource_Radixite:
		return ERTSResourceRichText::Radixite;
	case ERTSResourceType::Resource_Metal:
		return ERTSResourceRichText::Metal;
	case ERTSResourceType::Resource_VehicleParts:
		return ERTSResourceRichText::Vehicle;
	case ERTSResourceType::Resource_Fuel:
		return ERTSResourceRichText::Yellow;
	case ERTSResourceType::Resource_Ammo:
		return ERTSResourceRichText::Ammo;
	case ERTSResourceType::Blueprint_Weapon:
	case ERTSResourceType::Blueprint_Vehicle:
	case ERTSResourceType::Blueprint_Energy:
	case ERTSResourceType::Blueprint_Construction:
		return ERTSResourceRichText::Blueprint;
	default:
		return ERTSResourceRichText::Red;
	}
}

ERTSRichText FRTSRichTextConverter::ConvertResourceToRTSRichText(const ERTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case ERTSResourceType::Resource_None:
		break;
	case ERTSResourceType::Resource_Radixite:
		return ERTSRichText::Text_Radixite;
	case ERTSResourceType::Resource_Metal:
		return ERTSRichText::Text_Metal;
	case ERTSResourceType::Resource_VehicleParts:
		return ERTSRichText::Text_Vehicle;
	case ERTSResourceType::Resource_Fuel:
		break;
	case ERTSResourceType::Resource_Ammo:
		break;
	case ERTSResourceType::Blueprint_Weapon:
		break;
	case ERTSResourceType::Blueprint_Vehicle:
		break;
	case ERTSResourceType::Blueprint_Energy:
		break;
	case ERTSResourceType::Blueprint_Construction:
		break;
	}
	return ERTSRichText::Text_SubTitle;
}

EDamageText FRTSRichTextConverter::ConvertDamageTypeToDamageText(const ERTSDamageType DamageType)
{
	switch (DamageType)
	{
	case ERTSDamageType::Kinetic: return EDamageText::Text_Kinetic;
	case ERTSDamageType::Fire: return EDamageText::Text_Fire;
	case ERTSDamageType::Laser: return EDamageText::Text_Laser;
	case ERTSDamageType::Radiation: return EDamageText::Text_Radiation;
	default: ;
	}
	RTSFunctionLibrary::ReportError(
		"Could not convert: " + FString::FromInt(static_cast<int>(DamageType)) + " to EDamageText.");
	return EDamageText::Text_Kinetic;
}

FString FRTSRichTextConverter::MakeStringRTSResource(const FString& InString, const ERTSResourceRichText RichTextType)
{
	return GetResourceRichTextTag(RichTextType) + InString + "</>";
}

FString FRTSRichTextConverter::MakeRTSRich(const FString& InString, const ERTSRichText RichTextType)
{
	return GetRTSRichTextTag(RichTextType) + InString + "</>";
}

FText FRTSRichTextConverter::MakeRTSRichText(const FString& InString, const ERTSRichText RichTextType)
{
	return FText::FromString(GetRTSRichTextTag(RichTextType) + InString + "</>");
}

FString FRTSRichTextConverter::MakeStringDamageType(const FString& InString, const EDamageText DamageTextType)
{
	return GetDamageTextTag(DamageTextType) + InString + "</>";
}

FString FRTSRichTextConverter::MakeDamageTypeString_ForWeaponDmgType(const FString& InString,
                                                                     const ERTSDamageType DamageType)
{
	const EDamageText DamageTextType = ConvertDamageTypeToDamageText(DamageType);
	return MakeStringDamageType(InString, DamageTextType);
}


FString FRTSRichTextConverter::GetResourceRichImage(const ERTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case ERTSResourceType::Resource_Radixite:
		return "<img id=\"Radixite\"/>";
	case ERTSResourceType::Resource_Metal:
		return "<img id=\"Metal\"/>";
	case ERTSResourceType::Resource_VehicleParts:
		return "<img id=\"Vehicle\"/>";
	case ERTSResourceType::Resource_Ammo:
		return "<img id=\"Ammo\"/>";
	case ERTSResourceType::Resource_Fuel:
		return "<img id=\"Fuel\"/>";
	case ERTSResourceType::Blueprint_Vehicle:
		return "<img id=\"VehicleBlueprint\"/>";
	case ERTSResourceType::Blueprint_Weapon:
		return "<img id=\"WeaponBlueprint\"/>";
	case ERTSResourceType::Blueprint_Construction:
		return "<img id=\"ConstructionBlueprint\"/>";
	case ERTSResourceType::Blueprint_Energy:
		return "<img id=\"EnergyBlueprint\"/>";
	default:
		return "";
	}
}
