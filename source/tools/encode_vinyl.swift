// Convert the generated artwork to the device's RGB565 framebuffer format.
import Foundation
import CoreGraphics
import ImageIO

func load(_ path: String) -> CGImage {
    let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: path) as CFURL, nil)!
    return CGImageSourceCreateImageAtIndex(source, 0, nil)!
}

func raster(_ image: CGImage, _ w: Int, _ h: Int) -> [UInt8] {
    var bytes = [UInt8](repeating: 0, count: w * h * 4)
    bytes.withUnsafeMutableBytes { buffer in
        let ctx = CGContext(data: buffer.baseAddress, width: w, height: h,
                            bitsPerComponent: 8, bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        ctx.interpolationQuality = .high
        ctx.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
    }
    return bytes
}

func encode(_ bytes: [UInt8]) -> [UInt16] {
    stride(from: 0, to: bytes.count, by: 4).map { i in
        let alpha = Int(bytes[i + 3])
        guard alpha >= 128 else { return 0 }
        let r = min(255, Int(bytes[i]) * 255 / alpha)
        let g = min(255, Int(bytes[i + 1]) * 255 / alpha)
        let b = min(255, Int(bytes[i + 2]) * 255 / alpha)
        return max(1, UInt16((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)))
    }
}

let args = CommandLine.arguments
precondition(args.count == 3, "encode_vinyl.swift ATLAS OUTPUT")
let atlas = load(args[1])
let width = 196
let height = 158
var header = "#pragma once\n#include <stdint.h>\n\n"
header += "static constexpr int kCatWidth = \(width), kCatHeight = \(height);\n"
header += "static const uint16_t kVinylCats[2][3][kCatWidth * kCatHeight] = {\n"
for row in 0..<2 {
    header += "{\n"
    for column in 0..<3 {
        let x = column * atlas.width / 3
        let y = row * atlas.height / 2
        let w = (column + 1) * atlas.width / 3 - x
        let h = (row + 1) * atlas.height / 2 - y
        let frame = atlas.cropping(to: CGRect(x: x, y: y, width: w, height: h))!
        let pixels = encode(raster(frame, width, height))
        header += "{\n"
        for start in stride(from: 0, to: pixels.count, by: 24) {
            header += pixels[start..<min(start + 24, pixels.count)].map(String.init).joined(separator: ",") + ",\n"
        }
        header += "},\n"
    }
    header += "},\n"
}
header += "};\n"
try header.write(toFile: args[2], atomically: true, encoding: .utf8)
