import os
import zlib
import struct

def write_minimal_png(filename, color_rgb):
    # PNG signature
    png = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk
    ihdr_data = struct.pack('>IIBBBBB', 8, 8, 8, 2, 0, 0, 0)
    ihdr = b'IHDR' + ihdr_data
    png += struct.pack('>I', len(ihdr_data)) + ihdr + struct.pack('>I', zlib.crc32(ihdr))

    # IDAT chunk (raw rgb data compressed)
    raw_data = b''
    for y in range(8):
        raw_data += b'\x00' # filter type 0
        raw_data += color_rgb * 8
    idat_data = zlib.compress(raw_data)
    idat = b'IDAT' + idat_data
    png += struct.pack('>I', len(idat_data)) + idat + struct.pack('>I', zlib.crc32(idat))

    # IEND chunk
    iend = b'IEND'
    png += struct.pack('>I', 0) + iend + struct.pack('>I', zlib.crc32(iend))

    with open(filename, 'wb') as f:
        f.write(png)

def main():
    test_dir = "/home/dave/cpp_projects/Amifiles/casing_test"
    os.makedirs(test_dir, exist_ok=True)
    
    # 1. CD Folder Check
    cd_dir = os.path.join(test_dir, "CdAlbumFolder")
    os.makedirs(cd_dir, exist_ok=True)
    write_minimal_png(os.path.join(cd_dir, "folder.png"), b'\xff\x00\x00') # Red cover
    write_minimal_png(os.path.join(cd_dir, "cover.jpg"), b'\xff\x00\x00')
    
    # 2. DVD Folder Check
    dvd_dir = os.path.join(test_dir, "DvdMovieFolder")
    os.makedirs(dvd_dir, exist_ok=True)
    write_minimal_png(os.path.join(dvd_dir, "movie.png"), b'\x00\xff\x00') # Green cover
    write_minimal_png(os.path.join(dvd_dir, "poster.jpg"), b'\x00\xff\x00')
    
    # 3. Blu-ray Folder Check
    br_dir = os.path.join(test_dir, "BlurayMovieFolder")
    os.makedirs(br_dir, exist_ok=True)
    write_minimal_png(os.path.join(br_dir, "bluray.png"), b'\x00\x00\xff') # Blue cover
    write_minimal_png(os.path.join(br_dir, "bluray_cover.jpg"), b'\x00\x00\xff')

    # 4. Media Files specific checks
    open(os.path.join(test_dir, "song_track.mp3"), "w").close()
    write_minimal_png(os.path.join(test_dir, "song_track.png"), b'\xff\xff\x00') # Yellow cover
    
    open(os.path.join(test_dir, "dvd_movie.mp4"), "w").close()
    write_minimal_png(os.path.join(test_dir, "dvd_movie.jpg"), b'\x00\xff\xff') # Cyan cover
    
    open(os.path.join(test_dir, "bluray_movie.mkv"), "w").close()
    write_minimal_png(os.path.join(test_dir, "bluray_movie.jpg"), b'\xff\x00\xff') # Magenta cover

    print("Test casing folders and files generated successfully at:", test_dir)

if __name__ == "__main__":
    main()
