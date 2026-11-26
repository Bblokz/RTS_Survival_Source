# Step 2 of 3 of the HQ barbed wire line's option picture (see ue_barbed_wire_line_icon.py): scales the raw render's
# colours so its ground is exactly the field trench picture's olive, (144, 146, 114), and saves it opaque.
# Run with the system python (needs Pillow): python barbed_wire_line_icon_grade.py
import statistics
import sys

from PIL import Image

FOLDER = 'D:/UE5Projects/RTS_Shorts/Saved/BarbedWireCaptures'
RAW_PATH = FOLDER + '/T_BxpOption_GerBarbedWireLine_raw.png'
OUT_PATH = FOLDER + '/T_BxpOption_GerBarbedWireLine.png'
TRENCH_GROUND = (144, 146, 114)
# The ground is sampled in a band this wide along the picture's border, where no wire lies.
BORDER_BAND = 24
# A darker ground means the render failed (no light or no exposure), not a picture to grade.
MINIMUM_GROUND_LEVEL = 8


def sample_ground(picture):
    width, height = picture.size
    samples = []
    for x in range(0, width, 4):
        for y in list(range(0, BORDER_BAND, 4)) + list(range(height - BORDER_BAND, height, 4)):
            samples.append(picture.getpixel((x, y)))
    return tuple(statistics.median(sample[channel] for sample in samples) for channel in range(3))


def main():
    picture = Image.open(RAW_PATH).convert('RGB')
    ground = sample_ground(picture)
    print('raw ground', ground)
    if min(ground) < MINIMUM_GROUND_LEVEL:
        print('ground too dark; the render failed')
        return 1
    gains = [TRENCH_GROUND[channel] / ground[channel] for channel in range(3)]
    channels = [channel.point(lambda value, gain=gains[index]: min(255, round(value * gain)))
                for index, channel in enumerate(picture.split())]
    Image.merge('RGB', channels).convert('RGBA').save(OUT_PATH)
    print('graded with gains', [round(gain, 3) for gain in gains], '->', OUT_PATH)
    return 0


sys.exit(main())
