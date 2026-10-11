#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,sys
r=Path(__file__).resolve().parents[4]
subprocess.run([sys.executable,str(r/'scripts/build_core.py'),"nbashowt",os.environ.get('PLATFORM_NAME','iphoneos')],check=True)
