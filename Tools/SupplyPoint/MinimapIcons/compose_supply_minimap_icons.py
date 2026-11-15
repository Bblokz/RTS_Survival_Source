"""Supply truck / supply point minimap icons made from renders of the game's own meshes and materials.

Run:  python compose_supply_minimap_icons.py      (after run_render.ps1 wrote the cut-outs)

Parts come from Saved/SupplyPointMinimapIcons/renders (render_supply_parts.py: headless UnrealEditor-Cmd, the ability
icon capture pipeline): <job>_color.png is the tonemapped render, <job>_mask.png its inverted coverage.

The minimap draws these through the custom-icon path (EMinimapIconType -> DA_MinimapIcon -> FSlateBrush,
UW_MiniMap::DrawCustomMiniMapTextureIcon), untinted, so the icons are baked colour with the dark outline and soft
shadow of T_MinimapIcon_IncomingWave(Badge): 64x64 RGBA, no mips. Written as 64 px (texture) and 256 px (master).
A render is low contrast at 20-32 px, so cut-outs are graded up and get a light keyline inside the dark outline.

The proof sheet samples the 64 px texture the way the GPU does without mips (plain bilinear, no prefilter) at the real
draw size, with rotation and a sub-pixel offset, over crops of real minimap screenshots.
The earlier drawn (Pillow) icons are in _drawn_old.
"""
import math
import os

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
RENDERS = os.path.join(PROJECT, "Saved", "SupplyPointMinimapIcons", "renders")

GRID = 64          # design units == texels of the game texture
SS = 16            # supersampling of the design grid
TEX = 64           # game texture resolution
MASTER = 256       # 4x master

OUTLINE = (18, 14, 12)
YELLOW = (255, 206, 48)
KEYLINE = (250, 246, 232)
PLATE = (30, 32, 38)

OUTLINE_WIDTH = 2.0    # design units, outside the keyline
SHADOW_BLUR = 1.6
SHADOW_OFFSET = (0.6, 1.2)
SHADOW_OPACITY = 0.6

# Draw sizes in Slate pixels (= screen pixels at 1080p).
TRUCK_DRAW_SIZE = 28
POINT_DRAW_SIZE = 32

# Unit blips of BP_FogOfWar_Manager: tinted discs, sizes in Slate pixels.
BLIP_UNIT = (0, 255, 0)
BLIP_BUILDING = (0, 0, 255)
BLIP_ENEMY = (255, 0, 0)

_CUTS = {}


# Parts of a render: (job, region in render pixels). The FieldSupply mesh is a building's whole set of props spread
# over its yard, so one stack is cut from it.
REGIONS = {
    "Jerrycans_Side": ("FieldSupply_Side", (1392, 800, 1700, 1125)),
}


def cutout(name):
    """A render (or a REGIONS part of one) as RGBA, cropped to what it covers."""
    if name in REGIONS and name not in _CUTS:
        job, region = REGIONS[name]
        colour = Image.open(os.path.join(RENDERS, job + "_color.png")).convert("RGB").crop(region)
        alpha = Image.open(os.path.join(RENDERS, job + "_mask.png")).convert("RGBA").getchannel("A").crop(region)
        alpha = alpha.point(lambda v: 255 - v)
        colour.putalpha(alpha)
        _CUTS[name] = colour.crop(alpha.point(lambda v: 255 if v > 24 else 0).getbbox())
    if name not in _CUTS:
        colour = Image.open(os.path.join(RENDERS, name + "_color.png")).convert("RGB")
        alpha = Image.open(os.path.join(RENDERS, name + "_mask.png")).convert("RGBA").getchannel("A")
        alpha = alpha.point(lambda v: 255 - v)
        colour.putalpha(alpha)
        _CUTS[name] = colour.crop(alpha.point(lambda v: 255 if v > 24 else 0).getbbox())
    return _CUTS[name]


def grade(img, brightness=1.0, contrast=1.0, saturation=1.0):
    """Lifts a render so it still reads at 20-32 px."""
    alpha = img.getchannel("A")
    rgb = img.convert("RGB")
    rgb = ImageEnhance.Brightness(rgb).enhance(brightness)
    rgb = ImageEnhance.Contrast(rgb).enhance(contrast)
    rgb = ImageEnhance.Color(rgb).enhance(saturation)
    rgb.putalpha(alpha)
    return rgb


def grow(mask, width):
    """The mask grown by width design units."""
    grown = mask.filter(ImageFilter.GaussianBlur(width * SS * 0.55))
    return grown.point(lambda v: 255 if v >= 26 else 0).filter(ImageFilter.GaussianBlur(SS * 0.25))


class Art:
    """Icon canvas on the 64-unit grid: drawn backdrop shapes and placed render cut-outs."""

    def __init__(self):
        self.img = Image.new("RGBA", (GRID * SS, GRID * SS), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.img)

    def disc(self, cx, cy, r, colour):
        self.draw.ellipse([(cx - r) * SS, (cy - r) * SS, (cx + r) * SS, (cy + r) * SS], fill=colour + (255,))

    def place(self, name, box, rotate=0, align="centre", keyline=0.0, **grading):
        """Fits a cut-out into box (x0, y0, x1, y1), keeping its aspect; align "bottom" stands it on the box floor.
        keyline > 0 puts a light line of that width around this part alone (parts on a dark plate)."""
        cut = cutout(name)
        if rotate:
            cut = cut.rotate(rotate, expand=True, resample=Image.BICUBIC)
        if grading:
            cut = grade(cut, **grading)
        x0, y0, x1, y1 = [v * SS for v in box]
        scale = min((x1 - x0) / cut.width, (y1 - y0) / cut.height)
        size = (max(1, int(round(cut.width * scale))), max(1, int(round(cut.height * scale))))
        cut = cut.resize(size, Image.LANCZOS)
        x = int(round((x0 + x1 - size[0]) / 2))
        y = int(round(y1 - size[1])) if align == "bottom" else int(round((y0 + y1 - size[1]) / 2))
        layer = Image.new("RGBA", self.img.size, (0, 0, 0, 0))
        layer.paste(cut, (x, y))
        if keyline > 0:
            edge = grow(layer.getchannel("A").point(lambda v: 255 if v > 96 else 0), keyline)
            self.img.alpha_composite(Image.merge("RGBA", (*Image.new("RGB", self.img.size, KEYLINE).split(), edge)))
        self.img.alpha_composite(layer)

    def finish(self, keyline=1.3):
        """Adds the light keyline (0 = none), the dark outline and the shadow; returns the supersampled icon."""
        silhouette = self.img.getchannel("A").point(lambda v: 255 if v > 96 else 0)
        key_mask = grow(silhouette, keyline) if keyline > 0 else silhouette
        outline_mask = grow(silhouette, keyline + OUTLINE_WIDTH)

        shadow_mask = Image.new("L", self.img.size, 0)
        shadow_mask.paste(outline_mask, (int(SHADOW_OFFSET[0] * SS), int(SHADOW_OFFSET[1] * SS)))
        shadow_mask = shadow_mask.filter(ImageFilter.GaussianBlur(SHADOW_BLUR * SS))
        shadow_mask = shadow_mask.point(lambda v: int(v * SHADOW_OPACITY))

        def flat(colour, mask):
            return Image.merge("RGBA", (*Image.new("RGB", self.img.size, colour).split(), mask))

        out = flat((0, 0, 0), shadow_mask)
        out.alpha_composite(flat(OUTLINE, outline_mask))
        if keyline > 0:
            out.alpha_composite(flat(KEYLINE, key_mask))
        out.alpha_composite(self.img)
        return out


CAMO = dict(brightness=1.12, contrast=1.35, saturation=1.35)     # the Blitz: sand camo is close to the minimap sand
STOCK = dict(brightness=1.1, contrast=1.25, saturation=1.3)      # resource stockpiles
CANS = dict(brightness=1.6, contrast=1.3, saturation=1.3)       # the jerrycan stack is dark olive
CRATE = dict(brightness=1.35, contrast=1.3, saturation=1.2)      # the crate is dark olive


# ---------------------------------------------------------------------------------------------------------------------
# V1: top-down cut-outs. The truck points along +X and turns with the actor's yaw.
# ---------------------------------------------------------------------------------------------------------------------
def v1_truck():
    art = Art()
    art.place("Truck_Top", (5, 16, 59, 48), **CAMO)
    return art.finish()


def v1_point():
    art = Art()
    # Two supply crates side by side with radixite crystals on them: fewer, larger shapes than the three stockpiles.
    art.place("Crate_Top", (6, 9, 58, 31), **CRATE)
    art.place("Crate_Top", (6, 33, 58, 55), **CRATE)
    art.place("RadixiteHQ_Top", (15, 18, 49, 46), **STOCK)
    return art.finish()


# ---------------------------------------------------------------------------------------------------------------------
# V2: three-quarter views on a roundel. Upright: static rotation.
# ---------------------------------------------------------------------------------------------------------------------
def roundel(art, radius=26.5, ring=3.2):
    art.disc(32, 32, radius, YELLOW)
    art.disc(32, 32, radius - ring, PLATE)


def v2_truck():
    art = Art()
    roundel(art)
    art.place("Truck_Hero", (6, 8, 58, 56), keyline=1.1, **CAMO)
    return art.finish(keyline=0)


def v2_point():
    art = Art()
    roundel(art)
    art.place("Metal_Hero", (7, 8, 35, 36), keyline=1.0, **STOCK)
    art.place("Parts_Hero", (30, 10, 57, 38), keyline=1.0, **STOCK)
    art.place("Radixite_Hero", (12, 28, 52, 57), keyline=1.0, **STOCK)
    return art.finish(keyline=0)


# ---------------------------------------------------------------------------------------------------------------------
# V3: side views. Upright: static rotation.
# ---------------------------------------------------------------------------------------------------------------------
def v3_truck():
    art = Art()
    art.place("Truck_Side", (5, 14, 59, 50), **CAMO)
    return art.finish()


def v3_point(flag=True):
    art = Art()
    if flag:
        art.place("Holder_Side", (11, 5, 16, 58), rotate=90)
        art.place("Banner_Roll_Side", (16, 6, 49, 26), align="bottom", brightness=1.15, contrast=1.2)
        art.place("Jerrycans_Side", (18, 29, 38, 46), align="bottom", **CANS)
        art.place("Radixite_Side", (37, 28, 56, 46), align="bottom", **STOCK)
        art.place("Crate_Side", (15, 45, 57, 57), align="bottom", **STOCK)
    else:
        art.place("Jerrycans_Side", (8, 13, 33, 42), align="bottom", **CANS)
        art.place("Radixite_Side", (31, 14, 57, 42), align="bottom", **STOCK)
        art.place("Crate_Side", (7, 41, 57, 57), align="bottom", **STOCK)
    return art.finish()


VERSIONS = [
    ("V1", "top-down cut-outs: Blitz from above, turns with heading / two supply crates with radixite crystals "
           "from above", v1_truck, v1_point, True),
    ("V2", "three-quarter renders on a roundel: Blitz / the three HQ stockpiles. Upright art: static rotation, and needs "
           "a counter-rotation in code on maps whose minimap is turned (StartDirection N/S/W)",
     v2_truck, v2_point, False),
    ("V3", "side views: Blitz / Soviet banner on a pole over crate, jerrycans and radixite. Upright art: static rotation, "
           "and needs a counter-rotation in code on maps whose minimap is turned",
     v3_truck, v3_point, False),
]


def clean_transparent(img):
    """Black RGB under fully transparent texels, so straight-alpha bilinear filtering cannot fringe."""
    black = Image.new("RGBA", img.size, (0, 0, 0, 0))
    black.paste(img, (0, 0), img.getchannel("A").point(lambda v: 255 if v > 0 else 0))
    return black


def export(big, folder, name):
    os.makedirs(folder, exist_ok=True)
    texture = clean_transparent(big.resize((TEX, TEX), Image.LANCZOS))
    master = clean_transparent(big.resize((MASTER, MASTER), Image.LANCZOS))
    texture.save(os.path.join(folder, name + ".png"))
    master.save(os.path.join(folder, name + "_Master256.png"))
    return texture


# ---------------------------------------------------------------------------------------------------------------------
# Proof sheet
# ---------------------------------------------------------------------------------------------------------------------
def gpu_draw(canvas, texture, cx, cy, size, rotation_degrees=0.0):
    """Draws the texture as Slate does: a size x size quad centred on (cx, cy), turned clockwise, bilinear, no mips."""
    scale = texture.width / float(size)
    c, s = math.cos(math.radians(rotation_degrees)), math.sin(math.radians(rotation_degrees))
    half = texture.width / 2.0
    coeffs = (c * scale, s * scale, half - (c * cx + s * cy) * scale,
              -s * scale, c * scale, half - (-s * cx + c * cy) * scale)
    layer = texture.transform(canvas.size, Image.AFFINE, coeffs, resample=Image.BILINEAR)
    canvas.alpha_composite(layer)


def blip(canvas, cx, cy, size, colour):
    """Unit blip: a fully rounded box tinted with the unit colour."""
    k = 8
    big = Image.new("RGBA", (canvas.width * k, canvas.height * k), (0, 0, 0, 0))
    r = size / 2.0
    ImageDraw.Draw(big).ellipse([(cx - r) * k, (cy - r) * k, (cx + r) * k, (cy + r) * k], fill=colour + (255,))
    canvas.alpha_composite(big.resize(canvas.size, Image.BOX))


def load_backgrounds(width, height):
    """Dark fogged ground and lit sand from real minimap screenshots, plus a brightened sand as the bright case."""
    fog_src = Image.open(os.path.join(PROJECT, "Saved", "AnalogGauges", "hud_minimap_2160.png")).convert("RGB")
    fog = fog_src.crop((110, 300, 110 + width * 2, 300 + height * 2)).resize((width, height), Image.LANCZOS)
    sand_src = Image.open(os.path.join(PROJECT, "Saved", "AgentNotes", "wave_ui_review",
                                       "marker_minimap_zoom.png")).convert("RGB")
    sand = sand_src.crop((100, 190, 100 + width, 190 + height))
    bright = ImageEnhance.Brightness(sand).enhance(1.75)
    return [("fogged ground (real crop)", fog), ("lit sand (real crop)", sand), ("bright ground (sand x1.75)", bright)]


def font(size):
    for name in ("segoeui.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def scene(background, truck, point, truck_rotation):
    """One minimap patch at real pixel size: truck, supply point and ordinary blips."""
    canvas = background.convert("RGBA")
    gpu_draw(canvas, truck, 22.37, 26.61, TRUCK_DRAW_SIZE, truck_rotation)
    gpu_draw(canvas, point, 60.55, 27.3, POINT_DRAW_SIZE)
    blip(canvas, 88.4, 12.3, 6, BLIP_UNIT)          # vehicle
    blip(canvas, 97.2, 15.6, 6, BLIP_UNIT)
    blip(canvas, 92.5, 21.5, 1, BLIP_UNIT)          # squad soldiers
    blip(canvas, 95.5, 23.5, 1, BLIP_UNIT)
    blip(canvas, 89.7, 33.2, 10, BLIP_BUILDING)     # HQ
    blip(canvas, 101.3, 31.1, 8, BLIP_BUILDING)     # building expansion
    blip(canvas, 92.3, 45.8, 6, BLIP_ENEMY)
    blip(canvas, 100.6, 44.1, 6, BLIP_ENEMY)
    return canvas


def proof_sheet(textures):
    patch_w, patch_h, zoom = 110, 54, 8
    backgrounds = load_backgrounds(patch_w, patch_h)
    margin, label_w, gap = 24, 250, 18
    column_w = patch_w * zoom
    title_h = 96
    row_h = patch_h * zoom + patch_h + 54
    ladder_row_h = 40 + 6 + 160 + 16
    ladder_h = 50 + len(textures) * ladder_row_h
    sheet_w = margin * 2 + label_w + len(backgrounds) * (column_w + gap)
    sheet_h = title_h + len(textures) * row_h + ladder_h + margin
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (26, 27, 30, 255))
    draw = ImageDraw.Draw(sheet)
    big, mid, small = font(26), font(18), font(15)
    text, dim = (232, 232, 226), (160, 162, 160)

    draw.text((margin, 14), "Supply truck + supply point minimap icons (renders of in-game meshes)", font=big, fill=text)
    draw.text((margin, 50),
              "Top strip of each cell = real size at 1080p (truck %d px, point %d px; blips 6 / 1 / 10 / 8 px), below it the "
              "same pixels at %dx. Icons are baked colour: the custom-icon path does not tint, so there is no enemy tint."
              % (TRUCK_DRAW_SIZE, POINT_DRAW_SIZE, zoom), font=small, fill=dim)
    draw.text((margin, 70),
              "Sampled as in game: 64 px texture, bilinear, no mips, sub-pixel position. Truck shown driving at 0 deg "
              "(only V1 follows the heading: it is also shown turned in the ladder at the bottom).", font=small, fill=dim)

    for column, (name, _) in enumerate(backgrounds):
        draw.text((margin + label_w + column * (column_w + gap), title_h - 2 - 0), name, font=small, fill=dim)

    for row, (version, idea, truck, point, turns) in enumerate(textures):
        top = title_h + 22 + row * row_h
        draw.text((margin, top), version, font=big, fill=text)
        words, line, lines = idea.split(), "", []
        for word in words:
            if draw.textlength(line + " " + word, font=small) > label_w - 16:
                lines.append(line)
                line = word
            else:
                line = (line + " " + word).strip()
        lines.append(line)
        for index, each in enumerate(lines):
            draw.text((margin, top + 40 + index * 20), each, font=small, fill=dim)
        # The raw textures, for reference.
        sheet.alpha_composite(truck.resize((128, 128), Image.NEAREST), (margin, top + 150))
        sheet.alpha_composite(point.resize((128, 128), Image.NEAREST), (margin + 112, top + 150))
        draw.text((margin, top + 284), "64 px textures at 2x", font=small, fill=dim)

        for column, (_, background) in enumerate(backgrounds):
            left = margin + label_w + column * (column_w + gap)
            patch = scene(background, truck, point, 0.0)
            sheet.paste(patch, (left, top))
            sheet.paste(patch.resize((column_w, patch_h * zoom), Image.NEAREST), (left, top + patch_h + 8))

    # Ladder: draw sizes and rotations at real size and 4x, on fog and sand.
    top = title_h + len(textures) * row_h + 10
    draw.text((margin, top), "Draw size ladder: truck 20 / 24 / 28 / 32 px, point 20 / 24 / 32 px, then the truck at 28 px "
                             "heading 35 / 90 / 160 deg (V2 and V3 are upright art: not turned). Real size above, 4x below.",
              font=mid, fill=text)
    ladder_zoom = 4
    strip_w, strip_h = 330, 40
    for row, (version, _, truck, point, turns) in enumerate(textures):
        y = top + 40 + row * ladder_row_h
        draw.text((margin, y + 60), version, font=big, fill=text)
        for column, (_, background) in enumerate(backgrounds[:2]):
            base = background.resize((strip_w, strip_h), Image.LANCZOS).convert("RGBA")
            x = 6.3
            for size in (20, 24, 28, 32):
                gpu_draw(base, truck, x + size / 2.0, 20.4, size)
                x += size + 5
            x += 6
            for size in (20, 24, 32):
                gpu_draw(base, point, x + size / 2.0, 20.4, size)
                x += size + 5
            if turns:
                x += 6
                for angle in (35, 90, 160):
                    gpu_draw(base, truck, x + TRUCK_DRAW_SIZE / 2.0 + 0.6, 20.7, TRUCK_DRAW_SIZE, angle)
                    x += TRUCK_DRAW_SIZE + 4
            left = margin + 70 + column * (strip_w * ladder_zoom + gap)
            sheet.paste(base, (left, y))
            sheet.paste(base.resize((strip_w * ladder_zoom, strip_h * ladder_zoom), Image.NEAREST),
                        (left, y + strip_h + 6))
    sheet.convert("RGB").save(os.path.join(HERE, "proof_sheet.png"))


# ---------------------------------------------------------------------------------------------------------------------
# Final set (user pick): the V3 side-view truck, and for the supply point only the flag of whoever holds it.
# One file per EMinimapIconType value: Final/T_MinimapIcon_<value>.png.
# ---------------------------------------------------------------------------------------------------------------------
# The flag with the top of its pole, out of the Flag_<name> renders (Attachment_FlagPole, 2048 px).
FLAG_REGION = (620, 60, 1400, 830)
# EMinimapIconType value -> render job. GerItalian is missing on purpose: the game has no Italian flag texture or
# material, and cell 3 of the FactionAtlas is empty.
FINAL_FLAGS = {
    "SupplyFlag_DirectorOfShield": "Flag_DirectorOfShield",
    "SupplyFlag_DirectorOfForge": "Flag_DirectorOfForge",
    "SupplyFlag_DirectorOfHarvest": "Flag_DirectorOfHarvest",
    "SupplyFlag_GerBreakthrough": "Flag_GerBreakthrough",
    "SupplyFlag_GerStrikeDivision": "Flag_GerStrikeDivision",
}
FLAG = dict(brightness=1.25, contrast=1.2, saturation=1.25)
for _job in FINAL_FLAGS.values():
    REGIONS[_job + "_Top"] = (_job, FLAG_REGION)


def final_flag(job):
    art = Art()
    art.place(job + "_Top", (5, 5, 59, 59), **FLAG)
    return art.finish(keyline=1.5)


def final_proof(truck, flags):
    """Truck and every flag at real size and 8x on the three backgrounds."""
    zoom, gap = 8, 5
    patch_w = 14 + TRUCK_DRAW_SIZE + gap + len(flags) * (POINT_DRAW_SIZE + gap) + 26
    patch_h = 54
    backgrounds = load_backgrounds(patch_w, patch_h)
    margin, title_h = 24, 86
    row_h = patch_h + 8 + patch_h * zoom + 44
    sheet = Image.new("RGBA", (margin * 2 + patch_w * zoom, title_h + len(backgrounds) * row_h + margin),
                      (26, 27, 30, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "Supply icons, final set: truck + the flag of the faction holding the point",
              font=font(26), fill=(232, 232, 226))
    draw.text((margin, 50), "Left to right: SupplyTruck (%d px), " % TRUCK_DRAW_SIZE
              + ", ".join(name for name, _ in flags) + " (%d px), unit blips. Real size above, %dx below. "
              "GerItalian: no flag asset in the game." % (POINT_DRAW_SIZE, zoom), font=font(15), fill=(160, 162, 160))
    for row, (name, background) in enumerate(backgrounds):
        canvas = background.convert("RGBA")
        x = 14.37
        gpu_draw(canvas, truck, x + TRUCK_DRAW_SIZE / 2.0, 27.61, TRUCK_DRAW_SIZE)
        x += TRUCK_DRAW_SIZE + gap
        for _, flag in flags:
            gpu_draw(canvas, flag, x + POINT_DRAW_SIZE / 2.0 + 0.2, 27.3, POINT_DRAW_SIZE)
            x += POINT_DRAW_SIZE + gap
        blip(canvas, x + 6.4, 12.3, 6, BLIP_UNIT)
        blip(canvas, x + 15.2, 17.6, 6, BLIP_UNIT)
        blip(canvas, x + 7.7, 30.2, 10, BLIP_BUILDING)
        blip(canvas, x + 12.3, 44.8, 6, BLIP_ENEMY)
        top = title_h + row * row_h
        draw.text((margin, top), name, font=font(15), fill=(160, 162, 160))
        sheet.paste(canvas, (margin, top + 24))
        sheet.paste(canvas.resize((patch_w * zoom, patch_h * zoom), Image.NEAREST), (margin, top + 24 + patch_h + 8))
    sheet.convert("RGB").save(os.path.join(HERE, "Final", "proof_sheet_final.png"))


def final_set():
    folder = os.path.join(HERE, "Final")
    truck = export(v3_truck(), folder, "T_MinimapIcon_SupplyTruck")
    flags = [(value, export(final_flag(job), folder, "T_MinimapIcon_" + value)) for value, job in FINAL_FLAGS.items()]
    final_proof(truck, flags)


def main():
    textures = []
    for version, idea, make_truck, make_point, turns in VERSIONS:
        folder = os.path.join(HERE, version)
        truck = export(make_truck(), folder, "T_MinimapIcon_SupplyTruck_" + version)
        point = export(make_point(), folder, "T_MinimapIcon_SupplyPoint_" + version)
        textures.append((version, idea, truck, point, turns))
    no_flag = export(v3_point(flag=False), os.path.join(HERE, "V3"), "T_MinimapIcon_SupplyPoint_V3_NoFlag")
    textures.append(("V3b", "no flag: the V3 point without the banner (the only flag mesh in the game is Soviet); same truck",
                     textures[-1][2], no_flag, False))
    proof_sheet(textures)
    final_set()
    print("written to", HERE)


if __name__ == "__main__":
    main()
