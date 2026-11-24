"""Installs ruin Blueprint packages that could not be saved in place because another editor held the files.

Background (2026-11-13): every editor that has Ger_Berlin (or any ruin) loaded keeps the 23 BP_BerlinRuin_* .uasset
files open, and a save from another process then fails ("Error moving file", nothing written). The authoring editor
wrote its packages to a staging folder instead, one console command per package:
    OBJ SAVEPACKAGE PACKAGE="/Game/.../BP_BerlinRuin_X" FILE="<staged>/BP_BerlinRuin_X.uasset" SILENT=true
This script (plain Python, no Unreal) waits until nobody holds the 23 files and copies the staged ones over them at
once. Run:   python ruin_death_install_staged.py [wait_seconds, default 540]
Prints "identical 23/23" when it is done; anything else means nothing or not everything was installed.
Afterwards: ruin_death_compile_check.py (headless), then reload the 23 packages in every editor that was open
(BerlinRuinKit.md, "Changing the death while editors are open").
"""
import glob
import os
import shutil
import sys
import time

DST = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/'
SRC = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/ruin_death_test/staged/'


def main():
    files = sorted(os.path.basename(f) for f in glob.glob(SRC + 'BP_BerlinRuin_*.uasset'))
    if len(files) != 23:
        print('expected 23 staged packages in %s, found %d: nothing installed' % (SRC, len(files)))
        return 3
    limit = float(sys.argv[1]) if len(sys.argv) > 1 else 540.0
    start = time.time()
    while True:
        held = []
        for name in files:
            try:
                open(DST + name, 'r+b').close()
            except OSError:
                held.append(name)
        if not held:
            break
        if time.time() - start > limit:
            print('timeout after %.0f s: %d files still held by another process, nothing installed' % (limit, len(held)))
            return 2
        time.sleep(2)
    failed = []
    for name in files:
        try:
            shutil.copyfile(SRC + name, DST + name)
        except OSError as error:
            failed.append((name, str(error)))
    same = sum(open(SRC + n, 'rb').read() == open(DST + n, 'rb').read() for n in files)
    print('free after %.0f s; identical %d/23; failed %s' % (time.time() - start, same, failed))
    return 0 if same == 23 else 1


if __name__ == '__main__':
    sys.exit(main())
