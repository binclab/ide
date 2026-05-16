#include "editor.h"

typedef struct
{
    GtkSourceBuffer* buffer;
    GtkWidget* editor;
    GFile* file;
} EditorTab;

static GtkWidget* make_tab_header(const char* name, GtkWidget* page)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* label = gtk_label_new(name);
    GtkWidget* close = gtk_button_new_from_icon_name("window-close-symbolic");

    gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
    gtk_widget_set_focusable(close, FALSE);

    g_signal_connect(close, "clicked", G_CALLBACK(editor_close_tab), page);

    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), close);
    return box;
}

GtkWidget* editor_open_file(GtkNotebook* notebook, GFile* file)
{
    gchar* contents = NULL;
    gsize len = 0;
    if (!g_file_load_contents(file, NULL, &contents, &len, NULL, NULL)) return NULL;

    EditorTab* tab = g_new0(EditorTab, 1);
    tab->file = g_object_ref(file);

    tab->buffer = gtk_source_buffer_new(NULL);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(tab->buffer), contents, len);
    g_free(contents);

    tab->editor = gtk_source_view_new_with_buffer(tab->buffer);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->editor), TRUE);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tab->editor);

    char* name = g_file_get_basename(file);
    GtkWidget* header = make_tab_header(name, scroll);
    g_free(name);

    gtk_notebook_append_page(notebook, scroll, header);
    gtk_notebook_set_current_page(notebook, gtk_notebook_get_n_pages(notebook) - 1);

    const gchar* path = g_file_peek_path(file);
    g_object_set_data_full(G_OBJECT(scroll), "file-path", g_strdup(path), g_free);

    return scroll;
}
