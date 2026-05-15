#ifndef LIBRE_BIO_CORE_SEQUENCE_H_
#define LIBRE_BIO_CORE_SEQUENCE_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace libre_bio {

enum class Alphabet : uint8_t {
    kDNA     = 0,
    kRNA     = 1,
    kProtein = 2
};

class Sequence {
public:
    Sequence(std::string id, std::string description,
             std::string seq, Alphabet alphabet);

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;
    [[nodiscard]] const std::string& seq() const noexcept;
    [[nodiscard]] Alphabet alphabet() const noexcept;

    [[nodiscard]] size_t length() const noexcept;
    [[nodiscard]] Sequence sub_seq(size_t start, size_t count) const;
    [[nodiscard]] Sequence reverse_complement() const;
    [[nodiscard]] double gc_content() const noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    std::string m_id;
    std::string m_description;
    std::string m_seq;
    Alphabet m_alphabet;
};

} // namespace libre_bio

#endif // LIBRE_BIO_CORE_SEQUENCE_H_
