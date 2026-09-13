#include <chrono>
#include <cstdint>

#include "openlegend/motion/authoritative_motion.hpp"
#include "test_support.hpp"

namespace {

using openlegend::motion::AuthoritativeMotion;
using openlegend::motion::FixedPosition;
using openlegend::motion::GridPosition;
using openlegend::motion::MotionAdvanceResult;
using openlegend::motion::kFixedUnitsPerGridUnit;
using namespace std::chrono_literals;

void run_start_contract_tests() {
    AuthoritativeMotion motion;
    OL_CHECK(!motion.active());
    OL_CHECK(!motion.begin({1, 2, 3}, {2, 2, 3}, 0ns));
    OL_CHECK(!motion.begin({1, 2, 3}, {1, 2, 3}, 100ns));
    OL_CHECK(!motion.begin({0, 0, 0}, {1'025, 0, 0}, 100ns));
    OL_CHECK(!motion.begin({0, 0, 0}, {1, 0, 0}, 61s));

    OL_CHECK(motion.begin({4, 5, 6}, {5, 4, 8}, 100ns));
    OL_CHECK(motion.active());
    OL_CHECK((motion.source() == GridPosition{4, 5, 6}));
    OL_CHECK((motion.destination() == GridPosition{5, 4, 8}));
    OL_CHECK((motion.position() == FixedPosition{
        4 * kFixedUnitsPerGridUnit,
        5 * kFixedUnitsPerGridUnit,
        6 * kFixedUnitsPerGridUnit,
    }));
    OL_CHECK(!motion.begin({0, 0, 0}, {1, 0, 0}, 100ns));
}

void run_absolute_position_tests() {
    AuthoritativeMotion motion;
    OL_CHECK(motion.begin({5, -2, 4}, {4, -1, 6}, 100ns));

    OL_CHECK(motion.advance(25ns) == MotionAdvanceResult{});
    OL_CHECK((motion.position() == FixedPosition{
        5 * kFixedUnitsPerGridUnit - kFixedUnitsPerGridUnit / 4,
        -2 * kFixedUnitsPerGridUnit + kFixedUnitsPerGridUnit / 4,
        4 * kFixedUnitsPerGridUnit + kFixedUnitsPerGridUnit / 2,
    }));

    OL_CHECK(motion.advance(25ns) == MotionAdvanceResult{});
    OL_CHECK((motion.position() == FixedPosition{
        5 * kFixedUnitsPerGridUnit - kFixedUnitsPerGridUnit / 2,
        -2 * kFixedUnitsPerGridUnit + kFixedUnitsPerGridUnit / 2,
        5 * kFixedUnitsPerGridUnit,
    }));
    OL_CHECK(motion.elapsed() == 50ns);
}

void run_rounding_and_chunking_tests() {
    AuthoritativeMotion single;
    AuthoritativeMotion chunked;
    OL_CHECK(single.begin({0, 0, 0}, {1, -1, 0}, 3ns));
    OL_CHECK(chunked.begin({0, 0, 0}, {1, -1, 0}, 3ns));

    OL_CHECK(single.advance(2ns) == MotionAdvanceResult{});
    OL_CHECK(chunked.advance(1ns) == MotionAdvanceResult{});
    OL_CHECK(chunked.advance(1ns) == MotionAdvanceResult{});
    OL_CHECK((single.position() == FixedPosition{43'691, -43'691, 0}));
    OL_CHECK(chunked.position() == single.position());
}

void run_endpoint_tests() {
    AuthoritativeMotion motion;
    OL_CHECK(motion.begin({10, 20, 3}, {11, 20, 2}, 100ns));

    OL_CHECK(motion.advance(-1ns) == MotionAdvanceResult{});
    OL_CHECK(motion.elapsed() == 0ns);
    OL_CHECK(motion.advance(40ns) == MotionAdvanceResult{});
    OL_CHECK((motion.advance(75ns) == MotionAdvanceResult{true, 15ns}));
    OL_CHECK(!motion.active());
    OL_CHECK(motion.elapsed() == 100ns);
    OL_CHECK((motion.position() == FixedPosition{
        11 * kFixedUnitsPerGridUnit,
        20 * kFixedUnitsPerGridUnit,
        2 * kFixedUnitsPerGridUnit,
    }));
    OL_CHECK((motion.advance(7ns) == MotionAdvanceResult{false, 7ns}));

    motion.clear();
    OL_CHECK(!motion.active());
    OL_CHECK((motion.position() == FixedPosition{}));
    OL_CHECK(motion.elapsed() == 0ns);
    OL_CHECK(motion.duration() == 0ns);
}

}  // namespace

int main() {
    run_start_contract_tests();
    run_absolute_position_tests();
    run_rounding_and_chunking_tests();
    run_endpoint_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
