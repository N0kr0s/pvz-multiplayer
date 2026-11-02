#!/usr/bin/env python3
"""Render a separate serif UI label PNG for caching in the native game renderer."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[2]
font=ImageFont.truetype('/usr/share/fonts/liberation/LiberationSerif-Bold.ttf',96)
label='MULTIPLAYER';bbox=font.getbbox(label)
mask=Image.new('L',(bbox[2]-bbox[0],bbox[3]-bbox[1]),0)
ImageDraw.Draw(mask).text((-bbox[0],-bbox[1]),label,font=font,fill=255,stroke_width=0)
mask=mask.resize((211,31),Image.Resampling.LANCZOS)
spans=[]
for y in range(mask.height):
 x=0
 while x<mask.width:
  a=min(255,round(mask.getpixel((x,y))/17)*17)
  start=x;slant=x*34//246
  while x<mask.width and min(255,round(mask.getpixel((x,y))/17)*17)==a and x*34//246==slant:x+=1
  if a:spans.append((start,y+slant,x-start,a))
# Typography is rendered separately; no pixels from the generated stone are edited.
image=Image.new('RGBA',(246,85),(0,0,0,0))
for shadow in (True,False):
 for x,y,w,a in spans:
  for dx in range(w):
   px=x+17+dx+(1 if shadow else 0);py=y+13+(1 if shadow else 0)
   image.putpixel((px,py),(166,165,183,a*3//4) if shadow else (6,6,17,a))
image.save(root/'multiplayer/assets/multiplayer-label.png')
print('Rendered separate serif label PNG for native texture caching')
