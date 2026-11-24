"""DF station minimap icons: the station's own loop aerial (UE render) with the flag of the holder (the supply icon
flag renders), upright like the supply flags.

Run:  python compose_df_icons.py      (after run_render.ps1 wrote Saved/OderWarthe/stations/icons/renders/Loop_*)
Uses the supply icon compositor (Source/Tools/SupplyPoint/MinimapIcons/compose_supply_minimap_icons.py) for the
cut-outs, grading, keyline / outline / shadow and the GPU-like proof drawing, so both icon sets match.
Writes Final/T_MinimapIcon_DFStation_<holder>.png (64 px game texture) + _Master256.png and Final/proof_sheet.png.
"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "SupplyPoint", "MinimapIcons"))
import compose_supply_minimap_icons as sp  # noqa: E402

DF_RENDERS = os.path.join(sp.PROJECT, "Saved", "OderWarthe", "stations", "icons", "renders")
SUPPLY_FINAL = os.path.join(HERE, "..", "..", "SupplyPoint", "MinimapIcons", "Final")
LOOP_JOB = "Loop_Side20"
LOOP = dict(brightness=1.9, contrast=1.35, saturation=1.1)
# EMinimapIconType value -> holder flag render (Flag_<name>, Attachment_FlagPole as the supply icons use)
DF_ICONS = {
    "DFStation_DirectorOfShield": "Flag_DirectorOfShield",
    "DFStation_DirectorOfForge": "Flag_DirectorOfForge",
    "DFStation_GerBreakthrough": "Flag_GerBreakthrough",
    "DFStation_GerStrikeDivision": "Flag_GerStrikeDivision",
}
DRAW_SIZE = 32


def loop_cutout(job):
    colour = Image.open(os.path.join(DF_RENDERS, job + "_color.png")).convert("RGB")
    alpha = Image.open(os.path.join(DF_RENDERS, job + "_mask.png")).convert("RGBA").getchannel("A")
    alpha = alpha.point(lambda v: 255 - v)
    colour.putalpha(alpha)
    return colour.crop(alpha.point(lambda v: 255 if v > 24 else 0).getbbox())


def df_icon(flag_job):
    """Loop aerial bottom-left, the holder's flag top-right over it."""
    art = sp.Art()
    art.place(LOOP_JOB, (2, 10, 40, 62), align="bottom", **LOOP)
    art.place(flag_job + "_Top", (27, 2, 62, 37), **sp.FLAG)
    return art.finish(keyline=1.5)


def proof(icons):
    """DF icons next to the supply flags at real size and 8x on the three minimap backgrounds."""
    supply = [Image.open(os.path.join(SUPPLY_FINAL, "T_MinimapIcon_SupplyFlag_%s.png" % n)).convert("RGBA")
              for n in ("DirectorOfShield", "GerStrikeDivision")]
    row = [("supply", t) for t in supply] + icons
    zoom, gap = 8, 5
    patch_w = 14 + len(row) * (DRAW_SIZE + gap) + 26
    patch_h = 54
    backgrounds = sp.load_backgrounds(patch_w, patch_h)
    margin, title_h = 24, 86
    row_h = patch_h + 8 + patch_h * zoom + 44
    sheet = Image.new("RGBA", (margin * 2 + patch_w * zoom, title_h + len(backgrounds) * row_h + margin),
                      (26, 27, 30, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "DF station icons: loop aerial + the flag of the holder (32 px)", font=sp.font(26),
              fill=(232, 232, 226))
    draw.text((margin, 50), "Left to right: supply flag Shield, supply flag Strike Division (for comparison), "
              + ", ".join(name for name, _ in icons) + ", unit blips. Real size above, 8x below.",
              font=sp.font(15), fill=(160, 162, 160))
    for r, (name, background) in enumerate(backgrounds):
        canvas = background.convert("RGBA")
        x = 14.37
        for _, texture in row:
            sp.gpu_draw(canvas, texture, x + DRAW_SIZE / 2.0 + 0.2, 27.3, DRAW_SIZE)
            x += DRAW_SIZE + gap
        sp.blip(canvas, x + 6.4, 12.3, 6, sp.BLIP_UNIT)
        sp.blip(canvas, x + 7.7, 30.2, 10, sp.BLIP_BUILDING)
        sp.blip(canvas, x + 12.3, 44.8, 6, sp.BLIP_ENEMY)
        top = title_h + r * row_h
        draw.text((margin, top), name, font=sp.font(15), fill=(160, 162, 160))
        sheet.paste(canvas, (margin, top + 24))
        sheet.paste(canvas.resize((patch_w * zoom, patch_h * zoom), Image.NEAREST), (margin, top + 24 + patch_h + 8))
    sheet.convert("RGB").save(os.path.join(HERE, "Final", "proof_sheet.png"))


def main():
    sp._CUTS[LOOP_JOB] = loop_cutout(LOOP_JOB)
    folder = os.path.join(HERE, "Final")
    icons = [(value, sp.export(df_icon(job), folder, "T_MinimapIcon_" + value)) for value, job in DF_ICONS.items()]
    proof(icons)
    print("written to", folder)


if __name__ == "__main__":
    main()
