#ifndef VIEW_TEXT_TFT_H
#define VIEW_TEXT_TFT_H

// Declare the function so it can be used in other files
void TFT_text_loop();
void TFT_Draw_Text();
void setFocusOn(bool, uint32_t id = 0);
void toggleRssiDisplay();
extern bool show_avg_vario;

#endif // VIEW_TEXT_TFT_H