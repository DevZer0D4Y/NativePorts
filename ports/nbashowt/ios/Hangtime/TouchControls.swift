import UIKit

enum Style {
    static let gold = UIColor(red: 0.99, green: 0.74, blue: 0.16, alpha: 1)
    static let goldDeep = UIColor(red: 0.93, green: 0.55, blue: 0.05, alpha: 1)
    static let ink = UIColor(white: 0.11, alpha: 1)
    static func font(_ size: CGFloat, _ weight: UIFont.Weight = .heavy) -> UIFont {
        .systemFont(ofSize: size, weight: weight, width: .condensed)
    }
    static func display(_ size: CGFloat) -> UIFont {
        let base = UIFont.systemFont(ofSize: size, weight: .black, width: .condensed)
        guard let italic = base.fontDescriptor.withSymbolicTraits(base.fontDescriptor.symbolicTraits.union(.traitItalic)) else { return base }
        return UIFont(descriptor: italic, size: size)
    }
}

enum ControlID: String, CaseIterable, Codable {
    case stick, shoot, pass, turbo, spin, dunk, shove, coin, start

    var action: Action? { Action(rawValue: rawValue) }
    var isPill: Bool { self == .coin || self == .start }
    var canHide: Bool { self == .turbo || self == .spin || self == .dunk || self == .shove }
    var name: String { action?.title ?? "MOVE STICK" }
    /// Size at unit scale (an iPhone in landscape is about 414 pt tall).
    var baseSize: CGSize {
        switch self {
        case .stick: return CGSize(width: 164, height: 164)
        case .shoot: return CGSize(width: 98, height: 98)
        case .pass, .turbo: return CGSize(width: 80, height: 80)
        case .spin, .dunk, .shove: return CGSize(width: 64, height: 64)
        case .coin, .start: return CGSize(width: 92, height: 38)
        }
    }
}

/// Something the controller tutorial can point at.
enum Spot { case control(ControlID), pause }

enum Icon {
    static func image(_ id: ControlID, side: CGFloat) -> UIImage? {
        switch id {
        case .shoot: return draw(side, hoop)
        case .pass: return draw(side, pass)
        case .dunk: return draw(side, dunk)
        case .turbo: return symbol("bolt.fill", side)
        case .spin: return symbol("arrow.clockwise", side)
        case .shove: return symbol("hand.raised.fill", side)
        case .coin: return symbol("centsign.circle.fill", side)
        case .start: return symbol("play.fill", side)
        case .stick: return nil
        }
    }
    static func symbol(_ name: String, _ side: CGFloat) -> UIImage? {
        UIImage(systemName: name, withConfiguration: UIImage.SymbolConfiguration(pointSize: side * 0.78, weight: .heavy))
    }
    private static func draw(_ side: CGFloat, _ body: (CGContext) -> Void) -> UIImage {
        UIGraphicsImageRenderer(size: CGSize(width: side, height: side)).image { context in
            let c = context.cgContext
            c.scaleBy(x: side / 32, y: side / 32)
            c.setLineCap(.round); c.setLineJoin(.round)
            UIColor.white.setStroke(); UIColor.white.setFill()
            body(c)
        }.withRenderingMode(.alwaysTemplate)
    }
    private static func lines(_ c: CGContext, _ segments: [(CGFloat, CGFloat, CGFloat, CGFloat)]) {
        for s in segments { c.move(to: CGPoint(x: s.0, y: s.1)); c.addLine(to: CGPoint(x: s.2, y: s.3)) }
        c.strokePath()
    }
    // The drawings below use a 32 x 32 grid.
    private static func hoop(_ c: CGContext) {
        c.setLineWidth(2.4)
        c.addPath(UIBezierPath(roundedRect: CGRect(x: 3, y: 2, width: 26, height: 15), cornerRadius: 2.5).cgPath)
        c.strokePath()
        c.setLineWidth(1.8); c.stroke(CGRect(x: 11.5, y: 8, width: 9, height: 6.5))
        c.setLineWidth(2.6); c.strokeEllipse(in: CGRect(x: 7.5, y: 16, width: 17, height: 4.2))
        c.setLineWidth(1.7)
        lines(c, [(9, 19, 12.2, 29), (23, 19, 19.8, 29), (12.2, 29, 19.8, 29), (9.8, 23.5, 22.2, 23.5), (13, 19.8, 16, 29), (19, 19.8, 16, 29)])
    }
    private static func pass(_ c: CGContext) {
        ball(c, center: CGPoint(x: 12.5, y: 19), radius: 9.5)
        c.setLineWidth(2.4)
        lines(c, [(17, 6, 29, 6), (25, 2.5, 29, 6), (29, 6, 25, 9.5)])
    }
    private static func dunk(_ c: CGContext) {
        ball(c, center: CGPoint(x: 16, y: 8), radius: 6.5)
        c.setLineWidth(2.6); c.strokeEllipse(in: CGRect(x: 5, y: 16, width: 22, height: 5))
        c.setLineWidth(1.7)
        lines(c, [(7, 19.5, 11, 30), (25, 19.5, 21, 30), (11, 30, 21, 30), (8.5, 25, 23.5, 25)])
    }
    private static func ball(_ c: CGContext, center p: CGPoint, radius r: CGFloat) {
        c.setLineWidth(r > 8 ? 2.2 : 1.8)
        c.strokeEllipse(in: CGRect(x: p.x - r, y: p.y - r, width: 2 * r, height: 2 * r))
        c.setLineWidth(r > 8 ? 1.6 : 1.3)
        lines(c, [(p.x, p.y - r, p.x, p.y + r), (p.x - r, p.y, p.x + r, p.y)])
        for side in [-1.0, 1.0] as [CGFloat] {
            c.move(to: CGPoint(x: p.x + side * r * 0.62, y: p.y - r * 0.78))
            c.addQuadCurve(to: CGPoint(x: p.x + side * r * 0.62, y: p.y + r * 0.78), control: CGPoint(x: p.x + side * r * 0.05, y: p.y))
        }
        c.strokePath()
    }
}

/// Round mobile-style action button (or a pill for COIN/START) that stays pressed while touched.
final class ActionButton: UIView {
    let id: ControlID
    var onChange: ((Bool) -> Void)?
    private(set) var isPressed = false
    private var touch: UITouch?
    private let halo = CAShapeLayer(), disc = CAShapeLayer(), shine = CAGradientLayer(), shineMask = CAShapeLayer()
    private let icon = UIImageView(), title = UILabel(), subtitle = UILabel()
    private var iconSide: CGFloat = 0

    init(_ id: ControlID) {
        self.id = id
        super.init(frame: .zero)
        isMultipleTouchEnabled = false
        layer.addSublayer(halo); layer.addSublayer(disc)
        shine.type = .radial
        shine.colors = [UIColor.white.withAlphaComponent(0.3).cgColor, UIColor.white.withAlphaComponent(0).cgColor]
        shine.startPoint = CGPoint(x: 0.5, y: 0.12); shine.endPoint = CGPoint(x: 1.05, y: 0.95)
        shine.mask = shineMask
        layer.addSublayer(shine)
        icon.contentMode = .scaleAspectFit
        title.text = id.action?.title; subtitle.text = id.action?.subtitle
        for view in [icon, title, subtitle] as [UIView] { view.isUserInteractionEnabled = false; addSubview(view) }
        for label in [title, subtitle] { label.textAlignment = .center; label.adjustsFontSizeToFitWidth = true; label.minimumScaleFactor = 0.6 }
        isAccessibilityElement = true; accessibilityTraits = .button
        accessibilityLabel = [id.action?.title, id.action?.subtitle].compactMap { $0 }.joined(separator: " or ").capitalized
        style()
    }
    required init?(coder: NSCoder) { fatalError() }

    override func layoutSubviews() {
        super.layoutSubviews()
        CATransaction.begin(); CATransaction.setDisableActions(true)
        let b = bounds
        shine.frame = b
        var side: CGFloat
        if id.isPill {
            halo.path = nil
            let path = UIBezierPath(roundedRect: b.insetBy(dx: 1, dy: 1), cornerRadius: b.height / 2).cgPath
            disc.path = path; shineMask.path = path
            side = b.height * 0.52
            icon.frame = CGRect(x: b.height * 0.3, y: (b.height - side) / 2, width: side, height: side)
            title.font = Style.font(b.height * 0.42)
            title.frame = CGRect(x: icon.frame.maxX + b.height * 0.1, y: 0, width: b.width - icon.frame.maxX - b.height * 0.42, height: b.height)
            subtitle.frame = .zero
        } else {
            let d = min(b.width, b.height)
            let outer = CGRect(x: (b.width - d) / 2, y: (b.height - d) / 2, width: d, height: d)
            let inner = outer.insetBy(dx: d * 0.085, dy: d * 0.085)
            halo.path = UIBezierPath(ovalIn: outer.insetBy(dx: 0.5, dy: 0.5)).cgPath
            let path = UIBezierPath(ovalIn: inner).cgPath
            disc.path = path; shineMask.path = path
            let h = inner.height, sub = subtitle.text != nil
            side = h * (sub ? 0.31 : 0.36)
            let titleHeight = h * (sub ? 0.22 : 0.24), subHeight = sub ? h * 0.15 : 0
            var y = inner.midY - (side + titleHeight + subHeight) / 2 - h * 0.01
            icon.frame = CGRect(x: inner.midX - side / 2, y: y, width: side, height: side)
            y += side + h * 0.01
            title.font = Style.font(max(9, h * 0.21))
            title.frame = CGRect(x: inner.minX + h * 0.08, y: y, width: h * 0.84, height: titleHeight)
            subtitle.font = Style.font(max(8, h * 0.14), .bold)
            subtitle.frame = CGRect(x: inner.minX + h * 0.18, y: y + titleHeight - h * 0.02, width: h * 0.64, height: subHeight)
        }
        if abs(side - iconSide) > 0.5 { iconSide = side; icon.image = Icon.image(id, side: side) }
        CATransaction.commit()
    }

    private func style() {
        CATransaction.begin(); CATransaction.setDisableActions(true)
        let p = isPressed
        if id.isPill {
            disc.fillColor = (p ? Style.gold : UIColor(white: 0.07, alpha: 0.72)).cgColor
            disc.strokeColor = UIColor.white.withAlphaComponent(0.85).cgColor; disc.lineWidth = 1.4
        } else {
            halo.fillColor = (p ? Style.gold.withAlphaComponent(0.45) : UIColor.white.withAlphaComponent(0.2)).cgColor
            halo.strokeColor = UIColor.white.withAlphaComponent(p ? 0.9 : 0.55).cgColor; halo.lineWidth = 1
            disc.fillColor = (p ? Style.gold : UIColor(red: 0.09, green: 0.09, blue: 0.1, alpha: 0.8)).cgColor
            disc.strokeColor = UIColor.white.withAlphaComponent(0.92).cgColor; disc.lineWidth = 1.6
        }
        shine.opacity = p ? 0.35 : 1
        let foreground = p ? Style.ink : .white
        icon.tintColor = id == .coin && !p ? Style.gold : foreground
        title.textColor = foreground; subtitle.textColor = foreground.withAlphaComponent(0.75)
        transform = p ? CGAffineTransform(scaleX: 0.94, y: 0.94) : .identity
        CATransaction.commit()
    }

    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        if id.isPill { return bounds.insetBy(dx: -8, dy: -8).contains(point) }
        return hypot(point.x - bounds.midX, point.y - bounds.midY) <= bounds.width / 2 + 8
    }
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard touch == nil, let t = touches.first else { return }
        touch = t; setPressed(true)
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
    private func end(_ touches: Set<UITouch>) {
        if let t = touch, touches.contains(t) { touch = nil; setPressed(false) }
    }
    func cancel() { touch = nil; setPressed(false) }
    private func setPressed(_ value: Bool) {
        guard value != isPressed else { return }
        isPressed = value; style(); onChange?(value)
    }
}

/// Left stick: eight directions, pushing it to the rim engages TURBO (sprint). Floating mode
/// recenters the base under the thumb anywhere in its half of the screen.
final class StickView: UIView {
    var onChange: (() -> Void)?
    var floating = true
    var edgeSprint = true
    var home = CGPoint.zero { didSet { if touch == nil { base.center = home } } }
    var limits = CGRect.null
    var diameter: CGFloat = 0 { didSet { if diameter != oldValue { rebuild() } } }
    private(set) var directions: UInt16 = 0
    private(set) var sprinting = false
    private let base = UIView(), knob = UIView(), knobFill = CAGradientLayer()
    private let ring = CAShapeLayer(), inner = CAShapeLayer(), chevrons = CAShapeLayer(), glow = CAShapeLayer()
    private var touch: UITouch?

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = false
        base.isUserInteractionEnabled = false
        addSubview(base); base.addSubview(knob)
        glow.fillColor = nil; glow.strokeColor = Style.gold.cgColor; glow.lineWidth = 4; glow.opacity = 0
        glow.shadowColor = Style.gold.cgColor; glow.shadowRadius = 8; glow.shadowOpacity = 1; glow.shadowOffset = .zero
        ring.fillColor = UIColor.white.withAlphaComponent(0.1).cgColor
        ring.strokeColor = UIColor.white.withAlphaComponent(0.6).cgColor; ring.lineWidth = 2
        inner.fillColor = nil; inner.strokeColor = UIColor.white.withAlphaComponent(0.32).cgColor; inner.lineWidth = 1.5
        chevrons.fillColor = nil; chevrons.strokeColor = UIColor.white.withAlphaComponent(0.7).cgColor
        chevrons.lineWidth = 2.2; chevrons.lineCap = .round; chevrons.lineJoin = .round
        for l in [glow, ring, inner, chevrons] { base.layer.addSublayer(l) }
        knobFill.type = .radial
        knobFill.colors = [UIColor(white: 0.45, alpha: 0.96).cgColor, UIColor(white: 0.08, alpha: 0.96).cgColor]
        knobFill.startPoint = CGPoint(x: 0.35, y: 0.3); knobFill.endPoint = CGPoint(x: 1, y: 1)
        knob.layer.addSublayer(knobFill)
        knob.layer.borderColor = UIColor.white.withAlphaComponent(0.35).cgColor; knob.layer.borderWidth = 1
        knob.layer.shadowColor = UIColor.black.cgColor; knob.layer.shadowOpacity = 0.45
        knob.layer.shadowRadius = 6; knob.layer.shadowOffset = CGSize(width: 0, height: 3)
        isAccessibilityElement = true; accessibilityLabel = "Movement stick"
    }
    required init?(coder: NSCoder) { fatalError() }

    private func rebuild() {
        let d = diameter, r = CGRect(x: 0, y: 0, width: d, height: d), c = CGPoint(x: d / 2, y: d / 2)
        CATransaction.begin(); CATransaction.setDisableActions(true)
        base.bounds = r
        if touch == nil { base.center = home }
        for l in [glow, ring, inner, chevrons] { l.frame = r }
        ring.path = UIBezierPath(ovalIn: r.insetBy(dx: 1, dy: 1)).cgPath
        glow.path = UIBezierPath(ovalIn: r.insetBy(dx: -2, dy: -2)).cgPath
        inner.path = UIBezierPath(ovalIn: r.insetBy(dx: d * 0.21, dy: d * 0.21)).cgPath
        let path = UIBezierPath(), arm = d * 0.055
        for quarter in 0..<4 {
            let angle = CGFloat(quarter) * .pi / 2
            let tip = CGPoint(x: c.x + cos(angle) * d * 0.375, y: c.y + sin(angle) * d * 0.375)
            path.move(to: CGPoint(x: tip.x - arm * cos(angle + .pi / 4), y: tip.y - arm * sin(angle + .pi / 4)))
            path.addLine(to: tip)
            path.addLine(to: CGPoint(x: tip.x - arm * cos(angle - .pi / 4), y: tip.y - arm * sin(angle - .pi / 4)))
        }
        chevrons.path = path.cgPath
        let k = d * 0.42
        knob.bounds = CGRect(x: 0, y: 0, width: k, height: k)
        knob.layer.cornerRadius = k / 2
        knobFill.frame = knob.bounds; knobFill.cornerRadius = k / 2
        knob.center = c
        CATransaction.commit()
    }

    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        guard bounds.contains(point) else { return false }
        return floating || hypot(point.x - home.x, point.y - home.y) <= diameter * 0.65
    }
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard touch == nil, let t = touches.first else { return }
        touch = t
        if floating {
            let p = t.location(in: self)
            base.center = limits.isNull || limits.isEmpty ? p
                : CGPoint(x: min(max(p.x, limits.minX), limits.maxX), y: min(max(p.y, limits.minY), limits.maxY))
        }
        track(t)
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touch, touches.contains(t) { track(t) }
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches) }
    private func end(_ touches: Set<UITouch>) {
        if let t = touch, touches.contains(t) { release() }
    }

    private func track(_ t: UITouch) {
        let p = t.location(in: self)
        let dx = p.x - base.center.x, dy = p.y - base.center.y
        let travel = diameter * 0.3, length = hypot(dx, dy)
        let k = length > travel ? travel / length : 1
        knob.center = CGPoint(x: diameter / 2 + dx * k, y: diameter / 2 + dy * k)
        var mask: UInt16 = 0
        if length > travel * 0.22 {
            let sector = (Int((atan2(dy, dx) / (.pi / 4)).rounded()) + 8) % 8
            mask = [Pad.right, Pad.right | Pad.down, Pad.down, Pad.down | Pad.left,
                    Pad.left, Pad.left | Pad.up, Pad.up, Pad.up | Pad.right][sector]
        }
        let sprint = edgeSprint && mask != 0 && length >= travel * 0.92
        if mask != directions || sprint != sprinting { directions = mask; setSprinting(sprint); onChange?() }
    }

    func release() {
        touch = nil
        let changed = directions != 0 || sprinting
        directions = 0; setSprinting(false)
        UIView.animate(withDuration: 0.12, delay: 0, options: [.beginFromCurrentState, .allowUserInteraction]) {
            self.base.center = self.home
            self.knob.center = CGPoint(x: self.diameter / 2, y: self.diameter / 2)
        }
        if changed { onChange?() }
    }

    private func setSprinting(_ value: Bool) {
        guard value != sprinting else { return }
        sprinting = value
        glow.opacity = value ? 1 : 0
        ring.strokeColor = (value ? Style.gold : UIColor.white.withAlphaComponent(0.6)).cgColor
    }
}

/// Running-man "SPRINT" badge next to the stick; lights up while TURBO is held through the stick.
final class SprintBadge: UIView {
    private let icon = UIImageView(image: UIImage(systemName: "figure.run", withConfiguration: UIImage.SymbolConfiguration(weight: .bold)))
    private let label = UILabel()
    var lit = false { didSet { if lit != oldValue { UIView.animate(withDuration: 0.12) { self.style() } } } }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isUserInteractionEnabled = false
        icon.contentMode = .scaleAspectFit
        label.text = "SPRINT"; label.textAlignment = .center
        for view in [icon, label] as [UIView] {
            view.layer.shadowColor = UIColor.black.cgColor; view.layer.shadowOpacity = 0.8
            view.layer.shadowRadius = 2; view.layer.shadowOffset = .zero
            addSubview(view)
        }
        style()
    }
    required init?(coder: NSCoder) { fatalError() }
    override func layoutSubviews() {
        super.layoutSubviews()
        let h = bounds.height
        icon.frame = CGRect(x: 0, y: 0, width: bounds.width, height: h * 0.64)
        label.font = Style.font(h * 0.28)
        label.frame = CGRect(x: 0, y: h * 0.66, width: bounds.width, height: h * 0.34)
    }
    private func style() {
        let color = lit ? Style.gold : UIColor.white.withAlphaComponent(0.9)
        icon.tintColor = color; label.textColor = color
        transform = lit ? CGAffineTransform(scaleX: 1.12, y: 1.12) : .identity
    }
}

final class RoundIconButton: UIControl {
    private let disc = CAShapeLayer()
    private let icon: UIImageView

    init(symbol: String) {
        icon = UIImageView(image: UIImage(systemName: symbol, withConfiguration: UIImage.SymbolConfiguration(weight: .heavy)))
        super.init(frame: .zero)
        icon.contentMode = .scaleAspectFit; icon.isUserInteractionEnabled = false
        disc.strokeColor = UIColor.white.withAlphaComponent(0.85).cgColor; disc.lineWidth = 1.5
        layer.addSublayer(disc); addSubview(icon)
        updateFill()
    }
    required init?(coder: NSCoder) { fatalError() }
    override var isHighlighted: Bool { didSet { updateFill() } }
    private func updateFill() {
        disc.fillColor = (isHighlighted ? Style.gold : UIColor(white: 0.07, alpha: 0.7)).cgColor
        icon.tintColor = isHighlighted ? Style.ink : .white
    }
    override func layoutSubviews() {
        super.layoutSubviews()
        disc.path = UIBezierPath(ovalIn: bounds.insetBy(dx: 1, dy: 1)).cgPath
        let s = bounds.width * 0.4
        icon.frame = CGRect(x: bounds.midX - s / 2, y: bounds.midY - s / 2, width: s, height: s)
    }
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool { bounds.insetBy(dx: -10, dy: -10).contains(point) }
}

/// Full-screen touch overlay: stick, action buttons, COIN/START and pause, with a customizable layout.
final class ControlsView: UIView {
    let stick = StickView()
    let pauseButton = RoundIconButton(symbol: "pause.fill")
    private let sprintBadge = SprintBadge()
    private(set) var buttons: [ControlID: ActionButton] = [:]
    var placements: [ControlID: Placement] = [:] { didSet { setNeedsLayout() } }
    var hiddenControls: Set<ControlID> = [] { didSet { refreshVisibility(); setNeedsLayout() } }
    var sizeFactor: CGFloat = 1 { didSet { if sizeFactor != oldValue { setNeedsLayout() } } }
    var editing = false { didSet { releaseAll(); refreshVisibility() } }
    var touchControlsHidden = false {
        didSet {
            guard touchControlsHidden != oldValue else { return }
            if touchControlsHidden { releaseAll() }
            UIView.animate(withDuration: 0.2) { self.refreshVisibility() }
        }
    }
    var haptics = true
    var onPause: (() -> Void)?
    var onActionPressed: ((Action) -> Void)?
    var onTouch: (() -> Void)?
    private let impact = UIImpactFeedbackGenerator(style: .light)
    private let selection = UISelectionFeedbackGenerator()

    var heldActions: Set<Action> { Set(buttons.compactMap { $0.value.isPressed ? $0.key.action : nil }) }
    var directions: UInt16 { stick.directions }
    var sprinting: Bool { stick.sprinting }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        addSubview(stick); addSubview(sprintBadge)
        for id in ControlID.allCases where id != .stick {
            let button = ActionButton(id)
            button.onChange = { [weak self] pressed in self?.buttonChanged(id, pressed) }
            buttons[id] = button; addSubview(button)
        }
        addSubview(pauseButton)
        pauseButton.accessibilityLabel = "Pause"
        pauseButton.addTarget(self, action: #selector(pauseTapped), for: .touchUpInside)
        stick.onChange = { [weak self] in self?.stickChanged() }
    }
    required init?(coder: NSCoder) { fatalError() }

    /// Area the controls live in: the safe area, using most of the home-indicator strip.
    var layoutArea: CGRect {
        let s = safeAreaInsets
        return bounds.inset(by: UIEdgeInsets(top: max(s.top, 10), left: max(s.left, 18), bottom: max(s.bottom - 8, 10), right: max(s.right, 18)))
    }
    var unit: CGFloat { min(max(min(bounds.height, bounds.width / 1.6) / 414, 0.8), 1.3) }

    func size(of id: ControlID) -> CGSize {
        let k = unit * sizeFactor * (placements[id]?.scale ?? 1)
        return CGSize(width: id.baseSize.width * k, height: id.baseSize.height * k)
    }
    func center(of id: ControlID) -> CGPoint {
        let a = layoutArea
        guard let p = placements[id] else { return defaultCenter(of: id) }
        return CGPoint(x: a.minX + p.x * a.width, y: a.minY + p.y * a.height)
    }
    /// Frame of a control (for the stick, its resting base), kept inside the layout area.
    func frame(of id: ControlID) -> CGRect {
        let a = layoutArea, s = size(of: id), c = center(of: id)
        let x = min(max(c.x - s.width / 2, a.minX), a.maxX - s.width)
        let y = min(max(c.y - s.height / 2, a.minY), a.maxY - s.height)
        return CGRect(x: x, y: y, width: s.width, height: s.height)
    }
    func frame(of spot: Spot) -> CGRect? {
        switch spot {
        case .pause: return pauseButton.frame
        case .control(let id): return touchControlsHidden || hiddenControls.contains(id) ? nil : frame(of: id)
        }
    }
    /// Stores a control's position relative to the layout area so it survives other screen sizes.
    func place(_ id: ControlID, center: CGPoint, scale: CGFloat) {
        let a = layoutArea
        placements[id] = Placement(x: (center.x - a.minX) / max(a.width, 1), y: (center.y - a.minY) / max(a.height, 1), scale: scale)
    }

    private func defaultCenter(of id: ControlID) -> CGPoint {
        let a = layoutArea, u = unit * sizeFactor
        let shoot = CGPoint(x: a.maxX - 64 * u, y: a.maxY - 66 * u)
        switch id {
        case .stick: return CGPoint(x: a.minX + 96 * u, y: a.maxY - 90 * u)
        case .shoot: return shoot
        case .pass: return CGPoint(x: shoot.x - 112 * u, y: shoot.y + 8 * u)
        case .turbo: return CGPoint(x: shoot.x - 4 * u, y: shoot.y - 110 * u)
        case .spin: return CGPoint(x: shoot.x - 100 * u, y: shoot.y - 92 * u)
        case .dunk: return CGPoint(x: shoot.x - 196 * u, y: shoot.y - 44 * u)
        case .shove: return CGPoint(x: shoot.x - 204 * u, y: shoot.y + 30 * u)
        case .coin: return CGPoint(x: a.maxX - 152 * u, y: a.minY + 20 * u)
        case .start: return CGPoint(x: a.maxX - 48 * u, y: a.minY + 20 * u)
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        let a = layoutArea, u = unit
        for (id, button) in buttons {
            let f = frame(of: id)
            button.bounds = CGRect(origin: .zero, size: f.size)
            button.center = CGPoint(x: f.midX, y: f.midY)
        }
        let base = frame(of: .stick)
        let top = min(a.minY + 48 * u, base.minY - 10)
        let onLeft = base.midX < bounds.midX
        stick.frame = CGRect(x: onLeft ? 0 : bounds.midX, y: top, width: bounds.width / 2, height: bounds.height - top)
        stick.diameter = base.width
        stick.limits = convert(a, to: stick).insetBy(dx: base.width / 2, dy: base.width / 2)
        stick.home = convert(CGPoint(x: base.midX, y: base.midY), to: stick)
        sprintBadge.bounds = CGRect(x: 0, y: 0, width: 50 * u, height: 40 * u)
        sprintBadge.center = CGPoint(x: base.minX + 8 * u, y: base.minY - 6 * u)
        pauseButton.frame = CGRect(x: a.minX, y: a.minY, width: 42 * u, height: 42 * u)
    }

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        let hit = super.hitTest(point, with: event)
        if hit != nil, event?.type == .touches { onTouch?() }
        return hit
    }

    func releaseAll() {
        stick.release()
        for button in buttons.values { button.cancel() }
    }

    private func refreshVisibility() {
        let show = editing || !touchControlsHidden
        for (id, button) in buttons {
            let hidden = hiddenControls.contains(id)
            button.alpha = show ? (hidden ? (editing ? 0.35 : 0) : 1) : 0
        }
        stick.alpha = show ? 1 : 0
        sprintBadge.alpha = show ? 1 : 0
        pauseButton.alpha = editing ? 0 : 1
        isUserInteractionEnabled = !editing
    }

    private func buttonChanged(_ id: ControlID, _ pressed: Bool) {
        guard pressed, let action = id.action else { return }
        if haptics { impact.impactOccurred(intensity: 0.7) }
        onActionPressed?(action)
    }
    private func stickChanged() {
        if stick.sprinting && !sprintBadge.lit && haptics { selection.selectionChanged() }
        sprintBadge.lit = stick.sprinting
    }
    @objc private func pauseTapped() { onPause?() }
}
