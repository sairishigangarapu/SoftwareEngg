// DAY 1 - Trivial test: core module links and is callable.
#include <cassert>
#include <cstdio>
#include "core/transform.hpp"
#include "archive/archive_format.hpp"

int main() {
    using namespace uatc;

    // Verify shared types from the archive layer are visible.
    assert(TransformId::None == TransformId::None);
    assert(CodecId::None == CodecId::None);

    // Verify ByteSpan and ByteBuffer are usable.
    uatc::core::ByteSpan empty;
    assert(empty.empty());
    assert(empty.size == 0);

    uatc::core::ByteBuffer buf{1, 2, 3};
    uatc::core::ByteSpan span(buf);
    assert(span.size == 3);
    assert(span.data != nullptr);

    std::printf("trivial_test: PASS\n");
    return 0;
}
