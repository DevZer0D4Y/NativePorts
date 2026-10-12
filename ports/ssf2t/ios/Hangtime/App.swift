import UIKit
import AVFoundation
import GameController
import UniformTypeIdentifiers

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    func application(_ application: UIApplication, configurationForConnecting session: UISceneSession,
                     options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        let configuration = UISceneConfiguration(name: "Hangtime", sessionRole: session.role)
        configuration.delegateClass = SceneDelegate.self
        return configuration
    }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
    var window: UIWindow?
    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options: UIScene.ConnectionOptions) {
        guard let windowScene = scene as? UIWindowScene else { return }
        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = GameViewController()
        self.window = window
        window.makeKeyAndVisible()
    }
    func sceneWillResignActive(_ scene: UIScene) {
        (window?.rootViewController as? GameViewController)?.background()
    }
    func sceneDidEnterBackground(_ scene: UIScene) {
        (window?.rootViewController as? GameViewController)?.background()
    }
}

final class GameViewController: GCEventViewController, UIDocumentPickerDelegate {
    private let settings = GameSettings.shared
    private let screen = PortScreenView()
    private let controls = ControlsView()
    private let startScreen = StartScreenView()
    private let toast = ToastView()
    private let gamepads = GamepadHub()
    private var sequencers = [ActionSequencer](repeating: ActionSequencer(), count: 4)
    private var pauseMenu: PauseMenuView?
    private var tutorial: TutorialView?
    private var editor: LayoutEditorView?
    private let audio = AVAudioEngine()
    private var source: AVAudioSourceNode?
    private var displayLink: CADisplayLink?
    private var playing = false
    private var paused = false
    private var previousTime: CFTimeInterval = 0
    private var accumulated: CFTimeInterval = 0
    private var lastSave: CFTimeInterval = 0
    /// AUTO touch mode: the last input came from a gamepad, so the touch overlay steps aside.
    private var gamepadInUse = false
    private var coreAspect = 4.0 / 3.0
    private var frameAspect: CGFloat = 400.0 / 254.0
    private let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
    override var prefersStatusBarHidden: Bool { true }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .all }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }

    override func viewDidLoad() {
        super.viewDidLoad()
        // Make the app’s shared Documents folder available on first launch.
        do { try FileManager.default.createDirectory(at: documents, withIntermediateDirectories: true) }
        catch { showError("Cannot create game folder: \(error.localizedDescription)") }
        // Deliver controller input to GameController rather than UIKit navigation.
        controllerUserInteractionEnabled = false
        if #available(iOS 18.0, *) {
            let interaction = GCEventInteraction()
            interaction.handledEventTypes = .gamepad
            view.addInteraction(interaction)
        }
        view.backgroundColor = UIColor(red: 0.04, green: 0.07, blue: 0.15, alpha: 1)
        view.addSubview(screen)
        view.addSubview(startScreen)
        controls.isHidden = true
        view.addSubview(controls)
        view.addSubview(toast)
        startScreen.onImport = { [weak self] in self?.importROM() }
        controls.onPause = { [weak self] in self?.openPauseMenu() }
        controls.onActionPressed = { [weak self] action in self?.sequencers[0].trigger(action) }
        controls.onTouch = { [weak self] in self?.touchUsed() }
        gamepads.bindings = settings.padBindings
        gamepads.claimSystemButtons()
        gamepadInUse = gamepads.connected
        applySettings()
        NotificationCenter.default.addObserver(self, selector: #selector(background), name: UIApplication.willResignActiveNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(background), name: UIApplication.willTerminateNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(background), name: AVAudioSession.interruptionNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(controllerConnected), name: .GCControllerDidConnect, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(controllerDisconnected), name: .GCControllerDidDisconnect, object: nil)
        // Ticks run even before a game starts so gamepads can drive the menus.
        let link = CADisplayLink(target: self, selector: #selector(tick(_:)))
        link.add(to: .main, forMode: .common); displayLink = link
        let existing = documents.appendingPathComponent("ssf2t.zip")
        if FileManager.default.fileExists(atPath: existing.path) { start(existing) }
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let bounds = view.bounds
        for overlay in [startScreen, controls, pauseMenu, tutorial, editor] as [UIView?] { overlay?.frame = bounds }
        layoutScreen()
    }

    /// FULL stretches to the display, SQUARE keeps square pixels, ARCADE uses the core's arcade monitor aspect
    /// (4:3, or about 16:9 when colorSides shows the full bitmap).
    private func layoutScreen() {
        screen.frame = view.bounds
        screen.fill = settings.screen == .full
        screen.aspect = settings.screen == .square ? Double(frameAspect) : ht_aspect_ratio()
    }

    private func applySettings() {
        screen.colorSides = settings.colorSides
        ht_set_widescreen(settings.gameWidescreen)
        screen.crt = settings.crtMode == 1
        let filter: CALayerContentsFilter = settings.smoothPixels ? .linear : .nearest
        screen.layer.magnificationFilter = filter; screen.layer.minificationFilter = filter
        screen.smooth = settings.smoothPixels
        controls.sizeFactor = settings.sizeValue
        controls.alpha = settings.opacityValue
        controls.stick.floating = settings.floatingStick
        controls.stick.edgeSprint = settings.edgeSprint
        controls.haptics = settings.haptics
        if editor == nil {
            controls.placements = settings.layout
            controls.hiddenControls = settings.hiddenControls
        }
        updateTouchVisibility()
        view.setNeedsLayout()
    }

    private func updateTouchVisibility() {
        switch settings.touchMode {
        case .on: controls.touchControlsHidden = false
        case .off: controls.touchControlsHidden = true
        case .auto: controls.touchControlsHidden = gamepads.connected && gamepadInUse
        }
    }
    private func touchUsed() {
        guard gamepadInUse else { return }
        gamepadInUse = false; updateTouchVisibility()
    }

    @objc private func controllerConnected() {
        gamepads.claimSystemButtons()
        gamepadInUse = true; updateTouchVisibility()
        pauseMenu?.gamepadName = gamepads.name
        tutorial?.gamepadMode = controls.touchControlsHidden
        if let name = gamepads.name { toast.show("\(name.uppercased()) CONNECTED") }
    }
    @objc private func controllerDisconnected() {
        if !gamepads.connected { gamepadInUse = false }
        updateTouchVisibility()
        pauseMenu?.gamepadName = gamepads.name
        tutorial?.gamepadMode = controls.touchControlsHidden
        toast.show("CONTROLLER DISCONNECTED")
        if playing && !paused && editor == nil && tutorial == nil { openPauseMenu() }
    }

    @objc private func importROM() {
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.zip], asCopy: true)
        picker.delegate = self; present(picker, animated: true)
    }
    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        guard let selected = urls.first else { return }
        var report = [CChar](repeating: 0, count: 8192)
        guard ht_validate_rom(selected.path, &report, report.count) else { showError(String(cString: report)); return }
        do {
            let target = documents.appendingPathComponent("ssf2t.zip")
            let staging = documents.appendingPathComponent("import.zip")
            if FileManager.default.fileExists(atPath: staging.path) { try FileManager.default.removeItem(at: staging) }
            try FileManager.default.copyItem(at: selected, to: staging)
            if FileManager.default.fileExists(atPath: target.path) { _ = try FileManager.default.replaceItemAt(target, withItemAt: staging) }
            else { try FileManager.default.moveItem(at: staging, to: target) }
            start(target)
        } catch { showError(error.localizedDescription) }
    }

    private func start(_ url: URL) {
        guard !playing else { return }
        guard ht_start(url.path, documents.path) else { showError(String(cString: ht_error())); return }
        playing = true
        startScreen.isHidden = true; controls.isHidden = false
        view.setNeedsLayout()
        do {
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
            let format = AVAudioFormat(standardFormatWithSampleRate: ht_sample_rate(), channels: 2)!
            let node = AVAudioSourceNode(format: format) { _, _, count, list -> OSStatus in
                let buffers = UnsafeMutableAudioBufferListPointer(list)
                if buffers.count == 2, let l = buffers[0].mData, let r = buffers[1].mData {
                    ht_audio_read(l.assumingMemoryBound(to: Float.self), r.assumingMemoryBound(to: Float.self), count)
                }
                return noErr
            }
            source = node; audio.attach(node); audio.connect(node, to: audio.mainMixerNode, format: format)
            try audio.start()
        } catch { showError("Audio unavailable: \(error.localizedDescription)") }
        if !settings.tutorialSeen {
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { [weak self] in self?.showTutorial(fromMenu: false) }
        } else {
            toast.show(controls.touchControlsHidden ? "PRESS \(gamepads.primaryLabel(for: .coin)), THEN \(gamepads.label(for: .start)) TO PLAY" : "TAP COIN, THEN START TO PLAY", duration: 4)
        }
    }

    @objc private func tick(_ link: CADisplayLink) {
        let pads = gamepads.poll(now: link.timestamp)
        if pads.active && !gamepadInUse { gamepadInUse = true; updateTouchVisibility() }
        // Skip the tick that closes a menu so the button that closed it never reaches the game.
        let menuWasOpen = pauseMenu != nil || editor != nil
        for event in pads.events { handle(event) }
        guard playing, !paused, !menuWasOpen, editor == nil else { previousTime = 0; return }
        if previousTime == 0 { previousTime = link.timestamp; return }
        accumulated += min(link.timestamp - previousTime, 0.1); previousTime = link.timestamp
        // While the tutorial is up, gamepad presses page through it instead of reaching the game.
        let players = tutorial == nil ? pads.players : []
        for (index, player) in players.enumerated() {
            for action in player.pressed { sequencers[index].trigger(action) }
            if player.startTap { sequencers[index].pulseStart() }
        }
        let interval = 1 / ht_fps()
        var steps = 0
        while accumulated >= interval && steps < 4 {
            for index in 0..<4 {
                var player = index < players.count ? players[index] : PadState()
                var sprint = false
                if index == 0 {
                    player.held.formUnion(controls.heldActions)
                    player.directions |= controls.directions
                    sprint = controls.sprinting
                }
                // Controllers join as players 1, 3, 2 and 4: the first two play against each other.
                ht_set_buttons(UInt32(GamepadHub.playerOrder[index]), sequencers[index].frame(directions: player.directions, held: player.held, sprint: sprint))
            }
            ht_run(); accumulated -= interval; steps += 1
        }
        if steps == 4 { accumulated = 0 }
        guard steps > 0 else { return }
        if link.timestamp - lastSave >= 5 {
            lastSave = link.timestamp
            if !ht_save_data() { toast.show("SAVE FAILED") }
        }
        let w = Int(ht_width()), h = Int(ht_height())
        guard w > 0, h > 0, let pixels = ht_pixels() else { return }
        let aspect = CGFloat(w) / CGFloat(h)
        if abs(aspect - frameAspect) > 0.001 { frameAspect = aspect; layoutScreen() }
        // Widescreen can change the picture's shape without changing its size.
        if abs(ht_aspect_ratio() - coreAspect) > 0.001 { coreAspect = ht_aspect_ratio(); layoutScreen() }
        screen.upload(pixels, width: w, height: h)
    }

    private func handle(_ event: NavEvent) {
        if presentedViewController != nil { return }
        if let editor { editor.handle(event) }
        else if let tutorial { tutorial.handle(event) }
        else if let pauseMenu { pauseMenu.handle(event) }
        else if event == .pause && playing { openPauseMenu() }
    }

    private func setPaused(_ value: Bool) {
        paused = value; previousTime = 0; accumulated = 0
        controls.releaseAll()
        for index in 0..<4 { sequencers[index].reset(); ht_set_buttons(UInt32(index), 0) }
        ht_audio_clear()
        if value { if playing { _ = ht_save_data() }; audio.pause() } else if playing { try? audio.start() }
    }

    private func openPauseMenu(tab: PauseMenuView.Tab = .game) {
        guard playing, pauseMenu == nil, editor == nil, tutorial == nil else { return }
        if presentedViewController != nil { return }
        setPaused(true)
        let menu = PauseMenuView(tab: tab)
        menu.bindingText = { [weak self] action in self?.gamepads.label(for: action) ?? "—" }
        menu.gamepadName = gamepads.name
        menu.onResume = { [weak self] in self?.closePauseMenu(resume: true) }
        menu.onTutorial = { [weak self] in self?.closePauseMenu(resume: false); self?.showTutorial(fromMenu: true) }
        menu.onRestart = { [weak self] in self?.confirmRestart() }
        menu.onCustomize = { [weak self] in self?.closePauseMenu(resume: false); self?.openEditor() }
        menu.onResetLayout = { [weak self] in
            guard let self else { return }
            self.settings.layout = [:]; self.settings.hiddenControls = GameSettings.defaultHidden
            self.applySettings(); self.toast.show("LAYOUT RESET")
        }
        menu.onResetBindings = { [weak self] in
            guard let self else { return }
            self.gamepads.bindings = PadButton.defaults; self.settings.padBindings = PadButton.defaults
        }
        menu.onSettingsChanged = { [weak self] in self?.applySettings() }
        menu.onCaptureBinding = { [weak self] action in self?.captureBinding(action) }
        menu.frame = view.bounds; menu.alpha = 0
        view.insertSubview(menu, belowSubview: toast)
        UIView.animate(withDuration: 0.18) { menu.alpha = 1 }
        pauseMenu = menu
    }
    private func closePauseMenu(resume: Bool) {
        guard let menu = pauseMenu else { return }
        gamepads.onCapture = nil
        pauseMenu = nil
        UIView.animate(withDuration: 0.15, animations: { menu.alpha = 0 }) { _ in menu.removeFromSuperview() }
        if resume { gamepads.suppressHeldButtons(); setPaused(false) }
    }

    private func captureBinding(_ action: Action) {
        guard gamepads.connected else { toast.show("CONNECT A CONTROLLER FIRST"); return }
        pauseMenu?.setCapturing(action)
        gamepads.onCapture = { [weak self] button in
            guard let self else { return }
            self.gamepads.onCapture = nil
            if let button {
                self.gamepads.assign(button, to: action)
                self.settings.padBindings = self.gamepads.bindings
            }
            self.pauseMenu?.reload()
        }
    }

    private func showTutorial(fromMenu: Bool) {
        guard playing, tutorial == nil, editor == nil, pauseMenu == nil else { return }
        let guide = TutorialView()
        guide.spotFrame = { [weak self] spot in self?.controls.frame(of: spot) }
        guide.padLabel = { [weak self] action in self?.gamepads.primaryLabel(for: action) ?? action.title }
        guide.gamepadMode = controls.touchControlsHidden
        guide.onClose = { [weak self] in
            guard let self else { return }
            self.tutorial = nil; self.settings.tutorialSeen = true
            self.gamepads.suppressHeldButtons()
            if fromMenu || self.paused { self.openPauseMenu() }
            else { self.toast.show(self.controls.touchControlsHidden ? "PRESS \(self.gamepads.primaryLabel(for: .coin)), THEN \(self.gamepads.label(for: .start)) TO PLAY" : "TAP COIN, THEN START TO PLAY", duration: 4) }
        }
        guide.frame = view.bounds; guide.alpha = 0
        view.insertSubview(guide, belowSubview: toast)
        UIView.animate(withDuration: 0.2) { guide.alpha = 1 }
        tutorial = guide
    }

    private func openEditor() {
        guard editor == nil else { return }
        controls.alpha = 1
        controls.editing = true
        let layoutEditor = LayoutEditorView(controls: controls)
        layoutEditor.onFinish = { [weak self] saved in
            guard let self else { return }
            self.editor = nil
            self.controls.editing = false
            self.applySettings()
            if saved { self.toast.show("LAYOUT SAVED") }
            self.openPauseMenu(tab: .touch)
        }
        layoutEditor.frame = view.bounds
        view.insertSubview(layoutEditor, belowSubview: toast)
        editor = layoutEditor
    }

    private func confirmRestart() {
        let alert = UIAlertController(title: "Restart the arcade?", message: "The current game ends. High scores and settings are kept.", preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
        alert.addAction(UIAlertAction(title: "Restart", style: .destructive) { [weak self] _ in
            ht_reset(); self?.closePauseMenu(resume: true)
        })
        present(alert, animated: true)
    }

    @objc fileprivate func background() {
        guard playing else { return }
        if pauseMenu == nil && editor == nil && tutorial == nil { openPauseMenu() }
        if !paused { setPaused(true) }
    }

    private func showError(_ message: String) {
        let alert = UIAlertController(title: "Super Street Fighter II Turbo", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        if playing && !paused { openPauseMenu() }
        if presentedViewController != nil { dismiss(animated: true) { self.present(alert, animated: true) } }
        else { present(alert, animated: true) }
    }
}
