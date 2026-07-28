# OrbiScan - Automated 3D Scanner Control System

This repository contains the C++ (Arduino) source code for **OrbiScan**, an automated 3D scanner designed and built for my university graduation project. 

## Technical Overview
The system controls the physical scanning sequence by interpolating movements between a rotary base (X-axis) and a vertical tower (Y-axis), ensuring precise camera positioning around the scanned object.

* **Hardware Stack:** Arduino Uno, CNC Shield V3, A4988 (Full Step) & DRV8825 (1/32 Microstepping) stepper motor drivers.
* **Control Modes:**
  * **Auto Sequence:** Executes a pre-calculated 8-phase scanning cycle with synchronized dual-axis movement (concurrent interpolation).
  * **Manual Override:** Allows free, non-blocking movement via a physical joystick for calibration, bypassing software limits.
* **Safety Mechanisms:** Built-in hardware interrupts/overrides to halt automation and return the scanner to its absolute (0,0) home coordinates.

## Key Engineering Features
* Custom stepper delay functions for fluid, non-blocking hardware control.
* Gear reduction math (2:1 ratio) translated into stepper pulse logic.
* High-precision microstepping calculations (160 steps/mm) applied to a GT2 timing belt system.
