/* pango
 * pango-attributes.c: Attributed text
 *
 * Copyright (C) 2000-2002 Red Hat Software
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

#include <string.h>

#include <pango/pango-attributes.h>
#include "pjx-attributes.h"
#include <pango/pango-utils.h>
#include "pjx-markup-private.h"

static PangoAttrType attr_ruby;
static PangoAttrType attr_rb;
static PangoAttrType attr_rt;

static PangoAttribute *pjx_attr_void_copy (const PangoAttribute *attr);
static void pjx_attr_void_destroy (PangoAttribute *attr);
static gboolean pjx_attr_void_equal (const PangoAttribute *attr1,
				       const PangoAttribute *attr2);
static gboolean ruby_parse_func     (MarkupData            *md,
				     OpenTag               *tag,
				     const gchar          **names,
				     const gchar          **values,
				     GMarkupParseContext   *context,
				     GError               **error);
static gboolean rb_parse_func     (MarkupData            *md,
				   OpenTag               *tag,
				   const gchar          **names,
				   const gchar          **values,
				   GMarkupParseContext   *context,
				   GError               **error);
static gboolean rt_parse_func     (MarkupData            *md,
				   OpenTag               *tag,
				   const gchar          **names,
				   const gchar          **values,
				   GMarkupParseContext   *context,
				   GError               **error);

static PangoAttrClass ruby_klass = {0, pjx_attr_void_copy, pjx_attr_void_destroy, pjx_attr_void_equal};
static PangoAttrClass rb_klass = {0, pjx_attr_void_copy, pjx_attr_void_destroy, pjx_attr_void_equal};
static PangoAttrClass rt_klass = {0, pjx_attr_void_copy, pjx_attr_void_destroy, pjx_attr_void_equal};

static gboolean is_initalized = FALSE;

void
pjx_attr_init ()
{
  if (is_initalized)
    return;

  attr_ruby = pango_attr_type_register ("ruby");
  attr_rb = pango_attr_type_register ("rb");
  attr_rt = pango_attr_type_register ("rt");

  ruby_klass.type = PJX_ATTR_RUBY;
  rb_klass.type = PJX_ATTR_RB;
  rt_klass.type = PJX_ATTR_RT;

  pjx_markup_init ();

  pjx_markup_add_parse_func("ruby", ruby_parse_func);
  pjx_markup_add_parse_func("rb", rb_parse_func);
  pjx_markup_add_parse_func("rt", rt_parse_func);

  is_initalized = TRUE;
}

void
pjx_attr_destroy ()
{
  pjx_markup_destroy ();
}

static PangoAttribute *
pjx_attr_void_new (const PangoAttrClass *klass)
{
  PangoAttribute *result = g_new (PangoAttribute, 1);
  result->klass = klass;

  return result;
}

static PangoAttribute *
pjx_attr_void_copy (const PangoAttribute *attr)
{
  return pjx_attr_void_new (attr->klass);
}

static void
pjx_attr_void_destroy (PangoAttribute *attr)
{
  g_free (attr);
}

static gboolean
pjx_attr_void_equal (const PangoAttribute *attr1,
		       const PangoAttribute *attr2)
{
  return attr1->klass->type == attr2->klass->type;
}

PangoAttrType
pjx_attr_ruby ()
{
  return attr_ruby;
}

PangoAttribute *
pjx_attr_ruby_new ()
{
  return pjx_attr_void_new (&ruby_klass);
}

PangoAttrType
pjx_attr_rb ()
{
  return attr_rb;
}

PangoAttribute *
pjx_attr_rb_new ()
{
  return pjx_attr_void_new (&rb_klass);
}

PangoAttrType
pjx_attr_rt ()
{
  return attr_rt;
}

PangoAttribute *
pjx_attr_rt_new ()
{
  return pjx_attr_void_new (&rt_klass);
}

static gboolean
ruby_parse_func     (MarkupData            *md,
                     OpenTag               *tag,
                     const gchar          **names,
                     const gchar          **values,
                     GMarkupParseContext   *context,
                     GError               **error)
{
  pjx_markup_add_attribute_to_context (tag, pjx_attr_ruby_new ());

  return TRUE;
}

static gboolean
rb_parse_func     (MarkupData            *md,
                   OpenTag               *tag,
                   const gchar          **names,
                   const gchar          **values,
                   GMarkupParseContext   *context,
                   GError               **error)
{
  pjx_markup_add_attribute_to_context (tag, pjx_attr_rb_new ());

  return TRUE;
}

#define RT_RISE 15000
static gboolean
rt_parse_func     (MarkupData            *md,
                   OpenTag               *tag,
                   const gchar          **names,
                   const gchar          **values,
                   GMarkupParseContext   *context,
                   GError               **error)
{
  pjx_markup_add_attribute_to_context (tag, pjx_attr_rt_new ());
  pjx_markup_add_attribute_to_context (tag, pango_attr_rise_new (RT_RISE));

  if (tag)
    {
      tag->scale_level_delta -= 4;
      tag->scale_level -= 4;
    }

  return TRUE;
}

struct _PangoAttrList
{
  guint ref_count;
  GSList *attributes;
  GSList *attributes_tail;
};

/**
 * pango_attr_list_change:
 * @list: a #PangoAttrList
 * @attr: the attribute to insert. Ownership of this value is
 *        assumed by the list.
 * 
 * Insert the given attribute into the #PangoAttrList. It will
 * replace any attributes of the same type on that segment
 * and be merged with any adjoining attributes that are identical.
 *
 * This function is slower than pango_attr_list_insert() for
 * creating a attribute list in order (potentially much slower
 * for large lists). However, pango_attr_list_insert() is not
 * suitable for continually changing a set of attributes 
 * since it never removes or combines existing attributes.
 **/
void
pjx_attr_list_change (PangoAttrList  *list,
			PangoAttribute *attr)
{
  GSList *tmp_list, *prev, *link;
  gint start_index = attr->start_index;
  gint end_index = attr->end_index;

  g_return_if_fail (list != NULL);

  if (start_index == end_index)	/* empty, nothing to do */
    {
      pango_attribute_destroy (attr);
      return;
    }
  
  tmp_list = list->attributes;
  prev = NULL;
  while (1)
    {
      PangoAttribute *tmp_attr;

      if (!tmp_list ||
	  ((PangoAttribute *)tmp_list->data)->start_index > start_index)
	{
	  /* We need to insert a new attribute
	   */
	  link = g_slist_alloc ();
	  link->next = tmp_list;
	  link->data = attr;
	  
	  if (prev)
	    prev->next = link;
	  else
	    list->attributes = link;
	  
	  if (!tmp_list)
	    list->attributes_tail = link;
	  
	  prev = link;
	  tmp_list = prev->next;
	  break;
	}

      tmp_attr = tmp_list->data;

      if (tmp_attr->klass->type == attr->klass->type &&
	  tmp_attr->end_index >= start_index)
	{
	  /* We overlap with an existing attribute */
	  if (pango_attribute_equal (tmp_attr, attr))
	    {
	      /* We can merge the new attribute with this attribute
	       */
	      if (tmp_attr->end_index >= end_index)
		{
		  /* We are totally overlapping the previous attribute.
		   * No action is needed.
		   */
		  pango_attribute_destroy (attr);
		  return;
		}
	      tmp_attr->end_index = end_index;
	      pango_attribute_destroy (attr);
	      
	      attr = tmp_attr;
	      
	      prev = tmp_list;
	      tmp_list = tmp_list->next;
	      
	      break;
	    }
	  else
	    {
	      /* Split, truncate, or remove the old attribute
	       */
	      if (tmp_attr->end_index > attr->end_index)
		{
		  PangoAttribute *end_attr = pango_attribute_copy (tmp_attr);

		  end_attr->start_index = attr->end_index;
		  pango_attr_list_insert (list, end_attr);
		}

	      if (tmp_attr->start_index == attr->start_index)
		{
		  pango_attribute_destroy (tmp_attr);
		  tmp_list->data = attr;

		  prev = tmp_list;
		  tmp_list = tmp_list->next;
		  break;
		}
	      else
		{
		  tmp_attr->end_index = attr->start_index;
		}
	    }
	}
      prev = tmp_list;
      tmp_list = tmp_list->next;
    }
  /* At this point, prev points to the list node with attr in it,
   * tmp_list points to prev->next.
   */

  g_assert (prev->data == attr);
  g_assert (prev->next == tmp_list);
  
  /* We now have the range inserted into the list one way or the
   * other. Fix up the remainder
   */
  while (tmp_list)
    {
      PangoAttribute *tmp_attr = tmp_list->data;

      if (tmp_attr->start_index > end_index)
	break;
      else if (tmp_attr->klass->type == attr->klass->type)
	{
	  if (tmp_attr->end_index <= attr->end_index ||
	      pango_attribute_equal (tmp_attr, attr))
	    {
	      /* We can merge the new attribute with this attribute.
	       */
	      attr->end_index = MAX (end_index, tmp_attr->end_index);
	      
	      pango_attribute_destroy (tmp_attr);
	      prev->next = tmp_list->next;

	      if (!prev->next)
		list->attributes_tail = prev;
	      
	      g_slist_free_1 (tmp_list);
	      tmp_list = prev->next;

	      continue;
	    }
	  else
	    {
	      /* Trim the start of this attribute that it begins at the end
	       * of the new attribute. This may involve moving
	       * it in the list to maintain the required non-decreasing
	       * order of start indices
	       */
	      GSList *tmp_list2;
	      GSList *prev2;
	      
	      tmp_attr->start_index = attr->end_index;

	      tmp_list2 = tmp_list->next;
	      prev2 = tmp_list;

	      while (tmp_list2)
		{
		  PangoAttribute *tmp_attr2 = tmp_list2->data;

		  if (tmp_attr2->start_index >= tmp_attr->start_index)
		    break;

                  prev2 = tmp_list2;
		  tmp_list2 = tmp_list2->next;
		}

	      /* Now remove and insert before tmp_list2. We'll
	       * hit this attribute again later, but that's harmless.
	       */
	      if (prev2 != tmp_list)
		{
		  GSList *old_next = tmp_list->next;
		  
		  prev->next = old_next;
		  prev2->next = tmp_list;
		  tmp_list->next = tmp_list2;

		  if (!tmp_list->next)
		    list->attributes_tail = tmp_list;

		  tmp_list = old_next;

		  continue;
		}
	    }
	}
      
      prev = tmp_list;
      tmp_list = tmp_list->next;
    }
}

PangoAttribute *
pjx_attr_get_from_list (GSList *attrs,
			PangoAttrType attr_type)
{
  GSList *a = attrs;
  while(a)
    {
      PangoAttribute *attr = a->data;
      if(attr->klass->type == attr_type)
	return attr;
      a = a->next;
    }
  return NULL;
}
