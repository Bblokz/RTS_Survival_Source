"""Scratch voice for the intro cinematic: free offline Windows TTS (System.Speech), one wave per narration row.

    python make_scratch_vo.py

Writes Saved/IntroCinematic/vo_scratch/VO_Intro_<row>.wav and vo_scratch.json (duration and word timings per row).
The word timings are spread over the take by character count: good enough to cut against; the final lines get real
timings from the VoiceChanger transcript pipeline (Source/Docs/UI/PortraitSubtitles.md).
"""
import json
import os
import re
import subprocess
import wave

import intro_narration as narration

LEAD_SECONDS = 0.08
TAIL_SECONDS = 0.15
MAX_WORDS_PER_SENTENCE = 12

PS_TEMPLATE = r'''
Add-Type -AssemblyName System.Speech
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$s.SelectVoice("Microsoft David Desktop")
$s.Rate = -2
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(44100, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
$rows = Get-Content -Raw -Encoding UTF8 "%(rows)s" | ConvertFrom-Json
foreach ($r in $rows) {
  $s.SetOutputToWaveFile($r.path, $fmt)
  $s.Speak($r.text)
}
$s.SetOutputToNull()
$s.Dispose()
'''


def split_sentences(words):
    """Sentences of at most MAX_WORDS_PER_SENTENCE words, split at sentence ends and then near commas."""
    sentences, current = [], []
    for word in words:
        current.append(word)
        if re.search(r"[.!?]$|\.\.\.$", word["text"]):
            sentences.append(current)
            current = []
    if current:
        sentences.append(current)
    result = []
    for sentence in sentences:
        while len(sentence) > MAX_WORDS_PER_SENTENCE:
            middle = len(sentence) // 2
            commas = [i for i, w in enumerate(sentence[:-1]) if w["text"].endswith((",", ";"))]
            cut = min(commas, key=lambda i: abs(i - middle)) + 1 if commas else middle
            result.append(sentence[:cut])
            sentence = sentence[cut:]
        result.append(sentence)
    return result


def word_timings(text, duration):
    tokens = text.split()
    weights = [len(t) + 1.5 + (4.0 if t.endswith((".", "!", "?", "...")) else 2.0 if t.endswith((",", ";")) else 0.0)
               for t in tokens]
    span = max(duration - LEAD_SECONDS - TAIL_SECONDS, 0.1)
    total = sum(weights)
    words, cursor = [], LEAD_SECONDS
    for token, weight in zip(tokens, weights):
        length = span * weight / total
        words.append({"text": token, "start": round(cursor, 3), "end": round(cursor + length * 0.9, 3)})
        cursor += length
    return words


def main():
    os.makedirs(narration.VO_SCRATCH_DIR, exist_ok=True)
    rows = [{"path": ("%s/%s.wav" % (narration.VO_SCRATCH_DIR, narration.wave_name(row_id))).replace("/", "\\"),
             "text": subtitle} for row_id, _shot, _spoken, subtitle, _ph in narration.ROWS]
    rows_file = os.path.join(narration.VO_SCRATCH_DIR, "_rows.json")
    with open(rows_file, "w", encoding="utf-8") as handle:
        json.dump(rows, handle)
    script = os.path.join(narration.VO_SCRATCH_DIR, "_tts.ps1")
    with open(script, "w", encoding="utf-8") as handle:
        handle.write(PS_TEMPLATE % {"rows": rows_file.replace("/", "\\")})
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script], check=True)
    out = {}
    for row_id, shot, _spoken, subtitle, placeholder in narration.ROWS:
        path = "%s/%s.wav" % (narration.VO_SCRATCH_DIR, narration.wave_name(row_id))
        with wave.open(path, "rb") as handle:
            duration = handle.getnframes() / float(handle.getframerate())
        words = word_timings(subtitle, duration)
        out[row_id] = {"shot": shot, "wave": path, "duration": round(duration, 3), "subtitle": subtitle,
                       "placeholder": placeholder, "sentences": split_sentences(words)}
        print("%-5s %6.2fs  %s" % (row_id, duration, subtitle[:70]))
    with open(os.path.join(narration.VO_SCRATCH_DIR, "vo_scratch.json"), "w", encoding="utf-8") as handle:
        json.dump(out, handle, indent=1)
    print("total voice %.1fs" % sum(v["duration"] for v in out.values()))


if __name__ == "__main__":
    main()
