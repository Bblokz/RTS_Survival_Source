#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GetTargetResourceThread/FGetAsyncResource.h"

enum class ERTSDamageType : uint8;
enum class ERTSResourceType : uint8;
// Defines a rich text type to use with the RTS rich Text converter.
UENUM(BlueprintType)
enum class ERTSResourceRichText : uint8
{
	Radixite,
	Red,
	Metal,
	Yellow,
	Vehicle,
	Fuel,
	Ammo,
	Blueprint,
	DisplayAmount,
};


UENUM(BlueprintType)
enum class EDamageText : uint8
{
	Text_Kinetic,
	Text_Fire,
	Text_Laser,
	Text_Radiation
};


UENUM(BlueprintType)
enum class ERTSRichText : uint8
{
	Text_Title,
	Text_SubTitle,
	Text_Bad,
	Text_Accent,
	Text_Energy,
	Text_Radixite,
	Text_Armor,
	Text_Scavenging,
	Text_Vehicle,
	Text_Metal,
	Text_Blueprint,
	Text_Popup,
	Text_DescriptionHelper,
	Text_Weapon,
	Text_New,
	Text_Error,
	Text_GoodTitle,
	Text_BadTitle,
	Text_Bad14,
	Text_NewBad,
	Text_NewGood,
	Text_Exp,
	Text_Seats,
	Text_Cursive,
};


class RTS_SURVIVAL_API FRTSRichTextConverter
{
public:
	static ERTSResourceRichText ConvertResourceToRichTextType(const ERTSResourceType ResourceType);
	static ERTSRichText ConvertResourceToRTSRichText(const ERTSResourceType ResourceType);
	static EDamageText ConvertDamageTypeToDamageText(const ERTSDamageType DamageType);

	// Adds <[ERTSResourceText]> </> tags to the string.
	static FString MakeStringRTSResource(const FString& InString, const ERTSResourceRichText RichTextType);

	// adds <[ERTSRichText]? </> tags to the string.
	static FString MakeRTSRich(const FString& InString, const ERTSRichText RichTextType);
	static FText MakeRTSRichText(const FString& InString, const ERTSRichText RichTextType);

	static FString MakeStringDamageType(const FString& InString, const EDamageText DamageTextType);
	static FString MakeDamageTypeString_ForWeaponDmgType(const FString& InString, const ERTSDamageType DamageType );

	/** @return The image id link for the resource type.
	 * @note Assumes that the proper rich text decorator was setup in blueprints.*/
	static FString GetResourceRichImage(const ERTSResourceType ResourceType);

private:
	inline static FString GetResourceRichTextTag(const ERTSResourceRichText RichTextType)
	{
		switch (RichTextType)
		{
		case ERTSResourceRichText::Radixite:
			return "<Radixite>";
		case ERTSResourceRichText::Red:
			return "<Red>";
		case ERTSResourceRichText::Metal:
			return "<Metal>";
		case ERTSResourceRichText::Yellow:
			return "<Yellow>";
		case ERTSResourceRichText::Vehicle:
			return "<Vehicle>";
		case ERTSResourceRichText::Fuel:
			return "<Fuel>";
		case ERTSResourceRichText::Ammo:
			return "<Ammo>";
		case ERTSResourceRichText::Blueprint:
			return "<Blueprint>";
		case ERTSResourceRichText::DisplayAmount:
			return "<DisplayAmount>";
		default:
			return "";
		}
	}
	inline static FString GetDamageTextTag(const EDamageText DamageTextType)
	{
		switch (DamageTextType)
		{
		case EDamageText::Text_Kinetic:
			return "<Text_Kinetic>";
		case EDamageText::Text_Fire:
			return "<Text_Fire>";
		case EDamageText::Text_Laser:
			return "<Text_Laser>";
		case EDamageText::Text_Radiation:
			return "<Text_Radiation>";
		default:
			return "";
		}
	}

	inline static FString GetRTSRichTextTag(const ERTSRichText RichTextType)
	{
		switch (RichTextType)
		{
		case ERTSRichText::Text_Title:
			return "<Text_Title>";
		case ERTSRichText::Text_SubTitle:
			return "<Text_SubTitle>";
		case ERTSRichText::Text_Bad:
			return "<Text_Bad>";
		case ERTSRichText::Text_Accent:
			return "<Text_Accent>";
		case ERTSRichText::Text_Energy:
			return "<Text_Energy>";
		case ERTSRichText::Text_Radixite:
			return "<Text_Radixite>";
		case ERTSRichText::Text_Armor:
			return "<Text_Armor>";
		case ERTSRichText::Text_Scavenging:
			return "<Text_Scavenging>";
		case ERTSRichText::Text_Vehicle:
			return "<Text_Vehicle>";
		case ERTSRichText::Text_Metal:
			return "<Text_Metal>";
		case ERTSRichText::Text_Blueprint:
			return "<Text_Blueprint>";
		case ERTSRichText::Text_Popup:
			return "<Text_Popup>";
		case ERTSRichText::Text_DescriptionHelper:
			return "<Text_DescriptionHelper>";
		case ERTSRichText::Text_Weapon:
			return "<Text_Weapon>";
		case ERTSRichText::Text_New:
			return "<Text_New>";
		case ERTSRichText::Text_Error:
			return "<Text_Error>";
		case ERTSRichText::Text_GoodTitle:
			return "<Text_GoodTitle>";
		case ERTSRichText::Text_BadTitle:
			return "<Text_BadTitle>";
		case ERTSRichText::Text_Bad14:
			return "<Text_Bad14>";
		case ERTSRichText::Text_NewBad:
			return "<Text_NewBad>";
		case ERTSRichText::Text_NewGood:
			return "<Text_NewGood>";
		case ERTSRichText::Text_Exp:
			return "<Text_Exp>";
			case ERTSRichText::Text_Seats:
				return "<Text_Seats>";
		case ERTSRichText::Text_Cursive:
			return "<Text_Cursive>";
		default:
			return "";
		}
	}
};
