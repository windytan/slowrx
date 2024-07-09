#ifndef _VIS_H_
#define _VIS_H_

#include <glib.h>
#include <glib/gtypes.h>

#define VIS_POWER_SZ (2048)

extern TextStatusCallback OnVisStatusChange;
extern EventCallback OnVisIdentified;
extern EventCallback OnVisPowerComputed;
extern int VIS;
extern gboolean VisAutoStart;
extern double VisPower[VIS_POWER_SZ];

guchar   GetVIS        ();

#endif
