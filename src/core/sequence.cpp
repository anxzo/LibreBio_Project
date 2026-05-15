#include "libre_bio/core/sequence.h"

#include <array>
#include <string>
#include <utility>

namespace libre_bio {

namespace {

constexpr std::array<char, 256> MakeDNAComplementTable() noexcept
{
    std::array<char, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = 'N';
    }
    table['A'] = 'T'; table['T'] = 'A';
    table['C'] = 'G'; table['G'] = 'C';
    table['a'] = 't'; table['t'] = 'a';
    table['c'] = 'g'; table['g'] = 'c';
    table['N'] = 'N'; table['n'] = 'n';
    table[' '] = ' ';
    return table;
}

constexpr std::array<char, 256> MakeRNAComplementTable() noexcept
{
    std::array<char, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = 'N';
    }
    table['A'] = 'U'; table['U'] = 'A';
    table['C'] = 'G'; table['G'] = 'C';
    table['a'] = 'u'; table['u'] = 'a';
    table['c'] = 'g'; table['g'] = 'c';
    table['N'] = 'N'; table['n'] = 'n';
    table[' '] = ' ';
    return table;
}

constexpr auto kDNAComplement = MakeDNAComplementTable();
constexpr auto kRNAComplement = MakeRNAComplementTable();

constexpr std::array<bool, 256> MakeValidateTable(const std::string& valid_chars) noexcept
{
    std::array<bool, 256> table{};
    for (const char c : valid_chars) {
        table[static_cast<unsigned char>(c)] = true;
    }
    return table;
}

} // namespace

Sequence::Sequence(std::string id, std::string description,
                   std::string seq, Alphabet alphabet)
    : m_id(std::move(id))
    , m_description(std::move(description))
    , m_seq(std::move(seq))
    , m_alphabet(alphabet)
{
}

const std::string& Sequence::id() const noexcept
{
    return m_id;
}

const std::string& Sequence::description() const noexcept
{
    return m_description;
}

const std::string& Sequence::seq() const noexcept
{
    return m_seq;
}

Alphabet Sequence::alphabet() const noexcept
{
    return m_alphabet;
}

size_t Sequence::length() const noexcept
{
    return m_seq.length();
}

Sequence Sequence::sub_seq(const size_t start, const size_t count) const
{
    if (start >= m_seq.length()) {
        return Sequence(m_id, m_description, std::string{}, m_alphabet);
    }

    const size_t actual_count = (start + count > m_seq.length())
        ? m_seq.length() - start
        : count;

    return Sequence(m_id, m_description,
                    m_seq.substr(start, actual_count),
                    m_alphabet);
}

Sequence Sequence::reverse_complement() const
{
    if (m_alphabet != Alphabet::kDNA && m_alphabet != Alphabet::kRNA) {
        return Sequence(m_id, m_description, m_seq, m_alphabet);
    }

    const auto& table = (m_alphabet == Alphabet::kDNA)
        ? kDNAComplement
        : kRNAComplement;

    std::string result;
    result.reserve(m_seq.length());

    for (auto it = m_seq.rbegin(); it != m_seq.rend(); ++it) {
        result.push_back(table[static_cast<unsigned char>(*it)]);
    }

    return Sequence(m_id,
                    m_description + " reverse complement",
                    std::move(result),
                    m_alphabet);
}

double Sequence::gc_content() const noexcept
{
    if (m_alphabet != Alphabet::kDNA && m_alphabet != Alphabet::kRNA) {
        return 0.0;
    }

    if (m_seq.empty()) {
        return 0.0;
    }

    size_t gc_count = 0;
    for (const char c : m_seq) {
        if (c == 'G' || c == 'C' || c == 'g' || c == 'c') {
            ++gc_count;
        }
    }

    return static_cast<double>(gc_count) / static_cast<double>(m_seq.length());
}

bool Sequence::validate() const noexcept
{
    switch (m_alphabet) {
    case Alphabet::kDNA: {
        static const auto kDNAValid = MakeValidateTable("ACGTNacgtn");
        for (const char c : m_seq) {
            if (!kDNAValid[static_cast<unsigned char>(c)]) {
                return false;
            }
        }
        return true;
    }
    case Alphabet::kRNA: {
        static const auto kRNAValid = MakeValidateTable("ACGUNacgun");
        for (const char c : m_seq) {
            if (!kRNAValid[static_cast<unsigned char>(c)]) {
                return false;
            }
        }
        return true;
    }
    case Alphabet::kProtein: {
        static const auto kProteinValid = MakeValidateTable(
            "ACDEFGHIKLMNPQRSTVWY*acdefghiklmnpqrstvwy*");
        for (const char c : m_seq) {
            if (!kProteinValid[static_cast<unsigned char>(c)]) {
                return false;
            }
        }
        return true;
    }
    }

    return true;
}

} // namespace libre_bio
