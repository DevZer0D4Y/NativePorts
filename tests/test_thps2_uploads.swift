import Foundation
import Metal
struct Params { var sourceSize: SIMD2<Float>; var outputSize: SIMD2<Float>; var frame: Float; var mode: Int32 }
guard let device = MTLCreateSystemDefaultDevice(), let queue = device.makeCommandQueue() else { fatalError("Metal unavailable") }
let source = try String(contentsOfFile: (CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : Bundle.main.path(forResource: "NewPixie", ofType: "metal")!), encoding: .utf8)
let library = try device.makeLibrary(source: source, options: nil)
let desc = MTLRenderPipelineDescriptor()
desc.vertexFunction = library.makeFunction(name: "crtVertex")
desc.fragmentFunction = library.makeFunction(name: "crtFragment")
desc.colorAttachments[0].pixelFormat = .bgra8Unorm
let pipeline = try device.makeRenderPipelineState(descriptor: desc)
let td = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .rgba8Unorm, width: 64, height: 64, mipmapped: false)
td.storageMode = .shared; td.usage = .shaderRead
let inputs = (0..<3).map { _ in device.makeTexture(descriptor: td)! }
td.pixelFormat = .bgra8Unorm; td.usage = [.shaderRead, .renderTarget]
let outputs = (0..<3).map { _ in device.makeTexture(descriptor: td)! }
var commands: [MTLCommandBuffer?] = Array(repeating: nil, count: 3)
var expected = [UInt8](repeating: 0, count: 3)
func verify(_ slot: Int) {
 guard let command = commands[slot] else { return }
 command.waitUntilCompleted()
 if let error = command.error { fatalError("\(error)") }
 var data = [UInt8](repeating: 0, count: 64*64*4)
 outputs[slot].getBytes(&data, bytesPerRow: 256, from: MTLRegionMake2D(0,0,64,64), mipmapLevel: 0)
 assert(data[32*256+32*4+2] == expected[slot], "A pending input was overwritten")
}
for frame in 0..<120 {
 let slot = frame % 3
 verify(slot)
 var pixels = [UInt8](repeating: 0, count: 64*64*4)
 let red = UInt8(frame+20)
 for i in stride(from: 0, to: pixels.count, by: 4) { pixels[i]=red; pixels[i+3]=255 }
 inputs[slot].replace(region: MTLRegionMake2D(0,0,64,64), mipmapLevel: 0, withBytes: pixels, bytesPerRow: 256)
 let command = queue.makeCommandBuffer()!
 let pass = MTLRenderPassDescriptor(); pass.colorAttachments[0].texture = outputs[slot]
 pass.colorAttachments[0].loadAction = .clear; pass.colorAttachments[0].storeAction = .store
 let encoder = command.makeRenderCommandEncoder(descriptor: pass)!
 encoder.setRenderPipelineState(pipeline)
 var params = Params(sourceSize: SIMD2(64,64), outputSize: SIMD2(64,64), frame: Float(frame), mode: 0)
 encoder.setFragmentBytes(&params, length: MemoryLayout<Params>.stride, index: 0)
 for i in 0..<3 { encoder.setFragmentTexture(inputs[slot], index: i) }
 encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3); encoder.endEncoding()
 commands[slot]=command; expected[slot]=red; command.commit()
}
for slot in 0..<3 { verify(slot) }
let result = "PASS: 120 Metal frames across 3 upload slots; pending textures never overwritten."
print(result)
try result.write(to: Bundle.main.bundleURL.deletingLastPathComponent().appendingPathComponent("metal-upload-result.txt"), atomically: true, encoding: .utf8)
