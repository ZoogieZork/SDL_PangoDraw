/* Pango
 * pango-layout.h: Highlevel layout driver
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

#ifndef __PJX_LAYOUT_H__
#define __PJX_LAYOUT_H__

#include <pango/pango-layout.h>
#include <pango/pango-attributes.h>
#include <pango/pango-context.h>
#include <pango/pango-glyph-item.h>
#include <pango/pango-tabs.h>

G_BEGIN_DECLS

#define pango_layout_get_log_attrs pjx_layout_get_log_attrs
void     pjx_layout_get_log_attrs (PangoLayout    *layout,
				     PangoLogAttr  **attrs,
				     gint           *n_attrs);

#define pango_layout_get_cursor_pos pjx_layout_get_cursor_pos
void     pjx_layout_get_cursor_pos       (PangoLayout    *layout,
					    int             index_,
					    PangoRectangle *strong_pos,
					    PangoRectangle *weak_pos);

#define pango_layout_move_cursor_visually pjx_layout_move_cursor_visually
void     pjx_layout_move_cursor_visually (PangoLayout    *layout,
					    gboolean        strong,
					    int             old_index,
					    int             old_trailing,
					    int             direction,
					    int            *new_index,
					    int            *new_trailing);

#define pango_layout_get_line_count pjx_layout_get_line_count
int              pjx_layout_get_line_count       (PangoLayout    *layout);

#define pango_layout_get_line pjx_layout_get_line
PangoLayoutLine *pjx_layout_get_line             (PangoLayout    *layout,
						    int             line);
#define pango_layout_get_lines pjx_layout_get_lines
GSList *         pjx_layout_get_lines            (PangoLayout    *layout);

#define pango_layout_get_iter pjx_layout_get_iter
PangoLayoutIter *pjx_layout_get_iter  (PangoLayout     *layout);

#define pango_layout_get_extents pjx_layout_get_extents
void pjx_layout_get_extents (PangoLayout    *layout,
			  PangoRectangle *ink_rect,
			  PangoRectangle *logical_rect);

#define pango_layout_set_markup pjx_layout_set_markup
void           pjx_layout_set_markup     (PangoLayout    *layout,
                                            const char     *markup,
                                            int             length);

#define pango_layout_set_markup_with_accel pjx_layout_set_markup_with_accel
void           pjx_layout_set_markup_with_accel (PangoLayout    *layout,
                                                   const char     *markup,
                                                   int             length,
                                                   gunichar        accel_marker,
                                                   gunichar       *accel_char);

void pjx_layout_check(PangoLayout *layout);

G_END_DECLS

#endif /* __PJX_LAYOUT_H__ */

