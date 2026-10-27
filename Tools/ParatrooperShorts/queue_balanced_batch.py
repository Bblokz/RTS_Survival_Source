"""Queue the prepared three presets once in this project's isolated Studio instance."""
import argparse
import json
from pathlib import Path
import urllib.request

PROJECT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8766)
    args = parser.parse_args()
    url = 'http://127.0.0.1:%d' % args.port
    with urllib.request.urlopen(url + '/api/library', timeout=30) as response:
        library = json.load(response)
    assert Path(library['root']).resolve() == PROJECT / 'ShortsCreator', 'Use the RTS_Survival Studio instance.'
    with urllib.request.urlopen(url + '/api/state', timeout=30) as response:
        state = json.load(response)
    existing = {entry['name']: entry for entry in state['queue']}
    for path in sorted((PROJECT / 'ShortsCreator/Jobs').glob('ParatrooperFair0*.json')):
        job = json.loads(path.read_text(encoding='utf-8'))
        name = job['JobName']
        if name in existing:
            print(name, existing[name]['status'], existing[name]['id'])
            continue
        request = dict(jobId=path.stem, setupId='ParatrooperBattle', biome='source', name=name,
                       post=True, keepFrames=False, stack=False, advanced=job)
        http_request = urllib.request.Request(url + '/api/queue',
            data=json.dumps(request).encode('utf-8'),
            headers={'Content-Type': 'application/json', 'X-Shorts-Token': library['token']})
        with urllib.request.urlopen(http_request, timeout=30) as response:
            result = json.load(response)
        print(name, 'queued', result['id'])
    with urllib.request.urlopen(url + '/api/state', timeout=30) as response:
        state = json.load(response)
    summary = [{key: entry.get(key) for key in ['id', 'name', 'status', 'stage', 'take']}
               for entry in state['queue'] if entry['name'].startswith('ParatrooperFair0')]
    assert len(summary) == 3, summary
    output = Path(__file__).resolve().parent / 'BalancedBatchQueue.json'
    output.write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
