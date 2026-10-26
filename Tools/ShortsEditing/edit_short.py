"""Apply reusable, frame-timed typewriter callouts to a finished short.

Requires Pillow, ffmpeg and ffprobe. The source is never overwritten; audio is
copied without re-encoding. See README.md for recipe and pipeline usage.
"""
import argparse
from dataclasses import dataclass
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFilter, ImageFont


DEFAULT_STYLE = {
    "font": "C:/Windows/Fonts/seguisb.ttf",
    "font_size_fraction": 0.0463,
    "left_fraction": 0.05,
    "bottom_fraction": 0.115,
    "max_width_fraction": 0.78,
    "stroke_fraction": 0.0037,
    "exit_slide_fraction": 0.065,
    "cursor_blink_seconds": 0.4,
}


def run(arguments):
    result = subprocess.run(arguments, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr[-5000:])
    return result.stdout


def probe(path):
    return json.loads(run([
        "ffprobe", "-v", "error", "-show_entries",
        "format=duration:stream=codec_type,width,height,avg_frame_rate,nb_frames",
        "-of", "json", str(path),
    ]))


def audio_hash(path):
    return run([
        "ffmpeg", "-v", "error", "-i", str(path), "-map", "0:a:0",
        "-c:a", "copy", "-f", "hash", "-hash", "sha256", "-",
    ]).strip()


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def seconds_to_frames(value, fps, allow_zero=False):
    seconds = float(value)
    if not math.isfinite(seconds) or seconds < 0 or (seconds == 0 and not allow_zero):
        raise ValueError("Times must be finite and positive (starts/gaps may be zero).")
    return round(Fraction(str(value)) * fps)


@dataclass(frozen=True)
class Callout:
    text: str
    start: int
    typing: int
    hold: int
    exit: int

    @property
    def hold_start(self):
        return self.start + self.typing

    @property
    def exit_start(self):
        return self.hold_start + self.hold

    @property
    def end(self):
        return self.exit_start + self.exit

    def state(self, local_frame):
        if local_frame < 0 or local_frame >= self.end - self.start:
            return 0, 0.0
        if local_frame < self.typing:
            return math.floor(len(self.text) * local_frame / self.typing), 1.0
        if local_frame < self.typing + self.hold:
            return len(self.text), 1.0
        exit_frame = local_frame - self.typing - self.hold
        return len(self.text), 1.0 - (exit_frame + 1) / self.exit

    def timing(self, fps):
        return {
            "text": self.text,
            "start_seconds": float(self.start / fps),
            "fully_typed_seconds": float(self.hold_start / fps),
            "hold_seconds": float(self.hold / fps),
            "exit_start_seconds": float(self.exit_start / fps),
            "end_seconds": float(self.end / fps),
        }


def resolve_recipe(recipe, fps, duration):
    if recipe.get("version") != 1:
        raise ValueError("Expected recipe version 1.")
    resolved = []
    previous_end = 0
    for index, entry in enumerate(recipe.get("callouts", [])):
        text = entry.get("text", "")
        if not isinstance(text, str) or not text.strip() or "\n" in text:
            raise ValueError("Each callout needs nonempty, single-paragraph text; wrapping is automatic.")
        if ("start_seconds" in entry) == ("after_previous_seconds" in entry):
            raise ValueError("Use either start_seconds or after_previous_seconds.")
        if "after_previous_seconds" in entry:
            if index == 0:
                raise ValueError("The first callout requires start_seconds.")
            start = previous_end + seconds_to_frames(entry["after_previous_seconds"], fps, True)
        else:
            start = seconds_to_frames(entry["start_seconds"], fps, True)
        callout = Callout(
            text, start,
            seconds_to_frames(entry.get("type_seconds", 0.4), fps),
            seconds_to_frames(entry["hold_seconds"], fps),
            seconds_to_frames(entry.get("exit_seconds", 0.2), fps),
        )
        if min(callout.typing, callout.hold, callout.exit) < 1:
            raise ValueError("Each animation phase must last at least one frame.")
        if start < previous_end or callout.end / fps > duration + 0.001:
            raise ValueError("Callouts may not overlap or extend beyond the source video.")
        resolved.append(callout)
        previous_end = callout.end
    if not resolved:
        raise ValueError("The recipe contains no callouts.")
    style = {**DEFAULT_STYLE, **recipe.get("style", {})}
    for name in ("font_size_fraction", "left_fraction", "bottom_fraction", "max_width_fraction", "stroke_fraction", "exit_slide_fraction"):
        value = float(style[name])
        if not math.isfinite(value) or not 0 < value < 1:
            raise ValueError(f"Style {name} must be between zero and one.")
    if style["left_fraction"] + style["max_width_fraction"] > 0.98:
        raise ValueError("The callout exceeds the right safe margin.")
    if not math.isfinite(float(style["cursor_blink_seconds"])) or style["cursor_blink_seconds"] <= 0:
        raise ValueError("Cursor blink duration must be positive.")
    if not Path(style["font"]).is_file():
        raise ValueError(f"Font not found: {style['font']}")
    return resolved, style


class CalloutArtwork:
    def __init__(self, callout, style, width, height):
        self.callout = callout
        self.style = style
        self.font = ImageFont.truetype(style["font"], max(10, round(width * style["font_size_fraction"])))
        self.stroke = max(1, round(width * style["stroke_fraction"]))
        self.padding = self.stroke * 4
        self.line_step = round(self.font.size * 1.28)
        self.ascent = self.font.getmetrics()[0]
        self.lines = self.wrap(round(width * style["max_width_fraction"]))
        self.size = (
            math.ceil(max(self.font.getlength(line) for line in self.lines)) + self.padding * 2 + self.stroke * 2,
            self.line_step * len(self.lines) + self.padding * 2,
        )
        self.left = round(width * style["left_fraction"]) - self.padding
        self.top = height - round(height * style["bottom_fraction"]) - self.size[1]
        self.slide = round(width * style["exit_slide_fraction"])
        if self.top < 0 or self.left + self.size[0] > width:
            raise ValueError("Text does not fit in the video safe area.")

    def wrap(self, max_width):
        lines = []
        line = ""
        for word in self.callout.text.split(" "):
            if self.font.getlength(word) > max_width:
                raise ValueError("A word is too wide; reduce font_size_fraction.")
            candidate = (line + " " + word) if line else word
            if line and self.font.getlength(candidate) > max_width:
                lines.append(line)
                line = word
            else:
                line = candidate
        return lines + [line]

    def frame(self, frame_index, fps):
        visible, opacity = self.callout.state(frame_index)
        canvas = Image.new("RGBA", self.size)
        drawing = ImageDraw.Draw(canvas)
        remaining = visible
        cursor_x = self.padding
        cursor_y = self.padding + self.ascent
        for line_index, line in enumerate(self.lines):
            prefix = line[:max(0, remaining)]
            baseline = self.padding + self.ascent + line_index * self.line_step
            if prefix:
                drawing.text((self.padding, baseline), prefix, font=self.font, anchor="ls",
                             fill=(255, 255, 250, 255), stroke_width=self.stroke, stroke_fill=(5, 10, 9, 255))
            if remaining <= len(line):
                cursor_x = self.padding + self.font.getlength(prefix) + self.stroke
                cursor_y = baseline
                break
            remaining -= len(line) + 1
            cursor_x = self.padding + self.font.getlength(prefix) + self.stroke
            cursor_y = baseline
        typing = frame_index < self.callout.typing
        blinking = int(float(frame_index / fps) / self.style["cursor_blink_seconds"]) % 2 == 0
        if typing or blinking:
            drawing.rectangle((round(cursor_x), cursor_y - round(self.font.size * 0.82),
                               round(cursor_x) + max(1, self.stroke), cursor_y), fill=(255, 255, 250, 255))
        shadow = Image.new("RGBA", self.size, (0, 0, 0, 0))
        shadow.putalpha(canvas.getchannel("A").filter(ImageFilter.GaussianBlur(self.stroke * 1.1)))
        canvas = Image.alpha_composite(shadow, canvas)
        if opacity < 1:
            canvas.putalpha(canvas.getchannel("A").point(lambda value: round(value * opacity)))
        return canvas


def encode_overlay(artwork, fps, destination):
    with tempfile.TemporaryFile() as errors:
        process = subprocess.Popen([
            "ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pixel_format", "rgba",
            "-video_size", f"{artwork.size[0]}x{artwork.size[1]}", "-framerate", str(fps),
            "-i", "pipe:0", "-an", "-c:v", "qtrle", "-pix_fmt", "argb", str(destination),
        ], stdin=subprocess.PIPE, stderr=errors)
        try:
            for frame_index in range(artwork.callout.end - artwork.callout.start):
                process.stdin.write(artwork.frame(frame_index, fps).tobytes())
        finally:
            process.stdin.close()
            process.wait()
        if process.returncode:
            errors.seek(0)
            raise RuntimeError(errors.read().decode(errors="replace"))


def export(source, destination, recipe, report_path=None):
    source = Path(source).resolve(strict=True)
    destination = Path(destination).resolve()
    if source == destination or destination.exists():
        raise ValueError("Choose a new output path; existing files are never overwritten.")
    report_path = Path(report_path).resolve() if report_path else destination.with_suffix(".edit.json")
    if report_path in (source, destination):
        raise ValueError("The report must have its own path.")
    metadata = probe(source)
    video = next(stream for stream in metadata["streams"] if stream["codec_type"] == "video")
    fps = Fraction(video["avg_frame_rate"])
    duration = float(metadata["format"]["duration"])
    callouts, style = resolve_recipe(recipe, fps, duration)
    has_audio = any(stream["codec_type"] == "audio" for stream in metadata["streams"])
    before_audio = audio_hash(source) if has_audio else None
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="shorts_edit_") as temporary:
        temporary = Path(temporary)
        arguments = ["ffmpeg", "-v", "error", "-y", "-i", str(source)]
        filters = []
        for index, callout in enumerate(callouts, 1):
            artwork = CalloutArtwork(callout, style, video["width"], video["height"])
            overlay = temporary / f"callout_{index}.mov"
            encode_overlay(artwork, fps, overlay)
            arguments += ["-i", str(overlay)]
            start, end, exit_start, exit_length = [float(value / fps) for value in
                                                   (callout.start, callout.end, callout.exit_start, callout.exit)]
            filters.append(f"[{index}:v]setpts=PTS-STARTPTS+{start:.9f}/TB[o{index}]")
            previous = "0:v" if index == 1 else f"v{index - 1}"
            movement = f"{artwork.left}-if(gte(t,{exit_start:.9f}),{artwork.slide}*min(1,(t-{exit_start:.9f})/{exit_length:.9f}),0)"
            filters.append(f"[{previous}][o{index}]overlay=x='{movement}':y={artwork.top}:"
                           f"eof_action=pass:repeatlast=0:enable='gte(t,{start:.9f})*lt(t,{end:.9f})'[v{index}]")
        encoded = temporary / "edited.mp4"
        arguments += ["-filter_complex", ";".join(filters), "-map", f"[v{len(callouts)}]", "-map", "0:a?",
                      "-map_metadata", "0", "-c:v", "libx264", "-crf", "14", "-preset", "medium",
                      "-pix_fmt", "yuv420p", "-r", str(fps), "-c:a", "copy", "-t", str(duration),
                      "-movflags", "+faststart", str(encoded)]
        print("Encoding typewriter callouts; copying source audio.", flush=True)
        run(arguments)
        after = probe(encoded)
        after_video = next(stream for stream in after["streams"] if stream["codec_type"] == "video")
        if abs(float(after["format"]["duration"]) - duration) > max(0.001, float(1 / fps)):
            raise RuntimeError("Edited duration differs from the source.")
        for field in ("width", "height", "avg_frame_rate", "nb_frames"):
            if field in video and video[field] != after_video.get(field):
                raise RuntimeError(f"Edited video changed {field}.")
        after_audio = audio_hash(encoded) if has_audio else None
        if before_audio != after_audio:
            raise RuntimeError("Audio packet verification failed.")
        shutil.copyfile(encoded, destination)
    report = {
        "source": str(source), "source_sha256": file_hash(source), "output": str(destination),
        "duration_seconds": duration, "fps": str(fps), "style": style,
        "callouts": [callout.timing(fps) for callout in callouts],
        "audio_packets_identical": before_audio == after_audio, "audio_sha256": before_audio,
        "frame_count_preserved": video.get("nb_frames") == after_video.get("nb_frames"),
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--recipe", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    recipe = json.loads(arguments.recipe.read_text(encoding="utf-8-sig"))
    export(arguments.input, arguments.output, recipe, arguments.report)


if __name__ == "__main__":
    main()
