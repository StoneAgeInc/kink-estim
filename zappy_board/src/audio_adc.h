//
// Created by Benjamin Riggs on 6/28/21.
//

#ifndef AUDIO_ADC_H
#define AUDIO_ADC_H

#include <stdbool.h>

void audio_adc_init(void);

/// @brief Begin dumping data from audio ADC to USB serial.
void audio_adc_start(void);

/// @brief Stop dumping data from audio ADC to USB serial.
void audio_adc_stop(void);

#endif //AUDIO_ADC_H
