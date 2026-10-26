"""Bake numeric material blend weights in world coordinates for the isolated landscape."""
import math
from pathlib import Path
from PIL import Image

size=512
image=Image.new("RGBA",(size,size))
pixels=[]
def ellipse(x,y,cx,cy,rx,ry):
    return math.exp(-2*((x-cx)**2/rx**2+(y-cy)**2/ry**2))
for row in range(size):
    y=(row/(size-1)-.5)*37800
    for column in range(size):
        x=(column/(size-1)-.5)*37800
        warped_x=x+310*math.sin(y/930)+180*math.cos(x/570+y/310)
        warped_y=y+240*math.cos(x/1100)+140*math.sin(y/410-x/830)
        patch_noise=(math.sin(x/1800+y/2400)+.5*math.sin(x/650-y/890)+.25*math.cos(x/260+y/390))/1.75
        dry=max(0,min(.85,.35+.52*patch_noise))
        dirt=max(.80*ellipse(warped_x,warped_y,0,-350,2800,1800),
                 .72*ellipse(warped_x,warped_y,-1500,0,650,3100),
                 .72*ellipse(warped_x,warped_y,1200,0,680,2600),
                 .60*ellipse(warped_x,warped_y,-4600,3300,1900,1400),
                 .60*ellipse(warped_x,warped_y,4600,-3300,2200,1800))
        path_y=-2200+.12*(x+3500)+250*math.sin(x/1700)
        road=.70*math.exp(-((warped_y-path_y)/330)**2)*math.exp(-((x+500)/8000)**4)
        dirt=max(dirt,road)
        rocky=max(.80*ellipse(warped_x,warped_y,6500,6600,2900,2100),
                  .65*ellipse(warped_x,warped_y,-4800,-6500,2600,2100),
                  .65*ellipse(warped_x,warped_y,8200,-3500,2400,2800))
        pixels.append((round(255*dirt),round(255*dry),round(255*rocky),255))
image.putdata(pixels)
path=Path(__file__).parent/"ground_blend_weights.png"
image.save(path)
print(path.resolve())
