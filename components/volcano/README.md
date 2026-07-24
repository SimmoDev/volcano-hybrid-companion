# Volcano component (scaffold)

This is the `volcano` ESPHome external component. It is currently a scaffold: it registers a valid, empty `VolcanoComponent` with no BLE communication, no Volcano protocol handling, and no domain logic yet. See `volcano.h` for the `TODO` markers showing where that work belongs, and [ADR-0002](../../docs/decisions/ADR-0002-volcano-component-architecture.md) for the architectural boundary it must respect.

## Building / validating

An example configuration that loads this component is at [`examples/esp32-s3-devkit-minimal.yaml`](../../examples/esp32-s3-devkit-minimal.yaml).

From the repository root, with the [ESPHome CLI](https://esphome.io/) installed:

```sh
# Validate the configuration and generated C++ without a physical device
esphome config examples/esp32-s3-devkit-minimal.yaml
esphome compile examples/esp32-s3-devkit-minimal.yaml
```

`esphome config` checks that the component registers correctly and the YAML is valid. `esphome compile` additionally builds the generated firmware, which is the closest available check that the C++ in this component actually compiles, without needing physical ESP32-S3 hardware attached.
