"""Build Wii U branding sizes from the user's originals without redrawing artwork."""
from pathlib import Path
from PIL import Image, ImageOps
root=Path(__file__).resolve().parents[1]
icon=Image.open(root/'branding/source/icon.png').convert('RGBA')
splash=Image.open(root/'branding/source/splash.png').convert('RGB')
for name,size in [('icon.png',(128,128)),('hbl_icon.png',(256,96)),('store_icon.png',(256,150))]:
    if size[0]==size[1]:out=icon.resize(size,Image.Resampling.LANCZOS)
    else:
        out=Image.new('RGBA',size,(5,15,12,255))
        mark=ImageOps.contain(icon,(size[1],size[1]),Image.Resampling.LANCZOS)
        out.alpha_composite(mark,((size[0]-mark.width)//2,(size[1]-mark.height)//2))
    out.save(root/'branding'/name)
for name,size in [('splash_tv.png',(1280,720)),('splash_drc.png',(854,480)),('store_screen.png',(848,208))]:
    out=Image.new('RGB',size,splash.getpixel((0,0)))
    fit=ImageOps.contain(splash,size,Image.Resampling.LANCZOS)
    out.paste(fit,((size[0]-fit.width)//2,(size[1]-fit.height)//2))
    out.save(root/'branding'/name)
dest=root/'content/ui';dest.mkdir(parents=True,exist_ok=True)
icon.save(dest/'logo.png')
Image.open(root/'branding/splash_tv.png').save(dest/'startup.png')
print('Prepared supplied icon and TV/GamePad launch artwork; originals preserved.')
