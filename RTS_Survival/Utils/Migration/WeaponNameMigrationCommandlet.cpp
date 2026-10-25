#include "WeaponNameMigrationCommandlet.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "Components/ChildActorComponent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_SwitchEnum.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
// UE 5.5 location; before 5.5 this was Engine/UserDefinedStruct.h.
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/SavePackage.h"

namespace WeaponNameMigration
{
	const TCHAR* const TsvHeader =
		TEXT("AssetPackage\tKind\tContainer\tNodeClass\tNodeTitle\tNodeGuid\tPinOrPropertyPath\tDirection\tRawValue\tCanonicalValue\tNumericValue\tLinkSource\tLinkSourceNodeGuid\tLinkSourcePin");
	constexpr int32 TsvColumnCount = 14;
	// Loading every Blueprint without collecting runs the commandlet out of memory on this project.
	constexpr int32 AssetsPerGarbageCollect = 50;
	constexpr int32 AssetsPerProgressLog = 500;
	// Init structs nest at most one level (FInitWeaponStateRailgun); the margin covers user defined structs.
	constexpr int32 MaxStructSearchDepth = 4;
	// Guards the parent-class walk against a broken ParentClass tag that points back at a child.
	constexpr int32 MaxBlueprintInheritanceDepth = 32;
	// Before the migration every package that stores a weapon value imports the enum, so its name table holds this.
	const ANSICHAR* const WeaponEnumNameInPackage = "EWeaponName";
	const TCHAR* const WeaponNameFieldSuffix = TEXT("WeaponName");
	const TCHAR* const OrphanedPinMarker = TEXT(" [ORPHANED]");

	// After the migration a weapon field is an FName that may be called anything ("Weapon Name", "RailGunName").
	// Apply and Verify fill this from the snapshot so those fields are still recognised; Snapshot leaves it empty.
	TSet<FName> KnownWeaponFieldNames;

	struct FRow
	{
		FString AssetPackage;
		// Pin | StructPin | SwitchCase | Variable | LocalVariable | Property
		FString Kind;
		// Graph name for pins; CDO / CDOSub:<Path> / SCS:<Var> / ICH:<Name> / ChildActor:<Var> / Asset / Sub:<Path> /
		// Row:<RowName> / StructDefaults for properties.
		FString Container;
		FString NodeClass;
		FString NodeTitle;
		FString NodeGuid;
		FString PinOrPropertyPath;
		// In | Out for pins, "-" otherwise. Only unwired In pins hold a literal; Out rows show where a graph reads the value.
		FString Direction = TEXT("-");
		// Exactly as stored: "Kar_98k", "EWeaponName::KwK37_75MM", or a stale redirected name.
		FString RawValue;
		// Current enumerator short name after EnumRedirects. Empty means it could not be resolved; investigate.
		FString CanonicalValue;
		int64 NumericValue = INDEX_NONE;
		// Set when an input pin is wired: the value comes from another node, so there is no literal to restore.
		FString LinkSource;
		// Lets Apply reconnect a wire the migration broke; node GUIDs survive reconstruction.
		FString LinkSourceNodeGuid;
		FString LinkSourcePin;

		FString GetKey() const
		{
			return AssetPackage + TEXT("|") + Kind + TEXT("|") + Container + TEXT("|") + NodeGuid + TEXT("|") + PinOrPropertyPath;
		}
	};

	struct FApplyContext
	{
		const UEnum* WeaponEnum = nullptr;
		// True: do everything but save, so a spike run shows the FAILED lines without touching Content.
		bool bDryRun = false;
		int32 FailureCount = 0;
	};

	FString Sanitize(const FString& Value)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\t"), TEXT(" "));
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Result;
	}

	void ResolveValue(const UEnum* WeaponEnum, FRow& Row)
	{
		// GetValueByNameString accepts the short and the prefixed form and follows the ini EnumRedirects.
		Row.NumericValue = WeaponEnum->GetValueByNameString(Row.RawValue);
		if (Row.NumericValue == INDEX_NONE)
		{
			return;
		}
		Row.CanonicalValue = WeaponEnum->GetNameStringByValue(Row.NumericValue);
	}

	bool IsWeaponFieldName(const FString& FieldName)
	{
		return FieldName.EndsWith(WeaponNameFieldSuffix) || KnownWeaponFieldNames.Contains(FName(*FieldName));
	}

	bool IsWeaponNameProperty(const FProperty* Property, const UEnum* WeaponEnum)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum() == WeaponEnum;
		}
		// After the migration the Blueprint-facing fields and retyped variables are FName properties.
		return Property->IsA<FNameProperty>() && IsWeaponFieldName(Property->GetName());
	}

	bool StructHoldsWeaponName(const UStruct* Struct, const UEnum* WeaponEnum, const int32 Depth)
	{
		if (not Struct || Depth > MaxStructSearchDepth)
		{
			return false;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (IsWeaponNameProperty(*It, WeaponEnum))
			{
				return true;
			}
			const FStructProperty* StructProperty = CastField<FStructProperty>(*It);
			if (StructProperty && StructHoldsWeaponName(StructProperty->Struct, WeaponEnum, Depth + 1))
			{
				return true;
			}
		}
		return false;
	}

	bool IsWeaponNamePin(const UEdGraphPin* Pin, const UEnum* WeaponEnum)
	{
		if (Pin->PinType.PinSubCategoryObject == WeaponEnum)
		{
			return true;
		}
		// After the migration the struct, function and variable pins are FName pins that kept their names.
		return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name && IsWeaponFieldName(Pin->PinName.ToString());
	}

	bool IsWeaponStructType(const FEdGraphPinType& PinType, const UEnum* WeaponEnum)
	{
		if (PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			return false;
		}
		return StructHoldsWeaponName(Cast<UStruct>(PinType.PinSubCategoryObject.Get()), WeaponEnum, 0);
	}

	/**
	 * @brief Finds the WeaponName values inside an exported struct literal such as "(WeaponName=KwK37_75MM,Range=...)".
	 * Unsplit struct pins and struct-typed variables store their weapon only in this text.
	 * @return One value per WeaponName field, in text order; nested structs are included.
	 */
	TArray<FString> ExtractWeaponNamesFromStructText(const FString& StructText)
	{
		TArray<FString> Values;
		const FString FieldPrefix = FString(WeaponNameFieldSuffix) + TEXT("=");
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 FieldIndex = StructText.Find(FieldPrefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (FieldIndex == INDEX_NONE)
			{
				return Values;
			}
			SearchFrom = FieldIndex + FieldPrefix.Len();
			// Skip fields that only end in WeaponName, e.g. SecondaryWeaponName=.
			const TCHAR Preceding = FieldIndex > 0 ? StructText[FieldIndex - 1] : TEXT('(');
			if (Preceding != TEXT('(') && Preceding != TEXT(','))
			{
				continue;
			}
			int32 ValueEnd = SearchFrom;
			while (ValueEnd < StructText.Len() && StructText[ValueEnd] != TEXT(',') && StructText[ValueEnd] != TEXT(')'))
			{
				++ValueEnd;
			}
			FString Value = StructText.Mid(SearchFrom, ValueEnd - SearchFrom);
			Value.TrimQuotesInline();
			Values.Add(Value);
		}
	}

	void FillLinkSource(const UEdGraphPin* Pin, FRow& Row)
	{
		if (Pin->Direction != EGPD_Input || Pin->LinkedTo.IsEmpty())
		{
			return;
		}
		const UEdGraphPin* SourcePin = Pin->LinkedTo[0];
		const UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
		Row.LinkSource = Sanitize(SourceNode->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT(".")
			+ SourcePin->PinName.ToString());
		Row.LinkSourceNodeGuid = SourceNode->NodeGuid.ToString(EGuidFormats::Digits);
		Row.LinkSourcePin = SourcePin->PinName.ToString();
	}

	FRow MakeNodeRow(const UEdGraphNode* Node, const FString& Package, const FString& GraphName, const FString& Kind)
	{
		FRow Row;
		Row.AssetPackage = Package;
		Row.Kind = Kind;
		Row.Container = GraphName;
		Row.NodeClass = Node->GetClass()->GetName();
		Row.NodeTitle = Sanitize(Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		Row.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		return Row;
	}

	void CollectSwitchCases(const UK2Node_SwitchEnum* SwitchNode, const FString& Package, const FString& GraphName,
	                        TArray<FRow>& OutRows)
	{
		for (const UEdGraphPin* Pin : SwitchNode->Pins)
		{
			if (Pin->Direction != EGPD_Output || Pin->LinkedTo.IsEmpty())
			{
				continue;
			}
			FRow Row = MakeNodeRow(SwitchNode, Package, GraphName, TEXT("SwitchCase"));
			Row.PinOrPropertyPath = Pin->PinName.ToString();
			Row.RawValue = Pin->PinName.ToString();
			OutRows.Add(Row);
		}
	}

	// Unwired struct input pins (e.g. a Setup*Weapon call whose init struct is typed in) hold the weapon in their text.
	void CollectStructPinRows(const UEdGraphNode* Node, const UEdGraphPin* Pin, const FString& Package,
	                          const FString& GraphName, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		if (Pin->Direction != EGPD_Input || not Pin->LinkedTo.IsEmpty() || Pin->DefaultValue.IsEmpty())
		{
			return;
		}
		const TArray<FString> Values = ExtractWeaponNamesFromStructText(Pin->DefaultValue);
		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
		{
			FRow Row = MakeNodeRow(Node, Package, GraphName, TEXT("StructPin"));
			Row.PinOrPropertyPath = Pin->PinName.ToString() + TEXT(".WeaponName#") + FString::FromInt(ValueIndex);
			Row.Direction = TEXT("In");
			Row.RawValue = Values[ValueIndex];
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	void CollectNodeRows(const UEdGraphNode* Node, const FString& Package, const FString& GraphName,
	                     const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		const UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node);
		if (SwitchNode && SwitchNode->Enum == WeaponEnum)
		{
			CollectSwitchCases(SwitchNode, Package, GraphName, OutRows);
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (IsWeaponStructType(Pin->PinType, WeaponEnum))
			{
				CollectStructPinRows(Node, Pin, Package, GraphName, WeaponEnum, OutRows);
				continue;
			}
			if (not IsWeaponNamePin(Pin, WeaponEnum))
			{
				continue;
			}
			FRow Row = MakeNodeRow(Node, Package, GraphName, TEXT("Pin"));
			Row.PinOrPropertyPath = Pin->PinName.ToString() + (Pin->bOrphanedPin ? OrphanedPinMarker : TEXT(""));
			Row.Direction = Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out");
			Row.RawValue = Pin->DefaultValue;
			FillLinkSource(Pin, Row);
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	bool IsWeaponVariable(const FBPVariableDescription& Variable, const UEnum* WeaponEnum)
	{
		if (Variable.VarType.PinSubCategoryObject == WeaponEnum)
		{
			return true;
		}
		return Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_Name
			&& KnownWeaponFieldNames.Contains(Variable.VarName);
	}

	void CollectVariableRows(const TArray<FBPVariableDescription>& Variables, const FString& Package,
	                         const FString& Kind, const FString& Container, const UEnum* WeaponEnum,
	                         TArray<FRow>& OutRows)
	{
		for (const FBPVariableDescription& Variable : Variables)
		{
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = Kind;
			Row.Container = Container;
			if (IsWeaponVariable(Variable, WeaponEnum))
			{
				Row.PinOrPropertyPath = Variable.VarName.ToString();
				Row.RawValue = Variable.DefaultValue;
				ResolveValue(WeaponEnum, Row);
				OutRows.Add(Row);
				continue;
			}
			if (not IsWeaponStructType(Variable.VarType, WeaponEnum))
			{
				continue;
			}
			// Member variables also show up as CDO property rows; local struct variables only exist here.
			const TArray<FString> Values = ExtractWeaponNamesFromStructText(Variable.DefaultValue);
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				FRow StructRow = Row;
				StructRow.PinOrPropertyPath = Variable.VarName.ToString() + TEXT(".WeaponName#") + FString::FromInt(ValueIndex);
				StructRow.RawValue = Values[ValueIndex];
				ResolveValue(WeaponEnum, StructRow);
				OutRows.Add(StructRow);
			}
		}
	}

	bool TryReadWeaponProperty(const FProperty* Property, const void* Value, const UEnum* WeaponEnum, FString& OutRaw)
	{
		if (not IsWeaponNameProperty(Property, WeaponEnum))
		{
			return false;
		}
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			const int64 IntValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
			OutRaw = WeaponEnum->GetNameStringByValue(IntValue);
			return true;
		}
		OutRaw = CastField<FNameProperty>(Property)->GetPropertyValue(Value).ToString();
		return true;
	}

	void CollectPropertyRows(const UStruct* Struct, const void* Container, const FString& Package,
	                         const FString& ContainerLabel, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		if (not Struct || not Container)
		{
			return;
		}
		for (TPropertyValueIterator<const FProperty> It(Struct, Container); It; ++It)
		{
			FString RawValue;
			if (not TryReadWeaponProperty(It.Key(), It.Value(), WeaponEnum, RawValue))
			{
				continue;
			}
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = TEXT("Property");
			Row.Container = ContainerLabel;
			Row.PinOrPropertyPath = It.GetPropertyPathDebugString();
			Row.RawValue = RawValue;
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	// One UObject whose reflected properties can hold weapon values, with the label its rows use as Container.
	struct FPropertyContainer
	{
		FString Label;
		UObject* Object = nullptr;
	};

	// Instanced subobjects (e.g. the global abilities inside DA_GlobalAbilitySystem, C++ default subobject components
	// on a CDO) are separate UObjects, so the property walk of their owner does not reach them.
	void AddSubobjectContainers(UObject* Owner, const FString& LabelPrefix, TArray<FPropertyContainer>& OutContainers)
	{
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(Owner, Subobjects, true);
		for (UObject* Subobject : Subobjects)
		{
			if (Subobject)
			{
				OutContainers.Add({LabelPrefix + Subobject->GetPathName(Owner), Subobject});
			}
		}
	}

	void AddConstructionScriptContainers(const UBlueprint* Blueprint, TArray<FPropertyContainer>& OutContainers)
	{
		if (not Blueprint->SimpleConstructionScript)
		{
			return;
		}
		for (const USCS_Node* ScsNode : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			UActorComponent* Template = ScsNode ? ScsNode->ComponentTemplate.Get() : nullptr;
			if (not Template)
			{
				continue;
			}
			const FString VariableName = ScsNode->GetVariableName().ToString();
			OutContainers.Add({TEXT("SCS:") + VariableName, Template});
			const UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(Template);
			AActor* ChildTemplate = ChildActorComponent ? ChildActorComponent->GetChildActorTemplate() : nullptr;
			if (ChildTemplate)
			{
				OutContainers.Add({TEXT("ChildActor:") + VariableName, ChildTemplate});
			}
		}
	}

	void AddBlueprintContainers(UBlueprint* Blueprint, TArray<FPropertyContainer>& OutContainers)
	{
		if (UClass* GeneratedClass = Blueprint->GeneratedClass)
		{
			UObject* DefaultObject = GeneratedClass->GetDefaultObject();
			OutContainers.Add({TEXT("CDO"), DefaultObject});
			AddSubobjectContainers(DefaultObject, TEXT("CDOSub:"), OutContainers);
		}
		AddConstructionScriptContainers(Blueprint, OutContainers);
		const UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false);
		if (not Handler)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		Handler->GetAllTemplates(Templates);
		for (UActorComponent* Template : Templates)
		{
			OutContainers.Add({TEXT("ICH:") + Template->GetName(), Template});
		}
	}

	// Data tables and user defined structs hold their values in raw memory, not UObjects, and are handled separately.
	TArray<FPropertyContainer> GetPropertyContainers(UObject* Asset)
	{
		TArray<FPropertyContainer> Containers;
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			AddBlueprintContainers(Blueprint, Containers);
			return Containers;
		}
		if (not Asset || Asset->IsA<UDataTable>() || Asset->IsA<UUserDefinedStruct>())
		{
			return Containers;
		}
		Containers.Add({TEXT("Asset"), Asset});
		AddSubobjectContainers(Asset, TEXT("Sub:"), Containers);
		return Containers;
	}

	/**
	 * @param bRefreshNodes True after the migration: loading does not guarantee nodes were reconstructed against
	 * the new FName types, and reading the stale enum pins would report a false "verified".
	 */
	void CollectBlueprintGraphs(UBlueprint* Blueprint, const UEnum* WeaponEnum, const bool bRefreshNodes, TArray<FRow>& OutRows)
	{
		if (bRefreshNodes)
		{
			FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		}
		const FString Package = Blueprint->GetOutermost()->GetName();
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (not Graph)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (not Node)
				{
					continue;
				}
				CollectNodeRows(Node, Package, Graph->GetName(), WeaponEnum, OutRows);
				if (const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
				{
					CollectVariableRows(Entry->LocalVariables, Package, TEXT("LocalVariable"), Graph->GetName(),
					                    WeaponEnum, OutRows);
				}
			}
		}
		CollectVariableRows(Blueprint->NewVariables, Package, TEXT("Variable"), TEXT("Blueprint"), WeaponEnum, OutRows);
	}

	void CollectDataTable(const UDataTable* Table, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		const FString Package = Table->GetOutermost()->GetName();
		for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
		{
			CollectPropertyRows(Table->GetRowStruct(), RowPair.Value, Package,
			                    TEXT("Row:") + RowPair.Key.ToString(), WeaponEnum, OutRows);
		}
	}

	void CollectAsset(UObject* Asset, const UEnum* WeaponEnum, const bool bRefreshNodes, TArray<FRow>& OutRows)
	{
		if (not Asset)
		{
			return;
		}
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			CollectBlueprintGraphs(Blueprint, WeaponEnum, bRefreshNodes, OutRows);
		}
		else if (const UDataTable* Table = Cast<UDataTable>(Asset))
		{
			CollectDataTable(Table, WeaponEnum, OutRows);
		}
		else if (const UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Asset))
		{
			CollectPropertyRows(UserStruct, UserStruct->GetDefaultInstance(), UserStruct->GetOutermost()->GetName(),
			                    TEXT("StructDefaults"), WeaponEnum, OutRows);
		}
		const FString Package = Asset->GetOutermost()->GetName();
		for (const FPropertyContainer& Container : GetPropertyContainers(Asset))
		{
			CollectPropertyRows(Container.Object->GetClass(), Container.Object, Package, Container.Label, WeaponEnum, OutRows);
		}
	}

	// Cheap byte search so the snapshot only loads the few hundred packages that can hold a weapon value.
	bool PackageFileMentionsWeaponEnum(const FName PackageName)
	{
		FString Filename;
		if (not FPackageName::DoesPackageExist(PackageName.ToString(), &Filename))
		{
			return false;
		}
		TArray<uint8> Bytes;
		if (not FFileHelper::LoadFileToArray(Bytes, *Filename))
		{
			return false;
		}
		const int32 NeedleLength = FCStringAnsi::Strlen(WeaponEnumNameInPackage);
		const uint8* Needle = reinterpret_cast<const uint8*>(WeaponEnumNameInPackage);
		const int32 LastStart = Bytes.Num() - NeedleLength;
		for (int32 ByteIndex = 0; ByteIndex <= LastStart; ++ByteIndex)
		{
			if (Bytes[ByteIndex] == Needle[0] && FMemory::Memcmp(&Bytes[ByteIndex], Needle, NeedleLength) == 0)
			{
				return true;
			}
		}
		return false;
	}

	IAssetRegistry& GetAssetRegistry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	}

	TArray<FAssetData> GetCandidateAssets()
	{
		IAssetRegistry& AssetRegistry = GetAssetRegistry();
		AssetRegistry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		// bRecursiveClasses also picks up UWidgetBlueprint and other UBlueprint subclasses.
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UUserDefinedStruct::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);
		return Assets;
	}

	/**
	 * @brief Loads candidate assets and records their weapon values. Never saves: RefreshAllNodes only changes memory.
	 * @param OnlyPackages Packages to scan (Verify uses the snapshot's); null scans every package that names the enum.
	 */
	TArray<FRow> CollectAll(const UEnum* WeaponEnum, const bool bRefreshNodes, const TSet<FString>* OnlyPackages)
	{
		const TArray<FAssetData> Assets = GetCandidateAssets();
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration: %d candidate assets"), Assets.Num());
		TArray<FRow> Rows;
		int32 LoadedCount = 0;
		for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
		{
			if (AssetIndex > 0 && AssetIndex % AssetsPerProgressLog == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration: %d / %d scanned, %d loaded, %d rows"),
				       AssetIndex, Assets.Num(), LoadedCount, Rows.Num());
			}
			const FAssetData& AssetData = Assets[AssetIndex];
			const bool bIsCandidate = OnlyPackages
				? OnlyPackages->Contains(AssetData.PackageName.ToString())
				: PackageFileMentionsWeaponEnum(AssetData.PackageName);
			if (not bIsCandidate)
			{
				continue;
			}
			if (LoadedCount > 0 && LoadedCount % AssetsPerGarbageCollect == 0)
			{
				// RF_NoFlags: in an editor commandlet the default keep flags keep every loaded asset alive.
				CollectGarbage(RF_NoFlags);
			}
			++LoadedCount;
			CollectAsset(AssetData.GetAsset(), WeaponEnum, bRefreshNodes, Rows);
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration: loaded %d packages"), LoadedCount);
		return Rows;
	}

	bool WriteTsv(const TArray<FRow>& Rows, const FString& Path)
	{
		TArray<FString> Lines;
		Lines.Add(TsvHeader);
		for (const FRow& Row : Rows)
		{
			Lines.Add(FString::Join(TArray<FString>{
				Row.AssetPackage, Row.Kind, Row.Container, Row.NodeClass, Row.NodeTitle, Row.NodeGuid,
				Row.PinOrPropertyPath, Row.Direction, Sanitize(Row.RawValue), Row.CanonicalValue,
				LexToString(Row.NumericValue), Row.LinkSource, Row.LinkSourceNodeGuid, Row.LinkSourcePin}, TEXT("\t")));
		}
		return FFileHelper::SaveStringArrayToFile(Lines, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TArray<FRow> ReadTsv(const FString& Path)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *Path);
		TArray<FRow> Rows;
		for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
		{
			TArray<FString> Cells;
			Lines[LineIndex].ParseIntoArray(Cells, TEXT("\t"), false);
			if (Cells.Num() != TsvColumnCount)
			{
				UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration: malformed TSV line %d"), LineIndex + 1);
				continue;
			}
			FRow Row;
			Row.AssetPackage = Cells[0]; Row.Kind = Cells[1]; Row.Container = Cells[2]; Row.NodeClass = Cells[3];
			Row.NodeTitle = Cells[4]; Row.NodeGuid = Cells[5]; Row.PinOrPropertyPath = Cells[6];
			Row.Direction = Cells[7]; Row.RawValue = Cells[8]; Row.CanonicalValue = Cells[9];
			LexFromString(Row.NumericValue, *Cells[10]); Row.LinkSource = Cells[11];
			Row.LinkSourceNodeGuid = Cells[12]; Row.LinkSourcePin = Cells[13];
			Rows.Add(Row);
		}
		return Rows;
	}

	FString GetSnapshotPinName(const FRow& Row)
	{
		FString PinName = Row.PinOrPropertyPath;
		PinName.RemoveFromEnd(OrphanedPinMarker);
		return PinName;
	}

	// "Arr[2].WeaponName" -> "WeaponName", "Weapon Name" -> "Weapon Name".
	FString GetLastFieldName(const FString& PropertyPath)
	{
		FString FieldName = PropertyPath;
		int32 DotIndex = INDEX_NONE;
		if (FieldName.FindLastChar(TEXT('.'), DotIndex))
		{
			FieldName.RightChopInline(DotIndex + 1);
		}
		int32 BracketIndex = INDEX_NONE;
		if (FieldName.FindChar(TEXT('['), BracketIndex))
		{
			FieldName.LeftInline(BracketIndex);
		}
		return FieldName;
	}

	void RegisterKnownWeaponFields(const TArray<FRow>& Rows)
	{
		for (const FRow& Row : Rows)
		{
			const bool bIsPin = Row.Kind == TEXT("Pin");
			const FString FieldName = bIsPin ? GetSnapshotPinName(Row) : GetLastFieldName(Row.PinOrPropertyPath);
			if (bIsPin || Row.Kind == TEXT("Variable") || Row.Kind == TEXT("LocalVariable") || Row.Kind == TEXT("Property"))
			{
				KnownWeaponFieldNames.Add(FName(*FieldName));
			}
		}
	}

	UEdGraphNode* FindNodeByGuid(UBlueprint* Blueprint, const FString& GuidString)
	{
		FGuid Guid;
		if (not FGuid::Parse(GuidString, Guid))
		{
			return nullptr;
		}
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			const TObjectPtr<UEdGraphNode>* Found = Graph
				? Graph->Nodes.FindByPredicate([&Guid](const UEdGraphNode* Node) { return Node && Node->NodeGuid == Guid; })
				: nullptr;
			if (Found)
			{
				return *Found;
			}
		}
		return nullptr;
	}

	/**
	 * @brief Removes the orphan K2 leaves behind when a pin changes type, so it neither keeps the old enum value nor
	 * holds on to a wire. Links on the orphan are broken; Apply reconnects the live pin from the snapshot.
	 * @return Whether an orphan was removed, so the caller knows the Blueprint changed.
	 */
	bool RemoveOrphanedPin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* const* Orphan = Node->Pins.FindByPredicate([PinName](const UEdGraphPin* Pin)
		{
			return Pin->bOrphanedPin && Pin->PinName == PinName;
		});
		if (not Orphan)
		{
			return false;
		}
		Node->Modify();
		(*Orphan)->BreakAllPinLinks();
		Node->RemovePin(*Orphan);
		return true;
	}

	bool IsRestorableRow(const FRow& Row)
	{
		return Row.Kind == TEXT("Pin") && Row.Direction == TEXT("In") && Row.LinkSource.IsEmpty()
			&& not Row.CanonicalValue.IsEmpty();
	}

	bool IsWiredPinRow(const FRow& Row)
	{
		return Row.Kind == TEXT("Pin") && Row.Direction == TEXT("In") && not Row.LinkSourceNodeGuid.IsEmpty();
	}

	bool IsRestorablePropertyRow(const FRow& Row)
	{
		return Row.Kind == TEXT("Property") && not Row.CanonicalValue.IsEmpty();
	}

	UEdGraphPin* FindLivePin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* const* LivePin = Node->Pins.FindByPredicate([PinName](const UEdGraphPin* Pin)
		{
			return not Pin->bOrphanedPin && Pin->PinName == PinName;
		});
		return LivePin ? *LivePin : nullptr;
	}

	// Returns the migrated FName pin. A pin that is still the old enum pin means the node was not reconstructed
	// yet; writing to it would be undone by the next reconstruct, so reconstruct first and refuse if it stays stale.
	UEdGraphPin* FindMigratedPin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* LivePin = FindLivePin(Node, PinName);
		if (LivePin && LivePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			return LivePin;
		}
		Node->ReconstructNode();
		LivePin = FindLivePin(Node, PinName);
		if (LivePin && LivePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			return LivePin;
		}
		return nullptr;
	}

	void ReportFailure(FApplyContext& Context, const FString& Message)
	{
		++Context.FailureCount;
		UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: %s"), *Message);
	}

	// @return Whether the Blueprint was modified.
	bool ApplyPinLiteral(UBlueprint* Blueprint, const FRow& Row, FApplyContext& Context)
	{
		UEdGraphNode* Node = FindNodeByGuid(Blueprint, Row.NodeGuid);
		const FName PinName(*GetSnapshotPinName(Row));
		UEdGraphPin* MigratedPin = Node ? FindMigratedPin(Node, PinName) : nullptr;
		if (not MigratedPin)
		{
			ReportFailure(Context, FString::Printf(TEXT("no migrated Name pin %s on node %s (%s) in %s"),
				*PinName.ToString(), *Row.NodeTitle, *Row.NodeGuid, *Row.AssetPackage));
			return false;
		}
		const bool bRemovedOrphan = RemoveOrphanedPin(Node, PinName);
		if (MigratedPin->DefaultValue == Row.CanonicalValue)
		{
			return bRemovedOrphan;
		}
		Node->Modify();
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*MigratedPin, Row.CanonicalValue);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration restored %s.%s = %s in %s"),
		       *Row.NodeTitle, *PinName.ToString(), *Row.CanonicalValue, *Row.AssetPackage);
		return true;
	}

	/**
	 * @brief Reconnects a weapon pin to the node that fed it before the migration (usually a weapon variable getter).
	 * K2 may drop the wire while the two ends have different types during reconstruction.
	 * @return Whether the Blueprint was modified.
	 */
	bool RelinkWiredPin(UBlueprint* Blueprint, const FRow& Row, FApplyContext& Context)
	{
		const FName TargetPinName(*GetSnapshotPinName(Row));
		const FName SourcePinName(*Row.LinkSourcePin);
		UEdGraphNode* TargetNode = FindNodeByGuid(Blueprint, Row.NodeGuid);
		UEdGraphNode* SourceNode = FindNodeByGuid(Blueprint, Row.LinkSourceNodeGuid);
		UEdGraphPin* TargetPin = TargetNode ? FindLivePin(TargetNode, TargetPinName) : nullptr;
		UEdGraphPin* SourcePin = SourceNode ? FindLivePin(SourceNode, SourcePinName) : nullptr;
		if (not TargetPin || not SourcePin)
		{
			ReportFailure(Context, FString::Printf(TEXT("cannot find wire %s -> %s.%s (%s) in %s"),
				*Row.LinkSource, *Row.NodeTitle, *TargetPinName.ToString(), *Row.NodeGuid, *Row.AssetPackage));
			return false;
		}
		const bool bRemovedOrphans = RemoveOrphanedPin(TargetNode, TargetPinName) | RemoveOrphanedPin(SourceNode, SourcePinName);
		if (TargetPin->LinkedTo.Contains(SourcePin))
		{
			return bRemovedOrphans;
		}
		if (not GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(SourcePin, TargetPin))
		{
			ReportFailure(Context, FString::Printf(TEXT("cannot reconnect %s -> %s.%s in %s (types %s -> %s)"),
				*Row.LinkSource, *Row.NodeTitle, *TargetPinName.ToString(), *Row.AssetPackage,
				*SourcePin->PinType.PinCategory.ToString(), *TargetPin->PinType.PinCategory.ToString()));
			return bRemovedOrphans;
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration relinked %s -> %s.%s in %s"),
		       *Row.LinkSource, *Row.NodeTitle, *TargetPinName.ToString(), *Row.AssetPackage);
		return true;
	}

	// Keeps the container type, so an array of weapons becomes an array of names.
	FEdGraphPinType MakeNamePinType(const FEdGraphPinType& OldPinType)
	{
		FEdGraphPinType NamePinType = OldPinType;
		NamePinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		NamePinType.PinSubCategory = NAME_None;
		NamePinType.PinSubCategoryObject = nullptr;
		return NamePinType;
	}

	/**
	 * @brief Retypes an EWeaponName Blueprint member variable to Name. Done by hand instead of through
	 * ChangeMemberVariableType, which asks for confirmation in a dialog; the CDO value is restored afterwards.
	 * @return Whether the variable was retyped, which requires a structural recompile.
	 */
	bool RetypeMemberVariable(UBlueprint* Blueprint, const FRow& Row, FApplyContext& Context)
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*Row.PinOrPropertyPath));
		if (VariableIndex == INDEX_NONE)
		{
			ReportFailure(Context, FString::Printf(TEXT("variable %s not found in %s"), *Row.PinOrPropertyPath, *Row.AssetPackage));
			return false;
		}
		FBPVariableDescription& Variable = Blueprint->NewVariables[VariableIndex];
		if (Variable.VarType.PinSubCategoryObject != Context.WeaponEnum)
		{
			return false;
		}
		Blueprint->Modify();
		Variable.VarType = MakeNamePinType(Variable.VarType);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration retyped variable %s to Name in %s"),
		       *Row.PinOrPropertyPath, *Row.AssetPackage);
		return true;
	}

	UK2Node_FunctionEntry* FindFunctionEntry(UBlueprint* Blueprint, const FString& GraphName)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (not Graph || Graph->GetName() != GraphName)
			{
				continue;
			}
			const TObjectPtr<UEdGraphNode>* Entry = Graph->Nodes.FindByPredicate([](const UEdGraphNode* Node)
			{
				return Node && Node->IsA<UK2Node_FunctionEntry>();
			});
			return Entry ? Cast<UK2Node_FunctionEntry>(*Entry) : nullptr;
		}
		return nullptr;
	}

	// Local variable defaults live on the description itself, so the snapshot value is written there directly.
	bool RetypeLocalVariable(UBlueprint* Blueprint, const FRow& Row, FApplyContext& Context)
	{
		UK2Node_FunctionEntry* Entry = FindFunctionEntry(Blueprint, Row.Container);
		const FName VariableName(*Row.PinOrPropertyPath);
		FBPVariableDescription* Variable = Entry
			? Entry->LocalVariables.FindByPredicate([VariableName](const FBPVariableDescription& Local) { return Local.VarName == VariableName; })
			: nullptr;
		if (not Variable)
		{
			ReportFailure(Context, FString::Printf(TEXT("local variable %s not found in %s.%s"),
				*Row.PinOrPropertyPath, *Row.AssetPackage, *Row.Container));
			return false;
		}
		if (Variable->VarType.PinSubCategoryObject != Context.WeaponEnum)
		{
			return false;
		}
		Entry->Modify();
		Variable->VarType = MakeNamePinType(Variable->VarType);
		Variable->DefaultValue = Row.CanonicalValue;
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration retyped local variable %s.%s to Name = %s in %s"),
		       *Row.Container, *Row.PinOrPropertyPath, *Row.CanonicalValue, *Row.AssetPackage);
		return true;
	}

	// @return Whether the variables changed type, which requires a structural recompile before nodes are refreshed.
	bool RetypeWeaponVariables(UBlueprint* Blueprint, const TArray<const FRow*>& Rows, FApplyContext& Context)
	{
		bool bAnyRetyped = false;
		for (const FRow* Row : Rows)
		{
			if (Row->Kind == TEXT("Variable"))
			{
				bAnyRetyped |= RetypeMemberVariable(Blueprint, *Row, Context);
			}
			else if (Row->Kind == TEXT("LocalVariable"))
			{
				bAnyRetyped |= RetypeLocalVariable(Blueprint, *Row, Context);
			}
		}
		return bAnyRetyped;
	}

	bool HasGraphRows(const TArray<const FRow*>& Rows)
	{
		return Rows.ContainsByPredicate([](const FRow* Row)
		{
			return Row->Kind == TEXT("Variable") || Row->Kind == TEXT("LocalVariable") || IsRestorableRow(*Row)
				|| IsWiredPinRow(*Row);
		});
	}

	/**
	 * @brief Retypes weapon variables, rebuilds every node against the new types, then restores pin literals and wires.
	 * Always compiles when the Blueprint has graph rows, so the saved asset holds the migrated pins (outcome A included).
	 * @return Whether the Blueprint must be saved.
	 */
	bool ApplyBlueprintGraphRows(UBlueprint* Blueprint, const TArray<const FRow*>& Rows, FApplyContext& Context)
	{
		if (not HasGraphRows(Rows))
		{
			return false;
		}
		if (RetypeWeaponVariables(Blueprint, Rows, Context))
		{
			// Regenerates the skeleton class so the variable getters reconstruct with Name pins.
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		for (const FRow* Row : Rows)
		{
			if (IsRestorableRow(*Row))
			{
				ApplyPinLiteral(Blueprint, *Row, Context);
			}
			else if (IsWiredPinRow(*Row))
			{
				RelinkWiredPin(Blueprint, *Row, Context);
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return true;
	}

	// @return Whether the value differed and was written.
	bool WriteWeaponPropertyValue(const FProperty* Property, void* Value, const FRow& Row, const UEnum* WeaponEnum)
	{
		if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			const FName SnapshotName(*Row.CanonicalValue);
			if (NameProperty->GetPropertyValue(Value) == SnapshotName)
			{
				return false;
			}
			NameProperty->SetPropertyValue(Value, SnapshotName);
			return true;
		}
		const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
		const int64 SnapshotValue = WeaponEnum->GetValueByNameString(Row.CanonicalValue);
		if (not EnumProperty || SnapshotValue == INDEX_NONE
			|| EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value) == SnapshotValue)
		{
			return false;
		}
		EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(Value, SnapshotValue);
		return true;
	}

	/**
	 * @brief Writes the snapshot values back onto one container, matched by the same label and property path the
	 * snapshot recorded. Retyped variables and C++ fields lose their value when the property type changes.
	 * @return Whether any value was written.
	 */
	bool RestoreContainerProperties(const FPropertyContainer& Container, const TMap<FString, const FRow*>& RowsByPath,
	                                const UEnum* WeaponEnum)
	{
		bool bAnyWritten = false;
		for (TPropertyValueIterator<const FProperty> It(Container.Object->GetClass(), Container.Object); It; ++It)
		{
			if (not IsWeaponNameProperty(It.Key(), WeaponEnum))
			{
				continue;
			}
			const FRow* const* SnapshotRow = RowsByPath.Find(Container.Label + TEXT("|") + It.GetPropertyPathDebugString());
			if (not SnapshotRow)
			{
				continue;
			}
			if (not bAnyWritten)
			{
				Container.Object->Modify();
			}
			// The iterator only hands out const pointers; the container itself is a mutable object we own here.
			if (WriteWeaponPropertyValue(It.Key(), const_cast<void*>(It.Value()), **SnapshotRow, WeaponEnum))
			{
				bAnyWritten = true;
				UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration restored property %s %s = %s in %s"),
				       *Container.Label, *(*SnapshotRow)->PinOrPropertyPath, *(*SnapshotRow)->CanonicalValue,
				       *(*SnapshotRow)->AssetPackage);
			}
		}
		return bAnyWritten;
	}

	// @return Whether the asset must be saved.
	bool RestorePropertyRows(UObject* Asset, const TArray<const FRow*>& Rows, FApplyContext& Context)
	{
		TMap<FString, const FRow*> RowsByPath;
		for (const FRow* Row : Rows)
		{
			if (IsRestorablePropertyRow(*Row))
			{
				RowsByPath.Add(Row->Container + TEXT("|") + Row->PinOrPropertyPath, Row);
			}
		}
		if (RowsByPath.IsEmpty())
		{
			return false;
		}
		bool bAnyWritten = false;
		for (const FPropertyContainer& Container : GetPropertyContainers(Asset))
		{
			bAnyWritten |= RestoreContainerProperties(Container, RowsByPath, Context.WeaponEnum);
		}
		return bAnyWritten;
	}

	bool SavePackageToDisk(UPackage* Package)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);
	}

	void ApplyPackage(const FString& Package, const TArray<const FRow*>& Rows, FApplyContext& Context)
	{
		const FString ObjectPath = Package + TEXT(".") + FPackageName::GetShortName(Package);
		UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
		if (not Asset)
		{
			ReportFailure(Context, FString::Printf(TEXT("could not load %s"), *ObjectPath));
			return;
		}
		bool bMustSave = false;
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			bMustSave = ApplyBlueprintGraphRows(Blueprint, Rows, Context);
		}
		// After the compile: compiling rebuilds the CDO, and the restored values must land on the new one.
		bMustSave |= RestorePropertyRows(Asset, Rows, Context);
		if (not bMustSave)
		{
			return;
		}
		if (Context.bDryRun)
		{
			UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration DRYRUN would save %s"), *Package);
			return;
		}
		if (not SavePackageToDisk(Asset->GetOutermost()))
		{
			ReportFailure(Context, FString::Printf(TEXT("could not save %s"), *Package));
		}
	}

	/**
	 * @brief Counts Blueprint parents inside /Game using the ParentClass asset registry tag, without loading anything.
	 * Apply runs parents first so a child compiles against its parent's retyped variables.
	 */
	int32 GetBlueprintDepth(const FString& Package, IAssetRegistry& AssetRegistry, TMap<FString, int32>& DepthCache,
	                        const int32 Guard)
	{
		if (const int32* CachedDepth = DepthCache.Find(Package))
		{
			return *CachedDepth;
		}
		TArray<FAssetData> PackageAssets;
		AssetRegistry.GetAssetsByPackageName(FName(*Package), PackageAssets);
		int32 Depth = 0;
		for (const FAssetData& AssetData : PackageAssets)
		{
			FString ParentClassExportPath;
			if (not AssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassExportPath))
			{
				continue;
			}
			const FString ParentPackage = FPackageName::ObjectPathToPackageName(
				FPackageName::ExportTextPathToObjectPath(ParentClassExportPath));
			if (ParentPackage.StartsWith(TEXT("/Game/")) && Guard < MaxBlueprintInheritanceDepth)
			{
				Depth = 1 + GetBlueprintDepth(ParentPackage, AssetRegistry, DepthCache, Guard + 1);
			}
		}
		DepthCache.Add(Package, Depth);
		return Depth;
	}

	TArray<FString> GetPackagesParentsFirst(const TMap<FString, TArray<const FRow*>>& RowsByPackage)
	{
		IAssetRegistry& AssetRegistry = GetAssetRegistry();
		AssetRegistry.SearchAllAssets(true);
		TMap<FString, int32> DepthCache;
		TArray<FString> Packages;
		RowsByPackage.GetKeys(Packages);
		for (const FString& Package : Packages)
		{
			GetBlueprintDepth(Package, AssetRegistry, DepthCache, 0);
		}
		Packages.Sort([&DepthCache](const FString& Left, const FString& Right)
		{
			const int32 LeftDepth = DepthCache.FindRef(Left);
			const int32 RightDepth = DepthCache.FindRef(Right);
			return LeftDepth != RightDepth ? LeftDepth < RightDepth : Left < Right;
		});
		return Packages;
	}

	bool IsApplicableRow(const FRow& Row)
	{
		return IsRestorableRow(Row) || IsWiredPinRow(Row) || Row.Kind == TEXT("Variable")
			|| Row.Kind == TEXT("LocalVariable") || IsRestorablePropertyRow(Row);
	}

	int32 RunApply(const UEnum* WeaponEnum, const FString& TsvPath, const bool bDryRun)
	{
		const TArray<FRow> Rows = ReadTsv(TsvPath);
		RegisterKnownWeaponFields(Rows);
		FApplyContext Context;
		Context.WeaponEnum = WeaponEnum;
		Context.bDryRun = bDryRun;
		TMap<FString, TArray<const FRow*>> RowsByPackage;
		for (const FRow& Row : Rows)
		{
			if (Row.Container.StartsWith(TEXT("Row:")) || Row.Container == TEXT("StructDefaults"))
			{
				ReportFailure(Context, FString::Printf(TEXT("data table / struct default rows are not restored: %s %s"),
					*Row.AssetPackage, *Row.Container));
				continue;
			}
			if (IsApplicableRow(Row))
			{
				RowsByPackage.FindOrAdd(Row.AssetPackage).Add(&Row);
			}
		}
		for (const FString& Package : GetPackagesParentsFirst(RowsByPackage))
		{
			ApplyPackage(Package, RowsByPackage[Package], Context);
			// Save per package and collect garbage so a failure late in the run does not lose earlier work.
			CollectGarbage(RF_NoFlags);
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration apply: %d packages, %d failures%s"),
		       RowsByPackage.Num(), Context.FailureCount, bDryRun ? TEXT(" (dry run, nothing saved)") : TEXT(""));
		return Context.FailureCount == 0 ? 0 : 1;
	}

	int32 RunVerify(const UEnum* WeaponEnum, const FString& TsvPath)
	{
		const TArray<FRow> SnapshotRows = ReadTsv(TsvPath);
		RegisterKnownWeaponFields(SnapshotRows);
		TSet<FString> SnapshotPackages;
		for (const FRow& Row : SnapshotRows)
		{
			SnapshotPackages.Add(Row.AssetPackage);
		}
		TMap<FString, FRow> AfterByKey;
		for (const FRow& Row : CollectAll(WeaponEnum, true, &SnapshotPackages))
		{
			FRow Normalised = Row;
			Normalised.PinOrPropertyPath.RemoveFromEnd(OrphanedPinMarker);
			AfterByKey.Add(Normalised.GetKey(), Normalised);
		}
		int32 ProblemCount = 0;
		for (FRow Before : SnapshotRows)
		{
			Before.PinOrPropertyPath.RemoveFromEnd(OrphanedPinMarker);
			const FRow* After = AfterByKey.Find(Before.GetKey());
			if (not After)
			{
				UE_LOG(LogTemp, Warning, TEXT("VERIFY MISSING  %s  %s  %s = %s"), *Before.AssetPackage,
				       *Before.NodeTitle, *Before.PinOrPropertyPath, *Before.CanonicalValue);
				++ProblemCount;
				continue;
			}
			// Only stored values count: a Variable row keeps its value on the CDO, an Out pin has none, and a wired
			// In pin's literal is an unused leftover. Those rows only have to be found and keep their wire.
			const bool bHoldsStoredValue = Before.Kind != TEXT("Variable") && Before.Direction != TEXT("Out")
				&& Before.LinkSource.IsEmpty();
			const bool bValueChanged = bHoldsStoredValue && After->CanonicalValue != Before.CanonicalValue;
			if (bValueChanged || After->LinkSourceNodeGuid != Before.LinkSourceNodeGuid)
			{
				UE_LOG(LogTemp, Warning, TEXT("VERIFY CHANGED  %s  %s  %s : %s <- %s  ->  %s <- %s"), *Before.AssetPackage,
				       *Before.NodeTitle, *Before.PinOrPropertyPath, *Before.CanonicalValue, *Before.LinkSource,
				       *After->CanonicalValue, *After->LinkSource);
				++ProblemCount;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration verify: %d problem rows"), ProblemCount);
		return ProblemCount == 0 ? 0 : 1;
	}
}
#endif

UWeaponNameMigrationCommandlet::UWeaponNameMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWeaponNameMigrationCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WeaponNameMigration;
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	FString Mode;
	FString TsvPath;
	if (not WeaponEnum || not FParse::Value(*Params, TEXT("Mode="), Mode) || not FParse::Value(*Params, TEXT("Tsv="), TsvPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Usage: -run=WeaponNameMigration -Mode=Snapshot|Apply|Verify -Tsv=<path> [-DryRun]"));
		return 1;
	}
	if (Mode == TEXT("Snapshot"))
	{
		// No refresh: before the migration the loaded graphs are exactly what is on disk.
		const TArray<FRow> Rows = CollectAll(WeaponEnum, false, nullptr);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration snapshot: %d rows"), Rows.Num());
		return WriteTsv(Rows, TsvPath) ? 0 : 1;
	}
	if (Mode == TEXT("Apply"))
	{
		return RunApply(WeaponEnum, TsvPath, FParse::Param(*Params, TEXT("DryRun")));
	}
	if (Mode == TEXT("Verify"))
	{
		return RunVerify(WeaponEnum, TsvPath);
	}
	UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration: unknown mode %s"), *Mode);
	return 1;
#else
	return 1;
#endif
}
