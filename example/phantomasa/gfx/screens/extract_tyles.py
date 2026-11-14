import sys
import json
import os
from PIL import Image

def extract_spectrum_data(image_path):
    # Load image and ensure correct dimensions
    img = Image.open(image_path).convert('RGB')
    if img.size != (256, 192):
        print(f"Error: Image {image_path} must be exactly 256x192 pixels. Got {img.size[0]}x{img.size[1]}.")
        sys.exit(1)
        
    pixels = img.load()
    
    unique_tiles = []       # List of unique 8-byte tuples (each tuple is an 8x8 bitmap)
    tile_cache = {}         # Maps 8-byte tuple -> tile_index
    
    nametable = []          # Array of 32x24 tile indices
    attributes = []         # Array of 32x24 attribute bytes
    
    # Process each 8x8 cell in standard 32x24 grid order
    for cell_y in range(24):
        for cell_x in range(32):
            pixel_x = cell_x * 8
            pixel_y = cell_y * 8
            
            # 1. Sample all 64 pixels to find the two dominant colors (Ink vs Paper)
            cell_colors = {}
            for dy in range(8):
                for dx in range(8):
                    color = pixels[pixel_x + dx, pixel_y + dy]
                    cell_colors[color] = cell_colors.get(color, 0) + 1
                    
            # Sort colors by frequency descending
            sorted_colors = sorted(cell_colors.items(), key=lambda x: x[1], reverse=True)
            
            # Default fallback if cell is monolithic/solid color
            primary_color = sorted_colors[0][0]
            secondary_color = sorted_colors[1][0] if len(sorted_colors) > 1 else primary_color
            
            # 2. Convert 8x8 block into a 1-bit bitmap (8 bytes)
            # We assume the less frequent color is Ink (1) and most frequent is Paper (0)
            # Exception: if the whole cell is one color, it becomes all 0s
            tile_bytes = []
            for dy in range(8):
                byte_val = 0
                for dx in range(8):
                    color = pixels[pixel_x + dx, pixel_y + dy]
                    if len(sorted_colors) > 1 and color == secondary_color:
                        byte_val |= (1 << (7 - dx))
                tile_bytes.append(byte_val)
                
            tile_tuple = tuple(tile_bytes)
            
            # 3. Deduplicate tile and add to our tileset bank
            if tile_tuple not in tile_cache:
                tile_index = len(unique_tiles)
                if tile_index >= 256:
                    # A 128x128 image can hold up to 256 8x8 tiles (16 columns x 16 rows)
                    print("Warning: More than 256 unique tiles found! Hard capping at 256.")
                    tile_index = 255
                else:
                    tile_cache[tile_tuple] = tile_index
                    unique_tiles.append(tile_tuple)
            else:
                tile_index = tile_cache[tile_tuple]
                
            nametable.append(tile_index)
            
            # 4. Synthesize approximate ZX Spectrum Attribute Byte (Mental mapping)
            # Standard Speccy Palettes (Approximated BRIGHT 0 vs 1)
            def get_speccy_color_idx(rgb):
                r, g, b = rgb
                # Simple thresholding to guess Spectrum color channels
                r_bit = 1 if r > 100 else 0
                g_bit = 1 if g > 100 else 0
                b_bit = 1 if b > 100 else 0
                # Speccy GRB attribute mapping: bits 0-2 (Ink) or 3-5 (Paper) are encoded as (Blue | Red<<1 | Green<<2)
                return (b_bit) | (r_bit << 1) | (g_bit << 2)

            def is_bright(rgb):
                return 1 if max(rgb) > 200 else 0
            
            paper_idx = get_speccy_color_idx(primary_color)
            ink_idx = get_speccy_color_idx(secondary_color)
            bright_bit = is_bright(primary_color) | is_bright(secondary_color)
            flash_bit = 0 # Cannot be determined from a static PNG
            
            attr_byte = (flash_bit << 7) | (bright_bit << 6) | (paper_idx << 3) | ink_idx
            attributes.append(attr_byte)

    # Output A: Create the 128x128 PNG tileset atlas (Max 256 tiles, 16x16 layout)
    tileset_img = Image.new('1', (128, 128), color=0) # '1' mode is 1-bit black/white
    tileset_pixels = tileset_img.load()
    
    for idx, tile in enumerate(unique_tiles):
        tile_col = idx % 16
        tile_row = idx // 16
        base_x = tile_col * 8
        base_y = tile_row * 8
        
        for dy in range(8):
            byte_val = tile[dy]
            for dx in range(8):
                bit = (byte_val >> (7 - dx)) & 1
                # 1-bit image: 1 is White (Ink), 0 is Black (Paper)
                tileset_pixels[base_x + dx, base_y + dy] = bit

    # Save files matching the input source name
    base_name = os.path.splitext(image_path)[0]
    tileset_png_path = f"{base_name}_tileset.png"
    metadata_json_path = f"{base_name}_data.json"
    
    tileset_img.save(tileset_png_path)
    
    # Output B: Write the nametable map and attributes to JSON
    output_data = {
        "filename": os.path.basename(image_path),
        "unique_tiles_count": len(unique_tiles),
        "nametable_32x24": nametable,
        "attributes_32x24": attributes
    }
    
    with open(metadata_json_path, 'w') as f:
        json.dump(output_data, f, indent=2)
        
    print(f"Successfully processed {image_path}:")
    print(f"  -> Generated Tileset Atlas: {tileset_png_path}")
    print(f"  -> Generated Nametable/Attributes map: {metadata_json_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python extract_tiles.py <path_to_spectrum_screenshot.png>")
        sys.exit(1)
    extract_spectrum_data(sys.argv[1])