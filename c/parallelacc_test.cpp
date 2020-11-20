#include <gmp.h>
#include <iostream>

#include "gtest/gtest.h"
#include "alt_bn128.hpp"
#include "parallelacc.hpp"

using namespace AltBn128;


namespace {

void adderThread(PointParallelProcessor<Curve<RawFq>> *prc, PointParallelProcessor<Curve<RawFq>>::Point *r, u_int64_t n) {
    ParallelAcc< PointParallelProcessor<Curve<RawFq>> > acc;

    for (u_int64_t i=0; i<n; i++) {
      acc.add(*prc, prc->basePoint(0));
    }

    *r=acc.getPoint(*prc);
}


TEST(parallelaccumulator, multiAdd) {
    int N = 1<<30;
    int nThreads = 64;

    G1PointAffine bases[1];

    G1.copy(bases[0], G1.oneAffine());

    static PointParallelProcessor<Curve<RawFq>> prc(G1, nThreads, bases);

    ParallelAcc< PointParallelProcessor<Curve<RawFq>> > acc;

    PointParallelProcessor<Curve<RawFq>>::Point *results = new PointParallelProcessor<Curve<RawFq>>::Point[nThreads];


    std::vector<std::thread> threads(nThreads-1);
    u_int64_t increment = (nThreads >=4) ? N / (nThreads-1) : N/nThreads;
    for (u_int64_t i=0; i<nThreads-1; i++) {
        threads[i] = std::thread (adderThread, &prc, &(results[i]), increment);
    }
    adderThread(&prc, &(results[nThreads-1]), N - (nThreads-1)*increment);
    for (u_int32_t i=0; i<nThreads-1; i++) {
        if (threads[i].joinable()) threads[i].join();
    }
    for (u_int32_t i=0; i<nThreads; i++) {
      acc.add(prc, results[i]);
    }

    delete results;

    auto p = acc.getPoint(prc);

    std::cout << "Finalized\n";
    prc.calculate();

    std::cout << "Joined\n";

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
