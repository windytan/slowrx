#ifndef _LISTEN_H_
#define _LISTEN_H_

extern TextStatusCallback OnListenerStatusChange;
extern EventCallback OnListenerWaiting;
extern EventCallback OnListenerReceivedManual;
extern EventCallback OnListenerReceiveStarted;
extern EventCallback OnListenerReceiveFSK;
extern EventCallback OnListenerAutoSlantCorrect;
extern EventCallback OnListenerReceiveFinished;
extern TextStatusCallback OnListenerReceivedFSKID;
extern gboolean ListenerAutoSlantCorrect;
extern gboolean ListenerEnableFSKID;
extern struct tm *ListenerReceiveStartTime;

void     *Listen       ();

void StartListener(void);
void WaitForListenerStop(void);

#endif
