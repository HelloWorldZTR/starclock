from PIL import Image
import os

input_folder = "pics"
output_h_file = "pics.h"

# def rgb888_to_rgb565(r, g, b):
#     return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def rgb888_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


with open(output_h_file, "w") as f_h:
    f_h.write("// Auto-generated RGB565 images\n")
    f_h.write("#include <Arduino.h>\n\n")  # 包含 PROGMEM 定义，适用于 Arduino/ESP32

    for filename in os.listdir(input_folder):
        if filename.lower().endswith(".png"):
            img_path = os.path.join(input_folder, filename)
            img = Image.open(img_path)


            # 放大两倍
            img_resized = img.resize((80, 60), Image.NEAREST)
            img_rgb = img_resized.convert("RGB")

            array_name = os.path.splitext(filename)[0].replace("-", "_").replace(" ", "_")

            f_h.write(f"// Image: {filename}\n")
            f_h.write(f"const uint16_t {array_name}[80*60] PROGMEM = {{\n    ")

            count = 0
            for y in range(img_rgb.height):
                for x in range(img_rgb.width):
                    r, g, b = img_rgb.getpixel((x, y))
                    rgb565 = rgb888_to_rgb565(r, g, b)
                    f_h.write(f"0x{rgb565:04X}, ")
                    count += 1
                    if count % 12 == 0:
                        f_h.write("\n    ")

            f_h.write("\n};\n\n")

            print(f"{filename} 已加入到 {output_h_file}")

print("所有图片处理完成，生成合并的 .h 文件。")
