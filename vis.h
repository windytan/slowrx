#ifndef _VIS_H_
#define _VIS_H_

#include <glib.h>
#include <glib/gtypes.h>

extern TextStatusCallback OnVisStatusChange;
extern EventCallback OnVisIdentified;
extern UpdateVUCallback OnVisPowerComputed;
extern int VIS;
extern gboolean VisAutoStart;

guchar   GetVIS        ();

#endif
