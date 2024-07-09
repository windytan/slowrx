#ifndef _GUI_H_
#define _GUI_H_

void     createGUI     ();
void     setVU         (double *Power, int FFTLen, int WinIdx, gboolean ShowWin);

void     evt_chooseDir     ();
void     evt_show_about    ();

#endif
