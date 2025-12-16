from PIL import Image
import sys
import os

def convert_to_c_array(input_path, output_path, target_height=60):
    try:
        img = Image.open(input_path)
        
        # Calculate new width maintaining aspect ratio
        aspect_ratio = img.width / img.height
        new_width = int(target_height * aspect_ratio)
        
        img = img.resize((new_width, target_height), Image.Resampling.LANCZOS)
        
        # Convert to RGB565
        # LVGL RGB565 is 16-bit: RRRR RGGG GGGB BBBB
        # But we need bytes. Little Endian usually? Or Big?
        # LV_COLOR_16_SWAP might be needed.
        # Standard: Byte 0: GGG BB BBB, Byte 1: RRR RR GGG
        
        if img.mode != 'RGBA':
            img = img.convert('RGBA')
            
        print(f"Converting image: {new_width}x{target_height} (ARGB8888)")
        
        c_array = []
        for y in range(target_height):
            for x in range(new_width):
                r, g, b, a = img.getpixel((x, y))
                
                # LVGL ARGB8888 typically expects: Blue, Green, Red, Alpha (Little Endian uint32)
                # But as a byte array: [B, G, R, A]
                
                c_array.append(b)
                c_array.append(g)
                c_array.append(r)
                c_array.append(a)
                
        # Generate C File
        with open(output_path, 'w') as f:
            f.write('#include "lvgl.h"\n\n')
            f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n\n')
            
            f.write('const LV_ATTRIBUTE_MEM_ALIGN uint8_t logo_img_map[] = {\n')
            
            # Write data in chunks
            for i in range(0, len(c_array), 16):
                chunk = c_array[i:i+16]
                line = "    " + ", ".join([f"0x{b:02x}" for b in chunk]) + ","
                f.write(line + "\n")
                
            f.write('};\n\n')
            
            f.write('const lv_image_dsc_t logo_img = {\n')
            f.write('  .header.magic = LV_IMAGE_HEADER_MAGIC,\n')
            f.write('  .header.cf = LV_COLOR_FORMAT_ARGB8888,\n') # Updated Format
            f.write('  .header.flags = 0,\n')
            f.write(f'  .header.w = {new_width},\n')
            f.write(f'  .header.h = {target_height},\n')
            f.write(f'  .header.stride = {new_width * 4},\n') # 4 Bytes per pixel
            f.write('  .data_size = sizeof(logo_img_map),\n')
            f.write('  .data = logo_img_map,\n')
            f.write('};\n')
            
        print(f"Successfully generated {output_path}")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    convert_to_c_array(
        "/Users/amuthesan/Documents/Antigravity/ESP32 HMI Unit/BarGauge/asset/unisem_hi_res.png",
        "/Users/amuthesan/Documents/Antigravity/ESP32 HMI Unit/BarGauge/main/logo_img.c",
        target_height=60 # Increased height slightly for better quality
    )
