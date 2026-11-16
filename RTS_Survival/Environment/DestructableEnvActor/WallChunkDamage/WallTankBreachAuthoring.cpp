// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only setup of the Rus concrete walls for field holes and tank breaches (Docs/Environment/WallChunkDamage.md).
// RTS.WallBreach.ConfigureRusWall Blueprint=<wall BP> Materials=<mi0>|<mi1> [Path=x,y;x,y;x,y] [Thickness=cm]
//     [NavArea=<nav area class>] [NavBlocker=<child actor class>] [Dust=<niagara>] [Sound=<sound>]
//     [Attenuation=<att>] [Concurrency=<conc>] [HeavyOnly=1] [Unowned=1] [KillOnFall=1] [NavHalfDepth=cm]
//   Also used for the Berlin ruin walls (B1-B6): heavy tanks only, unowned, tall pieces die whole.
//   Base walls: hole component to field mode with these materials, a WallTankBreach component, nav that lets medium and
//   heavy tanks through. Child walls (_Soviet): only their own damaged materials (the rest is inherited).

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NavAreas/NavArea.h"
#include "NavModifierComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WallHoleDamageComponent.h"
#include "WallTankBreachComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallBreachAuthoring, Log, All);

namespace WallTankBreachAuthoring
{
	const FName BreachComponentName = TEXT("WallTankBreach");

	template <typename TAsset>
	TAsset* LoadByPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		const FString ObjectPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
		return LoadObject<TAsset>(nullptr, *ObjectPath);
	}

	UClass* LoadClassByPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (Path.StartsWith(TEXT("/Script/")))
		{
			return LoadObject<UClass>(nullptr, *Path);
		}
		const FString ClassPath = Path.EndsWith(TEXT("_C"))
			                          ? Path
			                          : Path + TEXT(".") + FPackageName::GetShortName(Path) + TEXT("_C");
		return LoadObject<UClass>(nullptr, *ClassPath);
	}

	FString GetArg(const FString& Line, const TCHAR* Key)
	{
		FString Value;
		FParse::Value(*Line, *(FString(Key) + TEXT("=")), Value, false);
		return Value;
	}

	TArray<FVector2D> ParsePath(const FString& Text)
	{
		TArray<FVector2D> Points;
		TArray<FString> PointTexts;
		Text.ParseIntoArray(PointTexts, TEXT(";"));
		for (const FString& PointText : PointTexts)
		{
			FString X;
			FString Y;
			if (PointText.Split(TEXT(","), &X, &Y))
			{
				Points.Add(FVector2D(FCString::Atod(*X), FCString::Atod(*Y)));
			}
		}
		return Points;
	}

	bool SaveAsset(UObject& Asset)
	{
		UPackage* const Package = Asset.GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                                                 FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *FileName, SaveArgs);
	}

	USCS_Node* FindNodeOfClass(const UBlueprint& Blueprint, const UClass* ComponentClass, bool& bOutIsInherited)
	{
		bOutIsInherited = false;
		for (const UBlueprint* Current = &Blueprint; Current != nullptr;
		     Current = UBlueprint::GetBlueprintFromClass(Current->ParentClass))
		{
			if (Current->SimpleConstructionScript != nullptr)
			{
				for (USCS_Node* const Node : Current->SimpleConstructionScript->GetAllNodes())
				{
					if (Node->ComponentClass != nullptr && Node->ComponentClass->IsChildOf(ComponentClass))
					{
						return Node;
					}
				}
			}
			bOutIsInherited = true;
		}
		return nullptr;
	}

	/** @return The template this Blueprint owns for the node: its own, or an override of an inherited one. */
	UActorComponent* GetEditableTemplate(UBlueprint& Blueprint, USCS_Node* const Node, const bool bIsInherited)
	{
		if (Node == nullptr)
		{
			return nullptr;
		}
		if (not bIsInherited)
		{
			return Node->ComponentTemplate;
		}
		UInheritableComponentHandler* const Handler = Blueprint.GetInheritableComponentHandler(true);
		const FComponentKey Key(Node);
		UActorComponent* const Override = Handler->GetOverridenComponentTemplate(Key);
		return Override != nullptr ? Override : Handler->CreateOverridenComponentTemplate(Key);
	}

	/** Inherited native components (BP_WallActor's NavModifier): the template override on this Blueprint. */
	UNavModifierComponent* GetEditableNavModifier(UBlueprint& Blueprint)
	{
		bool bIsInherited = false;
		USCS_Node* const Node = FindNodeOfClass(Blueprint, UNavModifierComponent::StaticClass(), bIsInherited);
		return Cast<UNavModifierComponent>(GetEditableTemplate(Blueprint, Node, bIsInherited));
	}

	TArray<UMaterialInterface*> ParseMaterials(const FString& Text)
	{
		TArray<UMaterialInterface*> Materials;
		TArray<FString> Paths;
		Text.ParseIntoArray(Paths, TEXT("|"));
		for (const FString& Path : Paths)
		{
			UMaterialInterface* const Material = LoadByPath<UMaterialInterface>(Path);
			if (Material == nullptr)
			{
				UE_LOG(LogWallBreachAuthoring, Error, TEXT("RTS_WALL_BREACH_SETUP missing material %s"), *Path);
			}
			Materials.Add(Material);
		}
		return Materials;
	}

	void SetNavArea(UNavModifierComponent& NavModifier, UClass* const AreaClass)
	{
		FClassProperty* const AreaProperty = FindFProperty<FClassProperty>(UNavModifierComponent::StaticClass(),
		                                                                   TEXT("AreaClass"));
		if (AreaProperty == nullptr)
		{
			return;
		}
		NavModifier.Modify();
		AreaProperty->SetObjectPropertyValue_InContainer(&NavModifier, AreaClass);
	}

	int32 SetNavBlockers(const UBlueprint& Blueprint, UClass* const NavBlockerClass)
	{
		int32 Changed = 0;
		if (Blueprint.SimpleConstructionScript == nullptr || NavBlockerClass == nullptr)
		{
			return Changed;
		}
		for (USCS_Node* const Node : Blueprint.SimpleConstructionScript->GetAllNodes())
		{
			UChildActorComponent* const ChildActor = Cast<UChildActorComponent>(Node->ComponentTemplate);
			if (ChildActor == nullptr || not Node->GetVariableName().ToString().StartsWith(TEXT("ObstacleNavMod")))
			{
				continue;
			}
			ChildActor->Modify();
			ChildActor->SetChildActorClass(NavBlockerClass);
			++Changed;
		}
		return Changed;
	}

	/**
	 * @brief The wall mesh must not carve the navmesh: with DynamicModifiersOnly the baked geometry never changes at
	 * play time, so only the wall's (switchable) nav boxes may decide who plans through it.
	 * @return Number of mesh templates changed.
	 */
	int32 SetWallMeshOutOfNavigation(UBlueprint& Blueprint, const FName WallMeshName)
	{
		int32 Changed = 0;
		for (USCS_Node* const Node : Blueprint.SimpleConstructionScript->GetAllNodes())
		{
			UStaticMeshComponent* const Mesh = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
			if (Mesh == nullptr || Node->GetVariableName() != WallMeshName || not Mesh->CanEverAffectNavigation())
			{
				continue;
			}
			Mesh->Modify();
			Mesh->SetCanEverAffectNavigation(false);
			++Changed;
		}
		return Changed;
	}

	UWallTankBreachComponent* EnsureBreachComponent(UBlueprint& Blueprint)
	{
		bool bIsInherited = false;
		USCS_Node* Node = FindNodeOfClass(Blueprint, UWallTankBreachComponent::StaticClass(), bIsInherited);
		if (Node == nullptr)
		{
			USimpleConstructionScript* const Construction = Blueprint.SimpleConstructionScript;
			Node = Construction->CreateNode(UWallTankBreachComponent::StaticClass(), BreachComponentName);
			Construction->AddNode(Node);
			bIsInherited = false;
		}
		return Cast<UWallTankBreachComponent>(GetEditableTemplate(Blueprint, Node, bIsInherited));
	}

	FWallTankBreachSettings MakeBreachSettings(const FString& Line)
	{
		FWallTankBreachSettings Settings;
		Settings.DustSystem = LoadByPath<UNiagaraSystem>(GetArg(Line, TEXT("Dust")));
		Settings.BreachSound = LoadByPath<USoundBase>(GetArg(Line, TEXT("Sound")));
		Settings.BreachAttenuation = LoadByPath<USoundAttenuation>(GetArg(Line, TEXT("Attenuation")));
		Settings.BreachConcurrency = LoadByPath<USoundConcurrency>(GetArg(Line, TEXT("Concurrency")));
		Settings.bHeavyTanksOnly = GetArg(Line, TEXT("HeavyOnly")) == TEXT("1");
		Settings.bBreachUnownedWall = GetArg(Line, TEXT("Unowned")) == TEXT("1");
		Settings.bKillWhenTopFalls = GetArg(Line, TEXT("KillOnFall")) == TEXT("1");
		const FString VSlope = GetArg(Line, TEXT("VSlope"));
		if (not VSlope.IsEmpty())
		{
			Settings.VSlope = FCString::Atof(*VSlope);
		}
		const FString SideMargin = GetArg(Line, TEXT("SideMargin"));
		if (not SideMargin.IsEmpty())
		{
			Settings.SideMargin = FCString::Atof(*SideMargin);
		}
		const FString NavHalfDepth = GetArg(Line, TEXT("NavHalfDepth"));
		if (not NavHalfDepth.IsEmpty())
		{
			Settings.NavBlockHalfDepth = FCString::Atof(*NavHalfDepth);
		}
		return Settings;
	}

	void ConfigureRusWall(const TArray<FString>& Args)
	{
		const FString Line = FString::Join(Args, TEXT(" "));
		const FString BlueprintPath = GetArg(Line, TEXT("Blueprint"));
		UBlueprint* const Blueprint = LoadByPath<UBlueprint>(BlueprintPath);
		if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
		{
			UE_LOG(LogWallBreachAuthoring, Error, TEXT("RTS_WALL_BREACH_SETUP FAIL no actor Blueprint %s"), *BlueprintPath);
			return;
		}
		Blueprint->Modify();
		bool bHolesInherited = false;
		USCS_Node* const HolesNode = FindNodeOfClass(*Blueprint, UWallHoleDamageComponent::StaticClass(), bHolesInherited);
		UWallHoleDamageComponent* const Holes = Cast<UWallHoleDamageComponent>(
			GetEditableTemplate(*Blueprint, HolesNode, bHolesInherited));
		if (Holes == nullptr)
		{
			UE_LOG(LogWallBreachAuthoring, Error, TEXT("RTS_WALL_BREACH_SETUP FAIL %s has no WallHoleDamage"), *BlueprintPath);
			return;
		}
		const TArray<UMaterialInterface*> Materials = ParseMaterials(GetArg(Line, TEXT("Materials")));
		int32 NavBlockers = 0;
		if (bHolesInherited)
		{
			// Child walls inherit field mode, the breach component and the nav setup; only their skin differs.
			Holes->Editor_SetDamagedMaterials(Materials);
		}
		else
		{
			Holes->Editor_SetFieldMode(Materials, ParsePath(GetArg(Line, TEXT("Path"))),
			                           FCString::Atof(*GetArg(Line, TEXT("Thickness"))));
			if (UWallTankBreachComponent* const Breach = EnsureBreachComponent(*Blueprint))
			{
				Breach->Editor_SetSettings(MakeBreachSettings(Line));
			}
			if (UClass* const AreaClass = LoadClassByPath(GetArg(Line, TEXT("NavArea"))))
			{
				if (UNavModifierComponent* const NavModifier = GetEditableNavModifier(*Blueprint))
				{
					SetNavArea(*NavModifier, AreaClass);
				}
			}
			NavBlockers = SetNavBlockers(*Blueprint, LoadClassByPath(GetArg(Line, TEXT("NavBlocker"))));
			const FString MeshNoNav = GetArg(Line, TEXT("MeshNoNav"));
			if (not MeshNoNav.IsEmpty())
			{
				UE_LOG(LogWallBreachAuthoring, Display, TEXT("RTS_WALL_BREACH_SETUP %s mesh %s out of navigation: %d"),
				       *BlueprintPath, *MeshNoNav, SetWallMeshOutOfNavigation(*Blueprint, FName(*MeshNoNav)));
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		const bool bCompiled = Blueprint->Status != BS_Error;
		const bool bSaved = bCompiled && SaveAsset(*Blueprint);
		UE_LOG(LogWallBreachAuthoring, Display,
		       TEXT("RTS_WALL_BREACH_SETUP %s %s inherited=%d materials=%d nav_blockers=%d"),
		       bSaved ? TEXT("SAVED") : TEXT("FAIL"), *BlueprintPath, bHolesInherited ? 1 : 0, Materials.Num(),
		       NavBlockers);
	}

	FAutoConsoleCommand ConfigureRusWallCommand(
		TEXT("RTS.WallBreach.ConfigureRusWall"),
		TEXT("Field holes + tank breaches on a wall BP. Args: Blueprint= Materials=a|b [Path=x,y;x,y] [Thickness=] "
			"[NavArea=] [NavBlocker=] [Dust=] [Sound=] [Attenuation=] [Concurrency=] [HeavyOnly=1] [Unowned=1] "
			"[KillOnFall=1] [NavHalfDepth=] [SideMargin=] [VSlope=] [MeshNoNav=<mesh component name>]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ConfigureRusWall));
}

#endif // WITH_EDITOR
