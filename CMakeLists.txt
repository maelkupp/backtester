cmake_minimum_required(VERSION 3.10)
project(Backtester CXX)
set(CMAKE_CXX_STANDARD 17)

# Build a static library from math_utils
#add_library(math_utils STATIC src/math_utils.cpp)
#target_include_directories(math_utils PUBLIC include)

# Build the main executable
add_executable(main 
    src/main.cpp
    src/core/data.cpp
    src/core/execution.cpp
    src/core/portfolio.cpp)



# Link the library into the executable
#target_link_libraries(calc PRIVATE math_utils)


#cmake -DCMAKE_BUILD_TYPE=Release ..   # optimised build

#cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..  # see exact compiler commands