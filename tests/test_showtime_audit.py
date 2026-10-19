"""Exercise actual arcade import checks with supplied images and bad inputs."""
import ctypes,subprocess,tempfile,zipfile,sys
from pathlib import Path
r=Path(__file__).resolve().parents[1]
b=r/'.build/showtime-audit';b.mkdir(parents=True,exist_ok=True)
subprocess.run(['clang','-shared','-fPIC',str(r/'ports/nbashowt/ios/Hangtime/ROMAudit.c'),'-lz','-o',str(b/'audit.dylib')],check=True)
lib=ctypes.CDLL(str(b/'audit.dylib'))
for name in ['ht_validate_rom','ht_validate_disk']:
 f=getattr(lib,name);f.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_size_t];f.restype=ctypes.c_bool
report=ctypes.create_string_buffer(8192)
def check(name,path,expected):assert getattr(lib,name)(str(path).encode(),report,len(report))==expected,report.value
if len(sys.argv)>2:
 rom,disk=map(Path,sys.argv[1:3]);check('ht_validate_rom',rom,True);check('ht_validate_disk',disk,True)
 with tempfile.TemporaryDirectory(dir=b) as tmp:
  tmp=Path(tmp)
  with zipfile.ZipFile(rom) as z: chips={n:z.read(n) for n in z.namelist()}
  for method in [zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED]:
   p=tmp/'valid.zip'
   with zipfile.ZipFile(p,'w',method) as z:
    for name,data in chips.items():z.writestr(name,data)
   check('ht_validate_rom',p,True)
  p=tmp/'missing.zip'
  with zipfile.ZipFile(p,'w') as z:z.writestr(next(iter(chips)),next(iter(chips.values())))
  check('ht_validate_rom',p,False)
  p=tmp/'damaged.zip';data=bytearray(rom.read_bytes());data[100]^=255;p.write_bytes(data);check('ht_validate_rom',p,False)
  p=tmp/'wrong.chd'
  with disk.open('rb') as f:h=bytearray(f.read(124))
  h[84]^=255;p.write_bytes(h);check('ht_validate_disk',p,False)
  for n in [0,8,21,22,100,123]:
   p.write_bytes(bytes(n));check('ht_validate_rom',p,False);check('ht_validate_disk',p,False)
print('PASS: arcade chips, stored/deflated ZIP, corrupt/missing chips, wrong disk and truncated inputs.')
