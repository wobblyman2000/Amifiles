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

    # Match project(Amifiles VERSION x.y.z LANGUAGES CXX)
    pattern = r"project\(Amifiles VERSION (\d+)\.(\d+)\.(\d+) LANGUAGES CXX\)"
    match = re.search(pattern, content)
    if not match:
        print("Version pattern not found in CMakeLists.txt!")
        return

    major, minor, patch = map(int, match.groups())
    new_patch = patch + 1
    new_version_str = f"project(Amifiles VERSION {major}.{minor}.{new_patch} LANGUAGES CXX)"
    
    new_content = re.sub(pattern, new_version_str, content)
    
    with open(cmake_path, "w") as f:
        f.write(new_content)

    print(f"Version bumped from {major}.{minor}.{patch} to {major}.{minor}.{new_patch}")

if __name__ == "__main__":
    bump_version()
