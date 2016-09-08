/**
 * MK4duo 3D Printer Firmware
 *
 * Based on Marlin, Sprinter and grbl
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 * Copyright (C) 2013 - 2016 Alberto Cotronei @MagoKimbra
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * stepper.h - stepper motor driver: executes motion plans of planner.c using the stepper motors
 * Part of Grbl
 *
 * Copyright (c) 2009-2011 Simen Svale Skogsrud
 *
 * Grbl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Grbl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Grbl. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STEPPER_H
#define STEPPER_H

#ifndef __SAM3X8E__
  #include "speed_lookuptable.h"
#endif

#include "stepper_indirection.h"

class Stepper;
extern Stepper stepper;

class Stepper {

  public:

    static block_t* current_block;  // A pointer to the block currently being traced

    #if ENABLED(ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED)
      static bool abort_on_endstop_hit;
    #endif

    #if ENABLED(Z_DUAL_ENDSTOPS)
      static bool performing_homing;
    #endif

  private:

    static unsigned char last_direction_bits;        // The next stepping-bits to be output
    static unsigned int cleaning_buffer_counter;

    #if ENABLED(Z_DUAL_ENDSTOPS)
      static bool locked_z_motor, locked_z2_motor;
    #endif

    // Counter variables for the Bresenham line tracer
    static long counter_X, counter_Y, counter_Z, counter_E;
    static volatile uint32_t step_events_completed; // The number of step events executed in the current block

    #if ENABLED(ADVANCE) || ENABLED(LIN_ADVANCE)
      static unsigned char old_OCR0A;
      static volatile unsigned char eISR_Rate;
      #if ENABLED(LIN_ADVANCE)
        static volatile int e_steps[E_STEPPERS];
        static int extruder_advance_k = LIN_ADVANCE_K;
        static int final_estep_rate;
        static int current_estep_rate[E_STEPPERS];  // Actual extruder speed [steps/s]
        static int current_adv_steps[E_STEPPERS];   // The amount of current added esteps due to advance.
                                                    // i.e., the current amount of pressure applied
                                                    // to the spring (=filament).

      #else
        static long e_steps[E_STEPPERS];
        static long advance_rate, advance, final_advance, old_advance;
      #endif
    #endif // ADVANCE or LIN_ADVANCE

    static long acceleration_time, deceleration_time;
    // unsigned long accelerate_until, decelerate_after, acceleration_rate, initial_rate, final_rate, nominal_rate;
    #ifdef __SAM3X8E__
      static uint32_t acc_step_rate; // needed for deceleration start point
      static uint32_t OCR1A_nominal;
    #else
      static uint16_t acc_step_rate; // needed for deceleration start point
      static uint16_t OCR1A_nominal;
    #endif

    static uint8_t step_loops, step_loops_nominal;

    static volatile long endstops_trigsteps[XYZ];
    static volatile long endstops_stepsTotal, endstops_stepsDone;

    #if PIN_EXISTS(MOTOR_CURRENT_PWM_XY)
      #ifndef PWM_MOTOR_CURRENT
        #define PWM_MOTOR_CURRENT DEFAULT_PWM_MOTOR_CURRENT
      #endif
      static constexpr int motor_current_setting[3] = PWM_MOTOR_CURRENT;
    #endif

    //
    // Positions of stepper motors, in step units
    //
    static volatile long count_position[NUM_AXIS];

    //
    // Current direction of stepper motors (+1 or -1)
    //
    static volatile signed char count_direction[NUM_AXIS];

    #if ENABLED(COLOR_MIXING_EXTRUDER)
      static long counter_m[MIXING_STEPPERS];
      #define MIXING_STEPPERS_LOOP(VAR) \
        for (uint8_t VAR = 0; VAR < MIXING_STEPPERS; VAR++) \
          if (current_block->mix_event_count[VAR])
    #endif

    #if ENABLED(LASERBEAM)
      static long counter_L;
      #if ENABLED(LASER_RASTER)
        static int counter_raster;
      #endif // LASER_RASTER
    #endif // LASERBEAM

  public:

    //
    // Constructor / initializer
    //
    Stepper() { };

    //
    // Initialize stepper hardware
    //
    static void init();

    //
    // Interrupt Service Routines
    //
    static void isr();

    #if ENABLED(ADVANCE) || ENABLED(LIN_ADVANCE)
      static void advance_isr();
    #endif

    //
    // Block until all buffered steps are executed
    //
    static void synchronize();

    //
    // Set current position in steps
    //
    static void set_position(const long &x, const long &y, const long &z, const long &e);
    static void set_e_position(const long &e);

    //
    // Set direction bits for all steppers
    //
    static void set_directions();

    //
    // Get the position of a stepper, in steps
    //
    static long position(AxisEnum axis);

    //
    // Report the positions of the steppers, in steps
    //
    static void report_positions();

    //
    // Get the position (mm) of an axis based on stepper position(s)
    //
    static float get_axis_position_mm(AxisEnum axis);

    //
    // The stepper subsystem goes to sleep when it runs out of things to execute. Call this
    // to notify the subsystem that it is time to go to work.
    //
    static void wake_up();

    //
    // Wait for moves to finish and disable all steppers
    //
    static void finish_and_disable();

    //
    // Quickly stop all steppers and clear the blocks queue
    //
    static void quick_stop();

    //
    // The direction of a single motor
    //
    static FORCE_INLINE bool motor_direction(AxisEnum axis) { return TEST(last_direction_bits, axis); }

    static void enable_all_steppers();
    static void disable_all_steppers();

    #if HAS(DIGIPOTSS)
      static void digitalPotWrite(int address, int value);
    #endif
    static void digipot_current(uint8_t driver, int current);
    static void microstep_ms(uint8_t driver, int8_t ms1, int8_t ms2);
    static void microstep_mode(uint8_t driver, uint8_t stepping);
    static void microstep_readings();

    #if ENABLED(Z_DUAL_ENDSTOPS)
      static FORCE_INLINE void set_homing_flag(bool state) { performing_homing = state; }
      static FORCE_INLINE void set_z_lock(bool state) { locked_z_motor = state; }
      static FORCE_INLINE void set_z2_lock(bool state) { locked_z2_motor = state; }
    #endif

    #if ENABLED(BABYSTEPPING)
      static void babystep(const uint8_t axis, const bool direction); // perform a short step with a single stepper motor, outside of any convention
    #endif

    static inline void kill_current_block() {
      step_events_completed = current_block->step_event_count;
    }

    //
    // Handle a triggered endstop
    //
    static void endstop_triggered(AxisEnum axis);

    //
    // Triggered position of an axis in mm (not core-savvy)
    //
    static FORCE_INLINE float triggered_position_mm(AxisEnum axis) {
      return endstops_trigsteps[axis] * planner.steps_to_mm[axis];
    }

    #if ENABLED(NPR2) // Multiextruder
      static void colorstep(long csteps, const bool direction);
    #endif

    #if MB(ALLIGATOR)
      static void set_driver_current();
    #endif

  private:
    
    #ifdef __SAM3X8E__
      static FORCE_INLINE uint32_t calc_timer(uint32_t step_rate) {
        uint32_t timer;
    #else
      static FORCE_INLINE uint16_t calc_timer(uint16_t step_rate) {
        uint16_t timer;
    #endif

        NOMORE(step_rate, MAX_STEP_FREQUENCY);

        #ifndef __SAM3X8E__ || ENABLED(ENABLE_HIGH_SPEED_STEPPING)
          if(step_rate > (2 * DOUBLE_STEP_FREQUENCY)) { // If steprate > 2*DOUBLE_STEP_FREQUENCY >> step 4 times
            step_rate >>= 2;
            step_loops = 4;
          }
          else if(step_rate > DOUBLE_STEP_FREQUENCY) { // If steprate > DOUBLE_STEP_FREQUENCY >> step 2 times
            step_rate >>= 1;
            step_loops = 2;
          }
          else
        #endif
        {
          step_loops = 1;
        }

        #ifdef __SAM3X8E__
          timer = HAL_TIMER_RATE / step_rate;
        #else
          NOLESS(step_rate, F_CPU / 500000);
          step_rate -= F_CPU / 500000; // Correct for minimal speed
          if (step_rate >= (8 * 256)) { // higher step rate
            uint16_t table_address = (uint16_t)&speed_lookuptable_fast[(unsigned char)(step_rate >> 8)][0];
            unsigned char tmp_step_rate = (step_rate & 0x00ff);
            uint16_t gain = (uint16_t)pgm_read_word_near(table_address + 2);
            MultiU16X8toH16(timer, tmp_step_rate, gain);
            timer = (uint16_t)pgm_read_word_near(table_address) - timer;
          }
          else { // lower step rates
            uint16_t table_address = (uint16_t)&speed_lookuptable_slow[0][0];
            table_address += ((step_rate) >> 1) & 0xfffc;
            timer = (uint16_t)pgm_read_word_near(table_address);
            timer -= (((uint16_t)pgm_read_word_near(table_address + 2) * (unsigned char)(step_rate & 0x0007)) >> 3);
          }

          if (timer < 100) { // (20kHz this should never happen)
            timer = 100;
            SERIAL_EMT(MSG_STEPPER_TOO_HIGH, step_rate);
          }
        #endif
      return timer;
    }
    
    // Initializes the trapezoid generator from the current block. Called whenever a new
    // block begins.
    static FORCE_INLINE void trapezoid_generator_reset() {

      static int8_t last_extruder = -1;

      if (current_block->direction_bits != last_direction_bits || current_block->active_extruder != last_extruder) {
        last_direction_bits = current_block->direction_bits;
        last_extruder = current_block->active_extruder;
        set_directions();
      }

      #if ENABLED(ADVANCE)

        advance = current_block->initial_advance;
        final_advance = current_block->final_advance;

        // Do E steps + advance steps
        #if ENABLED(COLOR_MIXING_EXTRUDER)
          long advance_factor = (advance >> 8) - old_advance;
          // ...for mixing steppers proportionally
          MIXING_STEPPERS_LOOP(j)
            e_steps[j] += advance_factor * current_block->step_event_count / current_block->mix_event_count[j];
        #else
          // ...for the active extruder
          e_steps[TOOL_E_INDEX] += ((advance >> 8) - old_advance);
        #endif

        old_advance = advance >> 8;

      #endif

      deceleration_time = 0;
      // step_rate to timer interval
      OCR1A_nominal = calc_timer(current_block->nominal_rate);
      // make a note of the number of step loops required at nominal speed
      step_loops_nominal = step_loops;
      acc_step_rate = current_block->initial_rate;
      acceleration_time = calc_timer(acc_step_rate);

      #ifdef __SAM3X8E__
        //HAL_timer_stepper_count(acceleration_time);
      #else
        OCR1A = acceleration_time;
      #endif

      #if ENABLED(LIN_ADVANCE)
        if (current_block->use_advance_lead) {
          current_estep_rate[current_block->active_extruder] = ((uint32_t)acc_step_rate * current_block->e_speed_multiplier8) >> 8;
          final_estep_rate = (current_block->nominal_rate * current_block->e_speed_multiplier8) >> 8;
        }
      #endif
    }

    static void digipot_init();
    static void microstep_init();

};

#endif // STEPPER_H
