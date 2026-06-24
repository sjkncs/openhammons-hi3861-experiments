#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H

#define BUZZER_OFF       0
#define BUZZER_ALWAYS    1
#define BUZZER_FLASH_ON  2

void OutputControl_Init(void);
void LedSet(int on);
void BuzzerSet(int style);
void BuzzerFlashToggle(void);
int GetBuzzerStyle(void);

#endif
