import AppKit
import Metal
struct Params { var sourceSize:SIMD2<Float>;var outputSize:SIMD2<Float>;var frame:Float;var mode:Int32 }
let root=Bundle.main.bundleURL.deletingLastPathComponent()
var result=""
do {
 guard let gpu=MTLCreateSystemDefaultDevice(),let queue=gpu.makeCommandQueue() else {throw NSError(domain:"GPU",code:1)}
 let source=try String(contentsOf:Bundle.main.url(forResource:"NewPixie",withExtension:"metal")!,encoding:.utf8)
 let library=try gpu.makeLibrary(source:source,options:nil)
 let d=MTLRenderPipelineDescriptor();d.vertexFunction=library.makeFunction(name:"crtVertex");d.fragmentFunction=library.makeFunction(name:"crtFragment");d.colorAttachments[0].pixelFormat = .bgra8Unorm
 let pipeline=try gpu.makeRenderPipelineState(descriptor:d)
 let td=MTLTextureDescriptor.texture2DDescriptor(pixelFormat:.bgra8Unorm,width:64,height:64,mipmapped:false);td.storageMode = .shared;td.usage=[.renderTarget,.shaderRead]
 let textures=(0..<5).map{_ in gpu.makeTexture(descriptor:td)!}
 var data=[UInt8](repeating:180,count:64*64*4);for i in stride(from:3,to:data.count,by:4){data[i]=255}
 for t in textures { t.replace(region:MTLRegionMake2D(0,0,64,64),mipmapLevel:0,withBytes:data,bytesPerRow:256) }
 let c=queue.makeCommandBuffer()!
 for mode:Int32 in [1,2,3,4] {
  let pass=MTLRenderPassDescriptor();pass.colorAttachments[0].texture=textures[Int(mode)];pass.colorAttachments[0].loadAction = .clear;pass.colorAttachments[0].storeAction = .store
  let e=c.makeRenderCommandEncoder(descriptor:pass)!;e.setRenderPipelineState(pipeline)
  var p=Params(sourceSize:SIMD2(64,64),outputSize:SIMD2(64,64),frame:10,mode:mode);e.setFragmentBytes(&p,length:MemoryLayout<Params>.stride,index:0)
  e.setFragmentTexture(textures[Int(mode)-1],index:0);e.setFragmentTexture(mode==1 ? textures[0]:textures[1],index:1);e.setFragmentTexture(mode==4 ? textures[3]:textures[0],index:2)
  e.drawPrimitives(type:.triangle,vertexStart:0,vertexCount:3);e.endEncoding()
 }
 c.commit();c.waitUntilCompleted();if let error=c.error{throw error}
 textures[4].getBytes(&data,bytesPerRow:256,from:MTLRegionMake2D(0,0,64,64),mipmapLevel:0)
 guard data[32*256+32*4]>5 else{throw NSError(domain:"Black output",code:2)}
 result="PASS: Metal shader compiled and all four NewPixie passes produced non-black output on \(gpu.name)."
} catch { result="FAIL: \(error)" }
try? result.write(to:root.appendingPathComponent("metal-result.txt"),atomically:true,encoding:.utf8)
let application=NSApplication.shared;application.setActivationPolicy(.regular)
let window=NSWindow(contentRect:NSRect(x:0,y:0,width:600,height:130),styleMask:[.titled,.closable],backing:.buffered,defer:false)
window.title="NewPixie Metal check";let label=NSTextField(wrappingLabelWithString:result);label.frame=NSRect(x:20,y:20,width:560,height:90);window.contentView?.addSubview(label);window.makeKeyAndOrderFront(nil);application.run()
