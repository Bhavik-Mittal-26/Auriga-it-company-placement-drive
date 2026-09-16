#include "price_importer.h"

#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cinema {
namespace {

constexpr const char* kRupeeSymbol = "₹";

std::string trim(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string unquote(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return trim(value.substr(1, value.size() - 2));
    }
    return value;
}

bool split_record(const std::string& line, std::string& seat_class, std::string& price) {
    const std::size_t separator = line.find(',');
    if (separator == std::string::npos || line.find(',', separator + 1) != std::string::npos) {
        return false;
    }
    seat_class = trim(line.substr(0, separator));
    price = trim(line.substr(separator + 1));
    return true;
}

bool parse_price_paisa(std::string value, Paisa& result, std::string& reason) {
    value = unquote(value);
    if (value.rfind(kRupeeSymbol, 0) == 0) {
        value = trim(value.substr(std::string(kRupeeSymbol).size()));
    }
    if (value.empty()) {
        reason = "Missing price";
        return false;
    }
    if (value.front() == '-') {
        reason = "Negative price";
        return false;
    }
    if (value.front() == '+') {
        value = value.substr(1);
    }
    if (value.empty()) {
        reason = "Malformed price";
        return false;
    }

    const std::size_t decimal = value.find('.');
    if (decimal != std::string::npos && value.find('.', decimal + 1) != std::string::npos) {
        reason = "Malformed price";
        return false;
    }
    const std::string whole = decimal == std::string::npos ? value : value.substr(0, decimal);
    const std::string fraction = decimal == std::string::npos ? "" : value.substr(decimal + 1);
    if (whole.empty() && fraction.empty()) {
        reason = "Malformed price";
        return false;
    }
    if (fraction.size() > 2 || (decimal != std::string::npos && fraction.empty())) {
        reason = "Price may have at most two decimal places";
        return false;
    }
    if (whole.empty() && decimal == std::string::npos) {
        reason = "Malformed price";
        return false;
    }
    for (char character : whole) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            reason = "Malformed price";
            return false;
        }
    }
    for (char character : fraction) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            reason = "Malformed price";
            return false;
        }
    }

    Paisa rupees = 0;
    for (char character : whole) {
        const int digit = character - '0';
        if (rupees > (std::numeric_limits<Paisa>::max() - digit) / 10) {
            reason = "Price is too large";
            return false;
        }
        rupees = rupees * 10 + digit;
    }
    Paisa paisa = fraction.empty() ? 0 : (fraction[0] - '0') * 10;
    if (fraction.size() == 2) {
        paisa += fraction[1] - '0';
    }
    if (rupees > (std::numeric_limits<Paisa>::max() - paisa) / 100) {
        reason = "Price is too large";
        return false;
    }
    result = rupees * 100 + paisa;
    return true;
}

bool normalize_class(const std::string& value, std::string& canonical, SeatTier& tier) {
    const std::string lowered = lower_ascii(unquote(value));
    if (lowered == "silver") {
        canonical = "Silver";
        tier = SeatTier::Silver;
        return true;
    }
    if (lowered == "gold") {
        canonical = "Gold";
        tier = SeatTier::Gold;
        return true;
    }
    if (lowered == "recliner") {
        canonical = "Recliner";
        tier = SeatTier::Recliner;
        return true;
    }
    return false;
}

ImportEntry rejected_entry(int line_number, const std::string& line, const std::string& reason) {
    ImportEntry entry;
    entry.line_number = line_number;
    entry.raw_line = line;
    entry.status = ImportStatus::Rejected;
    entry.reason = reason;
    return entry;
}

}  // namespace

PriceImportError::PriceImportError(const std::string& message)
    : std::runtime_error(message) {}

ImportReport PriceImporter::import_file(const std::string& path) const {
    std::ifstream input(path);
    if (!input) {
        throw PriceImportError("Could not open price list: " + path);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return import_lines(lines);
}

ImportReport PriceImporter::import_text(const std::string& text) const {
    std::istringstream input(text);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    if (text.empty()) {
        lines.clear();
    }
    return import_lines(lines);
}

ImportReport PriceImporter::import_lines(const std::vector<std::string>& lines) const {
    ImportReport report;
    bool first_nonblank_line = true;

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const int line_number = static_cast<int>(index + 1);
        const std::string& line = lines[index];
        const std::string trimmed_line = trim(line);
        if (trimmed_line.empty()) {
            report.entries.push_back(rejected_entry(line_number, line, "Blank row"));
            ++report.rejected_count;
            continue;
        }
        if (first_nonblank_line && lower_ascii(trimmed_line) == "seatclass,price") {
            ImportEntry entry;
            entry.line_number = line_number;
            entry.raw_line = line;
            entry.status = ImportStatus::Header;
            entry.reason = "Header row ignored";
            report.entries.push_back(entry);
            first_nonblank_line = false;
            continue;
        }
        first_nonblank_line = false;

        std::string raw_class;
        std::string raw_price;
        if (!split_record(line, raw_class, raw_price)) {
            report.entries.push_back(rejected_entry(line_number, line, "Expected exactly two comma-separated fields"));
            ++report.rejected_count;
            continue;
        }
        if (raw_class.empty()) {
            report.entries.push_back(rejected_entry(line_number, line, "Missing seat class"));
            ++report.rejected_count;
            continue;
        }

        SeatTier tier;
        std::string canonical_class;
        if (!normalize_class(raw_class, canonical_class, tier)) {
            report.entries.push_back(rejected_entry(line_number, line, "Unknown seat class"));
            ++report.rejected_count;
            continue;
        }

        Paisa price_paisa = 0;
        std::string price_reason;
        if (!parse_price_paisa(raw_price, price_paisa, price_reason)) {
            report.entries.push_back(rejected_entry(line_number, line, price_reason));
            ++report.rejected_count;
            continue;
        }

        ImportEntry entry;
        entry.line_number = line_number;
        entry.raw_line = line;
        entry.normalized_class = canonical_class;
        entry.normalized_price_paisa = price_paisa;
        const std::size_t tier_index = static_cast<std::size_t>(tier);
        const bool representation_changed = unquote(raw_class) != canonical_class ||
                                             unquote(raw_price) != format_rupees(price_paisa).substr(3);
        if (report.canonical_prices.present[tier_index]) {
            entry.status = report.canonical_prices.prices_paisa[tier_index] == price_paisa
                               ? ImportStatus::Duplicate
                               : ImportStatus::Conflict;
            entry.reason = entry.status == ImportStatus::Duplicate
                               ? "Already imported; first valid occurrence retained"
                               : "Conflicting price; first valid occurrence retained";
            if (entry.status == ImportStatus::Duplicate) {
                ++report.duplicate_count;
            } else {
                ++report.conflict_count;
            }
        } else {
            report.canonical_prices.present[tier_index] = true;
            report.canonical_prices.prices_paisa[tier_index] = price_paisa;
            entry.status = representation_changed ? ImportStatus::Normalized : ImportStatus::Imported;
            ++report.imported_count;
            if (representation_changed) {
                ++report.normalized_count;
            }
            entry.reason = "First valid occurrence retained";
        }
        report.entries.push_back(entry);
    }
    return report;
}

std::string import_status_name(ImportStatus status) {
    switch (status) {
    case ImportStatus::Imported:
        return "IMPORTED";
    case ImportStatus::Normalized:
        return "NORMALIZED";
    case ImportStatus::Duplicate:
        return "DUPLICATE";
    case ImportStatus::Conflict:
        return "DUPLICATE/CONFLICT";
    case ImportStatus::Rejected:
        return "REJECTED";
    case ImportStatus::Header:
        return "HEADER";
    }
    return "UNKNOWN";
}

std::string format_import_report(const ImportReport& report) {
    std::ostringstream output;
    output << "========== PRICE IMPORT REPORT ==========\n\n";
    for (const ImportEntry& entry : report.entries) {
        output << "Line " << entry.line_number << ":\n"
               << "Raw       : " << entry.raw_line << '\n';
        if (!entry.normalized_class.empty()) {
            output << "Normalized: " << entry.normalized_class << ", "
                   << format_rupees(entry.normalized_price_paisa) << '\n';
        }
        output << "Status    : " << import_status_name(entry.status) << '\n';
        if (!entry.reason.empty()) {
            output << "Reason    : " << entry.reason << '\n';
        }
        output << '\n';
    }
    output << "Imported  : " << report.imported_count << '\n'
           << "Normalized: " << report.normalized_count << '\n'
           << "Duplicates: " << report.duplicate_count << '\n'
           << "Conflicts : " << report.conflict_count << '\n'
            << "Rejected  : " << report.rejected_count << '\n';
    return output.str();
}

}  // namespace cinema
