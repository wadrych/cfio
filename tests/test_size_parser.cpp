/// @file test_size_parser.cpp
/// @brief Unit tests for the SizeParser utility.

#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "config/size_parser.h"

namespace cfio {
namespace {

using Allowed = SizeParser::AllowedSuffixes;

// --- Parse: valid inputs with default kKMG ---

TEST(SizeParserParse, KiloLower) {
  EXPECT_EQ(SizeParser::Parse("4k"), 4096U);
}

TEST(SizeParserParse, KiloUpper) {
  EXPECT_EQ(SizeParser::Parse("4K"), 4096U);
}

TEST(SizeParserParse, MegaLower) {
  EXPECT_EQ(SizeParser::Parse("1m"), 1048576U);
}

TEST(SizeParserParse, MegaUpper) {
  EXPECT_EQ(SizeParser::Parse("1M"), 1048576U);
}

TEST(SizeParserParse, GigaLower) {
  EXPECT_EQ(SizeParser::Parse("1g"), 1073741824U);
}

TEST(SizeParserParse, GigaUpper) {
  EXPECT_EQ(SizeParser::Parse("1G"), 1073741824U);
}

TEST(SizeParserParse, NoSuffix) {
  EXPECT_EQ(SizeParser::Parse("4096"), 4096U);
}

TEST(SizeParserParse, NoSuffixSmall) {
  EXPECT_EQ(SizeParser::Parse("512"), 512U);
}

TEST(SizeParserParse, NoSuffixOne) {
  EXPECT_EQ(SizeParser::Parse("1"), 1U);
}

TEST(SizeParserParse, LargerKilo) {
  EXPECT_EQ(SizeParser::Parse("128k"), 131072U);
}

TEST(SizeParserParse, LargerMega) {
  EXPECT_EQ(SizeParser::Parse("64m"), 67108864U);
}

TEST(SizeParserParse, LargerGiga) {
  EXPECT_EQ(SizeParser::Parse("2g"), 2147483648U);
}

// --- Parse: zero values ---

TEST(SizeParserParse, ZeroNoSuffix) {
  EXPECT_EQ(SizeParser::Parse("0"), 0U);
}

TEST(SizeParserParse, ZeroWithK) {
  EXPECT_EQ(SizeParser::Parse("0k"), 0U);
}

TEST(SizeParserParse, ZeroWithM) {
  EXPECT_EQ(SizeParser::Parse("0m"), 0U);
}

TEST(SizeParserParse, ZeroWithG) {
  EXPECT_EQ(SizeParser::Parse("0g"), 0U);
}

// --- Parse: invalid inputs ---

TEST(SizeParserParse, EmptyString) {
  EXPECT_THROW(SizeParser::Parse(""), std::invalid_argument);
}

TEST(SizeParserParse, NoDigits) {
  EXPECT_THROW(SizeParser::Parse("k"), std::invalid_argument);
}

TEST(SizeParserParse, UnknownSuffix) {
  EXPECT_THROW(SizeParser::Parse("4x"), std::invalid_argument);
}

TEST(SizeParserParse, DoubleSuffix) {
  EXPECT_THROW(SizeParser::Parse("4kk"), std::invalid_argument);
}

TEST(SizeParserParse, NegativeWithSuffix) {
  EXPECT_THROW(SizeParser::Parse("-1k"), std::invalid_argument);
}

TEST(SizeParserParse, NegativeNoSuffix) {
  EXPECT_THROW(SizeParser::Parse("-1"), std::invalid_argument);
}

TEST(SizeParserParse, NonNumeric) {
  EXPECT_THROW(SizeParser::Parse("abc"), std::invalid_argument);
}

TEST(SizeParserParse, LeadingSpaceWithSuffix) {
  EXPECT_THROW(SizeParser::Parse(" 4k"), std::invalid_argument);
}

TEST(SizeParserParse, LeadingSpaceNoSuffix) {
  EXPECT_THROW(SizeParser::Parse(" 4"), std::invalid_argument);
}

// --- Parse: suffix restriction (kKM only) ---

TEST(SizeParserParse, KM_AllowsKilo) {
  EXPECT_EQ(SizeParser::Parse("4k", Allowed::kKM), 4096U);
}

TEST(SizeParserParse, KM_AllowsMega) {
  EXPECT_EQ(SizeParser::Parse("1m", Allowed::kKM), 1048576U);
}

TEST(SizeParserParse, KM_RejectsGigaLower) {
  EXPECT_THROW(SizeParser::Parse("1g", Allowed::kKM), std::invalid_argument);
}

TEST(SizeParserParse, KM_RejectsGigaUpper) {
  EXPECT_THROW(SizeParser::Parse("1G", Allowed::kKM), std::invalid_argument);
}

// --- Parse: overflow ---

TEST(SizeParserParse, OverflowWithSuffix) {
  EXPECT_THROW(SizeParser::Parse("17179869185g"), std::invalid_argument);
}

TEST(SizeParserParse, OverflowNoSuffix) {
  EXPECT_THROW(SizeParser::Parse("18446744073709551616"), std::invalid_argument);
}

TEST(SizeParserParse, BoundaryLargeValid) {
  // 16G = 17179869184 bytes — fits in size_t on 64-bit.
  EXPECT_EQ(SizeParser::Parse("16g"), 17179869184ULL);
}

// --- Format ---

TEST(SizeParserFormat, Zero) {
  EXPECT_EQ(SizeParser::Format(0), "0");
}

TEST(SizeParserFormat, ExactKilo) {
  EXPECT_EQ(SizeParser::Format(4096), "4K");
}

TEST(SizeParserFormat, ExactMega) {
  EXPECT_EQ(SizeParser::Format(1048576), "1M");
}

TEST(SizeParserFormat, ExactGiga) {
  EXPECT_EQ(SizeParser::Format(1073741824), "1G");
}

TEST(SizeParserFormat, NotDivisible) {
  EXPECT_EQ(SizeParser::Format(512), "512");
}

TEST(SizeParserFormat, NotDivisibleOdd) {
  EXPECT_EQ(SizeParser::Format(5000), "5000");
}

TEST(SizeParserFormat, LargerKilo) {
  EXPECT_EQ(SizeParser::Format(131072), "128K");
}

// --- Roundtrip ---

TEST(SizeParserRoundtrip, KiloRoundtrip) {
  EXPECT_EQ(SizeParser::Format(SizeParser::Parse("4k")), "4K");
}

TEST(SizeParserRoundtrip, GigaRoundtrip) {
  EXPECT_EQ(SizeParser::Format(SizeParser::Parse("1G")), "1G");
}

TEST(SizeParserRoundtrip, FormatThenParse) {
  EXPECT_EQ(SizeParser::Parse(SizeParser::Format(4096)), 4096U);
}

}  // namespace
}  // namespace cfio
