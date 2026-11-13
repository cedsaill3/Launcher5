"""Genere src/font_data.c a partir des polices de assets/ (police integree au programme)."""
from pathlib import Path
out = ["/* Fichier genere par tools_fontgen.py : polices DejaVu (licence libre, voir assets/DejaVu-LICENSE.txt) */"]
for var, f in (("font_regular", "assets/DejaVuSans.ttf"), ("font_bold", "assets/DejaVuSans-Bold.ttf")):
    data = Path(f).read_bytes()
    out.append(f"const unsigned int {var}_len = {len(data)};")
    out.append(f"const unsigned char {var}[] = {{")
    for i in range(0, len(data), 24):
        out.append("  " + ",".join(str(b) for b in data[i:i+24]) + ",")
    out.append("};")
Path("src/font_data.c").write_text("\n".join(out) + "\n")
print("src/font_data.c :", Path("src/font_data.c").stat().st_size // 1024, "Ko")
