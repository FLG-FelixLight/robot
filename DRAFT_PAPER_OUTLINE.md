# Draft paper outline — ESP32-S3 line-following robot

## 1. Working title

**Design and Implementation of a Web-Supervised Line-Following Robot with Junction-Based Route Codes**

Vietnamese working title: **Thiết kế và triển khai robot dò line điều khiển qua web với mã tuyến dựa trên ngã rẽ**

This is a prototype/system paper outline. Do not claim autonomous delivery performance until the experiments below have been conducted and documented.

## 2. Paper status and evidence boundary

| Claim area | Current evidence | What is still needed |
|---|---|---|
| Firmware architecture and configured behavior | Source code in `src/`; PlatformIO build succeeded | Versioned source snapshot and board/firmware identification |
| Web/API and destination persistence | Implementation in `data/` and firmware | End-to-end test against a physical ESP32 |
| Junction detector | Code searches for `1...0...1`, 30 ms debounce, and 200 ms center-arm condition | Sensor logs and repeated on-track trials; user currently reports that junctions are not detected |
| PID line following | Code implements weighted line error and PID | Track trials, recorded errors, speed, and tuning protocol |
| Obstacle pause, clock display, and audio | Implemented in source | Hardware validation, safety checks, measured response |
| Performance and reliability | No runtime measurements available | Raw data, trial counts, environment, uncertainty/statistics |

Build success proves compilation/linking only; it is not evidence that the physical robot follows a line, detects junctions, reaches a destination, or stops within a measured distance.

## 3. Abstract scaffold

Replace each bracketed field only after completing the experiments.

> This paper presents a small line-following mobile robot prototype based on an ESP32-S3, five digital line sensors, and a dual-motor L298N driver. The firmware combines weighted line-position estimation and PID steering with a web interface for telemetry, manual control, PID configuration, and destination-table management. A destination is represented by a sequence of junction actions (`L`, `R`, `S`) followed by a terminal `0`; the table is stored in LittleFS. The implementation also includes a front-facing ultrasonic obstacle pause, NTP time display, and I2S MP3 playback. We evaluated [number] trials on [track description] under [conditions]. The measured junction-detection rate was [result], route-completion rate was [result], and [other measured metric] was [result]. Current limitations include [verified limitations]. These results characterize a fixed-track prototype and do not imply general-purpose mapping or localization.

## 4. Research question and contribution

### Possible research question

> How reliably can a low-cost ESP32-S3 prototype follow a predefined line track and execute symbolic junction routes while exposing its state through a browser-based interface?

### Conservative contribution statements

- Modular embedded implementation combining line following, symbolic junction routing, and web supervision.
- Destination CRUD and persistence through a browser UI and ESP32 LittleFS.
- A reproducible test protocol for measuring sensor-pattern detection and route execution on a fixed track.

Avoid claiming novelty for PID, line following, Wi-Fi control, or ultrasonic sensing by themselves. State the project-specific integration contribution and compare only against cited, verifiable work.

## 5. Suggested manuscript structure

### 5.1 Introduction

- Motivation: repeatable movement on a predefined indoor track.
- Scope: a small fixed-track prototype, not a map-building or localization robot.
- Research question and contributions.
- Explicitly distinguish the intended application scenario from what the prototype was actually tested to do.

### 5.2 Related work

Review and cite primary or peer-reviewed sources on:

- Reflectance-sensor line following and PID control.
- Junction detection using low-cost digital sensor arrays.
- Embedded web supervision/telemetry for mobile robots.
- Small-robot obstacle detection and limitations of ultrasonic ranging.

Record complete bibliographic details and verify each source. Do not insert uncited background claims or fabricated references.

### 5.3 System design

Describe:

- ESP32-S3, five digital line sensors, L298N, two DC motors, Wi-Fi web interface, MAX98357/I2S, TM1637, and HY-SRF05.
- The GPIO map from the project configuration/user-supplied wiring diagram; mark actual wiring as “verified” only after inspection.
- Firmware modules: `main`, `motor_control`, `path_control`, `destination_manager`, `web_control`, `audio_control`, `network`, `clock_display`, and `obstacle_monitor`.
- Data paths: sensors → line estimate/PID → motor command; browser → WebSocket/queue → mode transition; REST table update → LittleFS and RAM map.
- Safety and electrical constraints, especially verifying sensor outputs and keeping HY-SRF05 ECHO at or below 3.3 V.

### 5.4 Algorithms

#### Line-position estimate and PID

Sensor weights in code: `[-2, -1, 0, +1, +2]`. For active sensors:

`error = sum(weight_i * sensor_i) / active_sensor_count`

The current motor command is:

`left = baseSpeed - correction`, `right = baseSpeed + correction`

where correction is computed by PID. Initial configured parameters are `Kp=35`, `Ki=0`, `Kd=20`, `baseSpeed=150`, `maxSpeed=255`; report any changes made during experiments.

#### Route code and junction detector

- `L`: turn left at the next accepted junction.
- `R`: turn right at the next accepted junction.
- `S`: consume a junction and continue line following.
- `0`: stop at the next accepted junction, request `welcome.mp3`, and remain in STOP for at least 5 seconds.
- Current detector: a `1...0...1` pattern across S1–S5, 30 ms stable-pattern condition, and a prior centered/armed condition held for 200 ms.
- Turn completion in code requires more than 120 ms of turning, center sensor on, and no more than two active sensors; timeout is 1500 ms.
- These values are implementation settings, not measured optimum values.

Important current issue: the user reports that the robot does not detect junctions. Treat this as an unresolved runtime fault, not a successful result. Capture raw S1–S5 values while moving over a straight line, gap, and junction; also record mode, `gJunctionArmed`, pattern duration, and obstacle state.

### 5.5 Experimental methodology

#### Test setup record

| Field | Record for each experiment |
|---|---|
| Board/firmware revision | [board ID, source revision, build date] |
| Track | [dimensions, line width, junction shapes, surface/material] |
| Sensor setup | [height, spacing, polarity, threshold/calibration] |
| Robot setup | [mass, wheel diameter, motor/battery condition] |
| Controller | [Kp, Ki, Kd, baseSpeed, maxSpeed] |
| Environment | [lighting, floor condition, obstacle material/distance] |
| Repetitions | [number of runs, reset/start procedure] |

#### Recommended experiments

1. **Sensor/pattern characterization:** collect sensor bit strings on line, background, and each junction geometry; report confusion counts.
2. **Junction detection:** count true junction detections, missed detections, and false detections by pattern type and speed.
3. **Line-following:** run a defined distance or number of laps at fixed speed; record completion, mean absolute line error, RMS error, and failures.
4. **Route execution:** use short routes such as `0`, `L0`, `S0`, and `R0` where track topology supports them. Record per-junction action correctness, destination-stop success, and elapsed time.
5. **Obstacle pause:** approach representative objects at controlled distances; record detection distance, motor-stop response, resume threshold, false triggers, and no-echo behavior.
6. **Audio/time display:** verify playback trigger and completion; compare displayed time with a reference after NTP synchronization.

Do not use a route as a test if the actual track does not provide that branch. Keep raw logs and record unsuccessful runs rather than excluding them silently.

### 5.6 Metrics and blank results tables

Fill only from measured trials. Define the denominator and failure criteria before collecting data.

| Metric | Definition | Result |
|---|---|---|
| Junction recall | detected real junctions / observed real junctions | [ ] |
| Junction precision | true detections / all detector activations | [ ] |
| Route completion rate | runs reaching intended terminal junction / valid runs | [ ] |
| Route action accuracy | correctly executed L/R/S actions / expected actions | [ ] |
| Stop-position error | distance from intended terminal point at rest | [ ] |
| Mean absolute line error | mean absolute logged error over defined run | [ ] |
| RMS line error | square root of mean squared logged error | [ ] |
| Obstacle response distance | measured distance when motors are paused | [ ] |
| Control-loop period | timestamp difference between sensor samples | [ ] |

For each proportion, provide `n` and numerator/denominator. For continuous measures, report number of runs, mean, standard deviation, and range (or a justified confidence interval). Do not report the configured 5 ms delay as the measured sample period.

### 5.7 Results and discussion

Organize results around the research question:

- Show sensor pattern logs and a confusion matrix before presenting route success.
- Report the current junction-detection failure as a result if it remains unresolved at submission time.
- Compare trials only under the same track, sensor placement, speed, and lighting.
- Discuss failure causes as hypotheses unless logs establish causality.
- Separate build-time resource usage from runtime control performance.

### 5.8 Limitations, safety, and future work

- No encoder or wheel-speed feedback.
- No lost-line fail-safe timeout or total PATH timeout in the current implementation.
- No localization, physical distance in route code, or return-to-origin guarantee.
- Ultrasonic pause is not obstacle avoidance and has not been hardware-validated.
- Junction detection has a reported unresolved failure.
- Web control has no authentication; protect credentials and use a controlled network.
- Verify all GPIO electrical levels and strapping-pin behavior before operation.

## 6. Figures and tables to prepare

1. System block diagram and verified wiring diagram.
2. Firmware/module architecture and main-loop priority flow.
3. Sensor layout and representative raw bit patterns.
4. Junction state diagram (center-arm → stable pattern → action → turn/stop).
5. Screenshot of web telemetry and destination editor.
6. Track drawing with the exact tested route and start/terminal markers.
7. Experiment results tables from Section 5.6.

Do not use a generic binary-tree drawing unless it accurately represents the physical track topology.

## 7. Reproducibility appendix

Include:

- PlatformIO environment and library versions from the build log.
- Firmware and filesystem build/upload commands and exact outcomes.
- The tested source revision and configuration values.
- A sanitized track description, sensor polarity, sensor height, and route table.
- Raw serial/telemetry logs with timestamps and unsuccessful trials.

Never include Wi-Fi passwords, tokens, private network credentials, or personally identifying information in a submitted paper.
