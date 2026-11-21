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
            if ch == 'h' and x > 4: ch = 'S'                      # thinning like the front view: hair only in the tuft behind the ear
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
    "shades": {8: "..gGGGGGGG", 9: "..GBwBBBBG", 10: "..GBBwBBBG", 11: "..gBBBBBgg"},
    "dizzy":  {7: "...HHHHH..", 8: "....BBBB..", 9: "....wBiw..", 10: "....wiBw..", 11: "....ssss.."},
    "lids":   {7: "...HHHHH..", 8: "....ssss..", 9: "....BBBB..", 10: "....wiBw..", 11: "....SSSS.."},   # half-lidded bedroom eyes
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
    "pucker":  {15: ".......LLL", 16: ".......Lmm", 17: "........LL"},                  # lips pursed for a kiss
    "smirk":   {15: ".....LLLLL", 16: "...LL....."},                                  # closed, lopsided smile
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
    "YAWN": ("tight", "yawn", None), "HOWL": ("tight", "yawn", None),
    "SLEEP_A": ("sleep", "snore", None), "SLEEP_B": ("sleep", "snore", None),
    "SAD": ("sad", "frown", None),
    "CAPE_A": ("wink", "grin2", None),
    "BK_A": ("wink", "pucker", None), "BK_B": ("wink", "pucker", None),          # blown kiss
    "POSE_A": ("lids", "smirk", None), "POSE_B": ("lids", "smirk", None),        # pin-up pose
    "DANCE_A": ("happy", "grin2", None), "DANCE_B": ("happy", "grin2", None),    # disco
    "PEC_A": ("lids", "smirk", None), "PEC_B": ("lids", "smirk", None),          # pec pops
}
KEY = {'B': 'BLK', 'G': 'GOLD', 'g': 'GOLD2', 'w': 'WH', 'h': 'HAIR', 'H': 'HAIR2', 'o': 'RIM', 'm': 'MOUTH', 't': 'TONG', 's': 'SKD', 'k': 'SKM', 'S': 'SK', 'r': 'GR', 'l': 'HAIRL', 'p': 'BLUSH', 'i': 'IRIS', 'L': 'LIP', 'T': 'TOOTH', 'K': 'SKH'}

# ---- side view, facing right (flying / diving): 16 wide x 15 tall ----
SIDE_BASE = [      # thinning, like the front view: a bare shiny crown, a tuft behind the ear, a few combed-over strands
    "....oooooo......",
    "...oSKKSSSo.....",
    "..ohhhhhhSSoo...",
    ".ohhSSSSSSSSSo..",
    ".ohhSSSSSSSSSSo.",
    ".ohhSSSSSSSSSSSo",
    ".ohhhSSSSSSSSSSo",
    ".ohhhSSSsSSSSSSo",
    "..ohhSSsSSSSSkko",
    "..ohhSSSSSSSSSko",
    "...ohhSSSSSSSSo.",
    "...oohhSSSSSSo..",
    ".....oSSSSSSo...",
    "......oooooo....",
]
def _pad(rows, w=17): return [r.ljust(w, '.') for r in rows]
SIDE_BASE = _pad(SIDE_BASE)
# One canonical profile for flying, tumbling and the centre of every turn.
# Its silhouette is the same for each expression.  Crown hair is sparse,
# with a short rear tuft and the same combed sweep as the frontal head.
# Landmarks are in skull-local pixels, rather than drifting with the pose.
# In particular the ear (x=5) sits BEHIND the eye (x=12), the nose bridge
# remains at the leading edge (x=15), and the mouth remains below the moustache.
SIDE_LANDMARKS = {
    "crown": (8, 2), "ear": (5, 8), "brow": (12, 5),
    "eye": (12, 7), "nose": (15, 8), "moustache": (12, 9),
    "mouth": (12, 10), "chin": (12, 12),
}
SIDE_FEATURES = {
    "fly": {
        2: {7: "l", 8: "h"},
        5: {10: "H", 11: "H", 12: "H", 13: "H"},
        6: {11: "w", 12: "w", 13: "B", 14: "w"},
        7: {11: "w", 12: "i", 13: "B"},
        8: {5: "k", 6: "s", 14: "k"},
        9: {5: "s", 11: "H", 12: "H", 13: "H", 14: "H"},
        10: {10: "S", 11: "m", 12: "m", 13: "m"},
        11: {10: "S", 11: "m", 12: "t"},
        12: {10: "S", 11: "s"},
    },
    "dive": {
        2: {7: "l", 8: "h"},
        5: {10: "H", 11: "H", 12: "H", 13: "H"},
        6: {11: "B", 12: "B", 13: "B", 14: "B"},
        7: {11: "s", 12: "S", 13: "S"},
        8: {5: "k", 6: "s", 14: "k"},
        9: {5: "s", 11: "H", 12: "H", 13: "H", 14: "H"},
        10: {10: "S", 11: "w", 12: "w", 13: "w"},
        11: {10: "S", 11: "m", 12: "m"},
        12: {10: "S", 11: "s"},
    },
}
def side_head(kind):
    """Same skull/hair contour for every profile expression; eyes and lips vary."""
    rows = [list(r) for r in SIDE_BASE]
    for y, points in SIDE_FEATURES[kind].items():
        for x, ch in points.items():
            assert rows[y][x] != '.', (kind, y, x)  # never create a floating detail
            rows[y][x] = ch
    return ["".join(r) for r in rows]

def turn_head(angle, kind="dive"):
    """A continuous-width turnaround, retaining the front head's height.

    A 14-row side head used to pop upward on the 55-degree frame while
    19-row front heads stayed planted.  Resample the profile at 19 rows
    and keep the original brow/mouth heights through the turn.  The
    flattened front/head width is monotone (19->14) instead of ballooning
    back out at the side.  The returned grid is still pure 1-byte palette
    symbols; renderer memory and sprite frame count never increase.
    """
    a = min(125, max(0, abs(float(angle))))
    if a < 38:
        width = 19 if a < 18 else 17
        return scaled(head("open", "grin"), width, 19)
    if a < 56:
        return scaled(head("open", "grin"), 14, 19)
    return scaled(side_head(kind), 14, 19)

# how much each character wins a tie when several source pixels are squeezed into one: eyes, mouths and rims must survive the shrink
_PRI = {'B': 2.8, 'i': 1.7, 'w': 1.5, 'm': 1.9, 't': 1.7, 'T': 1.5, 'L': 1.5, 'G': 0.9, 'g': 0.8, 'H': 1.5, 'o': 1.35, 'r': 1.4, '.': 0.9}
def shrink(rows, w, h):
    """area-weighted resample of a character grid: every output pixel takes the character with the most (priority-weighted)
    area under it, so a 3-pixel eye stays an eye instead of being dropped like nearest-neighbour would"""
    H0, W0 = len(rows), len(rows[0])
    out = []
    for j in range(h):
        y0, y1 = j * H0 / h, (j + 1) * H0 / h
        line = []
        for i in range(w):
            x0, x1 = i * W0 / w, (i + 1) * W0 / w
            acc = {}
            for yy in range(int(y0), min(H0, int(y1 + 0.999))):
                oy = min(y1, yy + 1) - max(y0, yy)
                for xx in range(int(x0), min(W0, int(x1 + 0.999))):
                    ox = min(x1, xx + 1) - max(x0, xx)
                    if oy > 0 and ox > 0:
                        ch = rows[yy][xx]
                        acc[ch] = acc.get(ch, 0.0) + ox * oy * _PRI.get(ch, 1.0)
            line.append(max(acc, key=acc.get) if acc else '.')
        out.append("".join(line))
    return despeckle(out)

def despeckle(rows):
    """drop pixels that lost all their neighbours in a shrink (a knot's tip left floating above the head)"""
    H0, W0 = len(rows), len(rows[0]); out = [list(r) for r in rows]
    for j in range(H0):
        for i in range(W0):
            if rows[j][i] == '.': continue
            nb = [(j + dj, i + di) for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= j + dj < H0 and 0 <= i + di < W0]
            if all(rows[a][b] == '.' for a, b in nb): out[j][i] = '.'
    return ["".join(r) for r in out]

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


# ---- the Wolfman in profile, facing right (fly / dive): 22 wide x 15 tall.  S/s/k = fur (the palette turns them grey for the wolf outfit), K = pale muzzle,
#      t = pink inner ear, B = black (brow, nose, pupil), G = yellow eye, w = fang, m = mouth.  Two mouths: closed-ish snarl and open howl.
_WOLF_TOP = [
    "..oo..................",
    ".oSSo.................",
    ".oStSoo...............",
    ".oStSSSoooooo.........",
    ".oSSSSSSSSSSSooo......",
    "oSSSSSSSSBBBSSKKooo...",
    "oSSSSSSSSGGBSKKKKKKoo.",
    "oSSSSSSSSSSSKKKKKKKKBBo",
]
_WOLF_MOUTH = {
    "snarl": [
        "oSSSSSSSSSSKKKKKKKKKKBBo"[:23],
        ".oSSSSSSSSSKwKKwKKKKKooo",
        ".oSSSSSSSSoommmmmmmmoo..",
        "..oSSSSSSSSKKKKKKKKKo...",
        "...ooSSSSSSSSKKKKKoo....",
        ".....ooooooooooooo......",
    ],
    "howl": [
        "oSSSSSSSSSSKKKKKKKKKKBBo",
        ".oSSSSSSSSSKwKKwKKKKKooo",
        ".oSSSSSSSSoommmmmmmmmo..",
        "..oSSSSSSSommttttmmmmo..",
        "..oSSSSSSSSoKKKKKKKKKoo.",
        "...ooSSSSSSSKKKKKKKKKo..",
        ".....ooSSSSSooooooooo...",
    ],
}
def wolf_side_head(kind="snarl"):
    rows = _WOLF_TOP + _WOLF_MOUTH[kind]
    w = max(len(r) for r in rows)
    return [r.ljust(w, '.') for r in rows]


# ---- wolfify: turn any hand-pixelled human head (front, back or turning, any width) into the Wolfman's: fur for hair, two ears, a muzzle with a nose and fangs.
EAR_ROWS = 7
_EAR = ["...o...", "..oSo..", ".oStSo.", ".oStSo.", "oStttSo", "oSSStSo", "oSSSSSo"]
def wolfify(rows):
    H, W = len(rows), len(rows[0]); g = [list(r) for r in rows]
    front = any(ch in 'Bi' for r in rows[int(H * .35):int(H * .65)] for ch in r)       # eyes present: a face (else the back of the head)
    for r in g:
        for i, ch in enumerate(r):
            if ch == 'h': r[i] = 'S'
            elif ch == 'l': r[i] = 'K'
            elif ch == 'k' and not front: r[i] = 'S'
    if front:
        cx = W // 2; mw = max(2, int(W * .2)); top = int(H * .58); bot = H - 2
        tongue = any('t' in r for r in rows[int(H * .7):])
        for y in range(top, bot + 1):
            for x in range(cx - mw - 1, cx + mw + 2):
                edge = x in (cx - mw - 1, cx + mw + 1)
                if y > top or not edge: g[y][x] = 'o' if edge and y > top else 'K'
        for x in range(cx - 1, cx + 2):
            g[top + 1][x] = 'B'
        g[top + 2][cx] = 'B'
        my = bot - 2
        for x in range(cx - mw, cx + mw + 1): g[my][x] = 'm'
        g[my][cx - mw] = 'w'; g[my][cx + mw] = 'w'
        if tongue:
            for x in range(cx - 1, cx + 2): g[my + 1][x] = 't'
        for x in range(cx - mw - 1, cx + mw + 2):
            g[bot][x] = 'o'
    out = ["".join(r) for r in g]
    a = max(0, int(W * .04)); ear = []
    for e in _EAR:
        row = ['.'] * W
        for i, ch in enumerate(e):
            row[a + i] = ch; row[W - 7 - a + i] = ch
        ear.append("".join(row))
    return ear + out
