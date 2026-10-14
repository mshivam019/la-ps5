# LAS Recomp build
# macOS: Homebrew clang/SDL3/FFmpeg, Metal renderer.
# Windows: MSYS2 UCRT64 shell (see docs/BUILD-windows.md), clang + Vulkan renderer.
# Linux: clang + Vulkan renderer, x86-64 or aarch64 (see docs/linux.md).
UNAME := $(shell uname -s)
comma := ,
ifneq ($(filter MINGW% MSYS% CYGWIN% Windows_NT,$(UNAME) $(OS)),)
PLATFORM := windows
else ifeq ($(UNAME),Linux)
PLATFORM := linux
else
PLATFORM := macos
endif

# Linux: ARCH (x86_64 / aarch64, default this machine's) picks the target, separately from PLATFORM. The generated code
# differs per arch (AArch64 hosts run the guest's FP/SIMD as inline asm, others use the portable C of simd.h), so a
# cross build has its own generated-<arch>/ and build/<arch>/; SYSROOT is the target's root filesystem (headers, libs).
GEN      ?= generated
ifeq ($(PLATFORM),linux)
HOSTARCH := $(patsubst arm64,aarch64,$(shell uname -m))
ARCH     ?= $(HOSTARCH)
ifneq ($(ARCH),$(HOSTARCH))
CROSS    := --target=$(ARCH)-linux-gnu $(if $(SYSROOT),--sysroot=$(SYSROOT))
GEN      := generated-$(ARCH)
B        ?= build/$(ARCH)
endif
GENSIMD  := --simd $(if $(filter aarch64,$(ARCH)),asm,c)
endif

# B: build dir. MACOS_MIN: oldest macOS to run on (default: this one). DEPS: prefix with SDL3 / FFmpeg instead of
# Homebrew's (make deps). `make app` sets all three (build/mac15, 15.0, build/deps); plain `make` uses none.
B         ?= build
MACOS_MIN ?=
DEPS      ?=
MINFLAG  := $(if $(MACOS_MIN),-mmacosx-version-min=$(MACOS_MIN))
CC       := clang $(MINFLAG) $(CROSS)
CXX      := clang++ $(MINFLAG) $(CROSS)
IMGUI    := third_party/imgui
GENFLAGS := -O1 -fno-strict-aliasing -Wno-return-type -Wno-unused-label -Iruntime

GEN_SRC  := $(wildcard $(GEN)/main/*.c) $(wildcard $(GEN)/sdk/*.c)
# no generated code yet (make gen): link a stub, the start screen works
ifeq ($(strip $(GEN_SRC)),)
GEN_SRC  := tools/nogen_stub.c
endif
GEN_OBJ  := $(patsubst %.c,$(B)/obj/%.o,$(patsubst $(GEN)/%,%,$(GEN_SRC)))
RT_SRC   := $(wildcard runtime/*.c) $(wildcard runtime/hle/*.c) $(wildcard runtime/services/*.c) \
            $(wildcard runtime/nvn/*.c) $(wildcard runtime/nvn/maxwell/*.c) runtime/platform/$(PLATFORM).c
RT_HDRS  := $(wildcard runtime/*.h) $(wildcard runtime/platform/*.h) $(wildcard runtime/nvn/*.h) \
            $(wildcard runtime/nvn/maxwell/*.h) $(wildcard runtime/nvn/vulkan/*.h) $(wildcard runtime/ui/*.h)
UI_SRC   := $(wildcard runtime/ui/*.mm)
UI_OBJ   := $(patsubst runtime/%.mm,$(B)/rt/%.o,$(UI_SRC))
IMGUI_OBJ := $(B)/imgui/imgui.o $(B)/imgui/imgui_draw.o $(B)/imgui/imgui_tables.o $(B)/imgui/imgui_widgets.o \
             $(B)/imgui/imgui_impl_sdl3.o

ifeq ($(PLATFORM),macos)
SDL      := $(or $(DEPS),/opt/homebrew/opt/sdl3)
FFMPEG   := $(or $(DEPS),/opt/homebrew/opt/ffmpeg)
EXE      := $(B)/lasrecomp
DEPINC   := -I$(SDL)/include -I$(FFMPEG)/include
LDFLAGS  := -L$(SDL)/lib -lSDL3 -Wl,-rpath,$(SDL)/lib -L$(FFMPEG)/lib -lavformat -lavcodec -lavutil -lswscale -Wl,-rpath,$(FFMPEG)/lib \
            -framework CoreFoundation -framework Foundation -framework Metal -framework MetalFX -framework QuartzCore -framework AppKit -framework ImageIO -framework CoreGraphics -framework UniformTypeIdentifiers
BACKEND_SRC := $(wildcard runtime/nvn/*.m)
BACKEND_OBJ := $(patsubst runtime/%.m,$(B)/rt/%.o,$(BACKEND_SRC))
IMGUI_OBJ += $(B)/imgui/imgui_impl_metal.o
UIFLAGS  := -fobjc-arc
else ifeq ($(PLATFORM),linux)
EXE      := $(B)/lasrecomp
DEPINC   := $(if $(DEPS),-I$(DEPS)/include)
ifeq ($(ARCH),x86_64)
GENARCH  ?= -march=x86-64-v3  # as on Windows: AVX2 + FMA for the lifted FP/SIMD code (checked at startup)
endif
GENFLAGS += $(GENARCH)
OSFLAGS  := -Wno-format  # (%llx for uint64_t, which is unsigned long here: the same size)
SHADERC  ?= -lshaderc  # (Debian / Ubuntu's libshaderc.so; a shaderc built from source: -lshaderc_shared)
# -rdynamic: crash backtraces name host functions (runtime/platform/linux.c)
LDFLAGS  := -fuse-ld=lld -rdynamic $(if $(DEPS),-L$(DEPS)/lib) -lSDL3 -lavformat -lavcodec -lavutil -lswscale \
            -lvulkan $(SHADERC) -lpthread -ldl -lm
BACKEND_SRC := $(wildcard runtime/nvn/vulkan/*.c) $(wildcard runtime/nvn/vulkan/*.cpp)
BACKEND_OBJ := $(patsubst runtime/%.cpp,$(B)/rt/%.o,$(patsubst runtime/%.c,$(B)/rt/%.o,$(BACKEND_SRC)))
IMGUI_OBJ += $(B)/imgui/imgui_impl_vulkan.o
UIFLAGS  := -x c++  # the .mm UI sources are plain C++ apart from __APPLE__ blocks
else
EXE      := $(B)/lasrecomp.exe
DEPINC   := $(if $(DEPS),-I$(DEPS)/include)
OSFLAGS  := -D_FILE_OFFSET_BITS=64 -D_POSIX_THREAD_SAFE_FUNCTIONS -D_CRT_RAND_S -DWIN32_LEAN_AND_MEAN -DNOMINMAX
GENARCH  ?= -march=x86-64-v3  # AVX2 + FMA for the lifted FP/SIMD code (simd.h); override for older CPUs
GENFLAGS += -D_FILE_OFFSET_BITS=64 $(GENARCH)
LDFLAGS  := -fuse-ld=lld -Wl,--stack,16777216 $(if $(DEPS),-L$(DEPS)/lib) -lSDL3 -lavformat -lavcodec -lavutil -lswscale \
            -lvulkan-1 -lwinmm -lole32 -lshell32 -lcomdlg32 -luuid -lshaderc_shared \
            $(if $(RELEASE),-Wl$(comma)--subsystem$(comma)windows)  # release: no console window (log -> lasrecomp.log)
BACKEND_SRC := $(wildcard runtime/nvn/vulkan/*.c) $(wildcard runtime/nvn/vulkan/*.cpp)
BACKEND_OBJ := $(patsubst runtime/%.cpp,$(B)/rt/%.o,$(patsubst runtime/%.c,$(B)/rt/%.o,$(BACKEND_SRC)))
BACKEND_OBJ += $(B)/rt/platform/windows_rc.o  # the .exe's icon (Explorer, taskbar)
WINDRES  ?= windres
IMGUI_OBJ += $(B)/imgui/imgui_impl_vulkan.o
UIFLAGS  := -x c++ $(OSFLAGS)  # the .mm UI sources are plain C++ apart from __APPLE__ blocks
endif

RT_OBJ   := $(patsubst runtime/%.c,$(B)/rt/%.o,$(RT_SRC))
RTFLAGS  := -O2 -g -fno-strict-aliasing -Wall -Wno-unused-function -Iruntime -Iruntime/nvn $(DEPINC) $(OSFLAGS)
CXXFLAGS := -O2 -g -std=c++17 -I$(IMGUI) -I$(IMGUI)/backends -Iruntime -Iruntime/nvn $(DEPINC)

all: $(EXE)

$(B)/obj/%.o: $(GEN)/%.c runtime/cpu.h
	@mkdir -p $(dir $@)
	@$(CC) $(GENFLAGS) -c $< -o $@

$(B)/obj/tools/%.o: tools/%.c
	@mkdir -p $(dir $@)
	$(CC) $(GENFLAGS) -c $< -o $@

$(B)/rt/%.o: runtime/%.c $(RT_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(RTFLAGS) -c $< -o $@

# SMAA (third_party/smaa, MIT): the reference shader as a C string for runtime/nvn/smaa_data.c (both backends build
# their passes from it); the non-ASCII bytes in its comments become '?', and its CRLF line ends plain LF (a CR left in
# the string literals ends them early on macOS clang)
$(B)/inc/smaa_hlsl.inc: third_party/smaa/SMAA.hlsl
	@mkdir -p $(dir $@)
	LC_ALL=C tr -d '\r' < $< | LC_ALL=C tr '\200-\377' '?' | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' > $@
$(B)/rt/nvn/smaa_data.o: $(B)/inc/smaa_hlsl.inc
$(B)/rt/nvn/smaa_data.o: RTFLAGS += -I$(B)/inc -Ithird_party/smaa/Textures

$(B)/rt/%.o: runtime/%.cpp $(RT_HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OSFLAGS) -c $< -o $@

$(B)/rt/%.o: runtime/%.m $(RT_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(RTFLAGS) -fobjc-arc -c $< -o $@

# linked to a temp file, then renamed over: a copy of the game still running keeps its (old) file - relinking in
# place gets running copies killed by macOS (code signature of the changed pages)
$(EXE): $(GEN_OBJ) $(RT_OBJ) $(BACKEND_OBJ) $(UI_OBJ) $(IMGUI_OBJ)
	$(CXX) -o $@.tmp $^ $(LDFLAGS)
	mv -f $@.tmp $@

$(B)/rt/ui/%.o: runtime/ui/%.mm $(RT_HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(UIFLAGS) -c $< -o $@

$(B)/imgui/%.o: $(IMGUI)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/imgui/imgui_impl_%.o: $(IMGUI)/backends/imgui_impl_%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/imgui/imgui_impl_metal.o: $(IMGUI)/backends/imgui_impl_metal.mm
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -fobjc-arc -c $< -o $@

# Python: tools/venv (macOS, Linux: bin/python, Windows: Scripts/python.exe)
PY := $(firstword $(wildcard tools/venv/bin/python) $(wildcard tools/venv/Scripts/python.exe))

gen:
	cd tools && ../$(PY) gen.py sdk ../extracted/exefs/sdk 0x7004000000 ../$(GEN)/sdk --hle ../runtime/hle/hle_symbols.txt $(GENSIMD)
	cd tools && ../$(PY) gen.py main ../extracted/exefs/main 0x7000000000 ../$(GEN)/main $(GENSIMD)

gen-objs: $(GEN_OBJ)

# Runtime objects only (no generated code, no link): quick compile check
runtime-objs: $(RT_OBJ) $(BACKEND_OBJ) $(UI_OBJ)

$(B)/rt/platform/windows_rc.o: runtime/platform/windows.rc docs/icon/icon.ico
	@mkdir -p $(dir $@)
	$(WINDRES) -I docs/icon -O coff $< -o $@

# hactool (ISC): the start screen extracts the user's dump with it; shipped next to the executable
HACTOOL  := tools/hactool/hactool$(if $(filter windows,$(PLATFORM)),.exe)
$(HACTOOL):
	[ -d tools/hactool ] || git clone -q https://github.com/SciresM/hactool tools/hactool
	cd tools/hactool && cp -n config.mk.template config.mk; $(MAKE) CC=clang
hactool: $(HACTOOL)

VERSION  := $(shell sed -n 's/^\#define LARECOMP_VERSION "\(.*\)"/\1/p' runtime/ui/launcher.mm)

# macOS: build/LAS Recomp.app, runs on macOS $(APP_MIN)+: the game rebuilt for that in build/mac15 against build/deps
# (`make deps`: SDL3, LGPL FFmpeg, hactool, keystone, Python, from pinned sources), the generator helper when built
# (`make rando-helper` after `make deps`), packaged by tools/bundle_macos.sh. No game files, keys or fonts.
APP_MIN  := 15.0
APP_B    := build/mac15
deps:
	MACOS_MIN=$(APP_MIN) tools/build_deps_macos.sh
app:
	@[ -f build/deps/lib/libSDL3.0.dylib ] || { echo "make deps first"; exit 1; }
	$(MAKE) B=$(APP_B) MACOS_MIN=$(APP_MIN) DEPS=$(CURDIR)/build/deps $(APP_B)/lasrecomp
	MACOS_MIN=$(APP_MIN) tools/bundle_macos.sh $(APP_B)/lasrecomp

# macOS release disk image (the .app + an Applications shortcut to drag it onto): build/dist/LAS Recomp-<v>-macOS.dmg
dmg: app
	tools/make_dmg.sh

# Windows: build/dist/LAS Recomp-<version>-windows.zip, top folder "LAS Recomp/": the exe, hactool, the seed generator
# helper (make rando-helper) and the DLLs they load (runs without MSYS2). The exe is a release build in $(WIN_B):
# linked against the LGPL FFmpeg from `make deps-windows` (MSYS2's is a GPLv3 build) and without a console window
# (RELEASE=1: the log goes to lasrecomp.log next to the exe, fatal errors show a message box). Its DLLs are looked up
# with build/deps-win/bin first (the FFmpeg DLL names are the same as MSYS2's GPL ones). vulkan-1.dll is left
# out: every Vulkan driver installs the loader in System32. Saves, seeds and game files go next to the exe at run time.
# Needs: Windows 10+ x64, a Vulkan 1.3 driver, an AVX2 CPU (GENARCH x86-64-v3; checked at startup).
DIST     := build/dist/LAS Recomp
WIN_B    := build/win-release
deps-windows:
	tools/build_deps_windows.sh
dist: $(HACTOOL)
	@[ -f build/deps-win/lib/libavcodec.dll.a ] || { echo "make deps-windows first (LGPL FFmpeg)"; exit 1; }
	@[ -d build/helper/rando-gen ] || { echo "make rando-helper first (the seed generator)"; exit 1; }
	$(MAKE) B=$(WIN_B) DEPS=$(CURDIR)/build/deps-win RELEASE=1 $(WIN_B)/lasrecomp.exe
	rm -rf build/dist && mkdir -p "$(DIST)"
	cp $(WIN_B)/lasrecomp.exe $(HACTOOL) "$(DIST)/"
	cp docs/icon/icon_1024.png "$(DIST)/icon.png"
	cp third_party/fonts/Baloo2/Baloo2-SemiBold.ttf "$(DIST)/"  # (start screen font, OFL)
	cp third_party/fonts/Baloo2/OFL.txt "$(DIST)/Baloo2-OFL.txt"
	cp README.md LICENSE "$(DIST)/"  # (licenses and source links of the bundled components: README "License")
	cp -R build/helper/rando-gen "$(DIST)/"
	PATH="$(CURDIR)/build/deps-win/bin:$$PATH" ldd $(WIN_B)/lasrecomp.exe $(HACTOOL) | awk '(/ucrt64/ || /deps-win/) && !/vulkan-1/ {print $$3}' | sort -u | \
	    xargs -I{} cp -u {} "$(DIST)/"
	! ls "$(DIST)" | grep -iE 'x264|x265|xvid|postproc'  # (no GPL codec libraries in the release)
	cd build/dist && if command -v zip >/dev/null; then zip -qr "LAS Recomp-$(VERSION)-windows.zip" "LAS Recomp"; \
	    else /c/Windows/System32/tar.exe -a -cf "LAS Recomp-$(VERSION)-windows.zip" "LAS Recomp"; fi
	@echo "build/dist/LAS Recomp-$(VERSION)-windows.zip"

# Seed generator as a standalone helper, so players need no Python: build/helper/rando-gen/ (rando-gen[.exe] +
# _internal/), frozen by PyInstaller from tools/rando/rando_gen.spec in its own venv (build/rando-venv). Ship the
# rando-gen folder next to the exe (Windows) or as "LAS Recomp.app"/Contents/Resources/rando-gen (runtime/ui/rando_tab.mm
# generator_cmd). PyInstaller only builds for the OS it runs on.
# macOS arm64: the relocatable Python 3.12 and keystone from `make deps` (macOS 11+ / 15+: the helper runs where the
#   app does); without them Homebrew's python@3.12 and keystone (then only on the macOS they were built for). The
#   keystone wheel has no dylib: KEYSTONE_LIB tells rando_gen.spec which one to take.
# Windows x86-64 (untested): from the MSYS2 shell with a python.org CPython 3.12 (MSYS2's Python has no oead wheel),
#   e.g. make rando-helper RANDO_PY="/c/Users/<you>/AppData/Local/Programs/Python/Python312/python.exe"
ifeq ($(PLATFORM),macos)
RANDO_PY  ?= $(firstword $(wildcard build/deps/python/bin/python3.12) python3.12)
export KEYSTONE_LIB ?= $(wildcard $(CURDIR)/build/deps/lib/libkeystone.dylib)
RANDO_VPY := build/rando-venv/bin/python
else
RANDO_PY  ?= py -3.12
RANDO_VPY := build/rando-venv/Scripts/python.exe
endif

rando-helper: build/rando-venv/.ok
	$(if $(filter windows,$(PLATFORM)),USERPROFILE="$$(cygpath -w ~ 2>/dev/null || echo $$HOME)") \
	$(RANDO_VPY) -m PyInstaller --noconfirm --clean --log-level WARN --distpath build/helper --workpath build/pyinstaller \
	    tools/rando/rando_gen.spec
	@du -sh build/helper/rando-gen

build/rando-venv/.ok: third_party/LAS-Randomizer/requirements.txt
	$(RANDO_PY) -m venv build/rando-venv
	$(RANDO_VPY) -m pip install -q -r third_party/LAS-Randomizer/requirements.txt pyinstaller==6.22.3
	touch $@

# Linux: build/dist/LAS_Recomp-<version>-<arch>.AppImage, built in Ubuntu 22.04 containers (podman) with SDL3, LGPL
# FFmpeg, shaderc, hactool and the seed generator helper inside; the Vulkan loader and GPU drivers are the system's.
# `make appimage` (ARCH: this machine's, or aarch64 cross-compiled), `make appimages` both. See docs/linux.md.
appimage:
	tools/linux/appimage.sh $(ARCH)
appimages:
	tools/linux/appimage.sh x86_64
	tools/linux/appimage.sh aarch64
# release check of the x86-64 AppImage: start screen, into the game, warps to the overworld and every dungeon
smoketest:
	tools/linux/smoketest.sh

.PHONY: dmg all gen gen-objs runtime-objs hactool deps deps-windows app dist rando-helper appimage appimages smoketest

build/test_shaders: tools/test_shaders.m runtime/nvn/maxwell/translate.c runtime/nvn/maxwell/shader.h
	clang -O1 -fobjc-arc -framework Metal -framework Foundation -o $@ tools/test_shaders.m runtime/nvn/maxwell/translate.c
