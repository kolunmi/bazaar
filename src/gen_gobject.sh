#!/bin/sh

# gen_gobject.sh
# author: kolunmi

die() {
    echo "Usage: $0 [--header|--code] [spec] [output]" 1>&2
    echo "  [--header|--code] whether to output the header (.h) or code (.c)" 1>&2
    echo "  [spec]            spec keyfile" 1>&2
    echo "  [output]          output file" 1>&2
    echo "" 1>&2
    echo "  The spec file should contain these keys:" 1>&2
    echo "    [prefix]        lower case prefix,             EX: my" 1>&2
    echo "    [name]          lower case class name,         EX: class" 1>&2
    echo "    [parent-prefix] prefix of parent class,        EX: g" 1>&2
    echo "    [parent-name]   name of parent class,          EX: object" 1>&2
    echo "    [author]        author name,                   EX: <your name>" 1>&2
    echo "    [include]       #include for the header file, (can have multiple)" 1>&2
    echo "                       EX: <gtk/gtk.h>" 1>&2
    echo "                       EX: \"my-other-class.h\"" 1>&2
    echo "    [property]      property spec (can have multiple),     EX: (see below)" 1>&2
    echo "" 1>&2
    echo "      The properties are parsed with the form:" 1>&2
    echo "        [name] [ctype] [gtype] [spec-type] [free (optional)] [ref (optional)]" 1>&2
    echo "        EX: my_widget GtkWidget GTK_TYPE_WIDGET object" 1>&2
    echo "        EX: my_string char G_TYPE_STRING string" 1>&2
    echo "        EX: my_int int G_TYPE_INT int" 1>&2
    echo "        EX: my_ptr_array GPtrArray G_TYPE_PTR_ARRAY boxed g_ptr_array_unref g_ptr_array_ref" 1>&2
    echo "" 1>&2
    echo "$@, aborting!" 1>&2
    exit 1
}


if [ "$#" -ne 3 ]; then
    die wrong number of args
fi

OUTPUT_TYPE="$1"
SPEC_FILE="$2"
OUTPUT_FILE="$3"

if [ -z "$OUTPUT_TYPE" ] ||
       [ -z "$SPEC_FILE" ] ||
       [ -z "$OUTPUT_FILE" ]; then
    die one or more args are empty
fi

case "$OUTPUT_TYPE" in
    --header|--code) ;;
    *) die arg 1 must be '--header' or '--code' ;;
esac

if ! [ -f "$SPEC_FILE" ]; then
    die "$SPEC_FILE isn't a file"
fi
# if [ -e "$OUTPUT_FILE" ]; then
#     die "$OUTPUT_FILE already exists"
# fi

unset PREF
unset NAME
unset PAR_PREF
unset PAR_NAME
unset AUTHOR
unset INCLUDES
unset PROPS

while IFS= read -r line; do
    KEY="${line%%=*}"
    VAL="${line#*=}"

    case "$KEY" in
        prefix)          PREF="$VAL" ;;
        name)            NAME="$VAL" ;;
        parent-prefix)   PAR_PREF="$VAL" ;;
        parent-name)     PAR_NAME="$VAL" ;;
        author)          AUTHOR="$VAL" ;;
        include)
            if [ -n "$INCLUDES" ]; then
                INCLUDES="${INCLUDES}
#include ${VAL}"
            else
                INCLUDES="#include ${VAL}"
            fi
            ;;
        property)
            if [ -n "$PROPS" ]; then
                PROPS="${PROPS}
${VAL}"
            else
                PROPS="$VAL"
            fi
            ;;
        *)  die "unknown key '${KEY}' in ${SPEC_FILE}" ;;
    esac
done < "$SPEC_FILE"

if [ -z "$PREF" ] ||
       [ -z "$NAME" ] ||
       [ -z "$PAR_PREF" ] ||
       [ -z "$PAR_NAME" ] ||
       [ -z "$AUTHOR" ] ||
       [ -z "$PROPS" ]; then
    die "one or more params in $SPEC_FILE are empty"
fi

case "$OUTPUT_TYPE" in
    --header)
        H_FILE="$OUTPUT_FILE"
        C_FILE="${OUTPUT_FILE%\.h}.c"
        ;;
    --code)
        H_FILE="${OUTPUT_FILE%\.c}.h"
        C_FILE="$OUTPUT_FILE"
        ;;
esac

to_upper() {
    echo "$1" | tr '[a-z]' '[A-Z]'
}

to_pascal() {
    echo "$1" | sed 's/[^_]\+/\L\u&/g' | tr -d '_'
}

to_hyphened() {
    echo "$1" | tr '_' '-'
}

SNAKE="${PREF}_${NAME}"
MACRO_PREF="$(to_upper "${PREF}")"
MACRO_NAME="$(to_upper "${NAME}")"
MACRO="${MACRO_PREF}_${MACRO_NAME}"
TYPE="${MACRO_PREF}_TYPE_${MACRO_NAME}"
PASCAL="$(to_pascal "${SNAKE}")"
HYPHEN_NAME="$(to_hyphened "${NAME}")"
HYPHEN="$(to_hyphened "${PREF}")-${HYPHEN_NAME}"

PAR_SNAKE="${PAR_PREF}_${PAR_NAME}"
PAR_MACRO_PREF="$(to_upper "${PAR_PREF}")"
PAR_MACRO_NAME="$(to_upper "${PAR_NAME}")"
PAR_MACRO="${PAR_MACRO_PREF}_${PAR_MACRO_NAME}"
PAR_TYPE="${PAR_MACRO_PREF}_TYPE_${PAR_MACRO_NAME}"
PAR_PASCAL="$(to_pascal "${PAR_SNAKE}")"
PAR_HYPHEN_NAME="$(to_hyphened "${PAR_NAME}")"
PAR_HYPHEN="$(to_hyphened "${PAR_PREF}")-${PAR_HYPHEN_NAME}"

YEAR="$(date +'%Y')"


print_struct () {
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"
        
        printf '  %s ' "$LOC_CTYPE"
        case "$LOC_PTYPE" in
            char|uchar|boolean|int|uint|long|ulong|int64|uint64|unichar|enum|flags|float|double) ;;
            *) printf '*'
        esac
        printf "%s;\n" "$LOC_NAME"
    done <<EOF
$PROPS
EOF
}

print_enums () {
    printf '  PROP_0,\n\n'
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"
        
        printf '  PROP_%s,\n' "$(to_upper $LOC_NAME)"
    done <<EOF
$PROPS
EOF
    printf '\n  LAST_PROP\n'
}

print_dispose () {
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"
        LOC_FREE="$5"
        
        case "$LOC_PTYPE" in
            char|uchar|boolean|int|uint|long|ulong|int64|uint64|unichar|enum|flags|float|double) ;;
            *)
                printf '  g_clear_pointer (&self->%s, ' "${LOC_NAME}"
                
                if [ -n "$LOC_FREE" ]; then
                    printf "$LOC_FREE"
                else
                    case "$LOC_PTYPE" in
                        string) printf 'g_free' ;;
                        *) printf 'g_object_unref' ;;
                    esac
                fi

                printf ');\n'
                ;;
        esac
    done <<EOF
$PROPS
EOF
}


print_get_property () {
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"

        printf '    case PROP_%s:\n' "$(to_upper $LOC_NAME)"
        printf '      g_value_set_%s (value, %s_get_%s (self));\n' "$LOC_PTYPE" "$SNAKE" "$LOC_NAME"
        printf '      break;\n'
    done <<EOF
$PROPS
EOF
}

print_set_property () {
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"
        
        printf '    case PROP_%s:\n' "$(to_upper $LOC_NAME)"
        printf '      %s_set_%s (self, g_value_get_%s (value));\n' "$SNAKE" "$LOC_NAME" "$LOC_PTYPE" 
        printf '      break;\n'
    done <<EOF
$PROPS
EOF
}

print_init_properties () {
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"
        
        printf '  props[PROP_%s] =\n' "$(to_upper $LOC_NAME)"
        printf '      g_param_spec_%s (\n' "$LOC_PTYPE"
        printf '          "%s",\n' "$(to_hyphened "$LOC_NAME")"
        printf '          NULL, NULL,'
        case "$LOC_PTYPE" in
            uchar|uint|ulong|uint64|unichar)
                printf '\n          0, G_MAX%s, 0,\n' "$(to_upper "$LOC_PTYPE")"
                ;;
            char|int|long|int64)
                printf '\n          G_MIN%s, G_MAX%s, 0,\n' "$(to_upper "$LOC_PTYPE")" "$(to_upper "$LOC_PTYPE")"
                ;;
            float|double)
                printf '\n          0.0, G_MAX%s, 0.0,\n' "$(to_upper "$LOC_PTYPE")"
                ;;
            boolean)
                printf ' FALSE,\n'
                ;;
            string)
                printf ' NULL,\n'
                ;;
            *)
                printf '\n          %s,\n' "$LOC_GTYPE"
                ;;
        esac
        printf '          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);\n\n'
    done <<EOF
$PROPS
EOF
}


print_functions () {
    HEADER="$1"

    printf '%s *\n%s_new (void)' "$PASCAL" "$SNAKE"
    if [ "$HEADER" == header ]; then
        printf ';\n\n'
    else
        printf '{\n  return g_object_new (%s, NULL);\n}\n\n' "$TYPE"
    fi
}


print_get_property_methods () {
    HEADER="$1"
    
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"

        case "$LOC_PTYPE" in
            string) printf 'const ' ;;
        esac
        printf '%s' "$LOC_CTYPE"
        case "$LOC_PTYPE" in
            char|uchar|boolean|int|uint|long|ulong|int64|uint64|unichar|enum|flags|float|double) ;;
            *) printf ' *'
        esac
        printf '\n%s_get_%s (%s *self)' "$SNAKE" "$LOC_NAME" "$PASCAL"
        
        if [ "$HEADER" == header ]; then
            printf ';\n\n'
        else
            printf '{\n  g_return_val_if_fail (%s_IS_%s (self), ' "$MACRO_PREF" "$MACRO_NAME"
            case "$LOC_PTYPE" in
                uchar|uint|ulong|uint64|unichar|char|int|long|int64)
                    printf '0'
                    ;;
                float|double)
                    printf '0.0'
                    ;;
                boolean)
                    printf 'FALSE'
                    ;;
                *)
                    printf 'NULL'
                    ;;
            esac
            printf ');\n'
            printf '  return self->%s;\n' "$LOC_NAME"
            printf '}\n\n'
        fi
        
    done <<EOF
$PROPS
EOF
}


print_set_property_methods () {
    HEADER="$1"
    
    while read -r line; do
        set -- $line
        
        LOC_NAME="$1"
        LOC_CTYPE="$2"
        LOC_GTYPE="$3"
        LOC_PTYPE="$4"
        LOC_FREE="$5"
        LOC_REF="$6"

        printf 'void\n%s_set_%s (%s *self,\n    ' "$SNAKE" "$LOC_NAME" "$PASCAL"
        case "$LOC_PTYPE" in
            string) printf 'const ' ;;
        esac
        printf '%s ' "$LOC_CTYPE"
        case "$LOC_PTYPE" in
            char|uchar|boolean|int|uint|long|ulong|int64|uint64|unichar|enum|flags|float|double) ;;
            *) printf '*'
        esac
        printf '%s)' "$LOC_NAME"
        
        if [ "$HEADER" == header ]; then
            printf ';\n\n'
        else
            printf '{\n  g_return_if_fail (%s_IS_%s (self));\n\n' "$MACRO_PREF" "$MACRO_NAME"

            case "$LOC_PTYPE" in
                char|uchar|boolean|int|uint|long|ulong|int64|uint64|unichar|enum|flags|float|double) ;;
                *)
                    printf '  g_clear_pointer (&self->%s, ' "$LOC_NAME"
                    
                    if [ -n "$LOC_FREE" ]; then
                        printf "$LOC_FREE"
                    else
                        case "$LOC_PTYPE" in
                            string) printf 'g_free' ;;
                            *) printf 'g_object_unref' ;;
                        esac
                    fi

                    printf ');\n'
                    printf '  if (%s != NULL)\n  ' "$LOC_NAME"
                    ;;
            esac

            printf '  self->%s = ' "$LOC_NAME"
            if [ -n "$LOC_REF" ]; then
                printf '%s (%s)' "$LOC_REF" "$LOC_NAME"
            else
                case "$LOC_PTYPE" in
                    char|uchar|boolean|int|uint|long|ulong|int64|uint64|unichar|enum|flags|float|double)
                        printf '%s' "$LOC_NAME"
                        ;;
                    string)
                        printf 'g_strdup (%s)' "$LOC_NAME"
                        ;;
                    *)
                        printf 'g_object_ref (%s)' "$LOC_NAME"
                        ;;
                esac
            fi
            printf ';\n\n'
            
            printf '  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_%s]);\n' "$(to_upper $LOC_NAME)"
            printf '}\n\n'
        fi
        
    done <<EOF
$PROPS
EOF
}


[ "$OUTPUT_TYPE" = --header ] && cat > "$H_FILE" <<EOF
/* $H_FILE
 *
 * Copyright $YEAR $AUTHOR
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

#pragma once

#include <glib-object.h>
$INCLUDES

G_BEGIN_DECLS

#define $TYPE (${SNAKE}_get_type ())
G_DECLARE_FINAL_TYPE ($PASCAL, $SNAKE, $MACRO_PREF, $MACRO_NAME, $PAR_PASCAL)

$(print_functions header)

$(print_get_property_methods header)

$(print_set_property_methods header)

G_END_DECLS

/* End of $H_FILE */
EOF



[ "$OUTPUT_TYPE" = --code ] && cat > "$C_FILE" <<EOF
/* $C_FILE
 *
 * Copyright $YEAR $AUTHOR
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

#include "$H_FILE"

struct _${PASCAL}
{
  $PAR_PASCAL parent_instance;

$(print_struct)
};

G_DEFINE_FINAL_TYPE ($PASCAL, $SNAKE, $PAR_TYPE);

enum
{
$(print_enums)
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
${SNAKE}_dispose (GObject *object)
{
  $PASCAL *self = $MACRO (object);

$(print_dispose)

  G_OBJECT_CLASS (${SNAKE}_parent_class)->dispose (object);
}

static void
${SNAKE}_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  $PASCAL *self = $MACRO (object);

  switch (prop_id)
    {
$(print_get_property)
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
${SNAKE}_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  $PASCAL *self = $MACRO (object);

  switch (prop_id)
    {
$(print_set_property)
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
${SNAKE}_class_init (${PASCAL}Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = ${SNAKE}_set_property;
  object_class->get_property = ${SNAKE}_get_property;
  object_class->dispose      = ${SNAKE}_dispose;

$(print_init_properties)

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
${SNAKE}_init (${PASCAL} *self)
{
}

$(print_functions)

$(print_get_property_methods)

$(print_set_property_methods)

/* End of $C_FILE */
EOF


exit 0
