#!/usr/bin/env python3
"""Turn simulator frames into round animated previews + one HTML page.

Reads <out>/manifest.txt and <out>/<name>.rgb (raw 240x240 RGB frames), writes
<out>/<name>.webp (animated), <out>/sheet.png (key frames, for quick review)
and <out>/preview.html (self-contained, images embedded).
"""

import base64
import html
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 240
PAD = 14  # bezel around the round screen

CAPTIONS = {
    "radar": ("Radar", "Mapa da sua região ao fundo, varredura girando e “ping” quando passa pelos aviões."),
    "clima_sol": ("Clima — sol", "O sol nasce, os raios giram e a temperatura sobe contando."),
    "clima_nuvens": ("Clima — parcialmente nublado", "A nuvem chega deslizando na frente do sol."),
    "clima_nublado": ("Clima — nublado", "Duas nuvens flutuando devagar."),
    "clima_chuva": ("Clima — chuva", "A nuvem escurece e a chuva começa a cair."),
    "clima_tempestade": ("Clima — tempestade", "Chuva forte, relâmpago piscando e o céu clareando."),
    "clima_noite": ("Clima — noite", "Lua, estrelas piscando e uma estrela cadente de vez em quando."),
    "relogio": ("Relógio", "Os ponteiros giram até a hora certa; segundos deslizando."),
    "aviao": ("Avião mais próximo", "Um avião cruza a tela; depois: voo, companhia, rota e para onde olhar."),
    "ar_sol": ("Ar, UV e sol", "Medidores enchem e o sol percorre o arco do dia."),
    "sobre": ("Apresentação", "PLANE RADAR — made by Gustavo Soares, com o avião orbitando o radar."),
    "aviao_raio": ("Avião mais próximo — trocando o raio", "Toque duplo: aparece “Raio: 5 km”; sem avião, mostra a dica."),
}


def round_frame(raw: bytes) -> Image.Image:
    img = Image.frombytes("RGB", (SIZE, SIZE), raw)
    canvas = Image.new("RGB", (SIZE + PAD * 2, SIZE + PAD * 2), (14, 14, 16))
    mask = Image.new("L", (SIZE * 4, SIZE * 4), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, SIZE * 4 - 1, SIZE * 4 - 1), fill=255)
    mask = mask.resize((SIZE, SIZE), Image.LANCZOS)
    canvas.paste(img, (PAD, PAD), mask)
    d = ImageDraw.Draw(canvas)
    d.ellipse((PAD - 3, PAD - 3, PAD + SIZE + 2, PAD + SIZE + 2), outline=(40, 40, 44), width=3)
    return canvas


def main(out_dir: str) -> None:
    out = Path(out_dir)
    entries = []
    for line in (out / "manifest.txt").read_text().splitlines():
        name, frames, step = line.split()
        entries.append((name, int(frames), int(step)))

    sheet_rows = []
    blocks = []
    for name, frames, step in entries:
        raw = (out / f"{name}.rgb").read_bytes()
        n = SIZE * SIZE * 3
        imgs = [round_frame(raw[i * n:(i + 1) * n]) for i in range(frames)]
        webp = out / f"{name}.webp"
        imgs[0].save(webp, save_all=True, append_images=imgs[1:], duration=step, loop=0,
                     quality=88, method=4)
        picks = [imgs[min(frames - 1, int(ms / step))] for ms in (200, 600, 1200, frames * step - step)]
        sheet_rows.append(picks)
        title, desc = CAPTIONS.get(name, (name, ""))
        b64 = base64.b64encode(webp.read_bytes()).decode()
        blocks.append(
            f'<figure><img src="data:image/webp;base64,{b64}" alt="{html.escape(title)}">'
            f"<figcaption><b>{html.escape(title)}</b><span>{html.escape(desc)}</span></figcaption></figure>"
        )
        print(f"{name}: {frames} frames -> {webp.name} ({webp.stat().st_size // 1024} KB)")

    w = (SIZE + PAD * 2)
    sheet = Image.new("RGB", (w * 4, w * len(sheet_rows)), (0, 0, 0))
    for r, row in enumerate(sheet_rows):
        for c, im in enumerate(row):
            sheet.paste(im, (c * w, r * w))
    sheet.save(out / "sheet.png")

    page = f"""<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Plane Radar — prévia</title>
<style>
:root {{ --bg:#101114; --fg:#eceef2; --muted:#9aa3b2; --card:#181a1f; }}
body {{ margin:0; background:var(--bg); color:var(--fg); font:15px/1.45 -apple-system, system-ui, sans-serif; }}
main {{ max-width:1100px; margin:0 auto; padding:24px 16px 48px; }}
h1 {{ font-size:22px; margin:0 0 4px; }} p.sub {{ color:var(--muted); margin:0 0 24px; }}
.grid {{ display:grid; grid-template-columns:repeat(auto-fill,minmax(260px,1fr)); gap:18px; }}
figure {{ margin:0; background:var(--card); border-radius:14px; padding:12px; }}
figure img {{ width:100%; max-width:268px; display:block; margin:0 auto; }}
figcaption {{ margin-top:8px; display:flex; flex-direction:column; gap:2px; }}
figcaption span {{ color:var(--muted); font-size:13px; }}
</style></head><body><main>
<h1>Plane Radar — prévia das telas</h1>
<p class="sub">Renderizado pelo mesmo código que vai para a placa (240×240), com dados de exemplo (Belo Horizonte).</p>
<div class="grid">{''.join(blocks)}</div>
</main></body></html>"""
    (out / "preview.html").write_text(page)
    print(f"wrote {out / 'preview.html'} and {out / 'sheet.png'}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "sim/out")
