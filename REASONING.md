# Engineering Rationale

## 1. Requirements
The engine prices Silver, Gold, and Recliner tickets, rejects sold-out tiers, applies a flat festival discount, applies an optional capped member discount, adds a per-ticket convenience fee, calculates GST, and returns an itemized bill. It is independent of the command-line interface and uses configurable cinema rules.

## 2. Assumptions
- Demonstration prices are Silver ₹200.00, Gold ₹300.00, and Recliner ₹500.00.
- The demonstration festival discount is ₹50.00.
- Rates use integer basis points: 1000 means 10%, 1800 means 18%.
- Festival discount is applied once per booking and cannot exceed the base amount.
- Member discount is calculated after the festival discount and is available only to members.
- Member discount is capped after percentage calculation.
- Convenience fee is charged for every ticket, including discounted tickets.
- GST applies to discounted tickets plus convenience fee.
- Percentage results use floor division, so fractional paisa is discarded.
- Duplicate tier entries in one booking are rejected because the CLI aggregates each tier once.

## 3. Data Structures
`SeatTier` identifies a tier. `TierConfig` stores its price and availability. `CinemaConfig` stores all pricing rules. `Booking` stores selected quantities and membership. `Bill` stores each output line as paisa.

## 4. Architecture
`PricingEngine` validates configuration in its constructor, validates each booking before calculation, and calculates the bill in clear stages. `main.cpp` only gathers input and prints a bill. Tests call the engine directly.

## 5. Money Representation
`long long` paisa avoids binary floating-point errors. For example, ₹199.99 is exactly `19999`. Rates are also integer values, so no `double` is needed anywhere in pricing.

## 6. Validation Strategy
Configuration rejects negative monetary values and rates outside 0% to 100%. Bookings reject empty selections, non-positive quantities, duplicate tiers, and unavailable tiers. Checked arithmetic rejects paisa overflow rather than returning a corrupt bill.

## 7. Discount Strategy
The festival discount is bounded by the base amount. A member receives the configured percentage of the post-festival amount, capped by the configured cap and bounded by the remaining ticket amount.

## 8. Fee Calculation
The convenience fee is `ticket count * fee per ticket` and is added after discounts.

## 9. GST Calculation
Under the chosen assumption, GST is calculated on `discounted ticket amount + convenience fee`.

## 10. Rounding Behavior
All percentage calculations use `(amount * basis points) / 10000` with integer division. This deterministically rounds down to the nearest paisa.

## 11. Testing Strategy
The test executable uses standard-library assertions and covers normal members and non-members, all tiers, sold-out tiers, invalid inputs, discount boundaries, caps, exact paisa values, GST, small values, and a large valid quantity.

## 12. Edge Cases
A festival discount equal to or greater than the subtotal reduces the ticket amount to zero. A non-member always receives zero member discount. A 0% rate produces zero, and a 100% rate is valid but cannot make the final ticket amount negative.

## 13. Design Trade-offs
The solution uses one focused engine rather than a framework or pattern-heavy design. Basis points add a small naming convention, but they make decimal rates possible while preserving integer arithmetic. The fixed three-tier enum matches the assessment and keeps the public API easy to explain.
