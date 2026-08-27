import zlib, struct, math, sys

def make(path, size):
    W = H = size
    px = bytearray([0, 0, 0, 0] * W * H)

    def sp(x, y, r, g, b, a=255):
        x = int(x); y = int(y)
        if 0 <= x < W and 0 <= y < H:
            i = (y * W + x) * 4
            ia = a / 255.0
            for k, v in enumerate((r, g, b)):
                px[i + k] = int(px[i + k] * (1 - ia) + v * ia)
            px[i + 3] = max(px[i + 3], a)

    def fill_poly(pts, col):
        ys = [p[1] for p in pts]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            xs = []
            for i in range(len(pts)):
                x1, y1 = pts[i]; x2, y2 = pts[(i + 1) % len(pts)]
                if (y1 <= y < y2) or (y2 <= y < y1):
                    xs.append(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
            xs.sort()
            for j in range(0, len(xs) - 1, 2):
                for x in range(int(xs[j]), int(xs[j + 1]) + 1):
                    sp(x, y, *col)

    def disc(cx, cy, r, col):
        for y in range(int(cy - r), int(cy + r) + 1):
            for x in range(int(cx - r), int(cx + r) + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    sp(x, y, *col)

    def line(x0, y0, x1, y1, thick, col):
        n = int(math.hypot(x1 - x0, y1 - y0)) + 1
        for s in range(n):
            x = x0 + (x1 - x0) * s / n
            y = y0 + (y1 - y0) * s / n
            for ox in range(-thick, thick + 1):
                for oy in range(-thick, thick + 1):
                    if ox * ox + oy * oy <= thick * thick:
                        sp(x + ox, y + oy, *col)

    BLUE = (0x25, 0x63, 0xEB)
    DARK = (0x18, 0x18, 0x1B)
    WHITE = (0xFF, 0xFF, 0xFF)
    PAGE = (0xF1, 0xF5, 0xF9)

    cx, cy = W / 2.0, H / 2.0
    br = W * 0.18
    # rounded-square background
    for y in range(H):
        for x in range(W):
            dx = max(abs(x - cx) - (W / 2 - br), 0)
            dy = max(abs(y - cy) - (H / 2 - br), 0)
            if math.hypot(dx, dy) <= br:
                sp(x, y, *BLUE)

    u = W / 100.0
    spineY = 78 * u
    topL = (16 * u, 30 * u)
    topR = (84 * u, 30 * u)
    dip = (50 * u, 40 * u)          # slight valley at the spine top
    baseL = (10 * u, spineY)
    baseR = (90 * u, spineY)
    spineTop = (50 * u, 34 * u)
    spineBot = (50 * u, spineY)

    # book covers (dark) slightly larger behind the pages
    fill_poly([(8 * u, 28 * u), (50 * u, 36 * u), (50 * u, spineY + 4 * u), (6 * u, spineY)], DARK)
    fill_poly([(92 * u, 28 * u), (50 * u, 36 * u), (50 * u, spineY + 4 * u), (94 * u, spineY)], DARK)
    # pages (white/paper)
    fill_poly([topL, dip, spineBot, baseL], PAGE)
    fill_poly([topR, dip, spineBot, baseR], PAGE)
    # page lines
    for i in range(1, 4):
        yy = 44 * u + i * 8 * u
        line(18 * u, yy, 46 * u, yy - 2 * u, max(1, int(u)), (0xCB, 0xD5, 0xE1))
        line(54 * u, yy - 2 * u, 82 * u, yy, max(1, int(u)), (0xCB, 0xD5, 0xE1))
    # spine
    line(spineTop[0], spineTop[1], spineBot[0], spineBot[1], max(1, int(1.5 * u)), DARK)

    # clock: small disc on the upper book, hands
    ccx, ccy, cr = 50 * u, 22 * u, 15 * u
    disc(ccx, ccy, cr, WHITE)
    disc(ccx, ccy, cr - max(2, int(2 * u)), BLUE)
    disc(ccx, ccy, cr - max(3, int(3.5 * u)), WHITE)
    line(ccx, ccy, ccx + cr * 0.1, ccy - cr * 0.55, max(2, int(1.6 * u)), DARK)   # hour
    line(ccx, ccy, ccx + cr * 0.6, ccy + cr * 0.15, max(1, int(1.1 * u)), BLUE)   # minute
    disc(ccx, ccy, max(2, int(1.6 * u)), DARK)

    raw = bytearray()
    for y in range(H):
        raw.append(0)
        raw.extend(px[y * W * 4:(y + 1) * W * 4])
    comp = zlib.compress(bytes(raw), 9)

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', comp))
        f.write(chunk(b'IEND', b''))
    print('wrote', path)


make(sys.argv[1], int(sys.argv[2]))
