import UIKit

/// Light grey header/footer bar of the mobile basketball menus.
final class BarView: UIView {
    override class var layerClass: AnyClass { CAGradientLayer.self }
    override init(frame: CGRect) {
        super.init(frame: frame)
        let gradient = layer as! CAGradientLayer
        gradient.colors = [UIColor(white: 0.98, alpha: 1).cgColor, UIColor(white: 0.86, alpha: 1).cgColor]
        layer.shadowColor = UIColor.black.cgColor; layer.shadowOpacity = 0.35; layer.shadowRadius = 6; layer.shadowOffset = .zero
    }
    required init?(coder: NSCoder) { fatalError() }
}

/// Black square with a chevron followed by a label, like the "◀ SKIP" button of the reference menus.
final class BackControl: UIControl {
    init(_ title: String, symbol: String = "chevron.left") {
        super.init(frame: .zero)
        let box = UIView(); box.backgroundColor = Style.ink; box.isUserInteractionEnabled = false
        let arrow = UIImageView(image: UIImage(systemName: symbol, withConfiguration: UIImage.SymbolConfiguration(weight: .heavy)))
        arrow.tintColor = .white; arrow.contentMode = .scaleAspectFit
        let label = UILabel(); label.text = title; label.font = Style.font(17); label.textColor = Style.ink
        for view in [box, arrow, label] as [UIView] { view.translatesAutoresizingMaskIntoConstraints = false; view.isUserInteractionEnabled = false; addSubview(view) }
        NSLayoutConstraint.activate([
            box.leadingAnchor.constraint(equalTo: leadingAnchor), box.topAnchor.constraint(equalTo: topAnchor),
            box.bottomAnchor.constraint(equalTo: bottomAnchor), box.widthAnchor.constraint(equalTo: box.heightAnchor, multiplier: 1.25),
            arrow.centerXAnchor.constraint(equalTo: box.centerXAnchor), arrow.centerYAnchor.constraint(equalTo: box.centerYAnchor),
            arrow.heightAnchor.constraint(equalTo: box.heightAnchor, multiplier: 0.5),
            label.leadingAnchor.constraint(equalTo: box.trailingAnchor, constant: 8), label.centerYAnchor.constraint(equalTo: centerYAnchor),
            label.trailingAnchor.constraint(equalTo: trailingAnchor)
        ])
        accessibilityLabel = title.capitalized; accessibilityTraits = .button
    }
    required init?(coder: NSCoder) { fatalError() }
    override var isHighlighted: Bool { didSet { alpha = isHighlighted ? 0.6 : 1 } }
}

final class ChoiceControl: UIView {
    var onChange: ((Int) -> Void)?
    private(set) var selected: Int
    private var segments: [UIButton] = []
    var count: Int { segments.count }

    init(_ titles: [String], selected: Int) {
        self.selected = selected
        super.init(frame: .zero)
        let stack = UIStackView(); stack.distribution = .fillEqually; stack.spacing = 3
        stack.translatesAutoresizingMaskIntoConstraints = false; addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: leadingAnchor), stack.trailingAnchor.constraint(equalTo: trailingAnchor),
            stack.topAnchor.constraint(equalTo: topAnchor), stack.bottomAnchor.constraint(equalTo: bottomAnchor)
        ])
        for (index, title) in titles.enumerated() {
            let button = UIButton(type: .custom)
            button.setTitle(title, for: .normal); button.titleLabel?.font = Style.font(14)
            button.layer.cornerRadius = 3
            button.addAction(UIAction { [weak self] _ in self?.select(index, notify: true) }, for: .touchUpInside)
            segments.append(button); stack.addArrangedSubview(button)
        }
        refresh()
    }
    required init?(coder: NSCoder) { fatalError() }

    func select(_ index: Int, notify: Bool) {
        guard index != selected, segments.indices.contains(index) else { return }
        selected = index; refresh()
        if notify { onChange?(index) }
    }
    private func refresh() {
        for (index, button) in segments.enumerated() {
            let on = index == selected
            button.backgroundColor = on ? Style.gold : UIColor.white.withAlphaComponent(0.1)
            button.setTitleColor(on ? Style.ink : .white, for: .normal)
            button.accessibilityTraits = on ? [.button, .selected] : .button
        }
    }
}

/// A row of the pause menu that gamepad navigation can focus.
class MenuItem: UIView {
    var isCurrent = false { didSet { layer.borderWidth = isCurrent ? 2.5 : 0 } }
    init(focusColor: UIColor = Style.gold) {
        super.init(frame: .zero)
        layer.borderColor = focusColor.cgColor; layer.cornerRadius = 5
    }
    required init?(coder: NSCoder) { fatalError() }
    func activate() {}
    func adjust(_ delta: Int) {}
}

final class ChoiceRow: MenuItem {
    private let choice: ChoiceControl
    init(_ title: String, _ options: [String], selected: Int, onChange: @escaping (Int) -> Void) {
        choice = ChoiceControl(options, selected: selected)
        super.init()
        choice.onChange = onChange
        let label = UILabel(); label.text = title; label.font = Style.font(15); label.textColor = .white
        label.adjustsFontSizeToFitWidth = true; label.minimumScaleFactor = 0.7
        for view in [label, choice] as [UIView] { view.translatesAutoresizingMaskIntoConstraints = false; addSubview(view) }
        NSLayoutConstraint.activate([
            heightAnchor.constraint(equalToConstant: 44),
            label.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 10), label.centerYAnchor.constraint(equalTo: centerYAnchor),
            choice.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -6), choice.centerYAnchor.constraint(equalTo: centerYAnchor),
            choice.heightAnchor.constraint(equalToConstant: 32), choice.widthAnchor.constraint(equalToConstant: CGFloat(options.count) * 76),
            label.trailingAnchor.constraint(lessThanOrEqualTo: choice.leadingAnchor, constant: -8)
        ])
    }
    required init?(coder: NSCoder) { fatalError() }
    override func activate() { choice.select((choice.selected + 1) % choice.count, notify: true) }
    override func adjust(_ delta: Int) { choice.select(min(max(choice.selected + delta, 0), choice.count - 1), notify: true) }
}

final class ActionRow: MenuItem {
    enum Kind { case primary, normal, destructive }
    private let handler: () -> Void
    init(_ title: String, kind: Kind = .normal, handler: @escaping () -> Void) {
        self.handler = handler
        super.init(focusColor: kind == .primary ? .white : Style.gold)
        let plate = UIView(); plate.isUserInteractionEnabled = false; plate.layer.cornerRadius = 3
        plate.backgroundColor = kind == .primary ? Style.gold : UIColor.white.withAlphaComponent(0.12)
        plate.layer.borderWidth = kind == .primary ? 0 : 1; plate.layer.borderColor = UIColor.white.withAlphaComponent(0.25).cgColor
        let label = UILabel(); label.text = title; label.font = Style.font(17); label.textAlignment = .center
        label.textColor = kind == .primary ? Style.ink : kind == .destructive ? UIColor(red: 1, green: 0.45, blue: 0.4, alpha: 1) : .white
        for view in [plate, label] as [UIView] { view.translatesAutoresizingMaskIntoConstraints = false; addSubview(view) }
        NSLayoutConstraint.activate([
            heightAnchor.constraint(equalToConstant: 48),
            plate.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 4), plate.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -4),
            plate.topAnchor.constraint(equalTo: topAnchor, constant: 4), plate.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -4),
            label.centerXAnchor.constraint(equalTo: centerXAnchor), label.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
        isAccessibilityElement = true; accessibilityLabel = title.capitalized; accessibilityTraits = .button
        addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(tapped)))
    }
    required init?(coder: NSCoder) { fatalError() }
    @objc private func tapped() { activate() }
    override func activate() { handler() }
}

final class BindingRow: MenuItem {
    let action: Action
    private let value = UILabel()
    private let handler: () -> Void
    private let text: String
    var capturing = false { didSet { refresh() } }

    init(_ action: Action, value text: String, handler: @escaping () -> Void) {
        self.action = action; self.text = text; self.handler = handler
        super.init()
        let label = UILabel(); label.font = Style.font(15); label.textColor = .white
        label.text = [action.title, action.subtitle].compactMap { $0 }.joined(separator: " / ")
        value.font = Style.font(15); value.textAlignment = .center; value.layer.cornerRadius = 3; value.clipsToBounds = true
        for view in [label, value] as [UIView] { view.translatesAutoresizingMaskIntoConstraints = false; addSubview(view) }
        NSLayoutConstraint.activate([
            heightAnchor.constraint(equalToConstant: 44),
            label.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 10), label.centerYAnchor.constraint(equalTo: centerYAnchor),
            value.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -6), value.centerYAnchor.constraint(equalTo: centerYAnchor),
            value.heightAnchor.constraint(equalToConstant: 32), value.widthAnchor.constraint(equalToConstant: 228)
        ])
        addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(tapped)))
        refresh()
    }
    required init?(coder: NSCoder) { fatalError() }
    private func refresh() {
        value.text = capturing ? "PRESS A BUTTON…" : text
        value.backgroundColor = capturing ? Style.gold : UIColor.white.withAlphaComponent(0.1)
        value.textColor = capturing ? Style.ink : .white
    }
    @objc private func tapped() { activate() }
    override func activate() { handler() }
}

final class PauseMenuView: UIView {
    enum Tab: Int, CaseIterable {
        case game, display, touch, gamepad
        var title: String { ["GAME", "DISPLAY", "TOUCH", "GAMEPAD"][rawValue] }
    }
    var onResume: (() -> Void)?
    var onTutorial: (() -> Void)?
    var onRestart: (() -> Void)?
    var onCustomize: (() -> Void)?
    var onResetLayout: (() -> Void)?
    var onResetBindings: (() -> Void)?
    var onSettingsChanged: (() -> Void)?
    var onCaptureBinding: ((Action) -> Void)?
    var bindingText: ((Action) -> String)?
    var gamepadName: String? { didSet { if tab == .gamepad { reload() } else { updateHint() } } }
    private(set) var tab: Tab = .game
    private let settings = GameSettings.shared
    private var items: [MenuItem] = []
    private var focusIndex = 0
    private var showsFocus = false
    private let scroll = UIScrollView(), stack = UIStackView(), tabStack = UIStackView(), hint = UILabel()
    private var tabButtons: [UIButton] = []

    init(tab: Tab) {
        super.init(frame: .zero)
        let blur = UIVisualEffectView(effect: UIBlurEffect(style: .systemThinMaterialDark))
        let shade = UIView(); shade.backgroundColor = UIColor.black.withAlphaComponent(0.35)
        let top = BarView(), bottom = BarView()
        let back = BackControl("RESUME")
        back.addAction(UIAction { [weak self] _ in self?.onResume?() }, for: .touchUpInside)
        let title = UILabel(); title.text = "PAUSED"; title.font = Style.font(22, .black); title.textColor = Style.ink
        let brand = UILabel(); brand.text = "Tony Hawk’s Pro Skater 2"; brand.font = Style.display(17); brand.textColor = Style.goldDeep
        let panel = UIView(); panel.backgroundColor = UIColor.black.withAlphaComponent(0.45)
        panel.layer.borderColor = UIColor.white.withAlphaComponent(0.14).cgColor; panel.layer.borderWidth = 1; panel.layer.cornerRadius = 4
        tabStack.distribution = .fillEqually; tabStack.spacing = 3
        for item in Tab.allCases {
            let button = UIButton(type: .custom)
            button.setTitle(item.title, for: .normal); button.titleLabel?.font = Style.font(16)
            button.addAction(UIAction { [weak self] _ in self?.select(item) }, for: .touchUpInside)
            tabButtons.append(button); tabStack.addArrangedSubview(button)
        }
        stack.axis = .vertical; stack.spacing = 6
        scroll.indicatorStyle = .white; scroll.alwaysBounceVertical = true
        hint.font = Style.font(13, .bold); hint.textColor = Style.ink; hint.textAlignment = .center
        hint.adjustsFontSizeToFitWidth = true; hint.minimumScaleFactor = 0.7
        for view in [blur, shade, top, back, title, brand, tabStack, panel, bottom, hint] as [UIView] {
            view.translatesAutoresizingMaskIntoConstraints = false; addSubview(view)
        }
        for view in [scroll, stack] as [UIView] { view.translatesAutoresizingMaskIntoConstraints = false }
        panel.addSubview(scroll); scroll.addSubview(stack)
        let safe = safeAreaLayoutGuide
        let width = tabStack.widthAnchor.constraint(equalTo: safe.widthAnchor, constant: -40)
        width.priority = .defaultHigh
        NSLayoutConstraint.activate([
            blur.leadingAnchor.constraint(equalTo: leadingAnchor), blur.trailingAnchor.constraint(equalTo: trailingAnchor),
            blur.topAnchor.constraint(equalTo: topAnchor), blur.bottomAnchor.constraint(equalTo: bottomAnchor),
            shade.leadingAnchor.constraint(equalTo: leadingAnchor), shade.trailingAnchor.constraint(equalTo: trailingAnchor),
            shade.topAnchor.constraint(equalTo: topAnchor), shade.bottomAnchor.constraint(equalTo: bottomAnchor),
            top.leadingAnchor.constraint(equalTo: leadingAnchor), top.trailingAnchor.constraint(equalTo: trailingAnchor),
            top.topAnchor.constraint(equalTo: topAnchor), top.bottomAnchor.constraint(equalTo: safe.topAnchor, constant: 46),
            back.leadingAnchor.constraint(equalTo: safe.leadingAnchor, constant: 12), back.bottomAnchor.constraint(equalTo: top.bottomAnchor, constant: -6),
            back.heightAnchor.constraint(equalToConstant: 34),
            title.centerXAnchor.constraint(equalTo: centerXAnchor), title.centerYAnchor.constraint(equalTo: back.centerYAnchor),
            brand.trailingAnchor.constraint(equalTo: safe.trailingAnchor, constant: -16), brand.centerYAnchor.constraint(equalTo: back.centerYAnchor),
            tabStack.topAnchor.constraint(equalTo: top.bottomAnchor, constant: 12), tabStack.centerXAnchor.constraint(equalTo: centerXAnchor),
            tabStack.widthAnchor.constraint(lessThanOrEqualToConstant: 640), width, tabStack.heightAnchor.constraint(equalToConstant: 34),
            panel.topAnchor.constraint(equalTo: tabStack.bottomAnchor, constant: 8),
            panel.leadingAnchor.constraint(equalTo: tabStack.leadingAnchor), panel.trailingAnchor.constraint(equalTo: tabStack.trailingAnchor),
            panel.bottomAnchor.constraint(equalTo: bottom.topAnchor, constant: -8),
            scroll.leadingAnchor.constraint(equalTo: panel.leadingAnchor), scroll.trailingAnchor.constraint(equalTo: panel.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: panel.topAnchor), scroll.bottomAnchor.constraint(equalTo: panel.bottomAnchor),
            stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 10),
            stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -10),
            stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 10),
            stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -10),
            stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -20),
            bottom.leadingAnchor.constraint(equalTo: leadingAnchor), bottom.trailingAnchor.constraint(equalTo: trailingAnchor),
            bottom.bottomAnchor.constraint(equalTo: bottomAnchor), bottom.topAnchor.constraint(equalTo: safe.bottomAnchor, constant: -30),
            hint.topAnchor.constraint(equalTo: bottom.topAnchor), hint.heightAnchor.constraint(equalToConstant: 30),
            hint.leadingAnchor.constraint(equalTo: safe.leadingAnchor, constant: 16), hint.trailingAnchor.constraint(equalTo: safe.trailingAnchor, constant: -16)
        ])
        select(tab)
    }
    required init?(coder: NSCoder) { fatalError() }

    func select(_ newTab: Tab) {
        tab = newTab; focusIndex = 0
        for (index, button) in tabButtons.enumerated() {
            let on = index == newTab.rawValue
            button.backgroundColor = on ? Style.gold : UIColor(white: 0.93, alpha: 1)
            button.setTitleColor(Style.ink, for: .normal)
            button.accessibilityTraits = on ? [.button, .selected] : .button
        }
        reload()
        scroll.setContentOffset(.zero, animated: false)
    }

    func reload() {
        stack.arrangedSubviews.forEach { $0.removeFromSuperview() }
        items = []
        let changed: () -> Void = { [weak self] in self?.onSettingsChanged?() }
        switch tab {
        case .game:
            add(ActionRow("RESUME", kind: .primary) { [weak self] in self?.onResume?() })
            add(ChoiceRow("CRT FILTER", ["OFF", "ON"], selected: settings.crtMode) { [weak self] in
                self?.settings.crtMode = $0; changed()
            })
            add(note("CRT FILTER uses the NewPixie shader."))
            add(ActionRow("CONTROLLER TUTORIAL") { [weak self] in self?.onTutorial?() })
            add(ActionRow("RESTART GAME", kind: .destructive) { [weak self] in self?.onRestart?() })
            add(note("Dreamcast. 2 player slots. Game files are supplied separately."))
        case .display:
            add(ChoiceRow("WIDESCREEN", ["ON", "OFF"], selected: settings.gameWidescreen ? 0 : 1) { [weak self] in
                self?.settings.gameWidescreen = $0 == 0; changed()
            })
            add(ChoiceRow("COLOR SIDES", ["ON", "OFF"], selected: settings.colorSides ? 0 : 1) { [weak self] in
                self?.settings.colorSides = $0 == 0; changed()
            })
            add(ChoiceRow("SCREEN", ["FULL", "SQUARE", "ARCADE"], selected: settings.screen.rawValue) { [weak self] in
                self?.settings.screen = GameSettings.Screen(rawValue: $0) ?? .arcade; changed()
            })
            add(ChoiceRow("PIXELS", ["SHARP", "SMOOTH"], selected: settings.smoothPixels ? 1 : 0) { [weak self] in
                self?.settings.smoothPixels = $0 == 1; changed()
            })
            add(note("WIDESCREEN shows more of each level, with nothing stretched. COLOR SIDES fills unused space. ARCADE keeps the original proportions; FULL stretches to fill the display."))
        case .touch:
            add(ChoiceRow("TOUCH CONTROLS", ["AUTO", "ON", "OFF"], selected: settings.touchMode.rawValue) { [weak self] in
                self?.settings.touchMode = GameSettings.TouchMode(rawValue: $0) ?? .auto; changed()
            })
            add(ChoiceRow("STICK", ["FLOATING", "FIXED"], selected: settings.floatingStick ? 0 : 1) { [weak self] in
                self?.settings.floatingStick = $0 == 0; changed()
            })
            add(ChoiceRow("BUTTON SIZE", ["S", "M", "L"], selected: settings.buttonSize) { [weak self] in
                self?.settings.buttonSize = $0; changed()
            })
            add(ChoiceRow("OPACITY", ["LOW", "MID", "HIGH"], selected: settings.opacity) { [weak self] in
                self?.settings.opacity = $0; changed()
            })
            add(ChoiceRow("VIBRATION", ["ON", "OFF"], selected: settings.haptics ? 0 : 1) { [weak self] in
                self?.settings.haptics = $0 == 0; changed()
            })
            for id in [ControlID.spin, .dunk, .shove] {
                add(ChoiceRow("\(id.name) BUTTON", ["SHOW", "HIDE"], selected: settings.hiddenControls.contains(id) ? 1 : 0) { [weak self] in
                    guard let self else { return }
                    var hidden = self.settings.hiddenControls
                    if $0 == 1 { hidden.insert(id) } else { hidden.remove(id) }
                    self.settings.hiddenControls = hidden; changed()
                })
            }
            add(ActionRow("CUSTOMIZE LAYOUT", kind: .primary) { [weak self] in self?.onCustomize?() })
            add(ActionRow("RESET LAYOUT") { [weak self] in self?.onResetLayout?(); self?.reload() })
            add(note("AUTO hides the touch controls while you play with a gamepad; touch the screen to bring them back."))
        case .gamepad:
            add(note(gamepadName.map { "CONNECTED: \($0.uppercased())" } ?? "No controller connected. Pair one in iOS Settings › Bluetooth (Xbox, PlayStation, MFi, Switch Pro)."))
            for action in [Action.shoot, .pass, .turbo, .spin, .dunk, .shove] {
                add(BindingRow(action, value: bindingText?(action) ?? "—") { [weak self] in self?.onCaptureBinding?(action) })
            }
            let menu = bindingText?(.start) ?? "MENU"
            add(note("MOVE: left stick or D-pad.  START: tap \(menu).  PAUSE: hold \(menu) or press Home."))
            add(ActionRow("RESET GAMEPAD BUTTONS") { [weak self] in self?.onResetBindings?(); self?.reload() })
        }
        focusIndex = min(focusIndex, max(items.count - 1, 0))
        refreshFocus(scroll: false)
        updateHint()
    }

    func setCapturing(_ action: Action?) {
        for case let row as BindingRow in items { row.capturing = row.action == action }
    }

    func handle(_ event: NavEvent) {
        if !showsFocus {
            showsFocus = true; refreshFocus(scroll: true)
            if event == .up || event == .down { return }
        }
        switch event {
        case .up: move(-1)
        case .down: move(1)
        case .left: current?.adjust(-1)
        case .right: current?.adjust(1)
        case .accept: current?.activate()
        case .back, .start, .pause: onResume?()
        case .previousTab: select(Tab(rawValue: (tab.rawValue + Tab.allCases.count - 1) % Tab.allCases.count)!); refreshFocus(scroll: true)
        case .nextTab: select(Tab(rawValue: (tab.rawValue + 1) % Tab.allCases.count)!); refreshFocus(scroll: true)
        }
    }

    private var current: MenuItem? { items.indices.contains(focusIndex) ? items[focusIndex] : nil }
    private func add(_ view: UIView) {
        stack.addArrangedSubview(view)
        if let item = view as? MenuItem { items.append(item) }
    }
    private func note(_ text: String) -> UILabel {
        let label = UILabel(); label.text = text; label.numberOfLines = 0
        label.font = .systemFont(ofSize: 13, weight: .medium); label.textColor = UIColor.white.withAlphaComponent(0.72)
        return label
    }
    private func move(_ delta: Int) {
        guard !items.isEmpty else { return }
        focusIndex = min(max(focusIndex + delta, 0), items.count - 1)
        refreshFocus(scroll: true)
    }
    private func refreshFocus(scroll animated: Bool) {
        for (index, item) in items.enumerated() { item.isCurrent = showsFocus && index == focusIndex }
        guard animated, showsFocus, items.indices.contains(focusIndex) else { return }
        layoutIfNeeded()
        let item = items[focusIndex]
        scroll.scrollRectToVisible(item.convert(item.bounds, to: scroll).insetBy(dx: 0, dy: -12), animated: true)
    }
    private func updateHint() {
        if gamepadName != nil {
            hint.text = "A  SELECT     B  RESUME     ◀ ▶  CHANGE     LB / RB  TABS"
            return
        }
        switch tab {
        case .game: hint.text = "TIP: REMAP ANY ACTION IN THE GAMEPAD TAB"
        case .display: hint.text = "TIP: ARCADE KEEPS THE ORIGINAL PROPORTIONS, FULL FILLS THE SCREEN"
        case .touch: hint.text = "TIP: DRAG AND PINCH ANY CONTROL IN CUSTOMIZE LAYOUT"
        case .gamepad: hint.text = "TIP: TAP A ROW, THEN PRESS THE CONTROLLER BUTTON YOU WANT"
        }
    }
}

/// "CONTROLLER TUTORIAL" overlay: spotlights each control with an explanation card.
final class TutorialView: UIView {
    struct Step { let tab: Int; let spots: [Spot]; let title: String; let touch: String; let pad: String }
    static let tabs = ["MOVEMENT", "ACTIONS", "SYSTEM"]
    static let steps: [Step] = [
        Step(tab: 0, spots: [.control(.stick)], title: "MOVE", touch: "Use the floating stick to move.", pad: "Use the D-pad or left stick to move."),
        Step(tab: 1, spots: [.control(.shoot)], title: "OLLIE", touch: "Press OLLIE on the touch controls.", pad: "Press {shoot} for OLLIE."),
        Step(tab: 1, spots: [.control(.pass)], title: "FLIP", touch: "Press FLIP on the touch controls.", pad: "Press {pass} for FLIP."),
        Step(tab: 1, spots: [.control(.turbo)], title: "GRIND", touch: "Press GRIND on the touch controls.", pad: "Press {turbo} for GRIND."),
        Step(tab: 1, spots: [.control(.spin)], title: "GRAB", touch: "Press GRAB on the touch controls.", pad: "Press {spin} for GRAB."),
        Step(tab: 1, spots: [.control(.dunk)], title: "LEFT TRIGGER", touch: "Press LEFT TRIGGER on the touch controls.", pad: "Press {dunk} for LEFT TRIGGER."),
        Step(tab: 1, spots: [.control(.shove)], title: "RIGHT TRIGGER", touch: "Press RIGHT TRIGGER on the touch controls.", pad: "Press {shove} for RIGHT TRIGGER."),
        Step(tab: 2, spots: [.control(.start)], title: "START", touch: "Tap START to join.", pad: "Tap {start} to join."),
        Step(tab: 2, spots: [.control(.start)], title: "PAUSE", touch: "Tap the pause button for settings.", pad: "Hold Menu or press Home to pause.")
    ]

    var spotFrame: ((Spot) -> CGRect?)?
    var padLabel: ((Action) -> String)?
    var gamepadMode = false { didSet { setNeedsLayout() } }
    var onClose: (() -> Void)?
    private var index = 0
    private let shade = CAShapeLayer(), ring = CAShapeLayer()
    private let header = BarView(), headerTitle = UILabel(), counter = UILabel(), skip = BackControl("SKIP")
    private let tabStack = UIStackView()
    private var tabButtons: [UIButton] = []
    private let card = BarView(), cardTitle = UILabel(), cardText = UILabel()
    private let previousButton = UIButton(type: .custom), nextButton = UIButton(type: .custom)

    override init(frame: CGRect) {
        super.init(frame: frame)
        shade.fillRule = .evenOdd; shade.fillColor = UIColor.black.withAlphaComponent(0.55).cgColor
        ring.fillColor = nil; ring.strokeColor = Style.gold.cgColor; ring.lineWidth = 3
        ring.shadowColor = Style.gold.cgColor; ring.shadowRadius = 8; ring.shadowOpacity = 1; ring.shadowOffset = .zero
        layer.addSublayer(shade); layer.addSublayer(ring)
        headerTitle.text = "CONTROLLER TUTORIAL"; headerTitle.font = Style.font(19, .black); headerTitle.textColor = Style.ink
        headerTitle.textAlignment = .center; headerTitle.adjustsFontSizeToFitWidth = true; headerTitle.minimumScaleFactor = 0.6
        counter.font = Style.font(15); counter.textColor = UIColor(white: 0.35, alpha: 1); counter.textAlignment = .right
        skip.addAction(UIAction { [weak self] _ in self?.close() }, for: .touchUpInside)
        for view in [skip, headerTitle, counter] as [UIView] { header.addSubview(view) }
        tabStack.distribution = .fillEqually; tabStack.spacing = 3
        for (tab, title) in Self.tabs.enumerated() {
            let button = UIButton(type: .custom)
            button.setTitle(title, for: .normal); button.titleLabel?.font = Style.font(15); button.setTitleColor(Style.ink, for: .normal)
            button.addAction(UIAction { [weak self] _ in self?.jump(to: tab) }, for: .touchUpInside)
            tabButtons.append(button); tabStack.addArrangedSubview(button)
        }
        cardTitle.font = Style.font(20, .black); cardTitle.textColor = Style.ink; cardTitle.textAlignment = .center
        cardText.font = .systemFont(ofSize: 13, weight: .semibold); cardText.textColor = UIColor(white: 0.22, alpha: 1)
        cardText.numberOfLines = 4; cardText.textAlignment = .center; cardText.adjustsFontSizeToFitWidth = true; cardText.minimumScaleFactor = 0.75
        for (button, symbol) in [(previousButton, "chevron.left"), (nextButton, "chevron.right")] {
            button.backgroundColor = Style.ink; button.tintColor = .white
            button.setImage(UIImage(systemName: symbol, withConfiguration: UIImage.SymbolConfiguration(weight: .heavy)), for: .normal)
            card.addSubview(button)
        }
        previousButton.addAction(UIAction { [weak self] _ in self?.go(-1) }, for: .touchUpInside)
        nextButton.addAction(UIAction { [weak self] _ in self?.go(1) }, for: .touchUpInside)
        previousButton.accessibilityLabel = "Previous"; nextButton.accessibilityLabel = "Next"
        card.addSubview(cardTitle); card.addSubview(cardText)
        addSubview(header); addSubview(tabStack); addSubview(card)
    }
    required init?(coder: NSCoder) { fatalError() }

    override func layoutSubviews() {
        super.layoutSubviews()
        let u = min(max(bounds.height / 414, 0.85), 1.25), safe = safeAreaInsets
        let width = min(520 * u, bounds.width - 2 * max(safe.left, 16) - 40)
        header.frame = CGRect(x: bounds.midX - width / 2, y: max(safe.top, 10) + 50 * u, width: width, height: 38 * u)
        skip.frame = CGRect(x: 4 * u, y: 5 * u, width: 84 * u, height: 28 * u)
        counter.frame = CGRect(x: width - 62 * u, y: 0, width: 54 * u, height: header.bounds.height)
        headerTitle.frame = CGRect(x: 96 * u, y: 0, width: width - 166 * u, height: header.bounds.height)
        tabStack.frame = CGRect(x: header.frame.minX, y: header.frame.maxY + 5 * u, width: width, height: 30 * u)
        refresh(u: u)
    }

    private func refresh(u: CGFloat) {
        let step = Self.steps[index]
        counter.text = "\(index + 1)/\(Self.steps.count)"
        for (tab, button) in tabButtons.enumerated() { button.backgroundColor = tab == step.tab ? Style.gold : UIColor(white: 0.93, alpha: 1) }
        cardTitle.text = step.title
        var text = gamepadMode ? step.pad : step.touch
        for action in Action.allCases { text = text.replacingOccurrences(of: "{\(action.rawValue)}", with: padLabel?(action) ?? action.title) }
        cardText.text = text
        nextButton.setImage(UIImage(systemName: index == Self.steps.count - 1 ? "checkmark" : "chevron.right",
                                    withConfiguration: UIImage.SymbolConfiguration(weight: .heavy)), for: .normal)
        previousButton.alpha = index == 0 ? 0.35 : 1

        let target = step.spots.compactMap { spotFrame?($0) }.first
        let hole = target.map { $0.insetBy(dx: -10, dy: -10) }
        let path = UIBezierPath(rect: bounds)
        if let hole { path.append(UIBezierPath(roundedRect: hole, cornerRadius: min(hole.width, hole.height) / 2)) }
        shade.frame = bounds; shade.path = path.cgPath
        ring.isHidden = hole == nil
        if let hole {
            CATransaction.begin(); CATransaction.setDisableActions(true)
            ring.frame = hole
            ring.path = UIBezierPath(roundedRect: CGRect(origin: .zero, size: hole.size), cornerRadius: min(hole.width, hole.height) / 2).cgPath
            CATransaction.commit()
            if ring.animation(forKey: "pulse") == nil {
                let scale = CABasicAnimation(keyPath: "transform.scale"); scale.fromValue = 1; scale.toValue = 1.12
                let fade = CABasicAnimation(keyPath: "opacity"); fade.fromValue = 1; fade.toValue = 0.3
                let group = CAAnimationGroup(); group.animations = [scale, fade]
                group.duration = 0.8; group.autoreverses = true; group.repeatCount = .infinity
                ring.add(group, forKey: "pulse")
            }
        }

        let cardHeight = 104 * u
        card.frame = placeCard(maxWidth: min(460 * u, bounds.width * 0.54), height: cardHeight, avoiding: hole, u: u)
        let cardWidth = card.frame.width, arrow = 38 * u, inset = 8 * u
        previousButton.frame = CGRect(x: inset, y: inset, width: arrow, height: cardHeight - 2 * inset)
        nextButton.frame = CGRect(x: cardWidth - inset - arrow, y: inset, width: arrow, height: cardHeight - 2 * inset)
        let textX = inset * 2 + arrow, textWidth = cardWidth - 2 * textX
        cardTitle.font = Style.font(20 * u, .black)
        cardTitle.frame = CGRect(x: textX, y: 8 * u, width: textWidth, height: 26 * u)
        cardText.font = .systemFont(ofSize: 13 * u, weight: .semibold)
        cardText.frame = CGRect(x: textX, y: 34 * u, width: textWidth, height: cardHeight - 40 * u)
    }

    /// Widest free horizontal gap between the touch controls: bottom band first, then the middle, then the top.
    private func placeCard(maxWidth: CGFloat, height: CGFloat, avoiding hole: CGRect?, u: CGFloat) -> CGRect {
        let obstacles = ControlID.available.compactMap { spotFrame?(.control($0)) } + [hole].compactMap { $0 }
        let margin = max(safeAreaInsets.left, safeAreaInsets.right, 16)
        let bands = [bounds.height - max(safeAreaInsets.bottom, 10) - height - 6 * u, bounds.midY - height / 2 + 12 * u, tabStack.frame.maxY + 10 * u]
        for y in bands {
            let band = CGRect(x: 0, y: y, width: bounds.width, height: height)
            let blockers = obstacles.map { $0.insetBy(dx: -8, dy: -8) }.filter { $0.intersects(band) }.sorted { $0.minX < $1.minX }
            var gaps: [(CGFloat, CGFloat)] = [], x = margin
            for blocker in blockers {
                if blocker.minX > x { gaps.append((x, blocker.minX)) }
                x = max(x, blocker.maxX)
            }
            if bounds.width - margin > x { gaps.append((x, bounds.width - margin)) }
            if let gap = gaps.max(by: { $0.1 - $0.0 < $1.1 - $1.0 }), gap.1 - gap.0 >= 300 * u {
                let width = min(maxWidth, gap.1 - gap.0)
                return CGRect(x: (gap.0 + gap.1 - width) / 2, y: y, width: width, height: height)
            }
        }
        return CGRect(x: bounds.midX - maxWidth / 2, y: bands[1], width: maxWidth, height: height)
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        let hit = super.hitTest(point, with: event)
        return hit === self ? nil : hit
    }

    func handle(_ event: NavEvent) {
        switch event {
        case .accept, .right: go(1)
        case .left: go(-1)
        case .back, .start, .pause: close()
        case .previousTab: jump(to: max(Self.steps[index].tab - 1, 0))
        case .nextTab: jump(to: min(Self.steps[index].tab + 1, Self.tabs.count - 1))
        case .up, .down: break
        }
    }
    private func go(_ delta: Int) {
        if index + delta >= Self.steps.count { close(); return }
        index = max(index + delta, 0); setNeedsLayout()
    }
    private func jump(to tab: Int) {
        if let first = Self.steps.firstIndex(where: { $0.tab == tab }) { index = first; setNeedsLayout() }
    }
    private func close() {
        UIView.animate(withDuration: 0.2, animations: { self.alpha = 0 }) { _ in
            self.removeFromSuperview(); self.onClose?()
        }
    }
}

/// Drag, pinch and hide the touch controls; changes apply live and are saved on SAVE.
final class LayoutEditorView: UIView {
    var onFinish: ((Bool) -> Void)?
    private let controls: ControlsView
    private let savedPlacements: [ControlID: Placement]
    private let savedHidden: Set<ControlID>
    private var selected: ControlID = .shoot
    private var drag: (id: ControlID, offset: CGPoint, touch: UITouch)?
    private var pinchStart: CGFloat = 1
    private let outlines = CAShapeLayer(), highlight = CAShapeLayer()
    private let panel = UIView(), titleLabel = UILabel(), helpLabel = UILabel(), nameLabel = UILabel()
    private let minus = UIButton(type: .custom), plus = UIButton(type: .custom), hide = UIButton(type: .custom)
    private let reset = UIButton(type: .custom), cancel = UIButton(type: .custom), save = UIButton(type: .custom)

    init(controls: ControlsView) {
        self.controls = controls
        savedPlacements = controls.placements; savedHidden = controls.hiddenControls
        super.init(frame: .zero)
        backgroundColor = UIColor.black.withAlphaComponent(0.2)
        isMultipleTouchEnabled = true
        outlines.fillColor = nil; outlines.strokeColor = UIColor.white.withAlphaComponent(0.65).cgColor
        outlines.lineWidth = 1.5; outlines.lineDashPattern = [6, 5]
        highlight.fillColor = Style.gold.withAlphaComponent(0.12).cgColor; highlight.strokeColor = Style.gold.cgColor; highlight.lineWidth = 3
        layer.addSublayer(outlines); layer.addSublayer(highlight)
        panel.backgroundColor = UIColor(white: 0.05, alpha: 0.86); panel.layer.cornerRadius = 8
        panel.layer.borderColor = Style.gold.withAlphaComponent(0.6).cgColor; panel.layer.borderWidth = 1
        titleLabel.text = "CUSTOMIZE LAYOUT"; titleLabel.textColor = Style.gold; titleLabel.textAlignment = .center
        helpLabel.text = "DRAG TO MOVE  ·  PINCH OR − / + TO RESIZE"; helpLabel.textColor = UIColor.white.withAlphaComponent(0.7)
        helpLabel.textAlignment = .center; helpLabel.adjustsFontSizeToFitWidth = true; helpLabel.minimumScaleFactor = 0.7
        nameLabel.textColor = .white; nameLabel.textAlignment = .center
        for view in [titleLabel, helpLabel, nameLabel] as [UIView] { panel.addSubview(view) }
        let buttons: [(UIButton, String, Bool, () -> Void)] = [
            (minus, "−", false, { [weak self] in self?.resizeSelected(by: -0.1) }),
            (plus, "+", false, { [weak self] in self?.resizeSelected(by: 0.1) }),
            (hide, "HIDE", false, { [weak self] in self?.toggleHidden() }),
            (reset, "RESET", false, { [weak self] in self?.resetAll() }),
            (cancel, "CANCEL", false, { [weak self] in self?.finish(save: false) }),
            (save, "SAVE", true, { [weak self] in self?.finish(save: true) })
        ]
        for (button, title, primary, action) in buttons {
            button.setTitle(title, for: .normal); button.layer.cornerRadius = 4
            button.backgroundColor = primary ? Style.gold : UIColor.white.withAlphaComponent(0.14)
            button.setTitleColor(primary ? Style.ink : .white, for: .normal)
            button.setTitleColor(UIColor.white.withAlphaComponent(0.3), for: .disabled)
            button.addAction(UIAction { _ in action() }, for: .touchUpInside)
            panel.addSubview(button)
        }
        addSubview(panel)
        addGestureRecognizer(UIPinchGestureRecognizer(target: self, action: #selector(pinched(_:))))
    }
    required init?(coder: NSCoder) { fatalError() }

    override func layoutSubviews() {
        super.layoutSubviews()
        let u = min(max(bounds.height / 414, 0.85), 1.25)
        let width = 300 * u, height = 172 * u
        panel.frame = CGRect(x: bounds.midX - width / 2, y: bounds.midY - height / 2 - 18 * u, width: width, height: height)
        titleLabel.font = Style.font(19 * u, .black); titleLabel.frame = CGRect(x: 0, y: 8 * u, width: width, height: 24 * u)
        helpLabel.font = Style.font(12 * u, .bold); helpLabel.frame = CGRect(x: 10 * u, y: 32 * u, width: width - 20 * u, height: 16 * u)
        nameLabel.font = Style.font(16 * u); nameLabel.frame = CGRect(x: 0, y: 52 * u, width: width, height: 22 * u)
        let small = 44 * u, rowY = 78 * u, rowHeight = 34 * u
        let rowWidth = small * 2 + 96 * u + 16 * u
        minus.frame = CGRect(x: (width - rowWidth) / 2, y: rowY, width: small, height: rowHeight)
        plus.frame = CGRect(x: minus.frame.maxX + 8 * u, y: rowY, width: small, height: rowHeight)
        hide.frame = CGRect(x: plus.frame.maxX + 8 * u, y: rowY, width: 96 * u, height: rowHeight)
        let third = (width - 36 * u) / 3
        for (i, button) in [reset, cancel, save].enumerated() {
            button.frame = CGRect(x: 10 * u + CGFloat(i) * (third + 8 * u), y: 124 * u, width: third, height: 38 * u)
        }
        for button in [minus, plus, hide, reset, cancel, save] { button.titleLabel?.font = Style.font((button === minus || button === plus ? 22 : 16) * u) }
        refresh()
    }

    private func refresh() {
        let editable = ControlID.available
        let outlinePath = UIBezierPath()
        for id in editable where id != selected { outlinePath.append(shape(controls.frame(of: id), id)) }
        CATransaction.begin(); CATransaction.setDisableActions(true)
        outlines.path = outlinePath.cgPath
        highlight.path = shape(controls.frame(of: selected), selected).cgPath
        CATransaction.commit()
        let percent = Int(((controls.placements[selected]?.scale ?? 1) * 100).rounded())
        let hidden = controls.hiddenControls.contains(selected)
        nameLabel.text = "\(selected.name) · \(percent)%\(hidden ? " · HIDDEN" : "")"
        hide.setTitle(hidden ? "SHOW" : "HIDE", for: .normal)
        hide.isEnabled = selected.canHide
    }
    private func shape(_ frame: CGRect, _ id: ControlID) -> UIBezierPath {
        let r = frame.insetBy(dx: -5, dy: -5)
        return UIBezierPath(roundedRect: r, cornerRadius: id.isPill ? r.height / 2 : r.width / 2)
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        for button in [minus, plus, hide, reset, cancel, save] where button.isEnabled && panel.alpha > 0.9 {
            if button.bounds.insetBy(dx: -4, dy: -4).contains(button.convert(point, from: self)) { return button }
        }
        return bounds.contains(point) ? self : nil
    }
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard drag == nil, let touch = touches.first, let id = control(at: touch.location(in: self)) else { return }
        let p = touch.location(in: self), f = controls.frame(of: id)
        selected = id
        drag = (id, CGPoint(x: f.midX - p.x, y: f.midY - p.y), touch)
        UIView.animate(withDuration: 0.15) { self.panel.alpha = 0.3 }
        refresh()
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let drag, touches.contains(drag.touch) else { return }
        let p = drag.touch.location(in: self)
        move(drag.id, to: CGPoint(x: p.x + drag.offset.x, y: p.y + drag.offset.y), scale: controls.placements[drag.id]?.scale ?? 1)
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { endDrag(touches) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { endDrag(touches) }
    private func endDrag(_ touches: Set<UITouch>) {
        guard let drag, touches.contains(drag.touch) else { return }
        self.drag = nil
        UIView.animate(withDuration: 0.15) { self.panel.alpha = 1 }
    }

    @objc private func pinched(_ gesture: UIPinchGestureRecognizer) {
        switch gesture.state {
        case .began:
            if let id = control(at: gesture.location(in: self)) { selected = id }
            pinchStart = controls.placements[selected]?.scale ?? 1
        case .changed:
            let f = controls.frame(of: selected)
            move(selected, to: CGPoint(x: f.midX, y: f.midY), scale: min(max(pinchStart * gesture.scale, 0.6), 1.6))
        default: break
        }
    }

    func handle(_ event: NavEvent) {
        switch event {
        case .back: finish(save: false)
        case .start, .pause: finish(save: true)
        default: break
        }
    }

    private func control(at point: CGPoint) -> ControlID? {
        let hits = ControlID.available.filter { controls.frame(of: $0).insetBy(dx: -8, dy: -8).contains(point) }
        return hits.min { a, b in
            let fa = controls.frame(of: a), fb = controls.frame(of: b)
            return hypot(fa.midX - point.x, fa.midY - point.y) < hypot(fb.midX - point.x, fb.midY - point.y)
        }
    }
    private func move(_ id: ControlID, to center: CGPoint, scale: CGFloat) {
        controls.place(id, center: center, scale: scale)
        let a = controls.layoutArea, s = controls.size(of: id)
        let clamped = CGPoint(x: min(max(center.x, a.minX + s.width / 2), a.maxX - s.width / 2),
                              y: min(max(center.y, a.minY + s.height / 2), a.maxY - s.height / 2))
        controls.place(id, center: clamped, scale: scale)
        controls.layoutIfNeeded(); refresh()
    }
    private func resizeSelected(by delta: CGFloat) {
        let f = controls.frame(of: selected), scale = controls.placements[selected]?.scale ?? 1
        move(selected, to: CGPoint(x: f.midX, y: f.midY), scale: min(max(((scale + delta) * 10).rounded() / 10, 0.6), 1.6))
    }
    private func toggleHidden() {
        guard selected.canHide else { return }
        if controls.hiddenControls.contains(selected) { controls.hiddenControls.remove(selected) } else { controls.hiddenControls.insert(selected) }
        refresh()
    }
    private func resetAll() {
        controls.placements = [:]; controls.hiddenControls = GameSettings.defaultHidden
        controls.layoutIfNeeded(); refresh()
    }
    private func finish(save: Bool) {
        if save {
            GameSettings.shared.layout = controls.placements
            GameSettings.shared.hiddenControls = controls.hiddenControls
        } else {
            controls.placements = savedPlacements; controls.hiddenControls = savedHidden
        }
        removeFromSuperview()
        onFinish?(save)
    }
}

final class ToastView: UILabel {
    private var token = 0
    override init(frame: CGRect) {
        super.init(frame: frame)
        font = Style.font(15); textColor = .white; textAlignment = .center
        backgroundColor = UIColor.black.withAlphaComponent(0.78)
        layer.borderColor = Style.gold.cgColor; layer.borderWidth = 1.5; layer.cornerRadius = 17; clipsToBounds = true
        alpha = 0; isUserInteractionEnabled = false
    }
    required init?(coder: NSCoder) { fatalError() }

    func show(_ message: String, duration: TimeInterval = 2.6) {
        guard let parent = superview else { return }
        text = message
        let width = min(intrinsicContentSize.width + 40, parent.bounds.width - 40)
        let top = max(parent.safeAreaInsets.top, 10) + 52 * min(max(parent.bounds.height / 414, 0.85), 1.25)
        frame = CGRect(x: parent.bounds.midX - width / 2, y: top, width: width, height: 34)
        parent.bringSubviewToFront(self)
        token += 1
        let current = token
        UIView.animate(withDuration: 0.2) { self.alpha = 1 }
        DispatchQueue.main.asyncAfter(deadline: .now() + duration) { [weak self] in
            guard let self, self.token == current else { return }
            UIView.animate(withDuration: 0.3) { self.alpha = 0 }
        }
    }
}

/// Shown until the arcade ROM set has been imported.
final class StartScreenView: UIView {
    var onImport: (() -> Void)?
    override class var layerClass: AnyClass { CAGradientLayer.self }

    override init(frame: CGRect) {
        super.init(frame: frame)
        let gradient = layer as! CAGradientLayer
        gradient.colors = [UIColor(red: 0.16, green: 0.06, blue: 0.05, alpha: 1).cgColor, UIColor(red: 0.02, green: 0.02, blue: 0.04, alpha: 1).cgColor]
        gradient.type = .radial; gradient.startPoint = CGPoint(x: 0.5, y: 0.3); gradient.endPoint = CGPoint(x: 1.2, y: 1.3)
        let title = UILabel(); title.text = "Tony Hawk’s Pro Skater 2"; title.font = Style.display(48); title.adjustsFontSizeToFitWidth = true; title.minimumScaleFactor = 0.35; title.textColor = .white
        title.layer.shadowColor = Style.goldDeep.cgColor; title.layer.shadowOpacity = 0.9; title.layer.shadowRadius = 0; title.layer.shadowOffset = CGSize(width: 3, height: 3)
        let edition = UILabel(); edition.text = "Dreamcast"; edition.font = Style.font(16, .black); edition.textColor = Style.gold
        let detail = UILabel(); detail.numberOfLines = 0; detail.textAlignment = .center
        detail.text = "Import the game disc image (thps2.chd) to play. You can also copy it into this app's Documents folder with Finder or the Files app."
        detail.font = .systemFont(ofSize: 14, weight: .medium); detail.textColor = UIColor.white.withAlphaComponent(0.75)
        var config = UIButton.Configuration.filled()
        config.baseBackgroundColor = Style.gold; config.baseForegroundColor = Style.ink
        config.image = UIImage(systemName: "square.and.arrow.down.fill"); config.imagePadding = 8
        config.attributedTitle = AttributedString("IMPORT thps2.chd", attributes: AttributeContainer([.font: Style.font(19)]))
        config.contentInsets = NSDirectionalEdgeInsets(top: 12, leading: 26, bottom: 12, trailing: 26)
        config.cornerStyle = .capsule
        let button = UIButton(configuration: config)
        button.addAction(UIAction { [weak self] _ in self?.onImport?() }, for: .touchUpInside)
        let note = UILabel(); note.text = "ROMs are not included. Scores and settings stay on this device."
        note.font = .systemFont(ofSize: 12, weight: .medium); note.textColor = UIColor.white.withAlphaComponent(0.45)
        let stack = UIStackView(arrangedSubviews: [title, edition, detail, button, note])
        stack.axis = .vertical; stack.alignment = .center; stack.spacing = 12; stack.setCustomSpacing(4, after: title); stack.setCustomSpacing(22, after: detail)
        stack.translatesAutoresizingMaskIntoConstraints = false; addSubview(stack)
        NSLayoutConstraint.activate([
            stack.centerXAnchor.constraint(equalTo: centerXAnchor), stack.centerYAnchor.constraint(equalTo: centerYAnchor),
            stack.widthAnchor.constraint(lessThanOrEqualTo: safeAreaLayoutGuide.widthAnchor, constant: -40),
            detail.widthAnchor.constraint(lessThanOrEqualToConstant: 520)
        ])
    }
    required init?(coder: NSCoder) { fatalError() }
}
