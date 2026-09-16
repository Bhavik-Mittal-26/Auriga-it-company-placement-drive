#ifndef PRICE_IMPORTER_H
#define PRICE_IMPORTER_H

#include "pricing_engine.h"

#include <array>
#include <string>
#include <vector>

namespace cinema {

enum class ImportStatus {
    Imported,
    Normalized,
    Duplicate,
    Conflict,
    Rejected,
    Header
};

struct RawPriceRecord {
    std::string seat_class;
    std::string raw_price;
    int line_number = 0;
};

struct ImportEntry {
    int line_number = 0;
    std::string raw_line;
    std::string normalized_class;
    Paisa normalized_price_paisa = 0;
    ImportStatus status = ImportStatus::Rejected;
    std::string reason;
};

struct CanonicalPriceList {
    std::array<Paisa, kSeatTierCount> prices_paisa{};
    std::array<bool, kSeatTierCount> present{};
};

struct ImportReport {
    std::vector<ImportEntry> entries;
    CanonicalPriceList canonical_prices;
    int imported_count = 0;
    int normalized_count = 0;
    int duplicate_count = 0;
    int conflict_count = 0;
    int rejected_count = 0;
};

class PriceImportError : public std::runtime_error {
public:
    explicit PriceImportError(const std::string& message);
};

class PriceImporter {
public:
    ImportReport import_file(const std::string& path) const;
    ImportReport import_text(const std::string& text) const;

private:
    ImportReport import_lines(const std::vector<std::string>& lines) const;
};

std::string import_status_name(ImportStatus status);
std::string format_import_report(const ImportReport& report);

}  // namespace cinema

#endif
