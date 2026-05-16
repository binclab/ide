#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include "gtk/gtkshortcut.h"

/* -------------------------------------------------------
   GLOBALS
   ------------------------------------------------------- */
static GHashTable* open_tabs = NULL;

/* File browser state */
typedef struct
{
    GFile* current_dir;
    GtkDirectoryList* dirlist;
    GtkSelectionModel* selection;
    GtkListView* view;
} FileBrowser;

static FileBrowser* file_browser = NULL;

/* -------------------------------------------------------
   PER‑TAB DATA STRUCTURE
   ------------------------------------------------------- */
typedef struct
{
    GtkSourceBuffer* buffer;
    GtkWidget* editor;
    GFile* file;
    GtkWidget* tab_header;
} EditorTab;

/* -------------------------------------------------------
   CLOSE BUTTON HANDLER
   ------------------------------------------------------- */
static void on_tab_close(GtkButton* btn, gpointer page)
{
    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(page));
    GtkNotebook* nb = GTK_NOTEBOOK(gtk_widget_get_parent(parent));

    EditorTab* tab = g_object_get_data(G_OBJECT(page), "tab-data");
    if (tab && tab->file)
    {
        char* fullpath = g_file_get_path(tab->file);
        g_hash_table_remove(open_tabs, fullpath);
        g_free(fullpath);
    }

    gint idx = gtk_notebook_page_num(nb, GTK_WIDGET(page));
    if (idx != -1) gtk_notebook_remove_page(nb, idx);
}

/* -------------------------------------------------------
   CREATE TAB HEADER (Label + Close Button)
   ------------------------------------------------------- */
static GtkWidget* make_tab_header(const char* name, GtkWidget* page)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* label = gtk_label_new(name);
    GtkWidget* close = gtk_button_new_from_icon_name("window-close-symbolic");

    gtk_button_set_has_frame(GTK_BUTTON(close), FALSE);
    gtk_widget_set_focusable(close, FALSE);

    g_signal_connect(close, "clicked", G_CALLBACK(on_tab_close), page);

    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), close);

    return box;
}

/* -------------------------------------------------------
   SAVE TAB CONTENTS
   ------------------------------------------------------- */
static void save_tab(EditorTab* tab)
{
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(tab->buffer), &start, &end);

    gchar* text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(tab->buffer), &start, &end, FALSE);

    GError* error = NULL;

    g_file_replace_contents(tab->file, text, strlen(text), NULL, FALSE, G_FILE_CREATE_NONE, NULL,
                            NULL, &error);

    if (error)
    {
        g_warning("Save failed: %s", error->message);
        g_error_free(error);
    }

    g_free(text);
}

/* -------------------------------------------------------
   KEY HANDLER (Ctrl+S)
   ------------------------------------------------------- */
static gboolean on_key_press(GtkEventControllerKey* controller, guint keyval, guint keycode,
                             GdkModifierType state, gpointer user_data)
{
    GtkNotebook* notebook = GTK_NOTEBOOK(user_data);

    if (keyval == GDK_KEY_s && (state & GDK_CONTROL_MASK))
    {
        gint page = gtk_notebook_get_current_page(notebook);
        GtkWidget* scroll = gtk_notebook_get_nth_page(notebook, page);

        EditorTab* tab = g_object_get_data(G_OBJECT(scroll), "tab-data");
        if (tab) save_tab(tab);

        return TRUE;
    }

    return FALSE;
}

/* -------------------------------------------------------
   FILE BROWSER: SETUP + BIND (icons, left‑aligned labels)
   ------------------------------------------------------- */
static void on_browser_setup(GtkListItemFactory* factory, GtkListItem* item, gpointer data)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* icon = gtk_image_new();
    GtkWidget* label = gtk_label_new(NULL);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), label);

    gtk_list_item_set_child(item, box);
}

static void on_browser_bind(GtkListItemFactory* factory, GtkListItem* item, gpointer user_data)
{
    GFileInfo* info = gtk_list_item_get_item(item);
    const char* name = g_file_info_get_name(info);
    GIcon* gicon = g_file_info_get_icon(info);

    GtkWidget* box = gtk_list_item_get_child(item);
    GtkWidget* icon = gtk_widget_get_first_child(box);
    GtkWidget* label = gtk_widget_get_last_child(box);

    gtk_image_set_from_gicon(GTK_IMAGE(icon), gicon);
    gtk_label_set_text(GTK_LABEL(label), name);
}

/* -------------------------------------------------------
   FILE BROWSER: FOLDER NAVIGATION
   ------------------------------------------------------- */
static void on_browser_item_activated(GtkListView* view, guint pos, gpointer user_data)
{
    (void)view;
    (void)user_data;

    if (!file_browser) return;

    GFileInfo* info = g_list_model_get_item(G_LIST_MODEL(file_browser->dirlist), pos);
    const char* name = g_file_info_get_name(info);

    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
    {
        GFile* new_dir = g_file_get_child(file_browser->current_dir, name);
        gtk_directory_list_set_file(file_browser->dirlist, new_dir);
        g_clear_object(&file_browser->current_dir);
        file_browser->current_dir = new_dir;
    }

    g_object_unref(info);
}

/* -------------------------------------------------------
   FILE BROWSER: CREATE FILE / FOLDER / RENAME HELPERS
   ------------------------------------------------------- */
static void file_browser_create_file(const char* name)
{
    if (!file_browser || !file_browser->current_dir) return;

    GFile* file = g_file_get_child(file_browser->current_dir, name);
    g_file_create(file, G_FILE_CREATE_NONE, NULL, NULL);
    g_object_unref(file);
}

static void file_browser_create_folder(const char* name)
{
    if (!file_browser || !file_browser->current_dir) return;

    GFile* folder = g_file_get_child(file_browser->current_dir, name);
    g_file_make_directory(folder, NULL, NULL);
    g_object_unref(folder);
}

static void file_browser_rename(const char* old_name, const char* new_name)
{
    if (!file_browser || !file_browser->current_dir) return;

    GFile* old_file = g_file_get_child(file_browser->current_dir, old_name);
    GError* err = NULL;

    GFile* new_file = g_file_set_display_name(old_file, new_name, NULL, &err);

    if (err)
    {
        g_warning("Rename failed: %s", err->message);
        g_error_free(err);
    }

    g_clear_object(&old_file);
    g_clear_object(&new_file);
}

/* -------------------------------------------------------
   FILE SELECTION HANDLER — OPEN FILE IN NEW TAB
   (also handles directory click via activation)
   ------------------------------------------------------- */
static void on_file_selected(GtkSelectionModel* sel, guint position, guint n_items,
                             gpointer user_data)
{
    (void)position;
    (void)n_items;

    GtkNotebook* notebook = GTK_NOTEBOOK(user_data);

    if (!file_browser) return;

    GtkSingleSelection* single = GTK_SINGLE_SELECTION(sel);
    guint idx = gtk_single_selection_get_selected(single);
    if (idx == GTK_INVALID_LIST_POSITION) return;

    GFileInfo* info = g_list_model_get_item(G_LIST_MODEL(single), idx);
    const char* name = g_file_info_get_name(info);

    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
    {
        /* Let activation handler handle navigation */
        g_object_unref(info);
        return;
    }

    /* Build absolute GFile */
    GFile* file = g_file_get_child(file_browser->current_dir, name);
    char* fullpath = g_file_get_path(file);

    /* Check if already open */
    GtkWidget* existing_page = g_hash_table_lookup(open_tabs, fullpath);
    if (existing_page)
    {
        gint page_num = gtk_notebook_page_num(notebook, existing_page);
        if (page_num != -1) gtk_notebook_set_current_page(notebook, page_num);

        g_free(fullpath);
        g_object_unref(info);
        g_object_unref(file);
        return;
    }

    /* Load file */
    gchar* contents = NULL;
    gsize len = 0;

    if (!g_file_load_contents(file, NULL, &contents, &len, NULL, NULL))
    {
        g_free(fullpath);
        g_object_unref(info);
        g_object_unref(file);
        return;
    }

    /* Create tab */
    EditorTab* tab = g_new0(EditorTab, 1);
    tab->file = file;

    tab->buffer = gtk_source_buffer_new(NULL);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(tab->buffer), contents, len);
    g_free(contents);

    tab->editor = gtk_source_view_new_with_buffer(tab->buffer);

    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->editor), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(tab->editor), TRUE);

    GtkSourceLanguageManager* lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage* lang = gtk_source_language_manager_guess_language(lm, name, NULL);
    if (lang) gtk_source_buffer_set_language(tab->buffer, lang);

    GtkSourceStyleSchemeManager* sm = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme* scheme = gtk_source_style_scheme_manager_get_scheme(sm, "classic");
    gtk_source_buffer_set_style_scheme(tab->buffer, scheme);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tab->editor);

    g_object_set_data_full(G_OBJECT(scroll), "tab-data", tab, g_free);

    GtkWidget* header = make_tab_header(name, scroll);

    gtk_notebook_append_page(notebook, scroll, header);
    gtk_notebook_set_current_page(notebook, gtk_notebook_get_n_pages(notebook) - 1);

    g_hash_table_insert(open_tabs, fullpath, scroll);

    g_object_unref(info);
}

/* -------------------------------------------------------
   PANED POSITION CLAMP (keep left at <= 200)
   ------------------------------------------------------- */
static void on_paned_position_changed(GObject* obj, GParamSpec* pspec, gpointer user_data)
{
    (void)pspec;
    (void)user_data;

    GtkPaned* paned = GTK_PANED(obj);
    int pos = gtk_paned_get_position(paned);
    if (pos > 200) gtk_paned_set_position(paned, 200);
}
static int file_sort_func(GFileInfo* a, GFileInfo* b, gpointer user_data)
{
    GFileType ta = g_file_info_get_file_type(a);
    GFileType tb = g_file_info_get_file_type(b);

    /* Directories first */
    if (ta == G_FILE_TYPE_DIRECTORY && tb != G_FILE_TYPE_DIRECTORY) return -1;
    if (tb == G_FILE_TYPE_DIRECTORY && ta != G_FILE_TYPE_DIRECTORY) return 1;

    /* Otherwise alphabetical */
    return g_strcmp0(g_file_info_get_name(a), g_file_info_get_name(b));
}

/* -------------------------------------------------------
   ACTIVATE CALLBACK — BUILD UI
   ------------------------------------------------------- */
static void activate(GtkApplication* app, gpointer user_data)
{
    (void)user_data;

    open_tabs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/com/binclab/ide/style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "BIDE - Bret's IDE");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_window_set_child(GTK_WINDOW(window), paned);

    /* FILE BROWSER SETUP */
    file_browser = g_new0(FileBrowser, 1);
    file_browser->current_dir = g_file_new_for_path(".");

    file_browser->dirlist = gtk_directory_list_new("standard::name,standard::type,standard::icon",
                                                   file_browser->current_dir);

    GtkSorter* sorter =
        GTK_SORTER(gtk_custom_sorter_new((GCompareDataFunc)file_sort_func, NULL, NULL));

    GtkSortListModel* sorted = gtk_sort_list_model_new(G_LIST_MODEL(file_browser->dirlist), sorter);

    file_browser->selection = GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(sorted)));

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(on_browser_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(on_browser_bind), NULL);

    file_browser->view = GTK_LIST_VIEW(gtk_list_view_new(file_browser->selection, factory));

    g_signal_connect(file_browser->view, "activate", G_CALLBACK(on_browser_item_activated), NULL);

    GtkWidget* scroll_left = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_left), GTK_WIDGET(file_browser->view));

    GtkWidget* left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(left_box, 200, -1);
    gtk_widget_set_hexpand(left_box, FALSE);
    gtk_widget_set_vexpand(left_box, TRUE);
    gtk_widget_set_halign(left_box, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(left_box), scroll_left);
    gtk_widget_set_hexpand(scroll_left, TRUE);
    gtk_widget_set_vexpand(scroll_left, TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), left_box);

    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), TRUE);

    g_signal_connect(paned, "notify::position", G_CALLBACK(on_paned_position_changed), NULL);
    gtk_paned_set_position(GTK_PANED(paned), 200);

    /* RIGHT PANEL: NOTEBOOK */
    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_widget_set_hexpand(notebook, TRUE);
    gtk_paned_set_end_child(GTK_PANED(paned), notebook);

    /* Connect file selection to open files in tabs */
    g_signal_connect(file_browser->selection, "selection-changed", G_CALLBACK(on_file_selected),
                     notebook);

    /* Ctrl+S handler */
    GtkEventController* key = gtk_event_controller_key_new();
    g_signal_connect(key, "key-pressed", G_CALLBACK(on_key_press), notebook);
    gtk_widget_add_controller(window, key);

    gtk_window_present(GTK_WINDOW(window));
}

/* -------------------------------------------------------
   MAIN
   ------------------------------------------------------- */
int main(int argc, char** argv)
{
    GtkApplication* app = gtk_application_new("com.binclab.ide", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
