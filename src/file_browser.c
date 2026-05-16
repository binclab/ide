#include "file_browser.h"

struct _FileBrowser
{
    GtkTreeListModel* treemodel;
    GtkSelectionModel* selection;
    GtkListView* view;
};

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
   Correct signature for GtkTreeListModelCreateModelFunc
   ------------------------------------------------------- */
static GListModel* load_children(gpointer item, gpointer user_data)
{
    GFile* dir = NULL;

    /* Case 1: root item is a GFile */
    if (G_IS_FILE(item))
    {
        dir = G_FILE(item);
    }
    /* Case 2: child items are GFileInfo */
    else if (G_IS_FILE_INFO(item))
    {
        GFileInfo* info = G_FILE_INFO(item);
        GObject* obj = g_file_info_get_attribute_object(info, "standard::file");
        if (!obj) return NULL;
        dir = G_FILE(obj);
    }
    else
    {
        /* Unknown type — do nothing */
        return NULL;
    }

    GtkDirectoryList* list = gtk_directory_list_new(
        "standard::name,"
        "standard::type,"
        "standard::icon,"
        "standard::file",
        dir);

    /* Sort folders first, then alphabetical */
    GtkSorter* sorter =
        GTK_SORTER(gtk_custom_sorter_new((GCompareDataFunc)file_sort_func, NULL, NULL));
    GtkSortListModel* sorted = gtk_sort_list_model_new(G_LIST_MODEL(list), sorter);
    return G_LIST_MODEL(gtk_single_selection_new(G_LIST_MODEL(sorted)));
}

/* -------------------------------------------------------
   Row setup
   ------------------------------------------------------- */
static void setup_row(GtkListItemFactory* factory, GtkListItem* item)
{
    (void)factory;

    GtkWidget* expander = gtk_tree_expander_new();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* icon = gtk_image_new();
    GtkWidget* label = gtk_label_new(NULL);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), label);

    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), box);
    gtk_list_item_set_child(item, expander);
}

/* -------------------------------------------------------
   Row binding
   ------------------------------------------------------- */
static void bind_row(GtkListItemFactory* factory, GtkListItem* item)
{
    (void)factory;

    GtkTreeListRow* row = gtk_list_item_get_item(item);
    GtkWidget* expander = gtk_list_item_get_child(item);

    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);

    GFileInfo* info = gtk_tree_list_row_get_item(row);
    const char* name = g_file_info_get_name(info);
    GIcon* gicon = g_file_info_get_icon(info);

    GtkWidget* box = gtk_tree_expander_get_child(GTK_TREE_EXPANDER(expander));
    GtkWidget* icon = gtk_widget_get_first_child(box);
    GtkWidget* label = gtk_widget_get_last_child(box);

    gtk_image_set_from_gicon(GTK_IMAGE(icon), gicon);
    gtk_label_set_text(GTK_LABEL(label), name);
}

/* -------------------------------------------------------
   Constructor
   ------------------------------------------------------- */
FileBrowser* file_browser_new(const char* root_path)
{
    FileBrowser* fb = g_new0(FileBrowser, 1);

    GFile* root = g_file_new_for_path(root_path);

    fb->treemodel = gtk_tree_list_model_new(G_LIST_MODEL(load_children(root, NULL)),
                                            FALSE,  // do not auto-expand
                                            FALSE,  // enable expanders
                                            load_children, NULL, NULL);

    fb->selection = GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(fb->treemodel)));

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_row), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_row), NULL);

    fb->view = GTK_LIST_VIEW(gtk_list_view_new(fb->selection, factory));

    return fb;
}

GtkWidget* file_browser_get_widget(FileBrowser* fb) { return GTK_WIDGET(fb->view); }

/* -------------------------------------------------------
   Correct file selection using "standard::file"
   ------------------------------------------------------- */
GFile* file_browser_get_selected_file(FileBrowser* fb)
{
    GtkSingleSelection* sel = GTK_SINGLE_SELECTION(fb->selection);
    guint pos = gtk_single_selection_get_selected(sel);
    if (pos == GTK_INVALID_LIST_POSITION) return NULL;

    GtkTreeListRow* row = gtk_tree_list_model_get_row(fb->treemodel, pos);

    GFileInfo* info = gtk_tree_list_row_get_item(row);

    /* g_file_info_get_attribute_object returns GObject* → cast explicitly */
    GObject* obj = g_file_info_get_attribute_object(info, "standard::file");
    if (!obj) return NULL;

    return G_FILE(g_object_ref(obj));
}
