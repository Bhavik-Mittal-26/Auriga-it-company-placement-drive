# Cinema Pricing Engine

## Introduction

This C++17 command-line application imports and cleans a messy cinema price list, then calculates a booking bill for Silver, Gold, and Recliner seats. It validates bookings, prevents sold-out tiers from being selected, applies configurable discounts and fees, calculates GST, and prints an itemized total.

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
- Messy CSV price-list import with an audit report
- Independently testable pricing logic

## Tech Stack

- C++17
- Standard C++ Library
- GitHub Codespaces

## Project Structure

- `src/pricing_engine.h`: public data model and engine API
- `src/pricing_engine.cpp`: validation, calculations, formatting, and checked arithmetic
- `src/price_importer.h`: importer data model and API
- `src/price_importer.cpp`: CSV parsing, normalization, validation, deduplication, and reporting
- `src/main.cpp`: price import, CLI input, and bill output
- `data/prices.csv`: demonstration messy source data; it is intentionally not manually cleaned
- `tests/test_pricing.cpp`: dependency-free pricing, importer, and integration tests
- `REASONING.md`: auditable engineering decisions
- `AI_LOGS.md`: template for recording actual assessment interactions

## Setup

From the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/main.cpp src/pricing_engine.cpp src/price_importer.cpp -o cinema
```

## Running

Run from the repository root because the application reads `data/prices.csv`. It first prints the import report, then expects four values: Silver quantity, Gold quantity, Recliner quantity, and member flag (`0` or `1`).

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
g++ -std=c++17 -Wall -Wextra -pedantic tests/test_pricing.cpp src/pricing_engine.cpp src/price_importer.cpp -o pricing_tests
./pricing_tests
```

## Price List Import

The input is a simple two-column CSV-like file: `SeatClass,Price`. The first nonblank `SeatClass,Price` row is treated as a header. Blank rows, missing files, malformed rows, extra columns, missing fields, unknown classes, negative prices, and prices with more than two decimal places are rejected with a reason.

The importer accepts `200`, `200.00`, `₹200`, `₹ 200.50`, quoted values, and `.50`. It rejects `200.`, `200.123`, `abc`, and negative values. Prices are parsed directly from text into integer paisa; no floating point is used. Seat names are trimmed and matched case-insensitively only against Silver, Gold, and Recliner.

Normalization happens before duplicate detection. A first valid occurrence becomes canonical. Later equal prices are reported as duplicates. Later different prices are reported as duplicate conflicts and are excluded; they never overwrite the canonical price. `Imported` counts canonical rows accepted, `Normalized` counts accepted canonical rows whose representation changed, `Duplicates` counts later equal rows, `Conflicts` counts later different-price rows, and `Rejected` counts parsing or validation failures. The report retains every input line, including headers and blank rows.

Example source data is in `data/prices.csv`. The importer produces a canonical list of Silver ₹200.00, Gold ₹300.00, and Recliner ₹500.00 from that file. The pricing engine receives only those canonical prices.

Example report summary:

```text
Imported  : 3
Normalized: 3
Duplicates: 3
Conflicts : 1
Rejected  : 7
```

## Debugging

Read compiler errors from the first reported source location, then rebuild. Run from the repository root so `data/prices.csv` can be found. For file-not-found errors, check `pwd`, `ls data`, and the relative path. For parsing issues, inspect the raw line and its report reason. For duplicate issues, compare normalized class and paisa values. For incorrect conversion, test values such as `199.99`, `.50`, and `₹ 200.50`. Run `./pricing_tests` after changes. If `gdb` is available, use `gdb ./cinema`, then `run`, `break cinema::PricingEngine::calculate`, and `print` relevant variables. Temporary logging should inspect importer canonical prices and pricing intermediate values: base, each discount, fee, taxable amount, GST, and final amount; remove it after debugging.

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

The problem statement does not specify prices, discount values, tax rates, input format, GST base, or rounding. The demonstration configuration uses Silver ₹200.00, Gold ₹300.00, Recliner ₹500.00, festival discount ₹50.00, member rate 10%, member cap ₹100.00, convenience fee ₹20.00 per ticket, and GST 18%. These are demonstration assumptions, not official requirements. The importer supports only the three official seat tiers, treats the first valid price as authoritative, rejects conflicting duplicates, allows zero prices, and permits zero to two decimal places.

## Git Workflow

```bash
git status
git add .
git commit -m "Implement cinema pricing engine"
git push origin main
```

## Assessment Notes

`long long` provides a practical integer range for paisa and checked arithmetic rejects overflow. Importing is separate from pricing so unreliable source data is cleaned once before billing. Normalization precedes duplicate detection, and the first valid occurrence policy is deterministic. Configuration is separate so prices and rules can change without editing calculation code. Validation happens before calculation to prevent invalid states. The pipeline is staged so each business rule is auditable. Tests cover normal, boundary, invalid, precision, quantity, importer, file, duplicate, and integration cases.