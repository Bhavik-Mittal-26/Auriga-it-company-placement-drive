#ifndef PRICING_ENGINE_H
#define PRICING_ENGINE_H

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace cinema {

using Paisa = long long;
using Quantity = long long;

// Rates are integer basis points: 10000 basis points equals 100 percent.
using BasisPoints = long long;

constexpr std::size_t kSeatTierCount = 3;

enum class SeatTier {
    Silver = 0,
    Gold = 1,
    Recliner = 2
};

struct TierConfig {
    Paisa price_paisa = 0;
    bool available = true;
};

struct CinemaConfig {
    std::array<TierConfig, kSeatTierCount> tiers{};
    Paisa festival_discount_paisa = 0;
    BasisPoints member_discount_basis_points = 0;
    Paisa member_discount_cap_paisa = 0;
    Paisa convenience_fee_paisa = 0;
    BasisPoints gst_basis_points = 0;
};

struct TicketSelection {
    SeatTier tier;
    Quantity quantity;
};

struct Booking {
    std::vector<TicketSelection> tickets;
    bool is_member = false;
};

struct Bill {
    Paisa base_amount_paisa = 0;
    Paisa festival_discount_paisa = 0;
    Paisa member_discount_paisa = 0;
    Paisa convenience_fee_paisa = 0;
    Paisa gst_paisa = 0;
    Paisa final_amount_paisa = 0;
    Quantity total_tickets = 0;
};

class PricingError : public std::runtime_error {
public:
    explicit PricingError(const std::string& message);
};

class PricingEngine {
public:
    explicit PricingEngine(CinemaConfig config);

    const CinemaConfig& config() const noexcept;
    Bill calculate(const Booking& booking) const;

private:
    CinemaConfig config_;

    void validate_config() const;
    void validate_booking(const Booking& booking) const;
};

std::string seat_tier_name(SeatTier tier);
std::string format_rupees(Paisa paisa);

}  // namespace cinema

#endif
