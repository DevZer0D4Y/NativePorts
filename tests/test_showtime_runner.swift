import Foundation

// A deliberately slow core exercises presentation, input and pause races
// without needing game data. Every core entry must use the same OS thread.
private let coreLock = NSLock()
private var owner: Thread?
private var frames = 0
private var masks = [UInt16](repeating: 0, count: 4)
private var observed = [[UInt16]]()
private let picture = UnsafeMutablePointer<UInt8>.allocate(capacity: 16)
private func checkThread() {
    assert(!Thread.isMainThread)
    if let owner { assert(Thread.current === owner, "libco thread affinity lost") }
    else { owner = Thread.current }
}
func ht_start(_ path: String, _ directory: String) -> Bool { checkThread(); picture.initialize(repeating: 0, count: 16); return true }
func ht_error() -> UnsafePointer<CChar> { fatalError("unexpected start error") }
func ht_run() {
    checkThread()
    Thread.sleep(forTimeInterval: 0.035)
    coreLock.lock()
    frames += 1; observed.append(masks)
    picture.update(repeating: UInt8(truncatingIfNeeded: frames), count: 16)
    coreLock.unlock()
}
func ht_sample_rate() -> Double { checkThread(); return 48000 }
func ht_fps() -> Double { checkThread(); return 60 }
func ht_width() -> UInt32 { checkThread(); return 2 }
func ht_height() -> UInt32 { checkThread(); return 2 }
func ht_aspect_ratio() -> Double { checkThread(); return 4 / 3 }
func ht_pixels() -> UnsafePointer<UInt8>? { checkThread(); return UnsafePointer(picture) }
func ht_video_serial() -> UInt64 { checkThread(); return UInt64(frames) }
func ht_set_buttons(_ player: UInt32, _ mask: UInt16) { checkThread(); masks[Int(player)] = mask }
func ht_audio_clear() { checkThread() }
func ht_save_data() -> Bool { checkThread(); return true }
func ht_reset() { checkThread() }
func ht_stop() { checkThread() }

@main struct RunnerTest {
    static func main() {
        let runner = EmulationRunner()
        assert(runner.start(URL(fileURLWithPath: "/dummy"), directory: URL(fileURLWithPath: "/dummy")).error == nil)
        runner.setPaused(false)
        var retained: [ShowtimeFrame] = []
        let begin = ProcessInfo.processInfo.systemUptime
        for i in 0..<45 {
            var pad = PadState()
            pad.directions = Pad.left
            pad.startTap = i == 1
            runner.update(players: [PadState(), pad], touchHeld: [.shoot], directions: Pad.right, sprint: false)
            if i == 2 { runner.trigger(.spin) }
            if let frame = runner.takeFrame() {
                assert(frame.pixels.count == 16)
                assert(Set(frame.pixels).count == 1, "partially written frame")
                retained.append(frame)
            }
            // Display and input must remain responsive while the core is busy.
            Thread.sleep(forTimeInterval: 0.005)
        }
        assert(ProcessInfo.processInfo.systemUptime - begin < 0.8, "main thread blocked by emulation")
        runner.setPaused(true)
        coreLock.lock(); let pausedAt = frames; coreLock.unlock()
        Thread.sleep(forTimeInterval: 0.1)
        coreLock.lock()
        assert(frames == pausedAt, "core continued after pause returned")
        assert(observed.contains { $0[GamepadHub.playerOrder[1]] & Pad.start != 0 }, "start edge lost between frames")
        assert(observed.contains { $0[0] & (Pad.shoot | Pad.right) == Pad.shoot | Pad.right })
        assert(observed.contains { $0[GamepadHub.playerOrder[1]] & Pad.left != 0 })
        coreLock.unlock()
        for frame in retained { assert(Set(frame.pixels).count == 1, "published frame mutated") }
        runner.reset()
        runner.setPaused(false)
        Thread.sleep(forTimeInterval: 0.08)
        runner.setPaused(true)
        coreLock.lock(); assert(frames > pausedAt); coreLock.unlock()
        runner.shutdown()
        print("PASS: Showtime thread affinity, nonblocking display, input edges, frame ownership, pause/reset/resume")
    }
}
