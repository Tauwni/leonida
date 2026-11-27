// Which pipelines LEONIDA_ASYNC_PIPELINES=1 builds asynchronously by size, LEONIDA_ASYNC_MIN_BYTES
// (runtime/src/async_pipeline_size.h): the threshold's parsing and the decision. Pure stdlib, no Vulkan.
#include "../src/async_pipeline_size.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>

namespace {

int failures = 0;

#define CHECK(condition) do {                                              \
    if (!(condition)) {                                                    \
        std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);  \
        ++failures;                                                        \
    }                                                                      \
} while (0)

using leonida::async_pipelines::asynchronous;
using leonida::async_pipelines::parse_min_bytes;

void parsing() {
    bool valid = false;
    // Unset or empty: no threshold, and nothing to complain about.
    CHECK(parse_min_bytes(nullptr, &valid) == 0 && valid);
    CHECK(parse_min_bytes("", &valid) == 0 && valid);
    CHECK(parse_min_bytes("0", &valid) == 0 && valid);
    CHECK(parse_min_bytes("2097152", &valid) == 2097152 && valid);
    CHECK(parse_min_bytes("1", &valid) == 1 && valid);
    CHECK(parse_min_bytes("18446744073709551615", &valid) == UINT64_MAX && valid);
    CHECK(parse_min_bytes("2097152") == 2097152);  // `valid` is optional
    // Not a whole decimal number of bytes: ignored (0, every pipeline asynchronous) and reported as such.
    for (const char* text : {"2M", "2MiB", "-1", " 1", "1 ", "0x200000", "1.5", "abc", "18446744073709551616",
                             "99999999999999999999"}) {
        valid = true;
        CHECK(parse_min_bytes(text, &valid) == 0);
        CHECK(!valid);
        if (valid) std::fprintf(stderr, "  accepted: \"%s\"\n", text);
    }
}

void decision() {
    constexpr std::uint64_t mib = 1u << 20;
    // Asynchronous builds off: nothing is asynchronous, whatever the threshold.
    for (std::uint64_t threshold : {std::uint64_t{0}, std::uint64_t{1}, 2 * mib})
        for (std::uint64_t bytes : {std::uint64_t{0}, std::uint64_t{4}, 2 * mib - 1, 2 * mib, 26 * mib})
            CHECK(!asynchronous(false, threshold, bytes));
    // On without a threshold (unset, or 0): every pipeline, as before the variable, the smallest included.
    for (std::uint64_t bytes : {std::uint64_t{0}, std::uint64_t{4}, 64 * std::uint64_t{1024}, 2 * mib, 26 * mib})
        CHECK(asynchronous(true, 0, bytes));
    // On with 2 MiB: under it waits, at it and over it is asynchronous.
    CHECK(!asynchronous(true, 2 * mib, 0));
    CHECK(!asynchronous(true, 2 * mib, 4));
    CHECK(!asynchronous(true, 2 * mib, 512 * std::uint64_t{1024}));  // a lighting or post program
    CHECK(!asynchronous(true, 2 * mib, 2 * mib - 1));
    CHECK(asynchronous(true, 2 * mib, 2 * mib));
    CHECK(asynchronous(true, 2 * mib, 26 * mib));  // the largest ray-tracing resolve programs
    // A threshold of 1 byte: as unset for any pipeline with code.
    CHECK(asynchronous(true, 1, 4));
    // A threshold over every pipeline: nothing asynchronous, as with asynchronous builds off.
    CHECK(!asynchronous(true, UINT64_MAX, 26 * mib));
    CHECK(asynchronous(true, UINT64_MAX, UINT64_MAX));
}

}  // namespace

int main() {
    parsing();
    decision();
    if (failures) {
        std::fprintf(stderr, "async_pipeline_size_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("async_pipeline_size_test: ok\n");
    return 0;
}
