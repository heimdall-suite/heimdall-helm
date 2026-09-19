#ifndef HELM_SENSORS_H
#define HELM_SENSORS_H

/* Shared status tier every driver in this subsystem reports (issue #14),
   independent of each sensor's own value struct (imu.h etc.) -- same
   latest-value + status shape .docs/architecture/module-architecture.md
   already establishes for RX -> mapping, applied here per
   .docs/architecture/sensors.md. Not the same type as telemetry.h's
   TelemetryStatus: sensors and telemetry are independent producers/
   consumers reading the same underlying hardware for different reasons
   (control loop vs. ground-station reporting), so coupling their status
   types would be accidental, not structural -- unlike mapping.c's
   MappingFrame, which really is a literal passthrough of RxFrame.

   A driver reports OK/STALE/FAILED and nothing more -- whether a given
   consumer treats a sensor as hard-required or optional is that
   consumer's own judgment call, not something encoded here (see
   sensors.md's "Hard-required vs. optional/soft-dependency" section). */
typedef enum {
    SENSOR_STATUS_OK,
    SENSOR_STATUS_STALE,
    SENSOR_STATUS_FAILED,
} SensorStatus;

#endif /* HELM_SENSORS_H */
