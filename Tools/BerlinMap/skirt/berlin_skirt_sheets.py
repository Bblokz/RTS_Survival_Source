"""Contact sheets of the before / after pictures (PIL): Saved/MapSkirt/Ger_Berlin/sheet_*.jpg

    python berlin_skirt_sheets.py
"""
import os

from PIL import Image, ImageDraw

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/'
W, H = 640, 360


def tile(tag, name):
    p = ROOT + 'shots_%s/%s.png' % (tag, name)
    if not os.path.exists(p):
        im = Image.new('RGB', (W, H), (60, 0, 60))
    else:
        im = Image.open(p).convert('RGB').resize((W, H), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 250, 16], fill=(0, 0, 0))
    d.text((4, 2), '%s  %s' % (tag, name), fill=(255, 255, 0))
    return im


def pairs(out, names):
    sheet = Image.new('RGB', (2 * W, len(names) * H))
    for k, n in enumerate(names):
        sheet.paste(tile('before', n), (0, k * H))
        sheet.paste(tile('after', n), (W, k * H))
    sheet.save(ROOT + out, quality=88)


def grid(out, tag, names, cols=2):
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * W, rows * H))
    for k, n in enumerate(names):
        sheet.paste(tile(tag, n), ((k % cols) * W, (k // cols) * H))
    sheet.save(ROOT + out, quality=88)


pairs('sheet_edges_default.jpg', ['edge_N_default', 'edge_S_default', 'edge_E_default', 'edge_W_default'])
pairs('sheet_corners_default.jpg', ['corner_NE_default', 'corner_NW_default', 'corner_SE_default', 'corner_SW_default'])
pairs('sheet_edges_out.jpg', ['edge_N_out', 'edge_S_out', 'edge_E_out', 'edge_W_out'])
pairs('sheet_corners_out.jpg', ['corner_NE_out', 'corner_NW_out', 'corner_SE_out', 'corner_SW_out'])
pairs('sheet_river_rail.jpg', ['river_W_mouth', 'river_N_mouth', 'rail_E_end', 'rail_E_end_out'])
pairs('sheet_fog_join.jpg', ['fog_edge_E_out', 'fog_corner_SE_out', 'join_hill_E', 'join_flat_S'])
pairs('sheet_cine.jpg', ['cine_E_out', 'cine_NE_out', 'cine_N_out', 'cine_W_river'])
grid('sheet_after_cine2.jpg', 'after', ['cine_S_out', 'cine_N_river', 'cine_SW_out', 'x_join_far_E'])
grid('sheet_after_join.jpg', 'after', ['x_join_flat_S', 'x_join_hill_E', 'x_join_river_W', 'x_join_river_N', 'x_join_corner_NE',
                                       'x_join_top_E'])
print('sheets written')
