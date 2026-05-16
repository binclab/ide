#include <gtk/gtk.h>

#include "editor.h"
#include "file_browser.h"

GHashTable* open_tabs = NULL;

static void on_paned_position_changed(GObject* obj, GParamSpec* pspec, gpointer user_data)
{
    (void)pspec;
    (void)user_data;

    GtkPaned* paned = GTK_PANED(obj);
    int pos = gtk_paned_get_position(paned);
    if (pos > 200) gtk_paned_set_position(paned, 200);
}

static void on_browser_activated(GtkListView* view, guint pos, gpointer user_data)
{
    FileBrowser* fb = user_data;
    GtkNotebook* notebook = g_object_get_data(G_OBJECT(view), "notebook");

    GFile* file = file_browser_get_selected_file(fb);
    if (!file) return;

    char* path = g_file_get_path(file);

    /* If already open → switch to tab */
    gpointer existing_page = g_hash_table_lookup(open_tabs, path);
    if (existing_page)
    {
        int page_num = gtk_notebook_page_num(notebook, GTK_WIDGET(existing_page));
        if (page_num != -1) gtk_notebook_set_current_page(notebook, page_num);

        g_free(path);
        g_object_unref(file);
        return;
    }

    /* Otherwise → open new tab */
    GtkWidget* page = editor_open_file(notebook, file);

    /* Remember it */
    g_hash_table_insert(open_tabs, path, page);

    g_object_unref(file);
}

void editor_close_tab(GtkButton* button, GtkWidget* page)
{
    GtkWidget* stack = gtk_widget_get_parent(GTK_WIDGET(page));
    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_widget_get_parent(stack));
    const char* path = g_object_get_data(G_OBJECT(page), "file-path");
    if (path) g_hash_table_remove(open_tabs, path);

    int index = gtk_notebook_page_num(notebook, page);
    if (index != -1)
    {
        gtk_notebook_remove_page(notebook, index);
    }
}

static void activate(GtkApplication* app)
{
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/com/binclab/ide/css/main.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    GtkWidget* win = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(win), 1000, 700);

    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    g_signal_connect(paned, "notify::position", G_CALLBACK(on_paned_position_changed), NULL);
    gtk_paned_set_position(GTK_PANED(paned), 200);
    gtk_window_set_child(GTK_WINDOW(win), paned);

    /* File browser */
    FileBrowser* fb = file_browser_new(".");
    GtkWidget* browser = file_browser_get_widget(fb);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), browser);

    gtk_widget_set_size_request(scroll, 200, -1);
    gtk_paned_set_start_child(GTK_PANED(paned), scroll);

    /* Notebook */
    GtkWidget* notebook = gtk_notebook_new();
    gtk_paned_set_end_child(GTK_PANED(paned), notebook);
    g_object_set_data(G_OBJECT(browser), "notebook", notebook);

    g_signal_connect(browser, "activate", G_CALLBACK(on_browser_activated), fb);

    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char** argv)
{
    GtkApplication* app = gtk_application_new("com.binclab.ide", G_APPLICATION_DEFAULT_FLAGS);
    if (!open_tabs) open_tabs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
