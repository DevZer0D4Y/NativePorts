import MetalKit

private struct ShaderParams {
    var sourceSize: SIMD2<Float>
    var outputSize: SIMD2<Float>
    var frame: Float
    var mode: Int32
}

/// Four-pass NewPixie pipeline with separate initialized feedback textures.
final class CRTView: MTKView, MTKViewDelegate {
    var crt = true
    var fill = false
    var aspect: Double = 4.0 / 3.0
    var colorSides = true
    var smooth = false
    private let queue: MTLCommandQueue
    private let pipeline: MTLRenderPipelineState
    private var input: MTLTexture?
    private var inputs: [MTLTexture] = []
    private var uploadSlot = -1
    private var uploads: [MTLCommandBuffer?] = Array(repeating: nil, count: 3)
    private var targets: [MTLTexture] = []
    private var feedbackIndex = 0
    private var emulatedFrame: Float = 0
    private var fresh = false

    init(native: Bool) throws {
        guard let gpu = MTLCreateSystemDefaultDevice(), let q = gpu.makeCommandQueue(),
              let shaderURL = Bundle.main.url(forResource: "NewPixie", withExtension: "metal") else {
            throw NSError(domain: "SSF2T", code: 1, userInfo: [NSLocalizedDescriptionKey: "Metal is unavailable."])
        }
        let library = try gpu.makeLibrary(source: String(contentsOf: shaderURL, encoding: .utf8), options: nil)
        queue = q
        let d = MTLRenderPipelineDescriptor()
        d.vertexFunction = library.makeFunction(name: "crtVertex")
        d.fragmentFunction = library.makeFunction(name: "crtFragment")
        d.colorAttachments[0].pixelFormat = .bgra8Unorm
        pipeline = try gpu.makeRenderPipelineState(descriptor: d)
        super.init(frame: .zero, device: gpu)
        colorPixelFormat = .bgra8Unorm
        isPaused = true; enableSetNeedsDisplay = false
        framebufferOnly = true; delegate = self
        autoResizeDrawable = false
        clearColor = MTLClearColor(red: 0.055, green: 0.09, blue: 0.18, alpha: 1)
        isUserInteractionEnabled = false
        NotificationCenter.default.addObserver(self, selector: #selector(thermalChanged),
            name: ProcessInfo.thermalStateDidChangeNotification, object: nil)
    }
    deinit { NotificationCenter.default.removeObserver(self) }
    @objc private func thermalChanged() {
        DispatchQueue.main.async { [weak self] in self?.setNeedsLayout() }
    }
    required init(coder: NSCoder) { fatalError("init(coder:) unsupported") }
    override func layoutSubviews() {
        super.layoutSubviews()
        // Reduce only the filter's output workload when the phone gets hot.
        // The emulated framebuffer, view bounds and game proportions stay fixed.
        let thermal = ProcessInfo.processInfo.thermalState
        let limit: CGFloat = thermal == .serious || thermal == .critical ? 1024 : thermal == .fair ? 1280 : 1600
        let scale = min(window?.screen.scale ?? UIScreen.main.scale, limit / max(1, max(bounds.width, bounds.height)))
        drawableSize = CGSize(width: max(1, bounds.width * scale), height: max(1, bounds.height * scale))
    }
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

    func upload(_ bytes: UnsafePointer<UInt8>, width: Int, height: Int, frameNumber: Float) {
        guard width > 0, height > 0 else { return }
        if input?.width != width || input?.height != height {
            // Drain only on a size change. Ordinary uploads rotate through
            // three slots, waiting only before reusing a still-busy texture.
            for command in uploads { command?.waitUntilCompleted() }
            uploads = Array(repeating: nil, count: 3); uploadSlot = -1
            let d = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rgba8Unorm, width: width, height: height, mipmapped: false)
            d.storageMode = .shared; d.usage = .shaderRead
            inputs = (0..<3).compactMap { _ in device?.makeTexture(descriptor: d) }
            guard inputs.count == 3 else { return }
            d.pixelFormat = .bgra8Unorm; d.usage = [.renderTarget, .shaderRead]
            targets = (0..<4).compactMap { _ in device?.makeTexture(descriptor: d) }
            feedbackIndex = 0
            if let c = queue.makeCommandBuffer() {
                for tex in targets {
                    let pass = MTLRenderPassDescriptor()
                    pass.colorAttachments[0].texture = tex
                    pass.colorAttachments[0].loadAction = .clear
                    pass.colorAttachments[0].storeAction = .store
                    pass.colorAttachments[0].clearColor = MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
                    c.makeRenderCommandEncoder(descriptor: pass)?.endEncoding()
                }
                c.commit()
            }
        }
        uploadSlot = (uploadSlot + 1) % inputs.count
        uploads[uploadSlot]?.waitUntilCompleted()
        uploads[uploadSlot] = nil
        input = inputs[uploadSlot]
        input?.replace(region: MTLRegionMake2D(0, 0, width, height), mipmapLevel: 0, withBytes: bytes, bytesPerRow: width * 4)
        emulatedFrame = frameNumber; fresh = true
        // A dim average of the edge pixels fills the unused display, including game menus.
        var rgb = SIMD3<Double>(repeating: 0); var count = 0.0
        for y in stride(from: 0, to: height, by: max(1, height / 16)) {
            for x in [0, width - 1] {
                let i = (y * width + x) * 4
                rgb += SIMD3(Double(bytes[i]), Double(bytes[i + 1]), Double(bytes[i + 2])); count += 1
            }
        }
        rgb = rgb / (count * 255) * 0.3 + SIMD3(0.04, 0.04, 0.06)
        clearColor = MTLClearColor(red: rgb.x, green: rgb.y, blue: rgb.z, alpha: 1)
    }

    private func encode(_ c: MTLCommandBuffer, target: MTLTexture, source: MTLTexture,
                        mode: Int32, viewport: MTLViewport? = nil, clear: MTLClearColor? = nil) {
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = target
        pass.colorAttachments[0].loadAction = .clear
        pass.colorAttachments[0].storeAction = .store
        pass.colorAttachments[0].clearColor = clear ?? MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1)
        guard let e = c.makeRenderCommandEncoder(descriptor: pass) else { return }
        e.setRenderPipelineState(pipeline)
        if let viewport { e.setViewport(viewport) }
        e.setFragmentTexture(source, index: 0)
        e.setFragmentTexture(targets[feedbackIndex] === target ? input : targets[feedbackIndex], index: 1)
        e.setFragmentTexture(targets[3] === target ? input : targets[3], index: 2)
        let output = SIMD2<Float>(Float(viewport?.width ?? Double(target.width)), Float(viewport?.height ?? Double(target.height)))
        var p = ShaderParams(sourceSize: SIMD2(Float(source.width), Float(source.height)), outputSize: output, frame: emulatedFrame, mode: mode)
        e.setFragmentBytes(&p, length: MemoryLayout<ShaderParams>.stride, index: 0)
        e.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3); e.endEncoding()
    }

    func draw(in view: MTKView) {
        guard let input, targets.count == 4, let drawable = currentDrawable, let c = queue.makeCommandBuffer() else { return }
        if fresh && crt {
            let next = 1 - feedbackIndex
            encode(c, target: targets[next], source: input, mode: 1)
            feedbackIndex = next
            encode(c, target: targets[2], source: targets[next], mode: 2)
            encode(c, target: targets[3], source: targets[2], mode: 3)
        }
        fresh = false
        let w = Double(drawable.texture.width), h = Double(drawable.texture.height)
        let width = fill ? w : min(w, h * aspect)
        let height = fill ? h : min(h, w / aspect)
        let viewport = MTLViewport(originX: (w - width) / 2, originY: (h - height) / 2, width: width, height: height, znear: 0, zfar: 1)
        encode(c, target: drawable.texture, source: crt ? targets[feedbackIndex] : input,
               mode: crt ? 4 : (smooth ? 5 : 0), viewport: viewport, clear: colorSides && viewport.originY < 1 ? clearColor : MTLClearColor(red: 0, green: 0, blue: 0, alpha: 1))
        // Feedback passes stay ordered on this queue. CPU uploads reuse only
        // a slot whose last reader has completed, so no per-frame GPU drain.
        if uploadSlot >= 0 { uploads[uploadSlot] = c }
        c.present(drawable); c.commit()
    }
}
