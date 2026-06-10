#ifndef AUDIO_CLASSIFIER_H
#define AUDIO_CLASSIFIER_H

#include <stdint.h>

#define MIC_SCK_PIN       32
#define MIC_WS_PIN        13
#define MIC_SD_PIN        33
#define SQUEAK_THRESHOLD  0.80f

extern volatile bool  g_squeak_detected;
extern volatile float g_squeak_confidence;
extern volatile float g_normal_confidence;

#ifdef __cplusplus
extern "C" {
#endif

void audio_classifier_init(void);

#ifdef __cplusplus
}
#endif

#endif