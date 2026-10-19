import UIKit

/// Preserve the reference frontend's screen abstraction while adding native Metal.
/// A shader compilation error remains visible and falls back to the reference bitmap renderer.
final class PortScreenView: UIView {
    private var gpu: CRTView?
    private let bitmap = UIImageView()
    private let errorLabel = UILabel()
    private var frameNumber: Float = 0
    var crt = true { didSet { gpu?.crt = crt } }
    var fill = false { didSet { gpu?.fill = fill; setNeedsLayout() } }
    var aspect = 4.0 / 3.0 { didSet { gpu?.aspect = aspect; setNeedsLayout() } }
    var colorSides = true { didSet { gpu?.colorSides = colorSides } }
    var smooth = false { didSet { gpu?.smooth = smooth } }
    override init(frame: CGRect) {
        super.init(frame:frame)
        backgroundColor = .black
        do {
            let v = try CRTView(native:true); gpu = v; addSubview(v)
        } catch {
            addSubview(bitmap)
            errorLabel.text = "CRT unavailable: \(error.localizedDescription)"
            errorLabel.textColor = .systemYellow; errorLabel.numberOfLines = 2
            errorLabel.font = .systemFont(ofSize:12); addSubview(errorLabel)
        }
        isUserInteractionEnabled = false
    }
    required init?(coder:NSCoder) { fatalError() }
    convenience init() { self.init(frame:.zero) }
    override func layoutSubviews() {
        super.layoutSubviews(); gpu?.frame = bounds
        var size = bounds.size
        if !fill {
            size.width = min(bounds.width,bounds.height * CGFloat(aspect))
            size.height = min(bounds.height,bounds.width / CGFloat(aspect))
        }
        bitmap.frame = CGRect(x:(bounds.width-size.width)/2,y:(bounds.height-size.height)/2,width:size.width,height:size.height)
        errorLabel.frame = CGRect(x:12,y:12,width:bounds.width-24,height:44)
    }
    func upload(_ pixels:UnsafePointer<UInt8>,width:Int,height:Int) {
        frameNumber += 1
        if let gpu {
            let started = CACurrentMediaTime()
            gpu.upload(pixels,width:width,height:height,frameNumber:frameNumber); gpu.draw()
            ht_render_cpu_time((CACurrentMediaTime() - started) * 1000)
            return
        }
        let data = Data(bytes:pixels,count:width*height*4)
        if let provider = CGDataProvider(data:data as CFData),
           let image = CGImage(width:width,height:height,bitsPerComponent:8,bitsPerPixel:32,bytesPerRow:width*4,
             space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGBitmapInfo(rawValue:CGImageAlphaInfo.premultipliedLast.rawValue),
             provider:provider,decode:nil,shouldInterpolate:smooth,intent:.defaultIntent) {
            bitmap.image = UIImage(cgImage:image)
        }
    }
}
