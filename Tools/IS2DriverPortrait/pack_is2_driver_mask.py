"""Packs the IS-2 driver talking mask for M_UI_IS2DriverPribory from the Blender passes
(CommanderPortraits/Tools/is2_driver_portrait.py -- --passes):
  R = gauge faces + panel lamps (IS2Driver_Mask.png R): the dials wobble about their centres.
  G = engine-tremor weight: 1 on the station around him and on his fists + the lever grips, 0 on the rest of the man,
      and forced to 0 on his head (feathered), so the head never moves.
  B = 0.
Writes Renders/IS2Driver/IS2Driver_TalkMask.png, and IS2Driver_SwingPremul.png (the canteen layer, premultiplied).
Run with any Python that has Pillow + numpy."""
import numpy as np
from PIL import Image, ImageFilter

R = "D:/UE5Projects/RTS_Survival/CommanderPortraits/Renders/IS2Driver/"


def load(name):
    return np.asarray(Image.open(R + name).convert("RGB")).astype(np.float32) / 255.0


def blur(a, px):
    im = Image.fromarray((a * 255.0).clip(0, 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(px))).astype(np.float32) / 255.0


mask = load("IS2Driver_Mask.png")
driver = load("IS2Driver_Driver.png")[..., 0]
head = mask[..., 1]
# The head mask grown by ~8 px (0.006 height units on 1350) so the tremor fades out before it reaches the helmet.
head_hold = np.maximum(np.minimum(head * 2.0, 1.0), blur(np.minimum(head * 3.0, 1.0), 8.0) * 1.6).clip(0, 1)
station = 1.0 - blur(driver, 1.5)
shake = np.maximum(station, mask[..., 2]) * (1.0 - head_hold)
out = np.zeros_like(mask)
out[..., 0] = mask[..., 0]
out[..., 1] = shake
Image.fromarray((out * 255.0 + 0.5).clip(0, 255).astype(np.uint8)).save(R + "IS2Driver_TalkMask.png")
print("TALKMASK", out.shape, "shake mean %.3f" % shake.mean(), "head max shake %.4f" % (shake * (head > 0.5)).max())

# The hanging canteen layer (is2_driver_portrait.py -- --swing): Blender writes straight alpha. Premultiply in linear
# light, then encode back to sRGB, so the sRGB texture decodes to linear colour * alpha and filters without fringes.
sw = np.asarray(Image.open(R + "IS2Driver_Swing.png").convert("RGBA")).astype(np.float32) / 255.0
lin = np.where(sw[..., :3] <= 0.04045, sw[..., :3] / 12.92, ((sw[..., :3] + 0.055) / 1.055) ** 2.4) * sw[..., 3:4]
enc = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(np.maximum(lin, 0.0), 1 / 2.4) - 0.055)
pm = np.concatenate([enc, sw[..., 3:4]], -1)
Image.fromarray((pm * 255.0 + 0.5).clip(0, 255).astype(np.uint8), "RGBA").save(R + "IS2Driver_SwingPremul.png")
print("SWING", "coverage %.4f" % (sw[..., 3] > 0.5).mean())
