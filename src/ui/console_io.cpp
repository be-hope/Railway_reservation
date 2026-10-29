// console_io.cpp

#include "ui/console_io.h"

#include <cctype>
#include <istream>
#include <ostream>

namespace rrs {

namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

}  // namespace

std::string toUpperAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::optional<std::string> ConsoleIO::readLine(const std::string& prompt) {
    while (true) {
        out_ << prompt << std::flush;
        std::string raw;
        if (!std::getline(in_, raw)) {
            out_ << '\n';
            return std::nullopt;
        }
        if (raw.size() > kMaxLineLength) {
            out_ << "Invalid input: at most " << kMaxLineLength << " characters. Please re-enter.\n";
            continue;
        }
        return trim(raw);
    }
}

std::optional<int> ConsoleIO::readInt(const std::string& prompt, int min, int max) {
    while (true) {
        auto text = readLine(prompt);
        if (!text) return std::nullopt;
        bool digits = !text->empty() && text->size() <= 9;
        for (char c : *text) {
            if (!std::isdigit(static_cast<unsigned char>(c))) digits = false;
        }
        if (digits) {
            int value = std::stoi(*text);
            if (value >= min && value <= max) return value;
        }
        out_ << "Invalid choice. Enter a number from " << min << " to " << max << ".\n";
    }
}

std::optional<bool> ConsoleIO::readYesNo(const std::string& prompt) {
    while (true) {
        auto text = readLine(prompt);
        if (!text) return std::nullopt;
        std::string answer = toUpperAscii(*text);
        if (answer == "Y" || answer == "YES") return true;
        if (answer == "N" || answer == "NO") return false;
        out_ << "Invalid choice. Enter Y or N.\n";
    }
}

void ConsoleIO::ok(const std::string& message) { out_ << "OK: " << message << '\n'; }

void ConsoleIO::error(const std::string& formattedError) { out_ << formattedError << '\n'; }

void ConsoleIO::line(const std::string& text) { out_ << text << '\n'; }

void ConsoleIO::rule() { out_ << std::string(58, '=') << '\n'; }

}  // namespace rrs
