
import sys
import os
try:
    from PIL import Image
except ImportError:
    print("Pillow not installed. Installing...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
    from PIL import Image

def main():
    input_path = "asset/logo.png"
    output_path = "main/logo_img.c"
    
    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found.")
        return

    img = Image.open(input_path)
    print(f"Original size: {img.size}")

    # Resize to 70% of 800px width = 560px
    target_width = 560
    w_percent = (target_width / float(img.size[0]))
    target_height = int((float(img.size[1]) * float(w_percent)))
    
    img = img.resize((target_width, target_height), Image.Resampling.LANCZOS)
    print(f"Resized to: {img.size}")
    
    # Ensure RGB
    img = img.convert("RGB")

    # Generate C Array
    with open(output_path, "w") as f:
        f.write("#include \"lvgl.h\"\n\n")
        f.write("#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#define LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#endif\n\n")
        
        f.write(f"#ifndef LV_ATTRIBUTE_IMG_LOGO_IMG\n")
        f.write(f"#define LV_ATTRIBUTE_IMG_LOGO_IMG\n")
        f.write(f"#endif\n\n")

        f.write("const LV_ATTRIBUTE_MEM_ALIGN uint8_t logo_img_map[] = {\n")
        
        data = img.getdata()
        
        # Header (LVGL Image Header)
        # We will use LV_IMG_CF_TRUE_COLOR (RGB565 equivalent usually)
        # Actually LVGL v8/v9 defines specific headers.
        # But `lv_img_dsc_t` expects a pointer to data.
        # The data format depends on CF.
        # CF_TRUE_COLOR in LVGL typically means native color format.
        # For ESP32 with 16-bit color, it's usually RGB565.
        
        # Let's convert to RGB565 manually to be safe and platform independent-ish
        # RGB565: RRRRRGGG GGGBBBBB
        
        line_buffer = ""
        count = 0
        for r, g, b in data:
            # RGB888 -> RGB565
            val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            # Little Endian for ESP32/LVGL usually? 
            # LVGL usually wants bytes: Low, High or High, Low?
            # It depends on LV_COLOR_16_SWAP.
            # Assuming standard:
            low_byte = val & 0xFF
            high_byte = (val >> 8) & 0xFF
            
            # Note: If LV_COLOR_16_SWAP is 1, bytes are swapped. 
            # Most SPI displays use big endian or swapped.
            # Let's write them as 2 bytes.
            # Usually strict standard is Little Endian for raw arrays if directly mapped?
            # Let's try standard Little Endian: Low, High.
            
            line_buffer += f"0x{low_byte:02x}, 0x{high_byte:02x}, "
            count += 1
            if count % 16 == 0:
                f.write("    " + line_buffer + "\n")
                line_buffer = ""
        
        if line_buffer:
            f.write("    " + line_buffer + "\n")
            
        f.write("};\n\n")
        
        # LVGL v9 Image Descriptor
        f.write("const lv_image_dsc_t logo_img = {\n")
        f.write("  .header.magic = LV_IMAGE_HEADER_MAGIC,\n")
        f.write("  .header.cf = LV_COLOR_FORMAT_RGB565,\n")
        f.write("  .header.flags = 0,\n")
        f.write(f"  .header.w = {img.size[0]},\n")
        f.write(f"  .header.h = {img.size[1]},\n")
        f.write(f"  .header.stride = {img.size[0] * 2},\n")
        f.write("  .data_size = sizeof(logo_img_map),\n")
        f.write("  .data = logo_img_map,\n")
        f.write("};\n")

if __name__ == "__main__":
    main()
