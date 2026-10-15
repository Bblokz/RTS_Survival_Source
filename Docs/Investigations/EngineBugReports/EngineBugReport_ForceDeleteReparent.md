# Bug report: Force-deleting a Blueprint whose previous force-delete failed re-parents unrelated Blueprints

**Engine:** Unreal Engine 5.5 (source build, release branch), Windows 11
**Area:** Editor › Content Browser / Asset deletion (`ObjectTools::ForceDeleteObjects`)
**Severity:** silent data corruption of unrelated assets, including engine plugin content
**Reproducible:** yes, 100% with the attached script (`EngineBugReport_ForceDeleteReparent_repro.py`)

## Summary
When a force-delete of a Blueprint fails because the Blueprint is still referenced ("… is in use" → "ForceDeleteObject
failed to delete X, this package is now potentially corrupt"), the Blueprint object survives with
`GeneratedClass == nullptr`. Force-deleting that Blueprint again re-parents **unrelated** Blueprints to the deleted
Blueprint's parent class and recompiles them, without any prompt. In our project the engine widget
`/MovieRenderPipeline/Blueprints/UI_MovieRenderPipelineInfoTableRow` was re-parented from `UserWidget` to one of our
project C++ widget classes. The next "save dirty packages" wrote it into
`Engine/Plugins/MovieScene/MovieRenderPipeline/Content/Blueprints/`, after which every PIE start stopped on the
"Play anyway?" prompt (the engine Blueprint now failed to compile: "A required widget binding … was not found").

## Steps to reproduce
1. Enable the Movie Render Pipeline plugin (its widget Blueprint is the Blueprint that gets re-parented for us).
2. Duplicate any Blueprint into a temporary folder.
3. Keep a reference to an object inside the copy (the script holds a Python reference into its widget tree).
4. Force-delete the folder → "is in use" dialog, log: `ForceDeleteObject failed to delete …, this package is now potentially corrupt`.
   The Blueprint object survives; its `GeneratedClass` is now `nullptr`.
5. Force-delete the folder again.

**Expected:** the zombie Blueprint is deleted (or the delete fails again); no other asset changes.
**Actual:** unrelated Blueprints are re-parented to the zombie's parent class and recompiled.

Output of the attached script in the full editor (`-unattended -ExecutePythonScript=…`):
```
REPRO MRP before: parent=UserWidget
LogObjectTools: Warning: ForceDeleteObject failed to delete W_BP_ReproEntry, this package is now potentially corrupt
REPRO after pass 1: object alive, generated class = None
LogEditorAssetSubsystem: Error: DuplicateAsset failed: The destination path '…/W_BP_ReproEntry.W_BP_ReproEntry' is not valid. An asset already exists at this location.
LogObjectTools: Warning: ForceDeleteObject failed to delete W_BP_ReproEntry, this package is now potentially corrupt
REPRO PARENT CHANGED /MovieRenderPipeline/Blueprints/UI_MovieRenderPipelineInfoTableRow.UI_MovieRenderPipelineInfoTableRow : before=UserWidget after-pass1=UserWidget after-pass2=W_TrainingBuildingEntry
```
(`W_TrainingBuildingEntry` is the native parent of the Blueprint being deleted.)

## Root cause
`Engine/Source/Editor/UnrealEd/Private/ObjectTools.cpp`, `ObjectTools::ForceDeleteObjects` (~line 3565):

```cpp
// Reparent any direct children to the parent class of the blueprint that's about to be deleted
if (BlueprintObject->ParentClass != nullptr)
{
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
    {
        UClass* ChildClass = *ClassIt;
        if (ChildClass->GetSuperStruct() == BlueprintObject->GeneratedClass)   // GeneratedClass can be nullptr here
        {
            UBlueprint* ChildBlueprint = Cast<UBlueprint>(ChildClass->ClassGeneratedBy);
            if (ChildBlueprint != nullptr && ObjectsToDelete.Find(ChildBlueprint) == INDEX_NONE)
            {
                ChildBlueprint->Modify();
                ChildBlueprint->ParentClass = BlueprintObject->ParentClass;
                FKismetEditorUtilities::CompileBlueprint(ChildBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
```

Further down the same loop, `BlueprintObject->RemoveGeneratedClasses()` runs, which sets `GeneratedClass = NULL`
(`FKismet2CompilerModule::RemoveBlueprintGeneratedClasses`, KismetCompilerModule.cpp). If the delete then fails
because the package is still referenced, the Blueprint survives with a null `GeneratedClass`. On the next force-delete
the comparison above becomes `ChildClass->GetSuperStruct() == nullptr`, so any Blueprint-generated class with a null
super is treated as a "child" and its Blueprint is re-parented.

## Suggested fix
- Skip the re-parent loop when `BlueprintObject->GeneratedClass == nullptr`:
  `if (BlueprintObject->ParentClass != nullptr && BlueprintObject->GeneratedClass != nullptr)`.
- Optionally do not call `RemoveGeneratedClasses()` before it is known that the delete will succeed, so a failed
  delete does not leave a Blueprint without a generated class.

## Second, related issue: crash when the same delete runs in a commandlet
Running the same steps in `UnrealEditor-Cmd.exe -run=pythonscript` crashes on the first delete with
`EXCEPTION_ACCESS_VIOLATION reading address 0x0`:
```
ObjectTools::GatherObjectReferencersForDeletion()   ObjectTools.cpp:401
ObjectTools::DeleteSingleObject()                   ObjectTools.cpp:3183
ObjectTools::ForceDeleteObjects()                   ObjectTools.cpp:3654
UEditorAssetSubsystem::DeleteDirectory()
```
Line 401 is `GEditor->Trans->DisableObjectSerialization();` — `GEditor->Trans` is null in a commandlet. The call
(and the matching `EnableObjectSerialization()` on line 408) needs a null check.
