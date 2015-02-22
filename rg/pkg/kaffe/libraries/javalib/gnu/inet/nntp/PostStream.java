/*
 * PostStream.java
 * Copyright (C) 2002, 2003 The free Software Foundation
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

import java.io.FilterOutputStream;
import java.io.OutputStream;
import java.io.IOException;

/**
 * A stream to which article contents should be written.
 *
 * @author <a href='mailto:dog@gnu.org'>Chris Burdess</a>
 */
public final class PostStream
  extends FilterOutputStream
{

  private static final int LF = 0x0a;
  private static final int DOT = 0x2e;

  NNTPConnection connection;
  boolean isTakethis;
  byte last;
  
  PostStream(NNTPConnection connection, boolean isTakethis)
  {
    super(connection.out);
    this.connection = connection;
    this.isTakethis = isTakethis;
  }
  
  public void write(int c)
    throws IOException
  {
    super.write(c);
    if (c == DOT && last == LF)
      {
        super.write(c); // double up initial dot
      }
    last = (byte) c;
  }

  public void write(byte[] bytes)
    throws IOException
  {
    write(bytes, 0, bytes.length);
  }

  public void write(byte[] bytes, int pos, int len)
    throws IOException
  {
    int end = pos + len;
    for (int i = pos; i < end; i++)
      {
        byte c = bytes[i];
        if (c == DOT && last == LF)
          {
            // Double dot
            if (i > pos)
              {
                // Write everything up to i
                int l = i - pos;
                super.write(bytes, pos, l);
                pos += l;
                len -= l;
              }
            else
              {
                super.write(DOT);
              }
          }
        last = c;
      }
    if (len > 0)
      {
        super.write(bytes, pos, len);
      }
  }
  
  /**
   * Close the stream.
   * This calls NNTPConnection.postComplete().
   */
  public void close()
    throws IOException
  {
    if (last != 0x0d)
      {
        // Need to add LF
        write(0x0d);
      }
    if (isTakethis)
      {
        connection.takethisComplete();
      }
    else
      {
        connection.postComplete();
      }
  }
  
}

