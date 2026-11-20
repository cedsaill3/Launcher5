#!/usr/bin/env bash
# Launcher5 - interface GPU : l'interface compilee en application native (eboot.bin)
# qui dessine avec la carte graphique. Resultat dans build/gpu/, integre ensuite a
# launcher5.elf par « make » (le bootstrap installe l'application LNCH00005).
#
# A lancer dans Ubuntu (WSL) depuis le dossier launcher :  bash gpu/build.sh
#
# Necessaire (chemins modifiables par variables d'environnement) :
#   PS5_SDK_FULL  kit PS5 complet (bibliotheques SDL2_image, SDL2_ttf, curl...)
#   NATIVE_APP    modele d'application native (ps5-native-app-boilerplate), avec .deps/
#   OPENGL_SRC    sources de ps5-opengl (dossier native-app/ et tools/)
#   OPENGL_SDK    SDK ps5-opengl (dossier sdk/ de l'archive 1.0.2)
#   SDL_GPU       SDL2 compile avec le pont OpenGL, le rendu OpenGL et l'audio PS5
#                 (integration/SDL2 de ps5-opengl, options AUDIO et RENDER activees)
#
# Launcher5 (c) 2026 cedsaill
set -euo pipefail
SRC=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${GPU_WORK:-$HOME/gpuwork}
PS5_SDK_FULL=${PS5_SDK_FULL:-$HOME/ps5-payload-sdk-full}
NATIVE_APP=${NATIVE_APP:-$WORK/ps5-native-app-boilerplate}
OPENGL_SRC=${OPENGL_SRC:-$WORK/ps5-opengl}
OPENGL_SDK=${OPENGL_SDK:-$WORK/sdk/ps5-opengl-sdk-1.0.2/sdk}
SDL_GPU=${SDL_GPU:-$OPENGL_SRC/build/l5-sdl/sdk}
SDK=$NATIVE_APP/.deps/native/ps5-payload-sdk
HB=$PS5_SDK_FULL/target/user/homebrew
APP=$WORK/l5native/app
OBJ=$WORK/l5native/obj
TITLE=LNCH00005   # une seule tuile : l'application GPU prend la place de la tuile Launcher5
NAME=Launcher5
CONTENT=LAUNCHER50000000
EXTRA=""
OUT=$SRC/build/gpu
ICON=$SRC/assets/relay/icon0.png
PORT=5051
CC="env PS5_PAYLOAD_SDK=$SDK sh $NATIVE_APP/tooling/prospero-clang18"

# 1) dossier d'application prepare une fois (modele + integration OpenGL)
if [[ ! -f $APP/Makefile ]]; then
  mkdir -p "$APP"
  cp "$NATIVE_APP/Makefile" "$APP/Makefile"
  for d in assets runtime sce_sys tooling tools; do mkdir -p "$APP/$d"; cp -a "$NATIVE_APP/$d/." "$APP/$d/"; done
  sed -i 's/write_u64(result.data, result.heap_size, std::numeric_limits<std::uint64_t>::max());/write_u64(result.data, result.heap_size, 0x10000000ULL);/' \
    "$APP/tooling/native/sce_module_writer.cpp"
  sed -i 's/--eh-frame-hdr \\/--eh-frame-hdr --wrap=fcntl --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free --wrap=posix_memalign --wrap=malloc_usable_size \\/' \
    "$APP/tools/build.sh"
  # identifiants « LNCH0000x » acceptes (le modele n'accepte que PPSA)
  python3 - "$APP/tools/build.sh" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
s = s.replace(r'r"PPSA\d{5}"', r'r"[A-Z]{4}\d{5}"').replace(r'-PPSA\d{5}_00-', r'-[A-Z]{4}\d{5}_00-')
open(p, "w").write(s)
PY
  cp "$NATIVE_APP/tooling/native/ps5-pie.ld" "$APP/tooling/native/ps5-pie-base.ld"
  cp "$OPENGL_SRC/native-app/ps5-pie.ld" "$OPENGL_SRC/native-app/app-symbols.map" "$APP/tooling/native/"
  mkdir -p "$APP/src" "$APP/include" "$APP/vendor" "$APP/build/native-imports" "$APP/.deps"
  cp "$OPENGL_SRC/native-app/app_heap.c" "$APP/src/app_heap.c"
  for h in EGL GL KHR; do cp -a "$OPENGL_SDK/include/$h" "$APP/include/"; done
  cp "$OPENGL_SRC/native-app/param.json" "$APP/sce_sys/param.json"
  python3 "$OPENGL_SRC/tools/native-display-metadata.py" "$APP/sce_sys/param.json" --fps 120
  cp -a "$NATIVE_APP/.deps/native" "$APP/.deps/native"
  A=$APP/.deps/native/ps5-payload-sdk
  for s in agc_link_stub:libSceAgc agc_driver_link_stub:libSceAgcDriver; do
    PS5_PAYLOAD_SDK="$A" sh "$APP/tooling/prospero-clang18" -std=c11 -O2 -fPIC -ffunction-sections -fdata-sections \
      -c "$OPENGL_SRC/native-app/${s%%:*}.c" -o "$APP/build/native-imports/${s%%:*}.o"
    "$A/bin/prospero-lld" --shared -soname "${s##*:}.prx" -o "$A/target/lib/${s##*:}.so" "$APP/build/native-imports/${s%%:*}.o"
  done
  printf 'APP_INCLUDE_PATHS = include\nAPP_STATIC_ARCHIVES = vendor/libps5_opengl_group.a\n' > "$APP/.env"
fi

# 2) l'interface Launcher5 (meme code que la version classique, option L5_GPU)
rm -rf "$OBJ"; mkdir -p "$OBJ"
UI="main menu scan theme i18n platform_ps5 font_data sha256"
for f in $UI; do
  $CC -std=gnu11 -O2 -fPIC -ffunction-sections -fdata-sections -w \
    -D__PS5__ -DHAVE_MIXER -DL5_GPU -DSDL_MAIN_HANDLED=1 $EXTRA \
    -ffile-prefix-map=$SRC=. -ffile-prefix-map=$HOME=/build \
    -I$SDL_GPU/include/SDL2 -I$SDL_GPU/include -I$HB/include -I$HB/include/SDL2 -I$HB/include/freetype2 -I$SRC/src \
    -c "$SRC/src/$f.c" -o "$OBJ/$f.o" &
done
wait
for f in $UI; do test -s "$OBJ/$f.o"; done
ld.lld-18 -r -o "$OBJ/launcher5.o" $(for f in $UI; do echo "$OBJ/$f.o"; done)
sed "s/htons(5051)/htons($PORT)/" "$SRC/gpu/runtime_shims.c" > "$APP/src/runtime_shims.c"

# 3) edition des liens : interface + bibliotheques + OpenGL
L=$HB/lib
{
  printf 'SEARCH_DIR("%s")\n' "$SDK/target/lib"
  printf 'EXTERN(main)\n'
  printf 'GROUP (\n'
  for a in "$OBJ/launcher5.o" \
           $L/libSDL2_mixer.a $L/libvorbisfile.a $L/libvorbis.a $L/libogg.a $L/libmpg123.a $L/libFLAC.a \
           $L/libmodplug.a $L/libSDL2_image.a $L/libSDL2_ttf.a $L/libjansson.a $L/libfreetype.a \
           $L/libpng.a $L/libjpeg.a $L/libwebp.a $L/libwebpdemux.a $L/libsharpyuv.a \
           $L/libcurl.a $L/libpsl.a $L/libssl.a $L/libcrypto.a $L/libzstd.a \
           "$SDL_GPU/lib/libSDL2.a" $L/libsamplerate.a $L/libz.a $L/libbz2.a $L/libiconv.a \
           "$OPENGL_SDK/lib/libPS5OpenGLCore33.a" \
           "$SDK/target/lib/libSceSystemService.so" "$SDK/target/lib/libSceUserService.so" \
           "$SDK/target/lib/libSceAudioOut.so" "$SDK/target/lib/libScePad.so" \
           "$SDK/target/lib/libSceVideoOut.so" "$SDK/target/lib/libkernel.so" \
           "$PS5_SDK_FULL/target/lib/libSceNet.so" \
           "$SDK/target/lib/libunwind.a" "$SDK/target/lib/libc++abi.a" "$SDK/target/lib/libc++.a" \
           "$(clang-18 --print-resource-dir)/lib/linux/libclang_rt.builtins-x86_64.a"; do
    test -s "$a" || { echo "manque : $a" >&2; exit 2; }
    printf '  "%s"\n' "$a"
  done
  printf ')\n'
} > "$APP/vendor/libps5_opengl_group.a"

# 4) tuile : identifiant, nom, icone
python3 - "$APP/sce_sys/param.json" "$TITLE" "$NAME" "$CONTENT" <<'PY'
import json, sys
p, t, name, content = sys.argv[1:]
d = json.load(open(p))
d["titleId"] = t
d["contentId"] = "IV9999-%s_00-%s" % (t, content)
d["conceptId"] = t[-5:]   # demande par l'outil de construction, retire ensuite
for lang in ("en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "pt-PT"):
    d["localizedParameters"][lang] = {"titleName": name}
open(p, "w").write(json.dumps(d, indent=2) + "\n")
PY
cp "$ICON" "$APP/sce_sys/icon0.png"
rm -rf "$APP/dist"
make -C "$APP" --no-print-directory -j8 app 2>&1 | grep -E "error|Error|Build complete" || true
test -s "$APP/dist/$TITLE/eboot.bin"

# 5) copie pour launcher5.elf
rm -rf "$OUT" && mkdir -p "$OUT"
cp -r "$APP/dist/$TITLE/eboot.bin" "$APP/dist/$TITLE/sce_module" "$APP/dist/$TITLE/sce_sys" "$OUT/"
# tuile sans concept : la PS5 utilise alors une fiche locale (nom et image de la tuile)
python3 - "$OUT/sce_sys/param.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d.pop("conceptId", None)
open(p, "w").write(json.dumps(d, indent=2) + chr(10))
PY
echo "interface GPU : $(du -h "$OUT/eboot.bin" | cut -f1) dans ${OUT#$SRC/}"
