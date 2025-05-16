## ELS (Electronic Lead Screw) System Setup for Proculus Display

This guide provides clearer names and overall suggestions for configuring your Electronic Lead Screw (ELS) system via the SETUP tab on your Proculus display. Accurate configuration is crucial for precise machining operations like threading and power feeding.

---

### General Configuration Parameters:

**1. Unit Configuration:**

- **Original Name:** `unit`
- **Suggested Name:** `Measurement Unit` or `System Units`
- **Description:** Defines the primary unit of measurement used throughout the system (e.g., millimeters or inches).
- **Suggestion:** Ensure this is set _first_, as it may affect the interpretation of other values. Choose the unit system you will primarily work with for your projects and measuring tools.

**2. Feed Rate Units:**

- **Original Name:** `feed units`
- **Suggested Name:** `Feed Rate Units`
- **Description:** Specifies the units for feed rates (e.g., mm/revolution, inches/revolution, mm/minute, inches/minute).
- **Suggestion:** Select units that are intuitive for the type of work you do. For lathe work, per-revolution units are common for threading, while per-minute units might be used for general turning.

---

### Spindle/Encoder Configuration:

**3. Main Spindle/Chuck Gearing (if applicable):**

- **Original Name:** `chuck teeth`
- **Suggested Name:** `Spindle Gear Ratio (Chuck Teeth)` or `Headstock Gearing: Chuck Side Teeth`
- **Description:** If there's a gear reduction or multiplication between the main spindle motor and the chuck/spindle where the encoder is _not_ directly mounted, this refers to the number of teeth on the gear connected to the chuck or main spindle. This, in conjunction with `Encoder Drive Teeth` (see below), helps establish the relationship between the encoder and the actual spindle RPM.
- **Suggestion:** If your encoder is directly on the spindle (1:1 ratio), this might be irrelevant or set to a 1:1 value (e.g., both chuck and encoder drive teeth set to the same number if the software requires it). Carefully count the teeth on the gears if your setup uses indirect driving of the encoder or spindle.

**4. Encoder Drive Gearing:**

- **Original Name:** `encoder teeth`
- **Suggested Name:** `Spindle Gear Ratio (Encoder Drive Teeth)` or `Headstock Gearing: Encoder Side Teeth`
- **Description:** Number of teeth on the gear driving the spindle encoder. This is used with `Chuck Teeth` to calculate the precise spindle speed if the encoder is not 1:1 with the spindle.
- **Suggestion:** Ensure accurate counting of gear teeth. If a belt drive is used from the spindle to the encoder, these would be the pulley teeth counts (spindle pulley teeth and encoder pulley teeth).

**5. Encoder Resolution:**

- **Original Name:** `encoder steps`
- **Suggested Name:** `Encoder PPR (Pulses Per Revolution)` or `Encoder Resolution`
- **Description:** The number of pulses the spindle encoder generates for one full revolution of the _encoder shaft_. This is a critical value for accurate synchronization.
- **Suggestion:** This value is found in your encoder's datasheet. It's often listed as PPR. Do not confuse this with quadrature counts (which would be 4x the PPR). Clarify if the system expects PPR or quadrature counts.

---

### Z-Axis Configuration (Carriage Motion):

**6. Z-Axis Motor Direction:**

- **Original Name:** `z invert direction`
- **Suggested Name:** `Z-Axis Invert Motor Direction`
- **Description:** A boolean (True/False or 0/1) setting to reverse the direction of the Z-axis motor if it moves the carriage the wrong way relative to the desired input.
- **Suggestion:** Test with a small commanded move. If the carriage moves away from the headstock when you expect it to move towards it (or vice-versa), toggle this setting.

**7. Z-Axis Motor Drive Gearing:**

- **Original Name:** `z motor teeth`
- **Suggested Name:** `Z-Axis Motor Pulley/Gear Teeth`
- **Description:** Number of teeth on the pulley or gear directly attached to the Z-axis motor shaft.
- **Suggestion:** If using a belt drive, count the teeth on the motor pulley. If direct drive or gearbox, consult the motor/gearbox documentation.

**8. Z-Axis Lead Screw Gearing:**

- **Original Name:** `z lead screw teeth`
- **Suggested Name:** `Z-Axis Lead Screw Pulley/Gear Teeth`
- **Description:** Number of teeth on the pulley or gear directly attached to the Z-axis lead screw.
- **Suggestion:** If using a belt drive, count the teeth on the lead screw pulley. This, combined with `Z-Axis Motor Pulley/Gear Teeth`, determines the gear ratio between the Z-motor and the Z-lead screw.

**9. Z-Axis Lead Screw Type:**

- **Original Name:** `z lead scre type`
- **Suggested Name:** `Z-Axis Lead Screw Type`
- **Description:** Specifies the type of lead screw (e.g., Acme, Ball Screw, Trapezoidal). While often informational, some advanced ELS systems might use this for specific compensation algorithms, though pitch is usually the more critical factor.
- **Suggestion:** Select the appropriate type. For most hobbyist ELS setups, the pitch and mechanical backlash are the primary concerns.

**10. Z-Axis Lead Screw Pitch:**

- **Original Name:** `z lead scren pitch`
- **Suggested Name:** `Z-Axis Lead Screw Pitch`
- **Description:** The distance the carriage travels for one full revolution of the _Z-axis lead screw_. Expressed in the `Measurement Unit` (e.g., mm/revolution or TPI - Threads Per Inch, which would then need conversion to pitch in inches).
- **Suggestion:** This is a critical value. Measure carefully or check your lathe's manual. For imperial screws (TPI), pitch = 1 / TPI. For metric screws, the pitch is usually directly specified (e.g., 2mm pitch).

**11. Z-Axis Driver Microstepping / Resolution:**

- **Original Name:** `z driver pul/rev`
- **Suggested Name:** `Z-Axis Driver Pulses Per Revolution` or `Z-Axis Motor Steps Per Revolution (Factoring Microstepping)`
- **Description:** The number of electronic pulses the stepper or servo driver requires to make the Z-axis _motor_ complete one full revolution. For stepper motors, this is (Motor Steps Per Revolution \* Microstepping Value). For servo motors, this is often its encoder resolution if the drive is configured in a step/direction mode.
- **Suggestion:** Check your stepper motor's native steps (e.g., 200 for a 1.8-degree motor) and the microstepping setting on your stepper driver (e.g., 8, 16, 32). Multiply these (e.g., 200 steps/rev \* 16 microsteps/step = 3200 pulses/rev). For servos, refer to the servo drive manual for its pulse input settings.

**12. Z-Axis Maximum Travel Speed:**

- **Original Name:** `z travel speed`
- **Suggested Name:** `Z-Axis Maximum Feed Rate` or `Z-Axis Rapid Traverse Speed`
- **Description:** The maximum speed at which the Z-axis can reliably travel, usually in `Feed Rate Units` (e.g., mm/minute or inches/minute).
- **Suggestion:** Start with a conservative value. Too high can lead to missed steps (steppers) or following errors (servos). This is often limited by the motor torque, lead screw pitch, and system mechanics.

**13. Z-Axis Acceleration:**

- **Original Name:** `z acceleration`
- **Suggested Name:** `Z-Axis Acceleration Rate`
- **Description:** How quickly the Z-axis motor can accelerate to the `Z-Axis Maximum Feed Rate` and decelerate. Usually in units per second squared (e.g., mm/s² or inches/s²).
- **Suggestion:** Start conservatively. Too high can cause missed steps or excessive mechanical stress. Smooth motion is key. This value often requires tuning for optimal performance without stalling the motor.

**14. Z-Axis Backlash Compensation:**

- **Original Name:** `z backlash (based on units)`
- **Suggested Name:** `Z-Axis Backlash Compensation Value`
- **Description:** The amount of lost motion in the Z-axis when reversing direction, expressed in the selected `Measurement Unit`.
- **Suggestion:** Measure this accurately using a dial indicator. This is crucial for precise cuts, especially when changing direction (e.g., multi-pass threading). Some systems apply this compensation automatically.

**15. Z-Axis Manual Calibration (One Turn):**

- **Original Name:** `z one turn`
- **Suggested Name:** `Z-Axis Calibration: Distance Per Motor Revolution` or `Z-Axis Fine Tune: Travel per Motor Rev`
- **Description:** This is likely a calibration parameter. It might represent the actual measured travel of the Z-axis when the _Z-axis motor_ (not necessarily the lead screw) makes one full revolution. This can be used to fine-tune the system's understanding of the mechanical linkage if there are slight discrepancies in pulley/gear ratios or lead screw pitch from their nominal values.
- **Suggestion:** If your system uses this, perform a test: command the motor one revolution (or a known number of pulses equating to one theoretical revolution) and measure the actual carriage travel with a dial indicator. Enter this measured value.

---

### X-Axis Configuration (Cross Slide Motion - Optional Future Axis):

_(These follow the same logic as the Z-Axis parameters but apply to the cross-slide or X-axis.)_

**16. X-Axis Motor Direction:**

- **Original Name:** `x motor invert direction`
- **Suggested Name:** `X-Axis Invert Motor Direction`

**17. X-Axis Motor Drive Gearing:**

- **Original Name:** `x motor teeth`
- **Suggested Name:** `X-Axis Motor Pulley/Gear Teeth`

**18. X-Axis Lead Screw Gearing:**

- **Original Name:** `x leadscre teeth` (Typo noted: "leadscre")
- **Suggested Name:** `X-Axis Lead Screw Pulley/Gear Teeth`

**19. X-Axis Lead Screw Type:**

- **Original Name:** `x lead screw type`
- **Suggested Name:** `X-Axis Lead Screw Type`

**20. X-Axis Lead Screw Pitch:**

- **Original Name:** `x lead screw pitch`
- **Suggested Name:** `X-Axis Lead Screw Pitch`

**21. X-Axis Driver Microstepping / Resolution:**

- **Original Name:** `x driver pul/rev`
- **Suggested Name:** `X-Axis Driver Pulses Per Revolution` or `X-Axis Motor Steps Per Revolution (Factoring Microstepping)`

**22. X-Axis Maximum Travel Speed:**

- **Original Name:** `x travel speed`
- **Suggested Name:** `X-Axis Maximum Feed Rate` or `X-Axis Rapid Traverse Speed`

**23. X-Axis Acceleration:**

- **Original Name:** `x acceleration`
- **Suggested Name:** `X-Axis Acceleration Rate`

**24. X-Axis Backlash Compensation:**

- **Original Name:** `x backlash (unit based)`
- **Suggested Name:** `X-Axis Backlash Compensation Value`

**25. X-Axis Manual Calibration (One Turn):**

- **Original Name:** `x one turn`
- **Suggested Name:** `X-Axis Calibration: Distance Per Motor Revolution` or `X-Axis Fine Tune: Travel per Motor Rev`

---

### System Information:

**26. Firmware/Software Version:**

- **Original Name:** `Version type`
- **Suggested Name:** `Firmware Version` or `Software Version`
- **Description:** Displays the current version of the ELS controller firmware or Proculus display software.
- **Suggestion:** Keep a note of this. It's useful for troubleshooting, checking for updates, or seeking support.

---

### Overall Suggestions for ELS Setup:

1.  **Documentation is Key:** If you have a manual for your ELS kit or Proculus display, refer to it as the primary source of truth. The names and specific functions can vary between ELS implementations.
2.  **Accuracy Matters:**
    - **Mechanical Counts:** Double-check the tooth counts on all gears and pulleys.
    - **Encoder PPR:** Use the exact value from the encoder's datasheet.
    - **Lead Screw Pitch:** Measure accurately or confirm from specifications. An incorrect pitch is a common source of threading errors.
    - **Driver Settings:** Ensure the `pulses/rev` matches your stepper/servo driver configuration (especially microstepping for steppers).
3.  **Incremental Testing:**
    - After configuring initial values, test Z-axis movement with small, commanded distances. Verify direction and distance traveled using a dial indicator or calipers.
    - Test backlash compensation by moving in one direction, then reversing by a small amount, and checking if the slack is taken up correctly.
4.  **Start Conservatively:**
    - Use lower values for `Maximum Feed Rate` and `Acceleration Rate` initially. You can tune these upwards later once basic functionality is confirmed to avoid stalling motors or causing excessive wear.
5.  **Encoder Setup:**
    - Ensure the spindle encoder is securely mounted and aligned. Any slip or vibration here will lead to inaccurate speed readings and poor ELS performance.
    - The ratio between `Chuck Teeth` and `Encoder Teeth` (or spindle pulley and encoder pulley teeth) directly impacts how the system interprets spindle speed from encoder pulses. If the encoder is mounted directly to the spindle (1:1), these values should reflect that (e.g., both set to 1, or the same arbitrary number if the software requires a value).
6.  **Calculating Pulses per Unit (e.g., Pulses per mm or Pulses per inch):**
    - The system uses many of these parameters to internally calculate how many motor pulses are needed to move the carriage a specific distance. The core formula often involves:
      `(Driver Pulses Per Motor Revolution / Lead Screw Pitch) * (Lead Screw Pulley Teeth / Motor Pulley Teeth)`
    - Understanding this relationship can help in troubleshooting.
7.  **Backlash Measurement:**
    - Mount a dial indicator to read carriage movement.
    - Move the axis a small distance in one direction.
    - Zero the indicator.
    - Command a move in the opposite direction by a very small increment until the indicator just starts to move. The commanded distance before movement is your backlash. Repeat a few times for an average.
8.  **"One Turn" Calibration:**
    - The `z one turn` and `x one turn` parameters are likely for fine-tuning the "pulses per unit" calculation. If after setting all known mechanical values, a commanded 10mm move results in 9.95mm actual travel, this parameter might be used to apply a correction factor based on a measured single motor revolution's travel. Consult your specific ELS documentation for its exact use.
9.  **Save Configuration:** Once you have a working and tuned configuration, ensure you save it if your Proculus system allows. Take screenshots or make a written backup of all settings.
10. **Community Resources:** Look for forums or user groups related to your specific ELS kit or the Proculus display. Other users may have shared their configurations for similar lathes or offered solutions to common setup problems.

By carefully setting these parameters, your ELS system will be capable of precise and repeatable motion for threading and other machining tasks.
