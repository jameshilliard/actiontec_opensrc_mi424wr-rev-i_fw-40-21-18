package gnu.crypto.assembly;

// ----------------------------------------------------------------------------
// $Id: ModeStage.java,v 1.1.1.1 2007/05/07 23:32:40 jungo Exp $
//
// Copyright (C) 2003, Free Software Foundation, Inc.
//
// This file is part of GNU Crypto.
//
// GNU Crypto is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// GNU Crypto is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to the
//
//    Free Software Foundation Inc.,
//    51 Franklin Street, Fifth Floor,
//    Boston, MA 02110-1301
//    USA
//
// Linking this library statically or dynamically with other modules is
// making a combined work based on this library.  Thus, the terms and
// conditions of the GNU General Public License cover the whole
// combination.
//
// As a special exception, the copyright holders of this library give
// you permission to link this library with independent modules to
// produce an executable, regardless of the license terms of these
// independent modules, and to copy and distribute the resulting
// executable under terms of your choice, provided that you also meet,
// for each linked independent module, the terms and conditions of the
// license of that module.  An independent module is a module which is
// not derived from or based on this library.  If you modify this
// library, you may extend this exception to your version of the
// library, but you are not obligated to do so.  If you do not wish to
// do so, delete this exception statement from your version.
// ----------------------------------------------------------------------------

import gnu.crypto.mode.IMode;

import java.security.InvalidKeyException;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

/**
 * <p>An {@link IMode} {@link Stage} in a {@link Cascade} Cipher chain.</p>
 *
 * <p>Such a stage wraps an implementation of a Block Cipher Mode of Operation
 * ({@link IMode}) to allow inclusion of such an instance in a cascade of block
 * ciphers.</p>
 *
 * @version $Revision: 1.1.1.1 $
 */
class ModeStage extends Stage {

   // Constants and variables
   // -------------------------------------------------------------------------

   private IMode delegate;
   private transient Set cachedBlockSizes;

   // Constructor(s)
   // -------------------------------------------------------------------------

   ModeStage(IMode mode, Direction forwardDirection) {
      super(forwardDirection);

      delegate = mode;
      cachedBlockSizes = null;
   }

   // Class methods
   // -------------------------------------------------------------------------

   // Instance methods
   // -------------------------------------------------------------------------

   public Set blockSizes() {
      if (cachedBlockSizes == null) {
         HashSet result = new HashSet();
         for (Iterator it = delegate.blockSizes(); it.hasNext(); ) {
            result.add(it.next());
         }
         cachedBlockSizes = Collections.unmodifiableSet(result);
      }
      return cachedBlockSizes;
   }

   void initDelegate(Map attributes) throws InvalidKeyException {
      Direction flow = (Direction) attributes.get(DIRECTION);
      attributes.put(IMode.STATE, new Integer(
            flow.equals(forward) ? IMode.ENCRYPTION : IMode.DECRYPTION));

      delegate.init(attributes);
   }

   public int currentBlockSize() throws IllegalStateException {
      return delegate.currentBlockSize();
   }

   void resetDelegate() {
      delegate.reset();
   }

   void updateDelegate(byte[] in, int inOffset, byte[] out, int outOffset) {
      delegate.update(in, inOffset, out, outOffset);
   }

   public boolean selfTest() {
      return delegate.selfTest();
   }
}
