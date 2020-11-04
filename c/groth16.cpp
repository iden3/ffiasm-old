#include "fft.hpp"

namespace Groth16 {

template <typename Engine>
std::unique_ptr<Prover<Engine>> makeProver(
    u_int32_t nVars, 
    u_int32_t nPublic, 
    u_int32_t domainSize, 
    u_int64_t nCoeffs, 
    void *vk_alpha1,
    void *vk_beta_1,
    void *vk_beta_2,
    void *vk_delta_1,
    void *vk_delta_2,
    void *coefs, 
    void *pointsA, 
    void *pointsB1, 
    void *pointsB2, 
    void *pointsC, 
    void *pointsH
) {
    Prover<Engine> *p = new Prover<Engine>(
        Engine::engine, 
        nVars, 
        nPublic, 
        domainSize, 
        nCoeffs, 
        *(typename Engine::G1PointAffine *)vk_alpha1,
        *(typename Engine::G1PointAffine *)vk_beta_1,
        *(typename Engine::G2PointAffine *)vk_beta_2,
        *(typename Engine::G1PointAffine *)vk_delta_1,
        *(typename Engine::G2PointAffine *)vk_delta_2,
        (Coef<Engine> *)((uint64_t)coefs + 4), 
        (typename Engine::G1PointAffine *)pointsA,
        (typename Engine::G1PointAffine *)pointsB1,
        (typename Engine::G2PointAffine *)pointsB2,
        (typename Engine::G1PointAffine *)pointsC,
        (typename Engine::G1PointAffine *)pointsH
    );
    return std::unique_ptr< Prover<Engine> >(p);
}

template <typename Engine>
std::unique_ptr<Proof<Engine>> Prover<Engine>::prove(typename Engine::FrElement *wtns) {

    auto a = new typename Engine::FrElement[domainSize]();
    auto b = new typename Engine::FrElement[domainSize]();
    auto c = new typename Engine::FrElement[domainSize];

    typename Engine::FrElement aux;

    for (u_int64_t i=0; i<nCoefs; i++) {
        typename Engine::FrElement *ab = (coefs[i].m == 0) ? a : b;

        E.fr.mul(
            aux,
            wtns[coefs[i].s],
            coefs[i].coef
        );

        E.fr.add(
            ab[coefs[i].c],
            ab[coefs[i].c],
            aux
        );
    }

    for (u_int32_t i=0; i<nVars; i++) {
        E.fr.mul(
            c[i],
            a[i],
            b[i]
        );
    }

    FFT<typename Engine::Fr> fft(domainSize*2);
    u_int32_t domainPower = fft.log2(domainSize);


    fft.ifft(a, domainSize, 64);
    cout << E.fr.toString(a[0]) << endl;
    cout << E.fr.toString(a[1]) << endl;
    for (u_int64_t i=0; i<domainSize; i++) {
        E.fr.mul(a[i], a[i], fft.root(domainPower+1, i));
    }
    fft.fft(a, domainSize, 64);

    fft.ifft(b, domainSize, 64);
    for (u_int64_t i=0; i<domainSize; i++) {
        E.fr.mul(b[i], b[i], fft.root(domainPower+1, i));
    }
    fft.fft(b, domainSize, 64);

    fft.ifft(c, domainSize, 64);
    for (u_int64_t i=0; i<domainSize; i++) {
        E.fr.mul(c[i], c[i], fft.root(domainPower+1, i));
    }
    fft.fft(c, domainSize, 64);

    for (u_int64_t i=0; i<domainSize; i++) {
        E.fr.mul(a[i], a[i], b[i]);
        E.fr.sub(a[i], a[i], c[i]);
        E.fr.fromMontgomery(a[i], a[i]);
    }

    delete b;
    delete c;

    typename Engine::G1Point pih;
    E.g1.multiMulByScalar(pih, pointsH, (uint8_t *)a, sizeof(a[0]), domainSize);

    cout << E.g1.toString(pih);

    delete a;

    uint32_t sW = sizeof(wtns[0]);
    typename Engine::G1Point pi_a;
    E.g1.multiMulByScalar(pi_a, pointsA, (uint8_t *)wtns, sW, nVars);

    typename Engine::G1Point pib1;
    E.g1.multiMulByScalar(pib1, pointsB1, (uint8_t *)wtns, sW, nVars);

    typename Engine::G2Point pi_b;
    E.g2.multiMulByScalar(pi_b, pointsB2, (uint8_t *)wtns, sW, nVars);

    typename Engine::G1Point pi_c;
    E.g1.multiMulByScalar(pi_c, pointsC, (uint8_t *)((uint64_t)wtns + (nPublic +1)*sW), sW, nVars-nPublic-1);

    typename Engine::FrElement r;
    typename Engine::FrElement s;
    typename Engine::FrElement rs;

    // TODO set to random
    E.fr.copy(r, E.fr.zero());
    E.fr.copy(s, E.fr.zero());

    typename Engine::G1Point p1;
    typename Engine::G2Point p2;

    E.g1.add(pi_a, pi_a, vk_alpha1);
    E.g1.mulByScalar(p1, vk_delta1, (uint8_t *)&r, sizeof(r));
    E.g1.add(pi_a, pi_a, p1);

    E.g2.add(pi_b, pi_b, vk_beta2);
    E.g2.mulByScalar(p2, vk_delta2, (uint8_t *)&s, sizeof(s));
    E.g2.add(pi_b, pi_b, p2);

    E.g1.add(pib1, pib1, vk_beta1);
    E.g1.mulByScalar(p1, vk_delta1, (uint8_t *)&s, sizeof(s));
    E.g1.add(pib1, pib1, p1);

    E.g1.add(pi_c, pi_c, pih);

    E.g1.mulByScalar(p1, pi_a, (uint8_t *)&s, sizeof(s));
    E.g1.add(pi_c, pi_c, p1);

    E.g1.mulByScalar(p1, pib1, (uint8_t *)&r, sizeof(r));
    E.g1.add(pi_c, pi_c, p1);

    E.fr.mul(rs, r, s);
    E.fr.toMontgomery(rs, rs);

    E.g1.mulByScalar(p1, vk_delta1, (uint8_t *)&rs, sizeof(rs));
    E.g1.add(pi_c, pi_c, p1);

    Proof<Engine> *p = new Proof<Engine>(Engine::engine);
    E.g1.copy(p->A, pi_a);
    E.g2.copy(p->B, pi_b);
    E.g1.copy(p->C, pi_c);

    return std::unique_ptr<Proof<Engine>>(p);
}

template <typename Engine>
std::string Proof<Engine>::toJson() {

    std::ostringstream ss;
    ss << "{ A:[\"" << E.f1.toString(A.x) << "\",\"" << E.f1.toString(A.y) << "\",\"1\"], ";
    ss << " B: [[\"" << E.f1.toString(B.x.a) << "\",\"" << E.f1.toString(B.x.b) << "\"],[\"" << E.f1.toString(B.y.a) << "\",\"" << E.f1.toString(B.y.b) << "\", [\"1\",\"0\"]], ";
    ss << " C: [\"" << E.f1.toString(C.x) << "\",\"" << E.f1.toString(C.y) << "\",\"1\"] }";
    
    return ss.str();
}

} // namespace