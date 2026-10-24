# Wokwi ESP32 simulation (CoreSmoke)

`.github/workflows/wokwi.yml` compiles `examples/CoreSmoke` for `esp32:esp32:esp32`
(esp32 core 3.3.12, arduino-cli 1.5.1) into `tests/wokwi/coresmoke/build`, then runs
`wokwi/wokwi-ci-action` v1.3.0 (SHA-pinned) with `tests/wokwi/coresmoke/wokwi.toml`,
`diagram.json` and the scenario `coresmoke.test.yaml`. The scenario waits for a serial
line containing `"kind":"hello"` and then one containing `"kind":"reading"`; the run fails
on `Guru Meditation` or after 30 s.

## Status

The `WOKWI_CLI_TOKEN` secret is not configured, so the simulation has never run and the
scenario is unverified. Without the token the job fails at the "Require WOKWI_CLI_TOKEN"
step with `NOT RUN`, on purpose, so a missing simulation is never shown as a pass.

## Setup (repository owner)

1. Sign in at https://wokwi.com and create a CI token at https://wokwi.com/dashboard/ci.
2. In GitHub: Settings → Secrets and variables → Actions → New repository secret,
   name `WOKWI_CLI_TOKEN`, value the token. (`gh secret set WOKWI_CLI_TOKEN` also works.)
3. Re-run the "Wokwi ESP32 simulation" workflow. Secrets are not passed to workflows
   triggered from forks, so a pull request from a fork fails at the token step.

## Run locally

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --library . \
  --output-dir tests/wokwi/coresmoke/build examples/CoreSmoke
WOKWI_CLI_TOKEN=... wokwi-cli --scenario coresmoke.test.yaml tests/wokwi/coresmoke
```
