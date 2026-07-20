# 1. Compile the code using all CPU cores
cmake --build build -j$(nproc)
# 2. Run the newly compiled version
./build/Amifiles
