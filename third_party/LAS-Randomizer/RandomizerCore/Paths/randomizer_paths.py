import os
import sys
import appdirs
import platform

# check if user is running a precompiled binary
# [LAS Recomp: the PyInstaller seed generator (tools/rando/rando_gen.spec) keeps the source layout under sys._MEIPASS
#  and must behave exactly like the source run (same spoiler format, no user-data dir): it takes the branch below]
if getattr(sys, "frozen", False) and not hasattr(sys, '_MEIPASS'):
    IS_RUNNING_FROM_SOURCE = False
    ROOT_PATH = os.path.dirname(sys.executable)
    ASM_PATH = os.path.join(ROOT_PATH, 'lib/RandomizerCore/ASM/Patches')
    DATA_PATH = os.path.join(ROOT_PATH, 'Data')
    RESOURCE_PATH = os.path.join(ROOT_PATH, 'Resources')
    if platform.system() == 'Darwin':
        userdata_path = appdirs.user_data_dir('randomizer', 'LAS Randomizer')
        if not os.path.isdir(userdata_path):
            os.mkdir(userdata_path)
        SETTINGS_PATH = os.path.join(userdata_path, 'settings.txt')
        LOGS_PATH = os.path.join(userdata_path, 'log.txt')
    else:
        SETTINGS_PATH = os.path.join('.', 'settings.txt')
        LOGS_PATH = os.path.join('.', 'log.txt')
else:
    IS_RUNNING_FROM_SOURCE = True
    ROOT_PATH = os.path.dirname(sys.argv[0])
    ASM_PATH = os.path.join(ROOT_PATH, 'RandomizerCore/ASM/Patches')
    DATA_PATH = os.path.join(ROOT_PATH, 'RandomizerCore/Data')
    RESOURCE_PATH = os.path.join(ROOT_PATH, 'Resources')  # [LAS Recomp: was RandomizerUI/Resources (UI removed)]
    SETTINGS_PATH = os.path.join(ROOT_PATH, 'settings.txt')
    LOGS_PATH = os.path.join(ROOT_PATH, 'log.txt')

LOGIC_PATH = os.path.join(DATA_PATH, 'logic.yml')
