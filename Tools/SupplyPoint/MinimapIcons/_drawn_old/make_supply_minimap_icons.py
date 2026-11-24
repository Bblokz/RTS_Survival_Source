"""Supply truck / supply point minimap icons: three alternative versions plus a proof sheet.

Run:  python make_supply_minimap_icons.py

The minimap draws these through the custom-icon path (EMinimapIconType -> DA_MinimapIcon -> FSlateBrush,
UW_MiniMap::DrawCustomMiniMapTextureIcon). That path draws the texture untinted (FLinearColor::White), so the icons are
baked full colour with a dark outline and a soft shadow, like T_MinimapIcon_IncomingWave(Badge): 64x64 RGBA, no mips,
drawn at M_SizeXY Slate pixels. Art is drawn on a 64-unit grid, supersampled 16x, and written as 64 px (texture) and
256 px (master).

The proof sheet samples the 64 px texture the way the GPU does without mips (plain bilinear, no prefilter) at the real
draw size, with rotation and a sub-pixel offset, over crops of real minimap screenshots.
"""
import math
import os

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))

GRID = 64          # design units == texels of the game texture
SS = 16            # supersampling of the design grid
TEX = 64           # game texture resolution
MASTER = 256       # 4x master

OUTLINE = (18, 14, 12)
YELLOW = (255, 206, 48)
WHITE = (246, 242, 230)
PLATE = (34, 36, 42)

OUTLINE_WIDTH = 2.4    # design units
SHADOW_BLUR = 1.6
SHADOW_OFFSET = (0.6, 1.2)
SHADOW_OPACITY = 0.6

# Draw sizes in Slate pixels (= screen pixels at 1080p).
TRUCK_DRAW_SIZE = 24
POINT_DRAW_SIZE = 32

# Unit blips of BP_FogOfWar_Manager: tinted discs, sizes in Slate pixels.
BLIP_UNIT = (0, 255, 0)
BLIP_BUILDING = (0, 0, 255)
BLIP_ENEMY = (255, 0, 0)


class Art:
    """Flat colour shapes on the 64-unit grid; everything drawn here gets the outline and shadow."""

    def __init__(self):
        self.img = Image.new("RGBA", (GRID * SS, GRID * SS), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.img)

    @staticmethod
    def _s(values):
        return [v * SS for v in values]

    def rect(self, x0, y0, x1, y1, colour, radius=0.0):
        self.draw.rounded_rectangle(self._s([x0, y0, x1, y1]), radius=radius * SS, fill=colour + (255,))

    def poly(self, points, colour):
        self.draw.polygon([(x * SS, y * SS) for x, y in points], fill=colour + (255,))

    def disc(self, cx, cy, r, colour):
        self.draw.ellipse(self._s([cx - r, cy - r, cx + r, cy + r]), fill=colour + (255,))

    def line(self, x0, y0, x1, y1, width, colour):
        self.draw.line(self._s([x0, y0, x1, y1]), fill=colour + (255,), width=int(round(width * SS)))

    def square_cross_crate(self, cx, cy, half, fill, strap=2.6, rim=2.4):
        """Top-down crate: a square with a rim and two diagonal straps. Looks the same at every 90 degree turn."""
        self.rect(cx - half, cy - half, cx + half, cy + half, fill, radius=1.0)
        inner = half - rim
        self.line(cx - inner, cy - inner, cx + inner, cy + inner, strap, OUTLINE)
        self.line(cx - inner, cy + inner, cx + inner, cy - inner, strap, OUTLINE)

    def finish(self):
        """Adds the outline and shadow; returns the supersampled RGBA icon."""
        silhouette = self.img.getchannel("A")
        grown = silhouette.filter(ImageFilter.GaussianBlur(OUTLINE_WIDTH * SS * 0.55))
        outline_mask = grown.point(lambda v: 255 if v >= 26 else 0).filter(ImageFilter.GaussianBlur(SS * 0.25))

        shadow_mask = Image.new("L", self.img.size, 0)
        shadow_mask.paste(outline_mask, (int(SHADOW_OFFSET[0] * SS), int(SHADOW_OFFSET[1] * SS)))
        shadow_mask = shadow_mask.filter(ImageFilter.GaussianBlur(SHADOW_BLUR * SS))
        shadow_mask = shadow_mask.point(lambda v: int(v * SHADOW_OPACITY))

        out = Image.new("RGBA", self.img.size, (0, 0, 0, 0))
        out.paste(Image.new("RGBA", self.img.size, (0, 0, 0, 255)), (0, 0), shadow_mask)
        out.alpha_composite(Image.merge("RGBA", (*Image.new("RGB", self.img.size, OUTLINE).split(), outline_mask)))
        out.alpha_composite(self.img)
        return out


# ---------------------------------------------------------------------------------------------------------------------
# V1: side views. Upright art: add with a static zero rotation.
# ---------------------------------------------------------------------------------------------------------------------
def v1_truck():
    art = Art()
    art.rect(8, 17, 36, 41, YELLOW, radius=3.0)                       # canvas-covered load bed
    art.poly([(39, 24), (47, 24), (49, 31), (56, 32), (56, 41), (39, 41)], WHITE)   # cab and bonnet
    art.poly([(41.5, 26.5), (45.6, 26.5), (46.8, 31), (41.5, 31)], OUTLINE)        # side window
    art.rect(8, 41, 56, 44, OUTLINE)                                   # chassis
    for wheel_x in (17, 46):
        art.disc(wheel_x, 45, 6.5, OUTLINE)
        art.disc(wheel_x, 45, 2.6, WHITE)
    return art.finish()


def v1_point():
    art = Art()
    art.rect(13, 37, 51, 56, YELLOW, radius=1.0)                       # crate
    art.rect(13, 44.6, 51, 47.4, OUTLINE)                              # plank gap
    art.rect(30.6, 37, 33.4, 56, OUTLINE)
    art.rect(21, 7, 24.6, 37, WHITE)                                   # pole
    art.poly([(24.6, 8), (51, 16.5), (24.6, 25)], WHITE)               # pennant
    return art.finish()


# ---------------------------------------------------------------------------------------------------------------------
# V2: top-down art in a roundel. The truck points along +X and turns with the actor's yaw; the point is symmetric.
# ---------------------------------------------------------------------------------------------------------------------
def roundel(art, radius, ring):
    art.disc(32, 32, radius, YELLOW)
    art.disc(32, 32, radius - ring, PLATE)


def v2_truck():
    art = Art()
    roundel(art, 26, 3.2)
    art.rect(15.5, 24, 34.5, 40, YELLOW, radius=1.5)                       # load bed under canvas
    art.rect(37, 25, 44.5, 39, WHITE, radius=1.0)                # cab roof
    art.rect(44.5, 27.5, 52, 36.5, WHITE, radius=1.0)                  # bonnet, narrower than the cab
    art.rect(42.4, 26.5, 44.9, 37.5, OUTLINE)                          # windscreen
    return art.finish()


def v2_point():
    art = Art()
    roundel(art, 25.5, 3.6)
    art.square_cross_crate(32, 32, 12.5, WHITE, strap=3.0, rim=1.5)
    return art.finish()


# ---------------------------------------------------------------------------------------------------------------------
# V3: symbols. Truck = crate pushed by a chevron along +X (turns with the yaw); point = crate on a diamond plate.
# ---------------------------------------------------------------------------------------------------------------------
def v3_truck():
    art = Art()
    art.square_cross_crate(22, 32, 14, YELLOW, strap=3.0, rim=1.5)
    art.poly([(40, 15), (47, 15), (58, 32), (47, 49), (40, 49), (51, 32)], WHITE)
    return art.finish()


def v3_point():
    art = Art()
    half = 26.5
    art.poly([(32, 32 - half), (32 + half, 32), (32, 32 + half), (32 - half, 32)], YELLOW)
    inner = half - 5.5
    art.poly([(32, 32 - inner), (32 + inner, 32), (32, 32 + inner), (32 - inner, 32)], PLATE)
    art.square_cross_crate(32, 32, 9.5, WHITE, strap=2.8, rim=1.2)
    return art.finish()


VERSIONS = [
    ("V1", "side view: Blitz silhouette / pennant on a crate. Upright art: needs a counter-rotation in code on maps whose minimap is turned (StartDirection N/S/W)", v1_truck, v1_point, False),
    ("V2", "top-down in a roundel: truck turns with heading / crate", v2_truck, v2_point, True),
    ("V3", "symbol: crate + chevron turns with heading / crate on a diamond", v3_truck, v3_point, True),
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
    ladder_h = 50 + 3 * ladder_row_h
    sheet_w = margin * 2 + label_w + len(backgrounds) * (column_w + gap)
    sheet_h = title_h + len(textures) * row_h + ladder_h + margin
    sheet = Image.new("RGBA", (sheet_w, sheet_h), (26, 27, 30, 255))
    draw = ImageDraw.Draw(sheet)
    big, mid, small = font(26), font(18), font(15)
    text, dim = (232, 232, 226), (160, 162, 160)

    draw.text((margin, 14), "Supply truck + supply point minimap icons", font=big, fill=text)
    draw.text((margin, 50),
              "Top strip of each cell = real size at 1080p (truck %d px, point %d px; blips 6 / 1 / 10 / 8 px), below it the "
              "same pixels at %dx. Icons are baked colour: the custom-icon path does not tint, so there is no enemy tint."
              % (TRUCK_DRAW_SIZE, POINT_DRAW_SIZE, zoom), font=small, fill=dim)
    draw.text((margin, 70),
              "Sampled as in game: 64 px texture, bilinear, no mips, sub-pixel position. Truck shown driving at 0 deg "
              "(V1 upright) and, for V2/V3, also turned in the ladder at the bottom.", font=small, fill=dim)

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
    draw.text((margin, top), "Draw size ladder: truck 16 / 20 / 24 / 32 px, point 20 / 24 / 32 px, then the truck at 24 px "
                             "heading 35 / 90 / 160 / 270 deg (V1 is upright art: not turned). Real size above, 4x below.",
              font=mid, fill=text)
    ladder_zoom = 4
    strip_w, strip_h = 330, 40
    for row, (version, _, truck, point, turns) in enumerate(textures):
        y = top + 40 + row * ladder_row_h
        draw.text((margin, y + 60), version, font=big, fill=text)
        for column, (_, background) in enumerate(backgrounds[:2]):
            base = background.resize((strip_w, strip_h), Image.LANCZOS).convert("RGBA")
            x = 6.3
            for size in (16, 20, 24, 32):
                gpu_draw(base, truck, x + size / 2.0, 20.4, size)
                x += size + 5
            x += 6
            for size in (20, 24, 32):
                gpu_draw(base, point, x + size / 2.0, 20.4, size)
                x += size + 5
            if turns:
                x += 6
                for angle in (35, 90, 160, 270):
                    gpu_draw(base, truck, x + TRUCK_DRAW_SIZE / 2.0 + 0.6, 20.7, TRUCK_DRAW_SIZE, angle)
                    x += TRUCK_DRAW_SIZE + 4
            left = margin + 70 + column * (strip_w * ladder_zoom + gap)
            sheet.paste(base, (left, y))
            sheet.paste(base.resize((strip_w * ladder_zoom, strip_h * ladder_zoom), Image.NEAREST),
                        (left, y + strip_h + 6))
    sheet.convert("RGB").save(os.path.join(HERE, "proof_sheet.png"))


def main():
    textures = []
    for version, idea, make_truck, make_point, turns in VERSIONS:
        folder = os.path.join(HERE, version)
        truck = export(make_truck(), folder, "T_MinimapIcon_SupplyTruck_" + version)
        point = export(make_point(), folder, "T_MinimapIcon_SupplyPoint_" + version)
        textures.append((version, idea, truck, point, turns))
    proof_sheet(textures)
    print("written to", HERE)


if __name__ == "__main__":
    main()
