import UIKit

/// A customized touch control: center as a fraction of the layout area, size as a multiplier.
struct Placement: Codable, Equatable {
    var x: CGFloat
    var y: CGFloat
    var scale: CGFloat
}

final class GameSettings {
    static let shared = GameSettings()
    /// FULL stretches, SQUARE keeps square pixels, ARCADE keeps the arcade monitor's pixel shape.
    enum Screen: Int { case full, square, arcade }
    enum TouchMode: Int { case auto, on, off }
    static let defaultHidden: Set<ControlID> = [.dunk, .shove]

    private let store = UserDefaults.standard
    private init() {
        store.register(defaults: ["edgeSprint": true, "floatingStick": true, "haptics": true, "opacity": 2, "buttonSize": 1,
                                  "screen": Screen.arcade.rawValue, "colorSides": true, "crtMode": 1])
    }

    var screen: Screen {
        get { Screen(rawValue: store.integer(forKey: "screen")) ?? .arcade }
        set { store.set(newValue.rawValue, forKey: "screen") }
    }
    var touchMode: TouchMode {
        get { TouchMode(rawValue: store.integer(forKey: "touchMode")) ?? .auto }
        set { store.set(newValue.rawValue, forKey: "touchMode") }
    }
    /// Fills unused display margins with color.
    var colorSides: Bool {
        get { store.bool(forKey: "colorSides") }
        set { store.set(newValue, forKey: "colorSides") }
    }
    /// 0 disables CRT processing; 1 selects NewPixie.
    var crtMode: Int {
        get { min(max(store.integer(forKey: "crtMode"), 0), 1) }
        set { store.set(newValue, forKey: "crtMode") }
    }
    var smoothPixels: Bool {
        get { store.bool(forKey: "smoothPixels") }
        set { store.set(newValue, forKey: "smoothPixels") }
    }
    var edgeSprint: Bool {
        get { store.bool(forKey: "edgeSprint") }
        set { store.set(newValue, forKey: "edgeSprint") }
    }
    var floatingStick: Bool {
        get { store.bool(forKey: "floatingStick") }
        set { store.set(newValue, forKey: "floatingStick") }
    }
    var haptics: Bool {
        get { store.bool(forKey: "haptics") }
        set { store.set(newValue, forKey: "haptics") }
    }
    /// 0 low, 1 medium, 2 high.
    var opacity: Int {
        get { min(max(store.integer(forKey: "opacity"), 0), 2) }
        set { store.set(newValue, forKey: "opacity") }
    }
    /// 0 small, 1 medium, 2 large.
    var buttonSize: Int {
        get { min(max(store.integer(forKey: "buttonSize"), 0), 2) }
        set { store.set(newValue, forKey: "buttonSize") }
    }
    var tutorialSeen: Bool {
        get { store.bool(forKey: "tutorialSeen") }
        set { store.set(newValue, forKey: "tutorialSeen") }
    }
    var opacityValue: CGFloat { [0.4, 0.7, 1][opacity] }
    var sizeValue: CGFloat { [0.85, 1, 1.18][buttonSize] }

    var layout: [ControlID: Placement] {
        get {
            guard let data = store.data(forKey: "layout"),
                  let raw = try? JSONDecoder().decode([String: Placement].self, from: data) else { return [:] }
            return Dictionary(uniqueKeysWithValues: raw.compactMap { key, value in ControlID(rawValue: key).map { ($0, value) } })
        }
        set {
            let raw = Dictionary(uniqueKeysWithValues: newValue.map { ($0.key.rawValue, $0.value) })
            store.set(try? JSONEncoder().encode(raw), forKey: "layout")
        }
    }
    var hiddenControls: Set<ControlID> {
        get {
            guard let names = store.stringArray(forKey: "hiddenControls") else { return Self.defaultHidden }
            return Set(names.compactMap(ControlID.init(rawValue:)))
        }
        set { store.set(newValue.map(\.rawValue).sorted(), forKey: "hiddenControls") }
    }
    var padBindings: [Action: [PadButton]] {
        get {
            guard let data = store.data(forKey: "padBindings"),
                  let raw = try? JSONDecoder().decode([String: [PadButton]].self, from: data) else { return PadButton.defaults }
            return Dictionary(uniqueKeysWithValues: raw.compactMap { key, value in Action(rawValue: key).map { ($0, value) } })
        }
        set {
            let raw = Dictionary(uniqueKeysWithValues: newValue.map { ($0.key.rawValue, $0.value) })
            store.set(try? JSONEncoder().encode(raw), forKey: "padBindings")
        }
    }
}
