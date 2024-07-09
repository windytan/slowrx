#ifndef _VIS_H_
#define _VIS_H_

#include <glib.h>
#include <glib/gtypes.h>

extern TextStatusCallback OnVisStatusChange;
extern EventCallback OnVisIdentified;
extern EventCallback OnVisPowerComputed;
extern int VIS;
extern gboolean VisAutoStart;
extern double VisPower[2048];

guchar   GetVIS        ();

#endif
