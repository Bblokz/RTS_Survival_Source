"""Turns the start-content cactus pass off for the Kessel survival map (its start content is craters only by design)
and logs both assets' cactus settings. Run in a throwaway DebugGame editor with -ExecCmds="py <this file>"."""
import importlib
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/LevelTools')
import survival_cacti_setup as scs  # noqa: E402

importlib.reload(scs)
try:
    scs.disable_cacti(scs.KESSEL_ASSET)
    scs.report(scs.KESSEL_ASSET)
    scs.report(scs.DESERT_ASSET)
    unreal.log_warning('KESSELCACTI DONE')
except Exception as error:
    unreal.log_warning('KESSELCACTI FAILED %r' % (error,))
unreal.SystemLibrary.quit_editor()
