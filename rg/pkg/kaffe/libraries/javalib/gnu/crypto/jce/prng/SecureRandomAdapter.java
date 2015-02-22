package gnu.crypto.jce.prng;

// ----------------------------------------------------------------------------
// $Id: SecureRandomAdapter.java,v 1.1.1.1 2007/05/07 23:32:39 jungo Exp $
//
// Copyright (C) 2001, 2002, 2003 Free Software Foundation, Inc.
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

import gnu.crypto.prng.LimitReachedException;
import gnu.crypto.prng.MDGenerator;

import java.security.SecureRandomSpi;
import java.util.HashMap;

/**
 * <p>The implementation of a generic {@link java.security.SecureRandom} adapter
 * class to wrap gnu.crypto prng instances based on Message Digest algorithms.</p>
 *
 * <p>This class defines the <i>Service Provider Interface</i> (<b>SPI</b>) for
 * the {@link java.security.SecureRandom} class, which provides the
 * functionality of a cryptographically strong pseudo-random number generator.</p>
 *
 * <p>All the abstract methods in the {@link SecureRandomSpi} class are
 * implemented by this class and all its sub-classes.</p>
 *
 * @version $Revision: 1.1.1.1 $
 */
abstract class SecureRandomAdapter extends SecureRandomSpi {

   // Constants and variables
   // -------------------------------------------------------------------------

   /** Our underlying prng instance. */
   private MDGenerator adaptee = new MDGenerator();

   /** The name of the message digest algorithm used by the adaptee. */
   private String mdName;

   // Constructor(s)
   // -------------------------------------------------------------------------

   /**
    * <p>Trivial protected constructor.</p>
    *
    * @param mdName the canonical name of the underlying hash algorithm.
    */
   protected SecureRandomAdapter(String mdName) {
      super();

      this.mdName = mdName;
   }

   // Class methods
   // -------------------------------------------------------------------------

   // Instance methods
   // -------------------------------------------------------------------------

   // java.security.SecureRandomSpi interface implementation ------------------

   public byte[] engineGenerateSeed(int numBytes) {
      if (numBytes < 1) {
         return new byte[0];
      }
      byte[] result = new byte[numBytes];
      this.engineNextBytes(result);
      return result;
   }

   public void engineNextBytes(byte[] bytes) {
      if (!adaptee.isInitialised()) {
         this.engineSetSeed(new byte[0]);
      }
      try {
         adaptee.nextBytes(bytes, 0, bytes.length);
      } catch (LimitReachedException ignored) {
      }
   }

   public void engineSetSeed(byte[] seed) {
      HashMap attributes = new HashMap();
      attributes.put(MDGenerator.MD_NAME, mdName);
      attributes.put(MDGenerator.SEEED, seed);
      adaptee.init(attributes);
   }
}
