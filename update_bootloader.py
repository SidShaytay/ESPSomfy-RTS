"""PlatformIO pre-configuration hook for exact shared CC1101/Plus bootloader checks."""
Import('env')
import os
import sys
sys.path.insert(0, os.path.join(env.subst('$PROJECT_DIR'), 'tools'))
from update_bootloader import configure
configure(env)
