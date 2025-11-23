# QA Documentation

This directory contains test plans, QA processes, and regression tests to prevent building impressive systems that don't actually work.

## Files

- **test_plan.md** - What to test and how to test it
- **qa_plan.md** - QA gates and quality standards
- **regression_plan.md** - Automated regression tests
- **run_smoke_tests.sh** - Quick verification script

## Core Principle

**If it can't route requests to AI providers, nothing else matters.**

All testing focuses on verifying fundamental functionality first, then everything else.

## Quick Test

```bash
# Run basic smoke tests
./qa/run_smoke_tests.sh
```

If this fails, stop everything and fix core functionality before proceeding.