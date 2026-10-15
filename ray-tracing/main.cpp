#include "realtime_renderer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int numericValue(const char* text, const char* option) {
    char* end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0') { std::cerr << "Invalid value for " << option << ": " << text << "\n"; std::exit(2); }
    return static_cast<int>(value);
}
void usage() {
    std::cout << "Usage: raytracer [--width N] [--height N] [--samples N] [--spheres N] [--threads N] [--output FILE]\n"
                 "Writes binary PPM (P6) to stdout unless --output is supplied.\n";
}
}

int main(int argc, char** argv) {
    RtSettings settings;
    settings.width = 800; settings.height = 600; settings.samples_per_pixel = 32; settings.sphere_count = 220;
    std::string output_path;
    for (int i = 1; i < argc; ++i) {
        std::string option = argv[i];
        if (option == "--help" || option == "-h") { usage(); return 0; }
        if (i + 1 >= argc) { std::cerr << "Missing value after " << option << "\n"; return 2; }
        const char* argument = argv[++i];
        if (option == "--output") output_path = argument;
        else {
            int value = numericValue(argument, option.c_str());
            if (option == "--width") settings.width = value;
            else if (option == "--height") settings.height = value;
            else if (option == "--samples") settings.samples_per_pixel = std::max(1, value);
            else if (option == "--spheres") settings.sphere_count = value;
            else if (option == "--threads") settings.threads = static_cast<unsigned int>(std::max(1, value));
            else { std::cerr << "Unknown option: " << option << "\n"; usage(); return 2; }
        }
    }
    RtRenderer renderer(settings);
    settings = renderer.settings(); // Use the renderer's safe dimension/sphere bounds in the PPM header.
    renderer.render(RtCamera());
    std::ostream* stream = &std::cout;
    std::ofstream file;
    if (!output_path.empty()) {
        file.open(output_path.c_str(), std::ios::binary);
        if (!file) { std::cerr << "Could not open " << output_path << "\n"; return 1; }
        stream = &file;
    }
    *stream << "P6\n" << settings.width << ' ' << settings.height << "\n255\n";
    stream->write(reinterpret_cast<const char*>(renderer.pixels().data()), static_cast<std::streamsize>(renderer.pixels().size()));
    return stream->good() ? 0 : 1;
}
