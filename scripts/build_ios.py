#!/usr/bin/env python3
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--team');a=p.parse_args();r=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(r/'scripts/build_core.py'),a.game,'iphoneos'],check=True)
cmd=['xcodebuild','-project',str(r/'ports'/a.game/'ios/Hangtime.xcodeproj'),'-scheme','Hangtime','-configuration','Release','-sdk','iphoneos','-destination','generic/platform=iOS','-derivedDataPath',str(r/'.build'/a.game/'app')]
cmd+=['DEVELOPMENT_TEAM='+a.team] if a.team else ['CODE_SIGNING_ALLOWED=NO']
subprocess.run(cmd,check=True)
