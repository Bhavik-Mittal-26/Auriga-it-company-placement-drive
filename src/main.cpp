#include "pricing_engine.h"
#include "price_importer.h"

#include <iostream>
#include <string>

namespace {

using cinema::Booking;
using cinema::CinemaConfig;
using cinema::ImportReport;
using cinema::PricingEngine;
using cinema::PricingError;
using cinema::PriceImporter;
using cinema::PriceImportError;
using cinema::SeatTier;
using cinema::format_rupees;

CinemaConfig demonstration_config(const ImportReport& import_report) {
    CinemaConfig config;
    for (std::size_t index = 0; index < cinema::kSeatTierCount; ++index) {
        if (!import_report.canonical_prices.present[index]) {
            throw PriceImportError("Price list is missing a required seat tier.");
        }
        config.tiers[index] = {import_report.canonical_prices.prices_paisa[index], true};
    }
    config.festival_discount_paisa = 5000;
    config.member_discount_basis_points = 1000;
    config.member_discount_cap_paisa = 10000;
    config.convenience_fee_paisa = 2000;
    config.gst_basis_points = 1800;
    return config;
}

void print_bill(const cinema::Bill& bill) {
    std::cout << "\n========== CINEMA BILL ==========\n\n"
              << "Base Amount       : " << format_rupees(bill.base_amount_paisa) << '\n'
              << "Festival Discount : -" << format_rupees(bill.festival_discount_paisa) << '\n'
              << "Member Discount   : -" << format_rupees(bill.member_discount_paisa) << '\n'
              << "Convenience Fee   : " << format_rupees(bill.convenience_fee_paisa) << '\n'
              << "GST               : " << format_rupees(bill.gst_paisa) << "\n\n"
              << "---------------------------------\n"
              << "TOTAL             : " << format_rupees(bill.final_amount_paisa) << '\n'
              << "=================================\n";
}

}  // namespace

int main() {
    try {
        const PriceImporter importer;
        const ImportReport import_report = importer.import_file("data/prices.csv");
        std::cout << cinema::format_import_report(import_report) << '\n';
        const PricingEngine engine(demonstration_config(import_report));
        long long silver = 0;
        long long gold = 0;
        long long recliner = 0;
        int member = 0;

        std::cout << "Friday night at the multiplex\n"
                  << "Prices loaded from data/prices.csv\n"
                  << "Enter Silver, Gold, Recliner quantities and member flag (0/1): ";
        if (!(std::cin >> silver >> gold >> recliner >> member)) {
            std::cerr << "Invalid input. Expected four numbers.\n";
            return 1;
        }
        if (member != 0 && member != 1) {
            throw PricingError("Member flag must be 0 or 1.");
        }

        Booking booking{{}, member == 1};
        if (silver != 0) {
            booking.tickets.push_back({SeatTier::Silver, silver});
        }
        if (gold != 0) {
            booking.tickets.push_back({SeatTier::Gold, gold});
        }
        if (recliner != 0) {
            booking.tickets.push_back({SeatTier::Recliner, recliner});
        }
        print_bill(engine.calculate(booking));
        return 0;
    } catch (const PricingError& error) {
        std::cerr << "Booking error: " << error.what() << '\n';
        return 1;
    } catch (const PriceImportError& error) {
        std::cerr << "Price import error: " << error.what() << '\n';
        return 1;
    }
}
