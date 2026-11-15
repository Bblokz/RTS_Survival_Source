"""Offline audio mix of the intro cinematic (headless Movie Render Queue audio is unreliable, see the memory note
'headless-render-audio-mute'): every sound is placed from the same timeline the sequences were built from, with ffmpeg.

    python mix_audio.py            -> Saved/IntroCinematic/audio/intro_mix.wav
    python mix_audio.py --mux <frames dir> <fps> <out.mp4>   also encodes the frames with the mix

Sources: the narration waves (Saved/IntroCinematic/vo_scratch, or the final lines later) and the entries of
Saved/IntroCinematic/audio/sfx.json (written by the build: [{"file": wav, "start": s, "gain_db": x, "fade_in": s,
"fade_out": s, "duration": s, "loop": bool}]).
"""
import json
import os
import subprocess
import sys

ROOT = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic"
TIMELINE = ROOT + "/timeline.json"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import intro_narration

VO = intro_narration.vo_json()
SFX = ROOT + "/audio/sfx.json"
OUT = ROOT + "/audio/intro_mix.wav"
VOICE_GAIN_DB = 0.0


def main():
    with open(TIMELINE, "r", encoding="utf-8") as handle:
        timeline = json.load(handle)
    with open(VO, "r", encoding="utf-8") as handle:
        vo = json.load(handle)
    total = timeline["total"]
    entries = []
    for shot in timeline["shots"]:
        for row in shot["vo"]:
            entries.append({"file": vo[row["row"]]["wave"], "start": row["start"], "gain_db": VOICE_GAIN_DB})
    if os.path.exists(SFX):
        with open(SFX, "r", encoding="utf-8") as handle:
            entries += json.load(handle)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    command = ["ffmpeg", "-y", "-loglevel", "error"]
    filters = []
    for index, entry in enumerate(entries):
        if entry.get("loop"):
            command += ["-stream_loop", "-1"]
        command += ["-i", entry["file"]]
        chain = "[%d:a]aresample=48000,aformat=channel_layouts=stereo" % index
        if entry.get("duration"):
            chain += ",atrim=0:%.3f" % entry["duration"]
        if entry.get("fade_in"):
            chain += ",afade=t=in:st=0:d=%.3f" % entry["fade_in"]
        if entry.get("fade_out") and entry.get("duration"):
            chain += ",afade=t=out:st=%.3f:d=%.3f" % (entry["duration"] - entry["fade_out"], entry["fade_out"])
        chain += ",volume=%.2fdB,adelay=%d:all=1[a%d]" % (entry.get("gain_db", 0.0), int(round(entry["start"] * 1000)), index)
        filters.append(chain)
    mix = "".join("[a%d]" % index for index in range(len(entries)))
    filters.append("%samix=inputs=%d:normalize=0:dropout_transition=0,apad,atrim=0:%.3f,alimiter=limit=0.84:level=disabled[out]"
                   % (mix, len(entries), total))
    script = ROOT + "/audio/_filter.txt"
    with open(script, "w", encoding="utf-8") as handle:
        handle.write(";\n".join(filters))
    command += ["-filter_complex_script", script, "-map", "[out]", "-c:a", "pcm_s16le", OUT]
    subprocess.run(command, check=True)
    print("mix", OUT, "%.1f s" % total, len(entries), "sounds")
    if "--mux" in sys.argv:
        k = sys.argv.index("--mux")
        frames, fps, movie = sys.argv[k + 1], sys.argv[k + 2], sys.argv[k + 3]
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", fps, "-start_number", "0", "-i", frames + "/f.%05d.jpeg",
                        "-i", OUT, "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "17", "-preset", "slow", "-c:a", "aac",
                        "-b:a", "256k", "-shortest", movie], check=True)
        print("movie", movie)


if __name__ == "__main__":
    main()
