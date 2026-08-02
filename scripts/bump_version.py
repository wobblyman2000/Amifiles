#!/usr/bin/env python3
import re
import os

def bump_version():
    cmake_path = "CMakeLists.txt"
    if not os.path.exists(cmake_path):
        print("CMakeLists.txt not found!")
        return

    with open(cmake_path, "r") as f:
        content = f.read()

    # Match project(Amifiles VERSION x.y.z or x.y.z.w LANGUAGES CXX)
    pattern = r"project\(Amifiles VERSION ([\d\.]+) LANGUAGES CXX\)"
    match = re.search(pattern, content)
    if not match:
        print("Version pattern not found in CMakeLists.txt!")
        return

    version_str = match.group(1)
    parts = list(map(int, version_str.split(".")))
    if not parts:
        print("Invalid version format!")
        return

    old_version = ".".join(map(str, parts))
    parts[-1] += 1
    new_version = ".".join(map(str, parts))
    
    new_version_str = f"project(Amifiles VERSION {new_version} LANGUAGES CXX)"
    new_content = re.sub(pattern, new_version_str, content)
    
    with open(cmake_path, "w") as f:
        f.write(new_content)

    print(f"Version bumped from {old_version} to {new_version}")

if __name__ == "__main__":
    bump_version()
