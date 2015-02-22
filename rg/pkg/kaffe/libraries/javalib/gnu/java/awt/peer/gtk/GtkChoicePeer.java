/* GtkChoicePeer.java -- Implements ChoicePeer with GTK
   Copyright (C) 1998, 1999, 2005  Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


package gnu.java.awt.peer.gtk;

import java.awt.Choice;
import java.awt.event.ItemEvent;
import java.awt.peer.ChoicePeer;

public class GtkChoicePeer extends GtkComponentPeer
  implements ChoicePeer
{
  public GtkChoicePeer (Choice c)
  {
    super (c);

    int count = c.getItemCount ();
    if (count > 0)
      {
	String items[] = new String[count];
	for (int i = 0; i < count; i++)
	  items[i] = c.getItem (i);
	  
	append (items);
      }

    int selected = c.getSelectedIndex();
    if (selected >= 0)
      select(selected);
  }

  native void create ();

  native void append (String items[]);
  native int nativeGetSelected ();
  native void nativeAdd (String item, int index);
  native void nativeRemove (int index);
  native void nativeRemoveAll ();

  native void connectSignals ();

  native void selectNative (int position);
  native void selectNativeUnlocked (int position);

  public void select (int position)
  {
    if (Thread.currentThread() == GtkToolkit.mainThread)
      selectNativeUnlocked (position);
    else
      selectNative (position);
  }

  public void add (String item, int index)
  {
    int before = nativeGetSelected();
    
    nativeAdd (item, index);
    
    /* Generate an ItemEvent if we added the first one or
       if we inserted at or before the currently selected item. */
    if ((before < 0) || (before >= index))
      {
        // Must set our state before notifying listeners
	((Choice) awtComponent).select (((Choice) awtComponent).getItem (0));
        postItemEvent (((Choice) awtComponent).getItem (0), ItemEvent.SELECTED);
      }
  }

  public void remove (int index)
  {
    int before = nativeGetSelected();
    int after;
    
    nativeRemove (index);
    after = nativeGetSelected();
    
    /* Generate an ItemEvent if we are removing the currently selected item
       and there are at least one item left. */
    if ((before == index) && (after >= 0))
      {
        // Must set our state before notifying listeners
	((Choice) awtComponent).select (((Choice) awtComponent).getItem (0));
        postItemEvent (((Choice) awtComponent).getItem (0), ItemEvent.SELECTED);
      }
  }

  public void removeAll ()
  {
    nativeRemoveAll();
  }
  
  public void addItem (String item, int position)
  {
    add (item, position);
  }

  protected void postChoiceItemEvent (String label, int stateChange)
  {
    // Must set our state before notifying listeners
    if (stateChange == ItemEvent.SELECTED)
      ((Choice) awtComponent).select (label);
    postItemEvent (label, stateChange);
  }
}
