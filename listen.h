#ifndef _LISTEN_H_
#define _LISTEN_H_

extern TextStatusCallback OnListenerStatusChange;

void     *Listen       ();

void StartListener(void);
void WaitForListenerStop(void);

#endif
