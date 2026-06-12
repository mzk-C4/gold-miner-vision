"""Extract gold/diamond sprites from game sprite sheets."""
import xml.etree.ElementTree as ET
from PIL import Image
from pathlib import Path
import re

def parse_plist(plist_path):
    """Parse Apple plist XML and return {frame_name: (x,y,w,h)}."""
    tree = ET.parse(plist_path)
    root = tree.getroot()
    body = root.find('dict')
    children = list(body)

    frames = {}
    for i, child in enumerate(children):
        if child.tag == 'key' and child.text == 'frames':
            frames_container = children[i + 1]
            items = list(frames_container)
            j = 0
            while j < len(items):
                if items[j].tag == 'key':
                    name = items[j].text
                    j += 1
                    if j < len(items) and items[j].tag == 'dict':
                        info_dict = items[j]
                        info_items = list(info_dict)
                        for k, ik in enumerate(info_items):
                            if ik.tag == 'key' and ik.text == 'frame':
                                frame_str = info_items[k + 1].text
                                m = re.match(r'\{\{(\d+),(\d+)\},\{(\d+),(\d+)\}\}', frame_str)
                                if m:
                                    frames[name] = tuple(map(int, m.groups()))
                                break
                j += 1
            break
    return frames

# Process level-sheet
LEVEL_SHEET = Path(r"D:\GOLD MINER\cocos2d-proj\Resources\Resources\level-sheet.png")
LEVEL_PLIST = Path(r"D:\GOLD MINER\cocos2d-proj\Resources\Resources\level-sheet.plist")
OUTPUT_DIR = Path(r"D:\GOLD MINER\ppt-workspace\html\assets")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

frames = parse_plist(LEVEL_PLIST)
sheet = Image.open(LEVEL_SHEET)
print(f"Level sheet: {len(frames)} frames, size: {sheet.size}")

# Gold nuggets - various sizes and shapes
wanted = {
    'gold-0-0.png': 'gold-nugget-1',
    'gold-0-4.png': 'gold-nugget-2',
    'gold-1-2.png': 'gold-nugget-3',
    'gold-1-5.png': 'gold-nugget-4',
    'pulled-gold-0-4.png': 'gold-large-1',
    'pulled-gold-1-6.png': 'gold-large-2',
    'diamond-5.png': 'diamond-1',
    'pulled-diamond-5.png': 'diamond-2',
    'stone-0.png': 'stone-1',
    'stone-1.png': 'stone-2',
    'TNT.png': 'tnt',
    'treasure-bag.png': 'treasure',
    'bone.png': 'bone',
    'skull.png': 'skull',
}

extracted = []
for frame_name, output_name in wanted.items():
    if frame_name in frames:
        x, y, w, h = frames[frame_name]
        sprite = sheet.crop((x, y, x + w, y + h))
        out_path = OUTPUT_DIR / f"{output_name}.png"
        sprite.save(out_path)
        extracted.append((output_name, (w, h)))
        print(f"  {frame_name} → {output_name}.png ({w}×{h})")
    else:
        print(f"  MISSING: {frame_name}")

# Also extract some items from general-sheet
GENERAL_SHEET = Path(r"D:\GOLD MINER\cocos2d-proj\Resources\Resources\general-sheet.png")
GENERAL_PLIST = Path(r"D:\GOLD MINER\cocos2d-proj\Resources\Resources\general-sheet.plist")

if GENERAL_SHEET.exists():
    gen_frames = parse_plist(GENERAL_PLIST)
    gen_sheet = Image.open(GENERAL_SHEET)
    print(f"\nGeneral sheet: {len(gen_frames)} frames")

    gen_wanted = {
        'diamond-polish.png': 'gen-diamond',
        'lucky-clover.png': 'gen-clover',
        'strength-drink.png': 'gen-potion',
        'dynamite.png': 'gen-dynamite',
    }
    for frame_name, output_name in gen_wanted.items():
        if frame_name in gen_frames:
            x, y, w, h = gen_frames[frame_name]
            sprite = gen_sheet.crop((x, y, x + w, y + h))
            out_path = OUTPUT_DIR / f"{output_name}.png"
            sprite.save(out_path)
            extracted.append((output_name, (w, h)))
            print(f"  {frame_name} → {output_name}.png ({w}×{h})")

print(f"\nTotal extracted: {len(extracted)} sprites")
for name, size in extracted:
    print(f"  {name}: {size[0]}×{size[1]}")
