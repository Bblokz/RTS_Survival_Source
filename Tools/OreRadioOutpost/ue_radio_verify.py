"""Fresh-process read-back of BP_OreRadio_Funkbude (and the meshes) from disk.  sh run_ue_py.sh ue_radio_verify.py verify -nullrhi"""
HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/OreRadioOutpost'
with open(HERE + '/ue_radio_bp.py') as f:
    exec(compile(f.read(), 'ue_radio_bp.py', 'exec'), {'__name__': '__main__', 'VERIFY_ONLY': True})
