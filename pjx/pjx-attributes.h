/* Pango
 * pjx-attributes.h: Attributed text
 *
 * Copyright (C) 2000 Red Hat Software
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __PJX_ATTRIBUTES_H__
#define __PJX_ATTRIBUTES_H__

#include <pango/pango-font.h>
#include <glib-object.h>
#include <pango/pango-attributes.h>

G_BEGIN_DECLS

gboolean pjx_parse_markup (const char                 *markup_text,
                             int                         length,
                             gunichar                    accel_marker,
                             PangoAttrList             **attr_list,
                             char                      **text,
                             gunichar                   *accel_char,
                             GError                    **error);

void pjx_attr_init ();
void pjx_attr_destroy ();

PangoAttrType pjx_attr_ruby ();
PangoAttrType pjx_attr_rb ();
PangoAttrType pjx_attr_rt ();

#define PJX_ATTR_RUBY pjx_attr_ruby ()
#define PJX_ATTR_RB pjx_attr_rb ()
#define PJX_ATTR_RT pjx_attr_rt ()

PangoAttribute *pango_attr_ruby_new ();
PangoAttribute *pango_attr_rb_new ();
PangoAttribute *pango_attr_rt_new ();

PangoAttribute* pjx_attr_get_from_list (GSList *attrs, PangoAttrType attr_type);

void
pjx_attr_list_change (PangoAttrList  *list,
			PangoAttribute *attr);

G_END_DECLS

#endif /* __PJX_ATTRIBUTES_H__ */
