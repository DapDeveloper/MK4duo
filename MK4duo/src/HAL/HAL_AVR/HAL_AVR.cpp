/**
 * MK4duo Firmware for 3D Printer, Laser and CNC
 *
 * Based on Marlin, Sprinter and grbl
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 * Copyright (C) 2013 Alberto Cotronei @MagoKimbra
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
 * This is the main Hardware Abstraction Layer (HAL).
 * To make the firmware work with different processors and toolchains,
 * all hardware related code should be packed into the hal files.
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
 *
 * Description: HAL for Arduino and compatible
 *
 * Contributors:
 * Copyright (c) 2014 Bob Cousins bobcousins42@googlemail.com
 *                    Nico Tonnhofer wurstnase.reprap@gmail.com
 *
 * Copyright (c) 2015 - 2016 Alberto Cotronei @MagoKimbra
 *
 * ARDUINO_ARCH_ARM
 */

// --------------------------------------------------------------------------
// Includes
// --------------------------------------------------------------------------

#include "../../../MK4duo.h"

#if ENABLED(__AVR__)

#if ANALOG_INPUTS > 0
  int32_t AnalogInputRead[ANALOG_INPUTS];
  uint8_t adcCounter[ANALOG_INPUTS],
          adcSamplePos = 0;

  int16_t HAL::AnalogInputValues[NUM_ANALOG_INPUTS] = { 0 };
  bool    HAL::Analog_is_ready = false;
#endif

const uint8_t AnalogInputChannels[] PROGMEM = ANALOG_INPUT_CHANNELS;

HAL::HAL() {
  // ctor
}

HAL::~HAL() {
  // dtor
}

void HAL_stepper_timer_start() {
  // waveform generation = 0100 = CTC
  CBI(TCCR1B, WGM13);
  SBI(TCCR1B, WGM12);
  CBI(TCCR1A, WGM11);
  CBI(TCCR1A, WGM10);

  // output mode = 00 (disconnected)
  TCCR1A &= ~(3 << COM1A0);
  TCCR1A &= ~(3 << COM1B0);

  // Set the timer pre-scaler
  // Generally we use a divider of 8, resulting in a 2MHz timer
  // frequency on a 16MHz MCU. If you are going to change this, be
  // sure to regenerate speed_lookuptable.h with
  // create_speed_lookuptable.py
  TCCR1B = (TCCR1B & ~(0x07 << CS10)) | (2 << CS10);

  // Init Stepper ISR to 122 Hz for quick starting
  OCR1A = 0x4000;
  TCNT1 = 0;
}

void HAL_temp_timer_start() {
  TEMP_TCCR =  0; // set entire TEMP_TCCR register to 0
  TEMP_OCR  = 64; // Set divisor for 64 3906 Hz
}

uint32_t HAL_calc_timer_interval(uint32_t step_rate) {

  uint32_t timer = 0;

  NOMORE(step_rate, uint32_t(MAX_STEP_FREQUENCY));

  #if DISABLED(DISABLE_DOUBLE_QUAD_STEPPING)
    if (step_rate > (2 * DOUBLE_STEP_FREQUENCY)) { // If steprate > (2 * DOUBLE_STEP_FREQUENCY) Hz >> step 4 times
      step_rate >>= 2;
      stepper.step_loops = 4;
    }
    else if (step_rate > DOUBLE_STEP_FREQUENCY) { // If steprate > DOUBLE_STEP_FREQUENCY >> step 2 times
      step_rate >>= 1;
      stepper.step_loops = 2;
    }
    else
  #endif
      stepper.step_loops = 1;

  constexpr uint32_t min_step_rate = F_CPU / 500000U;
  NOLESS(step_rate, min_step_rate);
  step_rate -= min_step_rate;   // Correct for minimal speed
  if (step_rate >= (8 * 256)) { // higher step rate
    const uint8_t   tmp_step_rate = (step_rate & 0x00FF);
    const uint16_t  table_address = (uint16_t)&speed_lookuptable_fast[(uint8_t)(step_rate >> 8)][0],
                    gain = (uint16_t)pgm_read_word_near(table_address + 2);
    timer = MultiU16X8toH16(tmp_step_rate, gain);
    timer = (uint16_t)pgm_read_word_near(table_address) - timer;
  }
  else { // lower step rates
    uint16_t table_address = (uint16_t)&speed_lookuptable_slow[0][0];
    table_address += ((step_rate) >> 1) & 0xFFFC;
    timer = (uint16_t)pgm_read_word_near(table_address)
          - (((uint16_t)pgm_read_word_near(table_address + 2) * (uint8_t)(step_rate & 0x0007)) >> 3);
  }

  if (timer < 100) { // (20kHz this should never happen)
    timer = 100;
    SERIAL_EMV("Steprate too high: ", step_rate);
  }

  return timer;

}

bool HAL::execute_100ms = false;

// Return available memory
int HAL::getFreeRam() {
  int freeram = 0;
  InterruptProtectedBlock noInts;
  uint8_t * heapptr, * stackptr;
  heapptr = (uint8_t *)malloc(4);          // get heap pointer
  free(heapptr);      // free up the memory again (sets heapptr to 0)
  stackptr =  (uint8_t *)(SP);           // save value of stack pointer
  freeram = (int)stackptr-(int)heapptr;
  return freeram;
}

void(* resetFunc) (void) = 0; // declare reset function @ address 0

// Reset peripherals and cpu
void HAL::resetHardware() { resetFunc(); }

void HAL::showStartReason() {

  // Check startup - does nothing if bootloader sets MCUSR to 0
  const uint8_t mcu = MCUSR;
  if (mcu & 1)  SERIAL_EM(MSG_POWERUP);
  if (mcu & 2)  SERIAL_EM(MSG_EXTERNAL_RESET);
  if (mcu & 4)  SERIAL_EM(MSG_BROWNOUT_RESET);
  if (mcu & 8)  SERIAL_EM(MSG_WATCHDOG_RESET);
  if (mcu & 32) SERIAL_EM(MSG_SOFTWARE_RESET);

  MCUSR = 0;
}

#if ANALOG_INPUTS > 0

  void HAL::analogStart() {

    #if MB(RUMBA) && ((TEMP_SENSOR_0==-1) || (TEMP_SENSOR_1==-1) || (TEMP_SENSOR_2==-1) || (TEMP_SENSOR_BED==-1) || (TEMP_SENSOR_CHAMBER==-1) || (TEMP_SENSOR_COOLER==-1))
      // disable RUMBA JTAG in case the thermocouple extension is plugged on top of JTAG connector
      MCUCR = _BV(JTD);
      MCUCR = _BV(JTD);
    #endif

    ADMUX = ANALOG_REF; // refernce voltage
    for (uint8_t i = 0; i < ANALOG_INPUTS; i++) {
      adcCounter[i] = 0;
      AnalogInputRead[i] = 0;
    }

    ADCSRA = _BV(ADEN) | _BV(ADSC) | ANALOG_PRESCALER;

    DIDR0 = 0;
    #ifdef DIDR2
      DIDR2 = 0;
    #endif

    while (ADCSRA & _BV(ADSC) ) {} // wait for conversion

    const uint8_t channel = pgm_read_byte(&AnalogInputChannels[adcSamplePos]);

    #if ENABLED(ADCSRB) && ENABLED(MUX5)
      if (channel & 8)  // Reading channel 0-7 or 8-15?
        ADCSRB |= _BV(MUX5);
      else
        ADCSRB &= ~_BV(MUX5);
    #endif

    ADMUX = (ADMUX & ~(0x1F)) | (channel & 7);
    ADCSRA |= _BV(ADSC); // start conversion without interrupt!

    // Use timer for temperature measurement
    // Interleave temperature interrupt with millies interrupt
    HAL_TEMP_TIMER_START();
    ENABLE_TEMP_INTERRUPT();

  }

  void HAL::AdcChangePin(const pin_t old_pin, const pin_t new_pin) {
    UNUSED(old_pin);
    UNUSED(new_pin);
  }

#endif

void HAL::hwSetup() { /*nope*/ }

void HAL::setPwmFrequency(const pin_t pin, uint8_t val) {
  val &= 0x07;
  switch(digitalPinToTimer(pin)) {

    #if ENABLED(TCCR0A)
      case TIMER0A:
      case TIMER0B:
        // TCCR0B &= ~(_BV(CS00) | _BV(CS01) | _BV(CS02));
        // TCCR0B |= val;
        break;
    #endif

    #if ENABLED(TCCR1A)
      case TIMER1A:
      case TIMER1B:
        // TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
        // TCCR1B |= val;
        break;
    #endif

    #if ENABLED(TCCR2)
      case TIMER2:
      case TIMER2:
        TCCR2 &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
        TCCR2 |= val;
        break;
    #endif

    #if ENABLED(TCCR2A)
      case TIMER2A:
      case TIMER2B:
        TCCR2B &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));
        TCCR2B |= val;
        break;
    #endif

    #if ENABLED(TCCR3A)
      case TIMER3A:
      case TIMER3B:
      case TIMER3C:
        TCCR3B &= ~(_BV(CS30) | _BV(CS31) | _BV(CS32));
        TCCR3B |= val;
        break;
    #endif

    #if ENABLED(TCCR4A)
      case TIMER4A:
      case TIMER4B:
      case TIMER4C:
        TCCR4B &= ~(_BV(CS40) | _BV(CS41) | _BV(CS42));
        TCCR4B |= val;
        break;
    #endif

    #if ENABLED(TCCR5A)
      case TIMER5A:
      case TIMER5B:
      case TIMER5C:
        TCCR5B &= ~(_BV(CS50) | _BV(CS51) | _BV(CS52));
        TCCR5B |= val;
        break;
    #endif
  }
}

void HAL_temp_isr() {

  static uint16_t cycle_100ms       = 0;

  static uint8_t  pwm_count_heater  = 0,
                  pwm_count_fan     = 0,
                  channel           = 0;

  /**
   * Standard PWM modulation
   */
  if (pwm_count_heater == 0) {
    #if HEATER_COUNT > 0
      LOOP_HEATER() {
        if (heaters[h].pin > -1 && ((heaters[h].pwm_pos = (heaters[h].soft_pwm & HEATER_PWM_MASK)) > 0))
          HAL::digitalWrite(heaters[h].pin, heaters[h].isHWInverted() ? LOW : HIGH);
      }
    #endif
  }

  if (pwm_count_fan == 0) {
    #if FAN_COUNT >0
      LOOP_FAN() {
        if ((fans[f].pwm_pos = (fans[f].Speed & FAN_PWM_MASK)) > 0)
          HAL::digitalWrite(fans[f].pin, fans[f].isHWInverted() ? LOW : HIGH);
      }
    #endif
  }

  #if HEATER_COUNT > 0
    LOOP_HEATER() {
      if (heaters[h].pin > -1 && heaters[h].pwm_pos == pwm_count_heater && heaters[h].pwm_pos != HEATER_PWM_MASK)
        HAL::digitalWrite(heaters[h].pin, heaters[h].isHWInverted() ? HIGH : LOW);
    }
  #endif

  #if FAN_COUNT > 0
    LOOP_FAN() {
      if (fans[f].Kickstart == 0 && fans[f].pwm_pos == pwm_count_fan && fans[f].pwm_pos != FAN_PWM_MASK)
        HAL::digitalWrite(fans[f].pin, fans[f].isHWInverted() ? HIGH : LOW);
    }
  #endif

  // Calculation cycle approximate a 100ms
  cycle_100ms++;
  if (cycle_100ms >= (F_CPU / 40960)) {
    cycle_100ms = 0;
    HAL::execute_100ms = true;
    #if ENABLED(FAN_KICKSTART_TIME) && FAN_COUNT > 0
      LOOP_FAN() {
        if (fans[f].Kickstart) fans[f].Kickstart--;
      }
    #endif
  }

  // read analog values
  #if ANALOG_INPUTS > 0

    if ((ADCSRA & _BV(ADSC)) == 0) {  // Conversion finished?
      channel = pgm_read_byte(&AnalogInputChannels[adcSamplePos]);
      AnalogInputRead[adcSamplePos] += ADCW;
      if (++adcCounter[adcSamplePos] >= (OVERSAMPLENR)) {

        // update temperatures only when values have been read
        if (!HAL::execute_100ms || adcSamplePos >= ANALOG_INPUTS)
          HAL::AnalogInputValues[channel] = AnalogInputRead[adcSamplePos] / (OVERSAMPLENR);

        AnalogInputRead[adcSamplePos] = 0;
        adcCounter[adcSamplePos] = 0;

        // Start next conversion
        if (++adcSamplePos >= ANALOG_INPUTS) {
          adcSamplePos = 0;
          HAL::Analog_is_ready = true;
        }
        channel = pgm_read_byte(&AnalogInputChannels[adcSamplePos]);
        #if ENABLED(ADCSRB) && ENABLED(MUX5)
          if (channel & 8)  // Reading channel 0-7 or 8-15?
            ADCSRB |= _BV(MUX5);
          else
            ADCSRB &= ~_BV(MUX5);
        #endif
        ADMUX = (ADMUX & ~(0x1F)) | (channel & 7);
      }
      ADCSRA |= _BV(ADSC);  // start next conversion
    }

    // Update the raw values if they've been read. Else we could be updating them during reading.
    if (HAL::Analog_is_ready) thermalManager.set_current_temp_raw();

  #endif

  pwm_count_heater  += HEATER_PWM_STEP;
  pwm_count_fan     += FAN_PWM_STEP;

  // Tick endstops state, if required
  endstops.Tick();

}

/**
 * Timer 0 is is called 3906 timer per second.
 * It is used to update pwm values for heater and some other frequent jobs.
 *
 *  - Manage PWM to all the heaters and fan
 *  - Prepare or Measure one of the raw ADC sensor values
 *  - Step the babysteps value for each axis towards 0
 *  - For PINS_DEBUGGING, monitor and report endstop pins
 *  - For ENDSTOP_INTERRUPTS_FEATURE check endstops if flagged
 */
HAL_TEMP_TIMER_ISR {
  if (!printer.isRunning()) return;
  TEMP_OCR += 64;
  HAL_temp_isr();
}

/**
 * Interrupt Service Routines
 */
HAL_STEPPER_TIMER_ISR {

  // Set timer to maximum period
  HAL_timer_set_count(STEPPER_TIMER, HAL_TIMER_TYPE_MAX);

  // Call the ISR
  hal_timer_t ticks = stepper.Step();

  // Schedule next interrupt
  HAL_timer_set_count(STEPPER_TIMER, ticks);

}

#endif // __AVR__
