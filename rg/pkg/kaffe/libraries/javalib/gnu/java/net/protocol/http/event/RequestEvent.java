/* RequestEvent.java --
   Copyright (C) 2004 Free Software Foundation, Inc.

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


package gnu.java.net.protocol.http.event;

import gnu.java.net.protocol.http.Request;

import java.util.EventObject;

/**
 * A request event.
 *
 * @author Chris Burdess (dog@gnu.org)
 */
public class RequestEvent
  extends EventObject
{

  /**
   * The request created event type.
   */
  public static final int REQUEST_CREATED = 0;
  
  /**
   * The request sending event type.
   */
  public static final int REQUEST_SENDING = 1;
  
  /**
   * The request sent event type.
   */
  public static final int REQUEST_SENT = 2;
  
  /**
   * The type of this event.
   */
  protected int type;

  /**
   * The request associated with this event.
   */
  protected Request request;

  /**
   * Constructs a request event with the specified source, type, and request.
   */
  public RequestEvent(Object source, int type, Request request)
  {
    super(source);
    this.type = type;
    this.request = request;
  }

  /**
   * Returns the type of this event.
   * @see #type
   */
  public int getType()
  {
    return type;
  }

  /**
   * Returns the request associated with this event.
   */
  public Request getRequest()
  {
    return request;
  }
  
}

