#include "libre_bio/core/sequence.h"

#include <catch2/catch_test_macros.hpp>

using libre_bio::Alphabet;
using libre_bio::Sequence;

TEST_CASE("TC01: Normal DNA sequence construction", "[sequence]")
{
    const Sequence seq("seq1", "test", "ATCG", Alphabet::kDNA);

    REQUIRE(seq.id() == "seq1");
    REQUIRE(seq.description() == "test");
    REQUIRE(seq.seq() == "ATCG");
    REQUIRE(seq.alphabet() == Alphabet::kDNA);
    REQUIRE(seq.length() == 4);
}

TEST_CASE("TC02: Normal RNA sequence construction", "[sequence]")
{
    const Sequence seq("rna1", "", "AUCG", Alphabet::kRNA);

    REQUIRE(seq.alphabet() == Alphabet::kRNA);
    REQUIRE(seq.length() == 4);
}

TEST_CASE("TC03: Normal Protein sequence construction", "[sequence]")
{
    const Sequence seq("prot1", "enzyme", "MKTGFL", Alphabet::kProtein);

    REQUIRE(seq.alphabet() == Alphabet::kProtein);
    REQUIRE(seq.length() == 6);
}

TEST_CASE("TC04: Empty sequence construction", "[sequence]")
{
    const Sequence seq("empty", "", "", Alphabet::kDNA);

    REQUIRE(seq.length() == 0);
    REQUIRE(seq.gc_content() == 0.0);
}

TEST_CASE("TC05: sub_seq normal extraction", "[sequence]")
{
    const Sequence seq("s", "", "ACGTACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(0, 4);

    REQUIRE(sub.id() == "s");
    REQUIRE(sub.description() == "");
    REQUIRE(sub.seq() == "ACGT");
    REQUIRE(sub.alphabet() == Alphabet::kDNA);
    REQUIRE(sub.length() == 4);
}

TEST_CASE("TC06: sub_seq start out of bounds", "[sequence]")
{
    const Sequence seq("s", "", "ACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(4, 1);

    REQUIRE(sub.length() == 0);
}

TEST_CASE("TC07: sub_seq length exceeds bounds", "[sequence]")
{
    const Sequence seq("s", "", "ACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(2, 10);

    REQUIRE(sub.seq() == "GT");
    REQUIRE(sub.length() == 2);
}

TEST_CASE("TC08: sub_seq count is zero", "[sequence]")
{
    const Sequence seq("s", "", "ACGT", Alphabet::kDNA);
    const Sequence sub = seq.sub_seq(0, 0);

    REQUIRE(sub.length() == 0);
}

TEST_CASE("TC09: DNA reverse complement with space", "[sequence]")
{
    const Sequence seq("s", "", "ATC G", Alphabet::kDNA);
    const Sequence rc = seq.reverse_complement();

    REQUIRE(rc.seq() == "C GAT");
}

TEST_CASE("TC10: RNA reverse complement", "[sequence]")
{
    const Sequence seq("s", "", "AUCG", Alphabet::kRNA);
    const Sequence rc = seq.reverse_complement();

    REQUIRE(rc.seq() == "CGAU");
}

TEST_CASE("TC11: Protein reverse complement returns copy", "[sequence]")
{
    const Sequence seq("s", "", "MKTG", Alphabet::kProtein);
    const Sequence rc = seq.reverse_complement();

    REQUIRE(rc.seq() == "MKTG");
}

TEST_CASE("TC12: gc_content normal", "[sequence]")
{
    const Sequence seq("s", "", "GCGCATAT", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 0.5);
}

TEST_CASE("TC13: gc_content all GC", "[sequence]")
{
    const Sequence seq("s", "", "GGGGCCCC", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 1.0);
}

TEST_CASE("TC14: gc_content no GC", "[sequence]")
{
    const Sequence seq("s", "", "ATATATAT", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 0.0);
}

TEST_CASE("TC15: gc_content mixed case", "[sequence]")
{
    const Sequence seq("s", "", "gcatGCAT", Alphabet::kDNA);

    REQUIRE(seq.gc_content() == 0.5);
}

TEST_CASE("TC16: validate DNA valid sequence", "[sequence]")
{
    const Sequence seq("s", "", "ATCGN", Alphabet::kDNA);

    REQUIRE(seq.validate() == true);
}

TEST_CASE("TC17: validate DNA invalid character", "[sequence]")
{
    const Sequence seq("s", "", "ATCGM", Alphabet::kDNA);

    REQUIRE(seq.validate() == false);
}

TEST_CASE("TC18: validate Protein valid sequence", "[sequence]")
{
    const Sequence seq("s", "", "MKTGFL*", Alphabet::kProtein);

    REQUIRE(seq.validate() == true);
}

TEST_CASE("TC19: Long sequence performance", "[sequence][.perf]")
{
    std::string long_seq(10'000'000, 'A');
    const Sequence seq("long", "", long_seq, Alphabet::kDNA);

    const double gc = seq.gc_content();

    REQUIRE(gc == 0.0);
    REQUIRE(seq.length() == 10'000'000);
    REQUIRE(seq.validate() == true);
}
