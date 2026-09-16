# Cinema Pricing Engine

## Introduction

This C++17 command-line application calculates a cinema booking bill for Silver, Gold, and Recliner seats. It validates bookings, prevents sold-out tiers from being selected, applies configurable discounts and fees, calculates GST, and prints an itemized total.

The business rules live in `PricingEngine`; the CLI is only an input/output adapter. Demonstration values are assumptions and can be changed in `src/main.cpp` without changing the pricing algorithm.

## Features

- Configurable tier prices and availability
- Flat festival discount
- Optional capped member discount
- Per-ticket convenience fee
- GST on discounted tickets plus the convenience fee
- Exact integer-paisa arithmetic
- Deterministic percentage rounding
- Validation with useful errors
- Independently testable pricing logic

## Tech Stack

- C++17
- Standard C++ Library
- GitHub Codespaces

## Project Structure

- `src/pricing_engine.h`: public data model and engine API
- `src/pricing_engine.cpp`: validation, calculations, formatting, and checked arithmetic
- `src/main.cpp`: demonstration configuration, CLI input, and bill output
- `tests/test_pricing.cpp`: dependency-free assertion tests
- `REASONING.md`: auditable engineering decisions
- `AI_LOGS.md`: template for recording actual assessment interactions

## Setup

From the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/main.cpp src/pricing_engine.cpp -o cinema
```

## Running

The CLI expects four values: Silver quantity, Gold quantity, Recliner quantity, and member flag (`0` or `1`).

```bash
./cinema
# Example input: 1 2 0 1
```

With the demonstration configuration and input `1 2 0 1`, the bill is:

```text
Base Amount       : ₹800.00
Festival Discount : -₹50.00
Member Discount   : -₹75.00
Convenience Fee   : ₹60.00
GST               : ₹132.30
TOTAL             : ₹867.30
```

## Testing

```bash
g++ -std=c++17 -Wall -Wextra -pedantic tests/test_pricing.cpp src/pricing_engine.cpp -o pricing_tests
./pricing_tests
```

## Debugging

Read compiler errors from the first reported source location, then rebuild. Run `./cinema` with small values to inspect behavior. Run `./pricing_tests` after changes. If `gdb` is available, use `gdb ./cinema`, then `run`, `break cinema::PricingEngine::calculate`, and `print` relevant variables. Temporary logging should inspect intermediate paisa values: base, each discount, fee, taxable amount, GST, and final amount; remove it after debugging.

## Money Handling

Money is stored as integer paisa in `long long`: ₹199.99 is `19999`. No `float` or `double` is used for pricing, so binary floating-point representation cannot change a bill by a paisa. Rates are integer basis points, where `10000` means 100%; for example, 10% is `1000` and 18% is `1800`.

## Pricing Order

```text
Base amount
→ festival discount
→ member percentage discount and cap
→ convenience fee
→ GST
→ final total
```

GST is calculated on discounted tickets plus the convenience fee. Percentage results use integer division and therefore round down to the nearest paisa.

## Assumptions

The problem statement does not specify prices, discount values, tax rates, input format, GST base, or rounding. The demonstration configuration uses Silver ₹200.00, Gold ₹300.00, Recliner ₹500.00, festival discount ₹50.00, member rate 10%, member cap ₹100.00, convenience fee ₹20.00 per ticket, and GST 18%. These are demonstration assumptions, not official requirements. The engine also rejects duplicate tier entries and treats availability as a booking-time rule.

## Git Workflow

```bash
git status
git add .
git commit -m "Implement cinema pricing engine"
git push origin main
```

## Assessment Notes

`long long` provides a practical integer range for paisa and checked arithmetic rejects overflow. Configuration is separate so prices and rules can change without editing calculation code. Validation happens before calculation to prevent invalid states. The pipeline is staged so each business rule is auditable. The member cap is applied with `min`-equivalent logic, and tests cover normal, boundary, invalid, precision, and quantity cases.