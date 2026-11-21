"""Offline diffuse edge lighting for Dreamcast's independent Wolfman PVR atlas.

The Dreamcast character atlas is true-color ARGB4444, not the indexed DS/3DS
atlas. Shape/opacity/UV/pivots are never changed, and this runs only during
the art-baker workflow. Operates on either numpy RGBA arrays or nested lists.
"""

def finish_hd_wolf_volume(rgba, anchor_y):
    height = len(rgba)
    width = len(rgba[0])
    changed = 0
    # This begins *below* the anchored head: do not bleach eyes, ears,
    # muzzle, nose, teeth, or the Wolfman's canonical facial silhouette.
    for y in range(max(0, int(anchor_y) + 2),
                   min(height, int(anchor_y) + 35)):
        x = 0
        while x < width:
            if int(rgba[y][x][3]) < 128:
                x += 1
                continue
            first = x
            while x < width and int(rgba[y][x][3]) >= 128:
                x += 1
            size = x - first
            if size < 9:
                continue
            for side, depth in ((0, 4), (1, 4), (2, 4), (3, 4),
                                (size - 4, -4), (size - 3, -4),
                                (size - 2, -4), (size - 1, -4)):
                if side < 0 or side >= size:
                    continue
                col = rgba[y][first + side]
                rgb = [int(col[k]) for k in range(3)]
                # Do not tint the thin black contour or empty pixels; retain
                # RGB detail on all already-painted HD facial/cloth artwork.
                if max(rgb) < 68:
                    continue
                if depth > 0:
                    # A soft key light follows the front-facing contour.
                    strength = 4 - side
                    new = [min(255, v + (255 - v) * strength // 35)
                           for v in rgb]
                else:
                    # A quiet, not pitch-black, back-face fill shadow.
                    d = size - 1 - side
                    dark = 4 - d
                    new = [v * (100 - dark * 4) // 100 for v in rgb]
                if new != rgb:
                    for k in range(3):
                        col[k] = new[k]
                    changed += 1
    return changed
