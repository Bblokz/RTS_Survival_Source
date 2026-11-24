#!/usr/bin/env bash
# Starts the throwaway editor for the intro cinematic tools (never the shared editor) and waits until its command
# server answers. Autosave is off: autosaving the streamed landscape maps crashes the editor, and they must never be
# saved anyway.
ROOT=D:/UE5Projects/RTS_Survival
rm -f "$ROOT/Saved/IntroCinematic/cmd/server_ready.txt" "$ROOT"/Saved/IntroCinematic/cmd/req_* "$ROOT"/Saved/IntroCinematic/cmd/res_*
(MSYS_NO_PATHCONV=1 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" "$ROOT/RTS_Survival.uproject" \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976" \
  "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False" \
  "-ExecCmds=py $ROOT/Source/Tools/IntroCinematic/intro_editor_server.py" \
  -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding "-abslog=$ROOT/Saved/IntroCinematic/editor.log" > /dev/null 2>&1 &)
for i in $(seq 1 100); do sleep 5; [ -f "$ROOT/Saved/IntroCinematic/cmd/server_ready.txt" ] && break; done
echo "editor pid $(cat "$ROOT/Saved/IntroCinematic/cmd/server_ready.txt" 2>/dev/null)"
