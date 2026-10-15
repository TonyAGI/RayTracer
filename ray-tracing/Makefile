CXX := clang++
# This CLT installation stores libc++ inside the selected SDK, not beside clang.
# Passing it explicitly prevents the "<algorithm> file not found" failure.
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
CXXFLAGS := -O3 -std=c++17 -Wall -Wextra -pthread -isysroot $(SDKROOT) -isystem $(SDKROOT)/usr/include/c++/v1
.PHONY: all live benchmark clean
all: raytracer
raytracer: main.cpp realtime_renderer.h
	$(CXX) $(CXXFLAGS) main.cpp -o $@
live: interactive.mm realtime_renderer.h
	clang++ $(CXXFLAGS) -fobjc-arc interactive.mm -framework Cocoa -o raytracer-live

# Stable settings make comparisons between worker counts and code changes useful.
benchmark: raytracer
	@time -p ./raytracer --width 640 --height 420 --samples 8 --spheres 120 --output /private/tmp/raytracer-benchmark.ppm
	@rm -f /private/tmp/raytracer-benchmark.ppm

clean:
	rm -f raytracer raytracer-live
