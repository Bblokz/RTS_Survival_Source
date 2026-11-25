"""Generates the run-over scream takes with ElevenLabs sound effects (text to sound effects).

Usage: python gen_run_over_screams.py <out_dir> [--takes N]

Writes <out_dir>/raw/<slot>_t<take>.mp3, then cleaned mono 44.1 kHz WAVs in <out_dir>/wav (leading silence
trimmed, peak -1 dBFS, 40 ms fade out) and <out_dir>/takes.json with duration / loudness per take, so
pick_run_over_screams.py can choose. Keep the output outside the repo: the outer repo auto-commits new files.
The key comes from ELEVENLABS_API_KEY or the project .env.local (never printed).
"""
import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
ENV_FILE = os.path.join(PROJECT_DIR, ".env.local")

NO_MUSIC = "no music, no speech, no words, dry close recording"
SLOTS = {
    "Male": [
        f"a man's sudden agonized scream, short, choked off abruptly as he is crushed, {NO_MUSIC}",
        f"a soldier screaming in terror, cut short into a strangled groan, crushed under a heavy weight, {NO_MUSIC}",
        f"a man's raw throat shriek of pain, one second long, ending in a wet gasp, {NO_MUSIC}",
        f"a young man's panicked yell of horror, rising, then abruptly silenced, {NO_MUSIC}",
        f"a deep male roar of agony, brief and harsh, ending in a crushed grunt, {NO_MUSIC}",
        f"a man screams in agony and pain, a short desperate burst, abruptly cut off, {NO_MUSIC}",
        f"a soldier's horrified scream as something heavy runs him over, short, cut off, {NO_MUSIC}",
        f"a middle aged man's hoarse cry of pain turning into a choking groan, short, {NO_MUSIC}",
        f"a man's piercing scream of terror, short, stops suddenly, {NO_MUSIC}",
        f"a man yells in pain then wheezes, crushed chest, brief, {NO_MUSIC}",
    ],
    "Female": [
        f"a woman's sudden agonized scream, short, choked off abruptly as she is crushed, {NO_MUSIC}",
        f"a female soldier screaming in terror, cut short into a strangled gasp, {NO_MUSIC}",
        f"a woman's piercing shriek of pain, one second long, abruptly silenced, {NO_MUSIC}",
        f"a woman's hoarse cry of agony turning into a choking groan, short, {NO_MUSIC}",
        f"a young woman's panicked scream of horror, rising then cut off, {NO_MUSIC}",
        f"a woman screams in pain, a short desperate burst, stops suddenly, {NO_MUSIC}",
    ],
}


def load_key():
    from dotenv import load_dotenv
    load_dotenv(dotenv_path=ENV_FILE)
    key = os.environ.get("ELEVENLABS_API_KEY")
    if not key:
        raise SystemExit("No ElevenLabs key (ELEVENLABS_API_KEY or .env.local).")
    return key


def measure(wav_path):
    """Integrated loudness (LUFS) and duration of a wav, via ffmpeg ebur128."""
    out = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", "-i", wav_path, "-af", "ebur128", "-f", "null", "-"],
        capture_output=True, text=True).stderr
    lufs = None
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("I:") and "LUFS" in line:
            lufs = float(line.split()[1])
    dur = float(subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", wav_path],
        capture_output=True, text=True).stdout.strip() or 0)
    return lufs, dur


def clean(src, dst):
    # Trim leading silence, mono 44.1 kHz, peak to -1 dBFS, short fade out at the end.
    tmp = dst + ".tmp.wav"
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error", "-i", src,
                    "-af", "silenceremove=start_periods=1:start_threshold=-45dB,"
                           "areverse,silenceremove=start_periods=1:start_threshold=-50dB,areverse",
                    "-ac", "1", "-ar", "44100", tmp], check=True)
    _, dur = measure(tmp)
    peak = subprocess.run(["ffmpeg", "-hide_banner", "-i", tmp, "-af", "volumedetect", "-f", "null", "-"],
                          capture_output=True, text=True).stderr
    max_db = 0.0
    for line in peak.splitlines():
        if "max_volume" in line:
            max_db = float(line.split("max_volume:")[1].split("dB")[0])
    gain = -1.0 - max_db
    fade_start = max(dur - 0.04, 0)
    subprocess.run(["ffmpeg", "-y", "-hide_banner", "-loglevel", "error", "-i", tmp,
                    "-af", f"volume={gain}dB,afade=t=out:st={fade_start}:d=0.04",
                    "-c:a", "pcm_s16le", dst], check=True)
    os.remove(tmp)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out_dir")
    parser.add_argument("--takes", type=int, default=1)
    parser.add_argument("--duration", type=float, default=1.8)
    args = parser.parse_args()

    key = load_key()
    from elevenlabs import ElevenLabs
    client = ElevenLabs(api_key=key)
    raw_dir = os.path.join(args.out_dir, "raw")
    wav_dir = os.path.join(args.out_dir, "wav")
    os.makedirs(raw_dir, exist_ok=True)
    os.makedirs(wav_dir, exist_ok=True)

    report = []
    for kind, prompts in SLOTS.items():
        for prompt_index, prompt in enumerate(prompts, start=1):
            for take in range(1, args.takes + 1):
                name = f"{kind}_p{prompt_index:02d}_t{take}"
                mp3 = os.path.join(raw_dir, name + ".mp3")
                if not os.path.exists(mp3):
                    audio = client.text_to_sound_effects.convert(
                        text=prompt, duration_seconds=args.duration, prompt_influence=0.6)
                    with open(mp3, "wb") as fh:
                        for chunk in audio:
                            fh.write(chunk)
                wav = os.path.join(wav_dir, name + ".wav")
                clean(mp3, wav)
                lufs, dur = measure(wav)
                report.append({"name": name, "kind": kind, "prompt": prompt, "lufs": lufs, "duration": dur})
                print(f"{name}: {dur:.2f} s, {lufs} LUFS", flush=True)
    with open(os.path.join(args.out_dir, "takes.json"), "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1)


if __name__ == "__main__":
    sys.exit(main())
