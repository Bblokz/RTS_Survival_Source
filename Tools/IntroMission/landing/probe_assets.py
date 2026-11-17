import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
keys = ('fence', 'lamp', 'beacon', 'windsock', 'flood', 'searchlight', 'polelight', 'streetlight', 'runway', 'hangar', 'gate', 'barrier', 'radio', 'antenna', 'flag', 'tower')
out = []
for ad in ar.get_assets(unreal.ARFilter(package_paths=['/Game'], recursive_paths=True)):
    n = str(ad.asset_name).lower()
    cls = str(ad.asset_class_path.asset_name)
    if cls not in ('Blueprint', 'StaticMesh'):
        continue
    if '__externalactors__' in str(ad.package_name).lower() or 'debris' in n or n.startswith('sm_') and cls == 'StaticMesh' and '/RTS_Survival/Blueprints/Buildings' in str(ad.package_name):
        continue
    if any(k in n for k in keys):
        out.append('%s [%s]' % (ad.package_name, cls))
open('D:/UE5Projects/RTS_Survival/Saved/IntroLanding/assets.txt', 'w').write('\n'.join(sorted(out)))
print(len(out))
