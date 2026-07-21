#!/usr/bin/env python3
import os
import sys
import re
import shutil

def organize_artwork(directory):
    if not os.path.exists(directory):
        print(f"Directory not found: {directory}")
        return

    print(f"Scanning media directory: {directory}")
    
    # 1. Standard artwork mappings for TV shows & Movies
    artwork_patterns = [
        # (Regex pattern, Target standard filename)
        (r'^(?:poster|cover|folder|movie|front|default)[\.\-_]?(?:art|image)?\.(?:jpg|jpeg|png|webp)$', 'poster.jpg'),
        (r'^(?:fanart|backdrop|background|landscape|wall|wallpaper)[\.\-_]?(?:art|image)?\.(?:jpg|jpeg|png|webp)$', 'fanart.jpg'),
        (r'^(?:banner|head|header|title)[\.\-_]?(?:art|image)?\.(?:jpg|jpeg|png|webp)$', 'banner.jpg'),
    ]

    for root, dirs, files in os.walk(directory):
        rel_path = os.path.relpath(root, directory)
        folder_name = os.path.basename(root).lower()
        
        # Detect season folders
        season_match = re.match(r'^season[\s_\-]*([0-9]+)$', folder_name, re.IGNORECASE)
        season_num = season_match.group(1).zfill(2) if season_match else None

        for file in files:
            file_lower = file.lower()
            file_path = os.path.join(root, file)

            # Season artwork check (e.g. season01.jpg, season1.jpg, season-1-poster.jpg)
            s_art_match = re.match(r'^season[\s_\-]*([0-9]+)[\.\-_]?(?:poster|cover|folder)?\.(?:jpg|jpeg|png|webp)$', file_lower, re.IGNORECASE)
            if s_art_match:
                s_num = s_art_match.group(1).zfill(2)
                target_name = f"season{s_num}.jpg"
                if file != target_name and not os.path.exists(os.path.join(root, target_name)):
                    target_path = os.path.join(root, target_name)
                    print(f"Renaming season artwork: {file} -> {target_name}")
                    shutil.move(file_path, target_path)
                    continue

            # Standard artwork check
            for pattern, target_name in artwork_patterns:
                if re.match(pattern, file_lower, re.IGNORECASE):
                    target_path = os.path.join(root, target_name)
                    if file != target_name and not os.path.exists(target_path):
                        print(f"Renaming artwork: {file} -> {target_name} in [{rel_path}]")
                        shutil.move(file_path, target_path)

                    # If this is inside a Season folder, also copy poster.jpg to parent folder as seasonXX.jpg
                    if season_num and target_name == 'poster.jpg':
                        parent_dir = os.path.dirname(root)
                        parent_season_art = os.path.join(parent_dir, f"season{season_num}.jpg")
                        if not os.path.exists(parent_season_art):
                            print(f"Copying season poster to parent TV folder: season{season_num}.jpg")
                            shutil.copy2(target_path, parent_season_art)
                    break

    print("\nArtwork standardization complete!")

if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "."
    organize_artwork(path)
