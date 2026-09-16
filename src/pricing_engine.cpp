#include "pricing_engine.h"

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cinema {
namespace {

constexpr BasisPoints kMaximumBasisPoints = 10000;

std::size_t tier_index(SeatTier tier) {
    const auto index = static_cast<std::size_t>(tier);
    if (index >= kSeatTierCount) {
        throw PricingError("Invalid seat tier.");
    }
    return index;
}

Paisa checked_add(Paisa left, Paisa right, const std::string& context) {
    if (right > 0 && left > std::numeric_limits<Paisa>::max() - right) {
        throw PricingError("Amount overflow while calculating " + context + ".");
    }
    return left + right;
}

Paisa checked_multiply(Paisa left, Paisa right, const std::string& context) {
    if (left < 0 || right < 0 ||
        (right != 0 && left > std::numeric_limits<Paisa>::max() / right)) {
        throw PricingError("Amount overflow while calculating " + context + ".");
    }
    return left * right;
}

Paisa percentage_amount(Paisa amount, BasisPoints rate, const std::string& context) {
    const Paisa product = checked_multiply(amount, rate, context);
    return product / kMaximumBasisPoints;
}

}  // namespace

PricingError::PricingError(const std::string& message)
    : std::runtime_error(message) {}

PricingEngine::PricingEngine(CinemaConfig config) : config_(config) {
    validate_config();
}

const CinemaConfig& PricingEngine::config() const noexcept {
    return config_;
}

void PricingEngine::validate_config() const {
    for (const TierConfig& tier : config_.tiers) {
        if (tier.price_paisa < 0) {
            throw PricingError("Tier price cannot be negative.");
        }
    }
    if (config_.festival_discount_paisa < 0) {
        throw PricingError("Festival discount cannot be negative.");
    }
    if (config_.member_discount_cap_paisa < 0) {
        throw PricingError("Member discount cap cannot be negative.");
    }
    if (config_.convenience_fee_paisa < 0) {
        throw PricingError("Convenience fee cannot be negative.");
    }
    if (config_.member_discount_basis_points < 0 ||
        config_.member_discount_basis_points > kMaximumBasisPoints) {
        throw PricingError("Member discount rate must be between 0% and 100%.");
    }
    if (config_.gst_basis_points < 0 || config_.gst_basis_points > kMaximumBasisPoints) {
        throw PricingError("GST rate must be between 0% and 100%.");
    }
}

void PricingEngine::validate_booking(const Booking& booking) const {
    if (booking.tickets.empty()) {
        throw PricingError("A booking must contain at least one ticket.");
    }

    std::array<bool, kSeatTierCount> selected{};
    for (const TicketSelection& selection : booking.tickets) {
        const std::size_t index = tier_index(selection.tier);
        if (selection.quantity <= 0) {
            throw PricingError("Ticket quantities must be positive.");
        }
        if (selected[index]) {
            throw PricingError("Each seat tier may appear only once in a booking.");
        }
        selected[index] = true;
        if (!config_.tiers[index].available) {
            throw PricingError("Selected tier is sold out: " + seat_tier_name(selection.tier) + ".");
        }
    }
}

Bill PricingEngine::calculate(const Booking& booking) const {
    validate_booking(booking);

    Bill bill;
    for (const TicketSelection& selection : booking.tickets) {
        const TierConfig& tier = config_.tiers[tier_index(selection.tier)];
        bill.base_amount_paisa = checked_add(
            bill.base_amount_paisa,
            checked_multiply(tier.price_paisa, selection.quantity, "the base amount"),
            "the base amount");
        bill.total_tickets = checked_add(bill.total_tickets, selection.quantity, "the ticket count");
    }

    bill.festival_discount_paisa =
        config_.festival_discount_paisa < bill.base_amount_paisa
            ? config_.festival_discount_paisa
            : bill.base_amount_paisa;
    const Paisa after_festival = bill.base_amount_paisa - bill.festival_discount_paisa;

    if (booking.is_member) {
        const Paisa percentage_discount = percentage_amount(
            after_festival, config_.member_discount_basis_points, "the member discount");
        bill.member_discount_paisa = percentage_discount < config_.member_discount_cap_paisa
                                          ? percentage_discount
                                          : config_.member_discount_cap_paisa;
        if (bill.member_discount_paisa > after_festival) {
            bill.member_discount_paisa = after_festival;
        }
    }

    const Paisa discounted_tickets = after_festival - bill.member_discount_paisa;
    bill.convenience_fee_paisa = checked_multiply(
        config_.convenience_fee_paisa, bill.total_tickets, "the convenience fee");
    const Paisa taxable_amount = checked_add(
        discounted_tickets, bill.convenience_fee_paisa, "the taxable amount");
    bill.gst_paisa = percentage_amount(taxable_amount, config_.gst_basis_points, "GST");
    bill.final_amount_paisa = checked_add(taxable_amount, bill.gst_paisa, "the final amount");
    return bill;
}

std::string seat_tier_name(SeatTier tier) {
    switch (tier) {
    case SeatTier::Silver:
        return "Silver";
    case SeatTier::Gold:
        return "Gold";
    case SeatTier::Recliner:
        return "Recliner";
    }
    throw PricingError("Invalid seat tier.");
}

std::string format_rupees(Paisa paisa) {
    if (paisa < 0) {
        throw PricingError("Cannot format a negative amount.");
    }
    std::ostringstream output;
    output << "₹" << paisa / 100 << "." << std::setw(2) << std::setfill('0') << paisa % 100;
    return output.str();
}

}  // namespace cinema
