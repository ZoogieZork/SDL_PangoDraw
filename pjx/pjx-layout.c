/* Pango
 * pango-layout.c: Highlevel layout driver
 *
 * Copyright (C) 2000, 2001 Red Hat Software
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

#include <pango/pango-glyph.h>		/* For pango_shape() */
#include <pango/pango-break.h>
#include <pango/pango-item.h>
#include <pango/pango-engine.h>
#include <string.h>

#include "pjx-layout.h"
#include "pango-layout-private.h"
#include "pjx-attributes.h"

#define LINE_IS_VALID(line) ((line)->layout != NULL)

typedef struct _Extents Extents;
typedef struct _ItemProperties ItemProperties;
typedef struct _ParaBreakState ParaBreakState;

struct _Extents
{
  /* Vertical position of the line's baseline in layout coords */
  int baseline;
  
  /* Line extents in layout coords */
  PangoRectangle ink_rect;
  PangoRectangle logical_rect;
};

struct _ItemProperties
{
  PangoUnderline  uline;
  gint            rise;
  gint            letter_spacing;
  gboolean        shape_set;
  PangoRectangle *shape_ink_rect;
  PangoRectangle *shape_logical_rect;
};

struct _PangoLayoutIter
{
  PangoLayout *layout;
  GSList *line_list_link;
  PangoLayoutLine *line;

  /* If run is NULL, it means we're on a "virtual run"
   * at the end of the line with 0 width
   */
  GSList *run_list_link;
  PangoLayoutRun *run; /* FIXME nuke this, just keep the link */
  int index;

  /* layout extents in layout coordinates */
  PangoRectangle logical_rect;

  /* list of Extents for each line in layout coordinates */
  GSList *line_extents;
  GSList *line_extents_link;
  
  /* X position of the current run */
  int run_x;

  /* Extents of the current run */
  PangoRectangle run_logical_rect;

  /* this run is left-to-right */
  gboolean ltr;

  /* X position where cluster begins, where "begins" means left or
   * right side according to text direction
   */
  int cluster_x;

  /* Byte index of the cluster start from the run start */
  int cluster_index;
  
  /* Glyph offset to the current cluster start */
  int cluster_start;

  /* The next cluster_start */
  int next_cluster_start;
};

typedef struct _PangoLayoutLinePrivate PangoLayoutLinePrivate;

struct _PangoLayoutLinePrivate
{
  PangoLayoutLine line;
  guint ref_count;
};

static void pjx_layout_check_lines (PangoLayout *layout);

static PangoAttrList *pjx_layout_get_effective_attributes (PangoLayout *layout);

static PangoLayoutLine * pjx_layout_line_new         (PangoLayout     *layout);
static void              pjx_layout_line_postprocess (PangoLayoutLine *line,
							ParaBreakState  *state);

static int *pjx_layout_line_get_log2vis_map (PangoLayoutLine  *line,
					       gboolean          strong);
static int *pjx_layout_line_get_vis2log_map (PangoLayoutLine  *line,
					       gboolean          strong);

static void pjx_layout_get_item_properties (PangoItem      *item,
					      ItemProperties *properties);

/**
 * pjx_layout_get_log_attrs:
 * @layout: a #PangoLayout
 * @attrs: location to store a pointer to an array of logical attributes
 *         This value must be freed with g_free().
 * @n_attrs: location to store the number of the attributes in the
 *           array. (The stored value will be equal to the total number
 *           of characters in the layout.)
 * 
 * Retrieves an array of logical attributes for each character in
 * the @layout. 
 **/
void
pjx_layout_get_log_attrs (PangoLayout    *layout,
			    PangoLogAttr  **attrs,
			    gint           *n_attrs)
{
  g_return_if_fail (layout != NULL);

  pjx_layout_check_lines (layout);

  if (attrs)
    {
      *attrs = g_new (PangoLogAttr, layout->n_chars);
      memcpy (*attrs, layout->log_attrs, sizeof(PangoLogAttr) * layout->n_chars);
    }
  
  if (n_attrs)
    *n_attrs = layout->n_chars;
}


/**
 * pjx_layout_get_line_count:
 * @layout: #PangoLayout
 * 
 * Retrieves the count of lines for the @layout.
 * 
 * Return value: the line count
 **/
int
pjx_layout_get_line_count (PangoLayout   *layout)
{
  g_return_val_if_fail (layout != NULL, 0);

  pjx_layout_check_lines (layout);
  return g_slist_length (layout->lines);
}

/**
 * pjx_layout_get_lines:
 * @layout: a #PangoLayout
 * 
 * Returns the lines of the @layout as a list.
 * 
 * Return value: a #GSList containing the lines in the layout. This
 * points to internal data of the #PangoLayout and must be used with
 * care. It will become invalid on any change to the layout's
 * text or properties.
 **/
GSList *
pjx_layout_get_lines (PangoLayout *layout)
{
  pjx_layout_check_lines (layout);
  return layout->lines;
}

/**
 * pjx_layout_get_line:
 * @layout: a #PangoLayout
 * @line: the index of a line, which must be between 0 and
 *        <literal>pango_layout_get_line_count(layout) - 1</literal>, inclusive.
 * 
 * Retrieves a particular line from a #PangoLayout.
 * 
 * Return value: the requested #PangoLayoutLine, or %NULL if the
 *               index is out of range. This layout line can
 *               be ref'ed and retained, but will become invalid
 *               if changes are made to the #PangoLayout.
 **/
PangoLayoutLine *
pjx_layout_get_line (PangoLayout *layout,
		       int          line)
{
  GSList *list_item;
  g_return_val_if_fail (layout != NULL, NULL);
  g_return_val_if_fail (line >= 0, NULL);

  if (line < 0)
    return NULL;

  pjx_layout_check_lines (layout);
  list_item = g_slist_nth (layout->lines, line);
  if (list_item)
    return list_item->data;
  return NULL;
}

static PangoLayoutLine *
pjx_layout_index_to_line (PangoLayout      *layout,
			    int               index,
			    int              *line_nr,
			    PangoLayoutLine **line_before,
			    PangoLayoutLine **line_after)
{
  GSList *tmp_list;
  GSList *line_list;
  PangoLayoutLine *line = NULL;
  PangoLayoutLine *prev_line = NULL;
  int i = 0;
  
  line_list = tmp_list = layout->lines;
  while (tmp_list)
    {
      PangoLayoutLine *tmp_line = tmp_list->data;

      if (tmp_line && tmp_line->start_index > index)
        break; /* index was in paragraph delimiters */

      prev_line = line;
      line = tmp_line;
      line_list = tmp_list;
      i++;

      if (line->start_index + line->length > index)
        break;
      
      tmp_list = tmp_list->next;
    }

  if (line_nr)
    *line_nr = i;
  
  if (line_before)
    *line_before = prev_line;
  
  if (line_after)
    *line_after = (line_list && line_list->next) ? line_list->next->data : NULL;

  return line;
}

/**
 * pjx_layout_move_cursor_visually:
 * @layout:       a #PangoLayout.
 * @strong:       whether the moving cursor is the strong cursor or the
 *                weak cursor. The strong cursor is the cursor corresponding
 *                to text insertion in the base direction for the layout.
 * @old_index:    the byte index of the grapheme for the old index
 * @old_trailing: if 0, the cursor was at the trailing edge of the 
 *                grapheme indicated by @old_index, if > 0, the cursor
 *                was at the leading edge.
 * @direction:    direction to move cursor. A negative
 *                value indicates motion to the left.
 * @new_index:    location to store the new cursor byte index. A value of -1 
 *                indicates that the cursor has been moved off the beginning
 *                of the layout. A value of G_MAXINT indicates that
 *                the cursor has been moved off the end of the layout.
 * @new_trailing: number of characters to move forward from the location returned
 *                for @new_index to get the position where the cursor should
 *                be displayed. This allows distinguishing the position at
 *                the beginning of one line from the position at the end
 *                of the preceding line. @new_index is always on the line
 *                where the cursor should be displayed. 
 * 
 * Computes a new cursor position from an old position and
 * a count of positions to move visually. If @count is positive,
 * then the new strong cursor position will be one position
 * to the right of the old cursor position. If @count is position
 * then the new strong cursor position will be one position
 * to the left of the old cursor position. 
 *
 * In the presence of bidirection text, the correspondence
 * between logical and visual order will depend on the direction
 * of the current run, and there may be jumps when the cursor
 * is moved off of the end of a run.
 *
 * Motion here is in cursor positions, not in characters, so a
 * single call to pango_layout_move_cursor_visually() may move the
 * cursor over multiple characters when multiple characters combine
 * to form a single grapheme.
 **/
void
pjx_layout_move_cursor_visually (PangoLayout *layout,
				   gboolean     strong,
				   int          old_index,
				   int          old_trailing,
				   int          direction,
				   int         *new_index,
				   int         *new_trailing)
{
  PangoLayoutLine *line = NULL;
  PangoLayoutLine *prev_line;
  PangoLayoutLine *next_line;

  int *log2vis_map;
  int *vis2log_map;
  int n_vis;
  int vis_pos, log_pos;
  int start_offset;
  gboolean off_start = FALSE;
  gboolean off_end = FALSE;

  g_return_if_fail (layout != NULL);
  g_return_if_fail (old_index >= 0 && old_index <= layout->length);
  g_return_if_fail (old_index < layout->length || old_trailing == 0);
  g_return_if_fail (new_index != NULL);
  g_return_if_fail (new_trailing != NULL);

  pjx_layout_check_lines (layout);

  /* Find the line the old cursor is on */
  line = pjx_layout_index_to_line (layout, old_index,
				     NULL, &prev_line, &next_line);

  start_offset = g_utf8_pointer_to_offset (layout->text, layout->text + line->start_index);

  while (old_trailing--)
    old_index = g_utf8_next_char (layout->text + old_index) - layout->text;

  log2vis_map = pjx_layout_line_get_log2vis_map (line, strong);
  n_vis = g_utf8_strlen (layout->text + line->start_index, line->length);

  /* Clamp old_index to fit on the line */
  if (old_index > (line->start_index + line->length))
    old_index = line->start_index + line->length;
  
  vis_pos = log2vis_map[old_index - line->start_index];
  g_free (log2vis_map);

  /* Handling movement between lines */
  if (vis_pos == 0 && direction < 0)
    {
      if (line->resolved_dir == PANGO_DIRECTION_LTR)
	off_start = TRUE;
      else
	off_end = TRUE;
    }
  else if (vis_pos == n_vis && direction > 0)
    {
      if (line->resolved_dir == PANGO_DIRECTION_LTR)
	off_end = TRUE;
      else
	off_start = TRUE;
    }

  if (off_start || off_end)
    {
      /* If we move over a paragraph boundary, count that as
       * an extra position in the motion
       */
      gboolean paragraph_boundary;

      if (off_start)
	{
	  if (!prev_line)
	    {
	      *new_index = -1;
	      *new_trailing = 0;
	      return;
	    }
	  line = prev_line;
	  paragraph_boundary = (line->start_index + line->length != old_index);
	}
      else
	{
	  if (!next_line)
	    {
	      *new_index = G_MAXINT;
	      *new_trailing = 0;
	      return;
	    }
	  line = next_line;
	  paragraph_boundary = (line->start_index != old_index);
	}

      if (vis_pos == 0 && direction < 0)
	{
	  vis_pos = g_utf8_strlen (layout->text + line->start_index, line->length);
	  if (paragraph_boundary)
	    vis_pos++;
	}
      else /* (vis_pos == n_vis && direction > 0) */
	{
	  vis_pos = 0;
	  if (paragraph_boundary)
	    vis_pos--;
	}
    }

  vis2log_map = pjx_layout_line_get_vis2log_map (line, strong);

  do
    {
      vis_pos += direction > 0 ? 1 : -1;
      log_pos = g_utf8_pointer_to_offset (layout->text + line->start_index,
					  layout->text + line->start_index + vis2log_map[vis_pos]);
    }
  while (vis_pos > 0 && vis_pos < n_vis &&
	 !layout->log_attrs[start_offset + log_pos].is_cursor_position);
  
  *new_index = line->start_index + vis2log_map[vis_pos];
  g_free (vis2log_map);

  *new_trailing = 0;
    
  if (*new_index == line->start_index + line->length && line->length > 0)
    {
      do
	{
	  log_pos--;
	  *new_index = g_utf8_prev_char (layout->text + *new_index) - layout->text;
	  (*new_trailing)++;
	}
      while (log_pos > 0 && !layout->log_attrs[start_offset + log_pos].is_cursor_position);
    }
}

static void
pjx_layout_line_get_range (PangoLayoutLine *line,
			     char           **start,
			     char           **end)
{
  char *p;
  
  p = line->layout->text + line->start_index;

  if (start)
    *start = p;
  if (end)
    *end = p + line->length;
}

static int *
pjx_layout_line_get_vis2log_map (PangoLayoutLine *line,
				   gboolean         strong)
{
  PangoLayout *layout = line->layout;
  PangoDirection prev_dir;
  PangoDirection cursor_dir;
  GSList *tmp_list;
  gchar *start, *end;
  int *result;
  int pos;
  int n_chars;

  pjx_layout_line_get_range (line, &start, &end);
  n_chars = g_utf8_strlen (start, end - start);
  
  result = g_new (int, n_chars + 1);

  if (strong)
    cursor_dir = line->resolved_dir;
  else
    cursor_dir = (line->resolved_dir == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;

  /* Handle the first visual position
   */
  if (line->resolved_dir == cursor_dir)
    result[0] = line->resolved_dir == PANGO_DIRECTION_LTR ? 0 : end - start;

  prev_dir = line->resolved_dir;
  pos = 0;
  tmp_list = line->runs;
  while (tmp_list)
    {
      PangoLayoutRun *run = tmp_list->data;
      int run_n_chars = run->item->num_chars;
      PangoDirection run_dir = (run->item->analysis.level % 2) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
      char *p = layout->text + run->item->offset;
      int i;

      /* pos is the visual position at the start of the run */
      /* p is the logical byte index at the start of the run */

      if (run_dir == PANGO_DIRECTION_LTR)
	{
	  if ((cursor_dir == PANGO_DIRECTION_LTR) ||
	      (prev_dir == run_dir))
	    result[pos] = p - start;
	  
	  p = g_utf8_next_char (p);
	  
	  for (i = 1; i < run_n_chars; i++)
	    {
	      result[pos + i] = p - start;
	      p = g_utf8_next_char (p);
	    }
	  
	  if (cursor_dir == PANGO_DIRECTION_LTR)
	    result[pos + run_n_chars] = p - start;
	}
      else
	{
	  if (cursor_dir == PANGO_DIRECTION_RTL)
	    result[pos + run_n_chars] = p - start;

	  p = g_utf8_next_char (p);

	  for (i = 1; i < run_n_chars; i++)
	    {
	      result[pos + run_n_chars - i] = p - start;
	      p = g_utf8_next_char (p);
	    }

	  if ((cursor_dir == PANGO_DIRECTION_RTL) ||
	      (prev_dir == run_dir))
	    result[pos] = p - start;
	}

      pos += run_n_chars;
      prev_dir = run_dir;
      tmp_list = tmp_list->next;
    }

  /* And the last visual position
   */
  if ((cursor_dir == line->resolved_dir) || (prev_dir == line->resolved_dir))
    result[pos] = line->resolved_dir == PANGO_DIRECTION_LTR ? end - start : 0;
      
  return result;
}

static int *
pjx_layout_line_get_log2vis_map (PangoLayoutLine *line,
				   gboolean         strong)
{
  gchar *start, *end;
  int *reverse_map;
  int *result;
  int i;
  int n_chars;

  pjx_layout_line_get_range (line, &start, &end);
  n_chars = g_utf8_strlen (start, end - start);
  result = g_new0 (int, end - start + 1);

  reverse_map = pjx_layout_line_get_vis2log_map (line, strong);

  for (i=0; i <= n_chars; i++)
    result[reverse_map[i]] = i;

  g_free (reverse_map);

  return result;
}

static PangoLayoutLine *
pjx_layout_index_to_line_and_extents (PangoLayout     *layout,
					int              index,
					PangoRectangle  *line_rect)
{
  PangoLayoutIter *iter;
  PangoLayoutLine *line = NULL;
  
  iter = pjx_layout_get_iter (layout);

  while (TRUE)
    {
      PangoLayoutLine *tmp_line = pango_layout_iter_get_line (iter);

      if (tmp_line && tmp_line->start_index > index)
          break; /* index was in paragraph delimiters */

      line = tmp_line;
      
      pango_layout_iter_get_line_extents (iter, NULL, line_rect);
      
      if (line->start_index + line->length > index)
        break;

      if (!pango_layout_iter_next_line (iter))
	break; /* Use end of last line */
    }

  pango_layout_iter_free (iter);

  return line;
}

static PangoDirection
pjx_layout_line_get_char_direction (PangoLayoutLine *layout_line,
				      int              index)
{
  GSList *run_list;

  run_list = layout_line->runs;
  while (run_list)
    {
      PangoLayoutRun *run = run_list->data;
      
      if (run->item->offset <= index && run->item->offset + run->item->length > index)
	return run->item->analysis.level % 2 ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;

      run_list = run_list->next;
    }

  g_assert_not_reached ();

  return PANGO_DIRECTION_LTR;
}

/**
 * pjx_layout_get_cursor_pos:
 * @layout: a #PangoLayout
 * @index_: the byte index of the cursor
 * @strong_pos: location to store the strong cursor position (may be %NULL)
 * @weak_pos: location to store the weak cursor position (may be %NULL)
 * 
 * Given an index within a layout, determines the positions that of the
 * strong and weak cursors if the insertion point is at that
 * index. The position of each cursor is stored as a zero-width
 * rectangle. The strong cursor location is the location where
 * characters of the directionality equal to the base direction of the
 * layout are inserted.  The weak cursor location is the location
 * where characters of the directionality opposite to the base
 * direction of the layout are inserted.
 **/
void
pjx_layout_get_cursor_pos (PangoLayout    *layout,
			     int             index,
			     PangoRectangle *strong_pos,
			     PangoRectangle *weak_pos)
{
  PangoDirection dir1, dir2;
  PangoRectangle line_rect;
  PangoLayoutLine *layout_line = NULL; /* Quiet GCC */
  int x1_trailing;
  int x2;
  
  g_return_if_fail (layout != NULL);
  g_return_if_fail (index >= 0 && index <= layout->length);
  
  layout_line = pjx_layout_index_to_line_and_extents (layout, index,
							&line_rect);

  g_assert (index >= layout_line->start_index);
  
  /* Examine the trailing edge of the character before the cursor */
  if (index == layout_line->start_index)
    {
      dir1 = layout_line->resolved_dir;
      if (layout_line->resolved_dir == PANGO_DIRECTION_LTR)
        x1_trailing = 0;
      else
        x1_trailing = line_rect.width;
    }
  else
    {
      gint prev_index = g_utf8_prev_char (layout->text + index) - layout->text;
      dir1 = pjx_layout_line_get_char_direction (layout_line, prev_index);
      pango_layout_line_index_to_x (layout_line, prev_index, TRUE, &x1_trailing);
    }
  
  /* Examine the leading edge of the character after the cursor */
  if (index >= layout_line->start_index + layout_line->length)
    {
      dir2 = layout_line->resolved_dir;
      if (layout_line->resolved_dir == PANGO_DIRECTION_LTR)
        x2 = line_rect.width;
      else
        x2 = 0;
    }
  else
    {
      dir2 = pjx_layout_line_get_char_direction (layout_line, index);
      pango_layout_line_index_to_x (layout_line, index, FALSE, &x2);
    }
	  
  if (strong_pos)
    {
      strong_pos->x = line_rect.x;
      
      if (dir1 == layout_line->resolved_dir)
	strong_pos->x += x1_trailing;
      else
	strong_pos->x += x2;

      strong_pos->y = line_rect.y;
      strong_pos->width = 0;
      strong_pos->height = line_rect.height;
    }

  if (weak_pos)
    {
      weak_pos->x = line_rect.x;
      
      if (dir1 == layout_line->resolved_dir)
	weak_pos->x += x2;
      else
	weak_pos->x += x1_trailing;

      weak_pos->y = line_rect.y;
      weak_pos->width = 0;
      weak_pos->height = line_rect.height;
    }
}

static inline int
direction_simple (PangoDirection d)
{
  switch (d)
    {
    case PANGO_DIRECTION_LTR :	    return 1;
    case PANGO_DIRECTION_RTL :	    return -1;
    case PANGO_DIRECTION_TTB_LTR :  return 1;
    case PANGO_DIRECTION_TTB_RTL :  return -1;
    case PANGO_DIRECTION_WEAK_LTR : return 1;
    case PANGO_DIRECTION_WEAK_RTL : return -1;
    case PANGO_DIRECTION_NEUTRAL :  return 0;
    /* no default compiler should complain if a new values is added */
    }
  /* not reached */
  return 0;
}

static PangoAlignment
get_alignment (PangoLayout     *layout,
	       PangoLayoutLine *line)
{
  PangoAlignment alignment = layout->alignment;

  if (line->layout->auto_dir &&
      direction_simple (line->resolved_dir) ==
      -direction_simple (pango_context_get_base_dir (layout->context)))
    {
      if (alignment == PANGO_ALIGN_LEFT)
	alignment = PANGO_ALIGN_RIGHT;
      else if (alignment == PANGO_ALIGN_RIGHT)
	alignment = PANGO_ALIGN_LEFT;
    }

  return alignment;
}

static void
get_x_offset (PangoLayout     *layout,
              PangoLayoutLine *line,
              int              layout_width,
              int              line_width,
              int             *x_offset)
{
  PangoAlignment alignment = get_alignment (layout, line);

  /* Alignment */
  if (alignment == PANGO_ALIGN_RIGHT)
    *x_offset = layout_width - line_width;
  else if (alignment == PANGO_ALIGN_CENTER)
    *x_offset = (layout_width - line_width) / 2;
  else
    *x_offset = 0;

  /* Indentation */


  /* For center, we ignore indentation; I think I've seen word
   * processors that still do the indentation here as if it were
   * indented left/right, though we can't sensibly do that without
   * knowing whether left/right is the "normal" thing for this text
   */
   
  if (alignment == PANGO_ALIGN_CENTER)
    return;
  
  if (line->is_paragraph_start)
    {
      if (layout->indent > 0)
        {
          if (alignment == PANGO_ALIGN_LEFT)
            *x_offset += layout->indent;
          else
            *x_offset -= layout->indent;
        }
    }
  else
    {
      if (layout->indent < 0)
        {
          if (alignment == PANGO_ALIGN_LEFT)
            *x_offset -= layout->indent;
          else
            *x_offset += layout->indent;
        }
    }
}

static void
get_line_extents_layout_coords (PangoLayout     *layout,
                                PangoLayoutLine *line,
                                int              layout_width,
                                int              y_offset,
                                int             *baseline,
                                PangoRectangle  *line_ink_layout,
                                PangoRectangle  *line_logical_layout)
{
  int x_offset;
  /* Line extents in line coords (origin at line baseline) */
  PangoRectangle line_ink;
  PangoRectangle line_logical;
  
  pango_layout_line_get_extents (line, line_ink_layout ? &line_ink : NULL,
                                 &line_logical);
  
  get_x_offset (layout, line, layout_width, line_logical.width, &x_offset);
  
  /* Convert the line's extents into layout coordinates */
  if (line_ink_layout)
    {
      *line_ink_layout = line_ink;
      line_ink_layout->x = line_ink.x + x_offset;
      line_ink_layout->y = y_offset - line_logical.y + line_ink.y;
    }

  if (line_logical_layout)
    {
      *line_logical_layout = line_logical;
      line_logical_layout->x = line_logical.x + x_offset;
      line_logical_layout->y = y_offset;
    }

  if (baseline)
    *baseline = y_offset - line_logical.y;
}

/* if non-NULL line_extents returns a list of line extents
 * in layout coordinates
 */
static void
pjx_layout_get_extents_internal (PangoLayout    *layout,
                                   PangoRectangle *ink_rect,
                                   PangoRectangle *logical_rect,
                                   GSList        **line_extents)
{
  GSList *line_list;
  int y_offset = 0;
  int width;
  gboolean need_width = FALSE;

  g_return_if_fail (layout != NULL);

  pjx_layout_check_lines (layout);

  /* When we are not wrapping, we need the overall width of the layout to 
   * figure out the x_offsets of each line. However, we only need the 
   * x_offsets if we are computing the ink_rect or individual line extents.
   */
  width = layout->width;

  /* If one of the lines of the layout is not left aligned, then we need
   * the width of the width to calculate line x-offsets; this requires
   * looping through the lines for layout->auto_dir.
  */
  if (layout->auto_dir)
    {
      line_list = layout->lines;
      while (line_list)
	{
	  PangoLayoutLine *line = line_list->data;

	  if (get_alignment (layout, line) != PANGO_ALIGN_LEFT)
	    need_width = TRUE;

	  line_list = line_list->next;
	}
    }
  else if (layout->alignment != PANGO_ALIGN_LEFT)
    need_width = TRUE;
  
  if (width == -1 && need_width && (ink_rect || line_extents))
    {
      PangoRectangle overall_logical;
      
      pango_layout_get_extents (layout, NULL, &overall_logical);
      width = overall_logical.width;
    }
  
  line_list = layout->lines;
  while (line_list)
    {
      PangoLayoutLine *line = line_list->data;
      /* Line extents in layout coords (origin at 0,0 of the layout) */
      PangoRectangle line_ink_layout;
      PangoRectangle line_logical_layout;
      
      int new_pos;

      /* This block gets the line extents in layout coords */
      {
        int baseline;
        
        get_line_extents_layout_coords (layout, line,
                                        width, y_offset,
                                        &baseline,
                                        ink_rect ? &line_ink_layout : NULL,
                                        &line_logical_layout);
        
        if (line_extents)
          {
            Extents *ext = g_new (Extents, 1);
            ext->baseline = baseline;
            ext->ink_rect = line_ink_layout;
            ext->logical_rect = line_logical_layout;
            *line_extents = g_slist_prepend (*line_extents, ext);
          }
      }
      
      if (ink_rect)
	{
          /* Compute the union of the current ink_rect with
           * line_ink_layout
           */
          
	  if (line_list == layout->lines)
	    {
              *ink_rect = line_ink_layout;
	    }
	  else
	    {
	      new_pos = MIN (ink_rect->x, line_ink_layout.x);
	      ink_rect->width =
                MAX (ink_rect->x + ink_rect->width,
                     line_ink_layout.x + line_ink_layout.width) - new_pos;
	      ink_rect->x = new_pos;

	      new_pos = MIN (ink_rect->y, line_ink_layout.y);
	      ink_rect->height =
                MAX (ink_rect->y + ink_rect->height,
                     line_ink_layout.y + line_ink_layout.height) - new_pos;
	      ink_rect->y = new_pos;
            }
	}

      if (logical_rect)
	{
          /* Compute union of line_logical_layout with
           * current logical_rect
           */
          
	  if (line_list == layout->lines)
	    {
              *logical_rect = line_logical_layout;
	    }
	  else
	    {
	      new_pos = MIN (logical_rect->x, line_logical_layout.x);
	      logical_rect->width =
                MAX (logical_rect->x + logical_rect->width,
                     line_logical_layout.x + line_logical_layout.width) - new_pos;
	      logical_rect->x = new_pos;

	      logical_rect->height += line_logical_layout.height;
	    }

          /* No space after the last line, of course. */
          if (line_list->next != NULL)
            logical_rect->height += layout->spacing;
	}
      
      y_offset += line_logical_layout.height + layout->spacing;
      line_list = line_list->next;
    }

  if (line_extents)
    *line_extents = g_slist_reverse (*line_extents);
}

/**
 * pjx_layout_get_extents:
 * @layout:   a #PangoLayout
 * @ink_rect: rectangle used to store the extents of the layout as drawn
 *            or %NULL to indicate that the result is not needed.
 * @logical_rect: rectangle used to store the logical extents of the layout 
                 or %NULL to indicate that the result is not needed.
 * 
 * Computes the logical and ink extents of @layout. Logical extents
 * are usually what you want for positioning things. The extents
 * are given in layout coordinates; layout coordinates begin at the
 * top left corner of the layout. 
 */
void
pjx_layout_get_extents (PangoLayout    *layout,
			  PangoRectangle *ink_rect,
			  PangoRectangle *logical_rect)
{	  
  g_return_if_fail (layout != NULL);

  pjx_layout_get_extents_internal (layout, ink_rect, logical_rect, NULL);
}

/************************************************
 * Some functions for handling PANGO_ATTR_SHAPE *
 ************************************************/

static void
imposed_shape (const char       *text,
	       gint              n_chars,
	       PangoRectangle   *shape_ink,
	       PangoRectangle   *shape_logical,
	       PangoGlyphString *glyphs)
{
  int i;
  const char *p;
  
  pango_glyph_string_set_size (glyphs, n_chars);

  for (i=0, p = text; i < n_chars; i++, p = g_utf8_next_char (p))
    {
      glyphs->glyphs[i].glyph = 0;
      glyphs->glyphs[i].geometry.x_offset = 0;
      glyphs->glyphs[i].geometry.y_offset = 0;
      glyphs->glyphs[i].geometry.width = shape_logical->width;
      glyphs->glyphs[i].attr.is_cluster_start = 1;
      
      glyphs->log_clusters[i] = p - text;
    }
}

static void
imposed_extents (gint              n_chars,
		 PangoRectangle   *shape_ink,
		 PangoRectangle   *shape_logical,
		 PangoRectangle   *ink_rect,
		 PangoRectangle   *logical_rect)
{
  if (n_chars > 0)
    {
      if (ink_rect)
	{
	  ink_rect->x = MIN (shape_ink->x, shape_ink->x + shape_logical->width * (n_chars - 1));
	  ink_rect->width = MAX (shape_ink->width, shape_ink->width + shape_logical->width * (n_chars - 1));
	  ink_rect->y = shape_ink->y;
	  ink_rect->height = shape_ink->height;
	}
      if (logical_rect)
	{
	  logical_rect->x = MIN (shape_logical->x, shape_logical->x + shape_logical->width * (n_chars - 1));
	  logical_rect->width = MAX (shape_logical->width, shape_logical->width + shape_logical->width * (n_chars - 1));
	  logical_rect->y = shape_logical->y;
	  logical_rect->height = shape_logical->height;
	}
    }
  else
    {
      if (ink_rect)
	{
	  ink_rect->x = 0;
	  ink_rect->y = 0;
	  ink_rect->width = 0;
	  ink_rect->height = 0;
	}
      
      if (logical_rect)
	{
	  logical_rect->x = 0;
	  logical_rect->y = 0;
	  logical_rect->width = 0;
	  logical_rect->height = 0;
	}
    }
}


/*****************
 * Line Breaking *
 *****************/

static void shape_tab (PangoLayoutLine  *line,
		       PangoGlyphString *glyphs);

static void
free_run (PangoLayoutRun *run, gboolean free_item)
{
  if (free_item)
    pango_item_free (run->item);

  pango_glyph_string_free (run->glyphs);
  g_free (run);
}

static PangoItem *
uninsert_run (PangoLayoutLine *line)
{
  PangoLayoutRun *run;
  PangoItem *item;
  
  GSList *tmp_node = line->runs;

  run = tmp_node->data;
  item = run->item;
  
  line->runs = tmp_node->next;
  line->length -= item->length;
  
  g_slist_free_1 (tmp_node);
  free_run (run, FALSE);

  return item;
}

static void
ensure_tab_width (PangoLayout *layout)
{
  if (layout->tab_width == -1)
    {
      /* Find out how wide 8 spaces are in the context's default
       * font. Utter performance killer. :-(
       */
      PangoGlyphString *glyphs = pango_glyph_string_new ();
      PangoItem *item;
      GList *items;
      PangoAttribute *attr;
      PangoAttrList *layout_attrs;
      PangoAttrList *tmp_attrs;
      PangoAttrIterator *iter;
      PangoFontDescription *font_desc = pango_font_description_copy_static (pango_context_get_font_description (layout->context));
      PangoLanguage *language;
      int i;

      layout_attrs = pjx_layout_get_effective_attributes (layout);
      iter = pango_attr_list_get_iterator (layout_attrs);
      pango_attr_iterator_get_font (iter, font_desc, &language, NULL);
      
      tmp_attrs = pango_attr_list_new ();

      attr = pango_attr_font_desc_new (font_desc);
      pango_font_description_free (font_desc);
      
      attr->start_index = 0;
      attr->end_index = 1;
      pango_attr_list_insert_before (tmp_attrs, attr);
      
      if (language)
	{
	  attr = pango_attr_language_new (language);
	  attr->start_index = 0;
	  attr->end_index = 1;
	  pango_attr_list_insert_before (tmp_attrs, attr);
	}
	  
      items = pango_itemize (layout->context, " ", 0, 1, tmp_attrs, NULL);

      pango_attr_iterator_destroy (iter);
      if (layout_attrs != layout->attrs)
	pango_attr_list_unref (layout_attrs);
      pango_attr_list_unref (tmp_attrs);
      
      item = items->data;
      pango_shape ("        ", 8, &item->analysis, glyphs);
      
      pango_item_free (item);
      g_list_free (items);
      
      layout->tab_width = 0;
      for (i=0; i < glyphs->num_glyphs; i++)
	layout->tab_width += glyphs->glyphs[i].geometry.width;
      
      pango_glyph_string_free (glyphs);

      /* We need to make sure the tab_width is > 0 so finding tab positions
       * terminates. This check should be necessary only under extreme
       * problems with the font.
       */
      if (layout->tab_width <= 0)
	layout->tab_width = 50 * PANGO_SCALE; /* pretty much arbitrary */
    }
}

/* For now we only need the tab position, we assume
 * all tabs are left-aligned.
 */
static int
get_tab_pos (PangoLayout *layout, int index)
{
  gint n_tabs;
  gboolean in_pixels;

  if (layout->tabs)
    {
      n_tabs = pango_tab_array_get_size (layout->tabs);
      in_pixels = pango_tab_array_get_positions_in_pixels (layout->tabs);
    }
  else
    {
      n_tabs = 0;
      in_pixels = FALSE;
    }
  
  if (index < n_tabs)
    {
      gint pos = 0;

      pango_tab_array_get_tab (layout->tabs, index, NULL, &pos);

      if (in_pixels)
        return pos * PANGO_SCALE;
      else
        return pos;
    }

  if (n_tabs > 0)
    {
      /* Extrapolate tab position, repeating the last tab gap to
       * infinity.
       */
      int last_pos = 0;
      int next_to_last_pos = 0;
      int tab_width;
      
      pango_tab_array_get_tab (layout->tabs, n_tabs - 1, NULL, &last_pos);

      if (n_tabs > 1)
        pango_tab_array_get_tab (layout->tabs, n_tabs - 2, NULL, &next_to_last_pos);
      else
        next_to_last_pos = 0;

      if (in_pixels)
	{
	  next_to_last_pos *= PANGO_SCALE;
	  last_pos *= PANGO_SCALE;
	}
      
      if (last_pos > next_to_last_pos)
	{
	  tab_width = last_pos - next_to_last_pos;
	}
      else
	{
	  ensure_tab_width (layout);
	  tab_width = layout->tab_width;
	}

      return last_pos + tab_width * (index - n_tabs + 1);
    }
  else
    {
      /* No tab array set, so use default tab width
       */
      ensure_tab_width (layout);
      return layout->tab_width * index;
    }
}

static int
line_width (PangoLayoutLine *line)
{
  GSList *l;
  int i;
  int width = 0;
  
  /* Compute the width of the line currently - inefficient, but easier
   * than keeping the current width of the line up to date everywhere
   */
  for (l = line->runs; l; l = l->next)
    {
      PangoLayoutRun *run = l->data;
      
      for (i=0; i < run->glyphs->num_glyphs; i++)
	width += run->glyphs->glyphs[i].geometry.width;
    }

  return width;
}

static void
shape_tab (PangoLayoutLine  *line,
	   PangoGlyphString *glyphs)
{
  int i;

  int current_width = line_width (line);

  pango_glyph_string_set_size (glyphs, 1);
  
  glyphs->glyphs[0].glyph = 0;
  glyphs->glyphs[0].geometry.x_offset = 0;
  glyphs->glyphs[0].geometry.y_offset = 0;
  glyphs->glyphs[0].attr.is_cluster_start = 1;
  
  glyphs->log_clusters[0] = 0;

  for (i=0;;i++)
    {
      int tab_pos = get_tab_pos (line->layout, i);
      if (tab_pos > current_width)
	{
	  glyphs->glyphs[0].geometry.width = tab_pos - current_width;
	  break;
	}
    }
}

static inline gboolean
can_break_at (PangoLayout *layout,
	      gint         offset,
	      gboolean     always_wrap_char)
{
  PangoWrapMode wrap;
  /* We probably should have a mode where we treat all white-space as
   * of fungible width - appropriate for typography but not for
   * editing.
   */
  wrap = layout->wrap;
  
  if (wrap == PANGO_WRAP_WORD_CHAR)
    wrap = always_wrap_char ? PANGO_WRAP_CHAR : PANGO_WRAP_WORD;
  
  if (offset == layout->n_chars)
    return TRUE;
  else if (wrap == PANGO_WRAP_WORD)
    return layout->log_attrs[offset].is_line_break;
  else if (wrap == PANGO_WRAP_CHAR)
    return layout->log_attrs[offset].is_char_break;
  else
    {
      g_warning (G_STRLOC": broken PangoLayout");
      return TRUE;
    }
}

static inline gboolean
can_break_in (PangoLayout *layout,
	      int          start_offset,
	      int          num_chars,
	      gboolean     allow_break_at_start)
{
  int i;

  for (i = allow_break_at_start ? 0 : 1; i < num_chars; i++)
    if (can_break_at (layout, start_offset + i, FALSE))
      return TRUE;

  return FALSE;
}

typedef enum 
{
  BREAK_NONE_FIT,
  BREAK_SOME_FIT,
  BREAK_ALL_FIT,
  BREAK_EMPTY_FIT,
  BREAK_LINE_SEPARATOR
} BreakResult;

struct _ParaBreakState
{
  PangoAttrList *attrs;		/* Attributes being used for itemization */
  GList *items;			/* This paragraph turned into items */
  PangoDirection base_dir;	/* Current resolved base direction */
  gboolean first_line;		/* TRUE if this is the first line of the paragraph */
  gboolean last_line;		/* TRUE if this is the last line of the paragraph */
  int line_start_index;		/* Start index of line in layout->text */

  int remaining_width;		/* Amount of space remaining on line; < 0 is infinite */

  int start_offset;		/* Character offset of first item in state->items in layout->text */
  PangoGlyphString *glyphs;	/* Glyphs for the first item in state->items */
  ItemProperties properties;	/* Properties for the first item in state->items */
  PangoGlyphUnit *log_widths;	/* Logical widths for first item in state->items.. */
  int log_widths_offset;        /* Offset into log_widths to the point corresponding
				 * to the remaining portion of the first item */
};

static PangoGlyphString *
shape_run (PangoLayoutLine *line,
	   ParaBreakState  *state,
	   PangoItem       *item)
{
  PangoLayout *layout = line->layout;
  PangoGlyphString *glyphs = pango_glyph_string_new ();
  
  if (layout->text[item->offset] == '\t')
    shape_tab (line, glyphs);
  else
    {
      if (state->properties.shape_set)
	imposed_shape (layout->text + item->offset, item->num_chars,
		       state->properties.shape_ink_rect, state->properties.shape_logical_rect,
		       glyphs);
      else 
	pango_shape (layout->text + item->offset, item->length, &item->analysis, glyphs);

      if (state->properties.letter_spacing)
	{
	  PangoGlyphItem glyph_item;
	  
	  glyph_item.item = item;
	  glyph_item.glyphs = glyphs;
	  
	  pango_glyph_item_letter_space (&glyph_item,
					 layout->text,
					 layout->log_attrs + state->start_offset,
					 state->properties.letter_spacing);

	  /* We put all the letter spacing after the last glyph, then
	   * will go back and redistribute it at the beginning and the
	   * end in a post-processing step over the whole line.
	   */
	  glyphs->glyphs[glyphs->num_glyphs - 1].geometry.width += state->properties.letter_spacing;
	}
    }

  return glyphs;
}

static void
insert_run (PangoLayoutLine *line,
	    ParaBreakState  *state,
	    PangoItem       *run_item,
	    gboolean         last_run)
{
  PangoLayoutRun *run = g_new (PangoLayoutRun, 1);

  run->item = run_item;

  if (last_run && state->log_widths_offset == 0)
    run->glyphs = state->glyphs;
  else
    run->glyphs = shape_run (line, state, run_item);

  if (last_run)
    {
      if (state->log_widths_offset > 0)
	pango_glyph_string_free (state->glyphs);
      state->glyphs = NULL;
      g_free (state->log_widths);
    }
  
  line->runs = g_slist_prepend (line->runs, run);
  line->length += run_item->length;
}

/* Tries to insert as much as possible of the item at the head of
 * state->items onto @line. Three results are possible:
 *
 *  BREAK_NONE_FIT: Couldn't fit anything
 *  BREAK_SOME_FIT: The item was broken in the middle
 *  BREAK_ALL_FIT: Everything fit.
 *  BREAK_EMPTY_FIT: Nothing fit, but that was ok, as we can break at the first char.
 *
 * If @force_fit is TRUE, then BREAK_NONE_FIT will never
 * be returned, a run will be added even if inserting the minimum amount
 * will cause the line to overflow. This is used at the start of a line
 * and until we've found at least some place to break.
 *
 * If @no_break_at_end is TRUE, then BREAK_ALL_FIT will never be
 * returned even everything fits; the run will be broken earlier,
 * or BREAK_NONE_FIT returned. This is used when the end of the
 * run is not a break position.
 */
static BreakResult
process_item (PangoLayout     *layout,
	      PangoLayoutLine *line,
	      ParaBreakState  *state,
	      gboolean         force_fit,
	      gboolean         no_break_at_end)
{
  PangoItem *item = state->items->data;
  gboolean shape_set = FALSE;
  int width;
  int end_width;
  int length;
  int i;
  gboolean processing_new_item = FALSE;

  /* Only one character has type G_UNICODE_LINE_SEPARATOR in Unicode 4.0;
   * update this if that changes. */
#define LINE_SEPARATOR 0x2028

  if (!state->glyphs)
    {
      pjx_layout_get_item_properties (item, &state->properties);
      state->glyphs = shape_run (line, state, item);

      /* remove first space for grapheme cluster hack */
      if (state->glyphs->glyphs[0].glyph == 0)
	{
	  for (i = 1; i < state->glyphs->num_glyphs; i ++)
	    {
	      state->glyphs->glyphs[i - 1] = state->glyphs->glyphs[i];
	      state->glyphs->log_clusters[i - 1] = state->glyphs->log_clusters[i];
	    }
	  pango_glyph_string_set_size (state->glyphs, state->glyphs->num_glyphs - 1);
	}

      state->log_widths = NULL;
      state->log_widths_offset = 0;

      processing_new_item = TRUE;
    }

  if (g_utf8_get_char (layout->text + item->offset) == LINE_SEPARATOR)
    {
      insert_run (line, state, item, TRUE);
      state->log_widths_offset += item->num_chars;
      state->last_line = TRUE;
      return BREAK_LINE_SEPARATOR;
    }
  
  if (state->remaining_width < 0 && !no_break_at_end)  /* Wrapping off */
    {
      insert_run (line, state, item, TRUE);

      state->last_line = TRUE;

      return BREAK_ALL_FIT;
    }

  width = 0;
  if (processing_new_item)
    {
      for (i = 0; i < state->glyphs->num_glyphs; i++)
	width += state->glyphs->glyphs[i].geometry.width;
      
      /* We'll add half the letter spacing to each side of the item */
      width += state->properties.letter_spacing;
    }
  else
    {
      for (i = 0; i < item->num_chars; i++)
	width += state->log_widths[state->log_widths_offset + i];

      /* In this case, the letter spacing width has already been
       * added to the last element in log_widths */
    }

  if (pjx_attr_get_from_list (item->analysis.extra_attrs, PJX_ATTR_RB))
    width = 0;

  end_width = width;
  /* grapheme cluster hack : last space is not count for end_width */
  if (width > 0 && state->glyphs->glyphs[state->glyphs->num_glyphs - 1].glyph == 0)
    end_width = width - state->glyphs->glyphs[state->glyphs->num_glyphs - 1].geometry.width;

  if ((end_width <= state->remaining_width || (item->num_chars == 1 && !line->runs)) &&
      !no_break_at_end)
    {
      state->remaining_width -= width;
      state->remaining_width = MAX (state->remaining_width, 0);
      insert_run (line, state, item, TRUE);
      state->last_line = TRUE;

      return BREAK_ALL_FIT;
    }
  else
    {
      int num_chars = item->num_chars;
      int break_num_chars = num_chars;
      int break_width = width;
      int orig_width = width;
      gboolean retrying_with_char_breaks = FALSE;

      if (processing_new_item)
	{
	  state->log_widths = g_new (PangoGlyphUnit, item->num_chars);
	  pango_glyph_string_get_logical_widths (state->glyphs,
						 layout->text + item->offset, item->length, item->analysis.level,
						 state->log_widths);

	  /* The extra run letter spacing is actually divided after
	   * the last and and before the first, but it works to
	   * account it all on the last
	   */
	  if (item->num_chars > 0)
	    state->log_widths[item->num_chars - 1] += state->properties.letter_spacing;
	}

    state->last_line = FALSE;
    retry_break:

      /* Shorten the item by one line break
       */
      while (--num_chars >= 0)
	{
	  width -= (state->log_widths)[state->log_widths_offset + num_chars];

	  /* If there are no previous runs we have to take care to grab at least one char. */
	  if (can_break_at (layout, state->start_offset + num_chars, retrying_with_char_breaks) &&
	      (num_chars > 0 || line->runs) && (! pjx_attr_get_from_list (item->analysis.extra_attrs, PJX_ATTR_RUBY)))
	    {
	      int glyph_idx;

	      break_num_chars = num_chars;
	      break_width = width;
	      end_width = width;

	      /* grapheme cluster hack */
	      i = 0;
	      glyph_idx = 0;
	      if (num_chars)
		{
		  while (TRUE)
		    {
		      if (state->glyphs->glyphs[glyph_idx].attr.is_cluster_start)
			{
			  i ++;
			  if (i == num_chars)
			    {
 			      for (; glyph_idx < state->glyphs->num_glyphs - 1; glyph_idx ++)
				{
				  if (state->glyphs->glyphs[glyph_idx + 1].attr.is_cluster_start)
				    break;
				}
			      if (state->glyphs->glyphs[glyph_idx].glyph == 0)
				end_width -= state->glyphs->glyphs[glyph_idx].geometry.width;
			      break;
			    }
			}
		      glyph_idx ++;
		    }
		}

	      if (end_width <= state->remaining_width || (num_chars == 1 && !line->runs))
		break;
	    }
	}

      if (layout->wrap == PANGO_WRAP_WORD_CHAR && force_fit && end_width > state->remaining_width && !retrying_with_char_breaks)
	{
	  retrying_with_char_breaks = TRUE;
	  num_chars = item->num_chars;
	  width = orig_width;
	  break_num_chars = num_chars;
	  break_width = width;
	  end_width = width;
	  goto retry_break;
	}

      if (force_fit || end_width <= state->remaining_width)	/* Succesfully broke the item */
	{
	  if (state->remaining_width >= 0)
	    {
	      state->remaining_width -= break_width;
	      state->remaining_width = MAX (state->remaining_width, 0);
	    }
	  
	  if (break_num_chars == item->num_chars)
	    {
	      insert_run (line, state, item, TRUE);

	      return BREAK_ALL_FIT;
	    }
	  else if (break_num_chars == 0)
	    {
	      return BREAK_EMPTY_FIT;
	    }
	  else
	    {
	      PangoItem *new_item;

	      length = g_utf8_offset_to_pointer (layout->text + item->offset, break_num_chars) - (layout->text + item->offset);

              new_item = pango_item_split (item, length, break_num_chars);
	      
	      insert_run (line, state, new_item, FALSE);

	      state->log_widths_offset += break_num_chars;

	      /* Shaped items should never be broken */
	      g_assert (!shape_set);

	      return BREAK_SOME_FIT;
	    }
	}
      else
	{
	  pango_glyph_string_free (state->glyphs);
	  state->glyphs = NULL;
	  g_free (state->log_widths);

	  return BREAK_NONE_FIT;
	}
    }
}

static void
rewind_item (PangoLayoutLine *line,
	     ParaBreakState *state)
{
  PangoLayoutRun *run = line->runs->data;

  line->runs = g_slist_remove_link (line->runs, line->runs);
  state->items = g_list_prepend (state->items, run->item);
  state->start_offset -= run->item->num_chars;
  g_free (run);
}

static void
process_line (PangoLayout    *layout,
	      ParaBreakState *state)
{
  PangoLayoutLine *line;
  
  gboolean have_break = FALSE;      /* If we've seen a possible break yet */
  int break_remaining_width = 0;    /* Remaining width before adding run with break */
  int break_start_offset = 0;	    /* Start width before adding run with break */
  GSList *break_link = NULL;        /* Link holding run before break */
  int rb_width;
  PangoGlyphString *rb_gs;

  line = pjx_layout_line_new (layout);
  line->start_index = state->line_start_index;
  line->is_paragraph_start = state->first_line;
  line->resolved_dir = state->base_dir;

  if (layout->ellipsize != PANGO_ELLIPSIZE_NONE)
    state->remaining_width = -1;
  else if (state->first_line)
    state->remaining_width = (layout->indent >= 0) ? layout->width - layout->indent : layout->width;
  else
    state->remaining_width = (layout->indent >= 0) ? layout->width : layout->width + layout->indent;

  while (state->items)
    {
      PangoItem *item = state->items->data;
      BreakResult result;
      int old_num_chars;
      int old_remaining_width;
      gboolean first_item_in_line;

      old_num_chars = item->num_chars;
      old_remaining_width = state->remaining_width;
      first_item_in_line = line->runs != NULL;

      result = process_item (layout, line, state, !have_break, FALSE);

      if (pjx_attr_get_from_list (item->analysis.extra_attrs, PJX_ATTR_RB))
	{
	  int i;
	  PangoGlyphString *gs = ((PangoGlyphItem *)line->runs->data)->glyphs;
	  rb_width = 0;
	  for (i = 0; i < item->num_chars; i ++)
	    rb_width += gs->glyphs[i].geometry.width;
	  rb_gs = gs;
	}
      else if (pjx_attr_get_from_list (item->analysis.extra_attrs, PJX_ATTR_RT))
	{
	  int diff;
	  int i;
	  PangoGlyphString *gs = ((PangoGlyphItem *)line->runs->data)->glyphs;
	  int rt_width = 0;

	  if (result != BREAK_ALL_FIT)
	    {
	      rewind_item(line, state);

	      goto done;
	    }

	  for (i = 0; i < item->num_chars; i ++)
	    rt_width += gs->glyphs[i].geometry.width;

	  if (rb_width > rt_width)
	    {
	      diff = rb_width - rt_width;
	      if (state->remaining_width < diff)
		{
		  PangoLayoutRun *run;

		  run = line->runs->data;
		  line->runs = g_slist_remove_link (line->runs, line->runs);
		  g_free (run);
		  rewind_item(line, state);
		  state->remaining_width += rt_width;
		  state->last_line = FALSE;
		  goto done;
		}
	      state->remaining_width -= diff;
	    }
	  else
	    {
	      diff = rt_width - rb_width;
	      gs = rb_gs;
	    }
	    gs->glyphs[0].geometry.width += diff / (gs->num_glyphs * 2);
	    gs->glyphs[0].geometry.x_offset += diff / (gs->num_glyphs * 2);
	    for (i = 0; i < gs->num_glyphs - 1; i ++)
	      {
		int delta = diff / (gs->num_glyphs - i);
		gs->glyphs[i].geometry.width += delta;
		diff -= delta;
	      }
	}

      switch (result)
	{
	case BREAK_ALL_FIT:
	  if (can_break_in (layout, state->start_offset, old_num_chars, first_item_in_line))
	    {
	      have_break = TRUE;
	      break_remaining_width = old_remaining_width;
	      break_start_offset = state->start_offset;
	      break_link = line->runs->next;
	    }
	  
	  state->items = g_list_delete_link (state->items, state->items);
	  state->start_offset += old_num_chars;

	  break;

	case BREAK_EMPTY_FIT:
	  goto done;
	  
	case BREAK_SOME_FIT:
	  state->start_offset += old_num_chars - item->num_chars;
	  goto done;
	  
	case BREAK_NONE_FIT:
	  /* Back up over unused runs to run where there is a break */
	  while (line->runs && line->runs != break_link)
	    state->items = g_list_prepend (state->items, uninsert_run (line));

	  state->start_offset = break_start_offset;
	  state->remaining_width = break_remaining_width;

	  /* Reshape run to break */
	  item = state->items->data;
	  
	  old_num_chars = item->num_chars;
	  result = process_item (layout, line, state, TRUE, TRUE);
	  g_assert (result == BREAK_SOME_FIT || result == BREAK_EMPTY_FIT);
	  
	  state->start_offset += old_num_chars - item->num_chars;
	  
	  goto done;

        case BREAK_LINE_SEPARATOR:
          state->items = g_list_delete_link (state->items, state->items);
          state->start_offset += old_num_chars;
          goto done;
	}
    }

 done:  
  pjx_layout_line_postprocess (line, state);
  layout->lines = g_slist_prepend (layout->lines, line);
  state->first_line = FALSE;
  state->line_start_index += line->length;
}

static void
get_items_log_attrs (const char   *text,
                     GList        *items,
                     PangoLogAttr *log_attrs,
                     int           para_delimiter_len,
		     int	   start_index)
{
  int offset = 0;
  int index = 0;
  
  while (items)
    {
      GSList *i;
      PangoItem tmp_item = *(PangoItem *)items->data;
      tmp_item.analysis.extra_attrs = g_slist_copy (tmp_item.analysis.extra_attrs);

      /* Accumulate all the consecutive items that match in language
       * characteristics, ignoring font, style tags, etc.
       */
      while (items->next)
	{
	  PangoItem *next_item = items->next->data;

	  /* FIXME: Handle language tags */
	  if (next_item->analysis.lang_engine != tmp_item.analysis.lang_engine)
	    break;
	  else
	    {
	      tmp_item.length += next_item->length;
	      tmp_item.num_chars += next_item->num_chars;
	      tmp_item.analysis.extra_attrs = 
		g_slist_concat (tmp_item.analysis.extra_attrs, g_slist_copy (next_item->analysis.extra_attrs));
	    }

	  items = items->next;
	}
      
      i = tmp_item.analysis.extra_attrs;
      while (i)
	{
	  PangoAttribute *a = i->data;
	  a->start_index -= index + start_index;
	  a->end_index -= index + start_index;
	  i = i->next;
	}

      /* Break the paragraph delimiters with the last item */
      if (items->next == NULL)
	{
	  tmp_item.num_chars += g_utf8_strlen (text + index + tmp_item.length, para_delimiter_len);
	  tmp_item.length += para_delimiter_len;
	}

      pango_break (text + index, tmp_item.length, &tmp_item.analysis,
                   log_attrs + offset, tmp_item.num_chars + 1);

      offset += tmp_item.num_chars;
      index += tmp_item.length;

      g_slist_free (tmp_item.analysis.extra_attrs);
      
      items = items->next;
    }
}

static PangoAttrList *
pjx_layout_get_effective_attributes (PangoLayout *layout)
{
  PangoAttrList *attrs;

 if (layout->attrs)
   attrs = pango_attr_list_copy (layout->attrs);
  else
    attrs = pango_attr_list_new ();

  if (layout->font_desc)
    {
      PangoAttribute *attr = pango_attr_font_desc_new (layout->font_desc);
      attr->start_index = 0;
      attr->end_index = layout->length;
	  
      pango_attr_list_insert_before (attrs, attr);
    }

  return attrs;
}

static gboolean
no_shape_filter_func (PangoAttribute *attribute,
		      gpointer        data)
{
  static PangoAttrType no_shape_types[] = {
    PANGO_ATTR_FOREGROUND,
    PANGO_ATTR_BACKGROUND,
    PANGO_ATTR_UNDERLINE,
    PANGO_ATTR_STRIKETHROUGH,
    PANGO_ATTR_RISE
  };

  int i;

  for (i = 0; i < G_N_ELEMENTS (no_shape_types); i++)
    if (attribute->klass->type == no_shape_types[i])
      return TRUE;

  return FALSE;
}

static PangoAttrList *
filter_no_shape_attributes (PangoAttrList *attrs)
{
  return pango_attr_list_filter (attrs,
				 no_shape_filter_func,
				 NULL);
}

static void
apply_no_shape_attributes (PangoLayout   *layout,
			   PangoAttrList *no_shape_attrs)
{
  GSList *line_list;

  for (line_list = layout->lines; line_list; line_list = line_list->next)
    {
      PangoLayoutLine *line = line_list->data;
      GSList *old_runs = g_slist_reverse (line->runs);
      GSList *run_list;
      
      line->runs = NULL;
      for (run_list = old_runs; run_list; run_list = run_list->next)
	{
	  PangoGlyphItem *glyph_item = run_list->data;
	  GSList *new_runs;
	  
	  new_runs = pango_glyph_item_apply_attrs (glyph_item,
						   layout->text,
						   no_shape_attrs);

	  line->runs = g_slist_concat (new_runs, line->runs);
	}
      
      g_slist_free (old_runs);
    }
}

static void
pjx_layout_check_lines (PangoLayout *layout)
{
  const char *start;
  gboolean done = FALSE;
  int start_offset;
  PangoAttrList *attrs;
  PangoAttrList *no_shape_attrs;
  PangoAttrIterator *iter;
  PangoDirection prev_base_dir = PANGO_DIRECTION_NEUTRAL, base_dir = PANGO_DIRECTION_NEUTRAL;
  
  if (layout->lines)
    return;

  g_assert (!layout->log_attrs);

  /* For simplicity, we make sure at this point that layout->text
   * is non-NULL even if it is zero length
   */
  if (!layout->text)
    pango_layout_set_text (layout, NULL, 0);

  attrs = pjx_layout_get_effective_attributes (layout);
  no_shape_attrs = filter_no_shape_attributes (attrs);
  iter = pango_attr_list_get_iterator (attrs);
  
  layout->log_attrs = g_new (PangoLogAttr, layout->n_chars + 1);
  
  start_offset = 0;
  start = layout->text;
  
  /* Find the first strong direction of the text */
  if (layout->auto_dir)
    {
      prev_base_dir = pango_find_base_dir (layout->text, layout->length);
      if (prev_base_dir == PANGO_DIRECTION_NEUTRAL)
	prev_base_dir = pango_context_get_base_dir (layout->context);
    }
  else
    base_dir = pango_context_get_base_dir (layout->context);

  do
    {
      int delim_len;
      const char *end;
      int delimiter_index, next_para_index;
      ParaBreakState state;

      if (layout->single_paragraph)
        {
          delimiter_index = layout->length;
          next_para_index = layout->length;
        }
      else
        {
          pango_find_paragraph_boundary (start,
                                         (layout->text + layout->length) - start,
                                         &delimiter_index,
                                         &next_para_index);
        }

      g_assert (next_para_index >= delimiter_index);

      if (layout->auto_dir)
	{
	  base_dir = pango_find_base_dir (start, delimiter_index);
	  
	  /* Propagate the base direction for neutral paragraphs */
	  if (base_dir == PANGO_DIRECTION_NEUTRAL)
	    base_dir = prev_base_dir;
	  else
	    prev_base_dir = base_dir;
	}

      end = start + delimiter_index;
      
      delim_len = next_para_index - delimiter_index;
      
      if (end == (layout->text + layout->length))
	done = TRUE;

      g_assert (end <= (layout->text + layout->length));
      g_assert (start <= (layout->text + layout->length));
      g_assert (delim_len < 4);	/* PS is 3 bytes */
      g_assert (delim_len >= 0);

      state.attrs = attrs;
      state.items = pango_itemize_with_base_dir (layout->context,
						 base_dir,
						 layout->text,
						 start - layout->text,
						 end - start,
						 attrs,
						 iter);

      get_items_log_attrs (start, state.items,
                           layout->log_attrs + start_offset,
                           delim_len, start - layout->text);

      if (state.items)
	{
	  state.first_line = TRUE;
	  state.base_dir = base_dir;
	  state.start_offset = start_offset;
          state.line_start_index = start - layout->text;

	  state.glyphs = NULL;
	  state.log_widths = NULL;
	  
	  while (state.items)
            process_line (layout, &state);
	}
      else
        {
          PangoLayoutLine *empty_line;

          empty_line = pjx_layout_line_new (layout);
          empty_line->start_index = start - layout->text;
	  empty_line->is_paragraph_start = TRUE;	  
	  empty_line->resolved_dir = base_dir;

          layout->lines = g_slist_prepend (layout->lines,
                                           empty_line);
        }

      if (!done)
        start_offset += g_utf8_strlen (start, (end - start) + delim_len);

      start = end + delim_len;
    }
  while (!done);

  pango_attr_iterator_destroy (iter);
  pango_attr_list_unref (attrs);

  if (no_shape_attrs)
    {
      apply_no_shape_attributes (layout, no_shape_attrs);
      pango_attr_list_unref (no_shape_attrs);
    }

  layout->lines = g_slist_reverse (layout->lines);
}

static void
pjx_layout_run_get_extents (PangoLayoutRun *run,
                              PangoRectangle *run_ink,
                              PangoRectangle *run_logical)
{
  ItemProperties properties;
  PangoRectangle tmp_ink;
  gboolean need_ink;

  pjx_layout_get_item_properties (run->item, &properties);

  need_ink = run_ink || properties.uline == PANGO_UNDERLINE_LOW;
  
  if (properties.shape_set)
    imposed_extents (run->item->num_chars,
		     properties.shape_ink_rect,
		     properties.shape_logical_rect,
		     need_ink ? &tmp_ink : NULL, run_logical);
  else
    pango_glyph_string_extents (run->glyphs, run->item->analysis.font,
                                need_ink ? &tmp_ink : NULL,
                                run_logical);

  if (run_ink)
    *run_ink = tmp_ink;

  switch (properties.uline)
    {
    case PANGO_UNDERLINE_NONE:
      break;
    case PANGO_UNDERLINE_ERROR:
      if (run_ink)
        run_ink->height = MAX (run_ink->height, 3 * PANGO_SCALE - run_ink->y);
      if (run_logical)
	run_logical->height = MAX (run_logical->height, 3 * PANGO_SCALE - run_logical->y);
      break;
    case PANGO_UNDERLINE_SINGLE:
      if (run_ink)
        run_ink->height = MAX (run_ink->height, 2 * PANGO_SCALE - run_ink->y);
      if (run_logical)
	run_logical->height = MAX (run_logical->height, 2 * PANGO_SCALE - run_logical->y);
      break;
    case PANGO_UNDERLINE_DOUBLE:
      if (run_ink)
        run_ink->height = MAX (run_ink->height, 4 * PANGO_SCALE - run_ink->y);
      if (run_logical)
	run_logical->height = MAX (run_logical->height, 4 * PANGO_SCALE - run_logical->y);
      break;
    case PANGO_UNDERLINE_LOW:
      /* FIXME: Should this simply be run_logical->height += 2 * PANGO_SCALE instead?
       */
      if (run_ink)
	run_ink->height += 2 * PANGO_SCALE;
      if (run_logical)
	run_logical->height = MAX (run_logical->height,
				   tmp_ink.y + tmp_ink.height + 2 * PANGO_SCALE - run_logical->y);
      break;
    }

  if (properties.rise != 0)
    {
      if (run_ink)
        run_ink->y -= properties.rise;

      if (run_logical)
	run_logical->y -= properties.rise;
    }
}

static PangoLayoutLine *
pjx_layout_line_new (PangoLayout *layout)
{
  PangoLayoutLinePrivate *private = g_new (PangoLayoutLinePrivate, 1);

  private->ref_count = 1;
  private->line.layout = layout;
  private->line.runs = 0;
  private->line.length = 0;

  /* Note that we leave start_index, resolved_dir, and is_paragraph_start
   *  uninitialized */

  return (PangoLayoutLine *) private;
}

/*
 * NB: This implement the exact same algorithm as
 *     reorder-items.c:pango_reorder_items().
 */

static GSList *
reorder_runs_recurse (GSList *items, int n_items)
{
  GSList *tmp_list, *level_start_node;
  int i, level_start_i;
  int min_level = G_MAXINT;
  GSList *result = NULL;

  if (n_items == 0)
    return NULL;

  tmp_list = items;
  for (i=0; i<n_items; i++)
    {
      PangoLayoutRun *run = tmp_list->data;

      min_level = MIN (min_level, run->item->analysis.level);

      tmp_list = tmp_list->next;
    }

  level_start_i = 0;
  level_start_node = items;
  tmp_list = items;
  for (i=0; i<n_items; i++)
    {
      PangoLayoutRun *run = tmp_list->data;

      if (run->item->analysis.level == min_level)
	{
	  if (min_level % 2)
	    {
	      if (i > level_start_i)
		result = g_slist_concat (reorder_runs_recurse (level_start_node, i - level_start_i), result);
	      result = g_slist_prepend (result, run);
	    }
	  else
	    {
	      if (i > level_start_i)
		result = g_slist_concat (result, reorder_runs_recurse (level_start_node, i - level_start_i));
	      result = g_slist_append (result, run);
	    }

	  level_start_i = i + 1;
	  level_start_node = tmp_list->next;
	}

      tmp_list = tmp_list->next;
    }
  
  if (min_level % 2)
    {
      if (i > level_start_i)
	result = g_slist_concat (reorder_runs_recurse (level_start_node, i - level_start_i), result);
    }
  else
    {
      if (i > level_start_i)
	result = g_slist_concat (result, reorder_runs_recurse (level_start_node, i - level_start_i));
    }

  return result;
}

static void
pjx_layout_line_reorder (PangoLayoutLine *line)
{
  GSList *logical_runs = line->runs;
  line->runs = reorder_runs_recurse (logical_runs, g_slist_length (logical_runs));
  g_slist_free (logical_runs);
}

static int
get_item_letter_spacing (PangoItem *item)
{
  ItemProperties properties;

  pjx_layout_get_item_properties (item, &properties);

  return properties.letter_spacing;
}

static void
adjust_final_space (PangoGlyphString *glyphs,
		    int               adjustment)
{
  glyphs->glyphs[glyphs->num_glyphs - 1].geometry.width += adjustment;
}

static gboolean
is_tab_run (PangoLayout    *layout,
	    PangoLayoutRun *run)
{
  return (layout->text[run->item->offset] == '\t');
}

/* When doing shaping, we add the letter spacing value for a
 * run after every grapheme in the run. This produces ugly
 * asymetrical results, so what this routine is redistributes
 * that space to the beginning and the end of the run.
 *
 * We also trim the letter spacing from runs adjacent to
 * tabs and from the outside runs of the lines so that things
 * line up properly. The line breaking and tab positioning
 * were computed without this trimming so they are no longer
 * exactly correct, but this won't be very noticable in most
 * cases.
 */
static void
adjust_line_letter_spacing (PangoLayoutLine *line)
{
  PangoLayout *layout = line->layout;
  gboolean reversed;
  PangoLayoutRun *last_run;
  int tab_adjustment;
  GSList *l;
  
  /* If we have tab stops and the resolved direction of the
   * line is RTL, then we need to walk through the line
   * in reverse direction to figure out the corrections for
   * tab stops.
   */
  reversed = FALSE;
  if (line->resolved_dir == PANGO_DIRECTION_RTL)
    {
      for (l = line->runs; l; l = l->next)
	if (is_tab_run (layout, l->data))
	  {
	    line->runs = g_slist_reverse (line->runs);
	    reversed = TRUE;
	    break;
	  }
    }

  /* Walk over the runs in the line, redistributing letter
   * spacing from the end of the run to the start of the
   * run and trimming letter spacing from the ends of the
   * runs adjacent to the ends of the line or tab stops.
   *
   * We accumulate a correction factor from this trimming
   * which we add onto the next tab stop space to keep the
   * things properly aligned.
   */
  
  last_run = NULL;
  tab_adjustment = 0;
  for (l = line->runs; l; l = l->next)
    {
      PangoLayoutRun *run = l->data;
      PangoLayoutRun *next_run = l->next ? l->next->data : NULL;

      if (is_tab_run (layout, run))
	{
	  adjust_final_space (run->glyphs, tab_adjustment);
	  tab_adjustment = 0;
	}
      else
	{
	  PangoLayoutRun *visual_next_run = reversed ? last_run : next_run;
	  PangoLayoutRun *visual_last_run = reversed ? next_run : last_run;
	  int run_spacing = get_item_letter_spacing (run->item);
	  int adjustment = run_spacing / 2;
	  
	  if (visual_last_run && !is_tab_run (layout, visual_last_run))
	    adjust_final_space (visual_last_run->glyphs, adjustment);
	  else
	    tab_adjustment += adjustment;
	  
	  if (visual_next_run && !is_tab_run (layout, visual_next_run))
	    adjust_final_space (run->glyphs, - adjustment);
	  else
	    {
	      adjust_final_space (run->glyphs, - run_spacing);
	      tab_adjustment += run_spacing - adjustment;
	    }
	}

      last_run = run;
    }

  if (reversed)
    line->runs = g_slist_reverse (line->runs);
}

static void
adjust_line_justify (PangoLayoutLine *line, int remaining_width)
{
  int **widths;
  PangoLayout *layout = line->layout;
  GSList *l;
  int width_i = 0;
  int num_widths;
  PangoGlyphInfo *last_gi;
  int i;

  /* This is Japanese Extention. RTL not supported. */
  if (line->resolved_dir == PANGO_DIRECTION_RTL)
    return;

  if (remaining_width <= 0)
    return;

  /* line->length is longer than grapheme cluster size. This is 
   * inefficient but fast.
   */
  widths = g_new (gpointer, line->length);

      
  for (l = line->runs; l; l = l->next)
    {
      PangoLayoutRun *run = l->data;

      if (is_tab_run (layout, run))
	width_i = 0;

      if (pjx_attr_get_from_list (run->item->analysis.extra_attrs, PJX_ATTR_RB))
	continue;

      if (pjx_attr_get_from_list (run->item->analysis.extra_attrs, PJX_ATTR_RT))
	continue;

      for (i = 0; i < run->glyphs->num_glyphs; i ++, width_i ++)
	{
	  widths [width_i] = &(run->glyphs->glyphs[i].geometry.width);
	  last_gi = run->glyphs->glyphs + i;
	}
    }

  num_widths = width_i - 1;

  if (last_gi->glyph == 0)  /* grapheme cluster hack */
    remaining_width += last_gi->geometry.width;

  for (i = 0; i < num_widths; i ++)
    {
      int delta = remaining_width / (num_widths - i);
      *(widths [i]) += delta;
      remaining_width -= delta;
    }

  g_free (widths);
}

static void
pjx_layout_line_postprocess (PangoLayoutLine *line,
			       ParaBreakState  *state)
{
  /* NB: the runs are in reverse order at this point, since we prepended them to the list
   */
  
  /* Reverse the runs
   */
  line->runs = g_slist_reverse (line->runs);

  /* Ellipsize the line if necessary
   */
  _pango_layout_line_ellipsize (line, state->attrs);
  
  /* Now convert logical to visual order
   */
  pjx_layout_line_reorder (line);

  /* Fixup letter spacing between runs
   */
  adjust_line_letter_spacing (line);

  if (! state->last_line)
    adjust_line_justify (line, state->remaining_width);
}

static void
pjx_layout_get_item_properties (PangoItem      *item,
				  ItemProperties *properties)
{
  GSList *tmp_list = item->analysis.extra_attrs;

  properties->uline = PANGO_UNDERLINE_NONE;
  properties->letter_spacing = 0;
  properties->rise = 0;
  properties->shape_set = FALSE;
  properties->shape_ink_rect = NULL;
  properties->shape_logical_rect = NULL;

  while (tmp_list)
    {
      PangoAttribute *attr = tmp_list->data;

      switch (attr->klass->type)
	{
	case PANGO_ATTR_UNDERLINE:
	  properties->uline = ((PangoAttrInt *)attr)->value;
	  break;

        case PANGO_ATTR_RISE:
	  properties->rise = ((PangoAttrInt *)attr)->value;
          break;
          
	case PANGO_ATTR_LETTER_SPACING:
	  properties->letter_spacing = ((PangoAttrInt *)attr)->value;
	  break;

	case PANGO_ATTR_SHAPE:
	  properties->shape_set = TRUE;
	  properties->shape_logical_rect = &((PangoAttrShape *)attr)->logical_rect;
	  properties->shape_ink_rect = &((PangoAttrShape *)attr)->ink_rect;
	  break;

	default:
	  break;
	}
      tmp_list = tmp_list->next;
    }
}

static gboolean
check_invalid (PangoLayoutIter *iter,
               const char      *loc)
{
  if (iter->line->layout == NULL)
    {
      g_warning ("%s: PangoLayout changed since PangoLayoutIter was created, iterator invalid", loc);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

#ifdef G_DISABLE_CHECKS
#define IS_INVALID(iter) FALSE
#else
#define IS_INVALID(iter) check_invalid ((iter), G_STRLOC)
#endif

static int
next_cluster_start (PangoGlyphString *gs,
                    int               cluster_start)
{
  int i;

  i = cluster_start + 1;
  while (i < gs->num_glyphs)
    {
      if (gs->glyphs[i].attr.is_cluster_start)
        return i;
      
      ++i;
    }

  return gs->num_glyphs;
}

static int
cluster_end_index (PangoLayoutIter *iter)
{
  PangoGlyphString *gs;
  
  gs = iter->run->glyphs;
  
  if (iter->next_cluster_start == gs->num_glyphs)
    {
      /* Use the left or right end of the run */
      if (iter->ltr)
        return iter->run->item->length;
      else
        return 0;
    }
  else
    {
      return gs->log_clusters[iter->next_cluster_start];
    }
}

static inline void
offset_y (PangoLayoutIter *iter,
          int             *y)
{
  Extents *line_ext;

  line_ext = (Extents*)iter->line_extents_link->data;

  *y += line_ext->baseline;
}

static void
update_run (PangoLayoutIter *iter,
	    int              run_start_index)
{
  Extents *line_ext;

  line_ext = (Extents*)iter->line_extents_link->data;
  
  /* Note that in iter_new() the iter->run_logical_rect.width
   * is garbage but we don't use it since we're on the first run of
   * a line.
   */
  if (iter->run_list_link == iter->line->runs)
    iter->run_x = line_ext->logical_rect.x;
  else
    iter->run_x += iter->run_logical_rect.width;
  
  if (iter->run)
    {
      pjx_layout_run_get_extents (iter->run,
                                    NULL,
                                    &iter->run_logical_rect);

      /* Fix coordinates of the run extents */
      iter->run_logical_rect.x += iter->run_x;
      
      offset_y (iter, &iter->run_logical_rect.y);
    }
  else
    {
      iter->run_logical_rect.x = iter->run_x;
      iter->run_logical_rect.y = line_ext->logical_rect.y;
      iter->run_logical_rect.width = 0;
      iter->run_logical_rect.height = line_ext->logical_rect.height;
    }

  if (iter->run)
    iter->ltr = (iter->run->item->analysis.level % 2) == 0;
  else
    iter->ltr = TRUE;
  
  if (iter->ltr)    
    iter->cluster_x = iter->run_logical_rect.x;
  else
    iter->cluster_x = iter->run_logical_rect.x +
      iter->run_logical_rect.width;
  
  iter->cluster_start = 0;

  if (iter->run)
    iter->next_cluster_start = next_cluster_start (iter->run->glyphs,
                                                   iter->cluster_start);
  else
    iter->next_cluster_start = 0;
  
  /* Index of the first cluster in the glyph string, relative
   * to the start of the run
   */
  if (iter->run)
    iter->cluster_index = iter->run->glyphs->log_clusters[0];
  else
    iter->cluster_index = 0;
  
  /* Get an overall index, leaving it unchanged for
   * a the NULL run
   */
  iter->index = run_start_index;
}

/**
 * pjx_layout_get_iter:
 * @layout: a #PangoLayout
 * 
 * Returns an iterator to iterate over the visual extents of the layout.
 * 
 * Return value: a new #PangoLayoutIter
 **/
PangoLayoutIter*
pjx_layout_get_iter (PangoLayout *layout)
{
  PangoLayoutIter *iter;
  
  g_return_val_if_fail (PANGO_IS_LAYOUT (layout), NULL);
  
  iter = g_new (PangoLayoutIter, 1);

  iter->layout = layout;
  g_object_ref (iter->layout);

  pjx_layout_check_lines (layout);
  
  iter->line_list_link = layout->lines;
  iter->line = iter->line_list_link->data;
  pango_layout_line_ref (iter->line);

  iter->run_list_link = iter->line->runs;

  if (iter->run_list_link)
    iter->run = iter->run_list_link->data;
  else
    iter->run = NULL;

  iter->line_extents = NULL;  
  pjx_layout_get_extents_internal (layout,
                                     NULL,
                                     &iter->logical_rect,
                                     &iter->line_extents);  

  iter->line_extents_link = iter->line_extents;

  update_run (iter, 0);

  return iter;
}

/**
 * pjx_layout_set_markup:
 * @layout: a #PangoLayout
 * @markup: marked-up text
 * @length: length of marked-up text in bytes, or -1
 *
 * Same as pango_layout_set_markup_with_accel(), but
 * the markup text isn't scanned for accelerators.
 * 
 **/
void
pjx_layout_set_markup (PangoLayout *layout,
                         const char  *markup,
                         int          length)
{
  pjx_layout_set_markup_with_accel (layout, markup, length, 0, NULL);
}

/**
 * pjx_layout_set_markup_with_accel:
 * @layout: a #PangoLayout
 * @markup: some marked-up text 
 * (see <link linkend="PangoMarkupFormat">markup format</link>)
 * @length: length of @markup in bytes
 * @accel_marker: marker for accelerators in the text
 * @accel_char: return location for any located accelerators
 * 
 * Sets the layout text and attribute list from marked-up text (see
 * <link linkend="PangoMarkupFormat">markup format</link>). Replaces
 * the current text and attribute list.
 *
 * If @accel_marker is nonzero, the given character will mark the
 * character following it as an accelerator. For example, the accel
 * marker might be an ampersand or underscore. All characters marked
 * as an accelerator will receive a %PANGO_UNDERLINE_LOW attribute,
 * and the first character so marked will be returned in @accel_char.
 * Two @accel_marker characters following each other produce a single
 * literal @accel_marker character.
 **/
void
pjx_layout_set_markup_with_accel (PangoLayout    *layout,
                                    const char     *markup,
                                    int             length,
                                    gunichar        accel_marker,
                                    gunichar       *accel_char)
{
  PangoAttrList *list = NULL;
  char *text = NULL;
  GError *error;
  
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (markup != NULL);
  
  error = NULL;
  if (!pjx_parse_markup (markup, length,
                           accel_marker,
                           &list, &text,
                           accel_char,
                           &error))
    {
      g_warning ("%s: %s", G_STRLOC, error->message);
      g_error_free (error);
      return;
    }
  
  pango_layout_set_text (layout, text, -1);
  pango_layout_set_attributes (layout, list);
  pango_attr_list_unref (list);
  g_free (text);
}
