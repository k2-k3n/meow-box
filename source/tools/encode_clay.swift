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
precondition(args.count == 5, "encode_clay.swift ROOM CATS OPEN_CATS OUTPUT")
var header = "#pragma once\n#include <stdint.h>\n\n"
func emit(_ name: String, _ pixels: [UInt16]) {
    header += "static const uint16_t \(name)[] = {\n"
    for start in stride(from: 0, to: pixels.count, by: 24) {
        header += pixels[start..<min(start + 24, pixels.count)].map(String.init).joined(separator: ",") + ",\n"
    }
    header += "};\n"
}

let room = load(args[1])
emit("kClayRoom", encode(raster(room, 320, 172)))
let sheet = load(args[2])
let openSheet = load(args[3])
precondition(sheet.width == openSheet.width && sheet.height == openSheet.height)
let sheetPixels = raster(sheet, sheet.width, sheet.height)
let names = ["Kitten", "Tabby", "Lion"]
header += "struct ClayCat { const uint16_t *pixels; int w, h; const uint16_t *mouth; int mx, my, mw, mh; };\n"
var cats = ""
for kind in 0..<3 {
    // The transparent gutters separate the three character silhouettes.
    let boundaries = [0, 33, 67, 100]
    let left = boundaries[kind] * sheet.width / 100
    let right = boundaries[kind + 1] * sheet.width / 100
    var x0 = right, y0 = sheet.height, x1 = left, y1 = 0
    for y in 0..<sheet.height {
        for x in left..<right where sheetPixels[(y * sheet.width + x) * 4 + 3] >= 128 {
            x0 = min(x0, x); y0 = min(y0, y); x1 = max(x1, x + 1); y1 = max(y1, y + 1)
        }
    }
    precondition(x0 < x1 && y0 < y1)
    let crop = sheet.cropping(to: CGRect(x: x0, y: y0, width: x1 - x0, height: y1 - y0))!
    // Match the original kitten, tabby and lion sprite heights.
    let height = [80, 109, 126][kind]
    let width = crop.width * height / crop.height
    let name = "kClay" + names[kind]
    emit(name, encode(raster(crop, width, height)))
    let openCrop = openSheet.cropping(to: CGRect(x: x0, y: y0, width: x1 - x0, height: y1 - y0))!
    let openPixels = encode(raster(openCrop, width, height))
    let mx = [31, 33, 38][kind] * width / 100
    let my = [42, 42, 41][kind] * height / 100
    let mw = [42, 38, 38][kind] * width / 100
    let mh = [19, 21, 24][kind] * height / 100
    let mouth = (my..<(my + mh)).flatMap { y in
        openPixels[(y * width + mx)..<(y * width + mx + mw)]
    }
    emit(name + "Mouth", mouth)
    cats += "    {\(name), \(width), \(height), \(name)Mouth, \(mx), \(my), \(mw), \(mh)},\n"
}
header += "static const ClayCat kClayCats[] = {\n" + cats + "};\n"
try header.write(toFile: args[4], atomically: true, encoding: .utf8)
