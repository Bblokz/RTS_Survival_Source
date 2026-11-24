"""Final narration of the 1976 intro cinematic from the picked announcer takes.

    python make_final_vo.py

Reads VoiceLines/IntroCinematic1976/Takes/_picks.txt (line number + letter) and writes
VoiceLines/IntroCinematic1976/vo_final.json: per narration row the picked voice-changed wave (Takes/Picked), its
length and word-timed subtitles. intro_build.py and mix_audio.py use this file instead of the scratch voice as soon
as it exists (intro_narration.vo_json()).

Word times: whisper on the DRY ElevenLabs take (the clean voice), moved by the offset between the dry take and the
voice-changed one (found by matching their loudness curves), then matched to the subtitle's own words. A subtitle
word whisper did not hear gets its time from its neighbours.
"""
import difflib
import json
import os
import re
import subprocess

import numpy as np
import soundfile as sf

import intro_narration as narration
from make_scratch_vo import split_sentences, word_timings

ROOT = narration.VO_FINAL_DIR
WHISPER = r"D:/Tools/whisper/bin/Release/whisper-cli.exe"
MODEL = r"D:/Tools/whisper/models/ggml-large-v3-turbo.bin"
TMP = os.path.join(ROOT, "_check")
NUMBERS = {"1944": "nineteen fortyfour", "1945": "nineteen fortyfive", "30": "thirty", "v2": "v two", "v-2": "v two"}


def picks():
    out = {}
    with open(os.path.join(ROOT, "Takes", "_picks.txt"), encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if len(line) == 3 and line[:2].isdigit():
                out[int(line[:2])] = line[2]
    return out


def envelope(path, rate=200):
    audio, sample_rate = sf.read(path, dtype="float32", always_2d=True)
    mono = audio.mean(axis=1)
    hop = sample_rate // rate
    frames = len(mono) // hop
    return np.sqrt((mono[:frames * hop].reshape(frames, hop) ** 2).mean(axis=1) + 1e-12), rate


def offset_seconds(dry, final):
    """Seconds to add to a time in the dry take to get the time in the voice-changed take."""
    a, rate = envelope(dry)
    b, _ = envelope(final)
    a = (a - a.mean()) / (a.std() + 1e-9)
    b = (b - b.mean()) / (b.std() + 1e-9)
    corr = np.correlate(b, a, mode="full")
    lags = np.arange(-len(a) + 1, len(b))
    window = np.abs(lags) <= rate * 2          # the two never differ by more than a trimmed start
    return float(lags[window][np.argmax(corr[window])]) / rate


def heard_words(dry):
    """[(word, start, end)] from whisper, one word per segment."""
    os.makedirs(TMP, exist_ok=True)
    wav16, base = os.path.join(TMP, "t.wav"), os.path.join(TMP, "t")
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", dry, "-ar", "16000", "-ac", "1", wav16], check=True)
    subprocess.run([WHISPER, "-m", MODEL, "-f", wav16, "-l", "en", "-ml", "1", "-sow", "-oj", "-of", base, "-np"],
                   check=True, capture_output=True)
    with open(base + ".json", encoding="utf-8") as handle:
        segments = json.load(handle)["transcription"]
    words = []
    for segment in segments:
        text = segment["text"].strip()
        if text and re.search(r"[A-Za-z0-9]", text):
            words.append((text, segment["offsets"]["from"] / 1000.0, segment["offsets"]["to"] / 1000.0))
    return words


def key(word):
    word = word.lower().replace("kommandant", "commandant").replace("sterilized", "sterilised")
    word = NUMBERS.get(re.sub(r"[^a-z0-9\-]", "", word), word)
    return re.sub(r"[^a-z0-9]", "", word)


def timed_subtitle(subtitle, heard, shift, duration):
    """Subtitle words with times: matched whisper words where they agree, neighbours' times in between."""
    tokens = subtitle.split()
    fallback = word_timings(subtitle, duration)
    sub_keys, heard_keys = [key(t) for t in tokens], [key(w[0]) for w in heard]
    times = [None] * len(tokens)
    for block in difflib.SequenceMatcher(None, sub_keys, heard_keys, autojunk=False).get_matching_blocks():
        for k in range(block.size):
            _text, start, end = heard[block.b + k]
            times[block.a + k] = (max(start + shift, 0.0), min(max(end + shift, 0.0), duration))
    matched = sum(t is not None for t in times)
    if matched < max(1, len(tokens) // 2):
        return fallback, matched            # whisper and the script disagree too much: even spread over the take
    for index in range(len(tokens)):
        if times[index] is not None:
            continue
        before = next((times[j][1] for j in range(index - 1, -1, -1) if times[j] is not None), 0.05)
        after_index = next((j for j in range(index + 1, len(tokens)) if times[j] is not None), None)
        after = times[after_index][0] if after_index is not None else duration - 0.1
        gap = (after_index if after_index is not None else len(tokens)) - index
        length = max(after - before, 0.05) / gap
        times[index] = (before, before + length * 0.95)
    words = [{"text": token, "start": round(start, 3), "end": round(max(end, start + 0.05), 3)}
             for token, (start, end) in zip(tokens, times)]
    return words, matched


def main():
    chosen = picks()
    out = {}
    for index, (row_id, shot, _spoken, subtitle, placeholder) in enumerate(narration.ROWS, 1):
        name = "%02d%s" % (index, chosen[index])
        final = os.path.join(ROOT, "Takes", "Picked", name + ".wav").replace("\\", "/")
        dry = os.path.join(ROOT, "Dry", name + ".wav")
        duration = sf.info(final).duration
        shift = offset_seconds(dry, final)
        words, matched = timed_subtitle(subtitle, heard_words(dry), shift, duration)
        out[row_id] = {"shot": shot, "wave": final, "duration": round(duration, 3), "subtitle": subtitle,
                       "placeholder": placeholder, "take": name, "sentences": split_sentences(words)}
        print("%-5s %s %6.2fs  shift %+.2fs  words %d/%d  %s" % (row_id, name, duration, shift, matched, len(words), subtitle[:50]))
    with open(narration.VO_FINAL_JSON, "w", encoding="utf-8") as handle:
        json.dump(out, handle, indent=1)
    print("total voice %.1fs -> %s" % (sum(v["duration"] for v in out.values()), narration.VO_FINAL_JSON))


if __name__ == "__main__":
    main()
