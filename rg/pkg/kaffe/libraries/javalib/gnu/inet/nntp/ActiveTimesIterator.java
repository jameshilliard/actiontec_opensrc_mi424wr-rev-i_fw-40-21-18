/*
 * ActiveTimesIterator.java
 * Copyright (C) 2002 The Free Software Foundation
 * 
 * This file is part of GNU inetlib, a library.
 * 
 * GNU inetlib is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GNU inetlib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library.  Thus, the terms and
 * conditions of the GNU General Public License cover the whole
 * combination.
 *
 * As a special exception, the copyright holders of this library give you
 * permission to link this library with independent modules to produce an
 * executable, regardless of the license terms of these independent
 * modules, and to copy and distribute the resulting executable under
 * terms of your choice, provided that you also meet, for each linked
 * independent module, the terms and conditions of the license of that
 * module.  An independent module is a module which is not derived from
 * or based on this library.  If you modify this library, you may extend
 * this exception to your version of the library, but you are not
 * obliged to do so.  If you do not wish to do so, delete this
 * exception statement from your version.
 */

package gnu.inet.nntp;

import java.io.IOException;
import java.net.ProtocolException;
import java.text.ParseException;
import java.util.Date;
import java.util.NoSuchElementException;

/**
 * An iterator over an NNTP LIST ACTIVE.TIMES listing.
 *
 * @author <a href='mailto:dog@gnu.org'>Chris Burdess</a>
 */
public class ActiveTimesIterator
  extends LineIterator
{

  ActiveTimesIterator(NNTPConnection connection)
  {
    super(connection);
  }
  
  /**
   * Returns the next group active time.
   */
  public Object next()
  {
    try
      {
        return nextGroup();
      }
    catch (IOException e)
      {
        throw new NoSuchElementException("I/O error: " + e.getMessage());
      }
  }

  /**
   * Returns the next group active time.
   */
  public ActiveTime nextGroup()
    throws IOException
  {
    String line = nextLine();

    // Parse line
    try
      {
        int start = 0, end;
        end = line.indexOf(' ', start);
        String group = line.substring(start, end);
        start = end + 1;
        end = line.indexOf(' ', start);
        Date time = connection.parseDate(line.substring(start, end));
        start = end + 1;
        String email = line.substring(start);

        return new ActiveTime(group, time, email);
      }
    catch (ParseException e)
      {
        ProtocolException e2 =
          new ProtocolException("Invalid active time line: " + line);
        e2.initCause(e);
        throw e2;
      }
    catch (StringIndexOutOfBoundsException e)
      {
        ProtocolException e2 =
          new ProtocolException("Invalid active time line: " + line);
        e2.initCause(e);
        throw e2;
      }
  }

}

