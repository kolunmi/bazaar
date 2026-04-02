/* bge-wdgt-spec.c
 *
 * Copyright 2026 Eva M
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtk/gtk.h>

#include "bge-marshalers.h"
#include "bge.h"
#include "fmt/parser.h"
#include "graphene-gobject.h"
#include "util.h"

typedef enum
{
  VALUE_OBJECT = 0,
  VALUE_CHILD,
  VALUE_CONSTANT,
  VALUE_COMPONENT,
  VALUE_SPECIAL,
  VALUE_VARIABLE,
  VALUE_PROPERTY,
} ValueKind;

typedef void (*SnapshotCallFunc) (GtkSnapshot *snapshot,
                                  const GValue args[],
                                  const GValue rest[],
                                  guint        n_rest);

BGE_DEFINE_DATA (
    value,
    Value,
    {
      char     *name;
      GType     type;
      ValueKind kind;
      // union
      struct
      {
        GValue              constant;
        GPtrArray          *component;
        BgeWdgtSpecialValue special;
        /* Variables are just used as a proxy */
        struct
        {
          ValueData  *object;
          char       *prop_name;
          GParamFlags pspec_flags;
        } property;
      };
    },
    g_value_unset (&self->constant);
    BGE_RELEASE_DATA (name, g_free);
    BGE_RELEASE_DATA (component, g_ptr_array_unref);
    BGE_RELEASE_DATA (property.object, value_data_unref);
    BGE_RELEASE_DATA (property.prop_name, g_free))

BGE_DEFINE_DATA (
    snapshot_call,
    SnapshotCall,
    {
      BgeWdgtSnapshotInstrKind kind;
      SnapshotCallFunc         func;
      GPtrArray               *args;
      GPtrArray               *rest;
    },
    BGE_RELEASE_DATA (args, g_ptr_array_unref);
    BGE_RELEASE_DATA (rest, g_ptr_array_unref))

BGE_DEFINE_DATA (
    snapshot,
    Snapshot,
    {
      GPtrArray *calls;
    },
    BGE_RELEASE_DATA (calls, g_ptr_array_unref))

BGE_DEFINE_DATA (
    state,
    State,
    {
      char         *name;
      GHashTable   *setters;
      SnapshotData *snapshot;
    },
    BGE_RELEASE_DATA (name, g_free);
    BGE_RELEASE_DATA (setters, g_hash_table_unref);
    BGE_RELEASE_DATA (snapshot, snapshot_data_unref))

/* --------------------------- */
/* Spec Builder Implementation */
/* --------------------------- */

struct _BgeWdgtSpec
{
  GObject parent_instance;

  char *name;

  gboolean ready;

  GHashTable *values;
  GHashTable *states;
  GPtrArray  *children;
  GPtrArray  *nonchildren;

  StateData *default_state;
  struct
  {
    ValueData *motion_x;
    ValueData *motion_y;
  } special_values;
};

G_DEFINE_FINAL_TYPE (BgeWdgtSpec, bge_wdgt_spec, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_NAME,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

typedef struct
{
  const char      *name;
  guint            n_args;
  guint            n_rest;
  GType            args[16];
  gpointer         func;
  SnapshotCallFunc call;
} SnapshotInstr;

static gboolean
lookup_snapshot_push_instr (const char    *lookup_name,
                            SnapshotInstr *out);
static gboolean
lookup_snapshot_transform_instr (const char    *lookup_name,
                                 SnapshotInstr *out);
static gboolean
lookup_snapshot_append_instr (const char    *lookup_name,
                              SnapshotInstr *out);

static ValueData *
dig_variable_value (ValueData *value,
                    StateData *state);

static void
ensure_state_snapshot (StateData *state);

static void
bge_wdgt_spec_dispose (GObject *object)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  g_clear_pointer (&self->name, g_free);

  g_clear_pointer (&self->values, g_hash_table_unref);
  g_clear_pointer (&self->states, g_hash_table_unref);
  g_clear_pointer (&self->children, g_ptr_array_unref);
  g_clear_pointer (&self->nonchildren, g_ptr_array_unref);

  g_clear_pointer (&self->default_state, state_data_unref);

  G_OBJECT_CLASS (bge_wdgt_spec_parent_class)->dispose (object);
}

static void
bge_wdgt_spec_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, bge_wdgt_spec_get_name (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_spec_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  switch (prop_id)
    {
    case PROP_NAME:
      bge_wdgt_spec_set_name (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_spec_class_init (BgeWdgtSpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bge_wdgt_spec_set_property;
  object_class->get_property = bge_wdgt_spec_get_property;
  object_class->dispose      = bge_wdgt_spec_dispose;

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /* Make sure these names are available for the parser */
  g_type_ensure (GRAPHENE_TYPE_POINT);
  g_type_ensure (GRAPHENE_TYPE_POINT3D);
  g_type_ensure (GRAPHENE_TYPE_SIZE);
  g_type_ensure (GRAPHENE_TYPE_RECT);
  g_type_ensure (GRAPHENE_TYPE_VEC2);
  g_type_ensure (GRAPHENE_TYPE_VEC3);
  g_type_ensure (GRAPHENE_TYPE_VEC4);
  g_type_ensure (GRAPHENE_TYPE_QUAD);
  g_type_ensure (GRAPHENE_TYPE_QUATERNION);
  g_type_ensure (GRAPHENE_TYPE_MATRIX);
  g_type_ensure (GRAPHENE_TYPE_PLANE);
  g_type_ensure (GRAPHENE_TYPE_FRUSTUM);
  g_type_ensure (GRAPHENE_TYPE_SPHERE);
  g_type_ensure (GRAPHENE_TYPE_BOX);
  g_type_ensure (GRAPHENE_TYPE_TRIANGLE);
  g_type_ensure (GRAPHENE_TYPE_EULER);
  g_type_ensure (GRAPHENE_TYPE_RAY);

  /* Ensure GTK names are available for the parser */
  g_type_ensure (gtk_at_context_get_type ());
  g_type_ensure (gtk_about_dialog_get_type ());
  g_type_ensure (gtk_accessible_get_type ());
  g_type_ensure (gtk_accessible_announcement_priority_get_type ());
  g_type_ensure (gtk_accessible_autocomplete_get_type ());
  g_type_ensure (gtk_accessible_invalid_state_get_type ());
  g_type_ensure (gtk_accessible_list_get_type ());
  g_type_ensure (gtk_accessible_platform_state_get_type ());
  g_type_ensure (gtk_accessible_property_get_type ());
  g_type_ensure (gtk_accessible_range_get_type ());
  g_type_ensure (gtk_accessible_relation_get_type ());
  g_type_ensure (gtk_accessible_role_get_type ());
  g_type_ensure (gtk_accessible_sort_get_type ());
  g_type_ensure (gtk_accessible_state_get_type ());
  g_type_ensure (gtk_accessible_text_get_type ());
  g_type_ensure (gtk_accessible_text_content_change_get_type ());
  g_type_ensure (gtk_accessible_text_granularity_get_type ());
  g_type_ensure (gtk_accessible_tristate_get_type ());
  g_type_ensure (gtk_action_bar_get_type ());
  g_type_ensure (gtk_actionable_get_type ());
  g_type_ensure (gtk_activate_action_get_type ());
  g_type_ensure (gtk_adjustment_get_type ());
  g_type_ensure (gtk_alert_dialog_get_type ());
  g_type_ensure (gtk_align_get_type ());
  g_type_ensure (gtk_alternative_trigger_get_type ());
  g_type_ensure (gtk_any_filter_get_type ());
  g_type_ensure (gtk_app_chooser_get_type ());
  g_type_ensure (gtk_app_chooser_button_get_type ());
  g_type_ensure (gtk_app_chooser_dialog_get_type ());
  g_type_ensure (gtk_app_chooser_widget_get_type ());
  g_type_ensure (gtk_application_get_type ());
  g_type_ensure (gtk_application_inhibit_flags_get_type ());
  g_type_ensure (gtk_application_window_get_type ());
  g_type_ensure (gtk_arrow_type_get_type ());
  g_type_ensure (gtk_aspect_frame_get_type ());
  g_type_ensure (gtk_assistant_get_type ());
  g_type_ensure (gtk_assistant_page_get_type ());
  g_type_ensure (gtk_assistant_page_type_get_type ());
  g_type_ensure (gtk_baseline_position_get_type ());
  g_type_ensure (gtk_bin_layout_get_type ());
  g_type_ensure (gtk_bitset_get_type ());
  g_type_ensure (gtk_bitset_iter_get_type ());
  g_type_ensure (gtk_bookmark_list_get_type ());
  g_type_ensure (gtk_bool_filter_get_type ());
  g_type_ensure (gtk_border_get_type ());
  g_type_ensure (gtk_border_style_get_type ());
  g_type_ensure (gtk_box_get_type ());
  g_type_ensure (gtk_box_layout_get_type ());
  g_type_ensure (gtk_buildable_get_type ());
  g_type_ensure (gtk_builder_get_type ());
  g_type_ensure (gtk_builder_cscope_get_type ());
  g_type_ensure (gtk_builder_closure_flags_get_type ());
  g_type_ensure (gtk_builder_error_get_type ());
  g_type_ensure (gtk_builder_list_item_factory_get_type ());
  g_type_ensure (gtk_builder_scope_get_type ());
  g_type_ensure (gtk_button_get_type ());
  g_type_ensure (gtk_buttons_type_get_type ());
  g_type_ensure (gtk_cclosure_expression_get_type ());
  g_type_ensure (gtk_calendar_get_type ());
  g_type_ensure (gtk_callback_action_get_type ());
  g_type_ensure (gtk_cell_area_get_type ());
  g_type_ensure (gtk_cell_area_box_get_type ());
  g_type_ensure (gtk_cell_area_context_get_type ());
  g_type_ensure (gtk_cell_editable_get_type ());
  g_type_ensure (gtk_cell_layout_get_type ());
  g_type_ensure (gtk_cell_renderer_get_type ());
  g_type_ensure (gtk_cell_renderer_accel_get_type ());
  g_type_ensure (gtk_cell_renderer_accel_mode_get_type ());
  g_type_ensure (gtk_cell_renderer_combo_get_type ());
  g_type_ensure (gtk_cell_renderer_mode_get_type ());
  g_type_ensure (gtk_cell_renderer_pixbuf_get_type ());
  g_type_ensure (gtk_cell_renderer_progress_get_type ());
  g_type_ensure (gtk_cell_renderer_spin_get_type ());
  g_type_ensure (gtk_cell_renderer_spinner_get_type ());
  g_type_ensure (gtk_cell_renderer_state_get_type ());
  g_type_ensure (gtk_cell_renderer_text_get_type ());
  g_type_ensure (gtk_cell_renderer_toggle_get_type ());
  g_type_ensure (gtk_cell_view_get_type ());
  g_type_ensure (gtk_center_box_get_type ());
  g_type_ensure (gtk_center_layout_get_type ());
  g_type_ensure (gtk_check_button_get_type ());
  g_type_ensure (gtk_closure_expression_get_type ());
  g_type_ensure (gtk_collation_get_type ());
  g_type_ensure (gtk_color_button_get_type ());
  g_type_ensure (gtk_color_chooser_get_type ());
  g_type_ensure (gtk_color_chooser_dialog_get_type ());
  g_type_ensure (gtk_color_chooser_widget_get_type ());
  g_type_ensure (gtk_color_dialog_get_type ());
  g_type_ensure (gtk_color_dialog_button_get_type ());
  g_type_ensure (gtk_column_view_get_type ());
  g_type_ensure (gtk_column_view_cell_get_type ());
  g_type_ensure (gtk_column_view_column_get_type ());
  g_type_ensure (gtk_column_view_row_get_type ());
  g_type_ensure (gtk_column_view_sorter_get_type ());
  g_type_ensure (gtk_combo_box_get_type ());
  g_type_ensure (gtk_combo_box_text_get_type ());
  g_type_ensure (gtk_constant_expression_get_type ());
  g_type_ensure (gtk_constraint_get_type ());
  g_type_ensure (gtk_constraint_attribute_get_type ());
  g_type_ensure (gtk_constraint_guide_get_type ());
  g_type_ensure (gtk_constraint_layout_get_type ());
  g_type_ensure (gtk_constraint_layout_child_get_type ());
  g_type_ensure (gtk_constraint_relation_get_type ());
  g_type_ensure (gtk_constraint_strength_get_type ());
  g_type_ensure (gtk_constraint_target_get_type ());
  g_type_ensure (gtk_constraint_vfl_parser_error_get_type ());
  g_type_ensure (gtk_content_fit_get_type ());
  g_type_ensure (gtk_corner_type_get_type ());
  g_type_ensure (gtk_css_provider_get_type ());
  g_type_ensure (gtk_css_section_get_type ());
  g_type_ensure (gtk_custom_filter_get_type ());
  g_type_ensure (gtk_custom_layout_get_type ());
  g_type_ensure (gtk_custom_sorter_get_type ());
  g_type_ensure (gtk_debug_flags_get_type ());
  g_type_ensure (gtk_delete_type_get_type ());
  g_type_ensure (gtk_dialog_get_type ());
  g_type_ensure (gtk_dialog_error_get_type ());
  g_type_ensure (gtk_dialog_flags_get_type ());
  g_type_ensure (gtk_direction_type_get_type ());
  g_type_ensure (gtk_directory_list_get_type ());
  g_type_ensure (gtk_drag_icon_get_type ());
  g_type_ensure (gtk_drag_source_get_type ());
  g_type_ensure (gtk_drawing_area_get_type ());
  g_type_ensure (gtk_drop_controller_motion_get_type ());
  g_type_ensure (gtk_drop_down_get_type ());
  g_type_ensure (gtk_drop_target_get_type ());
  g_type_ensure (gtk_drop_target_async_get_type ());
  g_type_ensure (gtk_editable_get_type ());
  g_type_ensure (gtk_editable_label_get_type ());
  g_type_ensure (gtk_editable_properties_get_type ());
  g_type_ensure (gtk_emoji_chooser_get_type ());
  g_type_ensure (gtk_entry_get_type ());
  g_type_ensure (gtk_entry_buffer_get_type ());
  g_type_ensure (gtk_entry_completion_get_type ());
  g_type_ensure (gtk_entry_icon_position_get_type ());
  g_type_ensure (gtk_event_controller_get_type ());
  g_type_ensure (gtk_event_controller_focus_get_type ());
  g_type_ensure (gtk_event_controller_key_get_type ());
  g_type_ensure (gtk_event_controller_legacy_get_type ());
  g_type_ensure (gtk_event_controller_motion_get_type ());
  g_type_ensure (gtk_event_controller_scroll_get_type ());
  g_type_ensure (gtk_event_controller_scroll_flags_get_type ());
  g_type_ensure (gtk_event_sequence_state_get_type ());
  g_type_ensure (gtk_every_filter_get_type ());
  g_type_ensure (gtk_expander_get_type ());
  g_type_ensure (gtk_expression_get_type ());
  g_type_ensure (gtk_expression_watch_get_type ());
  g_type_ensure (gtk_file_chooser_get_type ());
  g_type_ensure (gtk_file_chooser_action_get_type ());
  g_type_ensure (gtk_file_chooser_dialog_get_type ());
  g_type_ensure (gtk_file_chooser_error_get_type ());
  g_type_ensure (gtk_file_chooser_native_get_type ());
  g_type_ensure (gtk_file_chooser_widget_get_type ());
  g_type_ensure (gtk_file_dialog_get_type ());
  g_type_ensure (gtk_file_filter_get_type ());
  g_type_ensure (gtk_file_launcher_get_type ());
  g_type_ensure (gtk_filter_get_type ());
  g_type_ensure (gtk_filter_change_get_type ());
  g_type_ensure (gtk_filter_list_model_get_type ());
  g_type_ensure (gtk_filter_match_get_type ());
  g_type_ensure (gtk_fixed_get_type ());
  g_type_ensure (gtk_fixed_layout_get_type ());
  g_type_ensure (gtk_fixed_layout_child_get_type ());
  g_type_ensure (gtk_flatten_list_model_get_type ());
  g_type_ensure (gtk_flow_box_get_type ());
  g_type_ensure (gtk_flow_box_child_get_type ());
  g_type_ensure (gtk_font_button_get_type ());
  g_type_ensure (gtk_font_chooser_get_type ());
  g_type_ensure (gtk_font_chooser_dialog_get_type ());
  g_type_ensure (gtk_font_chooser_level_get_type ());
  g_type_ensure (gtk_font_chooser_widget_get_type ());
  g_type_ensure (gtk_font_dialog_get_type ());
  g_type_ensure (gtk_font_dialog_button_get_type ());
  g_type_ensure (gtk_font_level_get_type ());
  g_type_ensure (gtk_font_rendering_get_type ());
  g_type_ensure (gtk_frame_get_type ());
  g_type_ensure (gtk_gl_area_get_type ());
  g_type_ensure (gtk_gesture_get_type ());
  g_type_ensure (gtk_gesture_click_get_type ());
  g_type_ensure (gtk_gesture_drag_get_type ());
  g_type_ensure (gtk_gesture_long_press_get_type ());
  g_type_ensure (gtk_gesture_pan_get_type ());
  g_type_ensure (gtk_gesture_rotate_get_type ());
  g_type_ensure (gtk_gesture_single_get_type ());
  g_type_ensure (gtk_gesture_stylus_get_type ());
  g_type_ensure (gtk_gesture_swipe_get_type ());
  g_type_ensure (gtk_gesture_zoom_get_type ());
  g_type_ensure (gtk_graphics_offload_get_type ());
  g_type_ensure (gtk_graphics_offload_enabled_get_type ());
  g_type_ensure (gtk_grid_get_type ());
  g_type_ensure (gtk_grid_layout_get_type ());
  g_type_ensure (gtk_grid_layout_child_get_type ());
  g_type_ensure (gtk_grid_view_get_type ());
  g_type_ensure (gtk_header_bar_get_type ());
  g_type_ensure (gtk_im_context_get_type ());
  g_type_ensure (gtk_im_context_simple_get_type ());
  g_type_ensure (gtk_im_multicontext_get_type ());
  g_type_ensure (gtk_icon_lookup_flags_get_type ());
  g_type_ensure (gtk_icon_paintable_get_type ());
  g_type_ensure (gtk_icon_size_get_type ());
  g_type_ensure (gtk_icon_theme_get_type ());
  g_type_ensure (gtk_icon_theme_error_get_type ());
  g_type_ensure (gtk_icon_view_get_type ());
  g_type_ensure (gtk_icon_view_drop_position_get_type ());
  g_type_ensure (gtk_image_get_type ());
  g_type_ensure (gtk_image_type_get_type ());
  g_type_ensure (gtk_info_bar_get_type ());
  g_type_ensure (gtk_input_hints_get_type ());
  g_type_ensure (gtk_input_purpose_get_type ());
  g_type_ensure (gtk_inscription_get_type ());
  g_type_ensure (gtk_inscription_overflow_get_type ());
  g_type_ensure (gtk_interface_color_scheme_get_type ());
  g_type_ensure (gtk_interface_contrast_get_type ());
  g_type_ensure (gtk_justification_get_type ());
  g_type_ensure (gtk_keyval_trigger_get_type ());
  g_type_ensure (gtk_label_get_type ());
  g_type_ensure (gtk_layout_child_get_type ());
  g_type_ensure (gtk_layout_manager_get_type ());
  g_type_ensure (gtk_level_bar_get_type ());
  g_type_ensure (gtk_level_bar_mode_get_type ());
  g_type_ensure (gtk_license_get_type ());
  g_type_ensure (gtk_link_button_get_type ());
  g_type_ensure (gtk_list_base_get_type ());
  g_type_ensure (gtk_list_box_get_type ());
  g_type_ensure (gtk_list_box_row_get_type ());
  g_type_ensure (gtk_list_header_get_type ());
  g_type_ensure (gtk_list_item_get_type ());
  g_type_ensure (gtk_list_item_factory_get_type ());
  g_type_ensure (gtk_list_scroll_flags_get_type ());
  g_type_ensure (gtk_list_store_get_type ());
  g_type_ensure (gtk_list_tab_behavior_get_type ());
  g_type_ensure (gtk_list_view_get_type ());
  g_type_ensure (gtk_lock_button_get_type ());
  g_type_ensure (gtk_map_list_model_get_type ());
  g_type_ensure (gtk_media_controls_get_type ());
  g_type_ensure (gtk_media_file_get_type ());
  g_type_ensure (gtk_media_stream_get_type ());
  g_type_ensure (gtk_menu_button_get_type ());
  g_type_ensure (gtk_message_dialog_get_type ());
  g_type_ensure (gtk_message_type_get_type ());
  g_type_ensure (gtk_mnemonic_action_get_type ());
  g_type_ensure (gtk_mnemonic_trigger_get_type ());
  g_type_ensure (gtk_mount_operation_get_type ());
  g_type_ensure (gtk_movement_step_get_type ());
  g_type_ensure (gtk_multi_filter_get_type ());
  g_type_ensure (gtk_multi_selection_get_type ());
  g_type_ensure (gtk_multi_sorter_get_type ());
  g_type_ensure (gtk_named_action_get_type ());
  g_type_ensure (gtk_native_get_type ());
  g_type_ensure (gtk_native_dialog_get_type ());
  g_type_ensure (gtk_natural_wrap_mode_get_type ());
  g_type_ensure (gtk_never_trigger_get_type ());
  g_type_ensure (gtk_no_selection_get_type ());
  g_type_ensure (gtk_notebook_get_type ());
  g_type_ensure (gtk_notebook_page_get_type ());
  g_type_ensure (gtk_notebook_tab_get_type ());
  g_type_ensure (gtk_nothing_action_get_type ());
  g_type_ensure (gtk_number_up_layout_get_type ());
  g_type_ensure (gtk_numeric_sorter_get_type ());
  g_type_ensure (gtk_object_expression_get_type ());
  g_type_ensure (gtk_ordering_get_type ());
  g_type_ensure (gtk_orientable_get_type ());
  g_type_ensure (gtk_orientation_get_type ());
  g_type_ensure (gtk_overflow_get_type ());
  g_type_ensure (gtk_overlay_get_type ());
  g_type_ensure (gtk_overlay_layout_get_type ());
  g_type_ensure (gtk_overlay_layout_child_get_type ());
  g_type_ensure (gtk_pack_type_get_type ());
  g_type_ensure (gtk_pad_action_type_get_type ());
  g_type_ensure (gtk_pad_controller_get_type ());
  g_type_ensure (gtk_page_orientation_get_type ());
  g_type_ensure (gtk_page_set_get_type ());
  g_type_ensure (gtk_page_setup_get_type ());
  g_type_ensure (gtk_pan_direction_get_type ());
  g_type_ensure (gtk_paned_get_type ());
  g_type_ensure (gtk_paper_size_get_type ());
  g_type_ensure (gtk_param_expression_get_type ());
  g_type_ensure (gtk_password_entry_get_type ());
  g_type_ensure (gtk_password_entry_buffer_get_type ());
  g_type_ensure (gtk_pick_flags_get_type ());
  g_type_ensure (gtk_picture_get_type ());
  g_type_ensure (gtk_policy_type_get_type ());
  g_type_ensure (gtk_popover_get_type ());
  g_type_ensure (gtk_popover_menu_get_type ());
  g_type_ensure (gtk_popover_menu_bar_get_type ());
  g_type_ensure (gtk_popover_menu_flags_get_type ());
  g_type_ensure (gtk_position_type_get_type ());
  g_type_ensure (gtk_print_context_get_type ());
  g_type_ensure (gtk_print_dialog_get_type ());
  g_type_ensure (gtk_print_duplex_get_type ());
  g_type_ensure (gtk_print_error_get_type ());
  g_type_ensure (gtk_print_operation_get_type ());
  g_type_ensure (gtk_print_operation_action_get_type ());
  g_type_ensure (gtk_print_operation_preview_get_type ());
  g_type_ensure (gtk_print_operation_result_get_type ());
  g_type_ensure (gtk_print_pages_get_type ());
  g_type_ensure (gtk_print_quality_get_type ());
  g_type_ensure (gtk_print_settings_get_type ());
  g_type_ensure (gtk_print_setup_get_type ());
  g_type_ensure (gtk_print_status_get_type ());
  g_type_ensure (gtk_progress_bar_get_type ());
  g_type_ensure (gtk_propagation_limit_get_type ());
  g_type_ensure (gtk_propagation_phase_get_type ());
  g_type_ensure (gtk_property_expression_get_type ());
  g_type_ensure (gtk_range_get_type ());
  g_type_ensure (gtk_recent_info_get_type ());
  g_type_ensure (gtk_recent_manager_get_type ());
  g_type_ensure (gtk_recent_manager_error_get_type ());
  g_type_ensure (gtk_requisition_get_type ());
  g_type_ensure (gtk_response_type_get_type ());
  g_type_ensure (gtk_revealer_get_type ());
  g_type_ensure (gtk_revealer_transition_type_get_type ());
  g_type_ensure (gtk_root_get_type ());
  g_type_ensure (gtk_scale_get_type ());
  g_type_ensure (gtk_scale_button_get_type ());
  g_type_ensure (gtk_scroll_info_get_type ());
  g_type_ensure (gtk_scroll_step_get_type ());
  g_type_ensure (gtk_scroll_type_get_type ());
  g_type_ensure (gtk_scrollable_get_type ());
  g_type_ensure (gtk_scrollable_policy_get_type ());
  g_type_ensure (gtk_scrollbar_get_type ());
  g_type_ensure (gtk_scrolled_window_get_type ());
  g_type_ensure (gtk_search_bar_get_type ());
  g_type_ensure (gtk_search_entry_get_type ());
  g_type_ensure (gtk_section_model_get_type ());
  g_type_ensure (gtk_selection_filter_model_get_type ());
  g_type_ensure (gtk_selection_mode_get_type ());
  g_type_ensure (gtk_selection_model_get_type ());
  g_type_ensure (gtk_sensitivity_type_get_type ());
  g_type_ensure (gtk_separator_get_type ());
  g_type_ensure (gtk_settings_get_type ());
  g_type_ensure (gtk_shortcut_get_type ());
  g_type_ensure (gtk_shortcut_action_get_type ());
  g_type_ensure (gtk_shortcut_action_flags_get_type ());
  g_type_ensure (gtk_shortcut_controller_get_type ());
  g_type_ensure (gtk_shortcut_label_get_type ());
  g_type_ensure (gtk_shortcut_manager_get_type ());
  g_type_ensure (gtk_shortcut_scope_get_type ());
  g_type_ensure (gtk_shortcut_trigger_get_type ());
  g_type_ensure (gtk_shortcut_type_get_type ());
  g_type_ensure (gtk_shortcuts_group_get_type ());
  g_type_ensure (gtk_shortcuts_section_get_type ());
  g_type_ensure (gtk_shortcuts_shortcut_get_type ());
  g_type_ensure (gtk_shortcuts_window_get_type ());
  g_type_ensure (gtk_signal_action_get_type ());
  g_type_ensure (gtk_signal_list_item_factory_get_type ());
  g_type_ensure (gtk_single_selection_get_type ());
  g_type_ensure (gtk_size_group_get_type ());
  g_type_ensure (gtk_size_group_mode_get_type ());
  g_type_ensure (gtk_size_request_mode_get_type ());
  g_type_ensure (gtk_slice_list_model_get_type ());
  g_type_ensure (gtk_snapshot_get_type ());
  g_type_ensure (gtk_sort_list_model_get_type ());
  g_type_ensure (gtk_sort_type_get_type ());
  g_type_ensure (gtk_sorter_get_type ());
  g_type_ensure (gtk_sorter_change_get_type ());
  g_type_ensure (gtk_sorter_order_get_type ());
  g_type_ensure (gtk_spin_button_get_type ());
  g_type_ensure (gtk_spin_button_update_policy_get_type ());
  g_type_ensure (gtk_spin_type_get_type ());
  g_type_ensure (gtk_spinner_get_type ());
  g_type_ensure (gtk_stack_get_type ());
  g_type_ensure (gtk_stack_page_get_type ());
  g_type_ensure (gtk_stack_sidebar_get_type ());
  g_type_ensure (gtk_stack_switcher_get_type ());
  g_type_ensure (gtk_stack_transition_type_get_type ());
  g_type_ensure (gtk_state_flags_get_type ());
  g_type_ensure (gtk_statusbar_get_type ());
  g_type_ensure (gtk_string_filter_get_type ());
  g_type_ensure (gtk_string_filter_match_mode_get_type ());
  g_type_ensure (gtk_string_list_get_type ());
  g_type_ensure (gtk_string_object_get_type ());
  g_type_ensure (gtk_string_sorter_get_type ());
  g_type_ensure (gtk_style_context_get_type ());
  g_type_ensure (gtk_style_context_print_flags_get_type ());
  g_type_ensure (gtk_style_provider_get_type ());
  g_type_ensure (gtk_switch_get_type ());
  g_type_ensure (gtk_symbolic_color_get_type ());
  g_type_ensure (gtk_symbolic_paintable_get_type ());
  g_type_ensure (gtk_system_setting_get_type ());
  g_type_ensure (gtk_text_get_type ());
  g_type_ensure (gtk_text_buffer_get_type ());
  g_type_ensure (gtk_text_buffer_notify_flags_get_type ());
  g_type_ensure (gtk_text_child_anchor_get_type ());
  g_type_ensure (gtk_text_direction_get_type ());
  g_type_ensure (gtk_text_extend_selection_get_type ());
  g_type_ensure (gtk_text_iter_get_type ());
  g_type_ensure (gtk_text_mark_get_type ());
  g_type_ensure (gtk_text_search_flags_get_type ());
  g_type_ensure (gtk_text_tag_get_type ());
  g_type_ensure (gtk_text_tag_table_get_type ());
  g_type_ensure (gtk_text_view_get_type ());
  g_type_ensure (gtk_text_view_layer_get_type ());
  g_type_ensure (gtk_text_window_type_get_type ());
  g_type_ensure (gtk_toggle_button_get_type ());
  g_type_ensure (gtk_tooltip_get_type ());
  g_type_ensure (gtk_tree_expander_get_type ());
  g_type_ensure (gtk_tree_iter_get_type ());
  g_type_ensure (gtk_tree_list_model_get_type ());
  g_type_ensure (gtk_tree_list_row_get_type ());
  g_type_ensure (gtk_tree_list_row_sorter_get_type ());
  g_type_ensure (gtk_tree_model_get_type ());
  g_type_ensure (gtk_tree_model_filter_get_type ());
  g_type_ensure (gtk_tree_model_flags_get_type ());
  g_type_ensure (gtk_tree_model_sort_get_type ());
  g_type_ensure (gtk_tree_path_get_type ());
  g_type_ensure (gtk_tree_row_reference_get_type ());
  g_type_ensure (gtk_tree_selection_get_type ());
  g_type_ensure (gtk_tree_sortable_get_type ());
  g_type_ensure (gtk_tree_store_get_type ());
  g_type_ensure (gtk_tree_view_get_type ());
  g_type_ensure (gtk_tree_view_column_get_type ());
  g_type_ensure (gtk_tree_view_column_sizing_get_type ());
  g_type_ensure (gtk_tree_view_drop_position_get_type ());
  g_type_ensure (gtk_tree_view_grid_lines_get_type ());
  g_type_ensure (gtk_unit_get_type ());
  g_type_ensure (gtk_uri_launcher_get_type ());
  g_type_ensure (gtk_video_get_type ());
  g_type_ensure (gtk_viewport_get_type ());
  g_type_ensure (gtk_volume_button_get_type ());
  g_type_ensure (gtk_widget_get_type ());
  g_type_ensure (gtk_widget_paintable_get_type ());
  g_type_ensure (gtk_window_get_type ());
  g_type_ensure (gtk_window_controls_get_type ());
  g_type_ensure (gtk_window_gravity_get_type ());
  g_type_ensure (gtk_window_group_get_type ());
  g_type_ensure (gtk_window_handle_get_type ());
  g_type_ensure (gtk_wrap_mode_get_type ());
}

static void
bge_wdgt_spec_init (BgeWdgtSpec *self)
{
  self->values      = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, value_data_unref);
  self->states      = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, state_data_unref);
  self->children    = g_ptr_array_new_with_free_func (value_data_unref);
  self->nonchildren = g_ptr_array_new_with_free_func (value_data_unref);
}

BgeWdgtSpec *
bge_wdgt_spec_new (void)
{
  return g_object_new (BGE_TYPE_WDGT_SPEC, NULL);
}

BgeWdgtSpec *
bge_wdgt_spec_new_for_string (const char *string,
                              GError    **error)
{
  g_return_val_if_fail (string != NULL, NULL);
  return bge_wdgt_parse_string (string, error);
}

BgeWdgtSpec *
bge_wdgt_spec_new_for_resource (const char *resource,
                                GError    **error)
{
  g_autoptr (GBytes) bytes = NULL;
  gsize         size       = 0;
  gconstpointer data       = NULL;

  g_return_val_if_fail (resource != NULL, NULL);

  bytes = g_resources_lookup_data (resource, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
  if (bytes == NULL)
    return NULL;

  data = g_bytes_get_data (bytes, &size);
  return bge_wdgt_parse_string ((const char *) data, error);
}

const char *
bge_wdgt_spec_get_name (BgeWdgtSpec *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), NULL);
  return self->name;
}

void
bge_wdgt_spec_set_name (BgeWdgtSpec *self,
                        const char  *name)
{
  g_return_if_fail (BGE_IS_WDGT_SPEC (self));

  if (name == self->name || (name != NULL && self->name != NULL && g_strcmp0 (name, self->name) == 0))
    return;

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

void
bge_wdgt_spec_set_name_take (BgeWdgtSpec *self,
                             char        *name)
{
  g_return_if_fail (BGE_IS_WDGT_SPEC (self));

  if (name != NULL && self->name != NULL && g_strcmp0 (name, self->name) == 0)
    {
      g_free (name);
      return;
    }

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = name;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

gboolean
bge_wdgt_spec_add_constant_source_value (BgeWdgtSpec  *self,
                                         const char   *name,
                                         const GValue *constant,
                                         GError      **error)
{
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (constant != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  value       = value_data_new ();
  value->name = g_strdup (name);
  value->type = constant->g_type;
  value->kind = VALUE_CONSTANT;
  g_value_copy (constant, g_value_init (&value->constant, constant->g_type));

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_component_source_value (BgeWdgtSpec       *self,
                                          const char        *name,
                                          GType              type,
                                          const char *const *components,
                                          guint              n_components,
                                          GError           **error)
{
  GType expected_types[32]    = { 0 };
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (components != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  if (type == GRAPHENE_TYPE_POINT)
    {
      if (n_components != 2)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed point value needs 2 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
    }
  else if (type == GRAPHENE_TYPE_POINT3D)
    {
      if (n_components != 3)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed 3d point value needs 3 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
      expected_types[2] = G_TYPE_DOUBLE;
    }
  else if (type == GRAPHENE_TYPE_SIZE)
    {
      if (n_components != 2)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed 2d size value needs 2 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
    }
  else if (type == GRAPHENE_TYPE_RECT)
    {
      if (n_components != 4)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed rectangle value needs 4 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
      expected_types[2] = G_TYPE_DOUBLE;
      expected_types[3] = G_TYPE_DOUBLE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' is not elligible for a component value",
                   g_type_name (type));
      return FALSE;
    }

  value            = value_data_new ();
  value->name      = g_strdup (name);
  value->type      = type;
  value->kind      = VALUE_COMPONENT;
  value->component = g_ptr_array_new_with_free_func (value_data_unref);

  for (guint i = 0; i < n_components; i++)
    {
      ValueData *value_data = NULL;

      value_data = g_hash_table_lookup (self->values, components[i]);
      if (value_data == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "value '%s' is undefined", components[i]);
          return FALSE;
        }
      if (!g_type_is_a (value_data->type, expected_types[i]))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "component %u for type %s "
                       "must be of type %s, got %s",
                       i, g_type_name (type),
                       g_type_name (expected_types[i]),
                       g_type_name (value_data->type));
          return FALSE;
        }

      g_ptr_array_add (value->component, value_data_ref (value_data));
    }

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_instance_source_value (BgeWdgtSpec *self,
                                         const char  *name,
                                         GType        type,
                                         GError     **error)
{
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }
  if (!g_type_is_a (type, G_TYPE_OBJECT))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' is not an object type",
                   g_type_name (type));
      return FALSE;
    }
  if (!G_TYPE_IS_INSTANTIATABLE (type))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' is not instantiable",
                   g_type_name (type));
      return FALSE;
    }

  value       = value_data_new ();
  value->kind = VALUE_OBJECT;
  value->type = type;
  value->name = g_strdup (name);

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  g_ptr_array_add (self->nonchildren, value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_special_source_value (BgeWdgtSpec        *self,
                                        const char         *name,
                                        BgeWdgtSpecialValue kind,
                                        GError            **error)
{
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  switch (kind)
    {
    case BGE_WDGT_SPECIAL_VALUE_MOTION_X:
      if (self->special_values.motion_x != NULL)
        value = value_data_ref (self->special_values.motion_x);
      break;
    case BGE_WDGT_SPECIAL_VALUE_MOTION_Y:
      if (self->special_values.motion_y != NULL)
        value = value_data_ref (self->special_values.motion_y);
      break;
    default:
      g_critical ("Invalid special value specified");
      return FALSE;
    }

  if (value == NULL)
    {
      value          = value_data_new ();
      value->name    = g_strdup (name);
      value->kind    = VALUE_SPECIAL;
      value->special = kind;

      switch (kind)
        {
        case BGE_WDGT_SPECIAL_VALUE_MOTION_X:
          self->special_values.motion_x = value_data_ref (value);
          break;
        case BGE_WDGT_SPECIAL_VALUE_MOTION_Y:
          self->special_values.motion_y = value_data_ref (value);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_variable_value (BgeWdgtSpec *self,
                                  GType        type,
                                  const char  *name,
                                  GError     **error)
{
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (!G_TYPE_IS_VALUE (type))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' cannot be stored in value '%s'",
                   g_type_name (type), name);
      return FALSE;
    }
  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  value       = value_data_new ();
  value->name = g_strdup (name);
  value->type = type;
  value->kind = VALUE_VARIABLE;

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_property_value (BgeWdgtSpec *self,
                                  const char  *name,
                                  const char  *object,
                                  const char  *property,
                                  GError     **error)
{
  ValueData *object_value           = NULL;
  ValueData *existing_value         = NULL;
  g_autoptr (GTypeClass) type_class = NULL;
  GParamSpec *pspec                 = NULL;
  g_autoptr (ValueData) value       = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (property != NULL, FALSE);

  object_value = g_hash_table_lookup (self->values, object);
  if (object_value == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' is undefined", object);
      return FALSE;
    }
  if (!g_type_is_a (object_value->type, G_TYPE_OBJECT))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' is not an object", object);
      return FALSE;
    }

  existing_value = g_hash_table_lookup (self->values, name);
  if (existing_value != NULL)
    {
      if (existing_value->kind == VALUE_PROPERTY &&
          existing_value->property.object == object_value &&
          g_strcmp0 (existing_value->property.prop_name, property) == 0)
        return TRUE;
      else
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "value '%s' already exists", name);
          return FALSE;
        }
    }

  type_class = g_type_class_ref (object_value->type);
  pspec      = g_object_class_find_property (G_OBJECT_CLASS (type_class), property);
  if (pspec == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "property '%s' doesn't exist on type %s",
                   property, g_type_name (object_value->type));
      return FALSE;
    }

  value                       = value_data_new ();
  value->name                 = g_strdup (name);
  value->type                 = pspec->value_type;
  value->kind                 = VALUE_PROPERTY;
  value->property.object      = value_data_ref (object_value);
  value->property.prop_name   = g_strdup (property);
  value->property.pspec_flags = pspec->flags;

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_child (BgeWdgtSpec *self,
                         GType        type,
                         const char  *name,
                         GError     **error)
{
  g_autoptr (ValueData) child = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }
  if (!g_type_is_a (type, GTK_TYPE_WIDGET))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' does not derive from GtkWidget",
                   g_type_name (type));
      return FALSE;
    }
  if (!G_TYPE_IS_INSTANTIATABLE (type))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' is not instantiable",
                   g_type_name (type));
      return FALSE;
    }

  child       = value_data_new ();
  child->kind = VALUE_CHILD;
  child->type = type;
  child->name = g_strdup (name);

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (child));
  g_ptr_array_add (self->children, value_data_ref (child));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_state (BgeWdgtSpec *self,
                         const char  *name,
                         gboolean     default_state,
                         GError     **error)
{
  g_autoptr (StateData) state = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->states, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "state '%s' already exists", name);
      return FALSE;
    }
  if (default_state && self->default_state != NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "default state already specified");
      return FALSE;
    }

  state          = state_data_new ();
  state->name    = g_strdup (name);
  state->setters = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      value_data_unref, value_data_unref);

  g_hash_table_replace (self->states, g_strdup (name), state_data_ref (state));
  if (default_state)
    self->default_state = state_data_ref (state);

  return TRUE;
}

gboolean
bge_wdgt_spec_set_value (BgeWdgtSpec *self,
                         const char  *state,
                         const char  *dest_value,
                         const char  *src_value,
                         GError     **error)
{
  StateData *state_data = NULL;
  ValueData *dest_data  = NULL;
  ValueData *src_data   = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (state != NULL, FALSE);
  g_return_val_if_fail (dest_value != NULL, FALSE);
  g_return_val_if_fail (src_value != NULL, FALSE);

  state_data = g_hash_table_lookup (self->states, state);
  dest_data  = g_hash_table_lookup (self->values, dest_value);
  src_data   = g_hash_table_lookup (self->values, src_value);

  if (state_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "state '%s' is undefined", state);
      return FALSE;
    }
  if (dest_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' is undefined", dest_value);
      return FALSE;
    }
  if (src_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' is undefined", src_value);
      return FALSE;
    }
  if (dest_data == src_data)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "cannot assign '%s' to itself", src_value);
      return FALSE;
    }
  if (!g_type_is_a (src_data->type, dest_data->type))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "source type %s cannot be assigned to destination type %s",
                   g_type_name (src_data->type), g_type_name (dest_data->type));
      return FALSE;
    }
  if (dest_data->kind == VALUE_CONSTANT)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "cannot assign to a constant source value");
      return FALSE;
    }
  if (dest_data->kind == VALUE_SPECIAL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "cannot assign to a special source value");
      return FALSE;
    }
  if (dest_data->kind == VALUE_PROPERTY &&
      !(dest_data->property.pspec_flags & G_PARAM_WRITABLE))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "property %s on type %s is not writable",
                   dest_data->property.prop_name,
                   g_type_name (dest_data->type));
      return FALSE;
    }
  if (src_data->kind == VALUE_PROPERTY &&
      !(src_data->property.pspec_flags & G_PARAM_READABLE))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "property %s on type %s is not writable",
                   src_data->property.prop_name,
                   g_type_name (src_data->type));
      return FALSE;
    }

  g_hash_table_replace (
      state_data->setters,
      value_data_ref (dest_data),
      value_data_ref (src_data));
  return TRUE;
}

gboolean
bge_wdgt_spec_append_snapshot_instr (BgeWdgtSpec             *self,
                                     const char              *state,
                                     BgeWdgtSnapshotInstrKind kind,
                                     const char              *instr,
                                     const char *const       *args,
                                     guint                    n_args,
                                     GError                 **error)
{
  gboolean      result              = FALSE;
  StateData    *state_data          = NULL;
  SnapshotInstr match               = { 0 };
  guint         match_rest_start    = 0;
  g_autoptr (SnapshotCallData) call = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (state != NULL, FALSE);
  g_return_val_if_fail (instr != NULL, FALSE);
  g_return_val_if_fail (n_args == 0 || args != NULL, FALSE);

  state_data = g_hash_table_lookup (self->states, state);
  if (state_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "state '%s' is undefined", state);
      return FALSE;
    }

  switch (kind)
    {
    case BGE_WDGT_SNAPSHOT_INSTR_PUSH:
      result = lookup_snapshot_push_instr (instr, &match);
      if (!result)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "\"%s\" is not a valid snapshot push instruction",
                       instr);
          return FALSE;
        }
      break;
    case BGE_WDGT_SNAPSHOT_INSTR_TRANSFORM:
      result = lookup_snapshot_transform_instr (instr, &match);
      if (!result)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "\"%s\" is not a valid snapshot transform instruction",
                       instr);
          return FALSE;
        }
      break;
    case BGE_WDGT_SNAPSHOT_INSTR_APPEND:
      result = lookup_snapshot_append_instr (instr, &match);
      if (!result)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "\"%s\" is not a valid snapshot append instruction",
                       instr);
          return FALSE;
        }
      break;
    case BGE_WDGT_SNAPSHOT_INSTR_POP:
    case BGE_WDGT_SNAPSHOT_INSTR_SAVE:
    case BGE_WDGT_SNAPSHOT_INSTR_RESTORE:
      {
        call       = snapshot_call_data_new ();
        call->kind = kind;

        ensure_state_snapshot (state_data);
        g_ptr_array_add (state_data->snapshot->calls, snapshot_call_data_ref (call));
      }
      return TRUE;
    default:
      g_critical ("invalid snapshot instruction kind specified");
      return FALSE;
    }
  match_rest_start = match.n_args - match.n_rest;

  if (n_args != match.n_args)
    {
      if (match.n_rest > 0 &&
          n_args > match.n_args)
        {
          if ((n_args - match.n_args) % match.n_rest != 0)
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                         "snapshot instruction %s cannot handle %u "
                         "trailing arguments",
                         match.name, n_args - match.n_args);
        }
      else
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "snapshot instruction %s requires %u "
                       "arguments, got %u",
                       match.name, match.n_args, n_args);
          return FALSE;
        }
    }

  call       = snapshot_call_data_new ();
  call->kind = kind;
  call->func = match.call;
  call->args = g_ptr_array_new_with_free_func (value_data_unref);
  call->rest = g_ptr_array_new_with_free_func (value_data_unref);

  for (guint i = 0; i < n_args; i++)
    {
      ValueData *value_data    = NULL;
      gboolean   in_rest       = FALSE;
      GType      expected_type = 0;

      value_data = g_hash_table_lookup (self->values, args[i]);
      if (value_data == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "value '%s' is undefined", args[i]);
          return FALSE;
        }

      in_rest = i >= match_rest_start;
      if (in_rest)
        expected_type = match.args[match_rest_start +
                                   ((i - match_rest_start) %
                                    match.n_rest)];
      else
        expected_type = match.args[i];

      if (!g_type_is_a (value_data->type, expected_type))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "argument %u for snapshot instruction %s "
                       "must be of type %s, got %s",
                       i, match.name,
                       g_type_name (expected_type),
                       g_type_name (value_data->type));
          return FALSE;
        }

      if (in_rest)
        g_ptr_array_add (call->rest, value_data_ref (value_data));
      else
        g_ptr_array_add (call->args, value_data_ref (value_data));
    }

  ensure_state_snapshot (state_data);
  g_ptr_array_add (state_data->snapshot->calls, snapshot_call_data_ref (call));

  return TRUE;
}

void
bge_wdgt_spec_mark_ready (BgeWdgtSpec *self)
{
  g_return_if_fail (BGE_IS_WDGT_SPEC (self));
  self->ready = TRUE;
}

static void
snapshot_push_instr_opacity (GtkSnapshot *snapshot,
                             const GValue args[],
                             const GValue rest[],
                             guint        n_rest)
{
  gtk_snapshot_push_opacity (
      snapshot,
      g_value_get_double (&args[0]));
}

static void
snapshot_push_instr_isolation (GtkSnapshot *snapshot,
                               const GValue args[],
                               const GValue rest[],
                               guint        n_rest)
{
  gtk_snapshot_push_isolation (
      snapshot,
      g_value_get_flags (&args[0]));
}

static void
snapshot_push_instr_blur (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_blur (
      snapshot,
      g_value_get_double (&args[0]));
}

static void
snapshot_push_instr_color_matrix (GtkSnapshot *snapshot,
                                  const GValue args[],
                                  const GValue rest[],
                                  guint        n_rest)
{
  gtk_snapshot_push_color_matrix (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_push_instr_component_transfer (GtkSnapshot *snapshot,
                                        const GValue args[],
                                        const GValue rest[],
                                        guint        n_rest)
{
  gtk_snapshot_push_component_transfer (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]),
      g_value_get_boxed (&args[3]));
}

static void
snapshot_push_instr_repeat (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_push_repeat (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_push_instr_clip (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_clip (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_push_instr_rounded_clip (GtkSnapshot *snapshot,
                                  const GValue args[],
                                  const GValue rest[],
                                  guint        n_rest)
{
  GskRoundedRect rrect = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  gtk_snapshot_push_rounded_clip (
      snapshot,
      &rrect);
}

static void
snapshot_push_instr_fill (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_fill (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_enum (&args[1]));
}

static void
snapshot_push_instr_stroke (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_push_stroke (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_push_instr_shadow (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  guint     n_shadows   = 0;
  GskShadow shadows[32] = { 0 };

  n_shadows = MIN (n_rest / 4, G_N_ELEMENTS (shadows));

  for (guint i = 0; i < n_shadows; i++)
    {
      shadows[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 4 + 0]);
      shadows[i].dx     = g_value_get_double (&rest[i * 4 + 1]);
      shadows[i].dy     = g_value_get_double (&rest[i * 4 + 2]);
      shadows[i].radius = g_value_get_double (&rest[i * 4 + 3]);
    }

  gtk_snapshot_push_shadow (
      snapshot,
      shadows,
      n_shadows);
}

static void
snapshot_push_instr_blend (GtkSnapshot *snapshot,
                           const GValue args[],
                           const GValue rest[],
                           guint        n_rest)
{
  gtk_snapshot_push_blend (
      snapshot,
      g_value_get_enum (&args[0]));
}

static void
snapshot_push_instr_mask (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_mask (
      snapshot,
      g_value_get_enum (&args[0]));
}

static void
snapshot_push_instr_copy (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_copy (
      snapshot);
}

static void
snapshot_push_instr_composite (GtkSnapshot *snapshot,
                               const GValue args[],
                               const GValue rest[],
                               guint        n_rest)
{
  gtk_snapshot_push_composite (
      snapshot,
      g_value_get_enum (&args[0]));
}

static void
snapshot_push_instr_cross_fade (GtkSnapshot *snapshot,
                                const GValue args[],
                                const GValue rest[],
                                guint        n_rest)
{
  gtk_snapshot_push_cross_fade (
      snapshot,
      g_value_get_double (&args[0]));
}

static gboolean
lookup_snapshot_push_instr (const char    *lookup_name,
                            SnapshotInstr *out)
{
  SnapshotInstr instrs[] = {
    {
     "opacity",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_opacity,
     snapshot_push_instr_opacity,
     },
    {
     "isolation",
     1,
     0,
     {
     GSK_TYPE_ISOLATION,
     },
     gtk_snapshot_push_isolation,
     snapshot_push_instr_isolation,
     },
    {
     "blur",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_blur,
     snapshot_push_instr_blur,
     },
    {
     "color-matrix",
     2,
     0,
     {
     GRAPHENE_TYPE_MATRIX,
     GRAPHENE_TYPE_VEC4,
     },
     gtk_snapshot_push_color_matrix,
     snapshot_push_instr_color_matrix,
     },
    {
     "component-transfer",
     4,
     0,
     {
     GSK_TYPE_COMPONENT_TRANSFER,
     GSK_TYPE_COMPONENT_TRANSFER,
     GSK_TYPE_COMPONENT_TRANSFER,
     GSK_TYPE_COMPONENT_TRANSFER,
     },
     gtk_snapshot_push_component_transfer,
     snapshot_push_instr_component_transfer,
     },
    {
     "repeat",
     2,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_push_repeat,
     snapshot_push_instr_repeat,
     },
    {
     "clip",
     1,
     0,
     {
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_push_clip,
     snapshot_push_instr_clip,
     },
    {
     "rounded-clip",
     5,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     },
     gtk_snapshot_push_rounded_clip,
     snapshot_push_instr_rounded_clip,
     },
    {
     "fill",
     2,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_FILL_RULE,
     },
     gtk_snapshot_push_fill,
     snapshot_push_instr_fill,
     },
    {
     "stroke",
     2,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_STROKE,
     },
     gtk_snapshot_push_stroke,
     snapshot_push_instr_stroke,
     },
    {
     "shadow",
     4,
     4,
     {
     GDK_TYPE_RGBA,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_shadow,
     snapshot_push_instr_shadow,
     },
    {
     "blend",
     1,
     0,
     {
     GSK_TYPE_BLEND_MODE,
     },
     gtk_snapshot_push_blend,
     snapshot_push_instr_blend,
     },
    {
     "mask",
     1,
     0,
     {
     GSK_TYPE_MASK_MODE,
     },
     gtk_snapshot_push_mask,
     snapshot_push_instr_mask,
     },
    {
     "copy",
     0,
     0,
     {},
     gtk_snapshot_push_copy,
     snapshot_push_instr_copy,
     },
    {
     "composite",
     1,
     0,
     {
     GSK_TYPE_PORTER_DUFF,
     },
     gtk_snapshot_push_composite,
     snapshot_push_instr_composite,
     },
    {
     "cross-fade",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_cross_fade,
     snapshot_push_instr_cross_fade,
     },
  };

  for (guint i = 0; i < G_N_ELEMENTS (instrs); i++)
    {
      if (g_strcmp0 (lookup_name, instrs[i].name) == 0)
        {
          *out = instrs[i];
          return TRUE;
        }
    }
  return FALSE;
}

static void
snapshot_transform_instr_transform (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  gtk_snapshot_transform (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_transform_matrix (GtkSnapshot *snapshot,
                                           const GValue args[],
                                           const GValue rest[],
                                           guint        n_rest)
{
  gtk_snapshot_transform_matrix (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_translate (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  gtk_snapshot_translate (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_translate_3d (GtkSnapshot *snapshot,
                                       const GValue args[],
                                       const GValue rest[],
                                       guint        n_rest)
{
  gtk_snapshot_translate_3d (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_rotate (GtkSnapshot *snapshot,
                                 const GValue args[],
                                 const GValue rest[],
                                 guint        n_rest)
{
  gtk_snapshot_rotate (
      snapshot,
      g_value_get_double (&args[0]));
}

static void
snapshot_transform_instr_rotate_3d (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  gtk_snapshot_rotate_3d (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_transform_instr_scale (GtkSnapshot *snapshot,
                                const GValue args[],
                                const GValue rest[],
                                guint        n_rest)
{
  gtk_snapshot_scale (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_double (&args[1]));
}

static void
snapshot_transform_instr_scale_3d (GtkSnapshot *snapshot,
                                   const GValue args[],
                                   const GValue rest[],
                                   guint        n_rest)
{
  gtk_snapshot_scale_3d (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_double (&args[1]),
      g_value_get_double (&args[2]));
}

static void
snapshot_transform_instr_perspective (GtkSnapshot *snapshot,
                                      const GValue args[],
                                      const GValue rest[],
                                      guint        n_rest)
{
  gtk_snapshot_scale_3d (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_double (&args[1]),
      g_value_get_double (&args[2]));
}

static gboolean
lookup_snapshot_transform_instr (const char    *lookup_name,
                                 SnapshotInstr *out)
{
  SnapshotInstr instrs[] = {
    {
     "transform",
     1,
     0,
     {
     GSK_TYPE_TRANSFORM,
     },
     gtk_snapshot_transform,
     snapshot_transform_instr_transform,
     },
    {
     "transform-matrix",
     1,
     0,
     {
     GRAPHENE_TYPE_MATRIX,
     },
     gtk_snapshot_transform_matrix,
     snapshot_transform_instr_transform_matrix,
     },
    {
     "translate",
     1,
     0,
     {
     GRAPHENE_TYPE_POINT,
     },
     gtk_snapshot_translate,
     snapshot_transform_instr_translate,
     },
    {
     "translate-3d",
     1,
     0,
     {
     GRAPHENE_TYPE_POINT3D,
     },
     gtk_snapshot_translate_3d,
     snapshot_transform_instr_translate_3d,
     },
    {
     "rotate",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_rotate,
     snapshot_transform_instr_rotate,
     },
    {
     "rotate-3d",
     2,
     0,
     {
     G_TYPE_DOUBLE,
     GRAPHENE_TYPE_VEC3,
     },
     gtk_snapshot_rotate_3d,
     snapshot_transform_instr_rotate_3d,
     },
    {
     "scale",
     2,
     0,
     {
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_scale,
     snapshot_transform_instr_scale,
     },
    {
     "scale-3d",
     3,
     0,
     {
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_scale_3d,
     snapshot_transform_instr_scale_3d,
     },
    {
     "perspective",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_perspective,
     snapshot_transform_instr_perspective,
     },
  };

  for (guint i = 0; i < G_N_ELEMENTS (instrs); i++)
    {
      if (g_strcmp0 (lookup_name, instrs[i].name) == 0)
        {
          *out = instrs[i];
          return TRUE;
        }
    }
  return FALSE;
}

static void
snapshot_append_instr_node (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_append_node (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_append_instr_texture (GtkSnapshot *snapshot,
                               const GValue args[],
                               const GValue rest[],
                               guint        n_rest)
{
  gtk_snapshot_append_texture (
      snapshot,
      g_value_get_object (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_append_instr_scaled_texture (GtkSnapshot *snapshot,
                                      const GValue args[],
                                      const GValue rest[],
                                      guint        n_rest)
{
  gtk_snapshot_append_scaled_texture (
      snapshot,
      g_value_get_object (&args[0]),
      g_value_get_enum (&args[1]),
      g_value_get_boxed (&args[2]));
}

static void
snapshot_append_instr_color (GtkSnapshot *snapshot,
                             const GValue args[],
                             const GValue rest[],
                             guint        n_rest)
{
  gtk_snapshot_append_color (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_append_instr_linear_gradient (GtkSnapshot *snapshot,
                                       const GValue args[],
                                       const GValue rest[],
                                       guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_linear_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_repeating_linear_gradient (GtkSnapshot *snapshot,
                                                 const GValue args[],
                                                 const GValue rest[],
                                                 guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_repeating_linear_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_radial_gradient (GtkSnapshot *snapshot,
                                       const GValue args[],
                                       const GValue rest[],
                                       guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_radial_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_double (&args[2]),
      g_value_get_double (&args[3]),
      g_value_get_double (&args[4]),
      g_value_get_double (&args[5]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_repeating_radial_gradient (GtkSnapshot *snapshot,
                                                 const GValue args[],
                                                 const GValue rest[],
                                                 guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_repeating_radial_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_double (&args[2]),
      g_value_get_double (&args[3]),
      g_value_get_double (&args[4]),
      g_value_get_double (&args[5]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_conic_gradient (GtkSnapshot *snapshot,
                                      const GValue args[],
                                      const GValue rest[],
                                      guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_conic_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_double (&args[2]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_border (GtkSnapshot *snapshot,
                              const GValue args[],
                              const GValue rest[],
                              guint        n_rest)
{
  GskRoundedRect rrect           = { 0 };
  float          border_width[4] = { 0 };
  GdkRGBA        border_color[4] = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  border_width[0] = g_value_get_double (&args[5]);
  border_width[1] = g_value_get_double (&args[6]);
  border_width[2] = g_value_get_double (&args[7]);
  border_width[3] = g_value_get_double (&args[8]);

  border_color[0] = *(GdkRGBA *) g_value_get_boxed (&args[9]);
  border_color[1] = *(GdkRGBA *) g_value_get_boxed (&args[10]);
  border_color[2] = *(GdkRGBA *) g_value_get_boxed (&args[11]);
  border_color[3] = *(GdkRGBA *) g_value_get_boxed (&args[12]);

  gtk_snapshot_append_border (
      snapshot,
      &rrect,
      border_width,
      border_color);
}

static void
snapshot_append_instr_inset_shadow (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  GskRoundedRect rrect = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  gtk_snapshot_append_inset_shadow (
      snapshot,
      &rrect,
      g_value_get_boxed (&args[5]),
      g_value_get_double (&args[6]),
      g_value_get_double (&args[7]),
      g_value_get_double (&args[8]),
      g_value_get_double (&args[9]));
}

static void
snapshot_append_instr_outset_shadow (GtkSnapshot *snapshot,
                                     const GValue args[],
                                     const GValue rest[],
                                     guint        n_rest)
{
  GskRoundedRect rrect = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  gtk_snapshot_append_outset_shadow (
      snapshot,
      &rrect,
      g_value_get_boxed (&args[5]),
      g_value_get_double (&args[6]),
      g_value_get_double (&args[7]),
      g_value_get_double (&args[8]),
      g_value_get_double (&args[9]));
}

static void
snapshot_append_instr_layout (GtkSnapshot *snapshot,
                              const GValue args[],
                              const GValue rest[],
                              guint        n_rest)
{
  gtk_snapshot_append_layout (
      snapshot,
      g_value_get_object (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_append_instr_fill (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_append_fill (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_enum (&args[1]),
      g_value_get_boxed (&args[2]));
}

static void
snapshot_append_instr_stroke (GtkSnapshot *snapshot,
                              const GValue args[],
                              const GValue rest[],
                              guint        n_rest)
{
  gtk_snapshot_append_stroke (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]));
}

static void
snapshot_append_instr_paste (GtkSnapshot *snapshot,
                             const GValue args[],
                             const GValue rest[],
                             guint        n_rest)
{
  gtk_snapshot_append_paste (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_uint64 (&args[1]));
}

static gboolean
lookup_snapshot_append_instr (const char    *lookup_name,
                              SnapshotInstr *out)
{
  SnapshotInstr instrs[] = {
    {
     "node",
     1,
     0,
     {
     GSK_TYPE_RENDER_NODE,
     },
     gtk_snapshot_append_node,
     snapshot_append_instr_node,
     },
    {
     "texture",
     2,
     0,
     {
     GDK_TYPE_TEXTURE,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_append_texture,
     snapshot_append_instr_texture,
     },
    {
     "scaled-texture",
     3,
     0,
     {
     GDK_TYPE_TEXTURE,
     GSK_TYPE_SCALING_FILTER,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_append_scaled_texture,
     snapshot_append_instr_scaled_texture,
     },
    {
     "color",
     2,
     0,
     {
     GDK_TYPE_RGBA,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_append_color,
     snapshot_append_instr_color,
     },
    {
     "linear-gradient",
     5,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_linear_gradient,
     snapshot_append_instr_linear_gradient,
     },
    {
     "repeating-linear-gradient",
     5,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_repeating_linear_gradient,
     snapshot_append_instr_repeating_linear_gradient,
     },
    {
     "radial-gradient",
     8,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_radial_gradient,
     snapshot_append_instr_radial_gradient,
     },
    {
     "repeating-radial-gradient",
     8,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_repeating_radial_gradient,
     snapshot_append_instr_repeating_radial_gradient,
     },
    {
     "conic-gradient",
     5,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_conic_gradient,
     snapshot_append_instr_conic_gradient,
     },
    {
     "border",
     13,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     GDK_TYPE_RGBA,
     GDK_TYPE_RGBA,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_border,
     snapshot_append_instr_border,
     },
    {
     "inset-shadow",
     10,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GDK_TYPE_RGBA,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_append_inset_shadow,
     snapshot_append_instr_inset_shadow,
     },
    {
     "outset-shadow",
     10,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GDK_TYPE_RGBA,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_append_outset_shadow,
     snapshot_append_instr_outset_shadow,
     },
    {
     "layout",
     2,
     0,
     {
     PANGO_TYPE_LAYOUT,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_layout,
     snapshot_append_instr_layout,
     },
    {
     "fill",
     3,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_FILL_RULE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_fill,
     snapshot_append_instr_fill,
     },
    {
     "stroke",
     3,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_STROKE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_stroke,
     snapshot_append_instr_stroke,
     },
    {
     "paste",
     2,
     0,
     {
     GRAPHENE_TYPE_RECT,
     G_TYPE_UINT64,
     },
     gtk_snapshot_append_paste,
     snapshot_append_instr_paste,
     },
  };

  for (guint i = 0; i < G_N_ELEMENTS (instrs); i++)
    {
      if (g_strcmp0 (lookup_name, instrs[i].name) == 0)
        {
          *out = instrs[i];
          return TRUE;
        }
    }
  return FALSE;
}

static ValueData *
dig_variable_value (ValueData *value,
                    StateData *state)
{
  g_assert (value->kind == VALUE_VARIABLE);
  for (;;)
    {
      value = g_hash_table_lookup (state->setters, value);
      if (value != NULL)
        {
          if (value->kind != VALUE_VARIABLE)
            return value;
        }
      else
        return NULL;
    }
}

static void
ensure_state_snapshot (StateData *state)
{
  if (state->snapshot != NULL)
    return;

  state->snapshot        = snapshot_data_new ();
  state->snapshot->calls = g_ptr_array_new_with_free_func (
      snapshot_call_data_unref);
}

/* ------------------------------ */
/* Widget Renderer Implementation */
/* ------------------------------ */

struct _BgeWdgtRenderer
{
  GtkWidget parent_instance;

  BgeWdgtSpec *spec;
  char        *state;
  GObject     *reference;
  GtkWidget   *child;

  StateData    *active_state;
  SnapshotData *active_snapshot;

  GHashTable *objects;
  GPtrArray  *children;
  GPtrArray  *nonchildren;
  GHashTable *state_instances;

  GPtrArray *bindings;
  GPtrArray *watches;
};

G_DEFINE_FINAL_TYPE (BgeWdgtRenderer, bge_wdgt_renderer, GTK_TYPE_WIDGET);

enum
{
  RENDERER_PROP_0,

  RENDERER_PROP_SPEC,
  RENDERER_PROP_STATE,
  RENDERER_PROP_REFERENCE,
  RENDERER_PROP_CHILD,

  LAST_RENDERER_PROP
};
static GParamSpec *renderer_props[LAST_RENDERER_PROP] = { 0 };

BGE_DEFINE_DATA (
    state_instance,
    StateInstance,
    {
      GHashTable *expressions;
      GPtrArray  *snapshot_deps;
    },
    BGE_RELEASE_DATA (expressions, g_hash_table_unref);
    BGE_RELEASE_DATA (snapshot_deps, g_ptr_array_unref))

static void
regenerate (BgeWdgtRenderer *self);

static void
apply_state (BgeWdgtRenderer *self);

static GtkExpression *
ensure_expressions (BgeWdgtRenderer   *self,
                    ValueData         *value,
                    StateData         *state,
                    StateInstanceData *instance);

static void
discard_binding (gpointer ptr);

static void
discard_watch (gpointer ptr);

static void
prop_change_queue_draw (BgeWdgtRenderer *self);

static void
bge_wdgt_renderer_dispose (GObject *object)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (object);

  g_clear_pointer (&self->spec, g_object_unref);
  g_clear_pointer (&self->state, g_free);
  g_clear_pointer (&self->reference, g_object_unref);
  g_clear_pointer (&self->child, gtk_widget_unparent);

  g_clear_pointer (&self->active_state, state_data_unref);
  g_clear_pointer (&self->active_snapshot, snapshot_data_unref);

  g_clear_pointer (&self->objects, g_hash_table_unref);
  g_clear_pointer (&self->children, g_ptr_array_unref);
  g_clear_pointer (&self->nonchildren, g_ptr_array_unref);
  g_clear_pointer (&self->state_instances, g_hash_table_unref);

  g_clear_pointer (&self->bindings, g_ptr_array_unref);
  g_clear_pointer (&self->watches, g_ptr_array_unref);

  G_OBJECT_CLASS (bge_wdgt_renderer_parent_class)->dispose (object);
}

static void
bge_wdgt_renderer_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (object);

  switch (prop_id)
    {
    case RENDERER_PROP_SPEC:
      g_value_set_object (value, bge_wdgt_renderer_get_spec (self));
      break;
    case RENDERER_PROP_STATE:
      g_value_set_string (value, bge_wdgt_renderer_get_state (self));
      break;
    case RENDERER_PROP_REFERENCE:
      g_value_set_object (value, bge_wdgt_renderer_get_reference (self));
      break;
    case RENDERER_PROP_CHILD:
      g_value_set_object (value, bge_wdgt_renderer_get_child (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_renderer_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (object);

  switch (prop_id)
    {
    case RENDERER_PROP_SPEC:
      bge_wdgt_renderer_set_spec (self, g_value_get_object (value));
      break;
    case RENDERER_PROP_STATE:
      bge_wdgt_renderer_set_state (self, g_value_get_string (value));
      break;
    case RENDERER_PROP_REFERENCE:
      bge_wdgt_renderer_set_reference (self, g_value_get_object (value));
      break;
    case RENDERER_PROP_CHILD:
      bge_wdgt_renderer_set_child (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_renderer_measure (GtkWidget     *widget,
                           GtkOrientation orientation,
                           int            for_size,
                           int           *minimum,
                           int           *natural,
                           int           *minimum_baseline,
                           int           *natural_baseline)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (widget);

  if (self->child != NULL)
    gtk_widget_measure (
        GTK_WIDGET (self->child),
        orientation, for_size,
        minimum, natural,
        minimum_baseline, natural_baseline);
}

static void
bge_wdgt_renderer_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (widget);

  if (self->child != NULL &&
      gtk_widget_should_layout (self->child))
    gtk_widget_allocate (self->child, width, height, baseline, NULL);

  for (guint i = 0; i < self->children->len; i++)
    {
      GtkWidget *child = NULL;

      child = g_ptr_array_index (self->children, i);
      gtk_widget_allocate (child, width, height, baseline, NULL);
    }
}

static void
bge_wdgt_renderer_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  BgeWdgtRenderer   *self     = BGE_WDGT_RENDERER (widget);
  StateData         *state    = NULL;
  StateInstanceData *instance = NULL;
  GPtrArray         *calls    = NULL;

  if (self->child != NULL)
    gtk_widget_snapshot_child (GTK_WIDGET (self), self->child, snapshot);

  if (self->active_state == NULL)
    return;

  state = self->active_state;
  if (self->active_snapshot != NULL)
    calls = self->active_snapshot->calls;

  instance = g_hash_table_lookup (self->state_instances, state);
  g_assert (instance != NULL);

  if (calls != NULL)
    {
      for (guint i = 0; i < calls->len; i++)
        {
          SnapshotCallData *call = NULL;

          call = g_ptr_array_index (calls, i);
          switch (call->kind)
            {
            case BGE_WDGT_SNAPSHOT_INSTR_POP:
              gtk_snapshot_pop (snapshot);
              break;
            case BGE_WDGT_SNAPSHOT_INSTR_SAVE:
              gtk_snapshot_save (snapshot);
              break;
            case BGE_WDGT_SNAPSHOT_INSTR_RESTORE:
              gtk_snapshot_restore (snapshot);
              break;
            case BGE_WDGT_SNAPSHOT_INSTR_APPEND:
            case BGE_WDGT_SNAPSHOT_INSTR_PUSH:
            case BGE_WDGT_SNAPSHOT_INSTR_TRANSFORM:
              {
                GValue arg_values[32]  = { 0 };
                guint  n_arg_values    = 0;
                GValue rest_values[32] = { 0 };
                guint  n_rest_values   = 0;

                n_arg_values  = MIN (call->args->len, G_N_ELEMENTS (arg_values));
                n_rest_values = MIN (call->rest->len, G_N_ELEMENTS (rest_values));

                for (guint j = 0; j < n_arg_values; j++)
                  {
                    ValueData     *value      = NULL;
                    GtkExpression *expression = NULL;

                    value      = g_ptr_array_index (call->args, j);
                    expression = g_hash_table_lookup (instance->expressions, value);
                    gtk_expression_evaluate (expression, self, &arg_values[j]);
                  }
                for (guint j = 0; j < n_rest_values; j++)
                  {
                    ValueData     *value      = NULL;
                    GtkExpression *expression = NULL;

                    value      = g_ptr_array_index (call->rest, j);
                    expression = g_hash_table_lookup (instance->expressions, value);
                    gtk_expression_evaluate (expression, self, &rest_values[j]);
                  }

                call->func (snapshot, arg_values, rest_values, n_rest_values);

                for (guint j = 0; j < n_rest_values; j++)
                  {
                    g_value_unset (&rest_values[j]);
                  }
                for (guint j = 0; j < n_arg_values; j++)
                  {
                    g_value_unset (&arg_values[j]);
                  }
              }
              break;
            default:
              break;
            }
        }
    }

  for (guint i = 0; i < self->children->len; i++)
    {
      GtkWidget *child = NULL;

      child = g_ptr_array_index (self->children, i);
      gtk_widget_snapshot_child (GTK_WIDGET (self), child, snapshot);
    }
}

static void
bge_wdgt_renderer_class_init (BgeWdgtRendererClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bge_wdgt_renderer_set_property;
  object_class->get_property = bge_wdgt_renderer_get_property;
  object_class->dispose      = bge_wdgt_renderer_dispose;

  renderer_props[RENDERER_PROP_SPEC] =
      g_param_spec_object (
          "spec",
          NULL, NULL,
          BGE_TYPE_WDGT_SPEC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  renderer_props[RENDERER_PROP_STATE] =
      g_param_spec_string (
          "state",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  renderer_props[RENDERER_PROP_REFERENCE] =
      g_param_spec_object (
          "reference",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  renderer_props[RENDERER_PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_RENDERER_PROP, renderer_props);

  widget_class->measure       = bge_wdgt_renderer_measure;
  widget_class->size_allocate = bge_wdgt_renderer_size_allocate;
  widget_class->snapshot      = bge_wdgt_renderer_snapshot;
}

static void
bge_wdgt_renderer_init (BgeWdgtRenderer *self)
{
  self->objects = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      value_data_unref, g_object_unref);
  self->children = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gtk_widget_unparent);
  self->nonchildren = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_object_unref);
  self->state_instances = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      state_data_unref, state_instance_data_unref);

  self->bindings = g_ptr_array_new_with_free_func (discard_binding);
  self->watches  = g_ptr_array_new_with_free_func (discard_watch);
}

BgeWdgtRenderer *
bge_wdgt_renderer_new (void)
{
  return g_object_new (BGE_TYPE_WDGT_RENDERER, NULL);
}

BgeWdgtSpec *
bge_wdgt_renderer_get_spec (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->spec;
}

const char *
bge_wdgt_renderer_get_state (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->state;
}

GObject *
bge_wdgt_renderer_get_reference (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->reference;
}

GtkWidget *
bge_wdgt_renderer_get_child (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->child;
}

void
bge_wdgt_renderer_set_spec (BgeWdgtRenderer *self,
                            BgeWdgtSpec     *spec)
{
  GBinding *name_binding = NULL;

  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));
  g_return_if_fail (spec == NULL || BGE_IS_WDGT_SPEC (spec));
  if (spec != NULL)
    g_return_if_fail (spec->ready);

  if (spec == self->spec)
    return;

  g_clear_pointer (&self->spec, g_object_unref);
  if (spec != NULL)
    self->spec = g_object_ref (spec);

  regenerate (self);
  apply_state (self);

  if (spec != NULL)
    {
      name_binding = g_object_bind_property (spec, "name", self, "name", G_BINDING_SYNC_CREATE);
      g_ptr_array_add (self->bindings, g_object_ref (name_binding));
    }
  else
    gtk_widget_set_name (GTK_WIDGET (self), NULL);

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_SPEC]);
}

void
bge_wdgt_renderer_set_state (BgeWdgtRenderer *self,
                             const char      *state)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (state == self->state || (state != NULL && self->state != NULL && g_strcmp0 (state, self->state) == 0))
    return;

  g_clear_pointer (&self->state, g_free);
  if (state != NULL)
    self->state = g_strdup (state);

  apply_state (self);

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_STATE]);
}

void
bge_wdgt_renderer_set_reference (BgeWdgtRenderer *self,
                                 GObject         *reference)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (reference == self->reference)
    return;

  g_clear_pointer (&self->reference, g_object_unref);
  if (reference != NULL)
    self->reference = g_object_ref (reference);

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_REFERENCE]);
}

void
bge_wdgt_renderer_set_child (BgeWdgtRenderer *self,
                             GtkWidget       *child)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (self->child == child)
    return;

  if (child != NULL)
    g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  self->child = child;

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_CHILD]);
}

void
bge_wdgt_renderer_set_state_take (BgeWdgtRenderer *self,
                                  char            *state)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (state != NULL && self->state != NULL && g_strcmp0 (state, self->state) == 0)
    {
      g_free (state);
      return;
    }

  g_clear_pointer (&self->state, g_free);
  if (state != NULL)
    self->state = state;

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_STATE]);
}

static void
regenerate (BgeWdgtRenderer *self)
{
  BgeWdgtSpec   *spec       = self->spec;
  GHashTableIter state_iter = { 0 };

  g_hash_table_remove_all (self->objects);
  g_ptr_array_set_size (self->children, 0);
  g_hash_table_remove_all (self->state_instances);

  if (self->spec == NULL)
    return;

  for (guint i = 0; i < spec->children->len; i++)
    {
      ValueData *value  = NULL;
      GtkWidget *widget = NULL;

      value = g_ptr_array_index (spec->children, i);
      g_assert (value->kind == VALUE_CHILD);

      widget = g_object_new (
          value->type,
          "name", value->name,
          NULL);
      gtk_widget_set_parent (widget, GTK_WIDGET (self));

      g_hash_table_replace (self->objects,
                            value_data_ref (value),
                            g_object_ref (widget));
      g_ptr_array_add (self->children, g_object_ref (widget));
    }

  for (guint i = 0; i < spec->nonchildren->len; i++)
    {
      ValueData *value           = NULL;
      g_autoptr (GObject) object = NULL;

      value = g_ptr_array_index (spec->nonchildren, i);
      g_assert (value->kind == VALUE_OBJECT);

      object = g_object_new (value->type, NULL);
      g_hash_table_replace (self->objects,
                            value_data_ref (value),
                            g_object_ref_sink (object));
      g_ptr_array_add (self->nonchildren, g_object_ref (object));
    }

  g_hash_table_iter_init (&state_iter, spec->states);
  for (;;)
    {
      GHashTableIter value_iter              = { 0 };
      char          *state_name              = NULL;
      StateData     *state                   = NULL;
      g_autoptr (StateInstanceData) instance = NULL;

      if (!g_hash_table_iter_next (
              &state_iter,
              (gpointer *) &state_name,
              (gpointer *) &state))
        break;

      instance              = state_instance_data_new ();
      instance->expressions = g_hash_table_new_full (
          g_direct_hash, g_direct_equal,
          value_data_unref, (GDestroyNotify) gtk_expression_unref);
      instance->snapshot_deps = g_ptr_array_new_with_free_func (
          (GDestroyNotify) gtk_expression_unref);

      g_hash_table_iter_init (&value_iter, spec->values);
      for (;;)
        {
          char      *value_name                = NULL;
          ValueData *value                     = NULL;
          g_autoptr (GtkExpression) expression = NULL;

          if (!g_hash_table_iter_next (
                  &value_iter,
                  (gpointer *) &value_name,
                  (gpointer *) &value))
            break;

          expression = ensure_expressions (self, value, state, instance);
        }

      g_hash_table_replace (self->state_instances,
                            state_data_ref (state),
                            state_instance_data_ref (instance));
    }

  g_hash_table_iter_init (&state_iter, spec->states);
  for (;;)
    {
      char              *state_name        = NULL;
      StateData         *state             = NULL;
      StateInstanceData *instance          = NULL;
      StateData         *snapshot_state    = NULL;
      StateInstanceData *snapshot_instance = NULL;

      if (!g_hash_table_iter_next (
              &state_iter,
              (gpointer *) &state_name,
              (gpointer *) &state))
        break;

      instance = g_hash_table_lookup (self->state_instances, state);
      g_assert (instance != NULL);

      if (state->snapshot != NULL)
        {
          snapshot_state    = state;
          snapshot_instance = instance;
        }
      else
        {
          if (self->spec->default_state != NULL &&
              self->spec->default_state->snapshot != NULL)
            /* If this state doesn't have snapshot instructions specified, fallback
               on using the default state */
            {
              snapshot_state    = self->spec->default_state;
              snapshot_instance = g_hash_table_lookup (self->state_instances, snapshot_state);
              g_assert (snapshot_instance != NULL);
            }
          else
            continue;
        }

      for (guint i = 0; i < snapshot_state->snapshot->calls->len; i++)
        {
          SnapshotCallData *call = NULL;

          call = g_ptr_array_index (snapshot_state->snapshot->calls, i);
          if (call->args == NULL)
            /* pop, save, restore... */
            continue;

          for (guint j = 0; j < call->args->len; j++)
            {
              ValueData     *arg        = NULL;
              gboolean       found      = FALSE;
              GtkExpression *expression = NULL;

              arg        = g_ptr_array_index (call->args, j);
              expression = g_hash_table_lookup (instance->expressions, arg);
              g_assert (expression != NULL);

              for (guint k = 0; k < instance->snapshot_deps->len; k++)
                {
                  GtkExpression *other = NULL;

                  other = g_ptr_array_index (instance->snapshot_deps, k);
                  if (expression == other)
                    {
                      found = TRUE;
                      break;
                    }
                }
              if (found)
                /* Ensure we don't duplicate deps */
                continue;

              g_ptr_array_add (instance->snapshot_deps,
                               gtk_expression_ref (expression));
            }
        }
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
apply_state (BgeWdgtRenderer *self)
{
  BgeWdgtSpec       *spec     = self->spec;
  StateData         *state    = NULL;
  StateInstanceData *instance = NULL;
  GHashTableIter     iter     = { 0 };

  g_clear_pointer (&self->active_state, state_data_unref);
  g_clear_pointer (&self->active_snapshot, snapshot_data_unref);

  g_ptr_array_set_size (self->bindings, 0);
  g_ptr_array_set_size (self->watches, 0);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  if (self->spec == NULL ||
      self->state == NULL)
    return;

  state = g_hash_table_lookup (spec->states, self->state);
  if (state == NULL)
    {
      g_critical ("state \"%s\" doesn't exist on spec \"%s\"",
                  self->state, spec->name);
      return;
    }
  instance = g_hash_table_lookup (self->state_instances, state);
  g_assert (instance != NULL);

  self->active_state = state_data_ref (state);

  g_hash_table_iter_init (&iter, state->setters);
  for (;;)
    {
      ValueData  *dest          = NULL;
      ValueData  *src           = NULL;
      GObject    *dest_object   = NULL;
      const char *dest_property = NULL;

      if (!g_hash_table_iter_next (
              &iter,
              (gpointer *) &dest,
              (gpointer *) &src))
        break;

      switch (dest->kind)
        {
        case VALUE_PROPERTY:
          dest_object = g_hash_table_lookup (
              self->objects,
              dest->property.object);
          g_assert (dest_object != NULL);
          dest_property = dest->property.prop_name;
          break;
        case VALUE_VARIABLE:
          break;
        case VALUE_COMPONENT:
        case VALUE_CONSTANT:
        case VALUE_SPECIAL:
        case VALUE_CHILD:
        case VALUE_OBJECT:
        default:
          g_assert_not_reached ();
        }
      if (dest_object == NULL)
        continue;

      if (src->kind == VALUE_VARIABLE)
        src = dig_variable_value (src, state);

      if (src != NULL)
        {
          GtkExpression *expression = NULL;
          GValue         resolved   = G_VALUE_INIT;

          expression = g_hash_table_lookup (
              instance->expressions, src);
          gtk_expression_evaluate (expression, self, &resolved);
          g_object_set_property (
              dest_object,
              dest_property,
              &resolved);
        }
      else
        {
          GValue empty = G_VALUE_INIT;

          g_value_init (&empty, dest->type);
          g_object_set_property (
              dest_object,
              dest_property,
              &empty);
          g_value_unset (&empty);
        }
    }

  if (state->snapshot != NULL)
    self->active_snapshot = snapshot_data_ref (state->snapshot);
  else if (self->spec->default_state != NULL &&
           self->spec->default_state->snapshot != NULL)
    self->active_snapshot = snapshot_data_ref (self->spec->default_state->snapshot);

  if (self->active_snapshot != NULL)
    {
      for (guint i = 0; i < instance->snapshot_deps->len; i++)
        {
          GtkExpression      *expression = NULL;
          GtkExpressionWatch *watch      = NULL;

          expression = g_ptr_array_index (instance->snapshot_deps, i);
          watch      = gtk_expression_watch (
              expression,
              self,
              (GtkExpressionNotify) prop_change_queue_draw,
              self, NULL);
          g_ptr_array_add (self->watches, watch);
        }
    }
}

static graphene_point_t *
expression_create_point (gpointer object,
                         double   x,
                         double   y,
                         gpointer user_data)
{
  graphene_point_t *point = NULL;

  point    = graphene_point_alloc ();
  point->x = x;
  point->y = y;

  return point;
}

static graphene_size_t *
expression_create_size (gpointer object,
                        double   width,
                        double   height,
                        gpointer user_data)
{
  graphene_size_t *size = NULL;

  size         = graphene_size_alloc ();
  size->width  = width;
  size->height = height;

  return size;
}

static graphene_point3d_t *
expression_create_point3d (gpointer object,
                           double   x,
                           double   y,
                           double   z,
                           gpointer user_data)
{
  graphene_point3d_t *point3d = NULL;

  point3d    = graphene_point3d_alloc ();
  point3d->x = x;
  point3d->y = y;
  point3d->z = z;

  return point3d;
}

static graphene_rect_t *
expression_create_rect (gpointer object,
                        double   x,
                        double   y,
                        double   width,
                        double   height,
                        gpointer user_data)
{
  graphene_rect_t *rect = NULL;

  rect              = graphene_rect_alloc ();
  rect->origin.x    = x;
  rect->origin.y    = y;
  rect->size.width  = width;
  rect->size.height = height;

  return rect;
}

static GtkExpression *
ensure_expressions (BgeWdgtRenderer   *self,
                    ValueData         *value,
                    StateData         *state,
                    StateInstanceData *instance)
{
  GtkExpression *cached                = NULL;
  g_autoptr (GtkExpression) expression = NULL;

  cached = g_hash_table_lookup (instance->expressions, value);
  if (cached != NULL)
    return gtk_expression_ref (cached);

  switch (value->kind)
    {
    case VALUE_CONSTANT:
      expression = gtk_constant_expression_new_for_value (&value->constant);
      break;
    case VALUE_OBJECT:
    case VALUE_CHILD:
      {
        GtkWidget *object = NULL;

        object = g_hash_table_lookup (self->objects, value);
        g_assert (object != NULL);
        expression = gtk_constant_expression_new (value->type, object);
      }
      break;
    case VALUE_VARIABLE:
      {
        ValueData *dig = NULL;

        dig = dig_variable_value (value, state);
        if (dig != NULL)
          expression = ensure_expressions (self, dig, state, instance);
        else
          {
            GValue empty_value = G_VALUE_INIT;

            g_value_init (&empty_value, value->type);
            expression = gtk_constant_expression_new_for_value (&empty_value);
            g_value_unset (&empty_value);
          }
      }
      break;
    case VALUE_COMPONENT:
      {
        g_autoptr (GPtrArray) params = NULL;
        GClosureMarshal marshal      = NULL;
        GCallback       callback     = NULL;

        params = g_ptr_array_new ();

        for (guint i = 0; i < value->component->len; i++)
          {
            ValueData *member                     = NULL;
            g_autoptr (GtkExpression) member_expr = NULL;

            member      = g_ptr_array_index (value->component, i);
            member_expr = ensure_expressions (self, member, state, instance);
            g_ptr_array_add (params, g_steal_pointer (&member_expr));
          }

        if (value->type == GRAPHENE_TYPE_POINT)
          {
            marshal  = bge_marshal_BOXED__DOUBLE_DOUBLE;
            callback = G_CALLBACK (expression_create_point);
          }
        else if (value->type == GRAPHENE_TYPE_SIZE)
          {
            marshal  = bge_marshal_BOXED__DOUBLE_DOUBLE;
            callback = G_CALLBACK (expression_create_size);
          }
        else if (value->type == GRAPHENE_TYPE_POINT3D)
          {
            marshal  = bge_marshal_BOXED__DOUBLE_DOUBLE_DOUBLE;
            callback = G_CALLBACK (expression_create_point3d);
          }
        else if (value->type == GRAPHENE_TYPE_RECT)
          {
            marshal  = bge_marshal_BOXED__DOUBLE_DOUBLE_DOUBLE_DOUBLE;
            callback = G_CALLBACK (expression_create_rect);
          }
        else
          g_assert (FALSE);

        expression = gtk_cclosure_expression_new (
            value->type,
            marshal,
            params->len,
            (GtkExpression **) params->pdata,
            callback,
            self,
            NULL);
      }
      break;
    case VALUE_PROPERTY:
      {
        g_autoptr (GtkExpression) object_expression = NULL;

        /* Mark subproperty values as dependencies as well */
        object_expression = ensure_expressions (
            self, value->property.object, state, instance);
        expression = gtk_property_expression_new (
            value->property.object->type,
            g_steal_pointer (&object_expression),
            value->property.prop_name);
      }
      break;
    case VALUE_SPECIAL:
      g_assert (FALSE);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  g_hash_table_replace (
      instance->expressions,
      value_data_ref (value),
      gtk_expression_ref (expression));
  return gtk_expression_ref (expression);
}

static void
discard_binding (gpointer ptr)
{
  GBinding *binding = ptr;

  g_binding_unbind (binding);
  g_object_unref (binding);
}

static void
discard_watch (gpointer ptr)
{
  GtkExpressionWatch *watch = ptr;

  /* unrefs for us */
  gtk_expression_watch_unwatch (watch);
}

static void
prop_change_queue_draw (BgeWdgtRenderer *self)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/* End of bge-wdgt-spec.c */
