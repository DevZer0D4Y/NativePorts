import Foundation

struct ShowtimeFrame {
    let pixels: Data
    let width: Int
    let height: Int
    let aspect: Double
    let saveFailed: Bool
}

/// MAME's libco context must stay on one OS thread, including initialization,
/// resets and saves. A serial dispatch queue does not guarantee that affinity.
final class EmulationRunner {
    private let condition = NSCondition()
    private let traceLock = NSLock()
    private var traceURL: URL?
    func trace(_ message: String) {
        guard ProcessInfo.processInfo.environment["SHOWTIME_BENCHMARK"] != nil else { return }
        traceLock.lock(); defer { traceLock.unlock() }
        guard let url = traceURL, let file = try? FileHandle(forWritingTo: url) else { return }
        defer { try? file.close() }
        _ = try? file.seekToEnd()
        try? file.write(contentsOf: Data(("\(Date()) \(message)\n").utf8))
    }
    private var thread: Thread!
    private var commands: [() -> Void] = []
    private var running = false
    private var finished = false
    private var players = [PadState](repeating: PadState(), count: 4)
    private var pending = [Set<Action>](repeating: [], count: 4)
    private var starts = [Bool](repeating: false, count: 4)
    private var sprint = false
    private var latest: ShowtimeFrame?
    // These fields belong exclusively to the emulation thread.
    private var sequencers = [ActionSequencer](repeating: ActionSequencer(), count: 4)
    private var lastSave = 0.0
    private var videoSerial: UInt64 = 0
    private var pacingDeadline = 0.0

    init() {
        thread = Thread { [weak self] in self?.loop() }
        thread.name = "Showtime Emulation"
        thread.qualityOfService = .userInteractive
        thread.start()
    }

    private func sync<T>(_ work: @escaping () -> T) -> T {
        let done = DispatchSemaphore(value: 0)
        var result: T?
        condition.lock()
        commands.append { result = work(); done.signal() }
        condition.signal()
        condition.unlock()
        done.wait()
        return result!
    }

    func start(_ url: URL, directory: URL) -> (sampleRate: Double, error: String?) {
        sync {
            self.traceLock.lock()
            self.traceURL = directory.appendingPathComponent("showtime-runner.log")
            if ProcessInfo.processInfo.environment["SHOWTIME_BENCHMARK"] != nil {
                try? Data().write(to: self.traceURL!)
            }
            self.traceLock.unlock()
            self.trace("starting core")
            guard ht_start(url.path, directory.path) else {
                return (0, String(cString: ht_error()))
            }
            self.trace("core ready")
            self.videoSerial = 0
            self.pacingDeadline = 0
            self.lastSave = ProcessInfo.processInfo.systemUptime
            return (ht_sample_rate(), nil)
        }
    }

    func trigger(_ action: Action) {
        condition.lock(); pending[0].insert(action); condition.unlock()
    }

    func update(players pads: [PadState], touchHeld: Set<Action>, directions: UInt16, sprint: Bool) {
        condition.lock()
        for i in 0..<4 {
            players[i] = i < pads.count ? pads[i] : PadState()
            pending[i].formUnion(players[i].pressed)
            starts[i] = starts[i] || players[i].startTap
        }
        players[0].held.formUnion(touchHeld)
        players[0].directions |= directions
        self.sprint = sprint
        condition.unlock()
    }

    /// The display consumes only the newest complete frame. A slow display
    /// never queues obsolete pictures or blocks the emulated CPU.
    func takeFrame() -> ShowtimeFrame? {
        condition.lock(); defer { condition.unlock() }
        let frame = latest; latest = nil
        return frame
    }

    func setPaused(_ paused: Bool) {
        trace("pause requested: \(paused)")
        // Stop submitting frames before waiting for the current one to finish.
        condition.lock(); running = false; condition.signal(); condition.unlock()
        sync {
            for i in 0..<4 { self.sequencers[i].reset(); ht_set_buttons(UInt32(i), 0) }
            ht_audio_clear()
            self.pacingDeadline = 0
            if paused { _ = ht_save_data() }
            self.condition.lock()
            self.players = [PadState](repeating: PadState(), count: 4)
            self.pending = [Set<Action>](repeating: [], count: 4)
            self.starts = [Bool](repeating: false, count: 4)
            self.sprint = false
            self.running = !paused
            self.condition.unlock()
        }
    }

    func reset() { sync { ht_reset() } }

    func shutdown() {
        condition.lock(); running = false; condition.signal(); condition.unlock()
        sync { ht_stop() }
        condition.lock(); finished = true; condition.signal(); condition.unlock()
    }

    private func loop() {
        var firstFrame = true
        while true {
            condition.lock()
            while !finished && commands.isEmpty && !running { condition.wait() }
            if finished { condition.unlock(); return }
            if !commands.isEmpty {
                let command = commands.removeFirst()
                condition.unlock(); command(); continue
            }
            let inputs = players, presses = pending, startTaps = starts, sprintHeld = sprint
            pending = [Set<Action>](repeating: [], count: 4)
            starts = [Bool](repeating: false, count: 4)
            condition.unlock()

            let began = ProcessInfo.processInfo.systemUptime
            autoreleasepool {
                for i in 0..<4 {
                    for action in presses[i] { sequencers[i].trigger(action) }
                    if startTaps[i] { sequencers[i].pulseStart() }
                    let mask = sequencers[i].frame(directions: inputs[i].directions,
                                                  held: inputs[i].held, sprint: i == 0 && sprintHeld)
                    ht_set_buttons(UInt32(GamepadHub.playerOrder[i]), mask)
                }
                if firstFrame { trace("first frame begin") }
                ht_run()
                if firstFrame { firstFrame = false; trace("first frame end") }
                var saveFailed = false
                if began - lastSave >= 5 {
                    lastSave = began; saveFailed = !ht_save_data()
                    trace("host thermal=\(ProcessInfo.processInfo.thermalState.rawValue) lowPower=\(ProcessInfo.processInfo.isLowPowerModeEnabled)")
                }
                let serial = ht_video_serial()
                let w = Int(ht_width()), h = Int(ht_height())
                if serial != videoSerial, w > 0, h > 0, let bytes = ht_pixels() {
                    videoSerial = serial
                    let frame = ShowtimeFrame(pixels: Data(bytes: bytes, count: w * h * 4),
                                              width: w, height: h, aspect: ht_aspect_ratio(), saveFailed: saveFailed)
                    condition.lock(); latest = frame; condition.unlock()
                }
            }
            // Keep one monotonic schedule: a relative sleep after each frame
            // accumulates wake-up jitter and slows both game time and audio.
            // Absorb brief overruns, but discard long stalls rather than building
            // an unbounded catch-up queue.
            let interval = 1 / ht_fps()
            if pacingDeadline == 0 { pacingDeadline = began }
            pacingDeadline += interval
            let ended = ProcessInfo.processInfo.systemUptime
            if ended - pacingDeadline > 0.5 { pacingDeadline = ended }
            let deadline = pacingDeadline
            condition.lock()
            while running && commands.isEmpty {
                let remaining = deadline - ProcessInfo.processInfo.systemUptime
                if remaining <= 0 { break }
                _ = condition.wait(until: Date(timeIntervalSinceNow: remaining))
            }
            condition.unlock()
        }
    }
}
