# Unreal (editor python): lists every blueprint whose native parent is ANomadicVehicle. Read-only.
import unreal

ar = unreal.AssetRegistryHelpers.get_asset_registry()
flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'Blueprint')], package_paths=['/Game'],
                      recursive_paths=True)
rows = []
for ad in ar.get_assets(flt):
    native = str(ad.get_tag_value('NativeParentClass') or '')
    if 'NomadicVehicle' in native:
        rows.append((str(ad.package_name), str(ad.get_tag_value('ParentClass') or '').rsplit('.', 1)[-1]))
for r in sorted(rows):
    print(r[0], '<-', r[1])
print('total', len(rows))
