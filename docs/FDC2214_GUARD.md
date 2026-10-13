# FDC2214 guard — checkpoint 12

This translation unit does not include, link, or vendor `Parvaz-Jamei/FDCxCapSense`. The API pin is commit `215031db9a6bc37ac26cd1d2b6dca96bbe3b4da7`. Upstream has both `readFrequencyHz` and `readFrequencyHzFromRegisters`. The guard keeps those paths distinct:

- `readFrequencyHz` uses the caller-supplied effective reference frequency. It does not divide that value and it does not invent a divider of 1.
- `readFrequencyHzFromRegisters` decodes `FIN_SEL` from `CLOCK_DIVIDERS` bits `[13:12]` and the reference divider from bits `[9:0]`, then uses `fREF = fCLK / divider` and `fSENSOR = finSel * fREF * raw / 2^28`.

A decoded divider of 0 returns `InvalidArgument` before any division. It is never replaced with 1. The resulting frequency is not the divider-1 frequency.

`Fdc2112` and `Fdc2114` precise conversion returns `NotSupported` on both paths. No frequency is invented for those variants.

Capacitance is computed only when the caller passes an explicit model with finite inductance `> 0` and parasitic capacitance `>= 0`. Otherwise the result is raw plus frequency (when the frequency path succeeded) and `capacitanceStatus` is `capacitance_not_derived`. A missing model is not treated as zero parasitic capacitance.
