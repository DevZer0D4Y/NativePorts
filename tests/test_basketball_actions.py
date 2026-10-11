from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
b=r/'.build/basketball-actions';b.mkdir(parents=True,exist_ok=True)
for game in ('nbashowt','slamjam96'):
 source=(r/'ports'/game/'ios/Hangtime/Input.swift').read_text()
 code=source[source.index('enum Pad {'):source.index('/// Bindable gamepad buttons.')]
 tests='''
var seq = ActionSequencer()
assert(seq.frame(directions: Pad.left, held: [.shoot], sprint: false) == (Pad.left | Pad.shoot))
seq.reset()
assert(seq.frame(directions: Pad.up, held: [], sprint: true) == (Pad.up | Pad.turbo))
seq.pulseStart()
for _ in 0..<6 { assert(seq.frame(directions: 0, held: [], sprint: false) == Pad.start) }
assert(seq.frame(directions: 0, held: [], sprint: false) == 0)
'''
 if game=='nbashowt':tests+='''
seq.reset(); seq.trigger(.dunk)
assert(seq.frame(directions: 0, held: [.dunk], sprint: false) == Pad.turbo)
assert(seq.frame(directions: 0, held: [.dunk], sprint: false) == (Pad.turbo | Pad.shoot))
assert(seq.frame(directions: 0, held: [], sprint: false) == 0)
seq.trigger(.shove)
assert(seq.frame(directions: 0, held: [.shove], sprint: false) == Pad.turbo)
assert(seq.frame(directions: 0, held: [.shove], sprint: false) == (Pad.turbo | Pad.pass))
seq.reset();seq.trigger(.spin)
for frame in 0..<10 {
 let pressed = [2,3,4,8,9].contains(frame)
 assert(seq.frame(directions: Pad.right, held: [], sprint: true) == (Pad.right | (pressed ? Pad.turbo : 0)))
}
assert(seq.frame(directions: 0, held: [], sprint: false) == 0)
var another = ActionSequencer()
seq.trigger(.spin)
assert(another.frame(directions: 0, held: [], sprint: false) == 0)
seq.reset();assert(seq.frame(directions: 0, held: [], sprint: false) == 0)
'''
 else:tests+='''
seq.reset()
for action in [Action.spin,.dunk,.shove] {
 assert(seq.frame(directions: 0, held: [action], sprint: false) == action.mask)
}
'''
 p=b/(game+'.swift');p.write_text(code+tests+'\nprint("PASS: '+game+' Swift action sequencing")\n')
 exe=b/game;subprocess.run(['swiftc','-module-cache-path',str(b/'module-cache'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
