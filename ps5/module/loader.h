#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace game_module {
using Resolver = void *(*)(const char *);

// Owns the mapped module. It must outlive all guest threads.
class Image {
public:
    Image() = default;
    ~Image();
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    bool load(const char *path, Resolver resolve, std::string &error);
    void *symbol(const char *name, size_t minimum_size = 1) const;

private:
    void *memory_ = nullptr;
    size_t size_ = 0;
    std::string symbols_, strings_;
    std::vector<const void *> frames_;
};
}
