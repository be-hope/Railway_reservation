// console_io.h
// Line-based console input/output for the menus (component C1).
// Input is read with std::getline into std::string and length-checked; no
// fixed-size C buffers (SAD §3.9.5). Streams are injected so the menus can be
// driven by scripted input in tests (USB-01, ST-BKG-01).

#pragma once

#include <iosfwd>
#include <optional>
#include <string>

namespace rrs {

class ConsoleIO {
public:
    static constexpr std::size_t kMaxLineLength = 200;

    ConsoleIO(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    std::ostream& out() { return out_; }

    // Prints the prompt and reads one trimmed line. nullopt on end of input.
    // Lines longer than kMaxLineLength are rejected and asked for again.
    std::optional<std::string> readLine(const std::string& prompt);
    // Re-asks until an integer in [min, max] is entered; nullopt on end of input.
    std::optional<int> readInt(const std::string& prompt, int min, int max);
    // Y/N question; nullopt on end of input.
    std::optional<bool> readYesNo(const std::string& prompt);

    void ok(const std::string& message);            // "OK: <message>"
    void error(const std::string& formattedError);  // already "ERROR [Exxxx]: ..."
    void line(const std::string& text = "");
    void rule();                                    // 58 '=' characters

private:
    std::istream& in_;
    std::ostream& out_;
};

// Upper-cases ASCII letters (station codes, class codes, Y/N answers).
std::string toUpperAscii(std::string s);

}  // namespace rrs
