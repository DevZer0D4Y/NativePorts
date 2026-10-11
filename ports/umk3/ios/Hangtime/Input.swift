import GameController
import QuartzCore

/// RetroPad bits read by the core's classic layout: B = Turbo, Y = Pass/Steal, A = Shoot/Block.
enum Pad {
    static let shoot: UInt16 = 2
    static let pass: UInt16 = 2048
    static let turbo: UInt16 = 256
    static let spin: UInt16 = 512
    static let dunk: UInt16 = 1
    static let shove: UInt16 = 8192
    static let coin: UInt16 = 4
    static let start: UInt16 = 8
    static let up: UInt16 = 16, down: UInt16 = 32, left: UInt16 = 64, right: UInt16 = 128
}

/// What a player can do; touch buttons and gamepad buttons both map onto these.
enum Action: String, CaseIterable, Codable {
    case shoot, pass, turbo, spin, dunk, shove, coin, start

    var title: String {
        switch self {
        case .shoot: return "LOW PUNCH"
        case .pass: return "HIGH PUNCH"
        case .turbo: return "BLOCK"
        case .spin: return "LOW KICK"
        case .dunk: return "HIGH KICK"
        case .shove: return "RUN"
        case .coin: return "COIN"
        case .start: return "START"
        }
    }
    var subtitle: String? { nil }
    var mask: UInt16 {
        switch self {
        case .shoot: return Pad.shoot
        case .pass: return Pad.pass
        case .turbo: return Pad.turbo
        case .spin: return Pad.spin
        case .dunk: return Pad.dunk
        case .shove: return Pad.shove
        case .coin: return Pad.coin
        case .start: return Pad.start
        }
    }
}

/// Builds one player's RetroPad mask per emulated frame, including the macro buttons.
struct ActionSequencer {
    private var startPulse = 0
    mutating func trigger(_ action: Action) {}
    mutating func pulseStart() { startPulse = 6 }
    mutating func reset() { startPulse = 0 }
    mutating func frame(directions: UInt16, held: Set<Action>, sprint: Bool) -> UInt16 {
        var mask = directions
        for action in held { mask |= action.mask }
        if startPulse > 0 { mask |= Pad.start; startPulse -= 1 }
        return mask
    }
}

/// Bindable gamepad buttons. Menu is reserved for START (tap) and PAUSE (hold); Home also pauses.
/// Share and the four paddles exist on Xbox Series and Elite controllers.
enum PadButton: String, CaseIterable, Codable {
    case a, b, x, y, lb, rb, lt, rt, l3, r3, view, share, p1, p2, p3, p4

    static let defaults: [Action: [PadButton]] = [
.shoot: [.x],.pass: [.y],.turbo: [.rb],.spin: [.a],.dunk: [.b],.shove: [.rt],.coin: [.view]
    ]

    func input(in g: GCExtendedGamepad) -> GCControllerButtonInput? {
        switch self {
        case .a: return g.buttonA
        case .b: return g.buttonB
        case .x: return g.buttonX
        case .y: return g.buttonY
        case .lb: return g.leftShoulder
        case .rb: return g.rightShoulder
        case .lt: return g.leftTrigger
        case .rt: return g.rightTrigger
        case .l3: return g.leftThumbstickButton
        case .r3: return g.rightThumbstickButton
        case .view: return g.buttonOptions
        case .share: return (g as? GCXboxGamepad)?.buttonShare
        case .p1: return (g as? GCXboxGamepad)?.paddleButton1
        case .p2: return (g as? GCXboxGamepad)?.paddleButton2
        case .p3: return (g as? GCXboxGamepad)?.paddleButton3
        case .p4: return (g as? GCXboxGamepad)?.paddleButton4
        }
    }

    /// Short name printed on the controller, by family: Xbox/MFi, PlayStation, Nintendo.
    func label(family: Int) -> String {
        let names: [PadButton: [String]] = [
            .a: ["A", "✕", "B"], .b: ["B", "○", "A"], .x: ["X", "□", "Y"], .y: ["Y", "△", "X"],
            .lb: ["LB", "L1", "L"], .rb: ["RB", "R1", "R"], .lt: ["LT", "L2", "ZL"], .rt: ["RT", "R2", "ZR"],
            .l3: ["LS", "L3", "L STICK"], .r3: ["RS", "R3", "R STICK"], .view: ["VIEW", "SHARE", "−"],
            .share: ["SHARE", "SHARE", "SHARE"], .p1: ["P1", "P1", "P1"], .p2: ["P2", "P2", "P2"],
            .p3: ["P3", "P3", "P3"], .p4: ["P4", "P4", "P4"]
        ]
        return names[self]![family]
    }
}

/// Menu navigation from any gamepad; `start` is a tap of Menu, `pause` a hold of Menu or a Home press.
enum NavEvent { case up, down, left, right, accept, back, previousTab, nextTab, start, pause }

struct PadState {
    var directions: UInt16 = 0
    var held: Set<Action> = []
    var pressed: [Action] = []
    var startTap = false
}

/// Polls up to four extended gamepads (Xbox, PlayStation, MFi, Switch): game input per player plus menu navigation events.
final class GamepadHub {
    /// Arcade player for each controller, in connection order: the second controller joins the other team.
    static let playerOrder = [0, 1, 2, 3]
    var bindings: [Action: [PadButton]] = PadButton.defaults { didSet { claimSystemButtons() } }
    var onCapture: ((PadButton?) -> Void)?
    private var previous: [ObjectIdentifier: Set<PadButton>] = [:]
    private var suppressed: [ObjectIdentifier: Set<PadButton>] = [:]
    private var previousDirections: [ObjectIdentifier: UInt16] = [:]
    private var previousSystem: [ObjectIdentifier: (menu: Bool, home: Bool)] = [:]
    private var menuSince: [ObjectIdentifier: CFTimeInterval] = [:]
    private var repeatState: (event: NavEvent, next: CFTimeInterval)?

    private let controllerProvider: () -> [GCController]
    private var slots = [GCController?](repeating: nil, count: 2)

    init(controllerProvider: @escaping () -> [GCController] = { GCController.controllers() }) {
        self.controllerProvider = controllerProvider
    }

    // The framework's list can reorder when a controller becomes current. Keep physical
    // controllers in their original slots, including holes after a disconnect.
    private func refreshControllers() {
        let available = controllerProvider().filter { $0.extendedGamepad != nil }
        for index in slots.indices {
            if let controller = slots[index], !available.contains(where: { $0 === controller }) {
                let id = ObjectIdentifier(controller)
                previous[id] = nil; suppressed[id] = nil; previousDirections[id] = nil
                previousSystem[id] = nil; menuSince[id] = nil
                slots[index] = nil
            }
        }
        for controller in available where !slots.contains(where: { $0 === controller }) {
            guard let index = slots.firstIndex(where: { $0 == nil }) else { break }
            slots[index] = controller
            controller.extendedGamepad?.buttonMenu.preferredSystemGestureState = .disabled
        }
    }

    var controllers: [GCController] { refreshControllers(); return slots.compactMap { $0 } }
    var connected: Bool { !controllers.isEmpty }
    var name: String? { controllers.first.map { $0.vendorName ?? "Game controller" } }
    var family: Int {
        guard let category = controllers.first?.productCategory.lowercased() else { return 0 }
        if category.contains("dualshock") || category.contains("dualsense") { return 1 }
        if category.contains("switch") || category.contains("joy-con") || category.contains("nintendo") { return 2 }
        return 0
    }

    /// Lets the app receive Home and View presses instead of the system gestures. The Xbox Share
    /// button keeps taking screenshots unless it is bound to an action.
    func claimSystemButtons() {
        let shareBound = bindings.values.contains { $0.contains(.share) }
        for c in controllers {
            c.extendedGamepad?.buttonMenu.preferredSystemGestureState = .disabled
            c.extendedGamepad?.buttonHome?.preferredSystemGestureState = .disabled
            c.extendedGamepad?.buttonOptions?.preferredSystemGestureState = .disabled
            (c.extendedGamepad as? GCXboxGamepad)?.buttonShare?.preferredSystemGestureState = shareBound ? .disabled : .enabled
        }
    }

    /// First bound button only, for sentences ("Hold RT to sprint").
    func primaryLabel(for action: Action) -> String {
        guard action != .start, let button = bindings[action]?.first else { return label(for: action) }
        return button.label(family: family)
    }

    func label(for action: Action) -> String {
        if action == .start { return family == 1 ? "OPTIONS" : family == 2 ? "+" : "MENU" }
        let buttons = bindings[action] ?? []
        return buttons.isEmpty ? "—" : buttons.map { $0.label(family: family) }.joined(separator: " · ")
    }

    /// Buttons held right now are ignored by the game until they are released (e.g. A after "RESUME").
    func suppressHeldButtons() { suppressed = previous }

    func assign(_ button: PadButton, to action: Action) {
        for key in bindings.keys { bindings[key]?.removeAll { $0 == button } }
        bindings[action] = [button]
    }

    func poll(now: CFTimeInterval) -> (players: [PadState], events: [NavEvent], active: Bool) {
        var players: [PadState] = []
        var events: [NavEvent] = []
        var active = false
        var heldNav: NavEvent?
        refreshControllers()
        for (index, controller) in slots.enumerated() {
            guard let controller, let g = controller.extendedGamepad else {
                players.append(PadState())
                continue
            }
            let player = Self.playerOrder[index]
            if controller.playerIndex.rawValue != player { controller.playerIndex = GCControllerPlayerIndex(rawValue: player) ?? .indexUnset }
            let id = ObjectIdentifier(controller)
            var state = PadState()
            let d = g.dpad, s = g.leftThumbstick
            if d.up.isPressed || s.yAxis.value > 0.35 { state.directions |= Pad.up }
            if d.down.isPressed || s.yAxis.value < -0.35 { state.directions |= Pad.down }
            if d.left.isPressed || s.xAxis.value < -0.35 { state.directions |= Pad.left }
            if d.right.isPressed || s.xAxis.value > 0.35 { state.directions |= Pad.right }
            let down = Set(PadButton.allCases.filter { $0.input(in: g)?.isPressed == true })
            let edges = down.subtracting(previous[id] ?? [])
            let newDirections = state.directions & ~(previousDirections[id] ?? 0)
            previous[id] = down
            previousDirections[id] = state.directions
            let menu = g.buttonMenu.isPressed, home = g.buttonHome?.isPressed ?? false
            let before = previousSystem[id] ?? (false, false)
            previousSystem[id] = (menu, home)
            if !down.isEmpty || state.directions != 0 || menu || home { active = true }

            if let capture = onCapture {
                if let button = PadButton.allCases.first(where: edges.contains) { capture(button) }
                else if menu && !before.menu { capture(nil) }
                players.append(PadState())
                continue
            }
            let blocked = (suppressed[id] ?? []).intersection(down)
            suppressed[id] = blocked
            let gameDown = down.subtracting(blocked)
            for (action, buttons) in bindings where buttons.contains(where: gameDown.contains) {
                state.held.insert(action)
                if buttons.contains(where: edges.contains) { state.pressed.append(action) }
            }
            // Menu: a tap is START, holding it opens the pause menu. Home always pauses.
            if menu && !before.menu { menuSince[id] = now }
            if menu, let since = menuSince[id], now - since >= 0.6 { events.append(.pause); menuSince[id] = nil }
            if !menu && before.menu && menuSince[id] != nil { state.startTap = true; menuSince[id] = nil; events.append(.start) }
            if home && !before.home { events.append(.pause) }

            if edges.contains(.a) { events.append(.accept) }
            if edges.contains(.b) { events.append(.back) }
            if edges.contains(.lb) || edges.contains(.lt) { events.append(.previousTab) }
            if edges.contains(.rb) || edges.contains(.rt) { events.append(.nextTab) }
            let nav: [(UInt16, NavEvent)] = [(Pad.up, .up), (Pad.down, .down), (Pad.left, .left), (Pad.right, .right)]
            for (bit, event) in nav {
                if newDirections & bit != 0 { events.append(event); repeatState = (event, now + 0.4) }
                if state.directions & bit != 0 && heldNav == nil { heldNav = event }
            }
            players.append(state)
        }
        if let held = heldNav, let state = repeatState, state.event == held {
            if now >= state.next { events.append(held); repeatState = (held, now + 0.12) }
        } else { repeatState = nil }
        return (players, events, active)
    }
}
