"""Bring the existing Forge caption tool into this project's universal render path."""
from pathlib import Path
import shutil

PROJECT = Path(__file__).resolve().parents[3]
TOOLS = PROJECT / 'ShortsCreator/Tools'
REFERENCE = Path('D:/UE5Projects/RTS_Shorts/ShortsCreator')
BACKUP = Path(__file__).resolve().parent / 'Before/RoundsIntegration'
BACKUP.mkdir(parents=True, exist_ok=True)
for name in ['run_shorts.py', 'dashboard_model.py', 'dashboard_server.py']:
    destination = BACKUP / name
    if not destination.exists():
        shutil.copy2(TOOLS / name, destination)
for source, target in [(REFERENCE / 'Tools/forge_rounds.py', TOOLS / 'forge_rounds.py'),
                       (REFERENCE / 'special_rounds.json', PROJECT / 'ShortsCreator/special_rounds.json')]:
    if not target.exists():
        shutil.copy2(source, target)
launcher = TOOLS / 'run_shorts.py'
text = launcher.read_text(encoding='utf-8')
if 'import forge_rounds' not in text:
    text = text.replace('PROJECT = os.path.abspath',
        'sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))\nimport forge_rounds\n\nPROJECT = os.path.abspath', 1)
    text = text.replace('    job["OutputDir"] = take_dir.replace',
        '    forge_rounds.annotate_job(job, PROJECT, EDITOR_CMD)\n    job["OutputDir"] = take_dir.replace', 1)
    launcher.write_text(text, encoding='utf-8')
model = TOOLS / 'dashboard_model.py'
text = model.read_text(encoding='utf-8')
if 'import forge_rounds' not in text:
    text = text.replace('from pathlib import Path', 'from pathlib import Path\n\nimport forge_rounds', 1)
    marker = '    return {"jobs": jobs, "setups": setups, "units": list(units.values()),'
    assert marker in text
    text = text.replace(marker,
        '    special_rounds = forge_rounds.load_table(str(root.parent))\n'
        '    for unit in units.values():\n'
        '        unit["name"] = forge_rounds.label_with_round(special_rounds, unit["classPath"], unit["name"] or "")\n'
        + marker, 1)
    model.write_text(text, encoding='utf-8')
rounds = TOOLS / 'forge_rounds.py'
text = rounds.read_text(encoding='utf-8')
old = '''        elif len(tokens) == len(classes):'''
new = '''        elif shared and _annotate_shared_groups(tokens, groups, cls, base, labels, tag):
            pass
        elif len(tokens) == len(classes):'''
helper = '''def _annotate_shared_groups(tokens, groups, special_class, base, labels, tag):
    """Match same-name regular/Forge groups while ignoring faction headers and aircraft/drop rows."""
    matching_groups = [group_class for group_class, _ in groups if _strip_tag(labels.get(group_class)) == base]
    matching_tokens = [index for index, token in enumerate(tokens)
                       if _strip_tag(re.sub(r"^\\s*\\d+\\s*[xX×]\\s*", "", token.strip())) == base]
    if len(matching_groups) != len(matching_tokens):
        return False
    for index, group_class in zip(matching_tokens, matching_groups):
        if group_class == special_class:
            tokens[index] = _tag_token(tokens[index], tag)
    return True


'''
if 'def _annotate_shared_groups' not in text:
    assert old in text
    text = text.replace(old, new, 1)
    text = text.replace('def _annotate_caption(', helper + 'def _annotate_caption(', 1)
if 'from pathlib import Path' not in text:
    text = text.replace('import tempfile', 'import tempfile\nfrom pathlib import Path', 1)
    text = text.replace('open(path, "rb").read()', 'Path(path).read_bytes()')
rounds.write_text(text, encoding='utf-8')
server = TOOLS / 'dashboard_server.py'
text = server.read_text(encoding='utf-8')
if 'state_dir=None' not in text:
    text = text.replace('def __init__(self, root):', 'def __init__(self, root, state_dir=None):', 1)
    text = text.replace('self.state_dir = self.root / "Dashboard" / "Data"',
        'self.state_dir = Path(state_dir).resolve() if state_dir else self.root / "Dashboard" / "Data"', 1)
    text = text.replace('    parser.add_argument("--port", type=int, default=8765)',
        '    parser.add_argument("--port", type=int, default=8765)\n'
        '    parser.add_argument("--state-dir", type=Path, help="Independent queue history for this Studio instance")', 1)
    text = text.replace('    app = Dashboard(root)', '    app = Dashboard(root, state_dir=args.state_dir)', 1)
    server.write_text(text, encoding='utf-8')
print('Installed universal pre-render round labels, Studio unit names, and paratrooper group matching.')
