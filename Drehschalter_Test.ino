#define ROT_SWITCH_POS_NUM 12
#define ROT_SWITCH_PIN  A0

uint8_t currentPos = 0;

uint8_t getSwitchPosition ()
{
  float analogValue = analogRead(ROT_SWITCH_PIN);;
  float stepMin = (5/ROT_SWITCH_POS_NUM) - ((5/ROT_SWITCH_POS_NUM)*0.15);
  float stepMax = (5/ROT_SWITCH_POS_NUM) - ((5/ROT_SWITCH_POS_NUM)*0.15);
  
   for (uint8_t x = 0; x <= ROT_SWITCH_POS_NUM; x++)
  {
    if (analogValue > stepMin*x || analogValue < stepMax*x)
      currentPos = x;
      return currentPos;
  }
}

