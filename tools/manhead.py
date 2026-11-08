"""Hand-pixelled big head for the console man (19 px wide, 19 tall, drawn at 4 px per cell like the rest of the art).
Built from a symmetric base (left half + centre column, mirrored) plus per-expression overlays; '.' = leave alone."""
W, H = 19, 19

def mirror(half): return half + half[:-1][::-1]
def full(rows): return [mirror(r) for r in rows]

BASE = [
    ".......ooo",
    "......ohhl",
    ".....ohhhl",
    "...oohhhll",
    "..ohhhhlhh",
    "..ohhhhhhh",
    ".ohhhhhhhh",
    ".ohhhSSSSS",
    ".ohSSSSSSS",
    ".ohSSSSSSS",
    ".ohSSSSSSK",
    ".oSSSSSkSK",
    ".oSSppSsSS",
    ".oSSSSSSSS",
    "..oSSSSSSS",
    "..oSSSSSSS",
    "...oSSSSSS",
    "....ooSSsS",
    "......oooo",
]
# overlays: dict of row -> half string, applied over the base (only non-'.' characters)
def ov(base_rows, over):
    rows = [list(r) for r in base_rows]
    for y, s in over.items():
        for x, ch in enumerate(s):
            if ch != '.': rows[y][x] = ch
    return ["".join(r) for r in rows]

EYES = {
    "open":   {7: "...HHHHH..", 8: "....BBBB..", 9: "....wiBw..", 10: "....wiiw..", 11: "....ssss.."},
    "wide":   {6: "...HHHHH..", 7: "..........", 8: "...wBBBw..", 9: "...wwiBw..", 10: "...wiiiw..", 11: "....www..."},
    "happy":  {7: "...HHHHH..", 9: ".....BB...", 10: "....B..B.."},
    "bliss":  {7: "...HHHHH..", 10: "....BBBB.."},
    "cross":  {7: "...HHHHH..", 8: "....BBBB..", 9: "....wwiB..", 10: "....wwiB..", 11: "....ssss.."},
    "tight":  {7: "..HHHHH...", 9: "....BB....", 10: ".....BBB.."},
    "sleep":  {7: "...HHHHH..", 10: "....BBBB..", 11: ".....rr..."},
    "sad":    {6: ".......HH.", 7: "...HHHH...", 8: "....BBBB..", 9: "....wiBw..", 10: "....wiiw..", 11: "....rwww..", 12: "....r....."},
    "shades": {8: "..gGGGGGGG", 9: "..GBwBBBBG", 10: "..GBBwBBBG", 11: "..gGGGGGgg"},
    "dizzy":  {7: "...HHHHH..", 8: "....BBBB..", 9: "....wBiw..", 10: "....wiBw..", 11: "....ssss.."},
}
# mouths: rows 13-17.  Row 13/14 mustache is common; mouth sits under it.
MUS = {13: "....hhhhhh", 14: "..hhhh...."}
MOUTH = {
    "grin":    {15: "......wTwT", 16: ".....LLTTT"},
    "grin2":   {15: ".....wTwTw", 16: "......LLLL"},
    "smile":   {15: "......LLLL"},
    "open":    {15: ".....mmmmm", 16: "....mmmmmm", 17: ".....mtttt"},
    "tongue":  {15: ".....mmmmm", 16: ".....mtttt", 17: "......tttt"},
    "burp":    {15: "....mmmmmm", 16: "....mmmmmm", 17: ".....mmmmm"},
    "puff":    {15: "......LLLL"},
    "yawn":    {14: "....mmmmmm", 15: "...mmmmmmm", 16: "...mmmmmmm", 17: "....mmtttt"},
    "frown":   {15: "......LLLL", 16: ".....L...."},
    "flat":    {15: "......LLLL"},
    "snore":   {15: "......LLLL", 16: ".....Lmmmm", 17: "........tt"},
}
EXTRA = {   # whole-face tweaks keyed by name (applied last, half strings)
    "puff": {11: ".oSppSSSSS", 12: ".oppppSsSS"},
}

def head(eyes, mouth, extra=None):
    rows = ov(BASE, MUS)
    rows = ov(rows, EYES["open" if eyes == "wink" else eyes])
    rows = ov(rows, MOUTH[mouth])
    if extra: rows = ov(rows, extra)
    rows = full(rows)
    if eyes == "wink":                                             # right eye shut: a happy arc
        for y, seg in ((8, "SSSS"), (9, "SBBS"), (10, "BSSB"), (11, "SSSS")):
            rows[y] = rows[y][:11] + seg + rows[y][15:]
    return rows

# frame name -> (eyes, mouth, extra)
FRAME_HEADS = {
    "IDLE_A": ("shades", "grin", None), "IDLE_B": ("shades", "grin", None),
    "FLEX_A": ("shades", "grin", None), "FLEX_B": ("shades", "grin", None),
    "DRAG": ("wide", "open", None),
    "BELLY_A": ("happy", "grin2", None), "BELLY_B": ("open", "tongue", None),
    "SCRATCH_A": ("bliss", "smile", None), "SCRATCH_B": ("happy", "grin2", None),
    "BURP_A": ("tight", "puff", EXTRA["puff"]), "BURP_B": ("open", "burp", None),
    "PICK_A": ("cross", "smile", None), "PICK_B": ("open", "smile", None),
    "YAWN": ("tight", "yawn", None),
    "SLEEP_A": ("sleep", "snore", None), "SLEEP_B": ("sleep", "snore", None),
    "SAD": ("sad", "frown", None),
    "CAPE_A": ("wink", "grin2", None),
}
KEY = {'B': 'BLK', 'G': 'GOLD', 'g': 'GOLD2', 'w': 'WH', 'h': 'HAIR', 'H': 'HAIR2', 'o': 'RIM', 'm': 'MOUTH', 't': 'TONG', 's': 'SKD', 'k': 'SKM', 'S': 'SK', 'r': 'GR', 'l': 'HAIRL', 'p': 'BLUSH', 'i': 'IRIS', 'L': 'LIP', 'T': 'TOOTH', 'K': 'SKH'}

# ---- side view, facing right (flying / diving): 16 wide x 15 tall ----
SIDE_BASE = [
    "....oooooo......",
    "...ohhhhhho.....",
    "..ohhhHhhhhoo...",
    ".ohhhhhhhhhhho..",
    ".ohhhhhhhhhhSSo.",
    ".ohhhhhhhhSSSSSo",
    ".ohhhhhhhHHHSSSo",
    ".ohhhhhhhSSSSSSo",
    "..ohhhhhSSSSSSkko",
    "..ohhhhSSSSSSSSko",
    "...ohhhSSSSSSSSo.",
    "...oohhSSSSSSSo..",
    ".....oSSSSSSo....",
    "......oooooo.....",
]
def _pad(rows, w=17): return [r.ljust(w, '.') for r in rows]
SIDE_BASE = _pad(SIDE_BASE)
SIDE_EYES = {
    "fly":  {5: ".ohhhhhhhhSSSwBo."[:17], 6: ".ohhhhhhhHHHwwBo."[:17]},
}
SIDE_OVER = {
    # fly: eyebrow, wide eye, mustache and a screaming mouth
    "fly":  {5: ".ohhhhhhhhHHHHSo.", 6: ".ohhhhhhhSSwwBSSo", 7: ".ohhhhhhhSSwBBSSo", 9: "..ohhhhSSSSHHHHHo", 10: "...ohhhSSSSmmmmmo", 11: "...oohhSSSSmmtmo.", 12: ".....oSSSSSmmmo.."},
    # dive: squinting, grinning
    "dive": {5: ".ohhhhhhhhHHHHSo.", 6: ".ohhhhhhhSSBBBSSo", 7: ".ohhhhhhhSSSSSSSo", 9: "..ohhhhSSSSHHHHHo", 10: "...ohhhSSSSwwwwSo", 11: "...oohhSSSSSmmmo."},
}
def side_head(kind):
    rows = [list(r) for r in SIDE_BASE]
    for y, s in SIDE_OVER[kind].items():
        s = s.ljust(17, '.')
        for x, ch in enumerate(s):
            if ch != '.': rows[y][x] = ch
    return ["".join(r) for r in rows]

def scaled(rows, w, h):
    """nearest-neighbour resample of a character grid (for the small stuck-man head)"""
    H0, W0 = len(rows), len(rows[0])
    return ["".join(rows[min(H0 - 1, int(j * H0 / h))][min(W0 - 1, int(i * W0 / w))] for i in range(w)) for j in range(h)]

# ---- back of the head (butt-wiggle and fart poses): 19 wide x 17 tall, mirrored ----
BACK_HALF = [
    ".......ooo",
    "......ohhl",
    ".....ohhhl",
    "...oohhhhh",
    "..ohhhhlhh",
    "..ohhhhhhl",
    ".ohhhhhhlH",
    ".ohhhhhhhh",
    ".ohhhhhhhh",
    ".ohhhhhhhh",
    "okkhhhhhhh",
    "okSkhhhhhh",
    "okkhhhhhhh",
    ".ohhhhhhhh",
    "..ohhhhhhh",
    "....ohhSSS",
    ".....sSSSS",
]
def back_head(): return full(BACK_HALF)
# a sideways glance over the shoulder: one eye at the right edge of the face
PEEK = [".ow", "oBw", "ow."]
