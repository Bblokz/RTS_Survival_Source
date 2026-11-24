# Unreal (editor python): compiled state of every deploy-mesh slot material of one building.
# Globals: DEPLOY_MESH. Prints one line per slot plus SUMMARY finished=<0|1> ok=<0|1>.
# ok = every slot compiled with uses_wpo=1 and no compile errors (MSPlain slots included).
import unreal

L = unreal.NomadicDeployEditorLibrary
mesh = unreal.load_asset(DEPLOY_MESH)
finished, ok = True, True
uv_sets = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0)
print('uv sets', uv_sets, '(VAT needs exactly 6: own sets padded to 4, then VatPiece, VatPivot)')
ok &= uv_sets == 6
for i, sm in enumerate(mesh.static_materials):
    mi = sm.material_interface
    text = L.describe_compiled_resource(mi) if mi else 'none'
    print('slot %02d %s: %s' % (i, mi.get_name() if mi else '-', text))
    finished &= 'finished=1' in text
    ok &= 'uses_wpo=1' in text and 'ERROR' not in text and 'finished=1' in text
print('SUMMARY finished=%d ok=%d' % (finished, ok))
