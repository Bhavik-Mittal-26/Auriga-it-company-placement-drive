#include "../src/pricing_engine.h"
#include "../src/price_importer.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

using namespace cinema;

namespace {

CinemaConfig config() {
    CinemaConfig value;
    value.tiers[0] = {19999, true};
    value.tiers[1] = {30000, true};
    value.tiers[2] = {50000, true};
    value.festival_discount_paisa = 5000;
    value.member_discount_basis_points = 1000;
    value.member_discount_cap_paisa = 10000;
    value.convenience_fee_paisa = 2000;
    value.gst_basis_points = 1800;
    return value;
}

Booking booking(std::initializer_list<TicketSelection> tickets, bool member = false) {
    return {std::vector<TicketSelection>(tickets), member};
}

void expect_error(const std::function<void()>& action, const std::string& expected) {
    try {
        action();
        assert(false && "Expected PricingError");
    } catch (const PricingError& error) {
        const std::string actual = error.what();
        if (actual.find(expected) == std::string::npos) {
            std::cerr << "Expected error containing '" << expected << "', got '" << actual << "'.\n";
            assert(false);
        }
    }
}

void normal_non_member() {
    PricingEngine engine(config());
    const Bill bill = engine.calculate(booking({{SeatTier::Silver, 1}}));
    assert(bill.base_amount_paisa == 19999);
    assert(bill.festival_discount_paisa == 5000);
    assert(bill.member_discount_paisa == 0);
    assert(bill.convenience_fee_paisa == 2000);
    assert(bill.gst_paisa == 3059);
    assert(bill.final_amount_paisa == 20058);
}

void normal_member_and_multiple_tiers() {
    PricingEngine engine(config());
    const Bill bill = engine.calculate(booking({{SeatTier::Silver, 1}, {SeatTier::Gold, 2}}, true));
    assert(bill.base_amount_paisa == 79999);
    assert(bill.member_discount_paisa == 7499);
    assert(bill.total_tickets == 3);
    assert(bill.final_amount_paisa == 86730);
}

void sold_out_tiers_are_rejected() {
    for (SeatTier tier : {SeatTier::Silver, SeatTier::Gold, SeatTier::Recliner}) {
        CinemaConfig value = config();
        value.tiers[static_cast<std::size_t>(tier)].available = false;
        expect_error([&] { PricingEngine(value).calculate(booking({{tier, 1}})); }, "sold out");
    }
}

void invalid_quantities_and_empty_bookings_are_rejected() {
    PricingEngine engine(config());
    expect_error([&] { engine.calculate(booking({{SeatTier::Silver, -1}})); }, "positive");
    expect_error([&] { engine.calculate(booking({{SeatTier::Silver, 0}})); }, "positive");
    expect_error([&] { engine.calculate(booking({})); }, "at least one");
}

void festival_discount_is_bounded() {
    CinemaConfig value = config();
    value.festival_discount_paisa = 19999;
    Bill bill = PricingEngine(value).calculate(booking({{SeatTier::Silver, 1}}));
    assert(bill.festival_discount_paisa == 19999);
    assert(bill.final_amount_paisa == 2360);

    value.festival_discount_paisa = 30000;
    bill = PricingEngine(value).calculate(booking({{SeatTier::Silver, 1}}));
    assert(bill.festival_discount_paisa == 19999);
    assert(bill.final_amount_paisa == 2360);
}

void member_cap_and_non_member_behavior() {
    CinemaConfig value = config();
    value.festival_discount_paisa = 0;
    value.member_discount_basis_points = 2000;
    Bill bill = PricingEngine(value).calculate(booking({{SeatTier::Recliner, 1}}, true));
    assert(bill.member_discount_paisa == 10000);

    value.member_discount_basis_points = 5000;
    bill = PricingEngine(value).calculate(booking({{SeatTier::Recliner, 1}}, true));
    assert(bill.member_discount_paisa == 10000);

    bill = PricingEngine(value).calculate(booking({{SeatTier::Recliner, 1}}, false));
    assert(bill.member_discount_paisa == 0);
}

void configuration_validation() {
    CinemaConfig value = config();
    value.tiers[0].price_paisa = -1;
    expect_error([&] { const PricingEngine engine(value); }, "price");
    value = config();
    value.member_discount_basis_points = 10001;
    expect_error([&] { const PricingEngine engine(value); }, "Member discount rate");
    value = config();
    value.gst_basis_points = -1;
    expect_error([&] { const PricingEngine engine(value); }, "GST rate");
    value = config();
    value.convenience_fee_paisa = -1;
    expect_error([&] { const PricingEngine engine(value); }, "Convenience fee");
}

void precision_small_amounts_and_large_quantity() {
    CinemaConfig value = config();
    value.tiers[0].price_paisa = 1;
    value.festival_discount_paisa = 0;
    value.convenience_fee_paisa = 1;
    value.gst_basis_points = 3333;
    Bill bill = PricingEngine(value).calculate(booking({{SeatTier::Silver, 3}}));
    assert(bill.base_amount_paisa == 3);
    assert(bill.convenience_fee_paisa == 3);
    assert(bill.gst_paisa == 1); // floor(6 * 33.33%)
    assert(bill.final_amount_paisa == 7);

    value.gst_basis_points = 0;
    bill = PricingEngine(value).calculate(booking({{SeatTier::Silver, 1000000}}));
    assert(bill.total_tickets == 1000000);
    assert(bill.final_amount_paisa == 2000000);
}

void duplicate_tiers_are_rejected() {
    expect_error([&] {
        PricingEngine(config()).calculate(booking({{SeatTier::Silver, 1}, {SeatTier::Silver, 1}}));
    }, "only once");
}

void importer_normalizes_prices_and_classes() {
    const PriceImporter importer;
    const ImportReport report = importer.import_text(
        "SeatClass,Price\n"
        "Silver,200\n"
        "silver,200.00\n"
        "SILVER,₹200.50\n"
        "Gold,₹ 300\n"
        "ReClInEr,.50\n");

    assert(report.canonical_prices.present[0]);
    assert(report.canonical_prices.prices_paisa[0] == 20000);
    assert(report.canonical_prices.prices_paisa[1] == 30000);
    assert(report.canonical_prices.prices_paisa[2] == 50);
    assert(report.imported_count == 3);
    assert(report.normalized_count == 3);
    assert(report.duplicate_count == 1);
    assert(report.conflict_count == 1);
    assert(report.rejected_count == 0);
}

void importer_rejects_invalid_rows_and_conflicts() {
    const PriceImporter importer;
    const ImportReport report = importer.import_text(
        "Silver,200\n"
        "silver,250\n"
        "Gold,\n"
        ",300\n"
        "Silver,-200\n"
        "Gold,abc\n"
        "Gold,200.123\n"
        "VIP,800\n"
        "bad row\n"
        "\n");

    assert(report.canonical_prices.prices_paisa[0] == 20000);
    assert(!report.canonical_prices.present[1]);
    assert(report.imported_count == 1);
    assert(report.duplicate_count == 0);
    assert(report.conflict_count == 1);
    assert(report.rejected_count == 8);
    assert(format_import_report(report).find("DUPLICATE/CONFLICT") != std::string::npos);
    assert(format_import_report(report).find("Negative price") != std::string::npos);
}

void importer_handles_empty_and_missing_files() {
    const PriceImporter importer;
    assert(importer.import_text("").entries.empty());

    const std::string path = "tests/temporary_prices.csv";
    {
        std::ofstream output(path);
        output << "Silver,199.99\nGold,300\nRecliner,500\n";
    }
    const ImportReport report = importer.import_file(path);
    assert(report.canonical_prices.prices_paisa[0] == 19999);
    std::remove(path.c_str());

    try {
        importer.import_file("tests/file_that_does_not_exist.csv");
        assert(false && "Expected PriceImportError");
    } catch (const PriceImportError& error) {
        assert(std::string(error.what()).find("Could not open") != std::string::npos);
    }
}

void cleaned_prices_are_used_by_pricing_engine() {
    const ImportReport report = PriceImporter().import_text(
        "Silver,199.99\nGold,300\nRecliner,500\n");
    CinemaConfig value = config();
    for (std::size_t index = 0; index < kSeatTierCount; ++index) {
        value.tiers[index].price_paisa = report.canonical_prices.prices_paisa[index];
    }
    value.festival_discount_paisa = 0;
    value.convenience_fee_paisa = 0;
    value.gst_basis_points = 0;
    const Bill bill = PricingEngine(value).calculate(booking({{SeatTier::Silver, 1}}));
    assert(bill.base_amount_paisa == 19999);
    assert(bill.final_amount_paisa == 19999);
}

}  // namespace

int main() {
    normal_non_member();
    normal_member_and_multiple_tiers();
    sold_out_tiers_are_rejected();
    invalid_quantities_and_empty_bookings_are_rejected();
    festival_discount_is_bounded();
    member_cap_and_non_member_behavior();
    configuration_validation();
    precision_small_amounts_and_large_quantity();
    duplicate_tiers_are_rejected();
    importer_normalizes_prices_and_classes();
    importer_rejects_invalid_rows_and_conflicts();
    importer_handles_empty_and_missing_files();
    cleaned_prices_are_used_by_pricing_engine();
    std::cout << "All pricing tests passed.\n";
}
