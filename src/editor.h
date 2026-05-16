#ifndef EDITOR_H
#define EDITOR_H
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

GtkWidget* editor_open_file(GtkNotebook* notebook, GFile* file);
void editor_close_tab(GtkButton* button, GtkWidget* page);

extern GHashTable* open_tabs;
#endif /* EDITOR_H */