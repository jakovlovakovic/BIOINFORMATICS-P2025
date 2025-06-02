# BambooFilter
This is a project for 2025 course Bioinformatics 1 at Faculty of Electrical Engineering and Computing at Zagreb.
In this project we implement Bamboo Filter and use it to find substrings in E. coli genome sequence.

## How to build and run
Position Yourself in BambooFilter directory. Make sure you have CMake and C++ compiler installed.
```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```
## Example of running the program
Position Yourself in the build directory inside BambooFilter directory
```
./BambooFilter <path_to_config_file> <path_to_string_sequence>
```

## Credits/References
Wang H et al. Bamboo filters: make resizing smooth and adaptive IEEE/ACM Trans. Netw. 2024
https://github.com/wanghanchengchn/bamboofilters/tree/main

Hashing algorithm:
https://github.com/Cyan4973/xxHash
