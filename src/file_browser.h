#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H
#include <gtk/gtk.h>

typedef struct _FileBrowser FileBrowser;

FileBrowser* file_browser_new(const char* root_path);
GtkWidget* file_browser_get_widget(FileBrowser* fb);
GFile* file_browser_get_selected_file(FileBrowser* fb);
#endif /* FILE_BROWSER_H */