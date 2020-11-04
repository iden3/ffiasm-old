#include <gmp.h>
#include <iostream>

#include "gtest/gtest.h"
#include "alt_bn128.hpp"
#include "parallelacc.hpp"

using namespace AltBn128;

namespace {

TEST(parallelaccumulator, multiAdd) {

    int N = 1<<27;

    G1PointAffine bases[1];

    G1.copy(bases[0], G1.oneAffine());

    static PointParallelProcessor<Curve<RawFq>> prc(G1, 1, bases);
    ParallelAcc< PointParallelProcessor<Curve<RawFq>> > acc;

    for (int i=0; i<N; i++) acc.add(prc, prc.basePoint(0));

    auto p = acc.getPoint(prc);

    std::cout << "Finalized";
    prc.terminateCalculus();

    G1Point res1;
    prc.extractResult(res1, p);

    G1Point res2;
    G1.mulByScalar(res2, G1.one(), (uint8_t *)&N, sizeof(N));

    ASSERT_TRUE(G1.eq(res1, res2));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
