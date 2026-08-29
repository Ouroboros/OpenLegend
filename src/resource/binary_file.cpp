#include "openlegend/resource/binary_file.hpp"

#include <fstream>
#include <limits>
#include <utility>

namespace openlegend::resource {

BinaryFile read_binary_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return {{}, "unable to open file: " + path.string()};
    }

    const auto end = stream.tellg();
    if (end < 0 || static_cast<unsigned long long>(end) > std::numeric_limits<std::size_t>::max()) {
        return {{}, "invalid file size: " + path.string()};
    }

    BinaryFile result;
    result.bytes.resize(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!result.bytes.empty() &&
        !stream.read(reinterpret_cast<char*>(result.bytes.data()), static_cast<std::streamsize>(result.bytes.size()))) {
        return {{}, "unable to read complete file: " + path.string()};
    }
    return result;
}

BinaryFile DataRoot::read(const std::filesystem::path& relative) const {
    if (relative.is_absolute()) {
        return {{}, "data-root reads require a relative path"};
    }
    const auto candidate = (root_ / relative).lexically_normal();
    return read_binary_file(candidate);
}

}  // namespace openlegend::resource
