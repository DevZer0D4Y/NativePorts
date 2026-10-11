from pathlib import Path
import subprocess,tempfile,zipfile,struct
r=Path(__file__).resolve().parents[1];exe=r/'dist/ssf2t/macos/ssf2t'
with tempfile.TemporaryDirectory(dir=r/'.build') as tmp:
 d=Path(tmp);bad=d/'broken.zip';bad.write_bytes(b'not a zip');res=subprocess.run([exe,'--audit',bad],capture_output=True,text=True);assert res.returncode==2
 missing=d/'empty.zip'
 with zipfile.ZipFile(missing,'w') as z:z.writestr('other.bin',b'x')
 res=subprocess.run([exe,'--audit',missing],capture_output=True,text=True);assert res.returncode==2 and 'sfxe.03c' in res.stdout
 forged=d/'forged.zip'
 with zipfile.ZipFile(forged,'w',compression=zipfile.ZIP_STORED) as z:z.writestr('sfxe.03c',bytes(524288))
 data=bytearray(forged.read_bytes());central=data.index(b'PK\x01\x02');struct.pack_into('<I',data,central+16,0x2fa1f396);struct.pack_into('<I',data,14,0x2fa1f396);forged.write_bytes(data)
 res=subprocess.run([exe,'--audit',forged],capture_output=True,text=True);assert res.returncode==2 and 'sfxe.03c' in res.stdout
 print('ROM audit: malformed ZIP, missing ROMs, and forged directory/payload checksums rejected.')
