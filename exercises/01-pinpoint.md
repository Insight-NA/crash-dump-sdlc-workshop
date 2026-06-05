# Exercise 01 — Pinpoint Phase 1 unaided

**Time:** 10 min · **Difficulty:** intermediate

## Goal

Cold-load the BUG-001 dump and find the failing frame in under 5 minutes, using only Plan Mode + a 3-file context belt.

## Setup

Captured `BUG-001.dmp.local` (or `.core.local`) per the pre-work email.

## Steps

1. Open the dump in VS Code's debug view.
2. Build a 3-file belt: the repro, the header, the implementation.
3. Run the Phase-1 prompt from the learner guide.
4. Stop the timer when Copilot has cited the offending line.

## Acceptance

The frame Copilot pinpoints matches the line where `m_offset` advances before bounds-checking.

## Reflection

If you exceeded 5 minutes, what was missing from your belt? Often the answer is the repro file — without it, Copilot has no anchor for "the third call".
